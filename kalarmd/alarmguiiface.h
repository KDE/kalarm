/*
 *  alarmguiiface.h  -  DCOP interface which alarm daemon clients must implement
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  (C) 2001, 2004 by David Jarvie <software@astrojar.org.uk>
 *  Based on the original, (c) 1998, 1999 Preston Brown
 *  Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
 *  Copyright (c) 1997-1999 Preston Brown
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef DAEMONGUIIFACE_H
#define DAEMONGUIIFACE_H

#include <dcopobject.h>

namespace KAlarmd
{
	enum RegisterResult   // result code of registerApp() DCOP call
	{
		FAILURE   = 0,
		SUCCESS   = 1,
		NOT_FOUND = 2    // notification type requires client start, but client executable not found
	};

	enum CalendarStatus    // parameters to client notification
	{
		CALENDAR_ENABLED,        // calendar is now being monitored
		CALENDAR_DISABLED,       // calendar is available but not being monitored
		CALENDAR_UNAVAILABLE     // calendar is unavailable for monitoring
	};
}

/*=============================================================================
=  Class: AlarmGuiIface
=  Client applications should inherit from this class to receive notifications
*  from the alarm daemon.
=============================================================================*/
class AlarmGuiIface : virtual public DCOPObject
{
		K_DCOP
	k_dcop:
		/** Called to notify a change in status of the calendar.
		    @param calendarStatus new calendar status. Value is of type CalendarStatus.
		 */
		virtual ASYNC alarmDaemonUpdate(int calendarStatus, const QString& calendarURL) = 0;

		/** Called to notify that an alarm is due.
		 */
		virtual ASYNC handleEvent(const QString& calendarURL, const QString& eventID) = 0;

		/** Called to indicate success/failure of (re)register() call.
		    @param result success/failure code. Value is of type RegisterResult.
		 */
		virtual ASYNC registered(bool reregister, int result) = 0;
};

#endif
