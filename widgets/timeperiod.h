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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
		explicit TimePeriod(QWidget* parent, const char* name = 0);
		virtual ~TimePeriod()  { }
		bool          isReadOnly() const             { return mReadOnly; }
		void          setReadOnly(bool);
		void          setUnitRange(int minValue, int maxValue);
		void          setHourMinRange(int minValue, int maxValue);
		void          setUnitSteps(int step, int shiftStep);
		void          setHourMinSteps(int minuteStep, int minuteShiftStep, int hourStep, int hourShiftStep);
		void          setSelectOnStep(bool sel);
		void          showUnit(bool show = true)     { setShowing(!show); }
		void          showHourMin(bool show = true)  { setShowing(show); }
		bool          showingUnit() const            { return !mHourMinute; }
		void          setValue(int count)            { setValue(mHourMinute, count); }
		void          setUnitValue(int count)        { setValue(false, count); }
		void          setHourMinValue(int minutes)   { setValue(true, minutes); }
		int           value() const                  { return value(mHourMinute); }
		int           unitValue() const              { return value(mHourMinute); }
		int           hourMinValue() const           { return value(mHourMinute); }
		void          setWhatsThis(const QString& unitText, const QString& hourMinText = QString::null);
		void          setUnitWhatsThis(const QString&);
		void          setHourMinWhatsThis(const QString&);
		virtual QSize minimumSizeHint() const;
		virtual QSize sizeHint() const;

	private:
		void          setShowing(bool hourMin);
		void          setValue(bool hourMin, int count);
		int           value(bool hourMin) const;

		SpinBox*      mSpinBox;
		TimeSpinBox*  mTimeSpinBox;
		bool          mReadOnly;
		bool          mHourMinute;
};

#endif // TIMEPERIOD_H
