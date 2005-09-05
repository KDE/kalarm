/*
 *  alarmdaemon.h  -  alarm daemon control routines
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  Copyright (c) 2001, 2004, 2005 by David Jarvie <software@astrojar.org.uk>
 *  Based on the original, (c) 1998, 1999 Preston Brown
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

#ifndef ALARMDAEMON_H
#define ALARMDAEMON_H

#include <libkcal/calendarlocal.h>

#include "alarmdaemoniface.h"

class ADCalendar;


class AlarmDaemon : public QObject, virtual public AlarmDaemonIface
{
		Q_OBJECT
	public:
		AlarmDaemon(QObject* parent = 0, const char* name = 0);

	private slots:
//#ifdef AUTOSTART_KALARM
		void    autostartKAlarm();
//#endif
		void    calendarLoaded(ADCalendar*, bool success);
		void    checkAlarmsSlot();
		void    checkAlarms();

	private:
		// DCOP interface
		void    enableAutoStart(bool enable);
		void    enableCalendar(const QString& urlString, bool enable)
		               { enableCal(expandURL(urlString), enable); }
		void    reloadCalendar(const QCString& appname, const QString& urlString)
		               { reloadCal(appname, expandURL(urlString), false); }
		void    resetCalendar(const QCString& appname, const QString& urlString)
		               { reloadCal(appname, expandURL(urlString), true); }
		void    registerApp(const QCString& appName, const QString& appTitle,
		                    const QCString& dcopObject, const QString& calendarUrl, bool startClient);
		void    registerChange(const QCString& appName, bool startClient);
		void    quit();
		// Other methods
		void    startMonitoring();
		void    enableCal(const QString& urlString, bool enable);
		void    reloadCal(const QCString& appname, const QString& urlString, bool reset);
		void    reloadCal(ADCalendar*, bool reset);
		void    checkAlarms(ADCalendar*);
		bool    notifyEvent(ADCalendar*, const QString& eventID);
		void    notifyCalStatus(const ADCalendar*);
		void    setTimerStatus();
		static QString expandURL(const QString& urlString);

		QTimer* mAlarmTimer;
		int     mAlarmTimerSyncCount; // countdown to re-synching the alarm timer
		bool    mAlarmTimerSyncing;   // true while alarm timer interval < 1 minute
};

#endif // ALARMDAEMON_H
