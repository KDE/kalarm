/*
 *  daemongui.h  -  handler for the alarm daemon GUI interface
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie  software@astrojar.org.uk
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
 */
#ifndef DAEMONGUIHANDLER_H
#define DAEMONGUIHANDLER_H

#include <qtimer.h>
#include <kaction.h>

#include <kalarmd/alarmguiiface.h>
class ActionAlarmsEnabled;


/*=============================================================================
=  Class: DaemonGuiHandler
=  Handles the the alarm daemon's GUI client DCOP interface,
=  and keeps track of the alarm daemon's alarm monitoring status.
=============================================================================*/

class DaemonGuiHandler : public QObject, virtual public AlarmGuiIface
{
		Q_OBJECT
	public:
		explicit DaemonGuiHandler(const char *name = 0);

		bool                 monitoringAlarms();
		void                 checkStatus()                 { checkIfDaemonRunning(); }
		void                 setAlarmsEnabled(bool enable);

	private slots:
		void                 timerCheckDaemonRunning();
		void                 slotPreferencesChanged();

	private:
		static QString       expandURL(const QString& urlString);
		void                 registerGuiWithDaemon();
		void                 daemonEnableCalendar(bool enable);
		bool                 checkIfDaemonRunning();
		void                 setFastDaemonCheck();
		// DCOP interface
		void                 alarmDaemonUpdate(int alarmGuiChangeType,
		                                       const QString& calendarURL, const QCString& appName);
		void                 handleEvent(const QString& calendarURL, const QString& eventID);
		void                 handleEvent(const QString& iCalendarString) ;

		QTimer               mDaemonStatusTimer;         // timer for checking daemon status
		int                  mDaemonStatusTimerCount;    // countdown for fast status checking
		int                  mDaemonStatusTimerInterval; // timer interval (seconds) for checking daemon status
		bool                 mDaemonRunning;             // whether the alarm daemon is currently running
		bool                 mCalendarDisabled;          // monitoring of calendar is currently disabled by daemon
		bool                 mEnableCalPending;          // waiting to tell daemon to enable calendar
};

/*=============================================================================
=  Class: ActionAlarmsEnabled
=============================================================================*/

class ActionAlarmsEnabled : public KAction
{
		Q_OBJECT
	public:
		ActionAlarmsEnabled(int accel, const QObject* receiver, const char* slot, QObject* parent, const char* name = 0);
		bool    alarmsEnabled() const           { return mAlarmsEnabled; }
		void    setAlarmsEnabled(bool enable);
	signals:
		void    alarmsEnabledChange(bool enabled);
	private:
		bool    mAlarmsEnabled;
};

#endif // DAEMONGUIHANDLER_H
