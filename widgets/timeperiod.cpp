/*
 *  timeperiod.h  -  time period data entry widget
 *  Program:  kalarm
 *  (C) 2003, 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "kalarm.h"

#include <qwidgetstack.h>
#include <qwhatsthis.h>

#include <klocale.h>
#include <kdialog.h>

#include "combobox.h"
#include "spinbox.h"
#include "timespinbox.h"
#include "timeperiod.moc"


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString TimePeriod::i18n_hours_mins()   { return i18n("hours/minutes"); }
QString TimePeriod::i18n_Hours_Mins()   { return i18n("Hours/Minutes"); }
QString TimePeriod::i18n_days()         { return i18n("days"); }
QString TimePeriod::i18n_Days()         { return i18n("Days"); }
QString TimePeriod::i18n_weeks()        { return i18n("weeks"); }
QString TimePeriod::i18n_Weeks()        { return i18n("Weeks"); }

static const int maxMinutes = 100*60-1;   // absolute maximum value for hours:minutes = 99H59M

/*=============================================================================
= Class TimePeriod
= Contains a time unit combo box, plus a time spinbox, to select a time period.
=============================================================================*/

TimePeriod::TimePeriod(bool allowHourMinute, QWidget* parent, const char* name)
	: QHBox(parent, name),
	  mMaxDays(9999),
	  mNoHourMinute(!allowHourMinute),
	  mReadOnly(false)
{
	setSpacing(KDialog::spacingHint());

	mSpinStack = new QWidgetStack(this);
	mSpinBox = new SpinBox(mSpinStack);
	mSpinBox->setLineStep(1);
	mSpinBox->setLineShiftStep(10);
	mSpinBox->setRange(1, mMaxDays);
	connect(mSpinBox, SIGNAL(valueChanged(int)), SLOT(slotDaysChanged(int)));
	mSpinStack->addWidget(mSpinBox, 0);

	mTimeSpinBox = new TimeSpinBox(0, 99999, mSpinStack);
	mTimeSpinBox->setRange(1, maxMinutes);    // max 99H59M
	connect(mTimeSpinBox, SIGNAL(valueChanged(int)), SLOT(slotTimeChanged(int)));
	mSpinStack->addWidget(mTimeSpinBox, 1);

	mSpinStack->setFixedSize(mSpinBox->sizeHint().expandedTo(mTimeSpinBox->sizeHint()));
	mHourMinuteRaised = mNoHourMinute;
	showHourMin(!mNoHourMinute);

	mUnitsCombo = new ComboBox(false, this);
	if (mNoHourMinute)
		mDateOnlyOffset = 1;
	else
	{
		mDateOnlyOffset = 0;
		mUnitsCombo->insertItem(i18n_hours_mins());
	}
	mUnitsCombo->insertItem(i18n_days());
	mUnitsCombo->insertItem(i18n_weeks());
	mMaxUnitShown = WEEKS;
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
	int oldmins = minutes();
	if (hourmin > 0)
	{
		if (hourmin > maxMinutes)
			hourmin = maxMinutes;
		mTimeSpinBox->setRange(1, hourmin);
	}
	mMaxDays = (days >= 0) ? days : 0;
	adjustDayWeekShown();
	setUnitRange();
	int mins = minutes();
	if (mins != oldmins)
		emit valueChanged(mins);
}

/******************************************************************************
 * Get the specified number of minutes.
 * Reply = 0 if error.
 */
int TimePeriod::minutes() const
{
	switch (mUnitsCombo->currentItem() + mDateOnlyOffset)
	{
		case HOURS_MINUTES:
			return mTimeSpinBox->value();
		case DAYS:
			return mSpinBox->value() * 24*60;
		case WEEKS:
			return mSpinBox->value() * 7*24*60;
	}
	return 0;
}

/******************************************************************************
*  Initialise the controls with a specified time period.
*  The time unit combo-box is initialised to 'defaultUnits', but if 'dateOnly'
*  is true, it will never be initialised to hours/minutes.
*/
void TimePeriod::setMinutes(int mins, bool dateOnly, TimePeriod::Units defaultUnits)
{
	int oldmins = minutes();
	if (!dateOnly  &&  mNoHourMinute)
		dateOnly = true;
	int item;
	if (mins)
	{
		int count = mins;
		if (mins % (24*60))
			item = HOURS_MINUTES;
		else if (mins % (7*24*60))
		{
			item = DAYS;
			count = mins / (24*60);
		}
		else
		{
			item = WEEKS;
			count = mins / (7*24*60);
		}
		if (item < mDateOnlyOffset)
			item = mDateOnlyOffset;
		else if (item > mMaxUnitShown)
			item = mMaxUnitShown;
		mUnitsCombo->setCurrentItem(item - mDateOnlyOffset);
		if (item == HOURS_MINUTES)
			mTimeSpinBox->setValue(count);
		else
			mSpinBox->setValue(count);
		item = setDateOnly(mins, dateOnly, false);
	}
	else
	{
		item = defaultUnits;
		if (item < mDateOnlyOffset)
			item = mDateOnlyOffset;
		else if (item > mMaxUnitShown)
			item = mMaxUnitShown;
		mUnitsCombo->setCurrentItem(item - mDateOnlyOffset);
		if (dateOnly && !mDateOnlyOffset  ||  !dateOnly && mDateOnlyOffset)
			item = setDateOnly(mins, dateOnly, false);
	}
	showHourMin(item == HOURS_MINUTES  &&  !mNoHourMinute);

	int newmins = minutes();
	if (newmins != oldmins)
		emit valueChanged(newmins);
}

/******************************************************************************
*  Enable/disable hours/minutes units (if hours/minutes were permitted in the
*  constructor).
*/
TimePeriod::Units TimePeriod::setDateOnly(int mins, bool dateOnly, bool signal)
{
	int oldmins = 0;
	if (signal)
		oldmins = minutes();
	int index = mUnitsCombo->currentItem();
	Units units = static_cast<Units>(index + mDateOnlyOffset);
	if (!mNoHourMinute)
	{
		if (!dateOnly  &&  mDateOnlyOffset)
		{
			// Change from date-only to allow hours/minutes
			mUnitsCombo->insertItem(i18n_hours_mins(), 0);
			mDateOnlyOffset = 0;
			adjustDayWeekShown();
			mUnitsCombo->setCurrentItem(++index);
		}
		else if (dateOnly  &&  !mDateOnlyOffset)
		{
			// Change from allowing hours/minutes to date-only
			mUnitsCombo->removeItem(0);
			mDateOnlyOffset = 1;
			if (index)
				--index;
			adjustDayWeekShown();
			mUnitsCombo->setCurrentItem(index);
			if (units == HOURS_MINUTES)
			{
				// Set units to days and round up the warning period
				units = DAYS;
				mUnitsCombo->setCurrentItem(DAYS - mDateOnlyOffset);
				mSpinBox->setValue((mins + 1439) / 1440);
			}
			showHourMin(false);
		}
	}

	if (signal)
	{
		int newmins = minutes();
		if (newmins != oldmins)
			emit valueChanged(newmins);
	}
	return units;
}

/******************************************************************************
*  Adjust the days/weeks units shown to suit the maximum days limit.
*/
void TimePeriod::adjustDayWeekShown()
{
	Units newMaxUnitShown = (mMaxDays >= 7) ? WEEKS : (mMaxDays || mDateOnlyOffset) ? DAYS : HOURS_MINUTES;
	if (newMaxUnitShown > mMaxUnitShown)
	{
		if (mMaxUnitShown < DAYS)
			mUnitsCombo->insertItem(i18n_days());
		if (newMaxUnitShown == WEEKS)
			mUnitsCombo->insertItem(i18n_weeks());
	}
	else if (newMaxUnitShown < mMaxUnitShown)
	{
		if (mMaxUnitShown == WEEKS)
			mUnitsCombo->removeItem(WEEKS - mDateOnlyOffset);
		if (newMaxUnitShown < DAYS)
			mUnitsCombo->removeItem(DAYS - mDateOnlyOffset);
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
	switch (static_cast<Units>(mUnitsCombo->currentItem() + mDateOnlyOffset))
	{
		case WEEKS:
			maxval = mMaxDays / 7;
			if (maxval)
				break;
			mUnitsCombo->setCurrentItem(DAYS - mDateOnlyOffset);
			// fall through to DAYS
		case DAYS:
			maxval = mMaxDays ? mMaxDays : 1;
			break;
		case HOURS_MINUTES:
		default:
			return;
	}
	mSpinBox->setRange(1, maxval);
}

/******************************************************************************
*  Called when a new item is made current in the time units combo box.
*/
void TimePeriod::slotUnitsSelected(int index)
{
	setUnitRange();
	showHourMin(index + mDateOnlyOffset == HOURS_MINUTES);
	emit valueChanged(minutes());
}

/******************************************************************************
*  Called when the value of the days/weeks spin box changes.
*/
void TimePeriod::slotDaysChanged(int)
{
	if (!mHourMinuteRaised)
		emit valueChanged(minutes());
}

/******************************************************************************
*  Called when the value of the time spin box changes.
*/
void TimePeriod::slotTimeChanged(int value)
{
	if (mHourMinuteRaised)
		emit valueChanged(value);
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
			mSpinStack->raiseWidget(mTimeSpinBox);
			mSpinStack->setFocusProxy(mTimeSpinBox);
		}
		else
		{
			mSpinStack->raiseWidget(mSpinBox);
			mSpinStack->setFocusProxy(mSpinBox);
		}
	}
}

/******************************************************************************
 * Set separate WhatsThis texts for the count spinboxes and the units combobox.
 * If the hours:minutes text is omitted, both spinboxes are set to the same
 * WhatsThis text.
 */
void TimePeriod::setWhatsThis(const QString& units, const QString& dayWeek, const QString& hourMin)
{
	QWhatsThis::add(mUnitsCombo, units);
	QWhatsThis::add(mSpinBox, dayWeek);
	QWhatsThis::add(mTimeSpinBox, (hourMin.isNull() ? dayWeek : hourMin));
}
