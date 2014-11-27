/*
 *  recurrenceedit.cpp  -  widget to edit the event's recurrence definition
 *  Program:  kalarm
 *  Copyright © 2002-2011 by David Jarvie <djarvie@kde.org>
 *
 *  Based originally on KOrganizer module koeditorrecurrence.cpp,
 *  Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
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
#include "recurrenceedit.h"
#include "recurrenceedit_p.h"

#include "alarmtimewidget.h"
#include "checkbox.h"
#include "combobox.h"
#include "kalarmapp.h"
#include "kalocale.h"
#include "preferences.h"
#include "radiobutton.h"
#include "repetitionbutton.h"
#include "spinbox.h"
#include "timeedit.h"
#include "timespinbox.h"
#include "buttongroup.h"

#include <kalarmcal/kaevent.h>
#include <kalarmcal/karecurrence.h>

#include <KCalCore/Event>
using namespace KCalCore;

#include <kglobal.h>
#include <KLocalizedString>
#include <kcalendarsystem.h>
#include <kiconloader.h>
#include <kdialog.h>
#include <kmessagebox.h>
#include <kdatecombobox.h>
#include <KHBox>

#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QListWidget>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QtAlgorithms>
#include <qdebug.h>


class ListWidget : public QListWidget
{
    public:
        explicit ListWidget(QWidget* parent) : QListWidget(parent) {}
        virtual QSize sizeHint() const  { return minimumSizeHint(); }
};

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString RecurrenceEdit::i18n_combo_NoRecur()        { return i18nc("@item:inlistbox Recurrence type", "No Recurrence"); }
QString RecurrenceEdit::i18n_combo_AtLogin()        { return i18nc("@item:inlistbox Recurrence type", "At Login"); }
QString RecurrenceEdit::i18n_combo_HourlyMinutely() { return i18nc("@item:inlistbox Recurrence type", "Hourly/Minutely"); }
QString RecurrenceEdit::i18n_combo_Daily()          { return i18nc("@item:inlistbox Recurrence type", "Daily"); }
QString RecurrenceEdit::i18n_combo_Weekly()         { return i18nc("@item:inlistbox Recurrence type", "Weekly"); }
QString RecurrenceEdit::i18n_combo_Monthly()        { return i18nc("@item:inlistbox Recurrence type", "Monthly"); }
QString RecurrenceEdit::i18n_combo_Yearly()         { return i18nc("@item:inlistbox Recurrence type", "Yearly"); }


RecurrenceEdit::RecurrenceEdit(bool readOnly, QWidget* parent)
    : QFrame(parent),
      mRule(0),
      mRuleButtonType(INVALID_RECUR),
      mDailyShown(false),
      mWeeklyShown(false),
      mMonthlyShown(false),
      mYearlyShown(false),
      mNoEmitTypeChanged(true),
      mReadOnly(readOnly)
{
    qDebug();
    QVBoxLayout* topLayout = new QVBoxLayout(this);
    topLayout->setMargin(0);
    topLayout->setSpacing(KDialog::spacingHint());

    /* Create the recurrence rule Group box which holds the recurrence period
     * selection buttons, and the weekly, monthly and yearly recurrence rule
     * frames which specify options individual to each of these distinct
     * sections of the recurrence rule. Each frame is made visible by the
     * selection of its corresponding radio button.
     */

    QGroupBox* recurGroup = new QGroupBox(i18nc("@title:group", "Recurrence Rule"), this);
    topLayout->addWidget(recurGroup);
    QHBoxLayout* hlayout = new QHBoxLayout(recurGroup);
    hlayout->setMargin(KDialog::marginHint());
    hlayout->setSpacing(KDialog::marginHint());   // use margin spacing due to vertical divider line

    // Recurrence period radio buttons
    QVBoxLayout* vlayout = new QVBoxLayout();
    vlayout->setSpacing(0);
    vlayout->setMargin(0);
    hlayout->addLayout(vlayout);
    mRuleButtonGroup = new ButtonGroup(recurGroup);
    connect(mRuleButtonGroup, &ButtonGroup::buttonSet, this, &RecurrenceEdit::periodClicked);
    connect(mRuleButtonGroup, &ButtonGroup::buttonSet, this, &RecurrenceEdit::contentsChanged);

    mNoneButton = new RadioButton(i18n_combo_NoRecur(), recurGroup);
    mNoneButton->setFixedSize(mNoneButton->sizeHint());
    mNoneButton->setReadOnly(mReadOnly);
    mNoneButton->setWhatsThis(i18nc("@info:whatsthis", "Do not repeat the alarm"));
    mRuleButtonGroup->addButton(mNoneButton);
    vlayout->addWidget(mNoneButton);

    mAtLoginButton = new RadioButton(i18n_combo_AtLogin(), recurGroup);
    mAtLoginButton->setFixedSize(mAtLoginButton->sizeHint());
    mAtLoginButton->setReadOnly(mReadOnly);
    mAtLoginButton->setWhatsThis(xi18nc("@info:whatsthis",
                                      "<para>Trigger the alarm at the specified date/time and at every login until then.</para>"
                                      "<para>Note that it will also be triggered any time <application>KAlarm</application> is restarted.</para>"));
    mRuleButtonGroup->addButton(mAtLoginButton);
    vlayout->addWidget(mAtLoginButton);

    mSubDailyButton = new RadioButton(i18n_combo_HourlyMinutely(), recurGroup);
    mSubDailyButton->setFixedSize(mSubDailyButton->sizeHint());
    mSubDailyButton->setReadOnly(mReadOnly);
    mSubDailyButton->setWhatsThis(i18nc("@info:whatsthis", "Repeat the alarm at hourly/minutely intervals"));
    mRuleButtonGroup->addButton(mSubDailyButton);
    vlayout->addWidget(mSubDailyButton);

    mDailyButton = new RadioButton(i18n_combo_Daily(), recurGroup);
    mDailyButton->setFixedSize(mDailyButton->sizeHint());
    mDailyButton->setReadOnly(mReadOnly);
    mDailyButton->setWhatsThis(i18nc("@info:whatsthis", "Repeat the alarm at daily intervals"));
    mRuleButtonGroup->addButton(mDailyButton);
    vlayout->addWidget(mDailyButton);

    mWeeklyButton = new RadioButton(i18n_combo_Weekly(), recurGroup);
    mWeeklyButton->setFixedSize(mWeeklyButton->sizeHint());
    mWeeklyButton->setReadOnly(mReadOnly);
    mWeeklyButton->setWhatsThis(i18nc("@info:whatsthis", "Repeat the alarm at weekly intervals"));
    mRuleButtonGroup->addButton(mWeeklyButton);
    vlayout->addWidget(mWeeklyButton);

    mMonthlyButton = new RadioButton(i18n_combo_Monthly(), recurGroup);
    mMonthlyButton->setFixedSize(mMonthlyButton->sizeHint());
    mMonthlyButton->setReadOnly(mReadOnly);
    mMonthlyButton->setWhatsThis(i18nc("@info:whatsthis", "Repeat the alarm at monthly intervals"));
    mRuleButtonGroup->addButton(mMonthlyButton);
    vlayout->addWidget(mMonthlyButton);

    mYearlyButton = new RadioButton(i18n_combo_Yearly(), recurGroup);
    mYearlyButton->setFixedSize(mYearlyButton->sizeHint());
    mYearlyButton->setReadOnly(mReadOnly);
    mYearlyButton->setWhatsThis(i18nc("@info:whatsthis", "Repeat the alarm at annual intervals"));
    mRuleButtonGroup->addButton(mYearlyButton);
    vlayout->addWidget(mYearlyButton);
    vlayout->addStretch();    // top-adjust the interval radio buttons

    // Sub-repetition button
    mSubRepetition = new RepetitionButton(i18nc("@action:button", "Sub-Repetition"), true, recurGroup);
    mSubRepetition->setFixedSize(mSubRepetition->sizeHint());
    mSubRepetition->setReadOnly(mReadOnly);
    mSubRepetition->setWhatsThis(i18nc("@info:whatsthis",
                                       "Set up a repetition within the recurrence, to trigger the alarm multiple times each time the recurrence is due."));
    connect(mSubRepetition, &RepetitionButton::needsInitialisation, this, &RecurrenceEdit::repeatNeedsInitialisation);
    connect(mSubRepetition, &RepetitionButton::changed, this, &RecurrenceEdit::frequencyChanged);
    connect(mSubRepetition, &RepetitionButton::changed, this, &RecurrenceEdit::contentsChanged);
    vlayout->addSpacing(KDialog::spacingHint());
    vlayout->addWidget(mSubRepetition);

    // Vertical divider line
    vlayout = new QVBoxLayout();
    vlayout->setMargin(0);
    hlayout->addLayout(vlayout);
    QFrame* divider = new QFrame(recurGroup);
    divider->setFrameStyle(QFrame::VLine | QFrame::Sunken);
    vlayout->addWidget(divider, 1);

    // Rule definition stack
    mRuleStack = new QStackedWidget(recurGroup);
    hlayout->addWidget(mRuleStack);
    hlayout->addStretch(1);
    mNoRule       = new NoRule(mRuleStack);
    mSubDailyRule = new SubDailyRule(mReadOnly, mRuleStack);
    mDailyRule    = new DailyRule(mReadOnly, mRuleStack);
    mWeeklyRule   = new WeeklyRule(mReadOnly, mRuleStack);
    mMonthlyRule  = new MonthlyRule(mReadOnly, mRuleStack);
    mYearlyRule   = new YearlyRule(mReadOnly, mRuleStack);

    connect(mSubDailyRule, &SubDailyRule::frequencyChanged, this, &RecurrenceEdit::frequencyChanged);
    connect(mDailyRule, &DailyRule::frequencyChanged, this, &RecurrenceEdit::frequencyChanged);
    connect(mWeeklyRule, &WeeklyRule::frequencyChanged, this, &RecurrenceEdit::frequencyChanged);
    connect(mMonthlyRule, &MonthlyRule::frequencyChanged, this, &RecurrenceEdit::frequencyChanged);
    connect(mYearlyRule, &YearlyRule::frequencyChanged, this, &RecurrenceEdit::frequencyChanged);
    connect(mSubDailyRule, &SubDailyRule::changed, this, &RecurrenceEdit::contentsChanged);
    connect(mDailyRule, &DailyRule::changed, this, &RecurrenceEdit::contentsChanged);
    connect(mWeeklyRule, &WeeklyRule::changed, this, &RecurrenceEdit::contentsChanged);
    connect(mMonthlyRule, &MonthlyRule::changed, this, &RecurrenceEdit::contentsChanged);
    connect(mYearlyRule, &YearlyRule::changed, this, &RecurrenceEdit::contentsChanged);

    mRuleStack->addWidget(mNoRule);
    mRuleStack->addWidget(mSubDailyRule);
    mRuleStack->addWidget(mDailyRule);
    mRuleStack->addWidget(mWeeklyRule);
    mRuleStack->addWidget(mMonthlyRule);
    mRuleStack->addWidget(mYearlyRule);
    hlayout->addSpacing(KDialog::marginHint());

    // Create the recurrence range group which contains the controls
    // which specify how long the recurrence is to last.

    mRangeButtonBox = new QGroupBox(i18nc("@title:group", "Recurrence End"), this);
    topLayout->addWidget(mRangeButtonBox);
    mRangeButtonGroup = new ButtonGroup(mRangeButtonBox);
    connect(mRangeButtonGroup, &ButtonGroup::buttonSet, this, &RecurrenceEdit::rangeTypeClicked);
    connect(mRangeButtonGroup, &ButtonGroup::buttonSet, this, &RecurrenceEdit::contentsChanged);

    vlayout = new QVBoxLayout(mRangeButtonBox);
    vlayout->setMargin(KDialog::marginHint());
    vlayout->setSpacing(KDialog::spacingHint());
    mNoEndDateButton = new RadioButton(i18nc("@option:radio", "No end"), mRangeButtonBox);
    mNoEndDateButton->setFixedSize(mNoEndDateButton->sizeHint());
    mNoEndDateButton->setReadOnly(mReadOnly);
    mNoEndDateButton->setWhatsThis(i18nc("@info:whatsthis", "Repeat the alarm indefinitely"));
    mRangeButtonGroup->addButton(mNoEndDateButton);
    vlayout->addWidget(mNoEndDateButton, 1, Qt::AlignLeft);
    QSize size = mNoEndDateButton->size();

    hlayout = new QHBoxLayout();
    hlayout->setMargin(0);
    vlayout->addLayout(hlayout);
    mRepeatCountButton = new RadioButton(i18nc("@option:radio", "End after:"), mRangeButtonBox);
    mRepeatCountButton->setReadOnly(mReadOnly);
    mRepeatCountButton->setWhatsThis(i18nc("@info:whatsthis", "Repeat the alarm for the number of times specified"));
    mRangeButtonGroup->addButton(mRepeatCountButton);
    mRepeatCountEntry = new SpinBox(1, 9999, mRangeButtonBox);
    mRepeatCountEntry->setFixedSize(mRepeatCountEntry->sizeHint());
    mRepeatCountEntry->setSingleShiftStep(10);
    mRepeatCountEntry->setSelectOnStep(false);
    mRepeatCountEntry->setReadOnly(mReadOnly);
    mRepeatCountEntry->setWhatsThis(i18nc("@info:whatsthis", "Enter the total number of times to trigger the alarm"));
    connect(mRepeatCountEntry, static_cast<void (SpinBox::*)(int)>(&SpinBox::valueChanged), this, &RecurrenceEdit::repeatCountChanged);
    connect(mRepeatCountEntry, static_cast<void (SpinBox::*)(int)>(&SpinBox::valueChanged), this, &RecurrenceEdit::contentsChanged);
    mRepeatCountButton->setFocusWidget(mRepeatCountEntry);
    mRepeatCountLabel = new QLabel(i18nc("@label", "occurrence(s)"), mRangeButtonBox);
    mRepeatCountLabel->setFixedSize(mRepeatCountLabel->sizeHint());
    hlayout->addWidget(mRepeatCountButton);
    hlayout->addSpacing(KDialog::spacingHint());
    hlayout->addWidget(mRepeatCountEntry);
    hlayout->addWidget(mRepeatCountLabel);
    hlayout->addStretch();
    size = size.expandedTo(mRepeatCountButton->sizeHint());

    hlayout = new QHBoxLayout();
    hlayout->setMargin(0);
    vlayout->addLayout(hlayout);
    mEndDateButton = new RadioButton(i18nc("@option:radio", "End by:"), mRangeButtonBox);
    mEndDateButton->setReadOnly(mReadOnly);
    mEndDateButton->setWhatsThis(
          xi18nc("@info:whatsthis", "<para>Repeat the alarm until the date/time specified.</para>"
                "<para><note>This applies to the main recurrence only. It does not limit any sub-repetition which will occur regardless after the last main recurrence.</note></para>"));
    mRangeButtonGroup->addButton(mEndDateButton);
    mEndDateEdit = new KDateComboBox(mRangeButtonBox);
    mEndDateEdit->setOptions(mReadOnly ? KDateComboBox::Options(0) : KDateComboBox::EditDate | KDateComboBox::SelectDate | KDateComboBox::DatePicker);
    static const QString tzText = i18nc("@info", "This uses the same time zone as the start time.");
    mEndDateEdit->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>Enter the last date to repeat the alarm.</para><para>%1</para>", tzText));
    connect(mEndDateEdit, &KDateComboBox::dateEdited, this, &RecurrenceEdit::contentsChanged);
    mEndDateButton->setFocusWidget(mEndDateEdit);
    mEndTimeEdit = new TimeEdit(mRangeButtonBox);
    mEndTimeEdit->setFixedSize(mEndTimeEdit->sizeHint());
    mEndTimeEdit->setReadOnly(mReadOnly);
    mEndTimeEdit->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>Enter the last time to repeat the alarm.</para><para>%1</para><para>%2</para>", tzText, TimeSpinBox::shiftWhatsThis()));
    connect(mEndTimeEdit, &TimeEdit::valueChanged, this, &RecurrenceEdit::contentsChanged);
    mEndAnyTimeCheckBox = new CheckBox(i18nc("@option:check", "Any time"), mRangeButtonBox);
    mEndAnyTimeCheckBox->setFixedSize(mEndAnyTimeCheckBox->sizeHint());
    mEndAnyTimeCheckBox->setReadOnly(mReadOnly);
    mEndAnyTimeCheckBox->setWhatsThis(i18nc("@info:whatsthis", "Stop repeating the alarm after your first login on or after the specified end date"));
    connect(mEndAnyTimeCheckBox, &CheckBox::toggled, this, &RecurrenceEdit::slotAnyTimeToggled);
    connect(mEndAnyTimeCheckBox, &CheckBox::toggled, this, &RecurrenceEdit::contentsChanged);
    hlayout->addWidget(mEndDateButton);
    hlayout->addSpacing(KDialog::spacingHint());
    hlayout->addWidget(mEndDateEdit);
    hlayout->addWidget(mEndTimeEdit);
    hlayout->addWidget(mEndAnyTimeCheckBox);
    hlayout->addStretch();
    size = size.expandedTo(mEndDateButton->sizeHint());

    // Line up the widgets to the right of the radio buttons
    mRepeatCountButton->setFixedSize(size);
    mEndDateButton->setFixedSize(size);

    // Create the exceptions group which specifies dates to be excluded
    // from the recurrence.

    mExceptionGroup = new QGroupBox(i18nc("@title:group", "Exceptions"), this);
    topLayout->addWidget(mExceptionGroup);
    topLayout->setStretchFactor(mExceptionGroup, 2);
    hlayout = new QHBoxLayout(mExceptionGroup);
    hlayout->setMargin(KDialog::marginHint());
    hlayout->setSpacing(KDialog::spacingHint());
    vlayout = new QVBoxLayout();
    vlayout->setMargin(0);
    hlayout->addLayout(vlayout);

    mExceptionDateList = new ListWidget(mExceptionGroup);
    mExceptionDateList->setWhatsThis(i18nc("@info:whatsthis", "The list of exceptions, i.e. dates/times excluded from the recurrence"));
    connect(mExceptionDateList, &QListWidget::currentRowChanged, this, &RecurrenceEdit::enableExceptionButtons);
    vlayout->addWidget(mExceptionDateList);

    if (mReadOnly)
    {
        mExceptionDateEdit     = 0;
        mChangeExceptionButton = 0;
        mDeleteExceptionButton = 0;
    }
    else
    {
        vlayout = new QVBoxLayout();
        vlayout->setMargin(0);
        hlayout->addLayout(vlayout);
        mExceptionDateEdit = new KDateComboBox(mExceptionGroup);
        mExceptionDateEdit->setOptions(mReadOnly ? KDateComboBox::Options(0) : KDateComboBox::EditDate | KDateComboBox::SelectDate | KDateComboBox::DatePicker);
        mExceptionDateEdit->setDate(KDateTime::currentLocalDate());
        mExceptionDateEdit->setWhatsThis(i18nc("@info:whatsthis",
              "Enter a date to insert in the exceptions list. "
              "Use in conjunction with the Add or Change button below."));
        vlayout->addWidget(mExceptionDateEdit, 0, Qt::AlignLeft);

        hlayout = new QHBoxLayout();
        hlayout->setMargin(0);
        vlayout->addLayout(hlayout);
        QPushButton* button = new QPushButton(i18nc("@action:button", "Add"), mExceptionGroup);
        button->setWhatsThis(i18nc("@info:whatsthis", "Add the date entered above to the exceptions list"));
        connect(button, &QPushButton::clicked, this, &RecurrenceEdit::addException);
        hlayout->addWidget(button);

        mChangeExceptionButton = new QPushButton(i18nc("@action:button", "Change"), mExceptionGroup);
        mChangeExceptionButton->setWhatsThis(i18nc("@info:whatsthis",
              "Replace the currently highlighted item in the exceptions list with the date entered above"));
        connect(mChangeExceptionButton, &QPushButton::clicked, this, &RecurrenceEdit::changeException);
        hlayout->addWidget(mChangeExceptionButton);

        mDeleteExceptionButton = new QPushButton(i18nc("@action:button", "Delete"), mExceptionGroup);
        mDeleteExceptionButton->setWhatsThis(i18nc("@info:whatsthis", "Remove the currently highlighted item from the exceptions list"));
        connect(mDeleteExceptionButton, &QPushButton::clicked, this, &RecurrenceEdit::deleteException);
        hlayout->addWidget(mDeleteExceptionButton);
    }

    vlayout->addStretch();

    mExcludeHolidays = new CheckBox(i18nc("@option:check", "Exclude holidays"), mExceptionGroup);
    mExcludeHolidays->setReadOnly(mReadOnly);
    mExcludeHolidays->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>Do not trigger the alarm on holidays.</para>"
          "<para>You can specify your holiday region in the Configuration dialog.</para>"));
    connect(mExcludeHolidays, &CheckBox::toggled, this, &RecurrenceEdit::contentsChanged);
    vlayout->addWidget(mExcludeHolidays);

    mWorkTimeOnly = new CheckBox(i18nc("@option:check", "Only during working time"), mExceptionGroup);
    mWorkTimeOnly->setReadOnly(mReadOnly);
    mWorkTimeOnly->setWhatsThis(xi18nc("@info:whatsthis",
          "<para>Only execute the alarm during working hours, on working days.</para>"
          "<para>You can specify working days and hours in the Configuration dialog.</para>"));
    connect(mWorkTimeOnly, &CheckBox::toggled, this, &RecurrenceEdit::contentsChanged);
    vlayout->addWidget(mWorkTimeOnly);

    topLayout->addStretch();
    mNoEmitTypeChanged = false;
}

/******************************************************************************
* Show or hide the exception controls.
*/
void RecurrenceEdit::showMoreOptions(bool more)
{
    if (more)
        mExceptionGroup->show();
    else
        mExceptionGroup->hide();
    updateGeometry();
}

/******************************************************************************
 * Verify the consistency of the entered data.
 * Reply = widget to receive focus on error, or 0 if no error.
 */
QWidget* RecurrenceEdit::checkData(const KDateTime& startDateTime, QString& errorMessage) const
{
    if (mAtLoginButton->isChecked())
        return 0;
    const_cast<RecurrenceEdit*>(this)->mCurrStartDateTime = startDateTime;
    if (mEndDateButton->isChecked())
    {
        // N.B. End date/time takes the same time spec as start date/time
        QWidget* errWidget = 0;
        bool noTime = !mEndTimeEdit->isEnabled();
        QDate endDate = mEndDateEdit->date();
        if (endDate < startDateTime.date())
            errWidget = mEndDateEdit;
        else if (!noTime  &&  QDateTime(endDate, mEndTimeEdit->time()) < startDateTime.dateTime())
            errWidget = mEndTimeEdit;
        if (errWidget)
        {
            errorMessage = noTime
                         ? i18nc("@info", "End date is earlier than start date")
                         : i18nc("@info", "End date/time is earlier than start date/time");
            return errWidget;
        }
    }
    if (!mRule)
        return 0;
    return mRule->validate(errorMessage);
}

/******************************************************************************
 * Called when a recurrence period radio button is clicked.
 */
void RecurrenceEdit::periodClicked(QAbstractButton* button)
{
    RepeatType oldType = mRuleButtonType;
    bool none     = (button == mNoneButton);
    bool atLogin  = (button == mAtLoginButton);
    bool subdaily = (button == mSubDailyButton);
    if (none)
    {
        mRule = 0;
        mRuleButtonType = NO_RECUR;
    }
    else if (atLogin)
    {
        mRule = 0;
        mRuleButtonType = AT_LOGIN;
        mEndDateButton->setChecked(true);
    }
    else if (subdaily)
    {
        mRule = mSubDailyRule;
        mRuleButtonType = SUBDAILY;
    }
    else if (button == mDailyButton)
    {
        mRule = mDailyRule;
        mRuleButtonType = DAILY;
        mDailyShown = true;
    }
    else if (button == mWeeklyButton)
    {
        mRule = mWeeklyRule;
        mRuleButtonType = WEEKLY;
        mWeeklyShown = true;
    }
    else if (button == mMonthlyButton)
    {
        mRule = mMonthlyRule;
        mRuleButtonType = MONTHLY;
        mMonthlyShown = true;
    }
    else if (button == mYearlyButton)
    {
        mRule = mYearlyRule;
        mRuleButtonType = ANNUAL;
        mYearlyShown = true;
    }
    else
        return;

    if (mRuleButtonType != oldType)
    {
        mRuleStack->setCurrentWidget(mRule ? mRule : mNoRule);
        if (oldType == NO_RECUR  ||  none)
            mRangeButtonBox->setEnabled(!none);
        mExceptionGroup->setEnabled(!(none || atLogin));
        mEndAnyTimeCheckBox->setEnabled(atLogin);
        if (!none)
        {
            mNoEndDateButton->setEnabled(!atLogin);
            mRepeatCountButton->setEnabled(!atLogin);
        }
        rangeTypeClicked();
        mSubRepetition->setEnabled(!(none || atLogin));
        if (!mNoEmitTypeChanged)
            emit typeChanged(mRuleButtonType);
    }
}

void RecurrenceEdit::slotAnyTimeToggled(bool on)
{
    QAbstractButton* button = mRuleButtonGroup->checkedButton();
    mEndTimeEdit->setEnabled((button == mAtLoginButton && !on)
                         ||  (button == mSubDailyButton && mEndDateButton->isChecked()));
}

/******************************************************************************
 * Called when a recurrence range type radio button is clicked.
 */
void RecurrenceEdit::rangeTypeClicked()
{
    bool endDate = mEndDateButton->isChecked();
    mEndDateEdit->setEnabled(endDate);
    mEndTimeEdit->setEnabled(endDate
                             &&  ((mAtLoginButton->isChecked() && !mEndAnyTimeCheckBox->isChecked())
                                  ||  mSubDailyButton->isChecked()));
    bool repeatCount = mRepeatCountButton->isChecked();
    mRepeatCountEntry->setEnabled(repeatCount);
    mRepeatCountLabel->setEnabled(repeatCount);
}

void RecurrenceEdit::showEvent(QShowEvent*)
{
    if (mRule)
        mRule->setFrequencyFocus();
    else
        mRuleButtonGroup->checkedButton()->setFocus();
    emit shown();
}

/******************************************************************************
* Return the sub-repetition interval and count within the recurrence, i.e. the
* number of repetitions after the main recurrence.
*/
Repetition RecurrenceEdit::subRepetition() const
{
    return (mRuleButtonType >= SUBDAILY) ? mSubRepetition->repetition() : Repetition();
}

/******************************************************************************
* Called when the Sub-Repetition button has been pressed to display the
* sub-repetition dialog.
* Alarm repetition has the following restrictions:
* 1) Not allowed for a repeat-at-login alarm
* 2) For a date-only alarm, the repeat interval must be a whole number of days.
* 3) The overall repeat duration must be less than the recurrence interval.
*/
void RecurrenceEdit::setSubRepetition(int reminderMinutes, bool dateOnly)
{
    int maxDuration;
    switch (mRuleButtonType)
    {
        case RecurrenceEdit::NO_RECUR:
        case RecurrenceEdit::AT_LOGIN:   // alarm repeat not allowed
            maxDuration = 0;
            break;
        default:          // repeat duration must be less than recurrence interval
        {
            KAEvent event;
            updateEvent(event, false);
            maxDuration = event.longestRecurrenceInterval().asSeconds()/60 - reminderMinutes - 1;
            break;
        }
    }
    mSubRepetition->initialise(mSubRepetition->repetition(), dateOnly, maxDuration);
    mSubRepetition->setEnabled(mRuleButtonType >= SUBDAILY && maxDuration);
}

/******************************************************************************
* Activate the sub-repetition dialog.
*/
void RecurrenceEdit::activateSubRepetition()
{
    mSubRepetition->activate();
}

/******************************************************************************
 * Called when the value of the repeat count field changes, to reset the
 * minimum value to 1 if the value was 0.
 */
void RecurrenceEdit::repeatCountChanged(int value)
{
    if (value > 0  &&  mRepeatCountEntry->minimum() == 0)
        mRepeatCountEntry->setMinimum(1);
}

/******************************************************************************
 * Add the date entered in the exception date edit control to the list of
 * exception dates.
 */
void RecurrenceEdit::addException()
{
    if (!mExceptionDateEdit  ||  !mExceptionDateEdit->date().isValid())
        return;
    QDate date = mExceptionDateEdit->date();
    DateList::Iterator it;
    int index = 0;
    bool insert = true;
    for (it = mExceptionDates.begin();  it != mExceptionDates.end();  ++index, ++it)
    {
        if (date <= *it)
        {
            insert = (date != *it);
            break;
        }
    }
    if (insert)
    {
        mExceptionDates.insert(it, date);
        mExceptionDateList->insertItem(index, new QListWidgetItem(KLocale::global()->formatDate(date)));
        emit contentsChanged();
    }
    mExceptionDateList->setCurrentItem(mExceptionDateList->item(index));
    enableExceptionButtons();
}

/******************************************************************************
 * Change the currently highlighted exception date to that entered in the
 * exception date edit control.
 */
void RecurrenceEdit::changeException()
{
    if (!mExceptionDateEdit  ||  !mExceptionDateEdit->date().isValid())
        return;
    QListWidgetItem* item = mExceptionDateList->currentItem();
    if (item  &&  mExceptionDateList->isItemSelected(item))
    {
        int index = mExceptionDateList->row(item);
        QDate olddate = mExceptionDates[index];
        QDate newdate = mExceptionDateEdit->date();
        if (newdate != olddate)
        {
            mExceptionDates.removeAt(index);
            mExceptionDateList->takeItem(index);
            emit contentsChanged();
            addException();
        }
    }
}

/******************************************************************************
 * Delete the currently highlighted exception date.
 */
void RecurrenceEdit::deleteException()
{
    QListWidgetItem* item = mExceptionDateList->currentItem();
    if (item  &&  mExceptionDateList->isItemSelected(item))
    {
        int index = mExceptionDateList->row(item);
        mExceptionDates.removeAt(index);
        mExceptionDateList->takeItem(index);
        emit contentsChanged();
        enableExceptionButtons();
    }
}

/******************************************************************************
 * Enable/disable the exception group buttons according to whether any item is
 * selected in the exceptions listbox.
 */
void RecurrenceEdit::enableExceptionButtons()
{
    QListWidgetItem* item = mExceptionDateList->currentItem();
    bool enable = item;
    if (mDeleteExceptionButton)
        mDeleteExceptionButton->setEnabled(enable);
    if (mChangeExceptionButton)
        mChangeExceptionButton->setEnabled(enable);

    // Prevent the exceptions list box receiving keyboard focus is it's empty
    mExceptionDateList->setFocusPolicy(mExceptionDateList->count() ? Qt::WheelFocus : Qt::NoFocus);
}

/******************************************************************************
 * Notify this instance of a change in the alarm start date.
 */
void RecurrenceEdit::setStartDate(const QDate& start, const QDate& today)
{
    if (!mReadOnly)
    {
        setRuleDefaults(start);
        if (start < today)
        {
            mEndDateEdit->setMinimumDate(today);
            if (mExceptionDateEdit)
                mExceptionDateEdit->setMinimumDate(today);
        }
        else
        {
            const QString startString = i18nc("@info", "Date cannot be earlier than start date");
            mEndDateEdit->setMinimumDate(start, startString);
            if (mExceptionDateEdit)
                mExceptionDateEdit->setMinimumDate(start, startString);
        }
    }
}

/******************************************************************************
 * Specify the default recurrence end date.
 */
void RecurrenceEdit::setDefaultEndDate(const QDate& end)
{
    if (!mEndDateButton->isChecked())
        mEndDateEdit->setDate(end);
}

void RecurrenceEdit::setEndDateTime(const KDateTime& end)
{
    KDateTime edt = end.toTimeSpec(mCurrStartDateTime.timeSpec());
    mEndDateEdit->setDate(edt.date());
    mEndTimeEdit->setValue(edt.time());
    mEndTimeEdit->setEnabled(!end.isDateOnly());
    mEndAnyTimeCheckBox->setChecked(end.isDateOnly());
}

KDateTime RecurrenceEdit::endDateTime() const
{
    if (mRuleButtonGroup->checkedButton() == mAtLoginButton  &&  mEndAnyTimeCheckBox->isChecked())
        return KDateTime(mEndDateEdit->date(), mCurrStartDateTime.timeSpec());
    return KDateTime(mEndDateEdit->date(), mEndTimeEdit->time(), mCurrStartDateTime.timeSpec());
}

/******************************************************************************
 * Set all controls to their default values.
 */
void RecurrenceEdit::setDefaults(const KDateTime& from)
{
    mCurrStartDateTime = from;
    QDate fromDate = from.date();
    mNoEndDateButton->setChecked(true);

    mSubDailyRule->setFrequency(1);
    mDailyRule->setFrequency(1);
    mWeeklyRule->setFrequency(1);
    mMonthlyRule->setFrequency(1);
    mYearlyRule->setFrequency(1);

    setRuleDefaults(fromDate);
    mMonthlyRule->setType(MonthYearRule::DATE);   // date in month
    mYearlyRule->setType(MonthYearRule::DATE);    // date in year

    mEndDateEdit->setDate(fromDate);

    mNoEmitTypeChanged = true;
    RadioButton* button;
    switch (Preferences::defaultRecurPeriod())
    {
        case AT_LOGIN: button = mAtLoginButton;  break;
        case ANNUAL:   button = mYearlyButton;   break;
        case MONTHLY:  button = mMonthlyButton;  break;
        case WEEKLY:   button = mWeeklyButton;   break;
        case DAILY:    button = mDailyButton;    break;
        case SUBDAILY: button = mSubDailyButton; break;
        case NO_RECUR:
        default:       button = mNoneButton;     break;
    }
    button->setChecked(true);
    mNoEmitTypeChanged = false;
    rangeTypeClicked();
    enableExceptionButtons();

    saveState();
}

/******************************************************************************
 * Set the controls for weekly, monthly and yearly rules which have not so far
 * been shown, to their default values, depending on the recurrence start date.
 */
void RecurrenceEdit::setRuleDefaults(const QDate& fromDate)
{
    int day       = fromDate.day();
    int dayOfWeek = fromDate.dayOfWeek();
    int month     = fromDate.month();
    if (!mDailyShown)
        mDailyRule->setDays(true);
    if (!mWeeklyShown)
        mWeeklyRule->setDay(dayOfWeek);
    if (!mMonthlyShown)
        mMonthlyRule->setDefaultValues(day, dayOfWeek);
    if (!mYearlyShown)
        mYearlyRule->setDefaultValues(day, dayOfWeek, month);
}

/******************************************************************************
* Initialise the recurrence to select repeat-at-login.
* This function and set() are mutually exclusive: call one or the other, not both.
*/
void RecurrenceEdit::setRepeatAtLogin()
{
    mAtLoginButton->setChecked(true);
    mEndDateButton->setChecked(true);
}

/******************************************************************************
* Set the state of all controls to reflect the data in the specified event.
*/
void RecurrenceEdit::set(const KAEvent& event)
{
    setDefaults(event.mainDateTime().kDateTime());
    if (event.repeatAtLogin())
    {
        mAtLoginButton->setChecked(true);
        mEndDateButton->setChecked(true);
        return;
    }
    mNoneButton->setChecked(true);
    KARecurrence* recurrence = event.recurrence();
    if (!recurrence)
        return;
    KARecurrence::Type rtype = recurrence->type();
    switch (rtype)
    {
        case KARecurrence::MINUTELY:
            mSubDailyButton->setChecked(true);
            break;

        case KARecurrence::DAILY:
        {
            mDailyButton->setChecked(true);
            QBitArray rDays = recurrence->days();
            bool set = false;
            for (int i = 0;  i < 7 && !set;  ++i)
                set = rDays.testBit(i);
            if (set)
                mDailyRule->setDays(rDays);
            else
                mDailyRule->setDays(true);
            break;
        }
        case KARecurrence::WEEKLY:
        {
            mWeeklyButton->setChecked(true);
            QBitArray rDays = recurrence->days();
            mWeeklyRule->setDays(rDays);
            break;
        }
        case KARecurrence::MONTHLY_POS:    // on nth (Tuesday) of the month
        {
            QList<RecurrenceRule::WDayPos> posns = recurrence->monthPositions();
            int i = posns.first().pos();
            if (!i)
            {
                // It's every (Tuesday) of the month. Convert to a weekly recurrence
                // (but ignoring any non-every xxxDay positions).
                mWeeklyButton->setChecked(true);
                mWeeklyRule->setFrequency(recurrence->frequency());
                QBitArray rDays(7);
                for (int i = 0, end = posns.count();  i < end;  ++i)
                {
                    if (!posns[i].pos())
                        rDays.setBit(posns[i].day() - 1, 1);
                }
                mWeeklyRule->setDays(rDays);
                break;
            }
            mMonthlyButton->setChecked(true);
            mMonthlyRule->setPosition(i, posns.first().day());
            break;
        }
        case KARecurrence::MONTHLY_DAY:     // on nth day of the month
        {
            mMonthlyButton->setChecked(true);
            QList<int> rmd = recurrence->monthDays();
            int day = (rmd.isEmpty()) ? event.mainDateTime().date().day() : rmd.first();
            mMonthlyRule->setDate(day);
            break;
        }
        case KARecurrence::ANNUAL_DATE:   // on the nth day of (months...) in the year
        case KARecurrence::ANNUAL_POS:     // on the nth (Tuesday) of (months...) in the year
        {
            if (rtype == KARecurrence::ANNUAL_DATE)
            {
                mYearlyButton->setChecked(true);
                const QList<int> rmd = recurrence->monthDays();
                int day = (rmd.isEmpty()) ? event.mainDateTime().date().day() : rmd.first();
                mYearlyRule->setDate(day);
                mYearlyRule->setFeb29Type(recurrence->feb29Type());
            }
            else if (rtype == KARecurrence::ANNUAL_POS)
            {
                mYearlyButton->setChecked(true);
                QList<RecurrenceRule::WDayPos> posns = recurrence->yearPositions();
                mYearlyRule->setPosition(posns.first().pos(), posns.first().day());
            }
            mYearlyRule->setMonths(recurrence->yearMonths());
            break;
        }
        default:
            return;
    }

    mRule->setFrequency(recurrence->frequency());

    // Get range information
    KDateTime endtime = mCurrStartDateTime;
    int duration = recurrence->duration();
    if (duration == -1)
        mNoEndDateButton->setChecked(true);
    else if (duration)
    {
        mRepeatCountButton->setChecked(true);
        mRepeatCountEntry->setValue(duration);
    }
    else
    {
        mEndDateButton->setChecked(true);
        endtime = recurrence->endDateTime();
        mEndTimeEdit->setValue(endtime.time());
    }
    mEndDateEdit->setDate(endtime.date());

    // Get exception information
    mExceptionDates = event.recurrence()->exDates();
    qSort(mExceptionDates);
    mExceptionDateList->clear();
    for (int i = 0, iend = mExceptionDates.count();  i < iend;  ++i)
        new QListWidgetItem(KLocale::global()->formatDate(mExceptionDates[i]), mExceptionDateList);
    enableExceptionButtons();
    mExcludeHolidays->setChecked(event.holidaysExcluded());
    mWorkTimeOnly->setChecked(event.workTimeOnly());

    // Get repetition within recurrence
    mSubRepetition->set(event.repetition());

    rangeTypeClicked();

    saveState();
}

/******************************************************************************
 * Update the specified KAEvent with the entered recurrence data.
 * If 'adjustStart' is true, the start date/time will be adjusted if necessary
 * to be the first date/time which recurs on or after the original start.
 */
void RecurrenceEdit::updateEvent(KAEvent& event, bool adjustStart)
{
    // Get end date and repeat count, common to all types of recurring events
    QDate  endDate;
    QTime  endTime;
    int    repeatCount;
    if (mNoEndDateButton->isChecked())
        repeatCount = -1;
    else if (mRepeatCountButton->isChecked())
        repeatCount = mRepeatCountEntry->value();
    else
    {
        repeatCount = 0;
        endDate = mEndDateEdit->date();
        endTime = mEndTimeEdit->time();
    }

    // Set up the recurrence according to the type selected
    event.startChanges();
    QAbstractButton* button = mRuleButtonGroup->checkedButton();
    event.setRepeatAtLogin(button == mAtLoginButton);
    int frequency = mRule ? mRule->frequency() : 0;
    if (button == mSubDailyButton)
    {
        KDateTime endDateTime(endDate, endTime, mCurrStartDateTime.timeSpec());
        event.setRecurMinutely(frequency, repeatCount, endDateTime);
    }
    else if (button == mDailyButton)
    {
        event.setRecurDaily(frequency, mDailyRule->days(), repeatCount, endDate);
    }
    else if (button == mWeeklyButton)
    {
        event.setRecurWeekly(frequency, mWeeklyRule->days(), repeatCount, endDate);
    }
    else if (button == mMonthlyButton)
    {
        if (mMonthlyRule->type() == MonthlyRule::POS)
        {
            // It's by position
            KAEvent::MonthPos pos;
            pos.days.fill(false);
            pos.days.setBit(mMonthlyRule->dayOfWeek() - 1);
            pos.weeknum = mMonthlyRule->week();
            QVector<KAEvent::MonthPos> poses(1, pos);
            event.setRecurMonthlyByPos(frequency, poses, repeatCount, endDate);
        }
        else
        {
            // It's by day
            int daynum = mMonthlyRule->date();
            QVector<int> daynums(1, daynum);
            event.setRecurMonthlyByDate(frequency, daynums, repeatCount, endDate);
        }
    }
    else if (button == mYearlyButton)
    {
        QVector<int> months = mYearlyRule->months();
        if (mYearlyRule->type() == YearlyRule::POS)
        {
            // It's by position
            KAEvent::MonthPos pos;
            pos.days.fill(false);
            pos.days.setBit(mYearlyRule->dayOfWeek() - 1);
            pos.weeknum = mYearlyRule->week();
            QVector<KAEvent::MonthPos> poses(1, pos);
            event.setRecurAnnualByPos(frequency, poses, months, repeatCount, endDate);
        }
        else
        {
            // It's by date in month
            event.setRecurAnnualByDate(frequency, months, mYearlyRule->date(),
                                       mYearlyRule->feb29Type(), repeatCount, endDate);
        }
    }
    else
    {
        event.setNoRecur();
        event.endChanges();
        return;
    }
    if (!event.recurs())
    {
        event.endChanges();
        return;    // an error occurred setting up the recurrence
    }
    if (adjustStart)
        event.setFirstRecurrence();

    // Set up repetition within the recurrence
    // N.B. This requires the main recurrence to be set up first.
    event.setRepetition((mRuleButtonType < SUBDAILY) ? Repetition() : mSubRepetition->repetition());

    // Set up exceptions
    event.recurrence()->setExDates(mExceptionDates);
    event.setWorkTimeOnly(mWorkTimeOnly->isChecked());
    event.setExcludeHolidays(mExcludeHolidays->isChecked());

    event.endChanges();
}

/******************************************************************************
 * Save the state of all controls.
 */
void RecurrenceEdit::saveState()
{
    mSavedRuleButton = mRuleButtonGroup->checkedButton();
    if (mRule)
        mRule->saveState();
    mSavedRangeButton = mRangeButtonGroup->checkedButton();
    if (mSavedRangeButton == mRepeatCountButton)
        mSavedRecurCount = mRepeatCountEntry->value();
    else if (mSavedRangeButton == mEndDateButton)
    {
        mSavedEndDateTime = KDateTime(QDateTime(mEndDateEdit->date(), mEndTimeEdit->time()), mCurrStartDateTime.timeSpec());
        mSavedEndDateTime.setDateOnly(mEndAnyTimeCheckBox->isChecked());
    }
    mSavedExceptionDates = mExceptionDates;
    mSavedWorkTimeOnly   = mWorkTimeOnly->isChecked();
    mSavedExclHolidays   = mExcludeHolidays->isChecked();
    mSavedRepetition     = mSubRepetition->repetition();
}

/******************************************************************************
 * Check whether any of the controls have changed state since initialisation.
 */
bool RecurrenceEdit::stateChanged() const
{
    if (mSavedRuleButton  != mRuleButtonGroup->checkedButton()
    ||  mSavedRangeButton != mRangeButtonGroup->checkedButton()
    ||  (mRule  &&  mRule->stateChanged()))
        return true;
    if (mSavedRangeButton == mRepeatCountButton
    &&  mSavedRecurCount  != mRepeatCountEntry->value())
        return true;
    if (mSavedRangeButton == mEndDateButton)
    {
        KDateTime edt(QDateTime(mEndDateEdit->date(), mEndTimeEdit->time()), mCurrStartDateTime.timeSpec());
        edt.setDateOnly(mEndAnyTimeCheckBox->isChecked());
        if (mSavedEndDateTime != edt)
            return true;
    }
    if (mSavedExceptionDates != mExceptionDates
    ||  mSavedWorkTimeOnly   != mWorkTimeOnly->isChecked()
    ||  mSavedExclHolidays   != mExcludeHolidays->isChecked()
    ||  mSavedRepetition     != mSubRepetition->repetition())
        return true;
    return false;
}



/*=============================================================================
= Class Rule
= Base class for rule widgets, including recurrence frequency.
=============================================================================*/

Rule::Rule(const QString& freqText, const QString& freqWhatsThis, bool time, bool readOnly, QWidget* parent)
    : NoRule(parent)
{
    mLayout = new QVBoxLayout(this);
    mLayout->setMargin(0);
    mLayout->setSpacing(KDialog::spacingHint());
    KHBox* freqBox = new KHBox(this);
    freqBox->setMargin(0);
    mLayout->addWidget(freqBox, 0, Qt::AlignLeft);
    KHBox* box = new KHBox(freqBox);    // this is to control the QWhatsThis text display area
    box->setMargin(0);
    box->setSpacing(KDialog::spacingHint());

    QLabel* label = new QLabel(i18nc("@label:spinbox", "Recur e&very"), box);
    label->setFixedSize(label->sizeHint());
    if (time)
    {
        mIntSpinBox = 0;
        mSpinBox = mTimeSpinBox = new TimeSpinBox(1, 5999, box);
        mTimeSpinBox->setFixedSize(mTimeSpinBox->sizeHint());
        mTimeSpinBox->setReadOnly(readOnly);
    }
    else
    {
        mTimeSpinBox = 0;
        mSpinBox = mIntSpinBox = new SpinBox(1, 999, box);
        mIntSpinBox->setFixedSize(mIntSpinBox->sizeHint());
        mIntSpinBox->setReadOnly(readOnly);
    }
    connect(mSpinBox, SIGNAL(valueChanged(int)), SIGNAL(frequencyChanged()));
    connect(mSpinBox, SIGNAL(valueChanged(int)), SIGNAL(changed()));
    label->setBuddy(mSpinBox);
    label = new QLabel(freqText, box);
    label->setFixedSize(label->sizeHint());
    box->setFixedSize(sizeHint());
    box->setWhatsThis(freqWhatsThis);

    new QWidget(freqBox);     // left adjust the visible widgets
    freqBox->setFixedHeight(freqBox->sizeHint().height());
    freqBox->setFocusProxy(mSpinBox);
}

int Rule::frequency() const
{
    if (mIntSpinBox)
        return mIntSpinBox->value();
    if (mTimeSpinBox)
        return mTimeSpinBox->value();
    return 0;
}

void Rule::setFrequency(int n)
{
    if (mIntSpinBox)
        mIntSpinBox->setValue(n);
    if (mTimeSpinBox)
        mTimeSpinBox->setValue(n);
}

/******************************************************************************
 * Save the state of all controls.
 */
void Rule::saveState()
{
    mSavedFrequency = frequency();
}

/******************************************************************************
 * Check whether any of the controls have changed state since initialisation.
 */
bool Rule::stateChanged() const
{
    return (mSavedFrequency != frequency());
}


/*=============================================================================
= Class SubDailyRule
= Sub-daily rule widget.
=============================================================================*/

SubDailyRule::SubDailyRule(bool readOnly, QWidget* parent)
    : Rule(i18nc("@label Time units for user-entered numbers", "hours:minutes"),
           i18nc("@info:whatsthis", "Enter the number of hours and minutes between repetitions of the alarm"),
           true, readOnly, parent)
{ }


/*=============================================================================
= Class DayWeekRule
= Daily/weekly rule widget base class.
=============================================================================*/

DayWeekRule::DayWeekRule(const QString& freqText, const QString& freqWhatsThis, const QString& daysWhatsThis,
                         bool readOnly, QWidget* parent)
    : Rule(freqText, freqWhatsThis, false, readOnly, parent),
      mSavedDays(7)
{
    QGridLayout* grid = new QGridLayout();
    grid->setMargin(0);
    grid->setRowStretch(0, 1);
    layout()->addLayout(grid);

    QLabel* label = new QLabel(i18nc("@label On: Tuesday", "O&n:"), this);
    label->setFixedSize(label->sizeHint());
    grid->addWidget(label, 0, 0, Qt::AlignRight | Qt::AlignTop);
    grid->setColumnMinimumWidth(1, KDialog::spacingHint());

    // List the days of the week starting at the user's start day of the week.
    // Save the first day of the week, just in case it changes while the dialog is open.
    QWidget* box = new QWidget(this);   // this is to control the QWhatsThis text display area
    QGridLayout* dgrid = new QGridLayout(box);
    dgrid->setMargin(0);
    dgrid->setSpacing(KDialog::spacingHint());
    const KCalendarSystem* calendar = KLocale::global()->calendar();
    for (int i = 0;  i < 7;  ++i)
    {
        int day = KAlarm::localeDayInWeek_to_weekDay(i);
        mDayBox[i] = new CheckBox(calendar->weekDayName(day), box);
        mDayBox[i]->setFixedSize(mDayBox[i]->sizeHint());
        mDayBox[i]->setReadOnly(readOnly);
        connect(mDayBox[i], SIGNAL(toggled(bool)), SIGNAL(changed()));
        dgrid->addWidget(mDayBox[i], i%4, i/4, Qt::AlignLeft);
    }
    box->setFixedSize(box->sizeHint());
    box->setWhatsThis(daysWhatsThis);
    grid->addWidget(box, 0, 2, Qt::AlignLeft);
    label->setBuddy(mDayBox[0]);
    grid->setColumnStretch(3, 1);
}

/******************************************************************************
 * Fetch which days of the week have been ticked.
 */
QBitArray DayWeekRule::days() const
{
    QBitArray ds(7);
    ds.fill(false);
    for (int i = 0;  i < 7;  ++i)
        if (mDayBox[i]->isChecked())
            ds.setBit(KAlarm::localeDayInWeek_to_weekDay(i) - 1, 1);
    return ds;
}

/******************************************************************************
 * Tick/untick every day of the week.
 */
void DayWeekRule::setDays(bool tick)
{
    for (int i = 0;  i < 7;  ++i)
        mDayBox[i]->setChecked(tick);
}

/******************************************************************************
 * Tick/untick each day of the week according to the specified bits.
 */
void DayWeekRule::setDays(const QBitArray& days)
{
    for (int i = 0;  i < 7;  ++i)
    {
        bool x = days.testBit(KAlarm::localeDayInWeek_to_weekDay(i) - 1);
        mDayBox[i]->setChecked(x);
    }
}

/******************************************************************************
 * Tick the specified day of the week, and untick all other days.
 */
void DayWeekRule::setDay(int dayOfWeek)
{
    for (int i = 0;  i < 7;  ++i)
        mDayBox[i]->setChecked(false);
    if (dayOfWeek > 0  &&  dayOfWeek <= 7)
        mDayBox[KAlarm::weekDay_to_localeDayInWeek(dayOfWeek)]->setChecked(true);
}

/******************************************************************************
 * Validate: check that at least one day is selected.
 */
QWidget* DayWeekRule::validate(QString& errorMessage)
{
    for (int i = 0;  i < 7;  ++i)
        if (mDayBox[i]->isChecked())
            return 0;
    errorMessage = i18nc("@info", "No day selected");
    return mDayBox[0];
}

/******************************************************************************
 * Save the state of all controls.
 */
void DayWeekRule::saveState()
{
    Rule::saveState();
    mSavedDays = days();
}

/******************************************************************************
 * Check whether any of the controls have changed state since initialisation.
 */
bool DayWeekRule::stateChanged() const
{
    return (Rule::stateChanged()
        ||  mSavedDays != days());
}


/*=============================================================================
= Class DailyRule
= Daily rule widget.
=============================================================================*/

DailyRule::DailyRule(bool readOnly, QWidget* parent)
    : DayWeekRule(i18nc("@label Time unit for user-entered number", "day(s)"),
                  i18nc("@info:whatsthis", "Enter the number of days between repetitions of the alarm"),
                  i18nc("@info:whatsthis", "Select the days of the week on which the alarm is allowed to occur"),
                  readOnly, parent)
{ }


/*=============================================================================
= Class WeeklyRule
= Weekly rule widget.
=============================================================================*/

WeeklyRule::WeeklyRule(bool readOnly, QWidget* parent)
    : DayWeekRule(i18nc("@label Time unit for user-entered number", "week(s)"),
                  i18nc("@info:whatsthis", "Enter the number of weeks between repetitions of the alarm"),
                  i18nc("@info:whatsthis", "Select the days of the week on which to repeat the alarm"),
                  readOnly, parent)
{ }


/*=============================================================================
= Class MonthYearRule
= Monthly/yearly rule widget base class.
=============================================================================*/

MonthYearRule::MonthYearRule(const QString& freqText, const QString& freqWhatsThis, bool allowEveryWeek,
                             bool readOnly, QWidget* parent)
    : Rule(freqText, freqWhatsThis, false, readOnly, parent),
      mEveryWeek(allowEveryWeek)
{
    mButtonGroup = new ButtonGroup(this);

    // Month day selector
    KHBox* box = new KHBox(this);
    box->setMargin(0);
    box->setSpacing(KDialog::spacingHint());
    layout()->addWidget(box);

    mDayButton = new RadioButton(i18nc("@option:radio On day number in the month", "O&n day"), box);
    mDayButton->setFixedSize(mDayButton->sizeHint());
    mDayButton->setReadOnly(readOnly);
    mButtonGroup->addButton(mDayButton);
    mDayButton->setWhatsThis(i18nc("@info:whatsthis", "Repeat the alarm on the selected day of the month"));

    mDayCombo = new ComboBox(box);
    mDayCombo->setEditable(false);
    mDayCombo->setMaxVisibleItems(11);
    for (int i = 0;  i < 31;  ++i)
        mDayCombo->addItem(QString::number(i + 1));
    mDayCombo->addItem(i18nc("@item:inlistbox Last day of month", "Last"));
    mDayCombo->setFixedSize(mDayCombo->sizeHint());
    mDayCombo->setReadOnly(readOnly);
    mDayCombo->setWhatsThis(i18nc("@info:whatsthis", "Select the day of the month on which to repeat the alarm"));
    mDayButton->setFocusWidget(mDayCombo);
    connect(mDayCombo, static_cast<void (ComboBox::*)(int)>(&ComboBox::activated), this, &MonthYearRule::slotDaySelected);
    connect(mDayCombo, static_cast<void (ComboBox::*)(int)>(&ComboBox::currentIndexChanged), this, &MonthYearRule::changed);

    box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
    box->setFixedHeight(box->sizeHint().height());

    // Month position selector
    box = new KHBox(this);
    box->setMargin(0);
    box->setSpacing(KDialog::spacingHint());
    layout()->addWidget(box);

    mPosButton = new RadioButton(i18nc("@option:radio On the 1st Tuesday", "On t&he"), box);
    mPosButton->setFixedSize(mPosButton->sizeHint());
    mPosButton->setReadOnly(readOnly);
    mButtonGroup->addButton(mPosButton);
    mPosButton->setWhatsThis(i18nc("@info:whatsthis", "Repeat the alarm on one day of the week, in the selected week of the month"));

    mWeekCombo = new ComboBox(box);
    mWeekCombo->setEditable(false);
    mWeekCombo->addItem(i18nc("@item:inlistbox", "1st"));
    mWeekCombo->addItem(i18nc("@item:inlistbox", "2nd"));
    mWeekCombo->addItem(i18nc("@item:inlistbox", "3rd"));
    mWeekCombo->addItem(i18nc("@item:inlistbox", "4th"));
    mWeekCombo->addItem(i18nc("@item:inlistbox", "5th"));
    mWeekCombo->addItem(i18nc("@item:inlistbox Last Monday in March", "Last"));
    mWeekCombo->addItem(i18nc("@item:inlistbox", "2nd Last"));
    mWeekCombo->addItem(i18nc("@item:inlistbox", "3rd Last"));
    mWeekCombo->addItem(i18nc("@item:inlistbox", "4th Last"));
    mWeekCombo->addItem(i18nc("@item:inlistbox", "5th Last"));
    if (mEveryWeek)
    {
        mWeekCombo->addItem(i18nc("@item:inlistbox Every (Monday...) in month", "Every"));
        mWeekCombo->setMaxVisibleItems(11);
    }
    mWeekCombo->setWhatsThis(i18nc("@info:whatsthis", "Select the week of the month in which to repeat the alarm"));
    mWeekCombo->setFixedSize(mWeekCombo->sizeHint());
    mWeekCombo->setReadOnly(readOnly);
    mPosButton->setFocusWidget(mWeekCombo);
    connect(mWeekCombo, static_cast<void (ComboBox::*)(int)>(&ComboBox::currentIndexChanged), this, &MonthYearRule::changed);

    mDayOfWeekCombo = new ComboBox(box);
    mDayOfWeekCombo->setEditable(false);
    const KCalendarSystem* calendar = KLocale::global()->calendar();
    for (int i = 0;  i < 7;  ++i)
    {
        int day = KAlarm::localeDayInWeek_to_weekDay(i);
        mDayOfWeekCombo->addItem(calendar->weekDayName(day));
    }
    mDayOfWeekCombo->setReadOnly(readOnly);
    mDayOfWeekCombo->setWhatsThis(i18nc("@info:whatsthis", "Select the day of the week on which to repeat the alarm"));
    connect(mDayOfWeekCombo, static_cast<void (ComboBox::*)(int)>(&ComboBox::currentIndexChanged), this, &MonthYearRule::changed);

    box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
    box->setFixedHeight(box->sizeHint().height());
    connect(mButtonGroup, &ButtonGroup::buttonSet, this, &MonthYearRule::clicked);
    connect(mButtonGroup, &ButtonGroup::buttonSet, this, &MonthYearRule::changed);
}

MonthYearRule::DayPosType MonthYearRule::type() const
{
    return (mButtonGroup->checkedButton() == mDayButton) ? DATE : POS;
}

void MonthYearRule::setType(MonthYearRule::DayPosType type)
{
    if (type == DATE)
        mDayButton->setChecked(true);
    else
        mPosButton->setChecked(true);
}

void MonthYearRule::setDefaultValues(int dayOfMonth, int dayOfWeek)
{
    --dayOfMonth;
    mDayCombo->setCurrentIndex(dayOfMonth);
    mWeekCombo->setCurrentIndex(dayOfMonth / 7);
    mDayOfWeekCombo->setCurrentIndex(KAlarm::weekDay_to_localeDayInWeek(dayOfWeek));
}

int MonthYearRule::date() const
{
    int daynum  = mDayCombo->currentIndex() + 1;
    return (daynum <= 31) ? daynum : 31 - daynum;
}

int MonthYearRule::week() const
{
    int weeknum = mWeekCombo->currentIndex() + 1;
    return (weeknum <= 5) ? weeknum : (weeknum == 11) ? 0 : 5 - weeknum;
}

int MonthYearRule::dayOfWeek() const
{
    return KAlarm::localeDayInWeek_to_weekDay(mDayOfWeekCombo->currentIndex());
}

void MonthYearRule::setDate(int dayOfMonth)
{
    mDayButton->setChecked(true);;
    mDayCombo->setCurrentIndex(dayOfMonth > 0 ? dayOfMonth - 1 : dayOfMonth < 0 ? 30 - dayOfMonth : 0);   // day 0 shouldn't ever occur
}

void MonthYearRule::setPosition(int week, int dayOfWeek)
{
    mPosButton->setChecked(true);
    mWeekCombo->setCurrentIndex((week > 0) ? week - 1 : (week < 0) ? 4 - week : mEveryWeek ? 10 : 0);
    mDayOfWeekCombo->setCurrentIndex(KAlarm::weekDay_to_localeDayInWeek(dayOfWeek));
}

void MonthYearRule::enableSelection(DayPosType type)
{
    bool date = (type == DATE);
    mDayCombo->setEnabled(date);
    mWeekCombo->setEnabled(!date);
    mDayOfWeekCombo->setEnabled(!date);
}

void MonthYearRule::clicked(QAbstractButton* button)
{
    enableSelection(button == mDayButton ? DATE : POS);
}

void MonthYearRule::slotDaySelected(int index)
{
    daySelected(index <= 30 ? index + 1 : 30 - index);
}

/******************************************************************************
 * Save the state of all controls.
 */
void MonthYearRule::saveState()
{
    Rule::saveState();
    mSavedType = type();
    if (mSavedType == DATE)
        mSavedDay = date();
    else
    {
        mSavedWeek    = week();
        mSavedWeekDay = dayOfWeek();
    }
}

/******************************************************************************
 * Check whether any of the controls have changed state since initialisation.
 */
bool MonthYearRule::stateChanged() const
{
    if (Rule::stateChanged()
    ||  mSavedType != type())
        return true;
    if (mSavedType == DATE)
    {
        if (mSavedDay != date())
            return true;
    }
    else
    {
        if (mSavedWeek    != week()
        ||  mSavedWeekDay != dayOfWeek())
            return true;
    }
    return false;
}


/*=============================================================================
= Class MonthlyRule
= Monthly rule widget.
=============================================================================*/

MonthlyRule::MonthlyRule(bool readOnly, QWidget* parent)
    : MonthYearRule(i18nc("@label Time unit for user-entered number", "month(s)"),
           i18nc("@info:whatsthis", "Enter the number of months between repetitions of the alarm"),
           false, readOnly, parent)
{ }


/*=============================================================================
= Class YearlyRule
= Yearly rule widget.
=============================================================================*/

YearlyRule::YearlyRule(bool readOnly, QWidget* parent)
    : MonthYearRule(i18nc("@label Time unit for user-entered number", "year(s)"),
           i18nc("@info:whatsthis", "Enter the number of years between repetitions of the alarm"),
           true, readOnly, parent)
{
    // Set up the month selection widgets
    QHBoxLayout* hlayout = new QHBoxLayout();
    hlayout->setMargin(0);
    layout()->addLayout(hlayout);
    QLabel* label = new QLabel(i18nc("@label List of months to select", "Months:"), this);
    label->setFixedSize(label->sizeHint());
    hlayout->addWidget(label, 0, Qt::AlignLeft | Qt::AlignTop);

    // List the months of the year.
    QWidget* w = new QWidget(this);   // this is to control the QWhatsThis text display area
    hlayout->addWidget(w, 1, Qt::AlignLeft);
    QGridLayout* grid = new QGridLayout(w);
    grid->setMargin(0);
    grid->setSpacing(KDialog::spacingHint());
    const KCalendarSystem* calendar = KLocale::global()->calendar();
    int year = KDateTime::currentLocalDate().year();
    for (int i = 0;  i < 12;  ++i)
    {
        mMonthBox[i] = new CheckBox(calendar->monthName(i + 1, year, KCalendarSystem::ShortName), w);
        mMonthBox[i]->setFixedSize(mMonthBox[i]->sizeHint());
        mMonthBox[i]->setReadOnly(readOnly);
        connect(mMonthBox[i], SIGNAL(toggled(bool)), SIGNAL(changed()));
        grid->addWidget(mMonthBox[i], i%3, i/3, Qt::AlignLeft);
    }
    connect(mMonthBox[1], SIGNAL(toggled(bool)), SLOT(enableFeb29()));
    w->setFixedHeight(w->sizeHint().height());
    w->setWhatsThis(i18nc("@info:whatsthis", "Select the months of the year in which to repeat the alarm"));

    // February 29th handling option
    KHBox* f29box = new KHBox(this);
    f29box->setMargin(0);
    layout()->addWidget(f29box);
    KHBox* box = new KHBox(f29box);    // this is to control the QWhatsThis text display area
    box->setMargin(0);
    box->setSpacing(KDialog::spacingHint());
    mFeb29Label = new QLabel(i18nc("@label:listbox", "February 2&9th alarm in non-leap years:"), box);
    mFeb29Label->setFixedSize(mFeb29Label->sizeHint());
    mFeb29Combo = new ComboBox(box);
    mFeb29Combo->setEditable(false);
    mFeb29Combo->addItem(i18nc("@item:inlistbox No date", "None"));
    mFeb29Combo->addItem(i18nc("@item:inlistbox 1st March (short form)", "1 Mar"));
    mFeb29Combo->addItem(i18nc("@item:inlistbox 28th February (short form)", "28 Feb"));
    mFeb29Combo->setFixedSize(mFeb29Combo->sizeHint());
    mFeb29Combo->setReadOnly(readOnly);
    connect(mFeb29Combo, static_cast<void (ComboBox::*)(int)>(&ComboBox::currentIndexChanged), this, &YearlyRule::changed);
    mFeb29Label->setBuddy(mFeb29Combo);
    box->setFixedSize(box->sizeHint());
    box->setWhatsThis(i18nc("@info:whatsthis", "Select which date, if any, the February 29th alarm should trigger in non-leap years"));
    new QWidget(f29box);     // left adjust the visible widgets
    f29box->setFixedHeight(f29box->sizeHint().height());
}

void YearlyRule::setDefaultValues(int dayOfMonth, int dayOfWeek, int month)
{
    MonthYearRule::setDefaultValues(dayOfMonth, dayOfWeek);
    --month;
    for (int i = 0;  i < 12;  ++i)
        mMonthBox[i]->setChecked(i == month);
    setFeb29Type(KARecurrence::defaultFeb29Type());
    daySelected(dayOfMonth);     // enable/disable month checkboxes as appropriate
}

/******************************************************************************
* Fetch which months have been checked (1 - 12).
* Reply = true if February has been checked.
*/
QVector<int> YearlyRule::months() const
{
    QVector<int> mnths;
    for (int i = 0;  i < 12;  ++i)
        if (mMonthBox[i]->isChecked()  &&  mMonthBox[i]->isEnabled())
            mnths.append(i + 1);
    return mnths;
}

/******************************************************************************
* Check/uncheck each month of the year according to the specified list.
*/
void YearlyRule::setMonths(const QList<int>& mnths)
{
    bool checked[12];
    for (int i = 0;  i < 12;  ++i)
        checked[i] = false;
    for (int i = 0, end = mnths.count();  i < end;  ++i)
        checked[mnths[i] - 1] = true;
    for (int i = 0;  i < 12;  ++i)
        mMonthBox[i]->setChecked(checked[i]);
    enableFeb29();
}

/******************************************************************************
* Return the date for February 29th alarms in non-leap years.
*/
KARecurrence::Feb29Type YearlyRule::feb29Type() const
{
    if (mFeb29Combo->isEnabled())
    {
        switch (mFeb29Combo->currentIndex())
        {
            case 1:   return KARecurrence::Feb29_Mar1;
            case 2:   return KARecurrence::Feb29_Feb28;
            default:  break;
        }
    }
    return KARecurrence::Feb29_None;
}

/******************************************************************************
 * Set the date for February 29th alarms to trigger in non-leap years.
 */
void YearlyRule::setFeb29Type(KARecurrence::Feb29Type type)
{
    int index;
    switch (type)
    {
        default:
        case KARecurrence::Feb29_None:  index = 0;  break;
        case KARecurrence::Feb29_Mar1:  index = 1;  break;
        case KARecurrence::Feb29_Feb28: index = 2;  break;
    }
    mFeb29Combo->setCurrentIndex(index);
}

/******************************************************************************
 * Validate: check that at least one month is selected.
 */
QWidget* YearlyRule::validate(QString& errorMessage)
{
    for (int i = 0;  i < 12;  ++i)
        if (mMonthBox[i]->isChecked()  &&  mMonthBox[i]->isEnabled())
            return 0;
    errorMessage = i18nc("@info", "No month selected");
    return mMonthBox[0];
}

/******************************************************************************
 * Called when a yearly recurrence type radio button is clicked,
 * to enable/disable month checkboxes as appropriate for the date selected.
 */
void YearlyRule::clicked(QAbstractButton* button)
{
    MonthYearRule::clicked(button);
    daySelected(buttonType(button) == DATE ? date() : 1);
}

/******************************************************************************
 * Called when a day of the month is selected in a yearly recurrence, to
 * disable months for which the day is out of range.
 */
void YearlyRule::daySelected(int day)
{
    mMonthBox[1]->setEnabled(day <= 29);  // February
    bool enable = (day != 31);
    mMonthBox[3]->setEnabled(enable);     // April
    mMonthBox[5]->setEnabled(enable);     // June
    mMonthBox[8]->setEnabled(enable);     // September
    mMonthBox[10]->setEnabled(enable);    // November
    enableFeb29();
}

/******************************************************************************
 * Enable/disable the February 29th combo box depending on whether February
 * 29th is selected.
 */
void YearlyRule::enableFeb29()
{
    bool enable = (type() == DATE  &&  date() == 29  &&  mMonthBox[1]->isChecked()  &&  mMonthBox[1]->isEnabled());
    mFeb29Label->setEnabled(enable);
    mFeb29Combo->setEnabled(enable);
}

/******************************************************************************
 * Save the state of all controls.
 */
void YearlyRule::saveState()
{
    MonthYearRule::saveState();
    mSavedMonths    = months();
    mSavedFeb29Type = feb29Type();
}

/******************************************************************************
 * Check whether any of the controls have changed state since initialisation.
 */
bool YearlyRule::stateChanged() const
{
    return (MonthYearRule::stateChanged()
        ||  mSavedMonths    != months()
        ||  mSavedFeb29Type != feb29Type());
}



// vim: et sw=4:
