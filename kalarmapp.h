/*
 *  kalarmapp.h  -  the KAlarm application object
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
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
 */

#ifndef KALARMAPP_H
#define KALARMAPP_H

#include <qguardedptr.h>
class QTimer;
class QDateTime;

#include <kuniqueapplication.h>
#include <kurl.h>
class KProcess;
namespace KCal { class Event; }

#include "alarmevent.h"
class DcopHandler;
class AlarmCalendar;
class KAlarmMainWindow;
class AlarmListView;
class MessageWin;
class TrayWindow;
class DaemonGuiHandler;
class Preferences;
class ShellProcess;


class KAlarmApp : public KUniqueApplication
{
		Q_OBJECT
	public:
		~KAlarmApp();
		virtual int        newInstance();
		static KAlarmApp*  getInstance();
		bool               checkCalendarDaemon()           { return initCheck(); }
		bool               KDEDesktop() const              { return mKDEDesktop; }
		bool               wantRunInSystemTray() const;
		bool               alarmsDisabledIfStopped() const { return mDisableAlarmsIfStopped; }
		bool               restoreSession();
		bool               sessionClosingDown() const      { return mSessionClosingDown; }
		void               quitIf()                        { quitIf(0); }
		void               doQuit(QWidget* parent);
		static void        displayFatalError(const QString& message);
		void               addWindow(TrayWindow* w)        { mTrayWindow = w; }
		void               removeWindow(TrayWindow*);
		TrayWindow*        trayWindow() const              { return mTrayWindow; }
		KAlarmMainWindow*  trayMainWindow() const;
		bool               displayTrayIcon(bool show, KAlarmMainWindow* = 0);
		bool               trayIconDisplayed() const       { return !!mTrayWindow; }
		DaemonGuiHandler*  daemonGuiHandler() const        { return mDaemonGuiHandler; }
		bool               editNewAlarm(KAlarmMainWindow* = 0);
		virtual void       commitData(QSessionManager&);

		void*              execAlarm(KAEvent&, const KAAlarm&, bool reschedule, bool allowDefer = true, bool noPreAction = false);
		void               alarmShowing(KAEvent&, KAAlarm::Type, const DateTime&);
		void               alarmCompleted(const KAEvent&);
		bool               deleteEvent(const QString& eventID)         { return handleEvent(eventID, EVENT_CANCEL); }
		void               commandMessage(ShellProcess*, QWidget* parent);
		// Methods called indirectly by the DCOP interface
		bool               scheduleEvent(KAEvent::Action, const QString& text, const QDateTime&,
		                                 int flags, const QColor& bg, const QColor& fg,
		                                 const QFont&, const QString& audioFile, float audioVolume,
		                                 int reminderMinutes, const KCal::Recurrence& recurrence,
		                                 const EmailAddressList& mailAddresses = EmailAddressList(),
		                                 const QString& mailSubject = QString::null,
		                                 const QStringList& mailAttachments = QStringList());
		bool               handleEvent(const QString& calendarFile, const QString& eventID)    { return handleEvent(calendarFile, eventID, EVENT_HANDLE); }
		bool               triggerEvent(const QString& calendarFile, const QString& eventID)   { return handleEvent(calendarFile, eventID, EVENT_TRIGGER); }
		bool               deleteEvent(const QString& calendarFile, const QString& eventID)    { return handleEvent(calendarFile, eventID, EVENT_CANCEL); }
	public slots:
		void               processQueue();
	signals:
		void               trayIconToggled();
	protected:
		KAlarmApp();
	private slots:
		void               quitFatal();
		void               slotPreferencesChanged();
		void               slotCommandExited(ShellProcess*);
		void               slotSystemTrayTimer();
		void               slotExpiredPurged();
	private:
		enum EventFunc { EVENT_HANDLE, EVENT_TRIGGER, EVENT_CANCEL };
		struct ProcData
		{
			ProcData(ShellProcess* p, KAEvent* e, KAAlarm* a, int f = 0)
			          : process(p), event(e), alarm(a), messageBoxParent(0), flags(f) { }
			~ProcData();
			enum { PRE_ACTION = 0x01, POST_ACTION = 0x02, RESCHEDULE = 0x04, ALLOW_DEFER = 0x08 };
			bool                 preAction() const   { return flags & PRE_ACTION; }
			bool                 postAction() const  { return flags & POST_ACTION; }
			bool                 reschedule() const  { return flags & RESCHEDULE; }
			bool                 allowDefer() const  { return flags & ALLOW_DEFER; }
			ShellProcess*        process;
			KAEvent*             event;
			KAAlarm*             alarm;
			QGuardedPtr<QWidget> messageBoxParent;
			int                  flags;
		};
		struct DcopQEntry
		{
			DcopQEntry(EventFunc f, const QString& id) : function(f), eventId(id) { }
			DcopQEntry(const KAEvent& e, EventFunc f = EVENT_HANDLE) : function(f), event(e) { }
			DcopQEntry() { }
			EventFunc  function;
			QString    eventId;
			KAEvent    event;
		};

		bool               initCheck(bool calendarOnly = false);
		void               quitIf(int exitCode, bool force = false);
		void               redisplayAlarms();
		bool               checkSystemTray();
		void               changeStartOfDay();
		void               setUpDcop();
		bool               handleEvent(const QString& calendarFile, const QString& eventID, EventFunc);
		bool               handleEvent(const QString& eventID, EventFunc);
		void               rescheduleAlarm(KAEvent&, const KAAlarm&, bool updateCalAndDisplay);
		void               cancelAlarm(KAEvent&, KAAlarm::Type, bool updateCalAndDisplay);
		ShellProcess*      doShellCommand(const QString& command, const KAEvent&, const KAAlarm*, int flags = 0);
		void               commandErrorMsg(const ShellProcess*, const KAEvent&, const KAAlarm*, int flags = 0);

		static KAlarmApp*     theInstance;          // the one and only KAlarmApp instance
		static int            mActiveCount;         // number of active instances without main windows
		static int            mFatalError;          // a fatal error has occurred - just wait to exit
		static QString        mFatalMessage;        // fatal error message to output
		bool                  mInitialised;         // initialisation complete: ready to handle DCOP calls
		DcopHandler*          mDcopHandler;         // the parent of the main DCOP receiver object
		DaemonGuiHandler*     mDaemonGuiHandler;    // the parent of the system tray DCOP receiver object
		TrayWindow*           mTrayWindow;          // active system tray icon
		QTime                 mStartOfDay;          // start-of-day time currently in use
		QColor                mPrefsExpiredColour;  // expired alarms text colour
		int                   mPrefsExpiredKeepDays;// how long expired alarms are being kept
		QPtrList<ProcData>    mCommandProcesses;    // currently active command alarm processes
		QValueList<DcopQEntry> mDcopQueue;          // DCOP command queue
		int                   mPendingQuitCode;     // exit code for a pending quit
		bool                  mPendingQuit;         // quit once the DCOP command and shell command queues have been processed
		bool                  mProcessingQueue;     // a mDcopQueue entry is currently being processed
		bool                  mKDEDesktop;          // running on KDE desktop
		bool                  mNoSystemTray;        // no KDE system tray exists
		bool                  mSavedNoSystemTray;   // mNoSystemTray before mCheckingSystemTray was true
		bool                  mCheckingSystemTray;  // the existence of the system tray is being checked
		bool                  mSessionClosingDown;  // session manager is closing the application
		bool                  mOldRunInSystemTray;  // running continuously in system tray was selected
		bool                  mDisableAlarmsIfStopped; // disable alarms whenever KAlarm is not running
		bool                  mRefreshExpiredAlarms; // need to refresh the expired alarms display
		bool                  mPrefsShowTime;       // Preferences setting for show alarm times in alarm list
		bool                  mPrefsShowTimeTo;     // Preferences setting for show time-to-alarms in alarm list
};

inline KAlarmApp* theApp()  { return KAlarmApp::getInstance(); }

#endif // KALARMAPP_H
