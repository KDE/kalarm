/*
 *  timeselector.cpp  -  widget to optionally set a time period
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
 */

#include "kalarm.h"

#include <qlayout.h>
#include <qlabel.h>
#include <qhbox.h>
#include <qwhatsthis.h>

#include <klocale.h>
#include <kdialog.h>
#include <kdebug.h>

#include "checkbox.h"
#include "combobox.h"
#include "timeperiod.h"
#include "timeselector.moc"


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString TimeSelector::i18n_hours_mins()   { return i18n("hours/minutes"); }
QString TimeSelector::i18n_Hours_Mins()   { return i18n("Hours/Minutes"); }
QString TimeSelector::i18n_days()         { return i18n("days"); }
QString TimeSelector::i18n_Days()         { return i18n("Days"); }
QString TimeSelector::i18n_weeks()        { return i18n("weeks"); }
QString TimeSelector::i18n_Weeks()        { return i18n("Weeks"); }


TimeSelector::TimeSelector(const QString& selectText, const QString& postfix, const QString& selectWhatsThis,
                           const QString& valueWhatsThis, bool allowHourMinute, QWidget* parent, const char* name)
	: QFrame(parent, name),
	  mMaxDays(9999),
	  mNoHourMinute(!allowHourMinute),
	  mReadOnly(false)
{
	setFrameStyle(QFrame::NoFrame);
	QVBoxLayout* topLayout = new QVBoxLayout(this, 0, KDialog::spacingHint());
	QHBoxLayout* layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
	mSelect = new CheckBox(selectText, this);
	mSelect->setFixedSize(mSelect->sizeHint());
	connect(mSelect, SIGNAL(toggled(bool)), SLOT(slotSelectToggled(bool)));
	QWhatsThis::add(mSelect, selectWhatsThis);
	layout->addWidget(mSelect);

	QHBox* box = new QHBox(this);    // to group widgets for QWhatsThis text
	box->setSpacing(KDialog::spacingHint());
	layout->addWidget(box);
	mCount = new TimePeriod(box);
	mCount->setHourMinRange(1, 100*60-1);    // max 99H59M
	mCount->setUnitRange(1, mMaxDays);
	mCount->setUnitSteps(1, 10);
	mCount->setFixedSize(mCount->sizeHint());
	mCount->setSelectOnStep(false);
	mSelect->setFocusWidget(mCount);

	mUnitsCombo = new ComboBox(false, box);
	if (mNoHourMinute)
	{
		mDateOnlyOffset = 1;
		mCount->showHourMin(false);
	}
	else
	{
		mDateOnlyOffset = 0;
		mUnitsCombo->insertItem(i18n_hours_mins());
	}
	mUnitsCombo->insertItem(i18n_days());
	mUnitsCombo->insertItem(i18n_weeks());
	mUnitsCombo->setFixedSize(mUnitsCombo->sizeHint());
	connect(mUnitsCombo, SIGNAL(activated(int)), SLOT(slotUnitsSelected(int)));

	mLabel = new QLabel(postfix, box);
	QWhatsThis::add(box, valueWhatsThis);
}

/******************************************************************************
*  Set the read-only status.
*/
void TimeSelector::setReadOnly(bool ro)
{
	if ((int)ro != (int)mReadOnly)
	{
		mReadOnly = ro;
		mSelect->setReadOnly(mReadOnly);
		mCount->setReadOnly(mReadOnly);
		mUnitsCombo->setReadOnly(mReadOnly);
	}
}

bool TimeSelector::isChecked() const
{
	return mSelect->isChecked();
}

void TimeSelector::setMaximum(int hourmin, int days)
{
	if (hourmin)
		mCount->setHourMinRange(1, hourmin);
	mMaxDays = days;
	setUnitRange();
}

/******************************************************************************
 * Get the specified number of minutes.
 * Reply = 0 if unselected.
 */
int TimeSelector::getMinutes() const
{
	if (!mSelect->isChecked())
		return 0;
	int warning = mCount->value();
	switch (mUnitsCombo->currentItem() + mDateOnlyOffset)
	{
		case HOURS_MINUTES:
			break;
		case DAYS:
			warning *= 24*60;
			break;
		case WEEKS:
			warning *= 7*24*60;
			break;
	}
	return warning;
}

/******************************************************************************
*  Initialise the controls with a specified time period.
*  If minutes = 0, it will be deselected.
*  The time unit combo-box is initialised to 'defaultUnits', but if 'dateOnly'
*  is true, it will never be initialised to hours/minutes.
*/
void TimeSelector::setMinutes(int minutes, bool dateOnly, TimeSelector::Units defaultUnits)
{
	if (!dateOnly  &&  mNoHourMinute)
		dateOnly = true;
	mSelect->setChecked(minutes);
	mCount->setEnabled(minutes);
	mUnitsCombo->setEnabled(minutes);
	mLabel->setEnabled(minutes);
	int item;
	if (minutes)
	{
		int count = minutes;
		if (minutes % (24*60))
			item = HOURS_MINUTES;
		else if (minutes % (7*24*60))
		{
			item = DAYS;
			count = minutes / (24*60);
		}
		else
		{
			item = WEEKS;
			count = minutes / (7*24*60);
		}
		if (item < mDateOnlyOffset)
			item = mDateOnlyOffset;
		mUnitsCombo->setCurrentItem(item - mDateOnlyOffset);
		if (item == HOURS_MINUTES)
			mCount->setHourMinValue(count);
		else
			mCount->setUnitValue(count);
		item = setDateOnly(minutes, dateOnly);
	}
	else
	{
		item = defaultUnits;
		if (item < mDateOnlyOffset)
			item = mDateOnlyOffset;
		mUnitsCombo->setCurrentItem(item - mDateOnlyOffset);
		if (dateOnly && !mDateOnlyOffset  ||  !dateOnly && mDateOnlyOffset)
			item = setDateOnly(minutes, dateOnly);
	}
	mCount->showHourMin(item == HOURS_MINUTES  &&  !mNoHourMinute);
}

/******************************************************************************
*  Set the units to days if "Any time" is selected.
*/
TimeSelector::Units TimeSelector::setDateOnly(int reminderMinutes, bool dateOnly)
{
	int index = mUnitsCombo->currentItem();
	Units units = static_cast<Units>(index + mDateOnlyOffset);
	if (!mNoHourMinute)
	{
		if (!dateOnly  &&  mDateOnlyOffset)
		{
			// Change from date-only to allow hours/minutes
			mUnitsCombo->insertItem(i18n_hours_mins(), 0);
			mDateOnlyOffset = 0;
			mUnitsCombo->setCurrentItem(++index);
		}
		else if (dateOnly  &&  !mDateOnlyOffset)
		{
			// Change from allowing hours/minutes to date-only
			mUnitsCombo->removeItem(0);
			mDateOnlyOffset = 1;
			if (index)
				--index;
			mUnitsCombo->setCurrentItem(index);
			if (units == HOURS_MINUTES)
			{
				// Set units to days and round up the warning period
				units = DAYS;
				mUnitsCombo->setCurrentItem(DAYS - mDateOnlyOffset);
				mCount->showUnit();
				mCount->setUnitValue((reminderMinutes + 1439) / 1440);
			}
			mCount->showHourMin(false);
		}
	}
	return units;
}

/******************************************************************************
*  Set the maximum value which may be entered into the unit count field,
*  depending on the current unit selection.
*/
void TimeSelector::setUnitRange()
{
	int maxval;
	switch (static_cast<Units>(mUnitsCombo->currentItem() + mDateOnlyOffset))
	{
		case DAYS:   maxval = mMaxDays;  break;
		case WEEKS:  maxval = mMaxDays / 7;  break;
		case HOURS_MINUTES:
		default:             return;
	}
	mCount->setUnitRange(1, maxval);
}

/******************************************************************************
*  Set the input focus on the count field.
*/
void TimeSelector::setFocusOnCount()
{
	mCount->setFocus();
}

/******************************************************************************
*  Called when the TimeSelector checkbox is toggled.
*/
void TimeSelector::slotSelectToggled(bool on)
{
	mUnitsCombo->setEnabled(on);
	mCount->setEnabled(on);
	mLabel->setEnabled(on);
	if (on  &&  mDateOnlyOffset)
	 	setDateOnly(getMinutes(), true);
	emit toggled(on);
}

/******************************************************************************
*  Called when a new item is made current in the time units combo box.
*/
void TimeSelector::slotUnitsSelected(int index)
{
	setUnitRange();
	mCount->showHourMin(index + mDateOnlyOffset == HOURS_MINUTES);
}
