/*
 *  reminder.h  -  reminder setting widget
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#ifndef REMINDER_H
#define REMINDER_H

#include <qframe.h>

class QLabel;
class CheckBox;
class ComboBox;
class TimePeriod;


class Reminder : public QFrame
{
		Q_OBJECT
	public:
		enum Units { HOURS_MINUTES, DAYS, WEEKS };

		Reminder(const QString& caption, const QString& reminderWhatsThis, const QString& valueWhatsThis,
		         bool allowHourMinute, QWidget* parent, const char* name = 0);
		bool         isReminder() const;
		int          getMinutes() const;
		void         setMinutes(int minutes, bool dateOnly);
		void         setReadOnly(bool);
		Units        setDateOnly(bool dateOnly)   { return setDateOnly(getMinutes(), dateOnly); }
		void         setMaximum(int hourmin, int days);
		void         setFocusOnCount();

	protected slots:
		void         slotReminderToggled(bool);
		void         slotUnitsSelected(int index);

	private:
		Units        setDateOnly(int minutes, bool dateOnly);
		void         setUnitRange();

		CheckBox*    mReminder;
		TimePeriod*  mCount;
		ComboBox*    mUnitsCombo;
		QLabel*      mLabel;
		int          mMaxDays;            // maximum day count
		bool         mNoHourMinute;       // hours/minutes cannot be displayed, ever
		bool         mReadOnly;           // the widget is read only
		int          mDateOnlyOffset;     // 1 if hours/minutes is disabled, else 0
};

#endif // REMINDER_H
