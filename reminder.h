/*
 *  reminder.h  -  reminder setting widget
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

		static const QString i18n_hours_mins;  // text of 'hours/minutes' units, lower case
		static const QString i18n_Hours_Mins;  // text of 'Hours/Minutes' units, initial capitals
		static const QString i18n_days;        // text of 'days' units, lower case
		static const QString i18n_Days;        // text of 'Days' units, initial capital
		static const QString i18n_weeks;       // text of 'weeks' units, lower case
		static const QString i18n_Weeks;       // text of 'Weeks' units, initial capital

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
