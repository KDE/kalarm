/*
 *  traydcop.cpp  -  DCOP handler for the system tray window
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
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
 */

#include "kalarm.h"
#include <kurl.h>
#include <kdebug.h>

//#include <kalarmd/alarmdaemoniface.h>
#include "kalarmapp.h"
#include "alarmcalendar.h"
#include "traywindow.h"
#include "traydcop.h"
#include "traydcop.moc"


TrayDcopHandler::TrayDcopHandler(const char *name)
	: QObject(),
	  DCOPObject(name)
{
}

/*
 * DCOP call from the alarm daemon to notify a change.
 * The daemon notifies calendar statuses when we first register as a GUI, and whenever
 * a calendar status changes. So we don't need to read its config files.
 */
void TrayDcopHandler::alarmDaemonUpdate(int alarmGuiChangeType,
                                        const QString& calendarURL, const QCString& /*appName*/)
{
	kdDebug(5950) << "TrayDcopHandler::alarmDaemonUpdate(" << alarmGuiChangeType << ")\n";
	TrayWindow* trayWin = theApp()->trayWindow();
	if (!trayWin)
		return;
	AlarmGuiChangeType changeType = AlarmGuiChangeType(alarmGuiChangeType);
	switch (changeType)
	{
		case CHANGE_STATUS:    // daemon autostart status change
		case CHANGE_CLIENT:    // change to daemon's client application list
			return;
		default:
		{
			// It must be a calendar-related change
			if (expandURL(calendarURL) != theApp()->getCalendar().urlString())
				return;     // it's not a notification about KAlarm's calendar
			bool monitoring = false;
			switch (changeType)
			{
				case DELETE_CALENDAR:
					kdDebug(5950) << "TrayDcopHandler::alarmDaemonUpdate(DELETE_CALENDAR)\n";
					break;
				case CALENDAR_UNAVAILABLE:
					// Calendar is not available for monitoring
					kdDebug(5950) << "TrayDcopHandler::alarmDaemonUpdate(CALENDAR_UNAVAILABLE)\n";
					break;
				case DISABLE_CALENDAR:
					// Calendar is available for monitoring but is not currently being monitored
					kdDebug(5950) << "TrayDcopHandler::alarmDaemonUpdate(DISABLE_CALENDAR)\n";
					break;
				case ENABLE_CALENDAR:
					// Calendar is currently being monitored
					kdDebug(5950) << "TrayDcopHandler::alarmDaemonUpdate(ENABLE_CALENDAR)\n";
					monitoring = true;
					break;
				case ADD_CALENDAR:        // add a KOrganizer-type calendar
				case ADD_MSG_CALENDAR:    // add a KAlarm-type calendar
				default:
					return;
			}
			trayWin->updateCalendarStatus(monitoring);
			break;
		}
	}
}

// Dummy handler functions which KAlarm doesn't use
void TrayDcopHandler::handleEvent(const QString&, const QString&)
{ }
void TrayDcopHandler::handleEvent(const QString&)
{ }
/*
 * Expand a DCOP call parameter URL to a full URL.
 * (We must store full URLs in the calendar data since otherwise
 *  later calls to reload or remove calendars won't necessarily
 *  find a match.)
 */
QString TrayDcopHandler::expandURL(const QString& urlString)
{
	if (urlString.isEmpty())
		return QString();
	return KURL(urlString).url();
}
