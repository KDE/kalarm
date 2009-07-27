/*
 *  kalarmapp.cpp  -  the KAlarm application object
 *  Program:  kalarm
 *  Copyright © 2001-2009 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"
#include "kalarmapp.moc"

#include "alarmcalendar.h"
#include "alarmlistview.h"
#include "autoqpointer.h"
#include "birthdaymodel.h"
#include "commandoptions.h"
#include "dbushandler.h"
#include "editdlgtypes.h"
#include "eventlistmodel.h"
#include "functions.h"
#include "kamail.h"
#include "karecurrence.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "messagewin.h"
#include "preferences.h"
#include "prefdlg.h"
#include "shellprocess.h"
#include "startdaytimer.h"
#include "traywindow.h"

#include <klocale.h>
#include <kstandarddirs.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <ktemporaryfile.h>
#include <kfileitem.h>
#include <kglobal.h>
#include <kstandardguiitem.h>
#include <kservicetypetrader.h>
#include <kspeechinterface.h>
#include <kspeech.h>
#include <ktoolinvocation.h>
#include <netwm.h>
#include <kdebug.h>
#include <kshell.h>

#include <QObject>
#include <QTimer>
#include <QRegExp>
#include <QFile>
#include <QByteArray>
#include <QTextStream>
#include <QtDBus/QtDBus>

#include <stdlib.h>
#include <ctype.h>
#include <iostream>
#include <climits>

static const char* KTTSD_DBUS_SERVICE  = "org.kde.kttsd";
static const char* KTTDS_DBUS_PATH     = "/KSpeech";

static void setEventCommandError(const KAEvent&, KAEvent::CmdErrType);
static void clearEventCommandError(const KAEvent&, KAEvent::CmdErrType);

/******************************************************************************
* Find the maximum number of seconds late which a late-cancel alarm is allowed
* to be. This is calculated as the late cancel interval, plus a few seconds
* leeway to cater for any timing irregularities.
*/
static inline int maxLateness(int lateCancel)
{
	static const int LATENESS_LEEWAY = 5;
	int lc = (lateCancel >= 1) ? (lateCancel - 1)*60 : 0;
	return LATENESS_LEEWAY + lc;
}


KAlarmApp*  KAlarmApp::theInstance  = 0;
int         KAlarmApp::mActiveCount = 0;
int         KAlarmApp::mFatalError  = 0;
QString     KAlarmApp::mFatalMessage;


/******************************************************************************
* Construct the application.
*/
KAlarmApp::KAlarmApp()
	: KUniqueApplication(),
	  mInitialised(false),
	  mQuitting(false),
	  mLoginAlarmsDone(false),
	  mDBusHandler(new DBusHandler()),
	  mTrayWindow(0),
	  mAlarmTimer(new QTimer(this)),
	  mArchivedPurgeDays(-1),      // default to not purging
	  mPurgeDaysQueued(-1),
	  mKSpeech(0),
	  mPendingQuit(false),
	  mProcessingQueue(false),
	  mSessionClosingDown(false),
	  mAlarmsEnabled(true),
	  mSpeechEnabled(false)
{
	kDebug();
#ifndef NDEBUG
	KAlarm::setTestModeConditions();
#endif
	mAlarmTimer->setSingleShot(true);
	connect(mAlarmTimer, SIGNAL(timeout()), SLOT(checkNextDueAlarm()));

	setQuitOnLastWindowClosed(false);
	Preferences::self()->readConfig();
	Preferences::setAutoStart(true);
	Preferences::self()->writeConfig();
	Preferences::connect(SIGNAL(startOfDayChanged(const QTime&, const QTime&)), this, SLOT(changeStartOfDay()));
	Preferences::connect(SIGNAL(feb29TypeChanged(Feb29Type)), this, SLOT(slotFeb29TypeChanged(Feb29Type)));
	Preferences::connect(SIGNAL(showInSystemTrayChanged(bool)), this, SLOT(slotShowInSystemTrayChanged()));
	Preferences::connect(SIGNAL(archivedKeepDaysChanged(int)), this, SLOT(setArchivePurgeDays()));
	slotFeb29TypeChanged(Preferences::defaultFeb29Type());

	connect(QDBusConnection::sessionBus().interface(), SIGNAL(serviceUnregistered(const QString&)),
	        SLOT(slotDBusServiceUnregistered(const QString&)));
	if (AlarmCalendar::initialiseCalendars())
	{
		connect(AlarmCalendar::resources(), SIGNAL(earliestAlarmChanged()), SLOT(checkNextDueAlarm()));

		KConfigGroup config(KGlobal::config(), "General");
		mNoSystemTray        = config.readEntry("NoSystemTray", false);
		mOldShowInSystemTray = wantShowInSystemTray();
		mStartOfDay          = Preferences::startOfDay();
		if (Preferences::hasStartOfDayChanged())
			mStartOfDay.setHMS(100,0,0);    // start of day time has changed: flag it as invalid
		DateTime::setStartOfDay(mStartOfDay);
		mPrefsArchivedColour = Preferences::archivedColour();
	}

	// Check if the speech synthesis daemon is installed
	mSpeechEnabled = (KServiceTypeTrader::self()->query("DBUS/Text-to-Speech", "Name == 'KTTSD'").count() > 0);
	if (!mSpeechEnabled)
		kDebug() << "Speech synthesis disabled (KTTSD not found)";
	// Check if KOrganizer is installed
	QString korg = QLatin1String("korganizer");
	mKOrganizerEnabled = !KStandardDirs::locate("exe", korg).isNull()  ||  !KStandardDirs::findExe(korg).isNull();
	if (!mKOrganizerEnabled)
		kDebug() << "KOrganizer options disabled (KOrganizer not found)";
}

/******************************************************************************
*/
KAlarmApp::~KAlarmApp()
{
	while (!mCommandProcesses.isEmpty())
	{
		ProcData* pd = mCommandProcesses[0];
		mCommandProcesses.pop_front();
		delete pd;
	}
	AlarmCalendar::terminateCalendars();
}

/******************************************************************************
* Return the one and only KAlarmApp instance.
* If it doesn't already exist, it is created first.
*/
KAlarmApp* KAlarmApp::getInstance()
{
	if (!theInstance)
	{
		theInstance = new KAlarmApp;

		if (mFatalError)
			theInstance->quitFatal();
	}
	return theInstance;
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
	kDebug() << "Restoring";
	++mActiveCount;
	if (!initCheck(true))     // open the calendar file (needed for main windows), don't process queue yet
	{
		--mActiveCount;
		quitIf(1, true);    // error opening the main calendar - quit
		return true;
	}
	MainWindow* trayParent = 0;
	for (int i = 1;  KMainWindow::canBeRestored(i);  ++i)
	{
		QString type = KMainWindow::classNameOfToplevel(i);
		if (type == QLatin1String("MainWindow"))
		{
			MainWindow* win = MainWindow::create(true);
			win->restore(i, false);
			if (win->isHiddenTrayParent())
				trayParent = win;
			else
				win->show();
		}
		else if (type == QLatin1String("MessageWin"))
		{
			MessageWin* win = new MessageWin;
			win->restore(i, false);
			if (win->isValid())
				win->show();
			else
				delete win;
		}
	}

	// Try to display the system tray icon if it is configured to be shown
	if (trayParent  ||  wantShowInSystemTray())
	{
		if (!MainWindow::count())
			kWarning() << "no main window to be restored!?";
		else
		{
			displayTrayIcon(true, trayParent);
			// Occasionally for no obvious reason, the main main window is
			// shown when it should be hidden, so hide it just to be sure.
			if (trayParent)
				trayParent->hide();
		}
	}

	--mActiveCount;
	quitIf(0);           // quit if no windows are open

	startProcessQueue();      // start processing the execution queue
	return true;
}

/******************************************************************************
* Called for a KUniqueApplication when a new instance of the application is
* started.
*/
int KAlarmApp::newInstance()
{
	kDebug();
	if (mFatalError)
	{
		quitFatal();
		return 1;
	}
	++mActiveCount;
	int exitCode = 0;               // default = success
	static bool firstInstance = true;
	bool dontRedisplay = false;
	if (!firstInstance || !isSessionRestored())
	{
		CommandOptions options;   // fetch and parse command line options
#ifndef NDEBUG
		if (options.simulationTime().isValid())
			KAlarm::setSimulatedSystemTime(options.simulationTime());
#endif
		CommandOptions::Command command = options.command();
		switch (command)
		{
			case CommandOptions::TRAY:
				// Display only the system tray icon
				if (!KSystemTrayIcon::isSystemTrayAvailable()
				||  !initCheck()   // open the calendar, start processing execution queue
				||  !displayTrayIcon(true))
					exitCode = 1;
				break;

			case CommandOptions::TRIGGER_EVENT:
			case CommandOptions::CANCEL_EVENT:
			{
				// Display or delete the event with the specified event ID
				EventFunc function = (command == CommandOptions::TRIGGER_EVENT) ? EVENT_TRIGGER : EVENT_CANCEL;
				if (!initCheck(true))   // open the calendar, don't start processing execution queue yet
					exitCode = 1;
				else
				{
					startProcessQueue();        // start processing the execution queue
					if (!handleEvent(options.eventId(), function))
						exitCode = 1;
				}
				break;
			}
			case CommandOptions::EDIT:
				// Edit a specified existing alarm
				if (!initCheck())
					exitCode = 1;
				else if (!KAlarm::editAlarm(options.eventId()))
				{
					CommandOptions::printError(i18nc("@info:shell", "<icode>%1</icode>: Event <resource>%2</resource> not found, or not editable", QString::fromLatin1("--edit"), options.eventId()));
					exitCode = 1;
				}
				break;

			case CommandOptions::EDIT_NEW:
			{
				// Edit a new alarm, and optionally preset selected values
				if (!initCheck())
					exitCode = 1;
				else
				{
					// Use AutoQPointer to guard against crash on application exit while
					// the dialogue is still open. It prevents double deletion (both on
					// deletion of parent, and on return from this function).
					AutoQPointer<EditAlarmDlg> editDlg = EditAlarmDlg::create(false, options.editType(), true);
					if (options.alarmTime().isValid())
						editDlg->setTime(options.alarmTime());
					if (options.recurrence())
						editDlg->setRecurrence(*options.recurrence(), options.subRepeatInterval(), options.subRepeatCount());
					else if (options.flags() & KAEvent::REPEAT_AT_LOGIN)
						editDlg->setRepeatAtLogin();
					editDlg->setAction(options.editAction(), options.text());
					if (options.lateCancel())
						editDlg->setLateCancel(options.lateCancel());
					if (options.flags() & KAEvent::COPY_KORGANIZER)
						editDlg->setShowInKOrganizer(true);
					switch (options.editType())
					{
						case EditAlarmDlg::DISPLAY:
						{
							// EditAlarmDlg::create() always returns EditDisplayAlarmDlg for type = DISPLAY
							EditDisplayAlarmDlg* dlg = qobject_cast<EditDisplayAlarmDlg*>(editDlg);
							if (options.fgColour().isValid())
								dlg->setFgColour(options.fgColour());
							if (options.bgColour().isValid())
								dlg->setBgColour(options.bgColour());
							if (!options.audioFile().isEmpty()
							||  options.flags() & (KAEvent::BEEP | KAEvent::SPEAK))
							{
								int flags = options.flags();
								Preferences::SoundType type = (flags & KAEvent::BEEP) ? Preferences::Sound_Beep
								                            : (flags & KAEvent::SPEAK) ? Preferences::Sound_Speak
								                            : Preferences::Sound_File;
								dlg->setAudio(type, options.audioFile(), options.audioVolume(), flags & KAEvent::REPEAT_SOUND);
							}
							if (options.reminderMinutes())
								dlg->setReminder(options.reminderMinutes());
							if (options.flags() & KAEvent::CONFIRM_ACK)
								dlg->setConfirmAck(true);
							if (options.flags() & KAEvent::AUTO_CLOSE)
								dlg->setAutoClose(true);
							break;
						}
						case EditAlarmDlg::COMMAND:
							break;
						case EditAlarmDlg::EMAIL:
						{
							// EditAlarmDlg::create() always returns EditEmailAlarmDlg for type = EMAIL
							EditEmailAlarmDlg* dlg = qobject_cast<EditEmailAlarmDlg*>(editDlg);
							if (options.fromID()
							||  !options.addressees().isEmpty()
							||  !options.subject().isEmpty()
							||  !options.attachments().isEmpty())
								dlg->setEmailFields(options.fromID(), options.addressees(), options.subject(), options.attachments());
							if (options.flags() & KAEvent::EMAIL_BCC)
								dlg->setBcc(true);
							break;
						}
						case EditAlarmDlg::NO_TYPE:
							break;
					}
					KAlarm::execNewAlarmDlg(editDlg);
				}
				break;
			}
			case CommandOptions::EDIT_NEW_PRESET:
				// Edit a new alarm, preset with a template
				if (!initCheck())
					exitCode = 1;
				else
					KAlarm::editNewAlarm(options.templateName());
				break;

			case CommandOptions::NEW:
				// Display a message or file, execute a command, or send an email
				if (!initCheck()
				||  !scheduleEvent(options.editAction(), options.text(), options.alarmTime(),
				                   options.lateCancel(), options.flags(), options.bgColour(),
				                   options.fgColour(), QFont(), options.audioFile(), options.audioVolume(),
				                   options.reminderMinutes(), (options.recurrence() ? *options.recurrence() : KARecurrence()),
				                   options.subRepeatInterval(), options.subRepeatCount(),
				                   options.fromID(), options.addressees(),
				                   options.subject(), options.attachments()))
					exitCode = 1;
				break;

			case CommandOptions::NONE:
				// No arguments - run interactively & display the main window
#ifndef NDEBUG
				if (options.simulationTime().isValid()  &&  !firstInstance)
					break;   // simulating time: don't open main window if already running
#endif
				if (!initCheck())
					exitCode = 1;
				else
					(MainWindow::create())->show();
				break;

			case CommandOptions::CMD_ERROR:
				exitCode = 1;
				break;
		}
	}

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
		if (AlarmCalendar::resources())
			MessageWin::redisplayAlarms();
	}

	--mActiveCount;
	firstInstance = false;

	// Quit the application if this was the last/only running "instance" of the program.
	// Executing 'return' doesn't work very well since the program continues to
	// run if no windows were created.
	quitIf(exitCode);
	return exitCode;
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
		displayTrayIcon(false);
		if (MessageWin::instanceCount())
			return false;
	}
	else if (mQuitting)
		return false;   // MainWindow::closeAll() causes quitIf() to be called again
	else
	{
		// Quit only if there are no more "instances" running
		mPendingQuit = false;
		if (mActiveCount > 0  ||  MessageWin::instanceCount())
			return false;
		int mwcount = MainWindow::count();
		MainWindow* mw = mwcount ? MainWindow::firstWindow() : 0;
		if (mwcount > 1  ||  (mwcount && (!mw->isHidden() || !mw->isTrayParent())))
			return false;
		// There are no windows left except perhaps a main window which is a hidden tray icon parent
		if (mTrayWindow)
		{
			// There is a system tray icon.
			// Don't exit unless the system tray doesn't seem to exist.
			if (checkSystemTray())
				return false;
		}
		if (!mDcopQueue.isEmpty()  ||  !mCommandProcesses.isEmpty())
		{
			// Don't quit yet if there are outstanding actions on the execution queue
			mPendingQuit = true;
			mPendingQuitCode = exitCode;
			return false;
		}
	}

	// This was the last/only running "instance" of the program, so exit completely.
	kDebug() << exitCode << ": quitting";
	delete mAlarmTimer;     // prevent checking for alarms after deleting calendars
	mAlarmTimer = 0;
	mInitialised = false;   // prevent processQueue() from running
	AlarmCalendar::terminateCalendars();
	BirthdayModel::close();
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
	kDebug();
	if (MessageBox::warningContinueCancel(parent, KMessageBox::Cancel,
	                                      i18nc("@info", "Quitting will disable alarms (once any alarm message windows are closed)."),
	                                      QString(), KStandardGuiItem::quit(), Preferences::QUIT_WARN
	                                     ) != KMessageBox::Yes)
		return;
	quitIf(0, true);
}

/******************************************************************************
* Called when the session manager is about to close down the application.
*/
void KAlarmApp::commitData(QSessionManager& sm)
{
	mSessionClosingDown = true;
	KUniqueApplication::commitData(sm);
	mSessionClosingDown = false;         // reset in case shutdown is cancelled
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
		if (theInstance)
			QTimer::singleShot(0, theInstance, SLOT(quitFatal()));
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
			KMessageBox::error(0, mFatalMessage);
			mFatalError = 3;
			// fall through to '3'
		case 3:
			if (theInstance)
				theInstance->quitIf(1, true);
			break;
	}
	QTimer::singleShot(1000, this, SLOT(quitFatal()));
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
	KAEvent* nextEvent = AlarmCalendar::resources()->earliestAlarm();
	if (!nextEvent)
		return;   // there are no alarms pending
	KDateTime nextDt = nextEvent->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
	KDateTime now = KDateTime::currentDateTime(Preferences::timeZone());
	qint64 interval = now.secsTo_long(nextDt);
	kDebug() << "now:" << qPrintable(now.toString("%Y-%m-%d %H:%M %:Z")) << ", next:" << qPrintable(nextDt.toString("%Y-%m-%d %H:%M %:Z")) << ", due:" << interval;
	if (interval <= 0)
	{
		// Queue the alarm
		queueAlarmId(nextEvent->id());
		kDebug() << nextEvent->id() << ": due now";
		QTimer::singleShot(0, this, SLOT(processQueue()));
	}
	else
	{
		// No alarm is due yet, so set timer to wake us when it's due.
		// Check for integer overflow before setting timer.
#ifndef HIBERNATION_SIGNAL
		/* TODO: REPLACE THIS CODE WHEN A SYSTEM NOTIFICATION SIGNAL BECOMES
		 *       AVAILABLE FOR WAKEUP FROM HIBERNATION.
		 * Re-evaluate the next alarm time every minute, in case the
		 * system clock jumps. The most common case when the clock jumps
		 * is when a laptop wakes from hibernation. If timers were left to
		 * run, they would trigger late by the length of time the system
		 * was asleep.
		 */
		if (interval > 60)    // 1 minute
			interval = 60;
#endif
		interval *= 1000;
		if (interval > INT_MAX)
			interval = INT_MAX;
		kDebug() << nextEvent->id() << "wait" << interval/1000 << "seconds";
		mAlarmTimer->start(static_cast<int>(interval));
	}
}

/******************************************************************************
* Called by the alarm timer when the next alarm is due.
* Also called when the execution queue has finished processing to check for the
* next alarm.
*/
void KAlarmApp::queueAlarmId(const QString& id)
{
	for (int i = 0, end = mDcopQueue.count();  i < end;  ++i)
	{
		if (mDcopQueue[i].function == EVENT_HANDLE  &&  mDcopQueue[i].eventId == id)
			return;  // the alarm is already queued
	}
	mDcopQueue.enqueue(DcopQEntry(EVENT_HANDLE, id));
}

/******************************************************************************
* Start processing the execution queue.
*/
void KAlarmApp::startProcessQueue()
{
	if (!mInitialised)
	{
		kDebug();
		mInitialised = true;
		QTimer::singleShot(0, this, SLOT(processQueue()));    // process anything already queued
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
		kDebug();
		mProcessingQueue = true;

		// Refresh alarms if that's been queued
		KAlarm::refreshAlarmsIfQueued();

		if (!mLoginAlarmsDone  &&  mAlarmsEnabled)
		{
			// Queue all at-login alarms once only, at program start-up
			KAEvent::List events = AlarmCalendar::resources()->atLoginAlarms();
			for (int i = 0, end = events.count();  i < end;  ++i)
				queueAlarmId(events[i]->id());
			mLoginAlarmsDone = true;
		}

		// Process queued events
		while (!mDcopQueue.isEmpty())
		{
			DcopQEntry& entry = mDcopQueue.head();
			if (entry.eventId.isEmpty())
			{
				// It's a new alarm
				switch (entry.function)
				{
				case EVENT_TRIGGER:
					execAlarm(entry.event, entry.event.firstAlarm(), false);
					break;
				case EVENT_HANDLE:
					KAlarm::addEvent(entry.event, 0, 0, KAlarm::ALLOW_KORG_UPDATE | KAlarm::NO_RESOURCE_PROMPT);
					break;
				case EVENT_CANCEL:
					break;
				}
			}
			else
				handleEvent(entry.eventId, entry.function);
			mDcopQueue.dequeue();
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

		// Schedule the application to be woken when the next alarm is due
		checkNextDueAlarm();
	}
}

/******************************************************************************
* Called when the system tray main window is closed.
*/
void KAlarmApp::removeWindow(TrayWindow*)
{
	mTrayWindow = 0;
}

/******************************************************************************
*  Display or close the system tray icon.
*/
bool KAlarmApp::displayTrayIcon(bool show, MainWindow* parent)
{
	static bool creating = false;
	if (show)
	{
		if (!mTrayWindow  &&  !creating)
		{
			if (!KSystemTrayIcon::isSystemTrayAvailable())
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
			connect(mTrayWindow, SIGNAL(deleted()), SIGNAL(trayIconToggled()));
			mTrayWindow->show();
			emit trayIconToggled();

			if (!checkSystemTray())
				quitIf(0);    // exit the application if there are no open windows
		}
	}
	else if (mTrayWindow)
	{
		delete mTrayWindow;
		mTrayWindow = 0;
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
	if (KSystemTrayIcon::isSystemTrayAvailable() == mNoSystemTray)
	{
		kDebug() << "changed ->" << mNoSystemTray;
		mNoSystemTray = !mNoSystemTray;

		// Store the new setting in the config file, so that if KAlarm exits it will
		// restart with the correct default.
		KConfigGroup config(KGlobal::config(), "General");
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
	return mTrayWindow ? mTrayWindow->assocMainWindow() : 0;
}

/******************************************************************************
* Called when the show-in-system-tray preference setting has changed, to show
* or hide the system tray icon.
*/
void KAlarmApp::slotShowInSystemTrayChanged()
{
	bool newShowInSysTray = wantShowInSystemTray();
	if (newShowInSysTray != mOldShowInSystemTray)
	{
		// The system tray run mode has changed
		++mActiveCount;         // prevent the application from quitting
		MainWindow* win = mTrayWindow ? mTrayWindow->assocMainWindow() : 0;
		delete mTrayWindow;     // remove the system tray icon if it is currently shown
		mTrayWindow = 0;
		mOldShowInSystemTray = newShowInSysTray;
		if (newShowInSysTray)
			displayTrayIcon(true);
		else
		{
			if (win  &&  win->isHidden())
				delete win;
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
	QTime sod = Preferences::startOfDay();
	DateTime::setStartOfDay(sod);
	AlarmCalendar* cal = AlarmCalendar::resources();
	if (KAEventData::adjustStartOfDay(cal->kcalEvents(KCalEvent::ACTIVE), Preferences::startOfDay(), Preferences::timeZone()))
		cal->save();
	Preferences::updateStartOfDayCheck(sod);  // now that calendar is updated, set OK flag in config file
	mStartOfDay = sod;
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
	return Preferences::showInSystemTray()  &&  KSystemTrayIcon::isSystemTrayAvailable();
}

/******************************************************************************
* Called when the length of time to keep archived alarms changes in KAlarm's
* preferences.
* Set the number of days to keep archived alarms.
* Alarms which are older are purged immediately, and at the start of each day.
*/
void KAlarmApp::setArchivePurgeDays()
{
	int newDays = Preferences::archivedKeepDays();
	if (newDays != mArchivedPurgeDays)
	{
		int oldDays = mArchivedPurgeDays;
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
* Enable or disable alarm monitoring.
*/
void KAlarmApp::setAlarmsEnabled(bool enabled)
{
	if (enabled != mAlarmsEnabled)
	{
		mAlarmsEnabled = enabled;
		emit alarmEnabledToggled(enabled);
		if (enabled  &&  !mProcessingQueue)
			checkNextDueAlarm();

	}
}

/******************************************************************************
* Spread or collect alarm message and error message windows.
*/
void KAlarmApp::spreadWindows(bool spread)
{
	spread = MessageWin::spread(spread);
	emit spreadWindowsToggled(spread);
}

/******************************************************************************
* Called when the spread status of message windows changes.
* Set the 'spread windows' action state.
*/
void KAlarmApp::setSpreadWindowsState(bool spread)
{
	emit spreadWindowsToggled(spread);
}

/******************************************************************************
* Called to schedule a new alarm, either in response to a DCOP notification or
* to command line options.
* Reply = true unless there was a parameter error or an error opening calendar file.
*/
bool KAlarmApp::scheduleEvent(KAEventData::Action action, const QString& text, const KDateTime& dateTime,
                              int lateCancel, int flags, const QColor& bg, const QColor& fg, const QFont& font,
                              const QString& audioFile, float audioVolume, int reminderMinutes,
                              const KARecurrence& recurrence, int repeatInterval, int repeatCount,
                              uint mailFromID, const EmailAddressList& mailAddresses,
                              const QString& mailSubject, const QStringList& mailAttachments)
{
	kDebug() << text;
	if (!dateTime.isValid())
		return false;
	KDateTime now = KDateTime::currentUtcDateTime();
	if (lateCancel  &&  dateTime < now.addSecs(-maxLateness(lateCancel)))
		return true;               // alarm time was already archived too long ago
	KDateTime alarmTime = dateTime;
	// Round down to the nearest minute to avoid scheduling being messed up
	if (!dateTime.isDateOnly())
		alarmTime.setTime(QTime(alarmTime.time().hour(), alarmTime.time().minute(), 0));

	KAEvent event(alarmTime, text, bg, fg, font, action, lateCancel, flags, true);
	if (reminderMinutes)
	{
		bool onceOnly = (reminderMinutes < 0);
		event.setReminder((onceOnly ? -reminderMinutes : reminderMinutes), onceOnly);
	}
	if (!audioFile.isEmpty())
		event.setAudioFile(audioFile, audioVolume, -1, 0);
	if (!mailAddresses.isEmpty())
		event.setEmail(mailFromID, mailAddresses, mailSubject, mailAttachments);
	event.setRecurrence(recurrence);
	event.setFirstRecurrence();
	event.setRepetition(repeatInterval, repeatCount - 1);
	event.endChanges();
	if (alarmTime <= now)
	{
		// Alarm is due for display already.
		// First execute it once without adding it to the calendar file.
		if (!mInitialised)
			mDcopQueue.enqueue(DcopQEntry(event, EVENT_TRIGGER));
		else
			execAlarm(event, event.firstAlarm(), false);
		// If it's a recurring alarm, reschedule it for its next occurrence
		if (!event.recurs()
		||  event.setNextOccurrence(now) == KAEventData::NO_OCCURRENCE)
			return true;
		// It has recurrences in the future
	}

	// Queue the alarm for insertion into the calendar file
	mDcopQueue.enqueue(DcopQEntry(event));
	if (mInitialised)
		QTimer::singleShot(0, this, SLOT(processQueue()));
	return true;
}

/******************************************************************************
* Called in response to a D-Bus request to trigger or cancel an event.
* Optionally display the event. Delete the event from the calendar file and
* from every main window instance.
*/
bool KAlarmApp::dbusHandleEvent(const QString& eventID, EventFunc function)
{
	kDebug() << eventID;
	mDcopQueue.append(DcopQEntry(function, eventID));
	if (mInitialised)
		QTimer::singleShot(0, this, SLOT(processQueue()));
	return true;
}

/******************************************************************************
* Either:
* a) Display the event and then delete it if it has no outstanding repetitions.
* b) Delete the event.
* c) Reschedule the event for its next repetition. If none remain, delete it.
* If the event is deleted, it is removed from the calendar file and from every
* main window instance.
*/
bool KAlarmApp::handleEvent(const QString& eventID, EventFunc function)
{
	KAEvent* event = AlarmCalendar::resources()->event(eventID);
	if (!event)
	{
		kWarning() << "Event ID not found:" << eventID;
		return false;
	}
	switch (function)
	{
		case EVENT_CANCEL:
			kDebug() << eventID << ", CANCEL";
			KAlarm::deleteEvent(*event, true);
			break;

		case EVENT_TRIGGER:    // handle it if it's due, else execute it regardless
		case EVENT_HANDLE:     // handle it if it's due
		{
			KDateTime now = KDateTime::currentUtcDateTime();
			kDebug() << eventID << "," << (function==EVENT_TRIGGER?"TRIGGER:":"HANDLE:") << qPrintable(now.dateTime().toString("yyyy-MM-dd hh:mm")) << "UTC";
			bool updateCalAndDisplay = false;
			bool alarmToExecuteValid = false;
			KAAlarm alarmToExecute;
			// Check all the alarms in turn.
			// Note that the main alarm is fetched before any other alarms.
			for (KAAlarm alarm = event->firstAlarm();  alarm.valid();  alarm = event->nextAlarm(alarm))
			{
				// Check if the alarm is due yet.
				KDateTime nextDT = alarm.dateTime(true).effectiveKDateTime();
				int secs = nextDT.secsTo(now);
				if (secs < 0)
				{
					// The alarm appears to be in the future.
					// Check if it's an invalid local clock time during a daylight
					// saving time shift, which has actually passed.
					if (alarm.dateTime().timeSpec() != KDateTime::ClockTime
					||  nextDT > now.toTimeSpec(KDateTime::ClockTime))
					{
						// This alarm is definitely not due yet
						kDebug() << "Alarm" << alarm.type() << "at" << nextDT.dateTime() << ": not due";
						continue;
					}
				}
				bool reschedule = false;
				if ((event->workTimeOnly() || event->holidaysExcluded())  &&  !alarm.deferred())
				{
					// The alarm is restricted to working hours and/or non-holidays
					// (apart from deferrals). This needs to be re-evaluated every
					// time it triggers, since working hours could change.
					if (alarm.dateTime().isDateOnly())
					{
						KDateTime dt(nextDT);
						dt.setDateOnly(true);
						reschedule = !KAlarm::isWorkingTime(dt, event);
					}
					else
						reschedule = !KAlarm::isWorkingTime(nextDT, event);
					if (reschedule)
						kDebug() << "Alarm" << alarm.type() << "at" << nextDT.dateTime() << ": not during working hours";
				}
				if (!reschedule  &&  alarm.repeatAtLogin())
				{
					// Alarm is to be displayed at every login.
					kDebug() << "REPEAT_AT_LOGIN";
					// Check if the main alarm is already being displayed.
					// (We don't want to display both at the same time.)
					if (alarmToExecute.valid())
						continue;

					// Set the time to display if it's a display alarm
					alarm.setTime(now);
				}
				if (!reschedule  &&  alarm.lateCancel())
				{
					// Alarm is due, and it is to be cancelled if too late.
					kDebug() << "LATE_CANCEL";
					bool cancel = false;
					if (alarm.dateTime().isDateOnly())
					{
						// The alarm has no time, so cancel it if its date is too far past
						int maxlate = alarm.lateCancel() / 1440;    // maximum lateness in days
						KDateTime limit(DateTime(nextDT.addDays(maxlate + 1)).effectiveKDateTime());
						if (now >= limit)
						{
							// It's too late to display the scheduled occurrence.
							// Find the last previous occurrence of the alarm.
							DateTime next;
							KAEventData::OccurType type = event->previousOccurrence(now, next, true);
							switch (type & ~KAEventData::OCCURRENCE_REPEAT)
							{
								case KAEventData::FIRST_OR_ONLY_OCCURRENCE:
								case KAEventData::RECURRENCE_DATE:
								case KAEventData::RECURRENCE_DATE_TIME:
								case KAEventData::LAST_RECURRENCE:
									limit.setDate(next.date().addDays(maxlate + 1));
									if (now >= limit)
									{
										if (type == KAEventData::LAST_RECURRENCE
										||  (type == KAEventData::FIRST_OR_ONLY_OCCURRENCE && !event->recurs()))
											cancel = true;   // last occurrence (and there are no repetitions)
										else
											reschedule = true;
									}
									break;
								case KAEventData::NO_OCCURRENCE:
								default:
									reschedule = true;
									break;
							}
						}
					}
					else
					{
						// The alarm is timed. Allow it to be the permitted amount late before cancelling it.
						int maxlate = maxLateness(alarm.lateCancel());
						if (secs > maxlate)
						{
							// It's over the maximum interval late.
							// Find the most recent occurrence of the alarm.
							DateTime next;
							KAEventData::OccurType type = event->previousOccurrence(now, next, true);
							switch (type & ~KAEventData::OCCURRENCE_REPEAT)
							{
								case KAEventData::FIRST_OR_ONLY_OCCURRENCE:
								case KAEventData::RECURRENCE_DATE:
								case KAEventData::RECURRENCE_DATE_TIME:
								case KAEventData::LAST_RECURRENCE:
									if (next.effectiveKDateTime().secsTo(now) > maxlate)
									{
										if (type == KAEventData::LAST_RECURRENCE
										||  (type == KAEventData::FIRST_OR_ONLY_OCCURRENCE && !event->recurs()))
											cancel = true;   // last occurrence (and there are no repetitions)
										else
											reschedule = true;
									}
									break;
								case KAEventData::NO_OCCURRENCE:
								default:
									reschedule = true;
									break;
							}
						}
					}

					if (cancel)
					{
						// All recurrences are finished, so cancel the event
						event->setArchive();
						if (cancelAlarm(*event, alarm.type(), false))
							return true;   // event has been deleted
						updateCalAndDisplay = true;
						continue;
					}
				}
				if (reschedule)
				{
					// The latest repetition was too long ago, so schedule the next one
					rescheduleAlarm(*event, alarm, false);
					updateCalAndDisplay = true;
					continue;
				}
				if (!alarmToExecuteValid)
				{
					kDebug() << "Alarm" << alarm.type() << ": execute";
					alarmToExecute = alarm;             // note the alarm to be displayed
					alarmToExecuteValid = true;         // only trigger one alarm for the event
				}
				else
					kDebug() << "Alarm" << alarm.type() << ": skip";
			}

			// If there is an alarm to execute, do this last after rescheduling/cancelling
			// any others. This ensures that the updated event is only saved once to the calendar.
			if (alarmToExecute.valid())
				execAlarm(*event, alarmToExecute, true, !alarmToExecute.repeatAtLogin());
			else
			{
				if (function == EVENT_TRIGGER)
				{
					// The alarm is to be executed regardless of whether it's due.
					// Only trigger one alarm from the event - we don't want multiple
					// identical messages, for example.
					KAAlarm alarm = event->firstAlarm();
					if (alarm.valid())
						execAlarm(*event, alarm, false);
				}
				if (updateCalAndDisplay)
					KAlarm::updateEvent(*event);     // update the window lists and calendar file
				else if (function != EVENT_TRIGGER)
					kDebug() << "No action";
			}
			break;
		}
	}
	return true;
}

/******************************************************************************
* Called when an alarm action has completed, to perform any post-alarm actions.
*/
void KAlarmApp::alarmCompleted(const KAEvent& event)
{
	if (!event.postAction().isEmpty())
	{
		if (!ShellProcess::authorised())
			setEventCommandError(event, KAEvent::CMD_ERROR_POST);
		else
		{
			QString command = event.postAction();
			kDebug() << event.id() << ":" << command;
			doShellCommand(command, event, 0, ProcData::POST_ACTION);
		}
	}
}

/******************************************************************************
* Reschedule the alarm for its next recurrence. If none remain, delete it.
* If the alarm is deleted and it is the last alarm for its event, the event is
* removed from the calendar file and from every main window instance.
*/
void KAlarmApp::rescheduleAlarm(KAEvent& event, const KAAlarm& alarm, bool updateCalAndDisplay)
{
	kDebug();
	bool update = false;
	event.startChanges();
	if (alarm.reminder()  ||  alarm.deferred())
	{
		// It's an advance warning alarm or an extra deferred alarm, so delete it
		event.removeExpiredAlarm(alarm.type());
		update = true;
	}
	else if (alarm.repeatAtLogin())
	{
		// Leave an alarm which repeats at every login until its main alarm is deleted
		if (updateCalAndDisplay  &&  event.updated())
			update = true;
	}
	else
	{
		// Reschedule the alarm for its next occurrence.
		KAEventData::OccurType type = event.setNextOccurrence(KDateTime::currentUtcDateTime());
		switch (type)
		{
			case KAEventData::NO_OCCURRENCE:
				// All repetitions are finished, so cancel the event
				if (cancelAlarm(event, alarm.type(), updateCalAndDisplay))
					return;
				break;
			default:
				if (!(type & KAEventData::OCCURRENCE_REPEAT))
					break;
				// Next occurrence is a repeat, so fall through to recurrence handling
			case KAEventData::RECURRENCE_DATE:
			case KAEventData::RECURRENCE_DATE_TIME:
			case KAEventData::LAST_RECURRENCE:
				// The event is due by now and repetitions still remain, so rewrite the event
				if (updateCalAndDisplay)
					update = true;
				break;
			case KAEventData::FIRST_OR_ONLY_OCCURRENCE:
				// The first occurrence is still due?!?, so don't do anything
				break;
		}
		if (event.deferred())
		{
			// Just in case there's also a deferred alarm, ensure it's removed
			event.removeExpiredAlarm(KAAlarm::DEFERRED_ALARM);
			update = true;
		}
	}
	event.endChanges();
	if (update)
		KAlarm::updateEvent(event);     // update the window lists and calendar file
}

/******************************************************************************
* Delete the alarm. If it is the last alarm for its event, the event is removed
* from the calendar file and from every main window instance.
* Reply = true if event has been deleted.
*/
bool KAlarmApp::cancelAlarm(KAEvent& event, KAAlarm::Type alarmType, bool updateCalAndDisplay)
{
	kDebug();
	if (alarmType == KAAlarm::MAIN_ALARM  &&  !event.displaying()  &&  event.toBeArchived())
	{
		// The event is being deleted. Save it in the archived resources first.
		QString id = event.id();    // save event ID since KAlarm::addArchivedEvent() changes it
		KAlarm::addArchivedEvent(event);
		event.setEventId(id);       // restore event ID
	}
	event.removeExpiredAlarm(alarmType);
	if (!event.alarmCount())
	{
		KAlarm::deleteEvent(event, false);
		return true;
	}
	if (updateCalAndDisplay)
		KAlarm::updateEvent(event);    // update the window lists and calendar file
	return false;
}

/******************************************************************************
* Execute an alarm by displaying its message or file, or executing its command.
* Reply = ShellProcess instance if a command alarm
*       != 0 if successful
*       = -1 if execution has not completed
*       = 0 if the alarm is disabled, or if an error message was output.
*/
void* KAlarmApp::execAlarm(KAEvent& event, const KAAlarm& alarm, bool reschedule, bool allowDefer, bool noPreAction)
{
	if (!mAlarmsEnabled  ||  !event.enabled())
	{
		// The event (or all events) is disabled
		kDebug() << event.id() << ": disabled";
		if (reschedule)
			rescheduleAlarm(event, alarm, true);
		return 0;
	}

	void* result = (void*)1;
	event.setArchive();

	KAAlarm::Action action = alarm.action();
	if (action == KAAlarm::COMMAND && event.commandDisplay())
		action = KAAlarm::MESSAGE;
	switch (action)
	{
		case KAAlarm::MESSAGE:
		case KAAlarm::FILE:
		{
			// Display a message, file or command output, provided that the same event
			// isn't already being displayed
			MessageWin* win = MessageWin::findEvent(event.id());
			// Find if we're changing a reminder message to the real message
			bool reminder = (alarm.type() & KAAlarm::REMINDER_ALARM);
			bool replaceReminder = !reminder && win && (win->alarmType() & KAAlarm::REMINDER_ALARM);
			if (alarm.action() != KAAlarm::COMMAND
			&&  !reminder  &&  !event.deferred()
			&&  (replaceReminder || !win)  &&  !noPreAction
			&&  !event.preAction().isEmpty())
			{
				// It's not a reminder or a deferred alarm, and there is no message window
				// (other than a reminder window) currently displayed for this alarm,
				// and we need to execute a command before displaying the new window.
				if (!ShellProcess::authorised())
					setEventCommandError(event, KAEvent::CMD_ERROR_PRE);
				else
				{
					// Check whether the command is already being executed for this alarm.
					for (int i = 0, end = mCommandProcesses.count();  i < end;  ++i)
					{
						ProcData* pd = mCommandProcesses[i];
						if (pd->event->id() == event.id()  &&  (pd->flags & ProcData::PRE_ACTION))
						{
							kDebug() << "Already executing pre-DISPLAY command";
							return pd->process;   // already executing - don't duplicate the action
						}
					}

					QString command = event.preAction();
					kDebug() << "Pre-DISPLAY command:" << command;
					int flags = (reschedule ? ProcData::RESCHEDULE : 0) | (allowDefer ? ProcData::ALLOW_DEFER : 0);
					if (doShellCommand(command, event, &alarm, (flags | ProcData::PRE_ACTION)))
					{
						AlarmCalendar::resources()->setAlarmPending(&event);
						return result;     // display the message after the command completes
					}
					// Error executing command
					if (event.cancelOnPreActionError())
					{
						// Cancel the rest of the alarm execution
						kDebug() << event.id() << ": pre-action failed: cancelled";
						if (reschedule)
							rescheduleAlarm(event, alarm, true);
						return 0;
					}
					// Display the message even though it failed
				}
			}
			if (!win
			     ||  (!win->hasDefer() && !alarm.repeatAtLogin())
			     ||  replaceReminder)
			{
				// Either there isn't already a message for this event,
				// or there is a repeat-at-login message with no Defer
				// button, which needs to be replaced with a new message,
				// or the caption needs to be changed from "Reminder" to "Message".
				if (win)
					win->setRecreating();    // prevent post-alarm actions
				delete win;
				int flags = (reschedule ? 0 : MessageWin::NO_RESCHEDULE) | (allowDefer ? 0 : MessageWin::NO_DEFER);
				(new MessageWin(&event, alarm, flags))->show();
			}
			else
			{
				// Raise the existing message window and replay any sound
				win->repeat(alarm);    // N.B. this reschedules the alarm
			}
			break;
		}
		case KAAlarm::COMMAND:
			if (!ShellProcess::authorised())
				setEventCommandError(event, KAEvent::CMD_ERROR);
			else
			{
				result = execCommandAlarm(event, alarm);
				if (reschedule)
					rescheduleAlarm(event, alarm, true);
			}
			break;
		case KAAlarm::EMAIL:
		{
			kDebug() << "EMAIL to:" << event.emailAddresses(",");
			QStringList errmsgs;
			KAMail::JobData data(event, alarm, reschedule, (reschedule || allowDefer));
			data.queued = true;
			int ans = KAMail::send(data, errmsgs);
			if (ans)
			{
				// The email has either been sent or failed - not queued
				if (ans < 0)
					result = 0;  // failure
				data.queued = false;
				emailSent(data, errmsgs, (ans > 0));
			}
			else
			{
				result = (void*)-1;   // email has been queued
			}
			if (reschedule)
				rescheduleAlarm(event, alarm, true);
			break;
		}
		default:
			return 0;
	}
	return result;
}

/******************************************************************************
* Called when sending an email has completed.
*/
void KAlarmApp::emailSent(KAMail::JobData& data, const QStringList& errmsgs, bool copyerr)
{
	if (!errmsgs.isEmpty())
	{
		// Some error occurred, although the email may have been sent successfully
		if (copyerr)
			kDebug() << "Copy error:" << errmsgs[1];
		else
			kDebug() << "Failed:" << errmsgs[1];
		MessageWin::showError(data.event, data.alarm.dateTime(), errmsgs);
	}
	else if (data.queued)
		emit execAlarmSuccess();
}

/******************************************************************************
* Execute the command specified in a command alarm.
* To connect to the output ready signals of the process, specify a slot to be
* called by supplying 'receiver' and 'slot' parameters.
*/
ShellProcess* KAlarmApp::execCommandAlarm(const KAEvent& event, const KAAlarm& alarm, const QObject* receiver, const char* slot)
{
	int flags = (event.commandXterm()   ? ProcData::EXEC_IN_XTERM : 0)
	          | (event.commandDisplay() ? ProcData::DISP_OUTPUT : 0);
	QString command = event.cleanText();
	if (event.commandScript())
	{
		// Store the command script in a temporary file for execution
		kDebug() << "Script";
		QString tmpfile = createTempScriptFile(command, false, event, alarm);
		if (tmpfile.isEmpty())
		{
			setEventCommandError(event, KAEvent::CMD_ERROR);
			return 0;
		}
		return doShellCommand(tmpfile, event, &alarm, (flags | ProcData::TEMP_FILE), receiver, slot);
	}
	else
	{
		kDebug() << command;
		return doShellCommand(command, event, &alarm, flags, receiver, slot);
	}
}

/******************************************************************************
* Execute a shell command line specified by an alarm.
* If the PRE_ACTION bit of 'flags' is set, the alarm will be executed via
* execAlarm() once the command completes, the execAlarm() parameters being
* derived from the remaining bits in 'flags'.
* 'flags' must contain the bit PRE_ACTION or POST_ACTION if and only if it is
* a pre- or post-alarm action respectively.
* To connect to the output ready signals of the process, specify a slot to be
* called by supplying 'receiver' and 'slot' parameters.
*/
ShellProcess* KAlarmApp::doShellCommand(const QString& command, const KAEvent& event, const KAAlarm* alarm, int flags, const QObject* receiver, const char* slot)
{
	kDebug() << command << "," << event.id();
	QIODevice::OpenMode mode = QIODevice::WriteOnly;
	QString cmd;
	QString tmpXtermFile;
	if (flags & ProcData::EXEC_IN_XTERM)
	{
		// Execute the command in a terminal window.
		cmd = composeXTermCommand(command, event, alarm, flags, tmpXtermFile);
	}
	else
	{
		cmd = command;
		mode = QIODevice::ReadWrite;
	}

	ProcData* pd = 0;
	ShellProcess* proc = 0;
	if (!cmd.isEmpty())
	{
		proc = new ShellProcess(cmd);
		proc->setOutputChannelMode(KProcess::MergedChannels);   // combine stdout & stderr
		connect(proc, SIGNAL(shellExited(ShellProcess*)), SLOT(slotCommandExited(ShellProcess*)));
		if ((flags & ProcData::DISP_OUTPUT)  &&  receiver && slot)
		{
			connect(proc, SIGNAL(receivedStdout(ShellProcess*)), receiver, slot);
			connect(proc, SIGNAL(receivedStderr(ShellProcess*)), receiver, slot);
		}
		if (mode == QIODevice::ReadWrite  &&  !event.logFile().isEmpty())
		{
			// Output is to be appended to a log file.
			// Set up a logging process to write the command's output to.
			QString heading;
			if (alarm  &&  alarm->dateTime().isValid())
			{
				QString dateTime = alarm->dateTime().formatLocale();
				heading.sprintf("\n******* KAlarm %s *******\n", dateTime.toLatin1().data());
			}
			else
				heading = QLatin1String("\n******* KAlarm *******\n");
			QFile logfile(event.logFile());
			if (logfile.open(QIODevice::Append | QIODevice::Text))
			{
				QTextStream out(&logfile);
				out << heading;
				logfile.close();
			}
			proc->setStandardOutputFile(event.logFile(), QIODevice::Append);
		}
		pd = new ProcData(proc, new KAEvent(event), (alarm ? new KAAlarm(*alarm) : 0), flags);
		if (flags & ProcData::TEMP_FILE)
			pd->tempFiles += command;
		if (!tmpXtermFile.isEmpty())
			pd->tempFiles += tmpXtermFile;
		mCommandProcesses.append(pd);
		if (proc->start(mode))
			return proc;
	}

	// Error executing command - report it
	kWarning() << "Command failed to start";
	commandErrorMsg(proc, event, alarm, flags);
	if (pd)
	{
		mCommandProcesses.removeAt(mCommandProcesses.indexOf(pd));
		delete pd;
	}
	return 0;
}

/******************************************************************************
* Compose a command line to execute the given command in a terminal window.
* 'tempScriptFile' receives the name of a temporary script file which is
* invoked by the command line, if applicable.
* Reply = command line, or empty string if error.
*/
QString KAlarmApp::composeXTermCommand(const QString& command, const KAEvent& event, const KAAlarm* alarm, int flags, QString& tempScriptFile) const
{
	kDebug() << command << "," << event.id();
	tempScriptFile.clear();
	QString cmd = Preferences::cmdXTermCommand();
	cmd.replace("%t", KGlobal::mainComponent().aboutData()->programName());     // set the terminal window title
	if (cmd.indexOf("%C") >= 0)
	{
		// Execute the command from a temporary script file
		if (flags & ProcData::TEMP_FILE)
			cmd.replace("%C", command);    // the command is already calling a temporary file
		else
		{
			tempScriptFile = createTempScriptFile(command, true, event, *alarm);
			if (tempScriptFile.isEmpty())
				return QString();
			cmd.replace("%C", tempScriptFile);    // %C indicates where to insert the command
		}
	}
	else if (cmd.indexOf("%W") >= 0)
	{
		// Execute the command from a temporary script file,
		// with a sleep after the command is executed
		tempScriptFile = createTempScriptFile(command + QLatin1String("\nsleep 86400\n"), true, event, *alarm);
		if (tempScriptFile.isEmpty())
			return QString();
		cmd.replace("%W", tempScriptFile);    // %w indicates where to insert the command
	}
	else if (cmd.indexOf("%w") >= 0)
	{
		// Append a sleep to the command.
		// Quote the command in case it contains characters such as [>|;].
		QString exec = KShell::quoteArg(command + QLatin1String("; sleep 86400"));
		cmd.replace("%w", exec);    // %w indicates where to insert the command string
	}
	else
	{
		// Set the command to execute.
		// Put it in quotes in case it contains characters such as [>|;].
		QString exec = KShell::quoteArg(command);
		if (cmd.indexOf("%c") >= 0)
			cmd.replace("%c", exec);    // %c indicates where to insert the command string
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
	KTemporaryFile tmpFile;
	tmpFile.setAutoRemove(false);     // don't delete file when it is destructed
	if (!tmpFile.open())
		kError() << "Unable to create a temporary script file";
	else
	{
		tmpFile.setPermissions(QFile::ReadUser | QFile::WriteUser | QFile::ExeUser);
		QTextStream stream(&tmpFile);
		if (insertShell)
			stream << "#!" << ShellProcess::shellPath() << "\n";
		stream << command;
		stream.flush();
		if (tmpFile.error() != QFile::NoError)
			kError() << "Error" << tmpFile.errorString() << " writing to temporary script file";
		else
			return tmpFile.fileName();
	}

	QStringList errmsgs(i18nc("@info", "Error creating temporary script file"));
	MessageWin::showError(event, alarm.dateTime(), errmsgs, QLatin1String("Script"));
	return QString();
}

/******************************************************************************
* Called when a command alarm's execution completes.
*/
void KAlarmApp::slotCommandExited(ShellProcess* proc)
{
	kDebug();
	// Find this command in the command list
	for (int i = 0, end = mCommandProcesses.count();  i < end;  ++i)
	{
		ProcData* pd = mCommandProcesses[i];
		if (pd->process == proc)
		{
			// Found the command. Check its exit status.
			bool executeAlarm = pd->preAction();
			ShellProcess::Status status = proc->status();
			if (status == ShellProcess::SUCCESS  &&  !proc->exitCode())
			{
				kDebug() << pd->event->id() << ": SUCCESS";
				clearEventCommandError(*pd->event, pd->preAction() ? KAEvent::CMD_ERROR_PRE
				                                 : pd->postAction() ? KAEvent::CMD_ERROR_POST
				                                 : KAEvent::CMD_ERROR);
			}
			else
			{
				QString errmsg = proc->errorMessage();
				if (status == ShellProcess::SUCCESS  ||  status == ShellProcess::NOT_FOUND)
					kWarning() << pd->event->id() << ":" << errmsg << "exit status =" << status << ", code =" << proc->exitCode();
				else
					kWarning() << pd->event->id() << ":" << errmsg << "exit status =" << status;
				if (pd->messageBoxParent)
				{
					// Close the existing informational KMessageBox for this process
					QList<KDialog*> dialogs = pd->messageBoxParent->findChildren<KDialog*>();
					if (!dialogs.isEmpty())
					    delete dialogs[0];
					setEventCommandError(*pd->event, pd->preAction() ? KAEvent::CMD_ERROR_PRE
					                               : pd->postAction() ? KAEvent::CMD_ERROR_POST
					                               : KAEvent::CMD_ERROR);
					if (!pd->tempFile())
					{
						errmsg += '\n';
						errmsg += proc->command();
					}
					KMessageBox::error(pd->messageBoxParent, errmsg);
				}
				else
					commandErrorMsg(proc, *pd->event, pd->alarm, pd->flags);

				if (executeAlarm  &&  pd->event->cancelOnPreActionError())
				{
					kDebug() << pd->event->id() << ": pre-action failed: cancelled";
					if (pd->reschedule())
						rescheduleAlarm(*pd->event, *pd->alarm, true);
					executeAlarm = false;
				}
			}
			if (pd->preAction())
				AlarmCalendar::resources()->setAlarmPending(pd->event, false);
			if (executeAlarm)
				execAlarm(*pd->event, *pd->alarm, pd->reschedule(), pd->allowDefer(), true);
			mCommandProcesses.removeAt(i);
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
void KAlarmApp::commandErrorMsg(const ShellProcess* proc, const KAEvent& event, const KAAlarm* alarm, int flags)
{
	KAEvent::CmdErrType cmderr;
	QStringList errmsgs;
	QString dontShowAgain;
	if (flags & ProcData::PRE_ACTION)
	{
		errmsgs += i18nc("@info", "Pre-alarm action:");
		dontShowAgain = QLatin1String("Pre");
		cmderr = KAEvent::CMD_ERROR_PRE;
	}
	else if (flags & ProcData::POST_ACTION)
	{
		errmsgs += i18nc("@info", "Post-alarm action:");
		dontShowAgain = QLatin1String("Post");
		cmderr = (event.commandError() == KAEvent::CMD_ERROR_PRE)
		       ? KAEvent::CMD_ERROR_PRE_POST : KAEvent::CMD_ERROR_POST;
	}
	else
	{
		dontShowAgain = QLatin1String("Exec");
		cmderr = KAEvent::CMD_ERROR;
	}

	// Record the alarm's error status
	setEventCommandError(event, cmderr);
	// Display an error message
	if (proc)
	{
		errmsgs += proc->errorMessage();
		if (!(flags & ProcData::TEMP_FILE))
			errmsgs += proc->command();
		dontShowAgain += QString::number(proc->status());
	}
	MessageWin::showError(event, (alarm ? alarm->dateTime() : DateTime()), errmsgs, dontShowAgain);
}

/******************************************************************************
* Notes that an informational KMessageBox is displayed for this process.
*/
void KAlarmApp::commandMessage(ShellProcess* proc, QWidget* parent)
{
	// Find this command in the command list
	for (int i = 0, end = mCommandProcesses.count();  i < end;  ++i)
	{
		ProcData* pd = mCommandProcesses[i];
		if (pd->process == proc)
		{
			pd->messageBoxParent = parent;
			break;
		}
	}
}

/******************************************************************************
* Return a D-Bus interface object for KSpeech.
* The KTTSD D-Bus service is started if necessary.
* If the service cannot be started, 'error' is set to an error text.
*/
OrgKdeKSpeechInterface* KAlarmApp::kspeechInterface(QString& error) const
{
	error.clear();
	QDBusConnection client = QDBusConnection::sessionBus();
	if (!client.interface()->isServiceRegistered(KTTSD_DBUS_SERVICE))
	{
		// kttsd is not running, so start it
		delete mKSpeech;
		mKSpeech = 0;
		if (KToolInvocation::startServiceByDesktopName(QLatin1String("kttsd"), QStringList(), &error))
		{
			kDebug() << "Failed to start kttsd:" << error;
			return 0;
		}
	}
	if (!mKSpeech)
	{
		mKSpeech = new OrgKdeKSpeechInterface(KTTSD_DBUS_SERVICE, KTTDS_DBUS_PATH, QDBusConnection::sessionBus());
		mKSpeech->setParent(theApp());
		mKSpeech->setApplicationName(KGlobal::mainComponent().aboutData()->programName());
		mKSpeech->setDefaultPriority(KSpeech::jpMessage);
	}
	return mKSpeech;
}

/******************************************************************************
* Called when a D-Bus service unregisters.
* If it's the KTTSD service, delete the KSpeech interface object.
*/
void KAlarmApp::slotDBusServiceUnregistered(const QString& serviceName)
{
	if (serviceName == KTTSD_DBUS_SERVICE)
	{
		delete mKSpeech;
		mKSpeech = 0;
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
	{
		kDebug() << "first time";
		if (!mStartOfDay.isValid())
			changeStartOfDay();     // start of day time has changed, so adjust date-only alarms

		/* Need to open the display calendar now, since otherwise if display
		 * alarms are immediately due, they will often be processed while
		 * MessageWin::redisplayAlarms() is executing open() (but before open()
		 * completes), which causes problems!!
		 */
		AlarmCalendar::displayCalendar()->open();

		AlarmCalendar::resources()->open();
		setArchivePurgeDays();

		firstTime = false;
	}

	if (!calendarOnly)
		startProcessQueue();      // start processing the execution queue
	return true;
}

void setEventCommandError(const KAEvent& event, KAEvent::CmdErrType err)
{
	if (err == KAEvent::CMD_ERROR_POST  &&  event.commandError() == KAEvent::CMD_ERROR_PRE)
		err = KAEvent::CMD_ERROR_PRE_POST;
	event.setCommandError(err);
	KAEvent* ev = AlarmCalendar::resources()->event(event.id());
	if (ev  &&  ev->commandError() != err)
		ev->setCommandError(err);
	EventListModel::alarms()->updateCommandError(event.id());
}

void clearEventCommandError(const KAEvent& event, KAEvent::CmdErrType err)
{
	KAEvent::CmdErrType newerr = static_cast<KAEvent::CmdErrType>(event.commandError() & ~err);
	event.setCommandError(newerr);
	KAEvent* ev = AlarmCalendar::resources()->event(event.id());
	if (ev)
	{
		newerr = static_cast<KAEvent::CmdErrType>(ev->commandError() & ~err);
		ev->setCommandError(newerr);
	}
	EventListModel::alarms()->updateCommandError(event.id());
}


KAlarmApp::ProcData::ProcData(ShellProcess* p, KAEvent* e, KAAlarm* a, int f)
	: process(p),
	  event(e),
	  alarm(a),
	  messageBoxParent(0),
	  flags(f)
{ }

KAlarmApp::ProcData::~ProcData()
{
	while (!tempFiles.isEmpty())
	{
		// Delete the temporary file called by the XTerm command
		QFile f(tempFiles.first());
		f.remove();
		tempFiles.removeFirst();
	}
	delete process;
	delete event;
	delete alarm;
}
