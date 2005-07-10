/*
 *  timeedit.cpp  -  time-of-day edit widget, with AM/PM shown depending on locale
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
 */

#include "kalarm.h"

#include <kglobal.h>
#include <klocale.h>

#include "combobox.h"
#include "timespinbox.h"
#include "timeedit.moc"


TimeEdit::TimeEdit(QWidget* parent, const char* name)
	: QHBox(parent, name),
	  mAmPm(0),
	  mAmIndex(-1),
	  mPmIndex(-1),
	  mReadOnly(false)
{
	bool use12hour = KGlobal::locale()->use12Clock();
	mSpinBox = new TimeSpinBox(!use12hour, this);
	mSpinBox->setFixedSize(mSpinBox->sizeHint());
	connect(mSpinBox, SIGNAL(valueChanged(int)), SLOT(slotValueChanged(int)));
	if (use12hour)
	{
		mAmPm = new ComboBox(this);
		setAmPmCombo(1, 1);     // add "am" and "pm" options to the combo box
		mAmPm->setFixedSize(mAmPm->sizeHint());
		connect(mAmPm, SIGNAL(highlighted(int)), SLOT(slotAmPmChanged(int)));
	}
}

void TimeEdit::setReadOnly(bool ro)
{
	if (ro != mReadOnly)
	{
		mReadOnly = ro;
		mSpinBox->setReadOnly(ro);
		if (mAmPm)
			mAmPm->setReadOnly(ro);
	}
}

int TimeEdit::value() const
{
	return mSpinBox->value();
}

bool TimeEdit::isValid() const
{
	return mSpinBox->isValid();
}

/******************************************************************************
 * Set the edit value as valid or invalid.
 * If newly invalid, the value is displayed as asterisks.
 * If newly valid, the value is set to the minimum value.
 */
void TimeEdit::setValid(bool valid)
{
	bool oldValid = mSpinBox->isValid();
	if (valid  &&  !oldValid
	||  !valid  &&  oldValid)
	{
		mSpinBox->setValid(valid);
		if (mAmPm)
			mAmPm->setCurrentItem(0);
	}
}

/******************************************************************************
 * Set the widget's value.
 */
void TimeEdit::setValue(int minutes)
{
	if (mAmPm)
	{
		int i = (minutes >= 720) ? mPmIndex : mAmIndex;
		mAmPm->setCurrentItem(i >= 0 ? i : 0);
	}
	mSpinBox->setValue(minutes);
}

bool TimeEdit::wrapping() const
{
	return mSpinBox->wrapping();
}

void TimeEdit::setWrapping(bool on)
{
	mSpinBox->setWrapping(on);
}

int TimeEdit::minValue() const
{
	return mSpinBox->minValue();
}

int TimeEdit::maxValue() const
{
	return mSpinBox->maxValue();
}

void TimeEdit::setMinValue(int minutes)
{
	if (mAmPm)
		setAmPmCombo((minutes < 720 ? 1 : 0), -1);   // insert/remove "am" in combo box
	mSpinBox->setMinValue(minutes);
}

void TimeEdit::setMaxValue(int minutes)
{
	if (mAmPm)
		setAmPmCombo(-1, (minutes < 720 ? 0 : 1));   // insert/remove "pm" in combo box
	mSpinBox->setMaxValue(minutes);
}

/******************************************************************************
 * Called when the spin box value has changed.
 */
void TimeEdit::slotValueChanged(int value)
{
	if (mAmPm)
	{
		bool pm = (mAmPm->currentItem() == mPmIndex);
		if (pm  &&  value < 720)
			mAmPm->setCurrentItem(mAmIndex);
		else if (!pm  &&  value >= 720)
			mAmPm->setCurrentItem(mPmIndex);
	}
	emit valueChanged(value);
}

/******************************************************************************
 * Called when a new selection has been made by the user in the AM/PM combo box.
 * Adjust the current time value by 12 hours.
 */
void TimeEdit::slotAmPmChanged(int item)
{
	if (mAmPm)
	{
		int value = mSpinBox->value();
		if (item == mPmIndex  &&  value < 720)
			mSpinBox->setValue(value + 720);
		else if (item != mPmIndex  &&  value >= 720)
			mSpinBox->setValue(value - 720);
	}
}

/******************************************************************************
 * Set up the AM/PM combo box to contain the specified items.
 */
void TimeEdit::setAmPmCombo(int am, int pm)
{
	if (am > 0  &&  mAmIndex < 0)
	{
		// Insert "am"
		mAmIndex = 0;
		mAmPm->insertItem(KGlobal::locale()->translate("am"), mAmIndex);
		if (mPmIndex >= 0)
			mPmIndex = 1;
		mAmPm->setCurrentItem(mPmIndex >= 0 ? mPmIndex : mAmIndex);
	}
	else if (am == 0  &&  mAmIndex >= 0)
	{
		// Remove "am"
		mAmPm->removeItem(mAmIndex);
		mAmIndex = -1;
		if (mPmIndex >= 0)
			mPmIndex = 0;
		mAmPm->setCurrentItem(mPmIndex);
	}

	if (pm > 0  &&  mPmIndex < 0)
	{
		// Insert "pm"
		mPmIndex = mAmIndex + 1;
		mAmPm->insertItem(KGlobal::locale()->translate("pm"), mPmIndex);
		if (mAmIndex < 0)
			mAmPm->setCurrentItem(mPmIndex);
	}
	else if (pm == 0  &&  mPmIndex >= 0)
	{
		// Remove "pm"
		mAmPm->removeItem(mPmIndex);
		mPmIndex = -1;
		mAmPm->setCurrentItem(mAmIndex);
	}
}
