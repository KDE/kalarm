/*
 *  kalarmapp.h  -  the KAlarm application object
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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

#ifndef KALARMAPP_H
#define KALARMAPP_H

#include <qguardedptr.h>
class QTimer;
class QDateTime;

#include <kuniqueapplication.h>
#include <kurl.h>
class KAction;
class KActionCollection;
class KProcess;
namespace KCal { class Event; }

#include "alarmevent.h"
class DcopHandler;
class AlarmCalendar;
class KAlarmMainWindow;
class MessageWin;
class TrayWindow;
class DaemonGuiHandler;
class Preferences;
class ActionAlarmsEnabled;

extern const char* DAEMON_APP_NAME;
extern const char* DAEMON_DCOP_OBJECT;
extern const char* GUI_DCOP_OBJECT_NAME;


class KAlarmApp : public KUniqueApplication
{
		Q_OBJECT
	public:
		~KAlarmApp();
		virtual int        newInstance();
		static KAlarmApp*  getInstance();
		AlarmCalendar&     getCalendar()                   { return *mCalendar; }
		AlarmCalendar*     expiredCalendar(bool saveIfPurged = true);
		AlarmCalendar&     displayCalendar()               { return *mDisplayCalendar; }
		void               checkCalendar()                 { initCheck(); }
		bool               KDEDesktop() const              { return mKDEDesktop; }
		bool               wantRunInSystemTray() const;
		bool               alarmsDisabledIfStopped() const { return mDisableAlarmsIfStopped; }
		bool               noShellAccess() const           { return mNoShellAccess; }
		bool               restoreSession();
		bool               sessionClosingDown() const      { return mSessionClosingDown; }
		void               quitIf()                        { quitIf(0); }
		void               addWindow(TrayWindow* w)        { mTrayWindow = w; }
		void               removeWindow(TrayWindow*);
		KAlarmMainWindow*  displayMainWindowSelected(const QString& eventID = QString::null);
		TrayWindow*        trayWindow() const              { return mTrayWindow; }
		KAlarmMainWindow*  trayMainWindow() const;
		bool               displayTrayIcon(bool show, KAlarmMainWindow* = 0);
		bool               trayIconDisplayed() const       { return !!mTrayWindow; }
		DaemonGuiHandler*  daemonGuiHandler() const        { return mDaemonGuiHandler; }
		ActionAlarmsEnabled* actionAlarmEnable() const     { return mActionAlarmEnable; }
		KAction*           actionPreferences() const       { return mActionPrefs; }
		KAction*           actionDaemonControl() const     { return mActionDaemonControl; }
		KAction*           actionNewAlarm() const          { return mActionNewAlarm; }
		static KAction*    createNewAlarmAction(const QString& label, QObject* receiver, const char* slot, KActionCollection*);
		bool               editNewAlarm(KAlarmMainWindow* = 0);
		void               resetDaemon();
		bool               isDaemonRunning(bool startDaemon = true);
		void               readDaemonCheckInterval();
		QSize              readConfigWindowSize(const char* window, const QSize& defaultSize);
		void               writeConfigWindowSize(const char* window, const QSize&);
		virtual void       commitData(QSessionManager&);

		const KCal::Event* getEvent(const QString& eventID);
		bool               addEvent(const KAlarmEvent&, KAlarmMainWindow*, bool useEventID = false);
		void               modifyEvent(KAlarmEvent& oldEvent, const KAlarmEvent& newEvent, KAlarmMainWindow*);
		void               updateEvent(KAlarmEvent&, KAlarmMainWindow*, bool archiveOnDelete = true);
		void               deleteEvent(KAlarmEvent&, KAlarmMainWindow*, bool tellDaemon = true, bool archive = true);
		void               deleteDisplayEvent(const QString& eventID) const;
		void               undeleteEvent(KAlarmEvent&, KAlarmMainWindow*);
		void               archiveEvent(KAlarmEvent&);
		void               startCalendarUpdate();
		void               endCalendarUpdate();
		void               calendarSave(bool reload = true);
		void*              execAlarm(KAlarmEvent&, const KAlarmAlarm&, bool reschedule, bool allowDefer = true);
		void               alarmShowing(KAlarmEvent&, KAlarmAlarm::Type, const DateTime&);
		void               deleteEvent(const QString& eventID)         { handleEvent(eventID, EVENT_CANCEL); }
		void               commandMessage(KProcess*, QWidget* parent);
		int                maxLateness();
		// Methods called indirectly by the DCOP interface
		bool               scheduleEvent(const QString& text, const QDateTime&, const QColor& bg, const QColor& fg, const QFont&,
		                                 int flags, const QString& audioFile, const EmailAddressList& mailAddresses,
		                                 const QString& mailSubject, const QStringList& mailAttachments,
		                                 KAlarmEvent::Action, const KCal::Recurrence&, int reminderMinutes);
		void               handleEvent(const QString& calendarFile, const QString& eventID)    { handleEvent(calendarFile, eventID, EVENT_HANDLE); }
		void               triggerEvent(const QString& calendarFile, const QString& eventID)   { handleEvent(calendarFile, eventID, EVENT_TRIGGER); }
		void               deleteEvent(const QString& calendarFile, const QString& eventID)    { handleEvent(calendarFile, eventID, EVENT_CANCEL); }

		static int         fileType(const QString& mimetype);
	public slots:
		void               displayMainWindow()     { displayMainWindowSelected(); }
		void               slotDaemonControl();
	signals:
		void               trayIconToggled();
	protected:
		KAlarmApp();
	private slots:
		void               slotPreferences();
		void               toggleAlarmsEnabled();
		void               slotPreferencesChanged();
		void               slotNewAlarm();
		void               slotCommandExited(KProcess*);
		void               slotSystemTrayTimer();
		void               calendarSaved(AlarmCalendar*);
	private:
		enum EventFunc { EVENT_HANDLE, EVENT_TRIGGER, EVENT_CANCEL };
		struct ProcData
		{
			ProcData(KProcess* p, KAlarmEvent* e, KAlarmAlarm* a, QCString sh)
			          : process(p), event(e), alarm(a), shell(sh), messageBoxParent(0) { }
			~ProcData();
			KProcess*            process;
			KAlarmEvent*         event;
			KAlarmAlarm*         alarm;
			QCString             shell;
			QGuardedPtr<QWidget> messageBoxParent;
		};

		bool               initCheck(bool calendarOnly = false);
		void               quitIf(int exitCode, bool force = false);
		void               redisplayAlarms();
		bool               checkSystemTray();
		void               changeStartOfDay();
		void               setUpDcop();
		bool               stopDaemon();
		void               startDaemon();
		void               reloadDaemon();
		void               registerWithDaemon(bool reregister);
		void               handleEvent(const QString& calendarFile, const QString& eventID, EventFunc);
		bool               handleEvent(const QString& eventID, EventFunc);
		void               rescheduleAlarm(KAlarmEvent&, const KAlarmAlarm&, bool updateCalAndDisplay);
		void               cancelAlarm(KAlarmEvent&, KAlarmAlarm::Type, bool updateCalAndDisplay);

		static KAlarmApp*     theInstance;          // the one and only KAlarmApp instance
		static int            activeCount;          // number of active instances without main windows
		DcopHandler*          mDcopHandler;         // the parent of the main DCOP receiver object
		DaemonGuiHandler*     mDaemonGuiHandler;    // the parent of the system tray DCOP receiver object
		TrayWindow*           mTrayWindow;          // active system tray icon
		AlarmCalendar*        mCalendar;            // the calendar containing the active alarms
		AlarmCalendar*        mExpiredCalendar;     // the calendar containing closed alarms
		AlarmCalendar*        mDisplayCalendar;     // the calendar containing currently displaying alarms
		ActionAlarmsEnabled*  mActionAlarmEnable;   // action to enable/disable alarms
		KAction*              mActionPrefs;         // action to display the preferences dialog
		KAction*              mActionDaemonControl; // action to display the alarm daemon control dialog
		KAction*              mActionNewAlarm;      // action to display the alarm edit dialog to create a new alarm
		QDateTime             mLastDaemonCheck;     // last time daemon checked alarms before check interval change
		QDateTime             mNextDaemonCheck;     // next time daemon will check alarms after check interval change
		QTime                 mStartOfDay;          // start-of-day time currently in use
		QColor                mOldExpiredColour;    // expired alarms text colour
		int                   mOldExpiredKeepDays;  // how long expired alarms are being kept
		QPtrList<ProcData>    mCommandProcesses;    // currently active command alarm processes
		int                   mDaemonCheckInterval; // daemon check interval (seconds)
		int                   mCalendarUpdateCount; // nesting level of calendarUpdate calls
		bool                  mCalendarUpdateSave;  // save() was called while mCalendarUpdateCount > 0
		bool                  mCalendarUpdateReload;// reloadDaemon() was called while mCalendarUpdateCount > 0
		bool                  mDaemonRegistered;    // true if we've registered with alarm daemon
		bool                  mNoShellAccess;       // shell commands are not allowed (kiosk mode)
		bool                  mKDEDesktop;          // running on KDE desktop
		bool                  mNoSystemTray;        // no KDE system tray exists
		bool                  mSavedNoSystemTray;   // mNoSystemTray before mCheckingSystemTray was true
		bool                  mCheckingSystemTray;  // the existence of the system tray is being checked
		bool                  mDaemonRunning;       // whether the alarm daemon is currently running
		bool                  mSessionClosingDown;  // session manager is closing the application
		bool                  mOldRunInSystemTray;  // running continuously in system tray was selected
		bool                  mDisableAlarmsIfStopped; // disable alarms whenever KAlarm is not running
};

inline KAlarmApp* theApp()  { return KAlarmApp::getInstance(); }

#endif // KALARMAPP_H
