/*
 *  kalarmapp.cpp  -  the KAlarm application object
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kalarmapp.h"

#include "kalarm.h"
#include "commandoptions.h"
#include "dbushandler.h"
#include "displaycalendar.h"
#include "editdlgtypes.h"
#include "functions.h"
#include "kamail.h"
#include "mainwindow.h"
#include "messagenotification.h"
#include "messagewindow.h"
#include "pluginmanager.h"
#include "resourcescalendar.h"
#include "startdaytimer.h"
#include "traywindow.h"
#include "resources/datamodel.h"
#include "resources/resources.h"
#include "lib/desktop.h"
#include "lib/messagebox.h"
#include "notifications_interface.h" // DBUS-generated
#include "dbusproperties.h"          // DBUS-generated
#include "kalarmcalendar/datetime.h"
#include "kalarmcalendar/karecurrence.h"
#include "kalarm_debug.h"

#include <KLocalizedString>
#include <KConfig>
#include <KConfigGui>
#include <KAboutData>
#include <KSharedConfig>
#include <KStandardGuiItem>
#include <netwm.h>
#include <KShell>

#include <QObject>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QCommandLineParser>

#include <stdlib.h>
#include <ctype.h>
#include <iostream>
#include <climits>

namespace
{
const int RESOURCES_TIMEOUT = 30;   // timeout (seconds) for resources to be populated

const char FDO_NOTIFICATIONS_SERVICE[] = "org.freedesktop.Notifications";
const char FDO_NOTIFICATIONS_PATH[]    = "/org/freedesktop/Notifications";

/******************************************************************************
* Find the maximum number of seconds late which a late-cancel alarm is allowed
* to be. This is calculated as the late cancel interval, plus a few seconds
* leeway to cater for any timing irregularities.
*/
inline int maxLateness(int lateCancel)
{
    static const int LATENESS_LEEWAY = 5;
    int lc = (lateCancel >= 1) ? (lateCancel - 1)*60 : 0;
    return LATENESS_LEEWAY + lc;
}

QWidget* mainWidget()
{
    return MainWindow::mainMainWindow();
}
}


KAlarmApp*  KAlarmApp::mInstance  = nullptr;
int         KAlarmApp::mActiveCount = 0;
int         KAlarmApp::mFatalError  = 0;
QString     KAlarmApp::mFatalMessage;


/******************************************************************************
* Construct the application.
*/
KAlarmApp::KAlarmApp(int& argc, char** argv)
    : QApplication(argc, argv)
    , mDBusHandler(new DBusHandler())
{
    qCDebug(KALARM_LOG) << "KAlarmApp:";
}

/******************************************************************************
*/
KAlarmApp::~KAlarmApp()
{
    while (!mCommandProcesses.isEmpty())
    {
        ProcData* pd = mCommandProcesses.at(0);
        mCommandProcesses.pop_front();
        delete pd;
    }
    ResourcesCalendar::terminate();
    DisplayCalendar::terminate();
    DataModel::terminate();
    delete mDBusHandler;
}

/******************************************************************************
* Return the one and only KAlarmApp instance.
* If it doesn't already exist, it is created first.
*/
KAlarmApp* KAlarmApp::create(int& argc, char** argv)
{
    if (!mInstance)
    {
        mInstance = new KAlarmApp(argc, argv);

        if (mFatalError)
            mInstance->quitFatal();
    }
    return mInstance;
}

/******************************************************************************
* Perform initialisations which may require KAboutData to have been set up.
*/
void KAlarmApp::initialise()
{
#ifndef NDEBUG
    KAlarm::setTestModeConditions();
#endif

    setQuitOnLastWindowClosed(false);
    Preferences::self();    // read KAlarm configuration
    if (!Preferences::noAutoStart())
    {
        // Strip out any "OnlyShowIn=KDE" list from kalarm.autostart.desktop
        Preferences::setNoAutoStart(false);
        // Enable kalarm.autostart.desktop to start KAlarm
        Preferences::setAutoStart(true);
        Preferences::self()->save();
    }
    Preferences::connect(&Preferences::startOfDayChanged, this, &KAlarmApp::changeStartOfDay);
    Preferences::connect(&Preferences::workTimeChanged, this, &KAlarmApp::slotWorkTimeChanged);
    Preferences::connect(&Preferences::holidaysChanged, this, &KAlarmApp::slotHolidaysChanged);
    Preferences::connect(&Preferences::feb29TypeChanged, this, &KAlarmApp::slotFeb29TypeChanged);
    Preferences::connect(&Preferences::showInSystemTrayChanged, this, &KAlarmApp::slotShowInSystemTrayChanged);
    Preferences::connect(&Preferences::archivedKeepDaysChanged, this, &KAlarmApp::setArchivePurgeDays);
    Preferences::connect(&Preferences::messageFontChanged, this, &KAlarmApp::slotMessageFontChanged);
    slotFeb29TypeChanged(Preferences::defaultFeb29Type());

    KAEvent::setStartOfDay(Preferences::startOfDay());
    KAEvent::setWorkTime(Preferences::workDays(), Preferences::workDayStart(), Preferences::workDayEnd(), Preferences::timeSpec());
    KAEvent::setHolidays(Preferences::holidays());
    KAEvent::setDefaultFont(Preferences::messageFont());

    // Load plugins
    if (PluginManager::instance()->akonadiPlugin())
        qCDebug(KALARM_LOG) << "KAlarmApp: Akonadi plugin found";
    else
        qCDebug(KALARM_LOG) << "KAlarmApp: Akonadi dependent features not available (Akonadi plugin not found)";

    // Check if KOrganizer is installed
    const QString korg = QStringLiteral("korganizer");
    mKOrganizerEnabled = !QStandardPaths::findExecutable(korg).isEmpty();
    if (!mKOrganizerEnabled) { qCDebug(KALARM_LOG) << "KAlarmApp: KOrganizer options disabled (KOrganizer not found)"; }
    // Check if the window manager can't handle keyboard focus transfer between windows
    mWindowFocusBroken = (Desktop::currentIdentity() == Desktop::Unity);
    if (mWindowFocusBroken) { qCDebug(KALARM_LOG) << "KAlarmApp: Window keyboard focus broken"; }

    Resources* resources = Resources::instance();
    connect(resources, &Resources::resourceAdded,
                 this, &KAlarmApp::slotResourceAdded);
    connect(resources, &Resources::resourcePopulated,
                 this, &KAlarmApp::slotResourcePopulated);
    connect(resources, &Resources::resourcePopulated,
                 this, &KAlarmApp::purgeNewArchivedDefault);
    connect(resources, &Resources::resourcesCreated,
                 this, &KAlarmApp::slotResourcesCreated);
    connect(resources, &Resources::resourcesPopulated,
                 this, &KAlarmApp::processQueue);

    initialiseTimerResources();   // initialise calendars and alarm timer

    KConfigGroup config(KSharedConfig::openConfig(), "General");
    mNoSystemTray        = config.readEntry("NoSystemTray", false);
    mOldShowInSystemTray = wantShowInSystemTray();
    DateTime::setStartOfDay(Preferences::startOfDay());
    mPrefsArchivedColour = Preferences::archivedColour();

    // Get notified when the Freedesktop notifications properties have changed.
    QDBusConnection conn = QDBusConnection::sessionBus();
    if (conn.interface()->isServiceRegistered(QString::fromLatin1(FDO_NOTIFICATIONS_SERVICE)))
    {
        OrgFreedesktopDBusPropertiesInterface* piface = new OrgFreedesktopDBusPropertiesInterface(
                QString::fromLatin1(FDO_NOTIFICATIONS_SERVICE),
                QString::fromLatin1(FDO_NOTIFICATIONS_PATH),
                conn, this);
        connect(piface, &OrgFreedesktopDBusPropertiesInterface::PropertiesChanged,
                this, &KAlarmApp::slotFDOPropertiesChanged);
        OrgFreedesktopNotificationsInterface niface(
                QString::fromLatin1(FDO_NOTIFICATIONS_SERVICE),
                QString::fromLatin1(FDO_NOTIFICATIONS_PATH),
                conn);
        mNotificationsInhibited = niface.inhibited();
    }
}

/******************************************************************************
* Initialise or reinitialise things which are tidied up/closed by quitIf().
* Reinitialisation can be necessary if session restoration finds nothing to
* restore and starts quitting the application, but KAlarm then starts up again
* before the application has exited.
* Reply = true if calendars were initialised successfully,
*         false if they were already initialised, or if initialisation failed.
*/
bool KAlarmApp::initialiseTimerResources()
{
    if (!mAlarmTimer)
    {
        mAlarmTimer = new QTimer(this);
        mAlarmTimer->setSingleShot(true);
        connect(mAlarmTimer, &QTimer::timeout, this, &KAlarmApp::checkNextDueAlarm);
    }
    if (!ResourcesCalendar::instance())
    {
        qCDebug(KALARM_LOG) << "KAlarmApp::initialise: initialising calendars";
        Desktop::setMainWindowFunc(&mainWidget);
        // First, initialise calendar resources, which need to be ready to
        // receive signals when resources initialise.
        ResourcesCalendar::initialise(KALARM_NAME, KALARM_VERSION);
        connect(ResourcesCalendar::instance(), &ResourcesCalendar::earliestAlarmChanged, this, &KAlarmApp::checkNextDueAlarm);
        connect(ResourcesCalendar::instance(), &ResourcesCalendar::atLoginEventAdded, this, &KAlarmApp::atLoginEventAdded);
        DisplayCalendar::initialise();
        // Finally, initialise the resources which generate signals as they initialise.
        DataModel::initialise();
        return true;
    }
    return false;
}

/******************************************************************************
* Restore the saved session if required.
*/
bool KAlarmApp::restoreSession()
{
    if (!isSessionRestored())
        return false;
    if (mFatalError)
    {
        quitFatal();
        return false;
    }

    // Process is being restored by session management.
    qCDebug(KALARM_LOG) << "KAlarmApp::restoreSession: Restoring";
    ++mActiveCount;
    // Create the session config object now.
    // This is necessary since if initCheck() below causes calendars to be updated,
    // the session config created after that points to an invalid file, resulting
    // in no windows being restored followed by a later crash.
    KConfigGui::sessionConfig();

    // When KAlarm is session restored, automatically set start-at-login to true.
    Preferences::self()->load();
    Preferences::setAutoStart(true);
    Preferences::setNoAutoStart(false);
    Preferences::setAskAutoStart(true);  // cancel any start-at-login prompt suppression
    Preferences::self()->save();

    if (!initCheck(true))     // open the calendar file (needed for main windows), don't process queue yet
    {
        --mActiveCount;
        quitIf(1, true);    // error opening the main calendar - quit
        return false;
    }
    MainWindow* trayParent = nullptr;
    for (int i = 1;  KMainWindow::canBeRestored(i);  ++i)
    {
        const QString type = KMainWindow::classNameOfToplevel(i);
        if (type == QLatin1String("MainWindow"))
        {
            MainWindow* win = MainWindow::create(true);
            win->restore(i, false);
            if (win->isHiddenTrayParent())
                trayParent = win;
            else
                win->show();
        }
        else if (type == QLatin1String("MessageWindow"))
        {
            auto win = new MessageWindow;
            win->restore(i, false);
            if (win->isValid())
            {
                if (Resources::allCreated()  &&  !mNotificationsInhibited)
                    win->display();
                else
                    mRestoredWindows += win;
            }
            else
                delete win;
        }
    }

    MessageNotification::sessionRestore();

    // 1) There should have been a main window to restore, but even if not, there
    //    must always be one, so create one if necessary but don't show it.
    // 2) Try to display the system tray icon if it is configured to be shown
    if (trayParent  ||  wantShowInSystemTray())
    {
        if (!MainWindow::count())
        {
            qCWarning(KALARM_LOG) << "KAlarmApp::restoreSession: no main window to restore!?";
            trayParent = MainWindow::create();
        }
        displayTrayIcon(true, trayParent);
        // Occasionally for no obvious reason, the main main window is
        // shown when it should be hidden, so hide it just to be sure.
        if (trayParent)
            trayParent->hide();
    }
    else
        createOnlyMainWindow();

    --mActiveCount;
    if (quitIf(0))          // quit if no windows are open
        return false;       // quitIf() can sometimes return, despite calling exit()

    startProcessQueue();    // start processing the execution queue
    return true;
}

/******************************************************************************
* If resources have been created and notifications are not inhibited,
* show message windows restored at startup which are waiting to be displayed,
* and redisplay alarms showing when the program crashed or was killed.
*/
void KAlarmApp::showRestoredWindows()
{
    if (!mNotificationsInhibited  &&  Resources::allCreated())
    {
        if (!mRestoredWindows.isEmpty())
        {
            // Display message windows restored at startup.
            for (MessageWindow* win : std::as_const(mRestoredWindows))
                win->display();
            mRestoredWindows.clear();
        }
        if (mRedisplayAlarms)
        {
            // Display alarms which were showing when the program crashed or was killed.
            mRedisplayAlarms = false;
            MessageDisplay::redisplayAlarms();
        }
    }
}

/******************************************************************************
* Called to start a new instance of the unique QApplication.
* Reply: exit code (>= 0), or -1 to continue execution.
*        If exit code >= 0, 'outputText' holds text to output before terminating.
*/
int KAlarmApp::activateInstance(const QStringList& args, const QString& workingDirectory, QString* outputText)
{
    Q_UNUSED(workingDirectory)
    qCDebug(KALARM_LOG) << "KAlarmApp::activateInstance" << args;
    if (outputText)
        outputText->clear();
    if (mFatalError)
    {
        Q_EMIT setExitValue(1);
        quitFatal();
        return 1;
    }

    // The D-Bus call to activate a subsequent instance of KAlarm may not supply
    // any arguments, but we need one.
    if (!args.isEmpty()  &&  mActivateArg0.isEmpty())
        mActivateArg0 = args[0];
    QStringList fixedArgs(args);
    if (args.isEmpty()  &&  !mActivateArg0.isEmpty())
        fixedArgs << mActivateArg0;

    // Parse and interpret command line arguments.
    QCommandLineParser parser;
    KAboutData::applicationData().setupCommandLine(&parser);
    parser.setApplicationDescription(QApplication::applicationDisplayName());
    auto options = new CommandOptions;
    const QStringList nonexecArgs = options->setOptions(&parser, fixedArgs);
    options->parse();
    KAboutData::applicationData().processCommandLine(&parser);

    ++mActiveCount;
    int exitCode = 0;               // default = success
    static bool firstInstance = true;
    bool dontRedisplay = false;
    if (!firstInstance  ||  !isSessionRestored())
    {
        options->process();
#ifndef NDEBUG
        if (options->simulationTime().isValid())
            KAlarm::setSimulatedSystemTime(options->simulationTime());
#endif
        CommandOptions::Command command = options->command();
        if (options->disableAll())
            setAlarmsEnabled(false);   // disable alarm monitoring

        // Handle options which exit with a terminal message, before
        // making the application a unique application, since a
        // unique application won't output to the terminal if another
        // instance is already running.
        switch (command)
        {
            case CommandOptions::CMD_ERROR:
                Q_EMIT setExitValue(1);
                if (outputText)
                {
                    // Instance was activated from main().
                    *outputText = options->outputText();
                    delete options;
                    return 1;
                }
                // Instance was activated by DBus.
                std::cerr << qPrintable(options->outputText()) << std::endl;;
                mReadOnly = true;   // don't need write access to calendars
                exitCode = 1;
                break;
            default:
                break;
        }

        switch (command)
        {
            case CommandOptions::TRIGGER_EVENT:
            case CommandOptions::CANCEL_EVENT:
            {
                // Display or delete the event with the specified event ID
                auto action = static_cast<QueuedAction>(int((command == CommandOptions::TRIGGER_EVENT) ? QueuedAction::Trigger : QueuedAction::Cancel)
                                                      | int(QueuedAction::Exit));
                // Open the calendar, don't start processing execution queue yet,
                // and wait for the calendar resources to be populated.
                if (!initCheck(true))
                    exitCode = 1;
                else
                {
                    mCommandOption = options->commandName();
                    // Get the resource ID string and event UID. Note that if
                    // resources have not been created yet, the numeric
                    // resource ID can't yet be looked up.
                    if (options->resourceId().isEmpty())
                        action = static_cast<QueuedAction>((int)action | int(QueuedAction::FindId));
                    mActionQueue.enqueue(ActionQEntry(action, EventId(options->eventId()), options->resourceId()));
                    startProcessQueue(true);      // start processing the execution queue
                    dontRedisplay = true;
                }
                break;
            }
            case CommandOptions::LIST:
                // Output a list of scheduled alarms to stdout.
                // Open the calendar, don't start processing execution queue yet,
                // and wait for all calendar resources to be populated.
                mReadOnly = true;   // don't need write access to calendars
                if (firstInstance)
                    mAlarmsEnabled = false;   // prevent alarms being processed if no other instance is running
                if (!initCheck(true))
                    exitCode = 1;
                else
                {
                    const auto action = static_cast<QueuedAction>(int(QueuedAction::List) | int(QueuedAction::Exit));
                    mActionQueue.enqueue(ActionQEntry(action, EventId()));
                    startProcessQueue(true);      // start processing the execution queue
                    dontRedisplay = true;
                }
                break;

            case CommandOptions::EDIT:
                // Edit a specified existing alarm.
                // Open the calendar and wait for the calendar resources to be populated.
                if (!initCheck(false))
                    exitCode = 1;
                else
                {
                    mCommandOption = options->commandName();
                    if (firstInstance)
                        mEditingCmdLineAlarm = 0x10;   // want to redisplay alarms if successful
                    // Get the resource ID string and event UID. Note that if
                    // resources have not been created yet, the numeric
                    // resource ID can't yet be looked up.
                    mActionQueue.enqueue(ActionQEntry(QueuedAction::Edit, EventId(options->eventId()), options->resourceId()));
                    startProcessQueue(true);      // start processing the execution queue
                    dontRedisplay = true;
                }
                break;

            case CommandOptions::EDIT_NEW:
            {
                // Edit a new alarm, and optionally preset selected values
                if (!initCheck())
                    exitCode = 1;
                else
                {
                    EditAlarmDlg* editDlg = EditAlarmDlg::create(false, options->editType(), MainWindow::mainMainWindow());
                    if (!editDlg)
                    {
                        exitCode = 1;
                        break;
                    }
                    editDlg->setName(options->name());
                    if (options->alarmTime().isValid())
                        editDlg->setTime(options->alarmTime());
                    if (options->recurrence())
                        editDlg->setRecurrence(*options->recurrence(), options->subRepeatInterval(), options->subRepeatCount());
                    else if (options->flags() & KAEvent::REPEAT_AT_LOGIN)
                        editDlg->setRepeatAtLogin();
                    editDlg->setAction(options->editAction(), AlarmText(options->text()));
                    if (options->lateCancel())
                        editDlg->setLateCancel(options->lateCancel());
                    if (options->flags() & KAEvent::COPY_KORGANIZER)
                        editDlg->setShowInKOrganizer(true);
                    switch (options->editType())
                    {
                        case EditAlarmDlg::DISPLAY:
                        {
                            // EditAlarmDlg::create() always returns EditDisplayAlarmDlg for type = DISPLAY
                            auto dlg = qobject_cast<EditDisplayAlarmDlg*>(editDlg);
                            if (options->fgColour().isValid())
                                dlg->setFgColour(options->fgColour());
                            if (options->bgColour().isValid())
                                dlg->setBgColour(options->bgColour());
                            if (!options->audioFile().isEmpty()
                            ||  options->flags() & (KAEvent::BEEP | KAEvent::SPEAK))
                            {
                                const KAEvent::Flags flags = options->flags();
                                const Preferences::SoundType type = (flags & KAEvent::BEEP) ? Preferences::Sound_Beep
                                                                  : (flags & KAEvent::SPEAK) ? Preferences::Sound_Speak
                                                                  : Preferences::Sound_File;
                                dlg->setAudio(type, options->audioFile(), options->audioVolume(), ((flags & KAEvent::REPEAT_SOUND) ? 0 : -1));
                            }
                            if (options->reminderMinutes())
                                dlg->setReminder(options->reminderMinutes(), (options->flags() & KAEvent::REMINDER_ONCE));
                            if (options->flags() & KAEvent::NOTIFY)
                                dlg->setNotify(true);
                            if (options->flags() & KAEvent::CONFIRM_ACK)
                                dlg->setConfirmAck(true);
                            if (options->flags() & KAEvent::AUTO_CLOSE)
                                dlg->setAutoClose(true);
                            break;
                        }
                        case EditAlarmDlg::COMMAND:
                            break;
                        case EditAlarmDlg::EMAIL:
                        {
                            // EditAlarmDlg::create() always returns EditEmailAlarmDlg for type = EMAIL
                            auto dlg = qobject_cast<EditEmailAlarmDlg*>(editDlg);
                            if (options->fromID()
                            ||  !options->addressees().isEmpty()
                            ||  !options->subject().isEmpty()
                            ||  !options->attachments().isEmpty())
                                dlg->setEmailFields(options->fromID(), options->addressees(), options->subject(), options->attachments());
                            if (options->flags() & KAEvent::EMAIL_BCC)
                                dlg->setBcc(true);
                            break;
                        }
                        case EditAlarmDlg::AUDIO:
                        {
                            // EditAlarmDlg::create() always returns EditAudioAlarmDlg for type = AUDIO
                            auto dlg = qobject_cast<EditAudioAlarmDlg*>(editDlg);
                            if (!options->audioFile().isEmpty()  ||  options->audioVolume() >= 0)
                                dlg->setAudio(options->audioFile(), options->audioVolume());
                            break;
                        }
                        case EditAlarmDlg::NO_TYPE:
                            break;
                    }

                    // Execute the edit dialogue. Note that if no other instance of KAlarm is
                    // running, this new instance will not exit after the dialogue is closed.
                    // This is deliberate, since exiting would mean that KAlarm wouldn't
                    // trigger the new alarm.
                    KAlarm::execNewAlarmDlg(editDlg);

                    createOnlyMainWindow();   // prevent the application from quitting
                }
                break;
            }
            case CommandOptions::EDIT_NEW_PRESET:
                // Edit a new alarm, preset with a template
                if (!initCheck())
                    exitCode = 1;
                else
                {
                    // Execute the edit dialogue. Note that if no other instance of KAlarm is
                    // running, this new instance will not exit after the dialogue is closed.
                    // This is deliberate, since exiting would mean that KAlarm wouldn't
                    // trigger the new alarm.
                    KAlarm::editNewAlarm(options->name());

                    createOnlyMainWindow();   // prevent the application from quitting
                }
                break;

            case CommandOptions::NEW:
                // Display a message or file, execute a command, or send an email
                setResourcesTimeout();   // set timeout for resource initialisation
                if (!initCheck())
                    exitCode = 1;
                else
                {
                    QueuedAction flags = QueuedAction::CmdLine;
                    if (!MainWindow::count())
                        flags = static_cast<QueuedAction>(int(flags) + int(QueuedAction::ErrorExit));
                    if (!scheduleEvent(flags,
                                       options->editAction(), options->name(), options->text(),
                                       options->alarmTime(), options->lateCancel(), options->flags(),
                                       options->bgColour(), options->fgColour(), QFont(),
                                       options->audioFile(), options->audioVolume(),
                                       options->reminderMinutes(), (options->recurrence() ? *options->recurrence() : KARecurrence()),
                                       options->subRepeatInterval(), options->subRepeatCount(),
                                       options->fromID(), options->addressees(),
                                       options->subject(), options->attachments()))
                        exitCode = 1;
                    else
                        createOnlyMainWindow();   // prevent the application from quitting
                }
                break;

            case CommandOptions::TRAY:
                // Display only the system tray icon
                if (Preferences::showInSystemTray()  &&  QSystemTrayIcon::isSystemTrayAvailable())
                {
                    if (!initCheck())   // open the calendar, start processing execution queue
                        exitCode = 1;
                    else
                    {
                        if (!displayTrayIcon(true))
                            exitCode = 1;
                    }
                    break;
                }
                Q_FALLTHROUGH();   // fall through to NONE
            case CommandOptions::NONE:
                // No arguments - run interactively & display the main window
#ifndef NDEBUG
                if (options->simulationTime().isValid()  &&  !firstInstance)
                    break;   // simulating time: don't open main window if already running
#endif
                if (!initCheck())
                    exitCode = 1;
                else
                {
                    if (mTrayWindow  &&  mTrayWindow->assocMainWindow()  &&  !mTrayWindow->assocMainWindow()->isVisible())
                        mTrayWindow->showAssocMainWindow();
                    else
                    {
                        MainWindow* win = MainWindow::create();
                        if (command == CommandOptions::TRAY)
                            win->setWindowState(win->windowState() | Qt::WindowMinimized);
                        win->show();
                    }
                }
                break;
            default:
                break;
        }
    }
    if (options != CommandOptions::firstInstance())
        delete options;

    // If this is the first time through, redisplay any alarm message windows
    // from last time.
    if (firstInstance  &&  !dontRedisplay  &&  !exitCode)
    {
        /* First time through, so redisplay alarm message windows from last time.
         * But it is possible for session restoration in some circumstances to
         * not create any windows, in which case the alarm calendars will have
         * been deleted - if so, don't try to do anything. (This has been known
         * to happen under the Xfce desktop.)
         */
        if (ResourcesCalendar::instance())
        {
            mRedisplayAlarms = true;
            showRestoredWindows();
        }
    }

    --mActiveCount;
    firstInstance = false;

    // Quit the application if this was the last/only running "instance" of the program.
    // Executing 'return' doesn't work very well since the program continues to
    // run if no windows were created.
    if (quitIf(exitCode >= 0 ? exitCode : 0))
        return exitCode;    // exit this application instance

    return -1;   // continue executing the application instance
}

/******************************************************************************
* Create a minimised main window if none already exists.
* This prevents the application from quitting.
*/
void KAlarmApp::createOnlyMainWindow()
{
    if (!MainWindow::count())
    {
        if (wantShowInSystemTray())
        {
            if (displayTrayIcon(true))
                return;
        }
        MainWindow* win = MainWindow::create();
        win->setWindowState(Qt::WindowMinimized);
        win->show();
    }
}

/******************************************************************************
* Quit the program, optionally only if there are no more "instances" running.
* Reply = true if program exited.
*/
bool KAlarmApp::quitIf(int exitCode, bool force)
{
    if (force)
    {
        // Quit regardless, except for message windows
        mQuitting = true;
        MainWindow::closeAll();
        mQuitting = false;
        displayTrayIcon(false);
        if (MessageDisplay::instanceCount(true))    // ignore always-hidden displays (e.g. audio alarms)
            return false;
    }
    else if (mQuitting)
        return false;   // MainWindow::closeAll() causes quitIf() to be called again
    else
    {
        // Quit only if there are no more "instances" running
        mPendingQuit = false;
        if (mActiveCount > 0  ||  MessageDisplay::instanceCount(true))  // ignore always-hidden displays (e.g. audio alarms)
            return false;
        const int mwcount = MainWindow::count();
        MainWindow* mw = mwcount ? MainWindow::firstWindow() : nullptr;
        if (mwcount > 1  ||  (mwcount && (!mw->isHidden() || !mw->isTrayParent())))
            return false;
        // There are no windows left except perhaps a main window which is a hidden
        // tray icon parent, or an always-hidden message window.
        if (mTrayWindow)
        {
            // There is a system tray icon.
            // Don't exit unless the system tray doesn't seem to exist.
            if (checkSystemTray())
                return false;
        }
        if (!mActionQueue.isEmpty()  ||  !mCommandProcesses.isEmpty())
        {
            // Don't quit yet if there are outstanding actions on the execution queue
            mPendingQuit = true;
            mPendingQuitCode = exitCode;
            return false;
        }
    }

    // This was the last/only running "instance" of the program, so exit completely.
    // NOTE: Everything which is terminated/deleted here must where applicable
    //       be initialised in the initialiseTimerResources() method, in case
    //       KAlarm is started again before application exit completes!
    qCDebug(KALARM_LOG) << "KAlarmApp::quitIf:" << exitCode << ": quitting";
    MessageDisplay::stopAudio();
#if ENABLE_WAKE_FROM_SUSPEND
    if (mCancelRtcWake)
    {
        KAlarm::setRtcWakeTime(0, nullptr);
        KAlarm::deleteRtcWakeConfig();
    }
#endif
    delete mAlarmTimer;     // prevent checking for alarms after deleting calendars
    mAlarmTimer = nullptr;
    mInitialised = false;   // prevent processQueue() from running
    ResourcesCalendar::terminate();
    DisplayCalendar::terminate();
    DataModel::terminate();
    Q_EMIT setExitValue(exitCode);
    exit(exitCode);
    return true;    // sometimes we actually get to here, despite calling exit()
}

/******************************************************************************
* Called when the Quit menu item is selected.
* Closes the system tray window and all main windows, but does not exit the
* program if other windows are still open.
*/
void KAlarmApp::doQuit(QWidget* parent)
{
    qCDebug(KALARM_LOG) << "KAlarmApp::doQuit";
    if (KAMessageBox::warningCancelContinue(parent,
                                            i18nc("@info", "Quitting will disable alarms (once any alarm message windows are closed)."),
                                            QString(), KStandardGuiItem::quit(),
                                            KStandardGuiItem::cancel(), Preferences::QUIT_WARN
                                           ) != KMessageBox::Continue)
        return;
#if ENABLE_WAKE_FROM_SUSPEND
    if (!KAlarm::checkRtcWakeConfig(true).isEmpty())
    {
        // A wake-on-suspend alarm is set
        if (KAMessageBox::warningCancelContinue(parent,
                                                i18nc("@info", "Quitting will cancel the scheduled Wake from Suspend."),
                                                QString(), KStandardGuiItem::quit()
                                               ) != KMessageBox::Continue)
            return;
        mCancelRtcWake = true;
    }
#endif
    if (!Preferences::autoStart())
    {
        int option = KMessageBox::ButtonCode::SecondaryAction;
        if (!Preferences::autoStartChangedByUser())
        {
            option = KAMessageBox::questionYesNoCancel(parent,
                                         xi18nc("@info", "Do you want to start KAlarm at login?<nl/>"
                                                        "(Note that alarms will be disabled if KAlarm is not started.)"),
                                         QString(), KGuiItem(i18n("Yes")), KGuiItem(i18n("No")),
                                         KStandardGuiItem::cancel(), Preferences::ASK_AUTO_START);
        }
        switch (option)
        {
            case KMessageBox::ButtonCode::PrimaryAction:
                Preferences::setAutoStart(true);
                Preferences::setNoAutoStart(false);
                break;
            case KMessageBox::ButtonCode::SecondaryAction:
                Preferences::setNoAutoStart(true);
                break;
            case KMessageBox::Cancel:
            default:
                return;
        }
        Preferences::self()->save();
    }
    quitIf(0, true);
}

/******************************************************************************
* Display an error message for a fatal error. Prevent further actions since
* the program state is unsafe.
*/
void KAlarmApp::displayFatalError(const QString& message)
{
    if (!mFatalError)
    {
        mFatalError = 1;
        mFatalMessage = message;
        if (mInstance)
            QTimer::singleShot(0, mInstance, &KAlarmApp::quitFatal);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    }
}

/******************************************************************************
* Quit the program, once the fatal error message has been acknowledged.
*/
void KAlarmApp::quitFatal()
{
    switch (mFatalError)
    {
        case 0:
        case 2:
            return;
        case 1:
            mFatalError = 2;
            KMessageBox::error(nullptr, mFatalMessage);   // this is an application modal window
            mFatalError = 3;
            Q_FALLTHROUGH();   // fall through to '3'
        case 3:
            if (mInstance)
                mInstance->quitIf(1, true);
            break;
    }
    QTimer::singleShot(1000, this, &KAlarmApp::quitFatal);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
}

/******************************************************************************
* Called by the alarm timer when the next alarm is due.
* Also called when the execution queue has finished processing to check for the
* next alarm.
*/
void KAlarmApp::checkNextDueAlarm()
{
    if (!mAlarmsEnabled)
        return;
    // Find the first alarm due
    KADateTime nextDt;
    const KAEvent nextEvent = ResourcesCalendar::earliestAlarm(nextDt, mNotificationsInhibited);
    if (!nextEvent.isValid())
        return;   // there are no alarms pending
    const KADateTime now = KADateTime::currentDateTime(Preferences::timeSpec());
    qint64 interval = now.msecsTo(nextDt);
    qCDebug(KALARM_LOG) << "KAlarmApp::checkNextDueAlarm: now:" << qPrintable(now.toString(QStringLiteral("%Y-%m-%d %H:%M %:Z"))) << ", next:" << qPrintable(nextDt.toString(QStringLiteral("%Y-%m-%d %H:%M %:Z"))) << ", due:" << interval;
    if (interval <= 0)
    {
        // Queue the alarm
        queueAlarmId(nextEvent);
        qCDebug(KALARM_LOG) << "KAlarmApp::checkNextDueAlarm:" << nextEvent.id() << ": due now";
        QTimer::singleShot(0, this, &KAlarmApp::processQueue);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    }
    else
    {
        // No alarm is due yet, so set timer to wake us when it's due.
        // Check for integer overflow before setting timer.
#ifndef HIBERNATION_SIGNAL
        /* TODO: Use hibernation wakeup signal:
         *   #include <Solid/Power>
         *   connect(Solid::Power::self(), &Solid::Power::resumeFromSuspend, ...)
         *   (or resumingFromSuspend?)
         * to be notified when wakeup from hibernation occurs. But can't use it
         * unless we know that this notification is supported by the system!
         */
        /* Re-evaluate the next alarm time every minute, in case the
         * system clock jumps. The most common case when the clock jumps
         * is when a laptop wakes from hibernation. If timers were left to
         * run, they would trigger late by the length of time the system
         * was asleep.
         */
        if (interval > 60000)    // 1 minute
            interval = 60000;
#endif
        ++interval;    // ensure we don't trigger just before the minute boundary
        if (interval > INT_MAX)
            interval = INT_MAX;
        qCDebug(KALARM_LOG) << "KAlarmApp::checkNextDueAlarm:" << nextEvent.id() << "wait" << interval/1000 << "seconds";
        mAlarmTimer->start(static_cast<int>(interval));
    }
}

/******************************************************************************
* Called by the alarm timer when the next alarm is due.
* Also called when the execution queue has finished processing to check for the
* next alarm.
*/
void KAlarmApp::queueAlarmId(const KAEvent& event)
{
    const EventId id(event);
    for (const ActionQEntry& entry : std::as_const(mActionQueue))
    {
        if (entry.action == QueuedAction::Handle  &&  entry.eventId == id)
            return;  // the alarm is already queued
    }
    mActionQueue.enqueue(ActionQEntry(QueuedAction::Handle, id));
}

/******************************************************************************
* Start processing the execution queue.
*/
void KAlarmApp::startProcessQueue(bool evenIfStarted)
{
    if (!mInitialised  ||  evenIfStarted)
    {
        qCDebug(KALARM_LOG) << "KAlarmApp::startProcessQueue";
        mInitialised = true;
        // Process anything already queued.
        QTimer::singleShot(0, this, &KAlarmApp::processQueue);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    }
}

/******************************************************************************
* The main processing loop for KAlarm.
* All KAlarm operations involving opening or updating calendar files are called
* from this loop to ensure that only one operation is active at any one time.
* This precaution is necessary because KAlarm's activities are mostly
* asynchronous, being in response to D-Bus calls from other programs or timer
* events, any of which can be received in the middle of performing another
* operation. If a calendar file is opened or updated while another calendar
* operation is in progress, the program has been observed to hang, or the first
* calendar call has failed with data loss - clearly unacceptable!!
*/
void KAlarmApp::processQueue()
{
    if (mInitialised  &&  !mProcessingQueue)
    {
        qCDebug(KALARM_LOG) << "KAlarmApp::processQueue";
        mProcessingQueue = true;

        // Refresh alarms if that's been queued
        KAlarm::refreshAlarmsIfQueued();

        // Process queued events
        while (!mActionQueue.isEmpty())
        {
            ActionQEntry& entry = mActionQueue.head();

            // If the first action's resource ID is a string, can't process it
            // until its numeric resource ID can be found.
            if (!entry.resourceId.isEmpty())
            {
                if (!Resources::allCreated())
                {
                    // If resource population has timed out, discard all queued events.
                    if (mResourcesTimedOut)
                    {
                        qCCritical(KALARM_LOG) << "Error! Timeout creating calendars";
                        mActionQueue.clear();
                    }
                    break;
                }
                // Convert the resource ID string to the numeric resource ID.
                entry.eventId.setResourceId(EventId::getResourceId(entry.resourceId));
                entry.resourceId.clear();
            }

            // Can't process the first action until its resource has been populated.
            const ResourceId id = entry.eventId.resourceId();
            if ((id <  0 && !Resources::allPopulated())
            ||  (id >= 0 && !Resources::resource(id).isPopulated()))
            {
                // If resource population has timed out, discard all queued events.
                if (mResourcesTimedOut)
                {
                    qCCritical(KALARM_LOG) << "Error! Timeout reading calendars";
                    mActionQueue.clear();
                }
                break;
            }

            // Process the first action in the queue.
            const bool findUniqueId   = int(entry.action) & int(QueuedAction::FindId);
            bool       exitAfter      = int(entry.action) & int(QueuedAction::Exit);
            const bool exitAfterError = int(entry.action) & int(QueuedAction::ErrorExit);
            const bool commandLine    = int(entry.action) & int(QueuedAction::CmdLine);
            const auto action = static_cast<QueuedAction>(int(entry.action) & int(QueuedAction::ActionMask));

            bool ok = true;
            bool inhibit = false;
            if (entry.eventId.isEmpty())
            {
                // It's a new alarm
                switch (action)
                {
                    case QueuedAction::Trigger:
                        if (execAlarm(entry.event, entry.event.firstAlarm()) == (void*)-2)
                            inhibit = true;
                        break;
                    case QueuedAction::Handle:
                    {
                        Resource resource = Resources::destination(CalEvent::ACTIVE, nullptr, Resources::NoResourcePrompt | Resources::UseOnlyResource);
                        if (!resource.isValid())
                        {
                            qCWarning(KALARM_LOG) << "KAlarmApp::processQueue: Error! Cannot create alarm (no default calendar is defined)";

                            if (commandLine)
                            {
                                const QString errmsg = xi18nc("@info:shell", "Cannot create alarm: No default calendar is defined");
                                std::cerr << qPrintable(errmsg) << std::endl;
                            }
                            ok = false;
                        }
                        else
                        {
                            const KAlarm::UpdateResult result = KAlarm::addEvent(entry.event, resource, nullptr, KAlarm::ALLOW_KORG_UPDATE | KAlarm::NO_RESOURCE_PROMPT);
                            if (result >= KAlarm::UPDATE_ERROR)
                            {
//TODO: display error message for command line action, but first ensure that one is returned by addEvent()!
#if 0
                                if (commandLine)
                                {
                                    const QString errmsg = xi18nc("@info:shell", "Error creating alarm");
                                    std::cerr << errmsg.toLocal8Bit().data();
                                }
#endif
                                ok = false;
                            }
                        }
                        if (!ok  &&  exitAfterError)
                            exitAfter = true;
                        break;
                    }
                    case QueuedAction::List:
                    {
                        const QStringList alarms = scheduledAlarmList();
                        for (const QString& alarm : alarms)
                            std::cout << qUtf8Printable(alarm) << std::endl;
                        break;
                    }
                    default:
                        break;
                }
            }
            else
            {
                if (action == QueuedAction::Edit)
                {
                    int editingCmdLineAlarm = mEditingCmdLineAlarm & 3;
                    bool keepQueued = editingCmdLineAlarm <= 1;
                    switch (editingCmdLineAlarm)
                    {
                        case 0:
                            // Initiate editing an alarm specified on the command line.
                            mEditingCmdLineAlarm |= 1;
                            QTimer::singleShot(0, this, &KAlarmApp::slotEditAlarmById);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
                            break;
                        case 1:
                            // Currently editing the alarm.
                            break;
                        case 2:
                            // The edit has completed.
                            mEditingCmdLineAlarm = 0;
                            break;
                        default:
                            break;
                    }
                    if (keepQueued)
                        break;
                }
                else
                {
                    // Trigger the event if it's due.
                    const int result = handleEvent(entry.eventId, action, findUniqueId);
                    if (!result)
                        inhibit = true;
                    else if (result < 0  &&  exitAfter)
                    {
                        CommandOptions::printError(xi18nc("@info:shell", "%1: Event <resource>%2</resource> not found, or not unique", mCommandOption, entry.eventId.eventId()));
                        ok = false;
                    }
                }
            }

            mActionQueue.dequeue();

            if (inhibit)
            {
                // It's a display event which can't be executed because notifications
                // are inhibited. Move it to the inhibited queue until the inhibition
                // is removed.
            }
            else if (exitAfter)
            {
                mProcessingQueue = false;   // don't inhibit processing if there is another instance
                quitIf((ok ? 0 : 1), exitAfterError);
                return;  // quitIf() can sometimes return, despite calling exit()
            }
        }

        // Purge the default archived alarms resource if it's time to do so
        if (mPurgeDaysQueued >= 0)
        {
            KAlarm::purgeArchive(mPurgeDaysQueued);
            mPurgeDaysQueued = -1;
        }

        // Now that the queue has been processed, quit if a quit was queued
        if (mPendingQuit)
        {
            if (quitIf(mPendingQuitCode))
                return;  // quitIf() can sometimes return, despite calling exit()
        }

        mProcessingQueue = false;

        if (!mEditingCmdLineAlarm)
        {
            // Schedule the application to be woken when the next alarm is due
            checkNextDueAlarm();
        }
    }
}

/******************************************************************************
* Called when a repeat-at-login alarm has been added externally.
* Queues the alarm for triggering.
* First, cancel any scheduled reminder or deferral for it, since these will be
* superseded by the new at-login trigger.
*/
void KAlarmApp::atLoginEventAdded(const KAEvent& event)
{
    KAEvent ev = event;
    if (!cancelReminderAndDeferral(ev))
    {
        if (mAlarmsEnabled)
        {
            mActionQueue.enqueue(ActionQEntry(QueuedAction::Handle, EventId(ev)));
            if (mInitialised)
                QTimer::singleShot(0, this, &KAlarmApp::processQueue);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
        }
    }
}

/******************************************************************************
* Called when the system tray main window is closed.
*/
void KAlarmApp::removeWindow(TrayWindow*)
{
    mTrayWindow = nullptr;
}

/******************************************************************************
* Display or close the system tray icon.
*/
bool KAlarmApp::displayTrayIcon(bool show, MainWindow* parent)
{
    qCDebug(KALARM_LOG) << "KAlarmApp::displayTrayIcon";
    static bool creating = false;
    if (show)
    {
        if (!mTrayWindow  &&  !creating)
        {
            if (!QSystemTrayIcon::isSystemTrayAvailable())
                return false;
            if (!MainWindow::count())
            {
                // We have to have at least one main window to act
                // as parent to the system tray icon (even if the
                // window is hidden).
                creating = true;    // prevent main window constructor from creating an additional tray icon
                parent = MainWindow::create();
                creating = false;
            }
            mTrayWindow = new TrayWindow(parent ? parent : MainWindow::firstWindow());
            connect(mTrayWindow, &TrayWindow::deleted, this, &KAlarmApp::trayIconToggled);
            Q_EMIT trayIconToggled();

            if (!checkSystemTray())
                quitIf(0);    // exit the application if there are no open windows
        }
    }
    else
    {
        delete mTrayWindow;
        mTrayWindow = nullptr;
    }
    return true;
}

/******************************************************************************
* Check whether the system tray icon has been housed in the system tray.
*/
bool KAlarmApp::checkSystemTray()
{
    if (!mTrayWindow)
        return true;
    if (QSystemTrayIcon::isSystemTrayAvailable() == mNoSystemTray)
    {
        qCDebug(KALARM_LOG) << "KAlarmApp::checkSystemTray: changed ->" << mNoSystemTray;
        mNoSystemTray = !mNoSystemTray;

        // Store the new setting in the config file, so that if KAlarm exits it will
        // restart with the correct default.
        KConfigGroup config(KSharedConfig::openConfig(), "General");
        config.writeEntry("NoSystemTray", mNoSystemTray);
        config.sync();

        // Update other settings
        slotShowInSystemTrayChanged();
    }
    return !mNoSystemTray;
}

/******************************************************************************
* Return the main window associated with the system tray icon.
*/
MainWindow* KAlarmApp::trayMainWindow() const
{
    return mTrayWindow ? mTrayWindow->assocMainWindow() : nullptr;
}

/******************************************************************************
* Called when the show-in-system-tray preference setting has changed, to show
* or hide the system tray icon.
*/
void KAlarmApp::slotShowInSystemTrayChanged()
{
    const bool newShowInSysTray = wantShowInSystemTray();
    if (newShowInSysTray != mOldShowInSystemTray)
    {
        // The system tray run mode has changed
        ++mActiveCount;         // prevent the application from quitting
        MainWindow* win = mTrayWindow ? mTrayWindow->assocMainWindow() : nullptr;
        delete mTrayWindow;     // remove the system tray icon if it is currently shown
        mTrayWindow = nullptr;
        mOldShowInSystemTray = newShowInSysTray;
        if (newShowInSysTray)
        {
            // Show the system tray icon
            displayTrayIcon(true);
        }
        else
        {
            // Stop showing the system tray icon
            if (win  &&  win->isHidden())
            {
                if (MainWindow::count() > 1)
                    delete win;
                else
                {
                    win->setWindowState(win->windowState() | Qt::WindowMinimized);
                    win->show();
                }
            }
        }
        --mActiveCount;
    }
}

/******************************************************************************
* Called when the start-of-day time preference setting has changed.
* Change alarm times for date-only alarms.
*/
void KAlarmApp::changeStartOfDay()
{
    DateTime::setStartOfDay(Preferences::startOfDay());
    KAEvent::setStartOfDay(Preferences::startOfDay());
    Resources::adjustStartOfDay();
    DisplayCalendar::adjustStartOfDay();
}

/******************************************************************************
* Called when the default alarm message font preference setting has changed.
* Notify KAEvent.
*/
void KAlarmApp::slotMessageFontChanged(const QFont& font)
{
    KAEvent::setDefaultFont(font);
}

/******************************************************************************
* Called when the working time preference settings have changed.
* Notify KAEvent.
*/
void KAlarmApp::slotWorkTimeChanged(const QTime& start, const QTime& end, const QBitArray& days)
{
    KAEvent::setWorkTime(days, start, end, Preferences::timeSpec());
}

/******************************************************************************
* Called when the holiday region preference setting has changed.
* Notify KAEvent.
*/
void KAlarmApp::slotHolidaysChanged(const KHolidays::HolidayRegion& holidays)
{
    KAEvent::setHolidays(holidays);
}

/******************************************************************************
* Called when the date for February 29th recurrences has changed in the
* preferences settings.
*/
void KAlarmApp::slotFeb29TypeChanged(Preferences::Feb29Type type)
{
    KARecurrence::Feb29Type rtype;
    switch (type)
    {
        default:
        case Preferences::Feb29_None:   rtype = KARecurrence::Feb29_None;  break;
        case Preferences::Feb29_Feb28:  rtype = KARecurrence::Feb29_Feb28;  break;
        case Preferences::Feb29_Mar1:   rtype = KARecurrence::Feb29_Mar1;  break;
    }
    KARecurrence::setDefaultFeb29Type(rtype);
}

/******************************************************************************
* Return whether the program is configured to be running in the system tray.
*/
bool KAlarmApp::wantShowInSystemTray() const
{
    return Preferences::showInSystemTray()  &&  QSystemTrayIcon::isSystemTrayAvailable();
}

/******************************************************************************
* Set a timeout for populating resources.
*/
void KAlarmApp::setResourcesTimeout()
{
    QTimer::singleShot(RESOURCES_TIMEOUT * 1000, this, &KAlarmApp::slotResourcesTimeout);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
}

/******************************************************************************
* Called on a timeout to check whether resources have been populated.
* If not, exit the program with code 1.
*/
void KAlarmApp::slotResourcesTimeout()
{
    if (!Resources::allPopulated())
    {
        // Resource population has timed out.
        mResourcesTimedOut = true;
        quitIf(1);
    }
}

/******************************************************************************
* Called when all resources have been created at startup.
* Check whether there are any writable active calendars, and if not, warn the
* user.
* If alarms are being archived, check whether there is a default archived
* calendar, and if not, warn the user.
*/
void KAlarmApp::slotResourcesCreated()
{
    showRestoredWindows();   // display message windows restored at startup.
    checkWritableCalendar();
    checkArchivedCalendar();
    processQueue();
}

/******************************************************************************
* Called when all calendars have been fetched at startup, or calendar migration
* has completed.
* Check whether there are any writable active calendars, and if not, warn the
* user.
*/
void KAlarmApp::checkWritableCalendar()
{
    if (mReadOnly)
        return;    // don't need write access to calendars
    if (!Resources::allCreated()
    ||  !DataModel::isMigrationComplete())
        return;
    static bool done = false;
    if (done)
        return;
    done = true;
    qCDebug(KALARM_LOG) << "KAlarmApp::checkWritableCalendar";

    // Check for, and remove, any duplicate resources, i.e. those which use the
    // same calendar file/directory.
    DataModel::removeDuplicateResources();

    // Find whether there are any writable active alarm calendars
    const bool active = !Resources::enabledResources(CalEvent::ACTIVE, true).isEmpty();
    if (!active)
    {
        qCWarning(KALARM_LOG) << "KAlarmApp::checkWritableCalendar: No writable active calendar";
        KAMessageBox::information(MainWindow::mainMainWindow(),
                                  xi18nc("@info", "Alarms cannot be created or updated, because no writable active alarm calendar is enabled.<nl/><nl/>"
                                                 "To fix this, use <interface>View | Show Calendars</interface> to check or change calendar statuses."),
                                  QString(), QStringLiteral("noWritableCal"));
    }
}

/******************************************************************************
* If alarms are being archived, check whether there is a default archived
* calendar, and if not, warn the user.
*/
void KAlarmApp::checkArchivedCalendar()
{
    static bool done = false;
    if (done)
        return;
    done = true;

    // If alarms are to be archived, check that the default archived alarm
    // calendar is writable.
    if (Preferences::archivedKeepDays())
    {
        Resource standard = Resources::getStandard(CalEvent::ARCHIVED, true);
        if (!standard.isValid())
        {
            // Schedule the display of a user prompt, without holding up
            // other processing.
            QTimer::singleShot(0, this, &KAlarmApp::promptArchivedCalendar);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
        }
    }
}

/******************************************************************************
* Edit an alarm specified on the command line.
*/
void KAlarmApp::slotEditAlarmById()
{
    qCDebug(KALARM_LOG) << "KAlarmApp::slotEditAlarmById";
    ActionQEntry& entry = mActionQueue.head();
    if (!KAlarm::editAlarmById(entry.eventId))
    {
        CommandOptions::printError(xi18nc("@info:shell", "%1: Event <resource>%2</resource> not found, or not editable", mCommandOption, entry.eventId.eventId()));
        mActionQueue.clear();
        quitIf(1);
    }
    else
    {
        createOnlyMainWindow();    // prevent the application from quitting
        if (mEditingCmdLineAlarm & 0x10)
        {
            mRedisplayAlarms = true;
            showRestoredWindows();
        }
        mEditingCmdLineAlarm = 2;  // indicate edit completion
        QTimer::singleShot(0, this, &KAlarmApp::processQueue);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    }
}

/******************************************************************************
* If alarms are being archived, check whether there is a default archived
* calendar, and if not, warn the user.
*/
void KAlarmApp::promptArchivedCalendar()
{
    const bool archived = !Resources::enabledResources(CalEvent::ARCHIVED, true).isEmpty();
    if (archived)
    {
        qCWarning(KALARM_LOG) << "KAlarmApp::checkArchivedCalendar: Archiving, but no writable archived calendar";
        KAMessageBox::information(MainWindow::mainMainWindow(),
                                  xi18nc("@info", "Alarms are configured to be archived, but this is not possible because no writable archived alarm calendar is enabled.<nl/><nl/>"
                                                 "To fix this, use <interface>View | Show Calendars</interface> to check or change calendar statuses."),
                                  QString(), QStringLiteral("noWritableArch"));
    }
    else
    {
        qCWarning(KALARM_LOG) << "KAlarmApp::checkArchivedCalendar: Archiving, but no standard archived calendar";
        KAMessageBox::information(MainWindow::mainMainWindow(),
                                  xi18nc("@info", "Alarms are configured to be archived, but this is not possible because no archived alarm calendar is set as default.<nl/><nl/>"
                                                 "To fix this, use <interface>View | Show Calendars</interface>, select an archived alarms calendar, and check <interface>Use as Default for Archived Alarms</interface>."),
                                  QString(), QStringLiteral("noStandardArch"));
    }
}

/******************************************************************************
* Called when a new resource has been added, to note the possible need to purge
* its old alarms if it is the default archived calendar.
*/
void KAlarmApp::slotResourceAdded(const Resource& resource)
{
    if (resource.alarmTypes() & CalEvent::ARCHIVED)
        mPendingPurges += resource.id();
}

/******************************************************************************
* Called when a resource has been populated, to purge its old alarms if it is
* the default archived calendar.
*/
void KAlarmApp::slotResourcePopulated(const Resource& resource)
{
    if (mPendingPurges.removeAll(resource.id()) > 0)
        purgeNewArchivedDefault(resource);
}

/******************************************************************************
* Called when a new resource has been populated, or when a resource has been
* set as the standard resource for its type.
* If it is the default archived calendar, purge its old alarms if necessary.
*/
void KAlarmApp::purgeNewArchivedDefault(const Resource& resource)
{
    if (Resources::isStandard(resource, CalEvent::ARCHIVED))
    {
        qCDebug(KALARM_LOG) << "KAlarmApp::purgeNewArchivedDefault:" << resource.displayId() << ": standard archived...";
        if (mArchivedPurgeDays >= 0)
            purge(mArchivedPurgeDays);
        else
            setArchivePurgeDays();
    }
}

/******************************************************************************
* Called when the length of time to keep archived alarms changes in KAlarm's
* preferences.
* Set the number of days to keep archived alarms.
* Alarms which are older are purged immediately, and at the start of each day.
*/
void KAlarmApp::setArchivePurgeDays()
{
    const int newDays = Preferences::archivedKeepDays();
    if (newDays != mArchivedPurgeDays)
    {
        const int oldDays = mArchivedPurgeDays;
        mArchivedPurgeDays = newDays;
        if (mArchivedPurgeDays <= 0)
            StartOfDayTimer::disconnect(this);
        if (mArchivedPurgeDays < 0)
            return;   // keep indefinitely, so don't purge
        if (oldDays < 0  ||  mArchivedPurgeDays < oldDays)
        {
            // Alarms are now being kept for less long, so purge them
            purge(mArchivedPurgeDays);
            if (!mArchivedPurgeDays)
                return;   // don't archive any alarms
        }
        // Start the purge timer to expire at the start of the next day
        // (using the user-defined start-of-day time).
        StartOfDayTimer::connect(this, SLOT(slotPurge()));
    }
}

/******************************************************************************
* Purge all archived events from the calendar whose end time is longer ago than
* 'daysToKeep'. All events are deleted if 'daysToKeep' is zero.
*/
void KAlarmApp::purge(int daysToKeep)
{
    if (mPurgeDaysQueued < 0  ||  daysToKeep < mPurgeDaysQueued)
        mPurgeDaysQueued = daysToKeep;

    // Do the purge once any other current operations are completed
    processQueue();
}

/******************************************************************************
* Called when the Freedesktop notifications properties have changed.
* Check whether the inhibited property has changed.
*/
void KAlarmApp::slotFDOPropertiesChanged(const QString& interface,
                                         const QVariantMap& changedProperties,
                                         const QStringList& invalidatedProperties)
{
    Q_UNUSED(interface);     // always "org.freedesktop.Notifications"
    Q_UNUSED(invalidatedProperties);
    const auto it = changedProperties.find(QStringLiteral("Inhibited"));
    if (it != changedProperties.end())
    {
        const bool inhibited = it.value().toBool();
        if (inhibited != mNotificationsInhibited)
        {
            qCDebug(KALARM_LOG) << "KAlarmApp::slotFDOPropertiesChanged: Notifications inhibited ->" << inhibited;
            mNotificationsInhibited = inhibited;
            if (!mNotificationsInhibited)
            {
                showRestoredWindows();   // display message windows restored at startup.
                QTimer::singleShot(0, this, &KAlarmApp::processQueue);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
            }
        }
    }
}

/******************************************************************************
* Output a list of pending alarms, with their next scheduled occurrence.
*/
QStringList KAlarmApp::scheduledAlarmList()
{
    QStringList alarms;
    const QList<KAEvent> events = KAlarm::getSortedActiveEvents(this);
    for (const KAEvent& event : events)
    {
        const KADateTime dateTime = event.nextTrigger(KAEvent::DISPLAY_TRIGGER).effectiveKDateTime().toLocalZone();
        const Resource resource = Resources::resource(event.resourceId());
        QString text(resource.configName() + QLatin1String(":"));
        text += event.id() + QLatin1Char(' ')
             +  dateTime.toString(QStringLiteral("%Y%m%dT%H%M "))
             +  AlarmText::summary(event, 1);
        alarms << text;
    }
    return alarms;
}

/******************************************************************************
* Enable or disable alarm monitoring.
*/
void KAlarmApp::setAlarmsEnabled(bool enabled)
{
    if (enabled != mAlarmsEnabled)
    {
        mAlarmsEnabled = enabled;
        Q_EMIT alarmEnabledToggled(enabled);
        if (!enabled)
#if ENABLE_WAKE_FROM_SUSPEND
            KAlarm::cancelRtcWake(nullptr);
#else
            ;
#endif
        else if (!mProcessingQueue)
            checkNextDueAlarm();
    }
}

/******************************************************************************
* Spread or collect alarm message and error message windows.
*/
void KAlarmApp::spreadWindows(bool spread)
{
    spread = MessageWindow::spread(spread);
    Q_EMIT spreadWindowsToggled(spread);
}

/******************************************************************************
* Called when the spread status of message windows changes.
* Set the 'spread windows' action state.
*/
void KAlarmApp::setSpreadWindowsState(bool spread)
{
    Q_EMIT spreadWindowsToggled(spread);
}

/******************************************************************************
* Check whether the window manager's handling of keyboard focus transfer
* between application windows is broken. This is true for Ubuntu's Unity
* desktop, where MessageWindow windows steal keyboard focus from EditAlarmDlg
* windows.
*/
bool KAlarmApp::windowFocusBroken() const
{
    return mWindowFocusBroken;
}

/******************************************************************************
* Check whether window/keyboard focus currently needs to be fixed manually due
* to the window manager not handling it correctly. This will occur if there are
* both EditAlarmDlg and MessageWindow windows currently active.
*/
bool KAlarmApp::needWindowFocusFix() const
{
    return mWindowFocusBroken && MessageWindow::windowCount(true) && EditAlarmDlg::instanceCount();
}

/******************************************************************************
* Called to schedule a new alarm, either in response to a D-Bus notification or
* to command line options.
* Reply = true unless there was a parameter error or an error opening calendar file.
*/
bool KAlarmApp::scheduleEvent(QueuedAction queuedActionFlags,
                              KAEvent::SubAction action, const QString& name, const QString& text,
                              const KADateTime& dateTime, int lateCancel, KAEvent::Flags flags,
                              const QColor& bg, const QColor& fg, const QFont& font,
                              const QString& audioFile, float audioVolume, int reminderMinutes,
                              const KARecurrence& recurrence, const KCalendarCore::Duration& repeatInterval, int repeatCount,
                              uint mailFromID, const KCalendarCore::Person::List& mailAddresses,
                              const QString& mailSubject, const QStringList& mailAttachments)
{
    if (!dateTime.isValid())
    {
        qCWarning(KALARM_LOG) << "KAlarmApp::scheduleEvent: Error! Invalid time" << text;
        return false;
    }
    const KADateTime now = KADateTime::currentUtcDateTime();
    if (lateCancel  &&  dateTime < now.addSecs(-maxLateness(lateCancel)))
    {
        qCDebug(KALARM_LOG) << "KAlarmApp::scheduleEvent: not executed (late-cancel)" << text;
        return true;               // alarm time was already archived too long ago
    }
    KADateTime alarmTime = dateTime;
    // Round down to the nearest minute to avoid scheduling being messed up
    if (!dateTime.isDateOnly())
        alarmTime.setTime(QTime(alarmTime.time().hour(), alarmTime.time().minute(), 0));

    KAEvent event(alarmTime, name, text, bg, fg, font, action, lateCancel, flags, true);
    if (reminderMinutes)
    {
        const bool onceOnly = flags & KAEvent::REMINDER_ONCE;
        event.setReminder(reminderMinutes, onceOnly);
    }
    if (!audioFile.isEmpty())
        event.setAudioFile(audioFile, audioVolume, -1, 0, (flags & KAEvent::REPEAT_SOUND) ? 0 : -1);
    if (!mailAddresses.isEmpty())
        event.setEmail(mailFromID, mailAddresses, mailSubject, mailAttachments);
    event.setRecurrence(recurrence);
    event.setFirstRecurrence();
    event.setRepetition(Repetition(repeatInterval, repeatCount - 1));
    event.endChanges();
    if (alarmTime <= now)
    {
        // Alarm is due for execution already.
        // First execute it once without adding it to the calendar file.
        qCDebug(KALARM_LOG) << "KAlarmApp::scheduleEvent: executing" << text;
        if (!mInitialised
        ||  execAlarm(event, event.firstAlarm()) == (void*)-2)
            mActionQueue.enqueue(ActionQEntry(event, QueuedAction::Trigger));
        // If it's a recurring alarm, reschedule it for its next occurrence
        if (!event.recurs()
        ||  event.setNextOccurrence(now) == KAEvent::NO_OCCURRENCE)
            return true;
        // It has recurrences in the future
    }

    // Queue the alarm for insertion into the calendar file
    qCDebug(KALARM_LOG) << "KAlarmApp::scheduleEvent: creating new alarm" << text;
    const QueuedAction qaction = static_cast<QueuedAction>(int(QueuedAction::Handle) + int(queuedActionFlags));
    mActionQueue.enqueue(ActionQEntry(event, qaction));
    if (mInitialised)
        QTimer::singleShot(0, this, &KAlarmApp::processQueue);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    return true;
}

/******************************************************************************
* Called in response to a D-Bus request to trigger or cancel an event.
* Optionally display the event. Delete the event from the calendar file and
* from every main window instance.
*/
bool KAlarmApp::dbusHandleEvent(const EventId& eventID, QueuedAction action)
{
    qCDebug(KALARM_LOG) << "KAlarmApp::dbusHandleEvent:" << eventID;
    mActionQueue.append(ActionQEntry(action, eventID));
    if (mInitialised)
        QTimer::singleShot(0, this, &KAlarmApp::processQueue);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    return true;
}

/******************************************************************************
* Called in response to a D-Bus request to list all pending alarms.
*/
QString KAlarmApp::dbusList()
{
    qCDebug(KALARM_LOG) << "KAlarmApp::dbusList";
    return scheduledAlarmList().join(QLatin1Char('\n')) + QLatin1Char('\n');
}

/******************************************************************************
* Either:
* a) Execute the event if it's due, and then delete it if it has no outstanding
*    repetitions.
* b) Delete the event.
* c) Reschedule the event for its next repetition. If none remain, delete it.
* If the event is deleted, it is removed from the calendar file and from every
* main window instance.
* If 'findUniqueId' is true and 'id' does not specify a resource, all resources
* will be searched for the event's unique ID.
* Reply = -1 if event ID not found, or if more than one event with the same ID
*            is found.
*       =  0 if can't trigger display event because notifications are inhibited.
*       =  1 if success.
*/
int KAlarmApp::handleEvent(const EventId& id, QueuedAction action, bool findUniqueId)
{
    Q_ASSERT(!(int(action) & ~int(QueuedAction::ActionMask)));

#if ENABLE_WAKE_FROM_SUSPEND
    // Delete any expired wake-on-suspend config data
    KAlarm::checkRtcWakeConfig();
#endif

    const QString eventID(id.eventId());
    KAEvent event = ResourcesCalendar::event(id, findUniqueId);
    if (!event.isValid())
    {
        if (id.resourceId() != -1)
            qCWarning(KALARM_LOG) << "KAlarmApp::handleEvent: Event ID not found:" << eventID;
        else if (findUniqueId)
            qCWarning(KALARM_LOG) << "KAlarmApp::handleEvent: Event ID not found, or duplicated:" << eventID;
        else
            qCCritical(KALARM_LOG) << "KAlarmApp::handleEvent: No resource ID specified for event:" << eventID;
        return -1;
    }
    switch (action)
    {
        case QueuedAction::Cancel:
        {
            qCDebug(KALARM_LOG) << "KAlarmApp::handleEvent:" << eventID << ", CANCEL";
            Resource resource = Resources::resource(event.resourceId());
            KAlarm::deleteEvent(event, resource, true);
            break;
        }
        case QueuedAction::Trigger:    // handle it if it's due, else execute it regardless
        case QueuedAction::Handle:     // handle it if it's due
        {
            const KADateTime now = KADateTime::currentUtcDateTime();
            qCDebug(KALARM_LOG) << "KAlarmApp::handleEvent:" << eventID << "," << (action==QueuedAction::Trigger?"TRIGGER:":"HANDLE:") << qPrintable(now.qDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm"))) << "UTC";
            bool updateCalAndDisplay = false;
            bool alarmToExecuteValid = false;
            KAAlarm alarmToExecute;
            bool restart = false;
            // Check all the alarms in turn.
            // Note that the main alarm is fetched before any other alarms.
            for (KAAlarm alarm = event.firstAlarm();
                 alarm.isValid();
                 alarm = (restart ? event.firstAlarm() : event.nextAlarm(alarm)), restart = false)
            {
                // Check if the alarm is due yet.
                const KADateTime nextDT = alarm.dateTime(true).effectiveKDateTime();
                const int secs = nextDT.secsTo(now);
                if (secs < 0)
                {
                    // The alarm appears to be in the future.
                    // Check if it's an invalid local time during a daylight
                    // saving time shift, which has actually passed.
                    if (alarm.dateTime().timeSpec() != KADateTime::LocalZone
                    ||  nextDT > now.toTimeSpec(KADateTime::LocalZone))
                    {
                        // This alarm is definitely not due yet
                        qCDebug(KALARM_LOG) << "KAlarmApp::handleEvent: Alarm" << alarm.type() << "at" << nextDT.qDateTime() << ": not due";
                        continue;
                    }
                }
                bool reschedule = false;
                bool rescheduleWork = false;
                if ((event.workTimeOnly() || event.holidaysExcluded())  &&  !alarm.deferred())
                {
                    // The alarm is restricted to working hours and/or non-holidays
                    // (apart from deferrals). This needs to be re-evaluated every
                    // time it triggers, since working hours could change.
                    if (alarm.dateTime().isDateOnly())
                    {
                        KADateTime dt(nextDT);
                        dt.setDateOnly(true);
                        reschedule = event.excludedByWorkTimeOrHoliday(dt);
                    }
                    else
                        reschedule = event.excludedByWorkTimeOrHoliday(nextDT);
                    rescheduleWork = reschedule;
                    if (reschedule)
                        qCDebug(KALARM_LOG) << "KAlarmApp::handleEvent: Alarm" << alarm.type() << "at" << nextDT.qDateTime() << ": not during working hours";
                }
                if (!reschedule  &&  alarm.repeatAtLogin())
                {
                    // Alarm is to be displayed at every login.
                    qCDebug(KALARM_LOG) << "KAlarmApp::handleEvent: REPEAT_AT_LOGIN";
                    // Check if the main alarm is already being displayed.
                    // (We don't want to display both at the same time.)
                    if (alarmToExecute.isValid())
                        continue;

                    // Set the time to display if it's a display alarm
                    alarm.setTime(now);
                }
                if (!reschedule  &&  event.lateCancel())
                {
                    // Alarm is due, and it is to be cancelled if too late.
                    qCDebug(KALARM_LOG) << "KAlarmApp::handleEvent: LATE_CANCEL";
                    bool cancel = false;
                    if (alarm.dateTime().isDateOnly())
                    {
                        // The alarm has no time, so cancel it if its date is too far past
                        const int maxlate = event.lateCancel() / 1440;    // maximum lateness in days
                        KADateTime limit(DateTime(nextDT.addDays(maxlate + 1)).effectiveKDateTime());
                        if (now >= limit)
                        {
                            // It's too late to display the scheduled occurrence.
                            // Find the last previous occurrence of the alarm.
                            DateTime next;
                            const KAEvent::OccurType type = event.previousOccurrence(now, next, true);
                            switch (type & ~KAEvent::OCCURRENCE_REPEAT)
                            {
                                case KAEvent::FIRST_OR_ONLY_OCCURRENCE:
                                case KAEvent::RECURRENCE_DATE:
                                case KAEvent::RECURRENCE_DATE_TIME:
                                case KAEvent::LAST_RECURRENCE:
                                    limit.setDate(next.date().addDays(maxlate + 1));
                                    if (now >= limit)
                                    {
                                        if (type == KAEvent::LAST_RECURRENCE
                                        ||  (type == KAEvent::FIRST_OR_ONLY_OCCURRENCE && !event.recurs()))
                                            cancel = true;   // last occurrence (and there are no repetitions)
                                        else
                                            reschedule = true;
                                    }
                                    break;
                                case KAEvent::NO_OCCURRENCE:
                                default:
                                    reschedule = true;
                                    break;
                            }
                        }
                    }
                    else
                    {
                        // The alarm is timed. Allow it to be the permitted amount late before cancelling it.
                        const int maxlate = maxLateness(event.lateCancel());
                        if (secs > maxlate)
                        {
                            // It's over the maximum interval late.
                            // Find the most recent occurrence of the alarm.
                            DateTime next;
                            const KAEvent::OccurType type = event.previousOccurrence(now, next, true);
                            switch (type & ~KAEvent::OCCURRENCE_REPEAT)
                            {
                                case KAEvent::FIRST_OR_ONLY_OCCURRENCE:
                                case KAEvent::RECURRENCE_DATE:
                                case KAEvent::RECURRENCE_DATE_TIME:
                                case KAEvent::LAST_RECURRENCE:
                                    if (next.effectiveKDateTime().secsTo(now) > maxlate)
                                    {
                                        if (type == KAEvent::LAST_RECURRENCE
                                        ||  (type == KAEvent::FIRST_OR_ONLY_OCCURRENCE && !event.recurs()))
                                            cancel = true;   // last occurrence (and there are no repetitions)
                                        else
                                            reschedule = true;
                                    }
                                    break;
                                case KAEvent::NO_OCCURRENCE:
                                default:
                                    reschedule = true;
                                    break;
                            }
                        }
                    }

                    if (cancel)
                    {
                        // All recurrences are finished, so cancel the event
                        event.setArchive();
                        if (cancelAlarm(event, alarm.type(), false))
                            return 1;   // event has been deleted
                        updateCalAndDisplay = true;
                        continue;
                    }
                }
                if (reschedule)
                {
                    // The latest repetition was too long ago, so schedule the next one
                    switch (rescheduleAlarm(event, alarm, false, (rescheduleWork ? nextDT : KADateTime())))
                    {
                        case 1:
                            // A working-time-only alarm has been rescheduled and the
                            // rescheduled time is already due. Start processing the
                            // event again.
                            alarmToExecuteValid = false;
                            restart = true;
                            break;
                        case -1:
                            return 1;   // event has been deleted
                        default:
                            break;
                    }
                    updateCalAndDisplay = true;
                    continue;
                }
                if (!alarmToExecuteValid)
                {
                    qCDebug(KALARM_LOG) << "KAlarmApp::handleEvent: Alarm" << alarm.type() << ": execute";
                    alarmToExecute = alarm;             // note the alarm to be displayed
                    alarmToExecuteValid = true;         // only trigger one alarm for the event
                }
                else
                    qCDebug(KALARM_LOG) << "KAlarmApp::handleEvent: Alarm" << alarm.type() << ": skip";
            }

            // If there is an alarm to execute, do this last after rescheduling/cancelling
            // any others. This ensures that the updated event is only saved once to the calendar.
            if (alarmToExecute.isValid())
            {
                if (execAlarm(event, alarmToExecute, Reschedule | (alarmToExecute.repeatAtLogin() ? NoExecFlag : AllowDefer)) == (void*)-2)
                    return 0;    // display alarm, but notifications are inhibited
            }
            else
            {
                if (action == QueuedAction::Trigger)
                {
                    // The alarm is to be executed regardless of whether it's due.
                    // Only trigger one alarm from the event - we don't want multiple
                    // identical messages, for example.
                    const KAAlarm alarm = event.firstAlarm();
                    if (alarm.isValid())
                    {
                        if (execAlarm(event, alarm) == (void*)-2)
                            return 0;    // display alarm, but notifications are inhibited
                    }
                }
                if (updateCalAndDisplay)
                    KAlarm::updateEvent(event);     // update the window lists and calendar file
                else if (action != QueuedAction::Trigger) { qCDebug(KALARM_LOG) << "KAlarmApp::handleEvent: No action"; }
            }
            break;
        }
        default:
            break;
    }
    return 1;
}

/******************************************************************************
* Called when an alarm action has completed, to perform any post-alarm actions.
*/
void KAlarmApp::alarmCompleted(const KAEvent& event)
{
    if (!event.postAction().isEmpty())
    {
        // doShellCommand() will error if the user is not authorised to run
        // shell commands.
        const QString command = event.postAction();
        qCDebug(KALARM_LOG) << "KAlarmApp::alarmCompleted:" << event.id() << ":" << command;
        doShellCommand(command, event, nullptr, ProcData::POST_ACTION);
    }
}

/******************************************************************************
* Reschedule the alarm for its next recurrence after now. If none remain,
* delete it.  If the alarm is deleted and it is the last alarm for its event,
* the event is removed from the calendar file and from every main window
* instance.
* If 'nextDt' is valid, the event is rescheduled for the next non-working
* time occurrence after that.
* Reply = 1 if 'nextDt' is valid and the rescheduled event is already due
*       = -1 if the event has been deleted
*       = 0 otherwise.
*/
int KAlarmApp::rescheduleAlarm(KAEvent& event, const KAAlarm& alarm, bool updateCalAndDisplay, const KADateTime& nextDt)
{
    qCDebug(KALARM_LOG) << "KAlarmApp::rescheduleAlarm: Alarm type:" << alarm.type();
    int reply = 0;
    bool update = false;
    event.startChanges();
    if (alarm.repeatAtLogin())
    {
        // Leave an alarm which repeats at every login until its main alarm triggers
        if (!event.reminderActive()  &&  event.reminderMinutes() < 0)
        {
            // Executing an at-login alarm: first schedule the reminder
            // which occurs AFTER the main alarm.
            event.activateReminderAfter(KADateTime::currentUtcDateTime());
        }
        // Repeat-at-login alarms are usually unchanged after triggering.
        // Ensure that the archive flag (which was set in execAlarm()) is saved.
        update = true;
    }
    else if (alarm.isReminder()  ||  alarm.deferred())
    {
        // It's a reminder alarm or an extra deferred alarm, so delete it
        event.removeExpiredAlarm(alarm.type());
        update = true;
    }
    else
    {
        // Reschedule the alarm for its next occurrence.
        bool cancelled = false;
        DateTime last = event.mainDateTime(false);   // note this trigger time
        if (last != event.mainDateTime(true))
            last = DateTime();                       // but ignore sub-repetition triggers
        bool next = nextDt.isValid();
        KADateTime next_dt = nextDt;
        const KADateTime now = KADateTime::currentUtcDateTime();
        do
        {
            const KAEvent::OccurType type = event.setNextOccurrence(next ? next_dt : now);
            switch (type)
            {
                case KAEvent::NO_OCCURRENCE:
                    // All repetitions are finished, so cancel the event
                    qCDebug(KALARM_LOG) << "KAlarmApp::rescheduleAlarm: No occurrence";
                    if (event.reminderMinutes() < 0  &&  last.isValid()
                    &&  alarm.type() != KAAlarm::AT_LOGIN_ALARM  &&  !event.mainExpired())
                    {
                        // Set the reminder which is now due after the last main alarm trigger.
                        // Note that at-login reminders are scheduled in execAlarm().
                        event.activateReminderAfter(last);
                        updateCalAndDisplay = true;
                    }
                    if (cancelAlarm(event, alarm.type(), updateCalAndDisplay))
                        return -1;
                    break;
                default:
                    if (!(type & KAEvent::OCCURRENCE_REPEAT))
                        break;
                    // Next occurrence is a repeat, so fall through to recurrence handling
                    Q_FALLTHROUGH();
                case KAEvent::RECURRENCE_DATE:
                case KAEvent::RECURRENCE_DATE_TIME:
                case KAEvent::LAST_RECURRENCE:
                    // The event is due by now and repetitions still remain, so rewrite the event
                    if (updateCalAndDisplay)
                        update = true;
                    break;
                case KAEvent::FIRST_OR_ONLY_OCCURRENCE:
                    // The first occurrence is still due?!?, so don't do anything
                    break;
            }
            if (cancelled)
                break;
            if (event.deferred())
            {
                // Just in case there's also a deferred alarm, ensure it's removed
                event.removeExpiredAlarm(KAAlarm::DEFERRED_ALARM);
                update = true;
            }
            if (next)
            {
                // The alarm is restricted to working hours and/or non-holidays.
                // Check if the calculated next time is valid.
                next_dt = event.mainDateTime(true).effectiveKDateTime();
                if (event.mainDateTime(false).isDateOnly())
                {
                    KADateTime dt(next_dt);
                    dt.setDateOnly(true);
                    next = event.excludedByWorkTimeOrHoliday(dt);
                }
                else
                    next = event.excludedByWorkTimeOrHoliday(next_dt);
            }
        } while (next && next_dt <= now);
        reply = (!cancelled && next_dt.isValid() && (next_dt <= now)) ? 1 : 0;

        if (event.reminderMinutes() < 0  &&  last.isValid()
        &&  alarm.type() != KAAlarm::AT_LOGIN_ALARM)
        {
            // Set the reminder which is now due after the last main alarm trigger.
            // Note that at-login reminders are scheduled in execAlarm().
            event.activateReminderAfter(last);
        }
    }
    event.endChanges();
    if (update)
        KAlarm::updateEvent(event, nullptr, true, false);   // update the window lists and calendar file
    return reply;
}

/******************************************************************************
* Delete the alarm. If it is the last alarm for its event, the event is removed
* from the calendar file and from every main window instance.
* Reply = true if event has been deleted.
*/
bool KAlarmApp::cancelAlarm(KAEvent& event, KAAlarm::Type alarmType, bool updateCalAndDisplay)
{
    qCDebug(KALARM_LOG) << "KAlarmApp::cancelAlarm";
    if (alarmType == KAAlarm::MAIN_ALARM  &&  !event.displaying()  &&  event.toBeArchived())
    {
        // The event is being deleted. Save it in the archived resource first.
        Resource resource;
        KAEvent ev(event);
        KAlarm::addArchivedEvent(ev, resource);
    }
    event.removeExpiredAlarm(alarmType);
    if (!event.alarmCount())
    {
        // If it's a command alarm being executed, mark it as deleted
        ProcData* pd = findCommandProcess(event.id());
        if (pd)
            pd->eventDeleted = true;

        // Delete it
        Resource resource;
        KAlarm::deleteEvent(event, resource, false);
        return true;
    }
    if (updateCalAndDisplay)
        KAlarm::updateEvent(event);    // update the window lists and calendar file
    return false;
}

/******************************************************************************
* Cancel any reminder or deferred alarms in an repeat-at-login event.
* This should be called when the event is first loaded.
* If there are no more alarms left in the event, the event is removed from the
* calendar file and from every main window instance.
* Reply = true if event has been deleted.
*/
bool KAlarmApp::cancelReminderAndDeferral(KAEvent& event)
{
    return cancelAlarm(event, KAAlarm::REMINDER_ALARM, false)
       ||  cancelAlarm(event, KAAlarm::DEFERRED_REMINDER_ALARM, false)
       ||  cancelAlarm(event, KAAlarm::DEFERRED_ALARM, true);
}

/******************************************************************************
* Execute an alarm by displaying its message or file, or executing its command.
* Reply = ShellProcess instance if a command alarm
*       = MessageWindow if an audio alarm
*       != null if successful
*       = -1 if execution has not completed
*       = -2 if can't execute display event because notifications are inhibited.
*       = null if the alarm is disabled, or if an error message was output.
*/
void* KAlarmApp::execAlarm(KAEvent& event, const KAAlarm& alarm, ExecAlarmFlags flags)
{
    if (!mAlarmsEnabled  ||  !event.enabled())
    {
        // The event (or all events) is disabled
        qCDebug(KALARM_LOG) << "KAlarmApp::execAlarm:" << event.id() << ": disabled";
        if (flags & Reschedule)
            rescheduleAlarm(event, alarm, true);
        return nullptr;
    }

    if (mNotificationsInhibited  &&  !(flags & NoNotifyInhibit)
    &&  (event.actionTypes() & KAEvent::ACT_DISPLAY))
    {
        // It's a display event and notifications are inhibited.
        qCDebug(KALARM_LOG) << "KAlarmApp::execAlarm:" << event.id() << ": notifications inhibited";
        return (void*)-2;
    }

    void* result = (void*)1;
    event.setArchive();

    switch (alarm.action())
    {
        case KAAlarm::COMMAND:
            if (!event.commandDisplay())
            {
                // execCommandAlarm() will error if the user is not authorised
                // to run shell commands.
                result = execCommandAlarm(event, alarm, flags & NoRecordCmdError);
                if (flags & Reschedule)
                    rescheduleAlarm(event, alarm, true);
                break;
            }
            Q_FALLTHROUGH();   // fall through to MESSAGE
        case KAAlarm::MESSAGE:
        case KAAlarm::FILE:
        {
            // Display a message, file or command output, provided that the same event
            // isn't already being displayed
            MessageDisplay* disp = MessageDisplay::findEvent(EventId(event));
            // Find if we're changing a reminder message to the real message
            const bool reminder = (alarm.type() & KAAlarm::REMINDER_ALARM);
            const bool replaceReminder = !reminder && disp && (disp->alarmType() & KAAlarm::REMINDER_ALARM);
            if (!reminder
            &&  (!event.deferred() || (event.extraActionOptions() & KAEvent::ExecPreActOnDeferral))
            &&  (replaceReminder || !disp)  &&  !(flags & NoPreAction)
            &&  !event.preAction().isEmpty())
            {
                // It's not a reminder alarm, and it's not a deferred alarm unless the
                // pre-alarm action applies to deferred alarms, and there is no message
                // window (other than a reminder window) currently displayed for this
                // alarm, and we need to execute a command before displaying the new window.
                //
                // NOTE: The pre-action is not executed for a recurring alarm if an
                // alarm message window for a previous occurrence is still visible.
                // Check whether the command is already being executed for this alarm.
                for (const ProcData* pd : std::as_const(mCommandProcesses))
                {
                    if (pd->event->id() == event.id()  &&  (pd->flags & ProcData::PRE_ACTION))
                    {
                        qCDebug(KALARM_LOG) << "KAlarmApp::execAlarm: Already executing pre-DISPLAY command";
                        return pd->process;   // already executing - don't duplicate the action
                    }
                }

                // doShellCommand() will error if the user is not authorised to run
                // shell commands.
                const QString command = event.preAction();
                qCDebug(KALARM_LOG) << "KAlarmApp::execAlarm: Pre-DISPLAY command:" << command;
                const int pdFlags = (flags & Reschedule ? ProcData::RESCHEDULE : 0) | (flags & AllowDefer ? ProcData::ALLOW_DEFER : 0);
                if (doShellCommand(command, event, &alarm, (pdFlags | ProcData::PRE_ACTION)))
                {
                    ResourcesCalendar::setAlarmPending(event);
                    return result;     // display the message after the command completes
                }
                // Error executing command
                if (event.extraActionOptions() & KAEvent::CancelOnPreActError)
                {
                    // Cancel the rest of the alarm execution
                    qCDebug(KALARM_LOG) << "KAlarmApp::execAlarm:" << event.id() << ": pre-action failed: cancelled";
                    if (flags & Reschedule)
                        rescheduleAlarm(event, alarm, true);
                    return nullptr;
                }
                // Display the message even though it failed
            }

            if (!disp)
            {
                // There isn't already a message for this event
                const int mdFlags = (flags & Reschedule ? 0 : MessageDisplay::NoReschedule)
                                  | (flags & AllowDefer ? 0 : MessageDisplay::NoDefer)
                                  | (flags & NoRecordCmdError ? MessageDisplay::NoRecordCmdError : 0);
                MessageDisplay::create(event, alarm, mdFlags)->showDisplay();
            }
            else if (replaceReminder)
            {
                // The caption needs to be changed from "Reminder" to "Message"
                disp->cancelReminder(event, alarm);
            }
            else if (!disp->hasDefer() && event.repeatAtLogin() && !alarm.repeatAtLogin())
            {
                // It's a repeat-at-login message with no Defer button,
                // which has now reached its final trigger time and needs
                // to be replaced with a new message.
                disp->showDefer();
                disp->showDateTime(event, alarm);
            }
            else
            {
                // Use the existing message window
            }
            if (disp)
            {
                // Raise the existing message window and replay any sound
                disp->repeat(alarm);    // N.B. this reschedules the alarm
            }
            break;
        }
        case KAAlarm::EMAIL:
        {
            qCDebug(KALARM_LOG) << "KAlarmApp::execAlarm: EMAIL to:" << event.emailAddresses(QStringLiteral(","));
            QStringList errmsgs;
            MailSend::JobData data(event, alarm, flags & Reschedule, flags & (Reschedule | AllowDefer));
            data.queued = true;
            int ans = KAMail::send(data, errmsgs);
            if (ans)
            {
                // The email has either been sent or failed - not queued
                if (ans < 0)
                    result = nullptr;  // failure
                data.queued = false;
                emailSent(data, errmsgs, (ans > 0));
            }
            else
            {
                result = (void*)-1;   // email has been queued
            }
            if (flags & Reschedule)
                rescheduleAlarm(event, alarm, true);
            break;
        }
        case KAAlarm::AUDIO:
        {
            // Play the sound, provided that the same event
            // isn't already playing
            MessageDisplay* disp = MessageDisplay::findEvent(EventId(event));
            if (!disp)
            {
                // There isn't already a message for this event.
                const int mdFlags = (flags & Reschedule ? 0 : MessageDisplay::NoReschedule) | MessageDisplay::AlwaysHide;
                event.setNotify(false);   // can't use notification system if audio only
                disp = MessageDisplay::create(event, alarm, mdFlags);
            }
            else
            {
                // There's an existing message window: replay the sound
                disp->repeat(alarm);    // N.B. this reschedules the alarm
            }
            return dynamic_cast<MessageWindow*>(disp);
        }
        default:
            return nullptr;
    }
    return result;
}

/******************************************************************************
* Called when sending an email has completed.
*/
void KAlarmApp::emailSent(const MailSend::JobData& data, const QStringList& errmsgs, bool copyerr)
{
    if (!errmsgs.isEmpty())
    {
        // Some error occurred, although the email may have been sent successfully
        if (errmsgs.count() > 1)
            qCDebug(KALARM_LOG) << "KAlarmApp::emailSent:" << (copyerr ? "Copy error:" : "Failed:") << errmsgs[1];
        MessageDisplay::showError(data.event, data.alarm.dateTime(), errmsgs);
    }
    else if (data.queued)
        Q_EMIT execAlarmSuccess();
}

/******************************************************************************
* Execute the command specified in a command alarm.
* To connect to the output ready signals of the process, specify a slot to be
* called by supplying 'receiver' and 'slot' parameters.
*/
ShellProcess* KAlarmApp::execCommandAlarm(const KAEvent& event, const KAAlarm& alarm, bool noRecordError,
                                          QObject* receiver, const char* slotOutput, const char* methodExited)
{
    // doShellCommand() will error if the user is not authorised to run
    // shell commands.
    const int flags = (event.commandXterm()   ? ProcData::EXEC_IN_XTERM : 0)
                    | (event.commandDisplay() ? ProcData::DISP_OUTPUT : 0)
                    | (noRecordError          ? ProcData::NO_RECORD_ERROR : 0);
    const QString command = event.cleanText();
    if (event.commandScript())
    {
        // Store the command script in a temporary file for execution
        qCDebug(KALARM_LOG) << "KAlarmApp::execCommandAlarm: Script";
        const QString tmpfile = createTempScriptFile(command, false, event, alarm);
        if (tmpfile.isEmpty())
        {
            setEventCommandError(event, KAEvent::CMD_ERROR);
            return nullptr;
        }
        return doShellCommand(tmpfile, event, &alarm, (flags | ProcData::TEMP_FILE), receiver, slotOutput, methodExited);
    }
    else
    {
        qCDebug(KALARM_LOG) << "KAlarmApp::execCommandAlarm:" << command;
        return doShellCommand(command, event, &alarm, flags, receiver, slotOutput, methodExited);
    }
}

/******************************************************************************
* Execute a shell command line specified by an alarm.
* If the PRE_ACTION bit of 'flags' is set, the alarm will be executed via
* execAlarm() once the command completes, the execAlarm() parameters being
* derived from the remaining bits in 'flags'.
* 'flags' must contain the bit PRE_ACTION or POST_ACTION if and only if it is
* a pre- or post-alarm action respectively.
* To connect to the exited signal of the process, specify the name of a method
* to be called by supplying 'receiver' and 'methodExited' parameters.
* To connect to the output ready signals of the process, specify a slot to be
* called by supplying 'receiver' and 'slotOutput' parameters.
*
* Note that if shell access is not authorised, the attempt to run the command
* will be errored.
*
* Reply = process which has been started, or null if a process couldn't be started.
*/
ShellProcess* KAlarmApp::doShellCommand(const QString& command, const KAEvent& event, const KAAlarm* alarm, int flags, QObject* receiver, const char* slotOutput, const char* methodExited)
{
    qCDebug(KALARM_LOG) << "KAlarmApp::doShellCommand:" << command << "," << event.id();
    QIODevice::OpenMode mode = QIODevice::WriteOnly;
    QString cmd;
    QString tmpXtermFile;
    if (flags & ProcData::EXEC_IN_XTERM)
    {
        // Execute the command in a terminal window.
        cmd = composeXTermCommand(command, event, alarm, flags, tmpXtermFile);
        if (cmd.isEmpty())
        {
            qCWarning(KALARM_LOG) << "KAlarmApp::doShellCommand: Command failed (no terminal selected)";
            const QStringList errors{i18nc("@info", "Failed to execute command\n(no terminal selected for command alarms)")};
            commandErrorMsg(nullptr, event, alarm, flags, errors);
            return nullptr;
        }
    }
    else
    {
        cmd = command;
        mode = QIODevice::ReadWrite;
    }

    ProcData* pd = nullptr;
    ShellProcess* proc = nullptr;
    if (!cmd.isEmpty())
    {
        // Use ShellProcess, which automatically checks whether the user is
        // authorised to run shell commands.
        proc = new ShellProcess(cmd);
        proc->setEnv(QStringLiteral("KALARM_UID"), event.id(), true);
        proc->setOutputChannelMode(KProcess::MergedChannels);   // combine stdout & stderr
        connect(proc, &ShellProcess::shellExited, this, &KAlarmApp::slotCommandExited);
        if ((flags & ProcData::DISP_OUTPUT)  &&  receiver && slotOutput)
        {
            connect(proc, SIGNAL(receivedStdout(ShellProcess*)), receiver, slotOutput);
            connect(proc, SIGNAL(receivedStderr(ShellProcess*)), receiver, slotOutput);
        }
        if (mode == QIODevice::ReadWrite  &&  !event.logFile().isEmpty())
        {
            // Output is to be appended to a log file.
            // Set up a logging process to write the command's output to.
            QString heading;
            if (alarm  &&  alarm->dateTime().isValid())
            {
                const QString dateTime = alarm->dateTime().formatLocale();
                heading = QStringLiteral("\n******* KAlarm %1 *******\n").arg(dateTime);
            }
            else
                heading = QStringLiteral("\n******* KAlarm *******\n");
            QFile logfile(event.logFile());
            if (logfile.open(QIODevice::Append | QIODevice::Text))
            {
                QTextStream out(&logfile);
                out << heading;
                logfile.close();
            }
            proc->setStandardOutputFile(event.logFile(), QIODevice::Append);
        }
        pd = new ProcData(proc, new KAEvent(event), (alarm ? new KAAlarm(*alarm) : nullptr), flags);
        if (flags & ProcData::TEMP_FILE)
            pd->tempFiles += command;
        if (!tmpXtermFile.isEmpty())
            pd->tempFiles += tmpXtermFile;
        if (receiver && methodExited)
        {
            pd->exitReceiver = receiver;
            pd->exitMethod   = methodExited;
        }
        mCommandProcesses.append(pd);
        if (proc->start(mode))
            return proc;
    }

    // Error executing command - report it
    qCWarning(KALARM_LOG) << "KAlarmApp::doShellCommand: Command failed to start";
    commandErrorMsg(proc, event, alarm, flags);
    if (pd)
    {
        mCommandProcesses.removeAt(mCommandProcesses.indexOf(pd));
        delete pd;
    }
    return nullptr;
}

/******************************************************************************
* Compose a command line to execute the given command in a terminal window.
* 'tempScriptFile' receives the name of a temporary script file which is
* invoked by the command line, if applicable.
* Reply = command line, or empty string if error.
*/
QString KAlarmApp::composeXTermCommand(const QString& command, const KAEvent& event, const KAAlarm* alarm, int flags, QString& tempScriptFile) const
{
    qCDebug(KALARM_LOG) << "KAlarmApp::composeXTermCommand:" << command << "," << event.id();
    tempScriptFile.clear();
    QString cmd = Preferences::cmdXTermCommand();
    if (cmd.isEmpty())
        return {};   // no terminal application is configured
    cmd.replace(QLatin1String("%t"), KAboutData::applicationData().displayName());  // set the terminal window title
    if (cmd.indexOf(QLatin1String("%C")) >= 0)
    {
        // Execute the command from a temporary script file
        if (flags & ProcData::TEMP_FILE)
            cmd.replace(QLatin1String("%C"), command);    // the command is already calling a temporary file
        else
        {
            tempScriptFile = createTempScriptFile(command, true, event, *alarm);
            if (tempScriptFile.isEmpty())
                return {};
            cmd.replace(QLatin1String("%C"), tempScriptFile);    // %C indicates where to insert the command
        }
    }
    else if (cmd.indexOf(QLatin1String("%W")) >= 0)
    {
        // Execute the command from a temporary script file,
        // with a sleep after the command is executed
        tempScriptFile = createTempScriptFile(command + QLatin1String("\nsleep 86400\n"), true, event, *alarm);
        if (tempScriptFile.isEmpty())
            return {};
        cmd.replace(QLatin1String("%W"), tempScriptFile);    // %w indicates where to insert the command
    }
    else if (cmd.indexOf(QLatin1String("%w")) >= 0)
    {
        // Append a sleep to the command.
        // Quote the command in case it contains characters such as [>|;].
        const QString exec = KShell::quoteArg(command + QLatin1String("; sleep 86400"));
        cmd.replace(QLatin1String("%w"), exec);    // %w indicates where to insert the command string
    }
    else
    {
        // Set the command to execute.
        // Put it in quotes in case it contains characters such as [>|;].
        const QString exec = KShell::quoteArg(command);
        if (cmd.indexOf(QLatin1String("%c")) >= 0)
            cmd.replace(QLatin1String("%c"), exec);    // %c indicates where to insert the command string
        else
            cmd.append(exec);           // otherwise, simply append the command string
    }
    return cmd;
}

/******************************************************************************
* Create a temporary script file containing the specified command string.
* Reply = path of temporary file, or null string if error.
*/
QString KAlarmApp::createTempScriptFile(const QString& command, bool insertShell, const KAEvent& event, const KAAlarm& alarm) const
{
    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(false);     // don't delete file when it is destructed
    if (!tmpFile.open())
        qCCritical(KALARM_LOG) << "Unable to create a temporary script file";
    else
    {
        tmpFile.setPermissions(QFile::ReadUser | QFile::WriteUser | QFile::ExeUser);
        QTextStream stream(&tmpFile);
        if (insertShell)
            stream << "#!" << ShellProcess::shellPath() << "\n";
        stream << command;
        stream.flush();
        if (tmpFile.error() != QFile::NoError)
            qCCritical(KALARM_LOG) << "Error" << tmpFile.errorString() << " writing to temporary script file";
        else
            return tmpFile.fileName();
    }

    const QStringList errmsgs(i18nc("@info", "Error creating temporary script file"));
    MessageDisplay::showError(event, alarm.dateTime(), errmsgs, QStringLiteral("Script"));
    return {};
}

/******************************************************************************
* Called when a command alarm's execution completes.
*/
void KAlarmApp::slotCommandExited(ShellProcess* proc)
{
    qCDebug(KALARM_LOG) << "KAlarmApp::slotCommandExited";
    // Find this command in the command list
    for (int i = 0, end = mCommandProcesses.count();  i < end;  ++i)
    {
        ProcData* pd = mCommandProcesses.at(i);
        if (pd->process == proc)
        {
            // Found the command. Check its exit status.
            bool executeAlarm = pd->preAction();
            const ShellProcess::Status status = proc->status();
            if (status == ShellProcess::SUCCESS  &&  !proc->exitCode())
            {
                qCDebug(KALARM_LOG) << "KAlarmApp::slotCommandExited:" << pd->event->id() << ": SUCCESS";
                clearEventCommandError(*pd->event, pd->preAction() ? KAEvent::CMD_ERROR_PRE
                                                 : pd->postAction() ? KAEvent::CMD_ERROR_POST
                                                 : KAEvent::CMD_ERROR);
            }
            else
            {
                QString errmsg = proc->errorMessage();
                if (status == ShellProcess::SUCCESS  ||  status == ShellProcess::NOT_FOUND)
                    qCWarning(KALARM_LOG) << "KAlarmApp::slotCommandExited:" << pd->event->id() << ":" << errmsg << "exit status =" << status << ", code =" << proc->exitCode();
                else
                    qCWarning(KALARM_LOG) << "KAlarmApp::slotCommandExited:" << pd->event->id() << ":" << errmsg << "exit status =" << status;
                if (pd->messageBoxParent)
                {
                    // Close the existing informational KMessageBox for this process
                    const QList<QDialog*> dialogs = pd->messageBoxParent->findChildren<QDialog*>();
                    if (!dialogs.isEmpty())
                        delete dialogs[0];
                    setEventCommandError(*pd->event, pd->preAction() ? KAEvent::CMD_ERROR_PRE
                                                   : pd->postAction() ? KAEvent::CMD_ERROR_POST
                                                   : KAEvent::CMD_ERROR);
                    if (!pd->tempFile())
                    {
                        errmsg += QLatin1Char('\n');
                        errmsg += proc->command();
                    }
                    KAMessageBox::error(pd->messageBoxParent, errmsg);
                }
                else
                    commandErrorMsg(proc, *pd->event, pd->alarm, pd->flags);

                if (executeAlarm
                &&  (pd->event->extraActionOptions() & KAEvent::CancelOnPreActError))
                {
                    qCDebug(KALARM_LOG) << "KAlarmApp::slotCommandExited:" << pd->event->id() << ": pre-action failed: cancelled";
                    if (pd->reschedule())
                        rescheduleAlarm(*pd->event, *pd->alarm, true);
                    executeAlarm = false;
                }
            }
            if (pd->preAction())
                ResourcesCalendar::setAlarmPending(*pd->event, false);
            if (executeAlarm)
            {
                execAlarm(*pd->event, *pd->alarm,   (pd->reschedule()     ? Reschedule : NoExecFlag)
                                                  | (pd->allowDefer()     ? AllowDefer : NoExecFlag)
                                                  | (pd->noRecordCmdErr() ? NoRecordCmdError : NoExecFlag)
                                                  | NoPreAction);
            }
            mCommandProcesses.removeAt(i);
            if (pd->exitReceiver && !pd->exitMethod.isEmpty())
                QMetaObject::invokeMethod(pd->exitReceiver, pd->exitMethod.constData(), Qt::DirectConnection, Q_ARG(ShellProcess::Status, status));
            delete pd;
            break;
        }
    }

    // If there are now no executing shell commands, quit if a quit was queued
    if (mPendingQuit  &&  mCommandProcesses.isEmpty())
        quitIf(mPendingQuitCode);
}

/******************************************************************************
* Output an error message for a shell command, and record the alarm's error status.
*/
void KAlarmApp::commandErrorMsg(const ShellProcess* proc, const KAEvent& event, const KAAlarm* alarm, int flags, const QStringList& errors)
{
    KAEvent::CmdErrType cmderr;
    QString dontShowAgain;
    QStringList errmsgs = errors;
    if (flags & ProcData::PRE_ACTION)
    {
        if (event.extraActionOptions() & KAEvent::DontShowPreActError)
            return;   // don't notify user of any errors for the alarm
        errmsgs += i18nc("@info", "Pre-alarm action:");
        dontShowAgain = QStringLiteral("Pre");
        cmderr = KAEvent::CMD_ERROR_PRE;
    }
    else if (flags & ProcData::POST_ACTION)
    {
        errmsgs += i18nc("@info", "Post-alarm action:");
        dontShowAgain = QStringLiteral("Post");
        cmderr = (event.commandError() == KAEvent::CMD_ERROR_PRE)
               ? KAEvent::CMD_ERROR_PRE_POST : KAEvent::CMD_ERROR_POST;
    }
    else
    {
        if (!event.commandHideError())
            dontShowAgain = QStringLiteral("Exec");
        cmderr = KAEvent::CMD_ERROR;
    }

    // Record the alarm's error status
    if (!(flags & ProcData::NO_RECORD_ERROR))
        setEventCommandError(event, cmderr);

    if (!dontShowAgain.isEmpty())
    {
        // Display an error message
        if (proc)
        {
            errmsgs += proc->errorMessage();
            if (!(flags & ProcData::TEMP_FILE))
                errmsgs += proc->command();
            dontShowAgain += QString::number(proc->status());
        }
        MessageDisplay::showError(event, (alarm ? alarm->dateTime() : DateTime()), errmsgs, dontShowAgain);
    }
}

/******************************************************************************
* Notes that an informational KMessageBox is displayed for this process.
*/
void KAlarmApp::commandMessage(ShellProcess* proc, QWidget* parent)
{
    // Find this command in the command list
    for (ProcData* pd : std::as_const(mCommandProcesses))
    {
        if (pd->process == proc)
        {
            pd->messageBoxParent = parent;
            break;
        }
    }
}

/******************************************************************************
* If this is the first time through, open the calendar file, and start
* processing the execution queue.
*/
bool KAlarmApp::initCheck(bool calendarOnly)
{
    static bool firstTime = true;
    if (firstTime)
        qCDebug(KALARM_LOG) << "KAlarmApp::initCheck: first time";

    if (initialiseTimerResources()  ||  firstTime)
    {
        /* Need to open the display calendar now, since otherwise if display
         * alarms are immediately due, they will often be processed while
         * MessageDisplay::redisplayAlarms() is executing open() (but before
         * open() completes), which causes problems!!
         */
        DisplayCalendar::open();
    }
    if (firstTime)
    {
        setArchivePurgeDays();

        firstTime = false;
    }

    if (!calendarOnly)
        startProcessQueue();      // start processing the execution queue

    return true;
}

/******************************************************************************
* Called when an audio thread starts or stops.
*/
void KAlarmApp::notifyAudioPlaying(bool playing)
{
    Q_EMIT audioPlaying(playing);
}

/******************************************************************************
* Stop audio play.
*/
void KAlarmApp::stopAudio()
{
    MessageDisplay::stopAudio();
}

/******************************************************************************
* Set the command error for the specified alarm.
*/
void KAlarmApp::setEventCommandError(const KAEvent& event, KAEvent::CmdErrType err) const
{
    ProcData* pd = findCommandProcess(event.id());
    if (pd && pd->eventDeleted)
        return;   // the alarm has been deleted, so can't set error status

    if (err == KAEvent::CMD_ERROR_POST  &&  event.commandError() == KAEvent::CMD_ERROR_PRE)
        err = KAEvent::CMD_ERROR_PRE_POST;
    event.setCommandError(err);
    KAEvent ev = ResourcesCalendar::event(EventId(event));
    if (ev.isValid()  &&  ev.commandError() != err)
    {
        ev.setCommandError(err);
        ResourcesCalendar::updateEvent(ev);
    }
    Resource resource = Resources::resourceForEvent(event.id());
    resource.handleCommandErrorChange(event);
}

/******************************************************************************
* Clear the command error for the specified alarm.
*/
void KAlarmApp::clearEventCommandError(const KAEvent& event, KAEvent::CmdErrType err) const
{
    ProcData* pd = findCommandProcess(event.id());
    if (pd && pd->eventDeleted)
        return;   // the alarm has been deleted, so can't set error status

    auto newerr = static_cast<KAEvent::CmdErrType>(event.commandError() & ~err);
    event.setCommandError(newerr);
    KAEvent ev = ResourcesCalendar::event(EventId(event));
    if (ev.isValid())
    {
        newerr = static_cast<KAEvent::CmdErrType>(ev.commandError() & ~err);
        ev.setCommandError(newerr);
        ResourcesCalendar::updateEvent(ev);
    }
    Resource resource = Resources::resourceForEvent(event.id());
    resource.handleCommandErrorChange(event);
}

/******************************************************************************
* Find the currently executing command process for an event ID, if any.
*/
KAlarmApp::ProcData* KAlarmApp::findCommandProcess(const QString& eventId) const
{
    for (ProcData* pd : std::as_const(mCommandProcesses))
    {
        if (pd->event->id() == eventId)
            return pd;
    }
    return nullptr;
}


KAlarmApp::ProcData::ProcData(ShellProcess* p, KAEvent* e, KAAlarm* a, int f)
    : process(p)
    , event(e)
    , alarm(a)
    , flags(f)
{ }

KAlarmApp::ProcData::~ProcData()
{
    while (!tempFiles.isEmpty())
    {
        // Delete the temporary file called by the XTerm command
        QFile f(tempFiles.constFirst());
        f.remove();
        tempFiles.removeFirst();
    }
    delete process;
    delete event;
    delete alarm;
}

// vim: et sw=4:
