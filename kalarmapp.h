/*
 *  kalarmapp.h  -  the KAlarm application object
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <software@astrojar.org.uk>
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

#ifndef KALARMAPP_H
#define KALARMAPP_H

/** @file kalarmapp.h - the KAlarm application object */

#include <QPointer>
#include <QQueue>
#include <QList>
class KDateTime;

#include <kuniqueapplication.h>
#include <kurl.h>
class K3Process;
namespace KCal { class Event; }

#include "alarmevent.h"
class DBusHandler;
class MainWindow;
class TrayWindow;
class ShellProcess;


class KAlarmApp : public KUniqueApplication
{
		Q_OBJECT
	public:
		~KAlarmApp();
		virtual int        newInstance();
		static KAlarmApp*  getInstance();
		bool               checkCalendarDaemon()           { return initCheck(); }
		bool               wantRunInSystemTray() const;
		bool               alarmsDisabledIfStopped() const { return mDisableAlarmsIfStopped; }
		bool               speechEnabled() const           { return mSpeechEnabled; }
		bool               korganizerEnabled() const       { return mKOrganizerEnabled; }
		bool               restoreSession();
		bool               sessionClosingDown() const      { return mSessionClosingDown; }
		void               quitIf()                        { quitIf(0); }
		void               doQuit(QWidget* parent);
		static void        displayFatalError(const QString& message);
		void               addWindow(TrayWindow* w)        { mTrayWindow = w; }
		void               removeWindow(TrayWindow*);
		TrayWindow*        trayWindow() const              { return mTrayWindow; }
		MainWindow*        trayMainWindow() const;
		bool               displayTrayIcon(bool show, MainWindow* = 0);
		bool               trayIconDisplayed() const       { return mTrayWindow; }
		bool               editNewAlarm(MainWindow* = 0);
		virtual void       commitData(QSessionManager&);

		void*              execAlarm(KAEvent&, const KAAlarm&, bool reschedule, bool allowDefer = true, bool noPreAction = false);
		void               alarmCompleted(const KAEvent&);
		void               rescheduleAlarm(KAEvent& e, const KAAlarm& a)   { rescheduleAlarm(e, a, true); }
		bool               deleteEvent(const QString& eventID)         { return handleEvent(eventID, EVENT_CANCEL); }
		void               purgeAll()             { purge(0); }
		void               commandMessage(ShellProcess*, QWidget* parent);
		// Methods called indirectly by the DCOP interface
		bool               scheduleEvent(KAEvent::Action, const QString& text, const KDateTime&,
		                                 int lateCancel, int flags, const QColor& bg, const QColor& fg,
		                                 const QFont&, const QString& audioFile, float audioVolume,
		                                 int reminderMinutes, const KARecurrence& recurrence,
						 int repeatInterval, int repeatCount,
		                                 const QString& mailFromID = QString(),
		                                 const EmailAddressList& mailAddresses = EmailAddressList(),
		                                 const QString& mailSubject = QString(),
		                                 const QStringList& mailAttachments = QStringList());
		bool               dbusHandleEvent(const QString& eventID)    { return dbusHandleEvent(eventID, EVENT_HANDLE); }
		bool               dbusTriggerEvent(const QString& eventID)   { return dbusHandleEvent(eventID, EVENT_TRIGGER); }
		bool               dbusDeleteEvent(const QString& eventID)    { return dbusHandleEvent(eventID, EVENT_CANCEL); }
	public slots:
		void               processQueue();
	signals:
		void               trayIconToggled();
	protected:
		KAlarmApp();
	private slots:
		void               quitFatal();
		void               slotPreferencesChanged();
		void               setArchivePurgeDays();
		void               slotPurge()                     { purge(mArchivedPurgeDays); }
		void               slotCommandOutput(K3Process*, char* buffer, int bufflen);
		void               slotLogProcExited(ShellProcess*);
		void               slotCommandExited(ShellProcess*);
	private:
		enum EventFunc
		{
			EVENT_HANDLE,    // if the alarm is due, execute it and then reschedule it
			EVENT_TRIGGER,   // execute the alarm regardless, and then reschedule it if it already due
			EVENT_CANCEL     // delete the alarm
		};
		struct ProcData
		{
			ProcData(ShellProcess*, ShellProcess* logprocess, KAEvent*, KAAlarm*, int flags = 0);
			~ProcData();
			enum { PRE_ACTION = 0x01, POST_ACTION = 0x02, RESCHEDULE = 0x04, ALLOW_DEFER = 0x08,
			       TEMP_FILE = 0x10, EXEC_IN_XTERM = 0x20 };
			bool                 preAction() const   { return flags & PRE_ACTION; }
			bool                 postAction() const  { return flags & POST_ACTION; }
			bool                 reschedule() const  { return flags & RESCHEDULE; }
			bool                 allowDefer() const  { return flags & ALLOW_DEFER; }
			bool                 tempFile() const    { return flags & TEMP_FILE; }
			bool                 execInXterm() const { return flags & EXEC_IN_XTERM; }
			ShellProcess*          process;
			QPointer<ShellProcess> logProcess;
			KAEvent*               event;
			KAAlarm*               alarm;
			QPointer<QWidget>      messageBoxParent;
			QStringList            tempFiles;
			int                    flags;
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
		bool               checkSystemTray();
		void               changeStartOfDay();
		void               setUpDcop();
		bool               dbusHandleEvent(const QString& eventID, EventFunc);
		bool               handleEvent(const QString& eventID, EventFunc);
		void               rescheduleAlarm(KAEvent&, const KAAlarm&, bool updateCalAndDisplay);
		void               cancelAlarm(KAEvent&, KAAlarm::Type, bool updateCalAndDisplay);
		ShellProcess*      doShellCommand(const QString& command, const KAEvent&, const KAAlarm*, int flags = 0);
		QString            createTempScriptFile(const QString& command, bool insertShell, const KAEvent&, const KAAlarm&);
		void               commandErrorMsg(const ShellProcess*, const KAEvent&, const KAAlarm*, int flags = 0);
		void               purge(int daysToKeep);

		static KAlarmApp*  theInstance;          // the one and only KAlarmApp instance
		static int         mActiveCount;         // number of active instances without main windows
		static int         mFatalError;          // a fatal error has occurred - just wait to exit
		static QString     mFatalMessage;        // fatal error message to output
		bool               mInitialised;         // initialisation complete: ready to handle DCOP calls
		DBusHandler*       mDBusHandler;         // the parent of the main DCOP receiver object
		TrayWindow*        mTrayWindow;          // active system tray icon
		QTime              mStartOfDay;          // start-of-day time currently in use
		QColor             mPrefsArchivedColour; // archived alarms text colour
		int                mArchivedPurgeDays;   // how long to keep archived alarms, 0 = don't keep, -1 = keep indefinitely
		int                mPurgeDaysQueued;     // >= 0 to purge the archive calendar from KAlarmApp::processLoop()
		QList<ProcData*>   mCommandProcesses;    // currently active command alarm processes
		QQueue<DcopQEntry> mDcopQueue;           // DCOP command queue
		int                mPendingQuitCode;     // exit code for a pending quit
		bool               mPendingQuit;         // quit once the DCOP command and shell command queues have been processed
		bool               mProcessingQueue;     // a mDcopQueue entry is currently being processed
		bool               mNoSystemTray;        // no system tray exists
		bool               mSessionClosingDown;  // session manager is closing the application
		bool               mOldRunInSystemTray;  // running continuously in system tray was selected
		bool               mDisableAlarmsIfStopped; // disable alarms whenever KAlarm is not running
		bool               mSpeechEnabled;       // speech synthesis is enabled (kttsd exists)
		bool               mKOrganizerEnabled;   // KOrganizer options are enabled (korganizer exists)
		bool               mPrefsShowTime;       // Preferences setting for show alarm times in alarm list
		bool               mPrefsShowTimeTo;     // Preferences setting for show time-to-alarms in alarm list
};

inline KAlarmApp* theApp()  { return KAlarmApp::getInstance(); }

#endif // KALARMAPP_H
