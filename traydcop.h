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
#ifndef TRAYDCOPHANDLER_H
#define TRAYDCOPHANDLER_H

#include <kalarmd/alarmguiiface.h>


class TrayDcopHandler : public QObject, virtual public AlarmGuiIface
{
		Q_OBJECT
	public:
		explicit TrayDcopHandler(const char *name = 0L);

		static QString  expandURL(const QString& urlString);

	private:
		// DCOP interface
		void            alarmDaemonUpdate(int alarmGuiChangeType,
		                                  const QString& calendarURL, const QCString& appName);
		void            handleEvent(const QString& calendarURL, const QString& eventID);
		void            handleEvent(const QString& iCalendarString) ;
};

#endif // TRAYDCOPHANDLER_H
