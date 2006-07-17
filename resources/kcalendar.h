/*
 *  kcalendar.h  -  kcal library calendar and event categorisation
 *  Program:  kalarm
 *  Copyright © 2006 by David Jarvie <software@astrojar.org.uk>
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

#ifndef KCALENDAR_H
#define KCALENDAR_H

namespace KCal {
  class Event;
  class Alarm;
}

class KDE_EXPORT KCalendar
{
	public:
		static QByteArray APPNAME;
		/** Compatibility of resource calendar format. */
		enum Status {
			Current,       // in current KAlarm format
			Converted,     // in current KAlarm format, but not yet saved
			Convertible,   // in an older KAlarm format
			Incompatible,  // not written by KAlarm, or in a newer KAlarm version
			ByEvent        // individual events have their own compatibility status
		};
};

class KDE_EXPORT KCalEvent
{
	public:
		/** The category of an event, indicated by the middle part of its UID. */
		enum Status
		{
			EMPTY,       // the event has no alarms
			ACTIVE,      // the event is currently active
			ARCHIVED,    // the event is archived
			DISPLAYING,  // the event is currently being displayed
			TEMPLATE     // the event is an alarm template
		};

		static QString uid(const QString& id, Status);
		static Status  status(const KCal::Event*, QString* param = 0);
		static void    setStatus(KCal::Event*, Status, const QString& param = QString());
};
		
#endif
