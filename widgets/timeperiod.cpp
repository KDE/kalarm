/*
 *  timeperiod.h  -  time period data entry widget
 *  Program:  kalarm
 *
 *  (C) 2003 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <qwhatsthis.h>

#include "spinbox.h"
#include "timespinbox.h"
#include "timeperiod.moc"


TimePeriod::TimePeriod(bool readOnly, QWidget* parent, const char* name)
	: QWidgetStack(parent, name),
	  mReadOnly(readOnly),
	  mHourMinute(true)
{
	mSpinBox = new SpinBox(this);
	mSpinBox->setLineShiftStep(10);
	mSpinBox->setSelectOnStep(false);
	mSpinBox->setReadOnly(mReadOnly);
	addWidget(mSpinBox, 0);

	mTimeSpinBox = new TimeSpinBox(0, 99999, this);
	mTimeSpinBox->setReadOnly(mReadOnly);
	addWidget(mTimeSpinBox, 1);

	setCurrent(false);   // initialise to show mSpinBox
}

void TimePeriod::setRange(bool hourMinute, int minValue, int maxValue)
{
	if (hourMinute)
		mTimeSpinBox->setRange(minValue, maxValue);
	else
		mSpinBox->setRange(minValue, maxValue);
}

void TimePeriod::setCountSteps(int step, int shiftStep)
{
	mSpinBox->setLineStep(step);
	mSpinBox->setLineShiftStep(shiftStep);
}

void TimePeriod::setHourMinuteSteps(int minuteStep, int minuteShiftStep, int hourStep, int hourShiftStep)
{
	mTimeSpinBox->setSteps(minuteStep, hourStep);
	mTimeSpinBox->setShiftSteps(minuteShiftStep, hourShiftStep);
}

void TimePeriod::setSelectOnStep(bool sel)
{
	mSpinBox->setSelectOnStep(sel);
}

/******************************************************************************
 * Set the currently displayed widget.
 */
void TimePeriod::setCurrent(bool hourMinute)
{
	if (hourMinute != mHourMinute)
	{
		mHourMinute = hourMinute;
		if (hourMinute)
		{
			raiseWidget(mTimeSpinBox);
			setFocusProxy(mTimeSpinBox);
		}
		else
		{
			raiseWidget(mSpinBox);
			setFocusProxy(mSpinBox);
		}
	}
}

void TimePeriod::setValue(bool hourMinute, int count)
{
	if (hourMinute)
		mTimeSpinBox->setValue(count);
	else
		mSpinBox->setValue(count);
}

int TimePeriod::value(bool hourMinute) const
{
	if (hourMinute)
		return mTimeSpinBox->value();
	else
		return mSpinBox->value();
}

void TimePeriod::setWhatsThis(bool hourMinute, const QString& text)
{
        QWidget *wid;
        if ( hourMinute ) wid = mTimeSpinBox;
        else wid = mSpinBox;
	QWhatsThis::add(wid, text);
}

QSize TimePeriod::sizeHint() const
{
	return mSpinBox->sizeHint().expandedTo(mTimeSpinBox->sizeHint());
}

QSize TimePeriod::minimumSizeHint() const
{
	return mSpinBox->minimumSizeHint().expandedTo(mTimeSpinBox->minimumSizeHint());
}
