/*
 *  alarmtimewidget.cpp  -  alarm date/time entry widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "alarmtimewidget.h"

#include "preferences.h"
#include "lib/buttongroup.h"
#include "lib/checkbox.h"
#include "lib/messagebox.h"
#include "lib/pushbutton.h"
#include "lib/radiobutton.h"
#include "lib/synchtimer.h"
#include "lib/timeedit.h"
#include "lib/timeperiod.h"
#include "lib/timespinbox.h"
#include "lib/timezonecombo.h"

#include <KDateComboBox>
#include <KLocalizedString>

#include <QTimeZone>
#include <QGroupBox>
#include <QGridLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QStandardItemModel>

//clazy:excludeall=non-pod-global-static

static const QTime time_23_59(23, 59);


const int AlarmTimeWidget::maxDelayTime = 999*60 + 59;    // < 1000 hours

QString AlarmTimeWidget::i18n_TimeAfterPeriod()
{
    return i18nc("@info", "Enter the length of time (in hours and minutes) after "
                "the current time to schedule the alarm.");
}


/******************************************************************************
* Construct a widget with a group box and title.
*/
AlarmTimeWidget::AlarmTimeWidget(const QString& groupBoxTitle, Mode mode, QWidget* parent)
    : QFrame(parent)
{
    init(mode, groupBoxTitle);
}

/******************************************************************************
* Construct a widget without a group box or title.
*/
AlarmTimeWidget::AlarmTimeWidget(Mode mode, QWidget* parent)
    : QFrame(parent)
{
    init(mode);
}

void AlarmTimeWidget::init(Mode mode, const QString& title)
{
    static const QString recurText = i18nc("@info",
                                           "If a recurrence is configured, the start date/time will be adjusted "
                                           "to the first recurrence on or after the entered date/time.");
    static const QString tzText = i18nc("@info",
                                        "This uses KAlarm's default time zone, set in the Configuration dialog.");

    QWidget* topWidget;
    if (title.isEmpty())
        topWidget = this;
    else
    {
        QBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        topWidget = new QGroupBox(title, this);
        layout->addWidget(topWidget);
    }
    mDeferring = mode & DEFER_TIME;
    mButtonGroup = new ButtonGroup(this);
    connect(mButtonGroup, &ButtonGroup::buttonSet, this, &AlarmTimeWidget::slotButtonSet);
    auto topLayout = new QVBoxLayout(topWidget);
    if (title.isEmpty())
        topLayout->setContentsMargins(0, 0, 0, 0);

    // At time radio button/label
    mAtTimeRadio = new RadioButton((mDeferring ? i18nc("@option:radio", "Defer until:") : i18nc("@option:radio", "At date/time:")), topWidget);
    mAtTimeRadio->setWhatsThis(mDeferring ? i18nc("@info:whatsthis", "Reschedule the alarm to the specified date and time.")
                                          : i18nc("@info:whatsthis", "Specify the date, or date and time, to schedule the alarm."));
    mButtonGroup->addButton(mAtTimeRadio);

    // Date edit box
    mDateEdit = new KDateComboBox(topWidget);
    mDateEdit->setOptions(KDateComboBox::EditDate | KDateComboBox::SelectDate | KDateComboBox::DatePicker);
    connect(mDateEdit, &KDateComboBox::dateChanged, this, &AlarmTimeWidget::dateTimeChanged);
    connect(mDateEdit, &KDateComboBox::dateEdited, this, &AlarmTimeWidget::dateTimeChanged);
    mDateEdit->setWhatsThis(xi18nc("@info:whatsthis",
                                   "<para>Enter the date to schedule the alarm.</para>"
                                   "<para>%1</para>", (mDeferring ? tzText : recurText)));
    mAtTimeRadio->setFocusWidget(mDateEdit);

    // Time edit box and Any time checkbox
    QWidget* timeBox = new QWidget(topWidget);
    auto timeBoxHLayout = new QHBoxLayout(timeBox);
    timeBoxHLayout->setContentsMargins(0, 0, 0, 0);
    mTimeEdit = new TimeEdit(timeBox);
    timeBoxHLayout->addWidget(mTimeEdit);
    connect(mTimeEdit, &TimeEdit::valueChanged, this, &AlarmTimeWidget::dateTimeChanged);
    mTimeEdit->setWhatsThis(xi18nc("@info:whatsthis",
                                   "<para>Enter the time to schedule the alarm.</para>"
                                   "<para>%1</para>"
                                   "<para>%2</para>", (mDeferring ? tzText : recurText), TimeSpinBox::shiftWhatsThis()));

    mAnyTime = -1;    // current status is uninitialised
    if (mode == DEFER_TIME)
    {
        mAnyTimeAllowed = false;
        mAnyTimeCheckBox = nullptr;
    }
    else
    {
        mAnyTimeAllowed = true;
        mAnyTimeCheckBox = new CheckBox(i18nc("@option:check", "Any time"), timeBox);
        timeBoxHLayout->addWidget(mAnyTimeCheckBox);
        connect(mAnyTimeCheckBox, &CheckBox::toggled, this, &AlarmTimeWidget::slotAnyTimeToggled);
        mAnyTimeCheckBox->setToolTip(i18nc("@info:tooltip", "Set only a date (without a time) for the alarm"));
        mAnyTimeCheckBox->setWhatsThis(i18nc("@info:whatsthis",
              "Check to specify only a date (without a time) for the alarm. The alarm will trigger at the first opportunity on the selected date."));
    }

    // 'Time from now' radio button/label
    mAfterTimeRadio = new RadioButton((mDeferring ? i18nc("@option:radio Defer for time interval", "Defer for:") : i18nc("@option:radio", "Time from now:")), topWidget);
    mAfterTimeRadio->setWhatsThis(mDeferring ? i18nc("@info:whatsthis", "Reschedule the alarm for the specified time interval after now.")
                                             : i18nc("@info:whatsthis", "Schedule the alarm after the specified time interval from now."));
    mButtonGroup->addButton(mAfterTimeRadio);

    if (mDeferring)
    {
        // Delay time period
        mDelayTimePeriod = new TimePeriod(TimePeriod::ShowMinutes, topWidget);
        mDelayTimePeriod->setPeriod(0, false, TimePeriod::HoursMinutes);
        connect(mDelayTimePeriod, &TimePeriod::valueChanged,
                this, [this](const KCalendarCore::Duration& period) { delayTimeChanged(period.asSeconds()/60); });
        mDelayTimePeriod->setWhatsThis(xi18nc("@info:whatsthis", "<para>%1</para><para>%2</para>", i18n_TimeAfterPeriod(), TimeSpinBox::shiftWhatsThis()));
        mAfterTimeRadio->setFocusWidget(mDelayTimePeriod);

        // Delay presets
        mPresetsCombo = new ComboBox(topWidget);
        mPresetsCombo->setEditable(false);
        mPresetsCombo->setPlaceholderText(i18nc("@item:inlistbox", "Preset"));
        const KLocalizedString minutesText = ki18ncp("@item:inlistbox", "1 minute", "%1 minutes");
        const KLocalizedString hoursText   = ki18ncp("@item:inlistbox", "1 hour", "%1 hours");
        mPresetsCombo->addItem(minutesText.subs(5).toString(),   5);
        mPresetsCombo->addItem(minutesText.subs(10).toString(), 10);
        mPresetsCombo->addItem(minutesText.subs(15).toString(), 15);
        mPresetsCombo->addItem(minutesText.subs(30).toString(), 30);
        mPresetsCombo->addItem(minutesText.subs(45).toString(), 45);
        mPresetsCombo->addItem(hoursText.subs(1).toString(),    60);
        mPresetsCombo->addItem(hoursText.subs(3).toString(),   180);
        mPresetsCombo->addItem(i18nc("@item:inlistbox", "1 day"), 1440);
        mPresetsCombo->addItem(i18nc("@item:inlistbox", "1 week"), 7*1440);
        mPresetsCombo->setCurrentIndex(-1);
        connect(mPresetsCombo, &ComboBox::activated, this, &AlarmTimeWidget::slotPresetSelected);
    }
    else
    {
        // Delay time spin box
        mDelayTimeEdit = new TimeSpinBox(1, maxDelayTime, topWidget);
        mDelayTimeEdit->setValue(1439);
        connect(mDelayTimeEdit, &TimeSpinBox::valueChanged, this, &AlarmTimeWidget::delayTimeChanged);
        mDelayTimeEdit->setWhatsThis(xi18nc("@info:whatsthis", "<para>%1</para><para>%2</para><para>%3</para>", i18n_TimeAfterPeriod(), recurText, TimeSpinBox::shiftWhatsThis()));
        mAfterTimeRadio->setFocusWidget(mDelayTimeEdit);
    }

    // Set up the layout
    auto grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    topLayout->addLayout(grid);
    const int atRow    = mDeferring ? 2 : 0;
    const int afterRow = mDeferring ? 0 : 2;
    grid->addWidget(mAtTimeRadio, atRow, 0, Qt::AlignLeft);
    auto hLayout = new QHBoxLayout;
    hLayout->addWidget(mDateEdit);
    hLayout->addSpacing(style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
    hLayout->addWidget(timeBox);
    grid->addLayout(hLayout, atRow, 1, Qt::AlignLeft);
    grid->setRowStretch(1, 1);
    grid->addWidget(mAfterTimeRadio, afterRow, 0, Qt::AlignLeft);
    if (mDelayTimeEdit)
        grid->addWidget(mDelayTimeEdit, 2, 1, Qt::AlignLeft);
    else
    {
        hLayout = new QHBoxLayout;
        hLayout->addWidget(mDelayTimePeriod);
        hLayout->addSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
        hLayout->addWidget(mPresetsCombo);
        grid->addLayout(hLayout, 0, 1, Qt::AlignLeft);
    }

    if (!mDeferring)
    {
        // Time zone selection push button
        mTimeZoneButton = new PushButton(i18nc("@action:button", "Time Zone..."), topWidget);
        connect(mTimeZoneButton, &PushButton::clicked, this, &AlarmTimeWidget::showTimeZoneSelector);
        mTimeZoneButton->setToolTip(i18nc("@info:tooltip", "Choose a time zone for this alarm"));
        mTimeZoneButton->setWhatsThis(i18nc("@info:whatsthis",
              "Choose a time zone for this alarm which is different from the default time zone set in KAlarm's configuration dialog."));
        grid->addWidget(mTimeZoneButton, 2, 2, 1, 2, Qt::AlignRight);

        grid->setColumnStretch(2, 1);
        topLayout->addStretch();

        auto layout = new QHBoxLayout();
        topLayout->addLayout(layout);
        layout->setSpacing(2 * style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));

        // Time zone selector
        mTimeZoneBox = new QWidget(topWidget);   // this is to control the QWhatsThis text display area
        auto hlayout = new QHBoxLayout(mTimeZoneBox);
        hlayout->setContentsMargins(0, 0, 0, 0);
        QLabel* label = new QLabel(i18nc("@label:listbox", "Time zone:"), mTimeZoneBox);
        hlayout->addWidget(label);
        mTimeZone = new TimeZoneCombo(mTimeZoneBox);
        hlayout->addWidget(mTimeZone);
        mTimeZone->setMaxVisibleItems(15);
        connect(mTimeZone, &TimeZoneCombo::activated, this, &AlarmTimeWidget::slotTimeZoneChanged);
        mTimeZoneBox->setWhatsThis(i18nc("@info:whatsthis", "Select the time zone to use for this alarm."));
        label->setBuddy(mTimeZone);
        layout->addWidget(mTimeZoneBox);
        layout->addStretch();

        // Initially show only the time zone button, not time zone selector
        mTimeZoneBox->hide();
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
    mDateEdit->setOptions(ro ? KDateComboBox::Options{} : KDateComboBox::EditDate | KDateComboBox::SelectDate | KDateComboBox::DatePicker);
    mTimeEdit->setReadOnly(ro);
    if (mAnyTimeCheckBox)
        mAnyTimeCheckBox->setReadOnly(ro);
    mAfterTimeRadio->setReadOnly(ro);
    if (!mDeferring)
        mTimeZone->setReadOnly(ro);
    if (mDelayTimeEdit)
        mDelayTimeEdit->setReadOnly(ro);
    else
        mDelayTimePeriod->setReadOnly(ro);
}

/******************************************************************************
* Select the "Time from now" radio button, or if minutes < 0, select
* 'At date/time'.
*/
void AlarmTimeWidget::selectTimeFromNow(int minutes)
{
    if (minutes >= 0)
    {
        mAfterTimeRadio->setChecked(true);
        if (minutes > 0)
        {
            if (mDelayTimeEdit)
                mDelayTimeEdit->setValue(minutes);
            else
                mDelayTimePeriod->setMinutes(minutes);
        }
    }
    else
        mAtTimeRadio->setChecked(true);
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
KADateTime AlarmTimeWidget::getDateTime(int* minsFromNow, bool checkExpired, bool showErrorMessage, QWidget** errorWidget) const
{
    if (minsFromNow)
        *minsFromNow = 0;
    if (errorWidget)
        *errorWidget = nullptr;
    KADateTime now = KADateTime::currentUtcDateTime();
    now.setTime(QTime(now.time().hour(), now.time().minute(), 0));
    if (!mAtTimeRadio->isChecked())
    {
        // A relative time has been entered.
        if ((mDelayTimeEdit  &&  !mDelayTimeEdit->isValid())
        ||  (mDelayTimePeriod  &&  !mDelayTimePeriod->isValid()))
        {
            if (showErrorMessage)
                KAMessageBox::error(const_cast<AlarmTimeWidget*>(this), i18nc("@info", "Invalid time"));
            if (errorWidget)
                *errorWidget = mDelayTimeEdit ? (QWidget*)mDelayTimeEdit : (QWidget*)mDelayTimePeriod;
            return {};
        }
        const int delayMins = mDelayTimeEdit ? mDelayTimeEdit->value() : mDelayTimePeriod->minutes();
        if (minsFromNow)
            *minsFromNow = delayMins;
        return now.addSecs(delayMins * 60).toTimeSpec(mTimeSpec);
    }
    else
    {
        // An absolute time has been entered.
        const bool dateOnly = mAnyTimeAllowed && mAnyTimeCheckBox && mAnyTimeCheckBox->isChecked();
        if (!mDateEdit->date().isValid()  ||  !mTimeEdit->isValid())
        {
            // The date and/or time is invalid
            if (!mDateEdit->date().isValid())
            {
                if (showErrorMessage)
                    KAMessageBox::error(const_cast<AlarmTimeWidget*>(this), i18nc("@info", "Invalid date"));
                if (errorWidget)
                    *errorWidget = mDateEdit;
            }
            else
            {
                if (showErrorMessage)
                    KAMessageBox::error(const_cast<AlarmTimeWidget*>(this), i18nc("@info", "Invalid time"));
                if (errorWidget)
                    *errorWidget = mTimeEdit;
            }
            return {};
        }

        KADateTime result;
        if (dateOnly)
        {
            result = KADateTime(mDateEdit->date(), mTimeSpec);
            if (checkExpired  &&  result.date() < now.date())
            {
                if (showErrorMessage)
                    KAMessageBox::error(const_cast<AlarmTimeWidget*>(this), i18nc("@info", "Alarm date has already expired"));
                if (errorWidget)
                    *errorWidget = mDateEdit;
                return {};
            }
        }
        else
        {
            result = KADateTime(mDateEdit->date(), mTimeEdit->time(), mTimeSpec);
            if (checkExpired  &&  result <= now.addSecs(1))
            {
                if (showErrorMessage)
                    KAMessageBox::error(const_cast<AlarmTimeWidget*>(this), i18nc("@info", "Alarm time has already expired"));
                if (errorWidget)
                    *errorWidget = mTimeEdit;
                return {};
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
        mTimeSpec = dt.timeSpec().isValid() ? dt.timeSpec() : KADateTime::LocalZone;
    else
    {
        const QTimeZone tz = (dt.timeSpec() == KADateTime::LocalZone) ? QTimeZone() : dt.timeZone();
        mTimeZone->setTimeZone(tz);
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
        if (mDelayTimeEdit)
            mDelayTimeEdit->setValid(false);
        else
            mDelayTimePeriod->setValid(false);
    }
    if (mAnyTimeCheckBox)
    {
        const bool dateOnly = dt.isDateOnly();
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
    mMinDateTime = KADateTime();
    const KADateTime now = KADateTime::currentDateTime(mTimeSpec);
    mDateEdit->setMinimumDate(now.date());
    setMaxMinTimeIf(now);
}

/******************************************************************************
* Set the minimum date/time, adjusting the entered date/time if necessary.
* If 'dt' is invalid, any current minimum date/time is cleared.
*/
void AlarmTimeWidget::setMinDateTime(const KADateTime& dt)
{
    mMinDateTimeIsNow = false;
    mMinDateTime = dt.toTimeSpec(mTimeSpec);
    mDateEdit->setMinimumDate(mMinDateTime.date());
    setMaxMinTimeIf(KADateTime::currentDateTime(mTimeSpec));
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
    const KADateTime now = KADateTime::currentDateTime(mTimeSpec);
    setMaxMinTimeIf(now);
    setMaxDelayTime(now);
}

/******************************************************************************
* If the minimum and maximum date/times fall on the same date, set the minimum
* and maximum times in the time edit box.
*/
void AlarmTimeWidget::setMaxMinTimeIf(const KADateTime& now)
{
    int   mint = 0;
    QTime maxt = time_23_59;
    mMinMaxTimeSet = false;
    if (mMaxDateTime.isValid())
    {
        bool set = true;
        KADateTime minDT;
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
void AlarmTimeWidget::setMaxDelayTime(const KADateTime& now)
{
    int maxVal = maxDelayTime;
    if (mMaxDateTime.isValid())
    {
        if (now.date().daysTo(mMaxDateTime.date()) < 100)    // avoid possible 32-bit overflow on secsTo()
        {
            KADateTime dt(now);
            dt.setTime(QTime(now.time().hour(), now.time().minute(), 0));   // round down to nearest minute
            maxVal = dt.secsTo(mMaxDateTime) / 60;
            if (maxVal > maxDelayTime)
                maxVal = maxDelayTime;
        }
    }
    if (mDelayTimeEdit)
        mDelayTimeEdit->setMaximum(maxVal);
    else
    {
        mDelayTimePeriod->setMaxMinutes(maxVal);
        // Disable all presets greater than the maximum delay minutes
        auto* model = qobject_cast<QStandardItemModel*>(mPresetsCombo->model());
        if (model)
        {
            for (int i = 0, count = mPresetsCombo->count();  i < count;  ++i)
            {
                const int minutes = mPresetsCombo->itemData(i).toInt();
                model->item(i)->setEnabled(minutes <= maxVal);
            }
        }
    }
}

/******************************************************************************
* Set the status for whether a time is specified, or just a date.
*/
void AlarmTimeWidget::setAnyTime()
{
    const int old = mAnyTime;
    mAnyTime = (mAtTimeRadio->isChecked() && mAnyTimeAllowed && mAnyTimeCheckBox && mAnyTimeCheckBox->isChecked()) ? 1 : 0;
    if (mAnyTime != old)
        Q_EMIT dateOnlyToggled(mAnyTime);
}

/******************************************************************************
* Enable/disable the "date only" radio button.
*/
void AlarmTimeWidget::enableAnyTime(bool enable)
{
    if (mAnyTimeCheckBox)
    {
        mAnyTimeAllowed = enable;
        const bool at = mAtTimeRadio->isChecked();
        mAnyTimeCheckBox->setEnabled(enable && at);
        if (at)
            mTimeEdit->setEnabled(!enable || !mAnyTimeCheckBox->isChecked());
        setAnyTime();
    }
}

/******************************************************************************
* Set the keyboard focus to the time from now field.
*/
void AlarmTimeWidget::focusTimeFromNow()
{
    if (!mAtTimeRadio->isChecked())
    {
        if (mDelayTimeEdit)
            mDelayTimeEdit->setFocus();
        else
            mDelayTimePeriod->setFocus();
    }
}

/******************************************************************************
* Called every minute to update the alarm time data entry fields.
* If the maximum date/time has been reached, a 'pastMax()' signal is emitted.
*/
void AlarmTimeWidget::updateTimes()
{
    KADateTime now;
    if (mMinDateTimeIsNow)
    {
        // Make sure that the minimum date is updated when the day changes
        now = KADateTime::currentDateTime(mTimeSpec);
        mDateEdit->setMinimumDate(now.date());
    }
    if (mMaxDateTime.isValid())
    {
        if (!now.isValid())
            now = KADateTime::currentDateTime(mTimeSpec);
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
                    Q_EMIT pastMax();
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
    {
        if (mDelayTimeEdit)
            delayTimeChanged(mDelayTimeEdit->value());
        else
            delayTimeChanged(mDelayTimePeriod->minutes());
    }
}


/******************************************************************************
* Called when the radio button states have been changed.
* Updates the appropriate edit box.
*/
void AlarmTimeWidget::slotButtonSet(QAbstractButton*)
{
    const bool at = mAtTimeRadio->isChecked();
    mDateEdit->setEnabled(at);
    mTimeEdit->setEnabled(at && (!mAnyTimeAllowed || !mAnyTimeCheckBox || !mAnyTimeCheckBox->isChecked()));
    if (mAnyTimeCheckBox)
        mAnyTimeCheckBox->setEnabled(at && mAnyTimeAllowed);
    // Ensure that the value of the delay edit box is > 0.
    const KADateTime att(mDateEdit->date(), mTimeEdit->time(), mTimeSpec);
    const int minutes = (KADateTime::currentUtcDateTime().secsTo(att) + 59) / 60;
    if (mDelayTimeEdit)
    {
        if (minutes <= 0)
            mDelayTimeEdit->setValid(true);
        mDelayTimeEdit->setEnabled(!at);
    }
    else
    {
        if (minutes <= 0)
            mDelayTimePeriod->setValid(true);
        mDelayTimePeriod->setEnabled(!at);
        mPresetsCombo->setEnabled(!at);
        QPalette pal = mPresetsCombo->palette();
        pal.setColor(QPalette::PlaceholderText, pal.color(at ? QPalette::Disabled : QPalette::Active, QPalette::Text));
        mPresetsCombo->setPalette(pal);
    }
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
        Q_EMIT changed(KADateTime(mDateEdit->date(), mTimeSpec));
    else
        Q_EMIT changed(KADateTime(mDateEdit->date(), mTimeEdit->time(), mTimeSpec));
}

/******************************************************************************
* Called after a new selection has been made in the time zone combo box.
* Re-evaluates the time specification to use.
*/
void AlarmTimeWidget::slotTimeZoneChanged()
{
    const QTimeZone tz = mTimeZone->timeZone();
    mTimeSpec = tz.isValid() ? KADateTime::Spec(tz) : KADateTime::LocalZone;
    if (!mTimeZoneBox->isVisible()  &&  mTimeSpec != Preferences::timeSpec())
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
* Called after the mTimeZoneButton button has been clicked.
* Show the time zone selection controls, and hide the button.
*/
void AlarmTimeWidget::showTimeZoneSelector()
{
    mTimeZoneButton->hide();
    mTimeZoneBox->show();
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
    const KADateTime dt(mDateEdit->date(), mTimeEdit->time(), mTimeSpec);
    const int minutes = (KADateTime::currentUtcDateTime().secsTo(dt) + 59) / 60;
    if (mDelayTimeEdit)
    {
        const bool blocked = mDelayTimeEdit->signalsBlocked();
        mDelayTimeEdit->blockSignals(true);     // prevent infinite recursion between here and delayTimeChanged()
        if (minutes <= 0  ||  minutes > mDelayTimeEdit->maximum())
            mDelayTimeEdit->setValid(false);
        else
            mDelayTimeEdit->setValue(minutes);
        mDelayTimeEdit->blockSignals(blocked);
    }
    else
    {
        const bool blocked = mDelayTimePeriod->signalsBlocked();
        mDelayTimePeriod->blockSignals(true);     // prevent infinite recursion between here and delayTimeChanged()
        if (minutes <= 0  ||  minutes > mDelayTimePeriod->maxMinutes())
            mDelayTimePeriod->setValid(false);
        else
            mDelayTimePeriod->setMinutes(minutes);
        mDelayTimePeriod->blockSignals(blocked);
    }
    if (mAnyTimeAllowed && mAnyTimeCheckBox && mAnyTimeCheckBox->isChecked())
        Q_EMIT changed(KADateTime(dt.date(), mTimeSpec));
    else
        Q_EMIT changed(dt);
}

/******************************************************************************
* Called when the delay time edit box value has changed.
* Updates the Date and Time edit boxes accordingly.
*/
void AlarmTimeWidget::delayTimeChanged(int minutes)
{
    if ((mDelayTimeEdit  &&  mDelayTimeEdit->isValid())
    ||  (mDelayTimePeriod  &&  mDelayTimePeriod->isValid()))
    {
        QDateTime dt = KADateTime::currentUtcDateTime().addSecs(minutes * 60).toTimeSpec(mTimeSpec).qDateTime();
        const bool blockedT = mTimeEdit->signalsBlocked();
        const bool blockedD = mDateEdit->signalsBlocked();
        mTimeEdit->blockSignals(true);     // prevent infinite recursion between here and dateTimeChanged()
        mDateEdit->blockSignals(true);
        mTimeEdit->setValue(dt.time());
        mDateEdit->setDate(dt.date());
        mTimeEdit->blockSignals(blockedT);
        mDateEdit->blockSignals(blockedD);
        Q_EMIT changed(KADateTime(dt.date(), dt.time(), mTimeSpec));
    }
}

/******************************************************************************
* Called when a new item is selected in the presets combo box.
*/
void AlarmTimeWidget::slotPresetSelected(int index)
{
    const int minutes = mPresetsCombo->itemData(index).toInt();
    if (minutes > 0)
    {
        switch (mDelayTimePeriod->units())
        {
            case TimePeriod::Minutes:
            case TimePeriod::HoursMinutes:
                if (minutes < 1440)
                    mDelayTimePeriod->setMinutes(minutes);
                else
                    mDelayTimePeriod->setMinutes(minutes, TimePeriod::Days);
                break;
            case TimePeriod::Days:
            case TimePeriod::Weeks:
                if (minutes >= 1440)
                    mDelayTimePeriod->setMinutes(minutes);
                else
                    mDelayTimePeriod->setMinutes(minutes, TimePeriod::HoursMinutes);
                break;
        }
    }
    mPresetsCombo->blockSignals(true);
    mPresetsCombo->setCurrentIndex(-1);
    mPresetsCombo->blockSignals(false);
}

#include "moc_alarmtimewidget.cpp"

// vim: et sw=4:
