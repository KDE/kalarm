/*
 *  alarmtimewidget.cpp  -  alarm date/time entry widget
 *  Program:  kalarm
 *  Copyright © 2001-2011 by David Jarvie <djarvie@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kalarm.h"

#include "buttongroup.h"
#include "checkbox.h"
#include "messagebox.h"
#include "preferences.h"
#include "pushbutton.h"
#include "radiobutton.h"
#include "synchtimer.h"
#include "timeedit.h"
#include "timespinbox.h"
#include "timezonecombo.h"
#include "alarmtimewidget.h"

#include <kalarmcal/datetime.h>

#include <kdatecombobox.h>
#include <kdialog.h>
#include <klocale.h>

#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>

static const QTime time_23_59(23, 59);


const int AlarmTimeWidget::maxDelayTime = 999*60 + 59;    // < 1000 hours

QString AlarmTimeWidget::i18n_TimeAfterPeriod()
{
    return i18nc("@info/plain", "Enter the length of time (in hours and minutes) after "
                "the current time to schedule the alarm.");
}


/******************************************************************************
* Construct a widget with a group box and title.
*/
AlarmTimeWidget::AlarmTimeWidget(const QString& groupBoxTitle, Mode mode, QWidget* parent)
    : QFrame(parent),
      mMinDateTimeIsNow(false),
      mPastMax(false),
      mMinMaxTimeSet(false)
{
    init(mode, groupBoxTitle);
}

/******************************************************************************
* Construct a widget without a group box or title.
*/
AlarmTimeWidget::AlarmTimeWidget(Mode mode, QWidget* parent)
    : QFrame(parent),
      mMinDateTimeIsNow(false),
      mPastMax(false),
      mMinMaxTimeSet(false)
{
    init(mode);
}

void AlarmTimeWidget::init(Mode mode, const QString& title)
{
    static const QString recurText = i18nc("@info/plain",
                                           "If a recurrence is configured, the start date/time will be adjusted "
                                           "to the first recurrence on or after the entered date/time.");
    static const QString tzText = i18nc("@info/plain",
                                        "This uses KAlarm's default time zone, set in the Configuration dialog.");

    QWidget* topWidget;
    if (title.isEmpty())
        topWidget = this;
    else
    {
        QBoxLayout* layout = new QVBoxLayout(this);
        layout->setMargin(0);
        layout->setSpacing(0);
        topWidget = new QGroupBox(title, this);
        layout->addWidget(topWidget);
    }
    mDeferring = mode & DEFER_TIME;
    mButtonGroup = new ButtonGroup(this);
    connect(mButtonGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotButtonSet(QAbstractButton*)));
    QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
    topLayout->setSpacing(KDialog::spacingHint());
    topLayout->setMargin(title.isEmpty() ? 0 : KDialog::marginHint());

    // At time radio button/label
    mAtTimeRadio = new RadioButton((mDeferring ? i18nc("@option:radio", "Defer to date/time:") : i18nc("@option:radio", "At date/time:")), topWidget);
    mAtTimeRadio->setFixedSize(mAtTimeRadio->sizeHint());
    mAtTimeRadio->setWhatsThis(mDeferring ? i18nc("@info:whatsthis", "Reschedule the alarm to the specified date and time.")
                                          : i18nc("@info:whatsthis", "Specify the date, or date and time, to schedule the alarm."));
    mButtonGroup->addButton(mAtTimeRadio);

    // Date edit box
    mDateEdit = new KDateComboBox(topWidget);
    mDateEdit->setOptions(KDateComboBox::EditDate | KDateComboBox::SelectDate | KDateComboBox::DatePicker);
    connect(mDateEdit, SIGNAL(dateEntered(QDate)), SLOT(dateTimeChanged()));
    mDateEdit->setWhatsThis(i18nc("@info:whatsthis",
          "<para>Enter the date to schedule the alarm.</para>"
          "<para>%1</para>", (mDeferring ? tzText : recurText)));
    mAtTimeRadio->setFocusWidget(mDateEdit);

    // Time edit box and Any time checkbox
    KHBox* timeBox = new KHBox(topWidget);
    timeBox->setSpacing(2*KDialog::spacingHint());
    mTimeEdit = new TimeEdit(timeBox);
    mTimeEdit->setFixedSize(mTimeEdit->sizeHint());
    connect(mTimeEdit, SIGNAL(valueChanged(int)), SLOT(dateTimeChanged()));
    mTimeEdit->setWhatsThis(i18nc("@info:whatsthis",
          "<para>Enter the time to schedule the alarm.</para>"
          "<para>%1</para>"
          "<para>%2</para>", (mDeferring ? tzText : recurText), TimeSpinBox::shiftWhatsThis()));

    mAnyTime = -1;    // current status is uninitialised
    if (mode == DEFER_TIME)
    {
        mAnyTimeAllowed = false;
        mAnyTimeCheckBox = 0;
    }
    else
    {
        mAnyTimeAllowed = true;
        mAnyTimeCheckBox = new CheckBox(i18nc("@option:check", "Any time"), timeBox);
        mAnyTimeCheckBox->setFixedSize(mAnyTimeCheckBox->sizeHint());
        connect(mAnyTimeCheckBox, SIGNAL(toggled(bool)), SLOT(slotAnyTimeToggled(bool)));
        mAnyTimeCheckBox->setWhatsThis(i18nc("@info:whatsthis",
              "Check to specify only a date (without a time) for the alarm. The alarm will trigger at the first opportunity on the selected date."));
    }

    // 'Time from now' radio button/label
    mAfterTimeRadio = new RadioButton((mDeferring ? i18nc("@option:radio", "Defer for time interval:") : i18nc("@option:radio", "Time from now:")), topWidget);
    mAfterTimeRadio->setFixedSize(mAfterTimeRadio->sizeHint());
    mAfterTimeRadio->setWhatsThis(mDeferring ? i18nc("@info:whatsthis", "Reschedule the alarm for the specified time interval after now.")
                                             : i18nc("@info:whatsthis", "Schedule the alarm after the specified time interval from now."));
    mButtonGroup->addButton(mAfterTimeRadio);

    // Delay time spin box
    mDelayTimeEdit = new TimeSpinBox(1, maxDelayTime, topWidget);
    mDelayTimeEdit->setValue(1439);
    mDelayTimeEdit->setFixedSize(mDelayTimeEdit->sizeHint());
    connect(mDelayTimeEdit, SIGNAL(valueChanged(int)), SLOT(delayTimeChanged(int)));
    mDelayTimeEdit->setWhatsThis(mDeferring ? i18nc("@info:whatsthis", "<para>%1</para><para>%2</para>", i18n_TimeAfterPeriod(), TimeSpinBox::shiftWhatsThis())
                                            : i18nc("@info:whatsthis", "<para>%1</para><para>%2</para><para>%3</para>", i18n_TimeAfterPeriod(), recurText, TimeSpinBox::shiftWhatsThis()));
    mAfterTimeRadio->setFocusWidget(mDelayTimeEdit);

    // Set up the layout, either narrow or wide
    QGridLayout* grid = new QGridLayout();
    grid->setMargin(0);
    topLayout->addLayout(grid);
    if (mDeferring)
    {
        grid->addWidget(mAtTimeRadio, 0, 0);
        grid->addWidget(mDateEdit, 0, 1, Qt::AlignLeft);
        grid->addWidget(timeBox, 1, 1, Qt::AlignLeft);
        grid->setColumnStretch(2, 1);
        topLayout->addStretch();
        QHBoxLayout* layout = new QHBoxLayout();
        topLayout->addLayout(layout);
        layout->addWidget(mAfterTimeRadio);
        layout->addWidget(mDelayTimeEdit);
        layout->addStretch();
    }
    else
    {
        grid->addWidget(mAtTimeRadio, 0, 0, Qt::AlignLeft);
        grid->addWidget(mDateEdit, 0, 1, Qt::AlignLeft);
        grid->addWidget(timeBox, 0, 2, Qt::AlignLeft);
        grid->setRowStretch(1, 1);
        grid->addWidget(mAfterTimeRadio, 2, 0, Qt::AlignLeft);
        grid->addWidget(mDelayTimeEdit, 2, 1, Qt::AlignLeft);

        // Time zone selection push button
        mTimeZoneButton = new PushButton(i18nc("@action:button", "Time Zone..."), topWidget);
        connect(mTimeZoneButton, SIGNAL(clicked()), SLOT(showTimeZoneSelector()));
        mTimeZoneButton->setWhatsThis(i18nc("@info:whatsthis",
              "Choose a time zone for this alarm which is different from the default time zone set in KAlarm's configuration dialog."));
        grid->addWidget(mTimeZoneButton, 2, 2, 1, 2, Qt::AlignRight);

        grid->setColumnStretch(2, 1);
        topLayout->addStretch();

        QHBoxLayout* layout = new QHBoxLayout();
        topLayout->addLayout(layout);
        layout->setSpacing(2*KDialog::spacingHint());

        // Time zone selector
        mTimeZoneBox = new KHBox(topWidget);   // this is to control the QWhatsThis text display area
        mTimeZoneBox->setMargin(0);
        mTimeZoneBox->setSpacing(KDialog::spacingHint());
        QLabel* label = new QLabel(i18nc("@label:listbox", "Time zone:"), mTimeZoneBox);
        mTimeZone = new TimeZoneCombo(mTimeZoneBox);
        mTimeZone->setMaxVisibleItems(15);
        connect(mTimeZone, SIGNAL(activated(int)), SLOT(slotTimeZoneChanged()));
        mTimeZoneBox->setWhatsThis(i18nc("@info:whatsthis", "Select the time zone to use for this alarm."));
        label->setBuddy(mTimeZone);
        layout->addWidget(mTimeZoneBox);

        // Time zone checkbox
        mNoTimeZone = new CheckBox(i18nc("@option:check", "Ignore time zone"), topWidget);
        connect(mNoTimeZone, SIGNAL(toggled(bool)), SLOT(slotTimeZoneToggled(bool)));
        mNoTimeZone->setWhatsThis(i18nc("@info:whatsthis",
                                        "<para>Check to use the local computer time, ignoring time zones.</para>"
                                        "<para>You are recommended not to use this option if the alarm has a "
                                        "recurrence specified in hours/minutes. If you do, the alarm may "
                                        "occur at unexpected times after daylight saving time shifts.</para>"));
        layout->addWidget(mNoTimeZone);
        layout->addStretch();

        // Initially show only the time zone button, not time zone selector
        mTimeZoneBox->hide();
        mNoTimeZone->hide();
    }

    // Initialise the radio button statuses
    mAtTimeRadio->setChecked(true);
    slotButtonSet(mAtTimeRadio);

    // Timeout every minute to update alarm time fields.
    MinuteTimer::connect(this, SLOT(updateTimes()));
}

/******************************************************************************
* Set or clear read-only status for the controls
*/
void AlarmTimeWidget::setReadOnly(bool ro)
{
    mAtTimeRadio->setReadOnly(ro);
    mDateEdit->setOptions(ro ? KDateComboBox::Options(0) : KDateComboBox::EditDate | KDateComboBox::SelectDate | KDateComboBox::DatePicker);
    mTimeEdit->setReadOnly(ro);
    if (mAnyTimeCheckBox)
        mAnyTimeCheckBox->setReadOnly(ro);
    mAfterTimeRadio->setReadOnly(ro);
    if (!mDeferring)
    {
        mTimeZone->setReadOnly(ro);
        mNoTimeZone->setReadOnly(ro);
    }
    mDelayTimeEdit->setReadOnly(ro);
}

/******************************************************************************
* Select the "Time from now" radio button.
*/
void AlarmTimeWidget::selectTimeFromNow(int minutes)
{
    mAfterTimeRadio->setChecked(true);
    if (minutes > 0)
        mDelayTimeEdit->setValue(minutes);
}

/******************************************************************************
* Fetch the entered date/time.
* If 'checkExpired' is true and the entered value <= current time, an error occurs.
* If 'minsFromNow' is non-null, it is set to the number of minutes' delay selected,
* or to zero if a date/time was entered.
* In this case, if 'showErrorMessage' is true, output an error message.
* 'errorWidget' if non-null, is set to point to the widget containing the error.
* Reply = invalid date/time if error.
*/
KDateTime AlarmTimeWidget::getDateTime(int* minsFromNow, bool checkExpired, bool showErrorMessage, QWidget** errorWidget) const
{
    if (minsFromNow)
        *minsFromNow = 0;
    if (errorWidget)
        *errorWidget = 0;
    KDateTime now = KDateTime::currentUtcDateTime();
    now.setTime(QTime(now.time().hour(), now.time().minute(), 0));
    if (!mAtTimeRadio->isChecked())
    {
        if (!mDelayTimeEdit->isValid())
        {
            if (showErrorMessage)
                KAMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18nc("@info", "Invalid time"));
            if (errorWidget)
                *errorWidget = mDelayTimeEdit;
            return KDateTime();
        }
        int delayMins = mDelayTimeEdit->value();
        if (minsFromNow)
            *minsFromNow = delayMins;
        return now.addSecs(delayMins * 60).toTimeSpec(mTimeSpec);
    }
    else
    {
        bool dateOnly = mAnyTimeAllowed && mAnyTimeCheckBox && mAnyTimeCheckBox->isChecked();
        if (!mDateEdit->date().isValid()  ||  !mTimeEdit->isValid())
        {
            // The date and/or time is invalid
            if (!mDateEdit->date().isValid())
            {
                if (showErrorMessage)
                    KAMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18nc("@info", "Invalid date"));
                if (errorWidget)
                    *errorWidget = mDateEdit;
            }
            else
            {
                if (showErrorMessage)
                    KAMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18nc("@info", "Invalid time"));
                if (errorWidget)
                    *errorWidget = mTimeEdit;
            }
            return KDateTime();
        }

        KDateTime result;
        if (dateOnly)
        {
            result = KDateTime(mDateEdit->date(), mTimeSpec);
            if (checkExpired  &&  result.date() < now.date())
            {
                if (showErrorMessage)
                    KAMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18nc("@info", "Alarm date has already expired"));
                if (errorWidget)
                    *errorWidget = mDateEdit;
                return KDateTime();
            }
        }
        else
        {
            result = KDateTime(mDateEdit->date(), mTimeEdit->time(), mTimeSpec);
            if (checkExpired  &&  result <= now.addSecs(1))
            {
                if (showErrorMessage)
                    KAMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18nc("@info", "Alarm time has already expired"));
                if (errorWidget)
                    *errorWidget = mTimeEdit;
                return KDateTime();
            }
        }
        return result;
    }
}

/******************************************************************************
* Set the date/time.
*/
void AlarmTimeWidget::setDateTime(const DateTime& dt)
{
    // Set the time zone first so that the call to dateTimeChanged() works correctly.
    if (mDeferring)
        mTimeSpec = dt.timeSpec().isValid() ? dt.timeSpec() : KDateTime::LocalZone;
    else
    {
        KTimeZone tz = dt.timeZone();
        mNoTimeZone->setChecked(!tz.isValid());
        mTimeZone->setTimeZone(tz.isValid() ? tz : Preferences::timeZone());
        slotTimeZoneChanged();
    }

    if (dt.date().isValid())
    {
        mTimeEdit->setValue(dt.effectiveTime());
        mDateEdit->setDate(dt.date());
        dateTimeChanged();     // update the delay time edit box
    }
    else
    {
        mTimeEdit->setValid(false);
        mDateEdit->setDate(QDate());
        mDelayTimeEdit->setValid(false);
    }
    if (mAnyTimeCheckBox)
    {
        bool dateOnly = dt.isDateOnly();
        if (dateOnly)
            mAnyTimeAllowed = true;
        mAnyTimeCheckBox->setChecked(dateOnly);
        setAnyTime();
    }
}

/******************************************************************************
* Set the minimum date/time to track the current time.
*/
void AlarmTimeWidget::setMinDateTimeIsCurrent()
{
    mMinDateTimeIsNow = true;
    mMinDateTime = KDateTime();
    KDateTime now = KDateTime::currentDateTime(mTimeSpec);
    mDateEdit->setMinimumDate(now.date());
    setMaxMinTimeIf(now);
}

/******************************************************************************
* Set the minimum date/time, adjusting the entered date/time if necessary.
* If 'dt' is invalid, any current minimum date/time is cleared.
*/
void AlarmTimeWidget::setMinDateTime(const KDateTime& dt)
{
    mMinDateTimeIsNow = false;
    mMinDateTime = dt.toTimeSpec(mTimeSpec);
    mDateEdit->setMinimumDate(mMinDateTime.date());
    setMaxMinTimeIf(KDateTime::currentDateTime(mTimeSpec));
}

/******************************************************************************
* Set the maximum date/time, adjusting the entered date/time if necessary.
* If 'dt' is invalid, any current maximum date/time is cleared.
*/
void AlarmTimeWidget::setMaxDateTime(const DateTime& dt)
{
    mPastMax = false;
    if (dt.isValid()  &&  dt.isDateOnly())
        mMaxDateTime = dt.effectiveKDateTime().addSecs(24*3600 - 60).toTimeSpec(mTimeSpec);
    else
        mMaxDateTime = dt.kDateTime().toTimeSpec(mTimeSpec);
    mDateEdit->setMaximumDate(mMaxDateTime.date());
    KDateTime now = KDateTime::currentDateTime(mTimeSpec);
    setMaxMinTimeIf(now);
    setMaxDelayTime(now);
}

/******************************************************************************
* If the minimum and maximum date/times fall on the same date, set the minimum
* and maximum times in the time edit box.
*/
void AlarmTimeWidget::setMaxMinTimeIf(const KDateTime& now)
{
    int   mint = 0;
    QTime maxt = time_23_59;
    mMinMaxTimeSet = false;
    if (mMaxDateTime.isValid())
    {
        bool set = true;
        KDateTime minDT;
        if (mMinDateTimeIsNow)
            minDT = now.addSecs(60);
        else if (mMinDateTime.isValid())
            minDT = mMinDateTime;
        else
            set = false;
        if (set  &&  mMaxDateTime.date() == minDT.date())
        {
            // The minimum and maximum times are on the same date, so
            // constrain the time value.
            mint = minDT.time().hour()*60 + minDT.time().minute();
            maxt = mMaxDateTime.time();
            mMinMaxTimeSet = true;
        }
    }
    mTimeEdit->setMinimum(mint);
    mTimeEdit->setMaximum(maxt);
    mTimeEdit->setWrapping(!mint  &&  maxt == time_23_59);
}

/******************************************************************************
* Set the maximum value for the delay time edit box, depending on the maximum
* value for the date/time.
*/
void AlarmTimeWidget::setMaxDelayTime(const KDateTime& now)
{
    int maxVal = maxDelayTime;
    if (mMaxDateTime.isValid())
    {
        if (now.date().daysTo(mMaxDateTime.date()) < 100)    // avoid possible 32-bit overflow on secsTo()
        {
            KDateTime dt(now);
            dt.setTime(QTime(now.time().hour(), now.time().minute(), 0));   // round down to nearest minute
            maxVal = dt.secsTo(mMaxDateTime) / 60;
            if (maxVal > maxDelayTime)
                maxVal = maxDelayTime;
        }
    }
    mDelayTimeEdit->setMaximum(maxVal);
}

/******************************************************************************
* Set the status for whether a time is specified, or just a date.
*/
void AlarmTimeWidget::setAnyTime()
{
    int old = mAnyTime;
    mAnyTime = (mAtTimeRadio->isChecked() && mAnyTimeAllowed && mAnyTimeCheckBox && mAnyTimeCheckBox->isChecked()) ? 1 : 0;
    if (mAnyTime != old)
        emit dateOnlyToggled(mAnyTime);
}

/******************************************************************************
* Enable/disable the "date only" radio button.
*/
void AlarmTimeWidget::enableAnyTime(bool enable)
{
    if (mAnyTimeCheckBox)
    {
        mAnyTimeAllowed = enable;
        bool at = mAtTimeRadio->isChecked();
        mAnyTimeCheckBox->setEnabled(enable && at);
        if (at)
            mTimeEdit->setEnabled(!enable || !mAnyTimeCheckBox->isChecked());
        setAnyTime();
    }
}

/******************************************************************************
* Called every minute to update the alarm time data entry fields.
* If the maximum date/time has been reached, a 'pastMax()' signal is emitted.
*/
void AlarmTimeWidget::updateTimes()
{
    KDateTime now;
    if (mMinDateTimeIsNow)
    {
        // Make sure that the minimum date is updated when the day changes
        now = KDateTime::currentDateTime(mTimeSpec);
        mDateEdit->setMinimumDate(now.date());
    }
    if (mMaxDateTime.isValid())
    {
        if (!now.isValid())
            now = KDateTime::currentDateTime(mTimeSpec);
        if (!mPastMax)
        {
            // Check whether the maximum date/time has now been reached
            if (now.date() >= mMaxDateTime.date())
            {
                // The current date has reached or has passed the maximum date
                if (now.date() > mMaxDateTime.date()
                ||  (!mAnyTime && now.time() > mTimeEdit->maxTime()))
                {
                    mPastMax = true;
                    emit pastMax();
                }
                else if (mMinDateTimeIsNow  &&  !mMinMaxTimeSet)
                {
                    // The minimum date/time tracks the clock, so set the minimum
                    // and maximum times
                    setMaxMinTimeIf(now);
                }
            }
        }
        setMaxDelayTime(now);
    }

    if (mAtTimeRadio->isChecked())
        dateTimeChanged();
    else
        delayTimeChanged(mDelayTimeEdit->value());
}


/******************************************************************************
* Called when the radio button states have been changed.
* Updates the appropriate edit box.
*/
void AlarmTimeWidget::slotButtonSet(QAbstractButton*)
{
    bool at = mAtTimeRadio->isChecked();
    mDateEdit->setEnabled(at);
    mTimeEdit->setEnabled(at && (!mAnyTimeAllowed || !mAnyTimeCheckBox || !mAnyTimeCheckBox->isChecked()));
    if (mAnyTimeCheckBox)
        mAnyTimeCheckBox->setEnabled(at && mAnyTimeAllowed);
    // Ensure that the value of the delay edit box is > 0.
    KDateTime att(mDateEdit->date(), mTimeEdit->time(), mTimeSpec);
    int minutes = (KDateTime::currentUtcDateTime().secsTo(att) + 59) / 60;
    if (minutes <= 0)
        mDelayTimeEdit->setValid(true);
    mDelayTimeEdit->setEnabled(!at);
    setAnyTime();
}

/******************************************************************************
* Called after the mAnyTimeCheckBox checkbox has been toggled.
*/
void AlarmTimeWidget::slotAnyTimeToggled(bool on)
{
    on = (on && mAnyTimeAllowed);
    mTimeEdit->setEnabled(!on && mAtTimeRadio->isChecked());
    setAnyTime();
    if (on)
        emit changed(KDateTime(mDateEdit->date(), mTimeSpec));
    else
        emit changed(KDateTime(mDateEdit->date(), mTimeEdit->time(), mTimeSpec));
}

/******************************************************************************
* Called after a new selection has been made in the time zone combo box.
* Re-evaluates the time specification to use.
*/
void AlarmTimeWidget::slotTimeZoneChanged()
{
    if (mNoTimeZone->isChecked())
        mTimeSpec = KDateTime::ClockTime;
    else
    {
        KTimeZone tz = mTimeZone->timeZone();
        mTimeSpec = tz.isValid() ? KDateTime::Spec(tz) : KDateTime::LocalZone;
    }
    if (!mTimeZoneBox->isVisible()  &&  mTimeSpec != Preferences::timeZone())
    {
        // The current time zone is not the default one, so
        // show the time zone selection controls
        showTimeZoneSelector();
    }
    mMinDateTime = mMinDateTime.toTimeSpec(mTimeSpec);
    mMaxDateTime = mMaxDateTime.toTimeSpec(mTimeSpec);
    updateTimes();
}

/******************************************************************************
* Called after the mNoTimeZone checkbox has been toggled.
*/
void AlarmTimeWidget::slotTimeZoneToggled(bool on)
{
    mTimeZone->setEnabled(!on);
    slotTimeZoneChanged();
}

/******************************************************************************
* Called after the mTimeZoneButton button has been clicked.
* Show the time zone selection controls, and hide the button.
*/
void AlarmTimeWidget::showTimeZoneSelector()
{
    mTimeZoneButton->hide();
    mTimeZoneBox->show();
    mNoTimeZone->show();
}

/******************************************************************************
* Show or hide the time zone button.
*/
void AlarmTimeWidget::showMoreOptions(bool more)
{
    if (more)
    {
        if (!mTimeZoneBox->isVisible())
            mTimeZoneButton->show();
    }
    else
        mTimeZoneButton->hide();
}

/******************************************************************************
* Called when the date or time edit box values have changed.
* Updates the time delay edit box accordingly.
*/
void AlarmTimeWidget::dateTimeChanged()
{
    KDateTime dt(mDateEdit->date(), mTimeEdit->time(), mTimeSpec);
    int minutes = (KDateTime::currentUtcDateTime().secsTo(dt) + 59) / 60;
    bool blocked = mDelayTimeEdit->signalsBlocked();
    mDelayTimeEdit->blockSignals(true);     // prevent infinite recursion between here and delayTimeChanged()
    if (minutes <= 0  ||  minutes > mDelayTimeEdit->maximum())
        mDelayTimeEdit->setValid(false);
    else
        mDelayTimeEdit->setValue(minutes);
    mDelayTimeEdit->blockSignals(blocked);
    if (mAnyTimeAllowed && mAnyTimeCheckBox && mAnyTimeCheckBox->isChecked())
        emit changed(KDateTime(dt.date(), mTimeSpec));
    else
        emit changed(dt);
}

/******************************************************************************
* Called when the delay time edit box value has changed.
* Updates the Date and Time edit boxes accordingly.
*/
void AlarmTimeWidget::delayTimeChanged(int minutes)
{
    if (mDelayTimeEdit->isValid())
    {
        QDateTime dt = KDateTime::currentUtcDateTime().addSecs(minutes * 60).toTimeSpec(mTimeSpec).dateTime();
        bool blockedT = mTimeEdit->signalsBlocked();
        bool blockedD = mDateEdit->signalsBlocked();
        mTimeEdit->blockSignals(true);     // prevent infinite recursion between here and dateTimeChanged()
        mDateEdit->blockSignals(true);
        mTimeEdit->setValue(dt.time());
        mDateEdit->setDate(dt.date());
        mTimeEdit->blockSignals(blockedT);
        mDateEdit->blockSignals(blockedD);
        emit changed(KDateTime(dt.date(), dt.time(), mTimeSpec));
    }
}
#include "moc_alarmtimewidget.cpp"
// vim: et sw=4:
