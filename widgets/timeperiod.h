/*
 *  timeperiod.cpp  -  time period data entry widget
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

#ifndef TIMEPERIOD_H
#define TIMEPERIOD_H

#include <qwidgetstack.h>

class QString;
class SpinBox;
class TimeSpinBox;


class TimePeriod : public QWidgetStack
{
		Q_OBJECT
	public:
		TimePeriod(bool readOnly, QWidget* parent, const char* name = 0);
		virtual ~TimePeriod()  { }
		void          setRange(bool hourMinute, int minValue, int maxValue);
		void          setCountSteps(int step, int shiftStep);
		void          setHourMinuteSteps(int minuteStep, int minuteShiftStep, int hourStep, int hourShiftStep);
		void          setSelectOnStep(bool sel);
		void          setCurrent(bool hourMinute);
		bool          current() const              { return mHourMinute; }
		void          setValue(int count)          { setValue(mHourMinute, count); }
		void          setValue(bool hourMinute, int count);
		int           value() const                { return value(mHourMinute); }
		int           value(bool hourMinute) const;
		void          setWhatsThis(bool hourMinute, const QString& text);
		virtual QSize minimumSizeHint() const;
		virtual QSize sizeHint() const;

	private:
		SpinBox*       mSpinBox;
		TimeSpinBox*   mTimeSpinBox;
		bool           mReadOnly;
		bool           mHourMinute;
};

#endif // TIMEPERIOD_H
