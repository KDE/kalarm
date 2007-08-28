/*
 *  alarmguiiface.h  -  DCOP interface which alarm daemon clients must implement
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  Copyright © 2001,2004,2006,2007 by David Jarvie <software@astrojar.org.uk>
 *  Based on the original, (c) 1998, 1999 Preston Brown
 *  Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
 *  Copyright (c) 1997-1999 Preston Brown <pbrown@kde.org>
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

#ifndef KALARM_ALARMGUIIFACE_H
#define KALARM_ALARMGUIIFACE_H

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

#endif // KALARM_ALARMGUIIFACE_H
