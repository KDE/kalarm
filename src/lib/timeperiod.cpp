/*
 *  timeperiod.h  -  time period data entry widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2003-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "timeperiod.h"

#include "combobox.h"
#include "spinbox.h"
#include "timespinbox.h"
#include "lib/stackedwidgets.h"

#include <KLocalizedString>

#include <QHBoxLayout>

using namespace KCalendarCore;

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString TimePeriod::i18n_minutes()      { return i18nc("@item:inlistbox Time units", "minutes"); }
QString TimePeriod::i18n_hours_mins()   { return i18nc("@item:inlistbox Time units", "hours/minutes"); }
QString TimePeriod::i18n_days()         { return i18nc("@item:inlistbox Time units", "days"); }
QString TimePeriod::i18n_weeks()        { return i18nc("@item:inlistbox Time units", "weeks"); }

static const int maxMinutes = 1000*60-1;   // absolute maximum value for hours:minutes = 999H59M

/*=============================================================================
= Class TimePeriod
= Contains a time unit combo box, plus a time spinbox, to select a time period.
=============================================================================*/

TimePeriod::TimePeriod(bool allowHourMinute, QWidget* parent)
    : QWidget(parent)
    , mNoHourMinute(!allowHourMinute)
{
    auto layout = new QHBoxLayout;
    setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);

    mSpinStack = new StackedWidget(this);
    mSpinBox = new SpinBox(mSpinStack);
    mSpinBox->setSingleStep(1);
    mSpinBox->setSingleShiftStep(10);
    mSpinBox->setRange(1, mMaxDays);
    connect(mSpinBox, &SpinBox::valueChanged, this, &TimePeriod::slotDaysChanged);
    mSpinStack->addWidget(mSpinBox);

    mTimeSpinBox = new TimeSpinBox(0, 99999, mSpinStack);
    mTimeSpinBox->setRange(1, maxMinutes);    // max 999H59M
    connect(mTimeSpinBox, &TimeSpinBox::valueChanged, this, &TimePeriod::slotTimeChanged);
    mSpinStack->addWidget(mTimeSpinBox);

    mHourMinuteRaised = mNoHourMinute;
    showHourMin(!mNoHourMinute);
    layout->addWidget(mSpinStack);

    mUnitsCombo = new ComboBox(this);
    mUnitsCombo->setEditable(false);
    if (mNoHourMinute)
        mDateOnlyOffset = 2;
    else
    {
        mDateOnlyOffset = 0;
        mUnitsCombo->addItem(i18n_minutes());
        mUnitsCombo->addItem(i18n_hours_mins());
    }
    mUnitsCombo->addItem(i18n_days());
    mUnitsCombo->addItem(i18n_weeks());
    mMaxUnitShown = Weeks;
    connect(mUnitsCombo, &ComboBox::activated, this, &TimePeriod::slotUnitsSelected);
    layout->addWidget(mUnitsCombo);

    setFocusProxy(mUnitsCombo);
    setTabOrder(mUnitsCombo, mSpinStack);
}

void TimePeriod::setReadOnly(bool ro)
{
    if (ro != mReadOnly)
    {
        mReadOnly = ro;
        mSpinBox->setReadOnly(ro);
        mTimeSpinBox->setReadOnly(ro);
        mUnitsCombo->setReadOnly(ro);
    }
}

/******************************************************************************
*  Set whether the editor text is to be selected whenever spin buttons are
*  clicked. Default is to select them.
*/
void TimePeriod::setSelectOnStep(bool sel)
{
    mSpinBox->setSelectOnStep(sel);
    mTimeSpinBox->setSelectOnStep(sel);
}

/******************************************************************************
*  Set the input focus on the count field.
*/
void TimePeriod::setFocusOnCount()
{
    mSpinStack->setFocus();
}

/******************************************************************************
*  Set the maximum values for the hours:minutes and days/weeks spinboxes.
*  If 'hourmin' = 0, the hours:minutes maximum is left unchanged.
*/
void TimePeriod::setMaximum(int hourmin, int days)
{
    const Duration oldmins = period();
    if (hourmin > 0)
    {
        if (hourmin > maxMinutes)
            hourmin = maxMinutes;
        mTimeSpinBox->setRange(1, hourmin);
    }
    mMaxDays = (days >= 0) ? days : 0;
    adjustDayWeekShown();
    setUnitRange();
    const Duration mins = period();
    if (mins != oldmins)
        Q_EMIT valueChanged(mins);
}

/******************************************************************************
 * Get the specified time period.
 * Reply = 0 if error.
 */
Duration TimePeriod::period() const
{
    int factor = 1;
    switch (mUnitsCombo->currentIndex() + mDateOnlyOffset)
    {
        case HoursMinutes:
            return Duration(mTimeSpinBox->value() * 60, Duration::Seconds);
        case Minutes:
            return Duration(mSpinBox->value() * 60, Duration::Seconds);
        case Weeks:
            factor = 7;
            // fall through to DAYS
            Q_FALLTHROUGH();
        case Days:
            return Duration(mSpinBox->value() * factor, Duration::Days);
    }
    return 0;
}

/******************************************************************************
*  Initialise the controls with a specified time period.
*  The time unit combo-box is initialised to 'defaultUnits', but if 'dateOnly'
*  is true, it will never be initialised to minutes or hours/minutes.
*/
void TimePeriod::setPeriod(const Duration& perod, bool dateOnly, TimePeriod::Units defaultUnits)
{
    const Duration oldinterval = period();
    if (!dateOnly  &&  mNoHourMinute)
        dateOnly = true;
    int item;
    if (!perod.isNull())
    {
        int count = perod.value();
        if (perod.isDaily())
        {
            if (count % 7)
                item = Days;
            else
            {
                item = Weeks;
                count /= 7;
            }
        }
        else
        {
            count /= 60;   // minutes
            item = (defaultUnits == Minutes && count <= mSpinBox->maximum()) ? Minutes : HoursMinutes;
        }
        if (item < mDateOnlyOffset)
            item = mDateOnlyOffset;
        else if (item > mMaxUnitShown)
            item = mMaxUnitShown;
        mUnitsCombo->setCurrentIndex(item - mDateOnlyOffset);
        if (item == HoursMinutes)
            mTimeSpinBox->setValue(count);
        else
            mSpinBox->setValue(count);
        item = setDateOnly(perod, dateOnly, false);
    }
    else
    {
        item = defaultUnits;
        if (item < mDateOnlyOffset)
            item = mDateOnlyOffset;
        else if (item > mMaxUnitShown)
            item = mMaxUnitShown;
        mUnitsCombo->setCurrentIndex(item - mDateOnlyOffset);
        if ((dateOnly && !mDateOnlyOffset)  ||  (!dateOnly && mDateOnlyOffset))
            item = setDateOnly(perod, dateOnly, false);
    }
    setUnitRange();
    showHourMin(item == HoursMinutes  &&  !mNoHourMinute);

    const Duration newinterval = period();
    if (newinterval != oldinterval)
        Q_EMIT valueChanged(newinterval);
}

/******************************************************************************
*  Enable/disable hours/minutes units (if hours/minutes were permitted in the
*  constructor).
*/
TimePeriod::Units TimePeriod::setDateOnly(const Duration& perod, bool dateOnly, bool signal)
{
    Duration oldinterval = 0;
    if (signal)
        oldinterval = period();
    int index = mUnitsCombo->currentIndex();
    auto units = static_cast<Units>(index + mDateOnlyOffset);
    if (!mNoHourMinute)
    {
        if (!dateOnly  &&  mDateOnlyOffset)
        {
            // Change from date-only to allow hours/minutes
            mUnitsCombo->insertItem(0, i18n_minutes());
            mUnitsCombo->insertItem(1, i18n_hours_mins());
            mDateOnlyOffset = 0;
            adjustDayWeekShown();
            mUnitsCombo->setCurrentIndex(index + 2);
        }
        else if (dateOnly  &&  !mDateOnlyOffset)
        {
            // Change from allowing hours/minutes to date-only
            mUnitsCombo->removeItem(0);
            mUnitsCombo->removeItem(0);
            mDateOnlyOffset = 2;
            if (index > 2)
                index -= 2;
            else
                index = 0;
            adjustDayWeekShown();
            mUnitsCombo->setCurrentIndex(index);
            if (units == HoursMinutes  ||  units == Minutes)
            {
                // Set units to days and round up the warning period
                units = Days;
                mUnitsCombo->setCurrentIndex(Days - mDateOnlyOffset);
                mSpinBox->setValue(perod.asDays());
            }
            showHourMin(false);
        }
    }

    if (signal)
    {
        const Duration newinterval = period();
        if (newinterval != oldinterval)
            Q_EMIT valueChanged(newinterval);
    }
    return units;
}

/******************************************************************************
*  Adjust the days/weeks units shown to suit the maximum days limit.
*/
void TimePeriod::adjustDayWeekShown()
{
    const Units newMaxUnitShown = (mMaxDays >= 7) ? Weeks : (mMaxDays || mDateOnlyOffset) ? Days : HoursMinutes;
    if (newMaxUnitShown > mMaxUnitShown)
    {
        if (mMaxUnitShown < Days)
            mUnitsCombo->addItem(i18n_days());
        if (newMaxUnitShown == Weeks)
            mUnitsCombo->addItem(i18n_weeks());
    }
    else if (newMaxUnitShown < mMaxUnitShown)
    {
        if (mMaxUnitShown == Weeks)
            mUnitsCombo->removeItem(Weeks - mDateOnlyOffset);
        if (newMaxUnitShown < Days)
            mUnitsCombo->removeItem(Days - mDateOnlyOffset);
    }
    mMaxUnitShown = newMaxUnitShown;
}

/******************************************************************************
*  Set the maximum value which may be entered into the day/week count field,
*  depending on the current unit selection.
*/
void TimePeriod::setUnitRange()
{
    int maxval;
    switch (static_cast<Units>(mUnitsCombo->currentIndex() + mDateOnlyOffset))
    {
        case Weeks:
            maxval = mMaxDays / 7;
            if (maxval)
                break;
            mUnitsCombo->setCurrentIndex(Days - mDateOnlyOffset);
            // fall through to Days
            Q_FALLTHROUGH();
        case Days:
            maxval = mMaxDays ? mMaxDays : 1;
            break;
        case Minutes:
            maxval = mTimeSpinBox->maximum();
            break;
        case HoursMinutes:
        default:
            return;
    }
    mSpinBox->setRange(1, maxval);
}

/******************************************************************************
* Set the time units selection.
*/
void TimePeriod::setUnits(Units units)
{
    const auto oldUnits = static_cast<Units>(mUnitsCombo->currentIndex() + mDateOnlyOffset);
    if (units == oldUnits)
        return;
    if (oldUnits == HoursMinutes  &&  units == Minutes)
    {
        if (mTimeSpinBox->value() > mSpinBox->maximum())
            return;
        mSpinBox->setValue(mTimeSpinBox->value());
    }
    else if (oldUnits == Minutes  &&  units == HoursMinutes)
        mTimeSpinBox->setValue(mSpinBox->value());
    if (units >= mDateOnlyOffset  &&  units <= mMaxUnitShown)
    {
        const int item = units - mDateOnlyOffset;
        mUnitsCombo->setCurrentIndex(item);
        slotUnitsSelected(item);
    }
}

/******************************************************************************
* Return the current time units selection.
*/
TimePeriod::Units TimePeriod::units() const
{
    return static_cast<Units>(mUnitsCombo->currentIndex() + mDateOnlyOffset);
}

/******************************************************************************
*  Called when a new item is made current in the time units combo box.
*/
void TimePeriod::slotUnitsSelected(int index)
{
    setUnitRange();
    showHourMin(index + mDateOnlyOffset == HoursMinutes);
    Q_EMIT valueChanged(period());
}

/******************************************************************************
*  Called when the value of the days/weeks spin box changes.
*/
void TimePeriod::slotDaysChanged(int)
{
    if (!mHourMinuteRaised)
        Q_EMIT valueChanged(period());
}

/******************************************************************************
*  Called when the value of the time spin box changes.
*/
void TimePeriod::slotTimeChanged(int)
{
    if (mHourMinuteRaised)
        Q_EMIT valueChanged(period());
}

/******************************************************************************
 * Set the currently displayed count widget.
 */
void TimePeriod::showHourMin(bool hourMinute)
{
    if (hourMinute != mHourMinuteRaised)
    {
        mHourMinuteRaised = hourMinute;
        if (hourMinute)
        {
            mSpinStack->setCurrentWidget(mTimeSpinBox);
            mSpinStack->setFocusProxy(mTimeSpinBox);
        }
        else
        {
            mSpinStack->setCurrentWidget(mSpinBox);
            mSpinStack->setFocusProxy(mSpinBox);
        }
    }
}

/******************************************************************************
 * Set separate WhatsThis texts for the count spinboxes and the units combobox.
 * If the hours:minutes text is omitted, both spinboxes are set to the same
 * WhatsThis text.
 */
void TimePeriod::setWhatsThises(const QString& units, const QString& dayWeek, const QString& hourMin)
{
    mUnitsCombo->setWhatsThis(units);
    mSpinBox->setWhatsThis(dayWeek);
    mTimeSpinBox->setWhatsThis(hourMin.isNull() ? dayWeek : hourMin);
}

// vim: et sw=4:
