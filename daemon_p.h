/*
 *  daemon_p.h  -  alarm daemon notification handler
 *  Program:  kalarm
 *  Copyright © 2007 by David Jarvie <software@astrojar.org.uk>
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

#ifndef DAEMON_P_H
#define DAEMON_P_H

#include "kalarm.h"
#include <QObject>


/*=============================================================================
=  Class: NotificationHandler
=  Handles the the alarm daemon's client notification D-Bus interface.
=============================================================================*/

class NotificationHandler : public QObject
{
		Q_OBJECT
	public:
		NotificationHandler();
		// D-Bus interface
		void alarmDaemonUpdate(int calendarStatus);
		void handleEvent(const QString& eventID);
		void registered(bool reregister, int result);
		void cacheDownloaded(const QString& resourceID);
};

#endif
