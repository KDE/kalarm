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

class TimeSelector;
class CheckBox;


class Reminder : public QFrame
{
		Q_OBJECT
	public:
		Reminder(const QString& caption, const QString& reminderWhatsThis, const QString& valueWhatsThis,
		         bool allowHourMinute, bool showOnceOnly, QWidget* parent, const char* name = 0);
		bool           isReminder() const;
		bool           isOnceOnly() const;
		int            getMinutes() const;
		void           setMinutes(int minutes, bool dateOnly);
		void           setReadOnly(bool);
		void           setDateOnly(bool dateOnly);
		void           setMaximum(int hourmin, int days);
		void           setFocusOnCount();
		void           setOnceOnly(bool);
		void           enableOnceOnly(bool enable);

		static QString i18n_first_recurrence_only();    // plain text of 'Reminder for first recurrence only' checkbox
		static QString i18n_u_first_recurrence_only();  // text of 'Reminder for first recurrence only' checkbox, with 'u' shortcut

	protected slots:
		void           slotReminderToggled(bool);

	private:
		TimeSelector*  mTime;
		CheckBox*      mOnceOnly;
		bool           mReadOnly;           // the widget is read only
		bool           mOnceOnlyEnabled;    // 'mOnceOnly' checkbox is allowed to be enabled
};

#endif // REMINDER_H
