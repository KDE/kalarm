/*
 *  kalarmapp.h  -  the KAlarm application object
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

/** @file kalarmapp.h - the KAlarm application object */

#include "eventid.h"
#include "preferences.h"
#include "kalarmcalendar/kaevent.h"

#include <QApplication>
#include <QPointer>
#include <QQueue>

namespace KCal { class Event; }
namespace MailSend { struct JobData; }
class Resource;
class DBusHandler;
class MainWindow;
class MessageWindow;
class TrayWindow;
class ShellProcess;

using namespace KAlarmCal;


class KAlarmApp : public QApplication
{
    Q_OBJECT
public:
    /** Flags for execAlarm(). */
    enum ExecAlarmFlag
    {
        NoExecFlag       = 0,
        Reschedule       = 0x01,  // reschedule the alarm after executing it
        AllowDefer       = 0x02,  // allow the alarm to be deferred
        NoRecordCmdError = 0x04,  // don't record command errors
        NoPreAction      = 0x08,  // it isn't a pre-alarm action
        NoNotifyInhibit  = 0x10   // ignore notification inhibit
    };
    Q_DECLARE_FLAGS(ExecAlarmFlags, ExecAlarmFlag)

    ~KAlarmApp() override;

    /** Create the unique instance. */
    static KAlarmApp*  create(int& argc, char** argv);

    /** Must be called to complete initialisation after KAboutData is set,
     *  but before the application is activated or restored.
     */
    void               initialise();

    /** Return the unique instance. */
    static KAlarmApp*  instance()                      { return mInstance; }

    bool               checkCalendar()                 { return initCheck(); }
    bool               wantShowInSystemTray() const;
    bool               alarmsEnabled() const           { return mAlarmsEnabled; }
    bool               korganizerEnabled() const       { return mKOrganizerEnabled; }
    int                activateInstance(const QStringList& args, const QString& workingDirectory, QString* outputText = nullptr);
    bool               restoreSession();
    bool               quitIf()                        { return quitIf(0); }
    void               doQuit(QWidget* parent);
    static void        displayFatalError(const QString& message);
    void               addWindow(TrayWindow* w)        { mTrayWindow = w; }
    void               removeWindow(TrayWindow*);
    TrayWindow*        trayWindow() const              { return mTrayWindow; }
    MainWindow*        trayMainWindow() const;
    bool               displayTrayIcon(bool show, MainWindow* = nullptr);
    bool               trayIconDisplayed() const       { return mTrayWindow; }
    bool               editNewAlarm(MainWindow* = nullptr);

    void*              execAlarm(KAEvent&, const KAAlarm&, ExecAlarmFlags flags = NoExecFlag);
    ShellProcess*      execCommandAlarm(const KAEvent&, const KAAlarm&, bool noRecordError = false,
                                        QObject* receiver = nullptr,
                                        const char* slotOutput = nullptr,
                                        const char* methodExited = nullptr);
    void               alarmCompleted(const KAEvent&);
    void               rescheduleAlarm(KAEvent& e, const KAAlarm& a)   { rescheduleAlarm(e, a, true); }
    void               purgeAll()             { purge(0); }
    void               commandMessage(ShellProcess*, QWidget* parent);
    void               notifyAudioPlaying(bool playing);
    void               setSpreadWindowsState(bool spread);
    bool               windowFocusBroken() const;
    bool               needWindowFocusFix() const;
    // Methods called indirectly by the D-Bus interface
    bool               scheduleEvent(KAEvent::SubAction subAction, const QString& name, const QString& text,
                                     const KADateTime& dt, int lateCancel, KAEvent::Flags flags,
                                     const QColor& bg, const QColor& fg, const QFont& font,
                                     const QString& audioFile, float audioVolume,
                                     int reminderMinutes, const KARecurrence& recurrence,
                                     const KCalendarCore::Duration& repeatInterval, int repeatCount,
                                     uint mailFromID = 0, const KCalendarCore::Person::List& mailAddresses = KCalendarCore::Person::List(),
                                     const QString& mailSubject = QString(),
                                     const QStringList& mailAttachments = QStringList())
    { return scheduleEvent(QueuedAction::NoAction,
                           subAction, name, text, dt, lateCancel, flags,
                           bg, fg, font, audioFile, audioVolume,
                           reminderMinutes, recurrence, repeatInterval, repeatCount,
                           mailFromID, mailAddresses, mailSubject, mailAttachments);
    }
    bool               dbusTriggerEvent(const EventId& eventID)   { return dbusHandleEvent(eventID, QueuedAction::Trigger); }
    bool               dbusDeleteEvent(const EventId& eventID)    { return dbusHandleEvent(eventID, QueuedAction::Cancel); }
    QString            dbusList();

public Q_SLOTS:
    void               activateByDBus(const QStringList& args, const QString& workingDirectory)
                                        { activateInstance(args, workingDirectory); }
    void               processQueue();
    void               setAlarmsEnabled(bool);
    void               purgeNewArchivedDefault(const Resource&);
    void               atLoginEventAdded(const KAlarmCal::KAEvent&);
    void               notifyAudioStopped()  { notifyAudioPlaying(false); }
    void               stopAudio();
    void               spreadWindows(bool);
    void               emailSent(const MailSend::JobData&, const QStringList& errmsgs, bool copyerr = false);

Q_SIGNALS:
    void               setExitValue(int);   // set exit code for duplicate application instances
    void               trayIconToggled();
    void               alarmEnabledToggled(bool);
    void               audioPlaying(bool);
    void               spreadWindowsToggled(bool);
    void               execAlarmSuccess();

private:
    using Feb29Type = Preferences::Feb29Type;   // allow it to be used in SIGNAL mechanism

private Q_SLOTS:
    void               quitFatal();
    void               checkNextDueAlarm();
    void               slotShowInSystemTrayChanged();
    void               changeStartOfDay();
    void               slotWorkTimeChanged(const QTime& start, const QTime& end, const QBitArray& days);
    void               slotHolidaysChanged(const KAlarmCal::Holidays&);
    void               slotFeb29TypeChanged(KAlarmApp::Feb29Type);
    void               slotResourcesTimeout();
    void               slotResourcesCreated();
    void               slotEditAlarmById();
    void               promptArchivedCalendar();
    void               slotMessageFontChanged(const QFont&);
    void               setArchivePurgeDays();
    void               slotResourceAdded(const Resource&);
    void               slotResourcePopulated(const Resource&);
    void               slotPurge()                     { purge(mArchivedPurgeDays); }
    void               slotCommandExited(ShellProcess*);
    void               slotFDOPropertiesChanged(const QString& interface,
                                                const QVariantMap& changedProperties,
                                                const QStringList& invalidatedProperties);

private:
    // Actions to execute in processQueue(). May be OR'ed together.
    enum class QueuedAction
    {
        // Action to execute
        NoAction   = 0,
        ActionMask = 0x07,  // bit mask to extract action to execute
        Handle     = 0x01,  // if the alarm is due, execute it and then reschedule it
        Trigger    = 0x02,  // execute the alarm regardless, and then reschedule it if it's already due
        Cancel     = 0x03,  // delete the alarm
        Edit       = 0x04,  // edit an alarm (command line option)
        List       = 0x05,  // list all alarms (command line option)
        // Modifier flags
        FindId     = 0x10,  // search all resources for unique event ID
        Exit       = 0x20,  // exit application after executing action
        ErrorExit  = 0x40,  // exit application after executing action if it fails
        CmdLine    = 0x80   // the action is from the command line, so output error messages
    };
    struct ProcData
    {
        ProcData(ShellProcess*, KAEvent*, KAAlarm*, int flags = 0);
        ~ProcData();
        enum
        {
            PRE_ACTION      = 0x01,
            POST_ACTION     = 0x02,
            RESCHEDULE      = 0x04,
            ALLOW_DEFER     = 0x08,
            TEMP_FILE       = 0x10,
            EXEC_IN_XTERM   = 0x20,
            DISP_OUTPUT     = 0x40,
            NO_RECORD_ERROR = 0x80   // don't record command error
        };
        bool  preAction() const      { return flags & PRE_ACTION; }
        bool  postAction() const     { return flags & POST_ACTION; }
        bool  reschedule() const     { return flags & RESCHEDULE; }
        bool  allowDefer() const     { return flags & ALLOW_DEFER; }
        bool  tempFile() const       { return flags & TEMP_FILE; }
        bool  execInXterm() const    { return flags & EXEC_IN_XTERM; }
        bool  dispOutput() const     { return flags & DISP_OUTPUT; }
        bool  noRecordCmdErr() const { return flags & NO_RECORD_ERROR; }
        ShellProcess*     process;
        KAEvent*          event;
        KAAlarm*          alarm;
        QPointer<QObject> exitReceiver;
        QByteArray        exitMethod;
        QPointer<QWidget> messageBoxParent;
        QStringList       tempFiles;
        int               flags;
        bool              eventDeleted {false};
    };
    struct ActionQEntry
    {
        ActionQEntry(QueuedAction a, const EventId& id) : action(a), eventId(id) { }
        ActionQEntry(QueuedAction a, const EventId& id, const QString& resId) : action(a), eventId(id), resourceId(resId) { }
        explicit ActionQEntry(const KAEvent& e, QueuedAction a = QueuedAction::Handle) : action(a), event(e) { }
        ActionQEntry() = default;       //cppcheck-suppress[uninitMemberVar]  user must initialise struct
        QueuedAction  action;
        EventId       eventId;
        KAEvent       event;
        QString       resourceId;   // resource ID or name, if resources not created yet
    };

    KAlarmApp(int& argc, char** argv);
    bool               initialiseTimerResources();
    bool               initCheck(bool calendarOnly = false);
    bool               quitIf(int exitCode, bool force = false);
    void               showRestoredWindows();
    void               createOnlyMainWindow();
    bool               checkSystemTray();
    void               startProcessQueue(bool evenIfStarted = false);
    void               setResourcesTimeout();
    void               checkWritableCalendar();
    void               checkArchivedCalendar();
    void               queueAlarmId(const KAEvent&);
    bool               dbusHandleEvent(const EventId&, QueuedAction);
    bool               scheduleEvent(QueuedAction queuedActionFlags,
                                     KAEvent::SubAction, const QString& name, const QString& text,
                                     const KADateTime&, int lateCancel, KAEvent::Flags flags,
                                     const QColor& bg, const QColor& fg, const QFont&,
                                     const QString& audioFile, float audioVolume,
                                     int reminderMinutes, const KARecurrence& recurrence,
                                     const KCalendarCore::Duration& repeatInterval, int repeatCount,
                                     uint mailFromID = 0, const KCalendarCore::Person::List& mailAddresses = KCalendarCore::Person::List(),
                                     const QString& mailSubject = QString(),
                                     const QStringList& mailAttachments = QStringList());
    int                handleEvent(const EventId&, QueuedAction, bool findUniqueId = false);
    int                rescheduleAlarm(KAEvent&, const KAAlarm&, bool updateCalAndDisplay,
                                       const KADateTime& nextDt = KADateTime());
    bool               cancelAlarm(KAEvent&, KAAlarm::Type, bool updateCalAndDisplay);
    bool               cancelReminderAndDeferral(KAEvent&);
    ShellProcess*      doShellCommand(const QString& command, const KAEvent&, const KAAlarm*,
                                      int flags = 0, QObject* receiver = nullptr,
                                      const char* slotOutput = nullptr,
                                      const char* methodExited = nullptr);
    QString            composeXTermCommand(const QString& command, const KAEvent&, const KAAlarm*,
                                           int flags, QString& tempScriptFile) const;
    QString            createTempScriptFile(const QString& command, bool insertShell, const KAEvent&, const KAAlarm&) const;
    void               commandErrorMsg(const ShellProcess*, const KAEvent&, const KAAlarm*, int flags = 0, const QStringList& errmsgs = QStringList());
    void               purge(int daysToKeep);
    QStringList        scheduledAlarmList();
    void               setEventCommandError(const KAEvent&, KAEvent::CmdErr) const;
    void               clearEventCommandError(const KAEvent&, KAEvent::CmdErr) const;
    ProcData*          findCommandProcess(const QString& eventId) const;

    static KAlarmApp*  mInstance;               // the one and only KAlarmApp instance
    static int         mActiveCount;            // number of active instances without main windows
    static int         mFatalError;             // a fatal error has occurred - just wait to exit
    static QString     mFatalMessage;           // fatal error message to output
    QString            mCommandOption;          // command option used on command line
    bool               mInitialised {false};    // initialisation complete: ready to process execution queue
    bool               mRedisplayAlarms {false}; // need to redisplay alarms when collection tree fetched
    bool               mQuitting {false};       // a forced quit is in progress
    bool               mReadOnly {false};       // only read-only access to calendars is needed
    QString            mActivateArg0;           // activate()'s first arg the first time it was called
    DBusHandler*       mDBusHandler;            // the parent of the main D-Bus receiver object
    TrayWindow*        mTrayWindow {nullptr};   // active system tray icon
    QTimer*            mAlarmTimer {nullptr};   // activates KAlarm when next alarm is due
    QColor             mPrefsArchivedColour;    // archived alarms text colour
    int                mArchivedPurgeDays {-1}; // how long to keep archived alarms, 0 = don't keep, -1 = keep indefinitely
    int                mPurgeDaysQueued {-1};   // >= 0 to purge the archive calendar from KAlarmApp::processLoop()
    QList<ResourceId>  mPendingPurges;          // new resources which may need to be purged when populated
    QList<ProcData*>   mCommandProcesses;       // currently active command alarm processes
    QQueue<ActionQEntry> mActionQueue;          // queued commands and actions
    QList<MessageWindow*> mRestoredWindows;     // message windows restored at startup, waiting to be displayed
    int                mEditingCmdLineAlarm {0}; // whether currently editing alarm specified on command line
    int                mPendingQuitCode;        // exit code for a pending quit
    bool               mPendingQuit {false};    // quit once the D-Bus command and shell command queues have been processed
    bool               mCancelRtcWake {false};   // cancel RTC wake on quitting
    bool               mProcessingQueue {false}; // a mActionQueue entry is currently being processed
    bool               mNoSystemTray;           // no system tray exists
    bool               mOldShowInSystemTray;    // showing in system tray was selected
    bool               mAlarmsEnabled {true};   // alarms are enabled
    bool               mKOrganizerEnabled;      // KOrganizer options are enabled (korganizer exists)
    bool               mWindowFocusBroken;      // keyboard focus transfer between windows doesn't work
    bool               mResourcesTimedOut {false}; // timeout has expired for populating resources
    bool               mNotificationsInhibited {false};  // Freedesktop notifications are inhibited
};

inline KAlarmApp* theApp()  { return KAlarmApp::instance(); }

Q_DECLARE_OPERATORS_FOR_FLAGS(KAlarmApp::ExecAlarmFlags)

// vim: et sw=4:
