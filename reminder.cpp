/*
 *  reminder.cpp  -  reminder setting widget
 *  Program:  kalarm
 *  (C) 2003 by David Jarvie <software@astrojar.org.uk>
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

#include <qlayout.h>
#include <qlabel.h>
#include <qhbox.h>
#include <qwhatsthis.h>
#include <qtooltip.h>

#include <kglobal.h>
#include <klocale.h>
#include <kdialog.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "preferences.h"
#include "checkbox.h"
#include "combobox.h"
#include "timeperiod.h"
#include "reminder.moc"


Reminder::Reminder(const QString& caption, const QString& reminderWhatsThis, const QString& valueWhatsThis,
                   bool allowHourMinute, QWidget* parent, const char* name)
	: QFrame(parent, name),
	  mMaxDays(9999),
	  mNoHourMinute(!allowHourMinute),
	  mReadOnly(false)
{
	setFrameStyle(QFrame::NoFrame);
	QHBoxLayout* layout = new QHBoxLayout(this, 0, KDialog::spacingHint());
	mReminder = new CheckBox(caption, this);
	mReminder->setFixedSize(mReminder->sizeHint());
	connect(mReminder, SIGNAL(toggled(bool)), SLOT(slotReminderToggled(bool)));
	QWhatsThis::add(mReminder, reminderWhatsThis);
	layout->addWidget(mReminder);

	QHBox* box = new QHBox(this);    // to group widgets for QWhatsThis text
	box->setSpacing(KDialog::spacingHint());
	layout->addWidget(box);
	mCount = new TimePeriod(box);
	mCount->setHourMinRange(1, 100*60-1);    // max 99H59M
	mCount->setUnitRange(1, mMaxDays);
	mCount->setUnitSteps(1, 10);
	mCount->setFixedSize(mCount->sizeHint());
	mCount->setSelectOnStep(false);
	mReminder->setFocusWidget(mCount);

	mUnitsCombo = new ComboBox(false, box);
	if (mNoHourMinute)
	{
		mDateOnlyOffset = 1;
		mCount->showHourMin(false);
	}
	else
	{
		mDateOnlyOffset = 0;
		mUnitsCombo->insertItem(i18n("hours/minutes"));
	}
	mUnitsCombo->insertItem(i18n("days"));
	mUnitsCombo->insertItem(i18n("weeks"));
	mUnitsCombo->setFixedSize(mUnitsCombo->sizeHint());
	connect(mUnitsCombo, SIGNAL(activated(int)), SLOT(slotUnitsSelected(int)));

	mLabel = new QLabel(i18n("in advance"), box);
	QWhatsThis::add(box, valueWhatsThis);
	layout->addStretch();
}

/******************************************************************************
*  Set the read-only status.
*/
void Reminder::setReadOnly(bool ro)
{
	if ((int)ro != (int)mReadOnly)
	{
		mReadOnly = ro;
		mReminder->setReadOnly(mReadOnly);
		mCount->setReadOnly(mReadOnly);
		mUnitsCombo->setReadOnly(mReadOnly);
	}
}

bool Reminder::isReminder() const
{
	return mReminder->isChecked();
}

void Reminder::setMaximum(int hourmin, int days)
{
	if (hourmin)
		mCount->setHourMinRange(1, hourmin);
	mMaxDays = days;
	setUnitRange();
}

/******************************************************************************
 * Get the specified number of minutes in advance of the main alarm the
 * reminder is to be.
 */
int Reminder::getMinutes() const
{
	if (!mReminder->isChecked())
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
*  Initialise the controls with a specified reminder time.
*/
void Reminder::setMinutes(int minutes, bool dateOnly)
{
	if (!dateOnly  &&  mNoHourMinute)
		dateOnly = true;
	bool on = !!minutes;
	mReminder->setChecked(on);
	mCount->setEnabled(on);
	mUnitsCombo->setEnabled(on);
	mLabel->setEnabled(on);
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
		item = Preferences::instance()->defaultReminderUnits();
		if (item < mDateOnlyOffset)
			item = mDateOnlyOffset;
		mUnitsCombo->setCurrentItem(item - mDateOnlyOffset);
		if (dateOnly && !mDateOnlyOffset  ||  !dateOnly && mDateOnlyOffset)
			item = setDateOnly(minutes, dateOnly);
	}
	mCount->showHourMin(item == HOURS_MINUTES  &&  !mNoHourMinute);
}

/******************************************************************************
*  Set the advance reminder units to days if "Any time" is checked.
*/
Reminder::Units Reminder::setDateOnly(int reminderMinutes, bool dateOnly)
{
	int index = mUnitsCombo->currentItem();
	Units units = static_cast<Units>(index + mDateOnlyOffset);
	if (!mNoHourMinute)
	{
		if (!dateOnly  &&  mDateOnlyOffset)
		{
			// Change from date-only to allow hours/minutes
			mUnitsCombo->insertItem(i18n("hours/minutes"), 0);
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
void Reminder::setUnitRange()
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
void Reminder::setFocusOnCount()
{
	mCount->setFocus();
}

/******************************************************************************
*  Called when the Reminder checkbox is toggled.
*/
void Reminder::slotReminderToggled(bool on)
{
	mUnitsCombo->setEnabled(on);
	mCount->setEnabled(on);
	mLabel->setEnabled(on);
	if (on  &&  mDateOnlyOffset)
	 	setDateOnly(getMinutes(), true);
}

/******************************************************************************
*  Called when a new item is made current in the reminder units combo box.
*/
void Reminder::slotUnitsSelected(int index)
{
	setUnitRange();
	mCount->showHourMin(index + mDateOnlyOffset == HOURS_MINUTES);
}
