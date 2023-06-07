/*
 *  repetitionbutton.cpp  -  pushbutton and dialog to specify alarm repetition
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "repetitionbutton.h"

#include "timeselector.h"
#include "lib/buttongroup.h"
#include "lib/radiobutton.h"
#include "lib/spinbox.h"
#include "lib/timeperiod.h"

#include <KLocalizedString>

#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>

using namespace KCalendarCore;


/*=============================================================================
= Class RepetitionButton
= Button to display the Simple Alarm Repetition dialog.
=============================================================================*/

RepetitionButton::RepetitionButton(const QString& caption, bool waitForInitialisation, QWidget* parent)
    : QPushButton(caption, parent)
    , mWaitForInit(waitForInitialisation)
{
    setCheckable(true);
    setChecked(false);
    connect(this, &RepetitionButton::clicked, this, &RepetitionButton::slotPressed);
}

void RepetitionButton::set(const Repetition& repetition)
{
    mRepetition = repetition;
    setChecked(mRepetition);
}

/******************************************************************************
* Set the data for the dialog.
*/
void RepetitionButton::set(const Repetition& repetition, bool dateOnly, int maxDuration)
{
    mRepetition  = repetition;
    mMaxDuration = maxDuration;
    mDateOnly    = dateOnly;
    setChecked(mRepetition);
}

/******************************************************************************
* Create the alarm repetition dialog.
* If 'waitForInitialisation' is true, the dialog won't be displayed until set()
* is called to initialise its data.
*/
void RepetitionButton::activate(bool waitForInitialisation)
{
    if (!mDialog)
        mDialog = new RepetitionDlg(i18nc("@title:window", "Alarm Sub-Repetition"), mReadOnly, this);
    mDialog->set(mRepetition, mDateOnly, mMaxDuration);
    if (waitForInitialisation)
        Q_EMIT needsInitialisation();     // request dialog initialisation
    else
        displayDialog();    // display the dialog now
}

/******************************************************************************
* Set the data for the dialog and display it.
* To be called only after needsInitialisation() has been emitted.
*/
void RepetitionButton::initialise(const Repetition& repetition, bool dateOnly, int maxDuration)
{
    mRepetition  = (maxDuration > 0  &&  repetition.intervalMinutes() > maxDuration)
                 ? Repetition() : repetition;
    mMaxDuration = maxDuration;
    mDateOnly    = dateOnly;
    if (mDialog)
    {
        mDialog->set(mRepetition, dateOnly, maxDuration);
        displayDialog();    // display the dialog now
    }
    else
        setChecked(mRepetition);
}

/******************************************************************************
* Display the alarm sub-repetition dialog.
* Alarm repetition has the following restrictions:
* 1) Not allowed for a repeat-at-login alarm
* 2) For a date-only alarm, the repeat interval must be a whole number of days.
* 3) The overall repeat duration must be less than the recurrence interval.
*/
void RepetitionButton::displayDialog()
{
    bool change = false;
    if (mReadOnly)
    {
        mDialog->setReadOnly(true);
        mDialog->exec();
    }
    else if (mDialog->exec() == QDialog::Accepted)
    {
        mRepetition = mDialog->repetition();
        change = true;
    }
    setChecked(mRepetition);
    delete mDialog;
    mDialog = nullptr;
    if (change)
        Q_EMIT changed();   // delete dialog first, or initialise() will redisplay dialog
}


/*=============================================================================
= Class RepetitionDlg
= Simple alarm repetition dialog.
=============================================================================*/

static const int MAX_COUNT = 9999;    // maximum range for count spinbox


RepetitionDlg::RepetitionDlg(const QString& caption, bool readOnly, QWidget* parent)
    : QDialog(parent),
      mMaxDuration(-1),
      mDateOnly(false),
      mReadOnly(readOnly)
{
    setWindowTitle(caption);

    auto topLayout = new QVBoxLayout(this);

    mTimeSelector = new TimeSelector(i18nc("@option:check Repeat every 10 minutes", "Repeat every"),
                      i18nc("@info:whatsthis", "Instead of the alarm triggering just once at each recurrence, "
                            "checking this option makes the alarm trigger multiple times at each recurrence."),
                      i18nc("@info:whatsthis", "Enter the time between repetitions of the alarm"),
                      true, this);
    connect(mTimeSelector, &TimeSelector::valueChanged, this, &RepetitionDlg::intervalChanged);
    connect(mTimeSelector, &TimeSelector::toggled, this, &RepetitionDlg::repetitionToggled);
    topLayout->addWidget(mTimeSelector, 0, Qt::AlignLeft);

    mButtonBox = new QGroupBox(this);
    topLayout->addWidget(mButtonBox);
    mButtonGroup = new ButtonGroup(mButtonBox);
    connect(mButtonGroup, &ButtonGroup::buttonSet, this, &RepetitionDlg::typeClicked);

    auto vlayout = new QVBoxLayout(mButtonBox);
    auto layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    vlayout->addLayout(layout);
    mCountButton = new RadioButton(i18nc("@option:radio", "Number of repetitions:"), mButtonBox);
    mCountButton->setWhatsThis(i18nc("@info:whatsthis", "Check to specify the number of times the alarm should repeat after each recurrence"));
    mButtonGroup->addButton(mCountButton);
    layout->addWidget(mCountButton);
    mCount = new SpinBox(1, MAX_COUNT, mButtonBox);
    mCount->setSingleShiftStep(10);
    mCount->setSelectOnStep(false);
    connect(mCount, &SpinBox::valueChanged, this, &RepetitionDlg::countChanged);
    mCount->setWhatsThis(i18nc("@info:whatsthis", "Enter the number of times to trigger the alarm after its initial occurrence"));
    layout->addWidget(mCount);
    mCountButton->setFocusWidget(mCount);
    layout->addStretch();

    layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    vlayout->addLayout(layout);
    mDurationButton = new RadioButton(i18nc("@option:radio", "Duration:"), mButtonBox);
    mDurationButton->setWhatsThis(i18nc("@info:whatsthis", "Check to specify how long the alarm is to be repeated"));
    mButtonGroup->addButton(mDurationButton);
    layout->addWidget(mDurationButton);
    mDuration = new TimePeriod(TimePeriod::ShowMinutes, mButtonBox);
    connect(mDuration, &TimePeriod::valueChanged, this, &RepetitionDlg::durationChanged);
    mDuration->setWhatsThis(i18nc("@info:whatsthis", "Enter the length of time to repeat the alarm"));
    layout->addWidget(mDuration);
    mDurationButton->setFocusWidget(mDuration);
    layout->addStretch();

    auto buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(QDialogButtonBox::Ok);
    buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
    topLayout->addWidget(buttonBox);

    mCountButton->setChecked(true);
    repetitionToggled(false);
    setReadOnly(mReadOnly);
}

/******************************************************************************
* Set the state of all controls to reflect the data in the specified alarm.
*/
void RepetitionDlg::set(const Repetition& repetition, bool dateOnly, int maxDuration)
{
    if (dateOnly != mDateOnly)
    {
        mDateOnly = dateOnly;
        mTimeSelector->setDateOnly(mDateOnly);
        mDuration->setDateOnly(mDateOnly);
    }
    mMaxDuration = maxDuration;
    if (mMaxDuration)
    {
        int maxhm = (mMaxDuration > 0) ? mMaxDuration : 9999;
        int maxdw = (mMaxDuration > 0) ? mMaxDuration / 1440 : 9999;
        mTimeSelector->setMaximum(maxhm, maxdw);
        mDuration->setMaximum(maxhm, maxdw);
    }
    // Set the units - needed later if the control is unchecked initially.
    TimePeriod::Units units = mDateOnly ? TimePeriod::Days : TimePeriod::HoursMinutes;
    mTimeSelector->setPeriod(repetition.interval(), mDateOnly, units);
    if (!mMaxDuration  ||  !repetition)
        mTimeSelector->setChecked(false);
    else
    {
        bool on = mTimeSelector->isChecked();
        repetitionToggled(on);    // enable/disable controls
        if (on)
            intervalChanged(repetition.interval());    // ensure mCount range is set
        mCount->setValue(repetition.count());
        mDuration->setPeriod(repetition.duration(), mDateOnly, units);
        mCountButton->setChecked(true);
    }
    mTimeSelector->setEnabled(mMaxDuration);
}

/******************************************************************************
* Set the read-only status.
*/
void RepetitionDlg::setReadOnly(bool ro)
{
    ro = ro || mReadOnly;
    mTimeSelector->setReadOnly(ro);
    mCountButton->setReadOnly(ro);
    mCount->setReadOnly(ro);
    mDurationButton->setReadOnly(ro);
    mDuration->setReadOnly(ro);
}

/******************************************************************************
* Get the entered interval and repeat count.
*/
Repetition RepetitionDlg::repetition() const
{
    int count = 0;
    Duration interval = mTimeSelector->period();
    if (!interval.isNull())
    {
        if (mCountButton->isChecked())
            count = mCount->value();
        else if (mDurationButton->isChecked())
            count = mDuration->period().asSeconds() / interval.asSeconds();
    }
    return Repetition(interval, count);
}

/******************************************************************************
* Called when the time interval widget has changed value.
* Adjust the maximum repetition count accordingly.
*/
void RepetitionDlg::intervalChanged(const Duration& interval)
{
    if (mTimeSelector->isChecked()  &&  interval.asSeconds() > 0)
    {
        mCount->setRange(1, (mMaxDuration >= 0 ? mMaxDuration / (interval.asSeconds()/60) : MAX_COUNT));
        if (mCountButton->isChecked())
            countChanged(mCount->value());
        else
            durationChanged(mDuration->period());
    }
}

/******************************************************************************
* Called when the count spinbox has changed value.
* Adjust the duration accordingly.
*/
void RepetitionDlg::countChanged(int count)
{
    Duration interval = mTimeSelector->period();
    if (!interval.isNull())
    {
        bool blocked = mDuration->signalsBlocked();
        mDuration->blockSignals(true);
        mDuration->setPeriod(interval * count, mDateOnly,
                              (mDateOnly ? TimePeriod::Days : TimePeriod::HoursMinutes));
        mDuration->blockSignals(blocked);
    }
}

/******************************************************************************
* Called when the duration widget has changed value.
* Adjust the count accordingly.
*/
void RepetitionDlg::durationChanged(const Duration& duration)
{
    Duration interval = mTimeSelector->period();
    if (!interval.isNull())
    {
        bool blocked = mCount->signalsBlocked();
        mCount->blockSignals(true);
        mCount->setValue(duration.asSeconds() / interval.asSeconds());
        mCount->blockSignals(blocked);
    }
}

/******************************************************************************
* Called when the time period widget is toggled on or off.
*/
void RepetitionDlg::repetitionToggled(bool on)
{
    if (mMaxDuration == 0)
        on = false;
    mButtonBox->setEnabled(on);
    mCount->setEnabled(on  &&  mCountButton->isChecked());
    mDuration->setEnabled(on  &&  mDurationButton->isChecked());
}

/******************************************************************************
* Called when one of the count or duration radio buttons is toggled.
*/
void RepetitionDlg::typeClicked()
{
    if (mTimeSelector->isChecked())
    {
        mCount->setEnabled(mCountButton->isChecked());
        mDuration->setEnabled(mDurationButton->isChecked());
    }
}

// vim: et sw=4:
