/*
 *  timeperiod.h  -  time period data entry widget
 *  Program:  kalarm
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "kalarm.h"

#include <qwhatsthis.h>

#include "spinbox.h"
#include "timespinbox.h"
#include "timeperiod.moc"


TimePeriod::TimePeriod(QWidget* parent, const char* name)
	: QWidgetStack(parent, name),
	  mReadOnly(false),
	  mHourMinute(true)
{
	mSpinBox = new SpinBox(this);
	mSpinBox->setLineShiftStep(10);
	mSpinBox->setSelectOnStep(false);
	addWidget(mSpinBox, 0);

	mTimeSpinBox = new TimeSpinBox(0, 99999, this);
	addWidget(mTimeSpinBox, 1);

	showUnit();   // initialise to show mSpinBox
}

void TimePeriod::setReadOnly(bool ro)
{
	if ((int)ro != (int)mReadOnly)
	{
		mReadOnly = ro;
		mSpinBox->setReadOnly(ro);
		mTimeSpinBox->setReadOnly(ro);
	}
}

void TimePeriod::setUnitRange(int minValue, int maxValue)
{
	mSpinBox->setRange(minValue, maxValue);
}

void TimePeriod::setHourMinRange(int minValue, int maxValue)
{
	mTimeSpinBox->setRange(minValue, maxValue);
}

void TimePeriod::setUnitSteps(int step, int shiftStep)
{
	mSpinBox->setLineStep(step);
	mSpinBox->setLineShiftStep(shiftStep);
}

void TimePeriod::setHourMinSteps(int minuteStep, int minuteShiftStep, int hourStep, int hourShiftStep)
{
	mTimeSpinBox->setSteps(minuteStep, hourStep);
	mTimeSpinBox->setShiftSteps(minuteShiftStep, hourShiftStep);
}

void TimePeriod::setSelectOnStep(bool sel)
{
	mSpinBox->setSelectOnStep(sel);
	mTimeSpinBox->setSelectOnStep(sel);
}

/******************************************************************************
 * Set the currently displayed widget.
 */
void TimePeriod::setShowing(bool hourMinute)
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

void TimePeriod::setWhatsThis(const QString& unitText, const QString& hourMinText)
{
	QWhatsThis::add(mSpinBox, unitText);
	QWhatsThis::add(mTimeSpinBox, (hourMinText.isNull() ? unitText : hourMinText));
}

void TimePeriod::setUnitWhatsThis(const QString& text)
{
	QWhatsThis::add(mSpinBox, text);
}

void TimePeriod::setHourMinWhatsThis(const QString& text)
{
	QWhatsThis::add(mTimeSpinBox, text);
}

QSize TimePeriod::sizeHint() const
{
	return mSpinBox->sizeHint().expandedTo(mTimeSpinBox->sizeHint());
}

QSize TimePeriod::minimumSizeHint() const
{
	return mSpinBox->minimumSizeHint().expandedTo(mTimeSpinBox->minimumSizeHint());
}
