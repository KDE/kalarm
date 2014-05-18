/*
 *  timeperiod.h  -  time period data entry widget
 *  Program:  kalarm
 *  Copyright © 2003-2005,2007,2008,2010 by David Jarvie <djarvie@kde.org>
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
#include "timeperiod.h"
#include "combobox.h"
#include "spinbox.h"
#include "timespinbox.h"

#include <klocale.h>
#include <kdialog.h>

#include <QStackedWidget>

using namespace KCalCore;

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
    : KHBox(parent),
      mMaxDays(9999),
      mNoHourMinute(!allowHourMinute),
      mReadOnly(false)
{
    setSpacing(KDialog::spacingHint());

    mSpinStack = new QStackedWidget(this);
    mSpinBox = new SpinBox(mSpinStack);
    mSpinBox->setSingleStep(1);
    mSpinBox->setSingleShiftStep(10);
    mSpinBox->setRange(1, mMaxDays);
    connect(mSpinBox, SIGNAL(valueChanged(int)), SLOT(slotDaysChanged(int)));
    mSpinStack->addWidget(mSpinBox);

    mTimeSpinBox = new TimeSpinBox(0, 99999, mSpinStack);
    mTimeSpinBox->setRange(1, maxMinutes);    // max 999H59M
    connect(mTimeSpinBox, SIGNAL(valueChanged(int)), SLOT(slotTimeChanged(int)));
    mSpinStack->addWidget(mTimeSpinBox);

    mSpinStack->setFixedSize(mSpinBox->sizeHint().expandedTo(mTimeSpinBox->sizeHint()));
    mHourMinuteRaised = mNoHourMinute;
    showHourMin(!mNoHourMinute);

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
    mUnitsCombo->setFixedSize(mUnitsCombo->sizeHint());
    connect(mUnitsCombo, SIGNAL(activated(int)), SLOT(slotUnitsSelected(int)));

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
    Duration oldmins = period();
    if (hourmin > 0)
    {
        if (hourmin > maxMinutes)
            hourmin = maxMinutes;
        mTimeSpinBox->setRange(1, hourmin);
    }
    mMaxDays = (days >= 0) ? days : 0;
    adjustDayWeekShown();
    setUnitRange();
    Duration mins = period();
    if (mins != oldmins)
        emit valueChanged(mins);
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
    Duration oldinterval = period();
    if (!dateOnly  &&  mNoHourMinute)
        dateOnly = true;
    int item;
    if (perod)
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

    Duration newinterval = period();
    if (newinterval != oldinterval)
        emit valueChanged(newinterval);
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
    Units units = static_cast<Units>(index + mDateOnlyOffset);
    if (!mNoHourMinute)
    {
        if (!dateOnly  &&  mDateOnlyOffset)
        {
            // Change from date-only to allow hours/minutes
            mUnitsCombo->insertItem(0, i18n_minutes());
            mUnitsCombo->insertItem(1, i18n_hours_mins());
            mDateOnlyOffset = 0;
            adjustDayWeekShown();
            mUnitsCombo->setCurrentIndex(index += 2);
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
        Duration newinterval = period();
        if (newinterval != oldinterval)
            emit valueChanged(newinterval);
    }
    return units;
}

/******************************************************************************
*  Adjust the days/weeks units shown to suit the maximum days limit.
*/
void TimePeriod::adjustDayWeekShown()
{
    Units newMaxUnitShown = (mMaxDays >= 7) ? Weeks : (mMaxDays || mDateOnlyOffset) ? Days : HoursMinutes;
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
    Units oldUnits = static_cast<Units>(mUnitsCombo->currentIndex() + mDateOnlyOffset);
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
        int item = units - mDateOnlyOffset;
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
    emit valueChanged(period());
}

/******************************************************************************
*  Called when the value of the days/weeks spin box changes.
*/
void TimePeriod::slotDaysChanged(int)
{
    if (!mHourMinuteRaised)
        emit valueChanged(period());
}

/******************************************************************************
*  Called when the value of the time spin box changes.
*/
void TimePeriod::slotTimeChanged(int)
{
    if (mHourMinuteRaised)
        emit valueChanged(period());
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
