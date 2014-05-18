/*
 *  kalarmapp.h  -  the KAlarm application object
 *  Program:  kalarm
 *  Copyright © 2001-2014 by David Jarvie <djarvie@kde.org>
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

#include "eventid.h"
#include "kamail.h"
#include "preferences.h"

#include <kalarmcal/kaevent.h>

#include <kuniqueapplication.h>

#include <QPointer>
#include <QQueue>
#include <QList>

class KDateTime;
namespace KCal { class Event; }
namespace Akonadi { class Collection; }
class DBusHandler;
class MainWindow;
class TrayWindow;
class ShellProcess;
class OrgKdeKSpeechInterface;

using namespace KAlarmCal;


class KAlarmApp : public KUniqueApplication
{
        Q_OBJECT
    public:
        ~KAlarmApp();
        virtual int        newInstance();
        static KAlarmApp*  getInstance();
        bool               checkCalendar()                 { return initCheck(); }
        bool               wantShowInSystemTray() const;
        bool               alarmsEnabled() const           { return mAlarmsEnabled; }
        bool               speechEnabled() const           { return mSpeechEnabled; }
        OrgKdeKSpeechInterface* kspeechInterface(QString& error) const;
        bool               korganizerEnabled() const       { return mKOrganizerEnabled; }
        bool               restoreSession();
        bool               sessionClosingDown() const      { return mSessionClosingDown; }
        bool               quitIf()                        { return quitIf(0); }
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
        ShellProcess*      execCommandAlarm(const KAEvent&, const KAAlarm&, const QObject* receiver = 0, const char* slot = 0);
        void               alarmCompleted(const KAEvent&);
        void               rescheduleAlarm(KAEvent& e, const KAAlarm& a)   { rescheduleAlarm(e, a, true); }
        void               purgeAll()             { purge(0); }
        void               commandMessage(ShellProcess*, QWidget* parent);
        void               notifyAudioPlaying(bool playing);
        void               setSpreadWindowsState(bool spread);
        // Methods called indirectly by the DCOP interface
        bool               scheduleEvent(KAEvent::SubAction, const QString& text, const KDateTime&,
                                         int lateCancel, KAEvent::Flags flags, const QColor& bg, const QColor& fg,
                                         const QFont&, const QString& audioFile, float audioVolume,
                                         int reminderMinutes, const KARecurrence& recurrence,
                                         int repeatInterval, int repeatCount,
                                         uint mailFromID = 0, const KCalCore::Person::List& mailAddresses = KCalCore::Person::List(),
                                         const QString& mailSubject = QString(),
                                         const QStringList& mailAttachments = QStringList());
        bool               dbusTriggerEvent(const EventId& eventID)   { return dbusHandleEvent(eventID, EVENT_TRIGGER); }
        bool               dbusDeleteEvent(const EventId& eventID)    { return dbusHandleEvent(eventID, EVENT_CANCEL); }
        QString            dbusList();

    public slots:
        void               processQueue();
        void               setAlarmsEnabled(bool);
        void               purgeNewArchivedDefault(const Akonadi::Collection&);
        void               atLoginEventAdded(const KAEvent&);
        void               notifyAudioStopped()  { notifyAudioPlaying(false); }
        void               stopAudio();
        void               spreadWindows(bool);
        void               emailSent(KAMail::JobData&, const QStringList& errmsgs, bool copyerr = false);

    signals:
        void               trayIconToggled();
        void               alarmEnabledToggled(bool);
        void               audioPlaying(bool);
        void               spreadWindowsToggled(bool);
        void               execAlarmSuccess();

    protected:
        KAlarmApp();

    private:
        typedef Preferences::Feb29Type Feb29Type;   // allow it to be used in SIGNAL mechanism

    private slots:
        void               quitFatal();
        void               checkNextDueAlarm();
        void               checkKtimezoned();
        void               slotShowInSystemTrayChanged();
        void               changeStartOfDay();
        void               slotWorkTimeChanged(const QTime& start, const QTime& end, const QBitArray& days);
        void               slotHolidaysChanged(const KHolidays::HolidayRegion&);
        void               slotFeb29TypeChanged(Feb29Type);
        void               checkWritableCalendar();
        void               slotMessageFontChanged(const QFont&);
        void               setArchivePurgeDays();
        void               slotPurge()                     { purge(mArchivedPurgeDays); }
        void               purgeAfterDelay();
        void               slotCommandExited(ShellProcess*);
        void               slotDBusServiceUnregistered(const QString& serviceName);

    private:
        enum EventFunc
        {
            EVENT_HANDLE,    // if the alarm is due, execute it and then reschedule it
            EVENT_TRIGGER,   // execute the alarm regardless, and then reschedule it if it already due
            EVENT_CANCEL     // delete the alarm
        };
        struct ProcData
        {
            ProcData(ShellProcess*, KAEvent*, KAAlarm*, int flags = 0);
            ~ProcData();
            enum { PRE_ACTION = 0x01, POST_ACTION = 0x02, RESCHEDULE = 0x04, ALLOW_DEFER = 0x08,
                   TEMP_FILE = 0x10, EXEC_IN_XTERM = 0x20, DISP_OUTPUT = 0x40 };
            bool  preAction() const   { return flags & PRE_ACTION; }
            bool  postAction() const  { return flags & POST_ACTION; }
            bool  reschedule() const  { return flags & RESCHEDULE; }
            bool  allowDefer() const  { return flags & ALLOW_DEFER; }
            bool  tempFile() const    { return flags & TEMP_FILE; }
            bool  execInXterm() const { return flags & EXEC_IN_XTERM; }
            bool  dispOutput() const  { return flags & DISP_OUTPUT; }
            ShellProcess*     process;
            KAEvent*          event;
            KAAlarm*          alarm;
            QPointer<QWidget> messageBoxParent;
            QStringList       tempFiles;
            int               flags;
        };
        struct ActionQEntry
        {
            ActionQEntry(EventFunc f, const EventId& id) : function(f), eventId(id) { }
            ActionQEntry(const KAEvent& e, EventFunc f = EVENT_HANDLE) : function(f), event(e) { }
            ActionQEntry() { }
            EventFunc  function;
            EventId    eventId;
            KAEvent    event;
        };

        bool               initialise();
        bool               initCheck(bool calendarOnly = false, bool waitForCollection = false, Akonadi::Collection::Id = -1);
        bool               quitIf(int exitCode, bool force = false);
        bool               checkSystemTray();
        void               startProcessQueue();
        void               queueAlarmId(const KAEvent&);
        bool               dbusHandleEvent(const EventId&, EventFunc);
        bool               handleEvent(const EventId&, EventFunc, bool checkDuplicates = false);
        int                rescheduleAlarm(KAEvent&, const KAAlarm&, bool updateCalAndDisplay,
                                           const KDateTime& nextDt = KDateTime());
        bool               cancelAlarm(KAEvent&, KAAlarm::Type, bool updateCalAndDisplay);
        bool               cancelReminderAndDeferral(KAEvent&);
        ShellProcess*      doShellCommand(const QString& command, const KAEvent&, const KAAlarm*,
                                          int flags = 0, const QObject* receiver = 0, const char* slot = 0);
        QString            composeXTermCommand(const QString& command, const KAEvent&, const KAAlarm*,
                                               int flags, QString& tempScriptFile) const;
        QString            createTempScriptFile(const QString& command, bool insertShell, const KAEvent&, const KAAlarm&) const;
        void               commandErrorMsg(const ShellProcess*, const KAEvent&, const KAAlarm*, int flags = 0);
        void               purge(int daysToKeep);
        QStringList        scheduledAlarmList();

        static KAlarmApp*  theInstance;          // the one and only KAlarmApp instance
        static int         mActiveCount;         // number of active instances without main windows
        static int         mFatalError;          // a fatal error has occurred - just wait to exit
        static QString     mFatalMessage;        // fatal error message to output
        bool               mInitialised;         // initialisation complete: ready to process execution queue
        bool               mRedisplayAlarms;     // need to redisplay alarms when collection tree fetched
        bool               mQuitting;            // a forced quit is in progress
        bool               mReadOnly;            // only read-only access to calendars is needed
        bool               mLoginAlarmsDone;     // alarms repeated at login have been processed
        DBusHandler*       mDBusHandler;         // the parent of the main DCOP receiver object
        TrayWindow*        mTrayWindow;          // active system tray icon
        QTimer*            mAlarmTimer;          // activates KAlarm when next alarm is due
        QColor             mPrefsArchivedColour; // archived alarms text colour
        int                mArchivedPurgeDays;   // how long to keep archived alarms, 0 = don't keep, -1 = keep indefinitely
        int                mPurgeDaysQueued;     // >= 0 to purge the archive calendar from KAlarmApp::processLoop()
        QList<ProcData*>   mCommandProcesses;    // currently active command alarm processes
        QQueue<ActionQEntry> mActionQueue;       // queued commands and actions
        mutable OrgKdeKSpeechInterface* mKSpeech;// KSpeech D-Bus interface object
        int                mPendingQuitCode;     // exit code for a pending quit
        bool               mPendingQuit;         // quit once the DCOP command and shell command queues have been processed
        bool               mCancelRtcWake;       // cancel RTC wake on quitting
        bool               mProcessingQueue;     // a mActionQueue entry is currently being processed
        bool               mNoSystemTray;        // no system tray exists
        bool               mSessionClosingDown;  // session manager is closing the application
        bool               mOldShowInSystemTray; // showing in system tray was selected
        bool               mAlarmsEnabled;       // alarms are enabled
        bool               mSpeechEnabled;       // speech synthesis is enabled (kttsd exists)
        bool               mKOrganizerEnabled;   // KOrganizer options are enabled (korganizer exists)
};

inline KAlarmApp* theApp()  { return KAlarmApp::getInstance(); }

#endif // KALARMAPP_H

// vim: et sw=4:
