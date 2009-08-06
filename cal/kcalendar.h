/*
 *  kcalendar.h  -  kcal library calendar and event categorisation
 *  Program:  kalarm
 *  Copyright © 2006,2007 by David Jarvie <software@astrojar.org.uk>
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

#include "kalarm_cal_export.h"
#include <QString>

namespace KCal {
  class Event;
  class Alarm;
}

class KALARM_CAL_EXPORT KCalendar
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

class KALARM_CAL_EXPORT KCalEvent
{
	public:
		/** The category of an event, indicated by the middle part of its UID. */
		enum Status
		{
			EMPTY      = 0,       // the event has no alarms
			ACTIVE     = 0x01,    // the event is currently active
			ARCHIVED   = 0x02,    // the event is archived
			TEMPLATE   = 0x04,    // the event is an alarm template
			DISPLAYING = 0x08     // the event is currently being displayed
		};

		static QString uid(const QString& id, Status);
		static Status  status(const KCal::Event*, QString* param = 0);
		static void    setStatus(KCal::Event*, Status, const QString& param = QString());
};

#endif
