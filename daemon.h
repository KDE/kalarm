/*
 *  daemon.h  -  interface with alarm daemon
 *  Program:  kalarm
 *  (C) 2001 - 2004 by David Jarvie <software@astrojar.org.uk>
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

#ifndef DAEMON_H
#define DAEMON_H

#include <qobject.h>
#include <qdatetime.h>
#include <kaction.h>

#include <kalarmd/alarmguiiface.h>

class KAction;
class KActionCollection;
class AlarmCalendar;
class AlarmEnableAction;
class DaemonGuiHandler;


class Daemon : public QObject
{
		Q_OBJECT
	public:
		static void      initialise();
		static void      createDcopHandler();
		static bool      isDcopHandlerReady()    { return mDcopHandler; }
		static KAction*  createControlAction(KActionCollection*, const char* name);
		static AlarmEnableAction* createAlarmEnableAction(KActionCollection*, const char* name);
		static bool      start();
		static bool      reregister()            { return registerWith(true); }
		static bool      reset();
		static bool      stop();
		static void      setAlarmsEnabled()      { mInstance->setAlarmsEnabled(true); }
		static void      checkStatus()           { checkIfRunning(); }
		static bool      monitoringAlarms();
		static bool      isRunning(bool startDaemon = true);
		static int       maxTimeSinceCheck();
		static void      readCheckInterval();
		static bool      isRegistered()          { return mStatus == REGISTERED; }
		static void      allowRegisterFailMsg()  { mRegisterFailMsg = false; }

	signals:
		void             daemonRunning(bool running);

	private slots:
		void             slotControl();
		void             slotCalendarSaved(AlarmCalendar*);
		void             checkIfStarted();
		void             slotStarted()           { updateRegisteredStatus(true); }
		void             registerTimerExpired()  { registrationResult((mStatus == REGISTERED), AlarmGuiIface::FAILURE); }

		void             setAlarmsEnabled(bool enable);
		void             timerCheckIfRunning();
		void             slotPreferencesChanged();

	private:
		enum Status    // daemon status.  KEEP IN THIS ORDER!!
		{
			STOPPED,     // daemon is not registered with DCOP
			RUNNING,     // daemon is newly registered with DCOP
			READY,       // daemon is ready to accept DCOP calls
			REGISTERED   // we have registered with the daemon
		};
		explicit Daemon() { }
		static bool      registerWith(bool reregister);
		static void      registrationResult(bool reregister, int result);
		static void      reload();
		static void      updateRegisteredStatus(bool timeout = false);
		static void      enableCalendar(bool enable);
		static void      calendarIsEnabled(bool enabled);
		static bool      checkIfRunning();
		static void      setFastCheck();

		static Daemon*   mInstance;            // only one instance allowed
		static DaemonGuiHandler* mDcopHandler; // handles DCOP requests from daemon
		static QTimer*   mStartTimer;          // timer to check daemon status after starting daemon
		static QTimer*   mRegisterTimer;       // timer to check whether daemon has sent registration status
		static QTimer*   mStatusTimer;         // timer for checking daemon status
		static int       mStatusTimerCount;    // countdown for fast status checking
		static int       mStatusTimerInterval; // timer interval (seconds) for checking daemon status
		static QDateTime mLastCheck;           // last time daemon checked alarms before check interval change
		static QDateTime mNextCheck;           // next time daemon will check alarms after check interval change
		static int       mCheckInterval;       // daemon check interval (seconds)
		static int       mStartTimeout;        // remaining number of times to check if alarm daemon has started
		static Status    mStatus;              // daemon status
		static bool      mRunning;             // whether the alarm daemon is currently running
		static bool      mCalendarDisabled;    // monitoring of calendar is currently disabled by daemon
		static bool      mEnableCalPending;    // waiting to tell daemon to enable calendar
		static bool      mRegisterFailMsg;     // true if registration failure message has been displayed

		friend class DaemonGuiHandler;
};

/*=============================================================================
=  Class: AlarmEnableAction
=============================================================================*/

class AlarmEnableAction : public KToggleAction
{
		Q_OBJECT
	public:
		AlarmEnableAction(int accel, QObject* parent, const char* name = 0);
	public slots:
		void         setCheckedActual(bool);  // set state and emit switched() signal
		virtual void setChecked(bool);        // request state change and emit userClicked() signal
	signals:
		void         switched(bool);          // state has changed (KToggleAction::toggled() is only emitted when clicked by user)
		void         userClicked(bool);       // user has clicked the control (param = desired state)
	private:
		bool         mInitialised;
};

#endif // DAEMON_H
