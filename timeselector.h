/*
 *  timeselector.h  -  widget to optionally set a time period
 *  Program:  kalarm
 *  Copyright (C) 2004 by David Jarvie <software@astrojar.org.uk>
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

#ifndef TIMESELECTOR_H
#define TIMESELECTOR_H

#include <qframe.h>
#include "timeperiod.h"

class QLabel;
class CheckBox;


class TimeSelector : public QFrame
{
		Q_OBJECT
	public:
		TimeSelector(const QString& selectText, const QString& postfix, const QString& selectWhatsThis,
		             const QString& valueWhatsThis, bool allowHourMinute, QWidget* parent, const char* name = 0);
		bool         isChecked() const;
		void         setChecked(bool on);
		int          minutes() const;
		void         setMinutes(int minutes, bool dateOnly, TimePeriod::Units defaultUnits);
		void         setReadOnly(bool);
		void         setDateOnly(bool dateOnly = true);
		void         setMaximum(int hourmin, int days);
		void         setFocusOnCount();

	signals:
		void         toggled(bool);             // selection checkbox has been toggled
		void         valueChanged(int minutes); // value has changed

	protected slots:
		void         selectToggled(bool);
		void         periodChanged(int minutes);

	private:
		CheckBox*    mSelect;
		TimePeriod*  mPeriod;
		QLabel*      mLabel;
		bool         mReadOnly;           // the widget is read only
};

#endif // TIMESELECTOR_H
