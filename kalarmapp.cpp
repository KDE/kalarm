/*
 *  kalarmapp.cpp  -  the KAlarm application object
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <stdlib.h>
#include <ctype.h>
#include <iostream>

#include <qfile.h>

#include <kcmdlineargs.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <dcopclient.h>
#include <kprocess.h>
#include <kfileitem.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kwinmodule.h>
#include <kdebug.h>

#include <kalarmd/clientinfo.h>

#include <libkcal/calformat.h>

#include "alarmcalendar.h"
#include "mainwindow.h"
#include "messagewin.h"
#include "daemongui.h"
#include "traywindow.h"
#include "prefsettings.h"
#include "prefdlg.h"
#include "kalarmapp.moc"

extern QCString execArguments;

#define     DAEMON_APP_NAME_DEF    "kalarmd"
const char* DCOP_OBJECT_NAME     = "display";
const char* GUI_DCOP_OBJECT_NAME = "tray";
const char* DAEMON_APP_NAME      = DAEMON_APP_NAME_DEF;
const char* DAEMON_DCOP_OBJECT   = "ad";

int         marginKDE2 = 0;

static bool convWakeTime(const QCString timeParam, QDateTime&, bool& noTime);

KAlarmApp*  KAlarmApp::theInstance = 0L;
int         KAlarmApp::activeCount = 0;


/******************************************************************************
* Construct the application.
*/
KAlarmApp::KAlarmApp()
	: KUniqueApplication(),
	  mDcopHandler(0L),
	  mDaemonGuiHandler(0L),
	  mTrayWindow(0L),
	  mCalendar(new AlarmCalendar),
	  mSettings(new Settings(0L)),
	  mDaemonCheckInterval(0),
	  mDaemonRegistered(false),
	  mDaemonRunning(false),
	  mSessionClosingDown(false)
{
#if KDE_VERSION < 290
	marginKDE2 = KDialog::marginHint();
#endif
	mSettings->loadSettings();
	connect(mSettings, SIGNAL(settingsChanged()), this, SLOT(slotSettingsChanged()));
	CalFormat::setApplication(aboutData()->programName(),
	                          QString::fromLatin1("-//K Desktop Environment//NONSGML %1 " KALARM_VERSION "//EN")
	                                       .arg(aboutData()->programName()));
	readDaemonCheckInterval();
	KWinModule wm;
	mKDEDesktop             = wm.systemTrayWindows().count();
	mOldRunInSystemTray     = mKDEDesktop && mSettings->runInSystemTray();
	mDisableAlarmsIfStopped = mOldRunInSystemTray && mSettings->disableAlarmsIfStopped();
	mStartOfDay             = mSettings->startOfDay();
	if (mSettings->startOfDayChanged())
		mStartOfDay.setHMS(100,0,0);    // start of day time has changed: flag it as invalid

	// Set up actions used by more than one menu
	KActionCollection* actions = new KActionCollection(this);
	mActionAlarmEnable = new ActionAlarmsEnabled(Qt::CTRL+Qt::Key_E, this, SLOT(toggleAlarmsEnabled()), actions, "alarmenable");
	mActionPrefs       = KStdAction::preferences(this, SLOT(slotPreferences()), actions);
	mActionDaemonPrefs = new KAction(i18n("Configure Alarm &Daemon..."), mActionPrefs->iconSet(),
	                                 0, this, SLOT(slotDaemonPreferences()), actions, "confdaemon");
}

/******************************************************************************
*/
KAlarmApp::~KAlarmApp()
{
	mCalendar->close();
}

/******************************************************************************
* Return the one and only KAlarmApp instance.
* If it doesn't already exist, it is created first.
*/
KAlarmApp* KAlarmApp::getInstance()
{
	if (!theInstance)
		theInstance = new KAlarmApp;
	return theInstance;
}

/******************************************************************************
* Restore the saved session if required.
*/
bool KAlarmApp::restoreSession()
{
	if (!isRestored())
		return false;

	// Process is being restored by session management.
	kdDebug(5950) << "KAlarmApp::restoreSession(): Restoring\n";
	int exitCode = !initCheck(true);     // open the calendar file (needed for main windows)
	KAlarmMainWindow* trayParent = 0L;
	for (int i = 1;  KMainWindow::canBeRestored(i);  ++i)
	{
		QString type = KMainWindow::classNameOfToplevel(i);
		if (type == QString::fromLatin1("KAlarmMainWindow"))
		{
			KAlarmMainWindow* win = new KAlarmMainWindow(true);
			win->restore(i, false);
			if (win->hiddenTrayParent())
				trayParent = win;
			else
				win->show();
		}
		else if (type == QString::fromLatin1("MessageWin"))
		{
			MessageWin* win = new MessageWin;
			win->restore(i, false);
			if (win->errorMessage())
				delete win;
			else
				win->show();
		}
	}
	initCheck();           // register with the alarm daemon

	// Display the system tray icon if it is configured to be autostarted,
	// or if we're in run-in-system-tray mode.
	if (mSettings->autostartTrayIcon()
	||  KAlarmMainWindow::count()  &&  runInSystemTray())
		displayTrayIcon(true, trayParent);

	quitIf(exitCode);           // quit if no windows are open
	return true;
}

/******************************************************************************
* Called for a KUniqueApplication when a new instance of the application is
* started.
*/
int KAlarmApp::newInstance()
{
	kdDebug(5950)<<"KAlarmApp::newInstance()\n";
	++activeCount;
	int exitCode = 0;               // default = success
	static bool firstInstance = true;
	bool skip = (firstInstance && isRestored());
	firstInstance = false;
	if (!skip)
	{
		QString usage;
		KCmdLineArgs* args = KCmdLineArgs::parsedArgs();

		// Use a 'do' loop which is executed only once to allow easy error exits.
		// Errors use 'break' to skip to the end of the function.

		// Note that DCOP handling is only set up once the command line parameters
		// have been checked, since we mustn't register with the alarm daemon only
		// to quit immediately afterwards.
		do
		{
			if (args->isSet("stop"))
			{
				// Stop the alarm daemon
				kdDebug(5950)<<"KAlarmApp::newInstance(): stop\n";
				args->clear();      // free up memory
				if (!stopDaemon())
				{
					exitCode = 1;
					break;
				}
			}
			else
			if (args->isSet("reset"))
			{
				// Reset the alarm daemon
				kdDebug(5950)<<"KAlarmApp::newInstance(): reset\n";
				args->clear();      // free up memory
				setUpDcop();        // we're now ready to handle DCOP calls, so set up handlers
				resetDaemon();
			}
			else
			if (args->isSet("tray"))
			{
				// Display only the system tray icon
				kdDebug(5950)<<"KAlarmApp::newInstance(): tray\n";
				args->clear();      // free up memory
				setUpDcop();        // we're now ready to handle DCOP calls, so set up handlers
				if (!mKDEDesktop
				||  !initCheck())   // open the calendar, register with daemon
				{
					exitCode = 1;
					break;
				}
				if (runInSystemTray()  &&  !KAlarmMainWindow::count())
					new KAlarmMainWindow;
				if (!displayTrayIcon(true))
				{
					exitCode = 1;
					break;
				}
			}
			else
			if (args->isSet("handleEvent")  ||  args->isSet("triggerEvent")  ||  args->isSet("cancelEvent")  ||  args->isSet("calendarURL")
/* superseded */	||  args->isSet("displayEvent"))  // kept for compatibility
			{
				// Display or delete the event with the specified event ID
				kdDebug(5950)<<"KAlarmApp::newInstance(): handle event\n";
				EventFunc function = EVENT_HANDLE;
				int count = 0;
				const char* option = 0;
				if (args->isSet("handleEvent"))   { function = EVENT_HANDLE;  option = "handleEvent";  ++count; }
/* superseded */		if (args->isSet("displayEvent"))  { function = EVENT_TRIGGER;  option = "displayEvent";  ++count; }
				if (args->isSet("triggerEvent"))  { function = EVENT_TRIGGER;  option = "triggerEvent";  ++count; }
				if (args->isSet("cancelEvent"))   { function = EVENT_CANCEL;  option = "cancelEvent";  ++count; }
				if (!count)
				{
					usage = i18n("%1 requires %2, %3 or %4").arg(QString::fromLatin1("--calendarURL")).arg(QString::fromLatin1("--handleEvent")).arg(QString::fromLatin1("--triggerEvent")).arg(QString::fromLatin1("--cancelEvent"));
					break;
				}
				if (count > 1)
				{
					usage = i18n("%1, %2, %3 mutually exclusive").arg(QString::fromLatin1("--handleEvent")).arg(QString::fromLatin1("--triggerEvent")).arg(QString::fromLatin1("--cancelEvent"));
					break;
				}
				if (!initCheck(true))   // open the calendar, don't register with daemon yet
				{
					exitCode = 1;
					break;
				}
				if (args->isSet("calendarURL"))
				{
					QString calendarUrl = args->getOption("calendarURL");
					if (KURL(calendarUrl).url() != mCalendar->urlString())
					{
						usage = i18n("%1: wrong calendar file").arg(QString::fromLatin1("--calendarURL"));
						break;
					}
				}
				QString eventID = args->getOption(option);
				args->clear();      // free up memory
				setUpDcop();        // we're now ready to handle DCOP calls, so set up handlers
				if (!handleEvent(eventID, function))
				{
					exitCode = 1;
					break;
				}
			}
			else
			if (args->isSet("file")  ||  args->isSet("exec")  ||  args->count())
			{
				// Display a message or file, or execute a command
				KAlarmAlarm::Type type = KAlarmAlarm::MESSAGE;
				QCString alMessage;
				if (args->isSet("file"))
				{
					kdDebug(5950)<<"KAlarmApp::newInstance(): file\n";
					if (args->isSet("exec"))
					{
						usage = i18n("%1 incompatible with %2").arg(QString::fromLatin1("--exec")).arg(QString::fromLatin1("--file"));
						break;
					}
					if (args->count())
					{
						usage = i18n("message incompatible with %1").arg(QString::fromLatin1("--file"));
						break;
					}
					alMessage = args->getOption("file");
					type = KAlarmAlarm::FILE;
				}
				else if (args->isSet("exec"))
				{
					kdDebug(5950)<<"KAlarmApp::newInstance(): exec\n";
					alMessage = execArguments;
					type = KAlarmAlarm::COMMAND;
				}
#ifdef KALARM_EMAIL
				else if (args->isSet("mail"))
				{
					kdDebug(5950)<<"KAlarmApp::newInstance(): mail\n";
					if (args->isSet("subject"))
						alSubject = args->getOption("subject");
					alAddrs = args->getOptionList("mail");
					for (QCStringList::iterator i = alAddrs.begin();  i != alAddrs.end();  ++i)
					{
						QString addr = QString::fromLatin1(*i);
						if (!KAMail::checkEmailAddress(addr))
							USAGE(i18n("%1: invalid email address").arg(QString::fromLatin1("--mail")))
						alAddresses += addr;
					}
					alMessage = args->arg(0);
					type = KAlarmAlarm::EMAIL;
				}
#endif
				else
				{
					kdDebug(5950)<<"KAlarmApp::newInstance(): message\n";
					alMessage = args->arg(0);
				}

				bool      alarmNoTime = false;
				QDateTime alarmTime, endTime;
				QColor    bgColour = mSettings->defaultBgColour();
				int       repeatCount = 0;
				int       repeatInterval = 0;
				KAlarmEvent::RecurType recurType = KAlarmEvent::NO_RECUR;
				if (args->isSet("color"))
				{
					// Colour is specified
					QCString colourText = args->getOption("color");
					if (static_cast<const char*>(colourText)[0] == '0'
					&&  tolower(static_cast<const char*>(colourText)[1]) == 'x')
						colourText.replace(0, 2, "#");
					bgColour.setNamedColor(colourText);
					if (!bgColour.isValid())
					{
						usage = i18n("Invalid %1 parameter").arg(QString::fromLatin1("--color"));
						break;
					}
				}

				if (args->isSet("time"))
				{
					QCString dateTime = args->getOption("time");
					if (!convWakeTime(dateTime, alarmTime, alarmNoTime))
					{
						usage = i18n("Invalid %1 parameter").arg(QString::fromLatin1("--time"));
						break;
					}
				}
				else
					alarmTime = QDateTime::currentDateTime();

				if (args->isSet("interval"))
				{
					// Repeat count is specified
					if (args->isSet("login"))
					{
						usage = i18n("%1 incompatible with %2").arg(QString::fromLatin1("--login")).arg(QString::fromLatin1("--interval"));
						break;
					}
					if (!args->isSet("repeat")  &&  !args->isSet("until"))
					{
						usage = i18n("%1 requires %2 or %3").arg(QString::fromLatin1("--interval")).arg(QString::fromLatin1("--repeat")).arg(QString::fromLatin1("--until"));
						break;
					}
					bool ok;
					if (args->isSet("repeat"))
					{
						repeatCount = args->getOption("repeat").toInt(&ok);
						if (!ok || !repeatCount || repeatCount < -1)
						{
							usage = i18n("Invalid %1 parameter").arg(QString::fromLatin1("--repeat"));
							break;
						}
					}
					else
					{
						QCString dateTime = args->getOption("until");
						if (!convWakeTime(dateTime, endTime, alarmNoTime))
						{
							usage = i18n("Invalid %1 parameter").arg(QString::fromLatin1("--until"));
							break;
						}
						if (endTime < alarmTime)
						{
							usage = i18n("%1 earlier than %2").arg(QString::fromLatin1("--until")).arg(QString::fromLatin1("--time"));
							break;
						}
					}

					// Get the recurrence interval
					ok = true;
					uint interval = 0;
					QCString optval = args->getOption("interval");
					uint length = optval.length();
					switch (optval[length - 1])
					{
						case 'Y':
							recurType = KAlarmEvent::ANNUAL_DATE;
							optval = optval.left(length - 1);
							break;
						case 'W':
							recurType = KAlarmEvent::WEEKLY;
							optval = optval.left(length - 1);
							break;
						case 'D':
							recurType = KAlarmEvent::DAILY;
							optval = optval.left(length - 1);
							break;
						case 'M':
						{
							int i = optval.find('H');
							if (i < 0)
								recurType = KAlarmEvent::MONTHLY_DAY;
							else
							{
								recurType = KAlarmEvent::MINUTELY;
								interval = optval.left(i).toUInt(&ok) * 60;
								optval = optval.right(length - i - 1);
							}
							break;
						}
						default:       // should be a digit
							recurType = KAlarmEvent::MINUTELY;
							break;
					}
					if (ok)
						interval += optval.toUInt(&ok);
					repeatInterval = static_cast<int>(interval);
					if (!ok || repeatInterval < 0)
					{
						usage = i18n("Invalid %1 parameter").arg(QString::fromLatin1("--interval"));
						break;
					}
				}
				else
				{
					if (args->isSet("repeat"))
					{
						usage = i18n("%1 requires %2").arg(QString::fromLatin1("--repeat")).arg(QString::fromLatin1("--interval"));
						break;
					}
					if (args->isSet("until"))
					{
						usage = i18n("%1 requires %2").arg(QString::fromLatin1("--until")).arg(QString::fromLatin1("--interval"));
						break;
					}
				}

				QCString audioFile;
				if (args->isSet("sound"))
				{
					// Play a sound with the alarm
					if (args->isSet("beep"))
					{
						usage = i18n("%1 incompatible with %2").arg(QString::fromLatin1("--beep")).arg(QString::fromLatin1("--sound"));
						break;
					}
					audioFile = args->getOption("sound");
				}

				int flags = 0;
				if (args->isSet("ack-confirm"))
					flags |= KAlarmEvent::CONFIRM_ACK;
				if (args->isSet("beep"))
					flags |= KAlarmEvent::BEEP;
				if (args->isSet("late-cancel"))
					flags |= KAlarmEvent::LATE_CANCEL;
				if (args->isSet("login"))
					flags |= KAlarmEvent::REPEAT_AT_LOGIN;
				args->clear();      // free up memory

				// Display or schedule the event
				setUpDcop();        // we're now ready to handle DCOP calls, so set up handlers
				if (!scheduleEvent(alMessage, alarmTime, bgColour, flags, audioFile, type, recurType,
				                   repeatInterval, repeatCount, endTime))
				{
					exitCode = 1;
					break;
				}
			}
			else
			{
				// No arguments - run interactively & display the main window
				kdDebug(5950)<<"KAlarmApp::newInstance(): interactive\n";
				if (args->isSet("ack-confirm"))
					usage += QString::fromLatin1("--ack-confirm ");
				if (args->isSet("beep"))
					usage += QString::fromLatin1("--beep ");
				if (args->isSet("color"))
					usage += QString::fromLatin1("--color ");
				if (args->isSet("late-cancel"))
					usage += QString::fromLatin1("--late-cancel ");
				if (args->isSet("login"))
					usage += QString::fromLatin1("--login ");
				if (args->isSet("sound"))
					usage += QString::fromLatin1("--sound ");
				if (args->isSet("time"))
					usage += QString::fromLatin1("--time ");
				if (!usage.isEmpty())
				{
					usage += i18n(": option(s) only valid with a message/%1/%2").arg(QString::fromLatin1("--file")).arg(QString::fromLatin1("--exec"));
					break;
				}

				args->clear();      // free up memory
				setUpDcop();        // we're now ready to handle DCOP calls, so set up handlers
				if (!initCheck())
				{
					exitCode = 1;
					break;
				}

				(new KAlarmMainWindow)->show();
			}
		} while (0);    // only execute once

		if (!usage.isEmpty())
		{
			// Note: we can't use args->usage() since that also quits any other
			// running 'instances' of the program.
			std::cerr << usage << i18n("\nUse --help to get a list of available command line options.\n");
			exitCode = 1;
		}
	}
	--activeCount;

	// Quit the application if this was the last/only running "instance" of the program.
	// Executing 'return' doesn't work very well since the program continues to
	// run if no windows were created.
	quitIf(exitCode);
	return exitCode;
}

/******************************************************************************
* Quit the program if there are no more "instances" running.
*/
void KAlarmApp::quitIf(int exitCode)
{
	if (activeCount <= 0  &&  !KAlarmMainWindow::count()  &&  !MessageWin::instanceCount()  &&  !mTrayWindow)
	{
		/* This was the last/only running "instance" of the program, so exit completely.
		 * First, change the name which we are registered with at the DCOP server. This is
		 * to ensure that the alarm daemon immediately sees us as not running. It prevents
		 * the following situation which otherwise has been observed:
		 *
		 * If KAlarm is not running and, for instance, it has registered more than one
		 * calendar at some time in the past, when the daemon checks pending alarms, it
		 * starts KAlarm to notify us of the first event. If this is for a different
		 * calendar from what KAlarm expects, we exit. But without DCOP re-registration,
		 * when the daemon then notifies us of the next event (from the correct calendar),
		 * it will still see KAlarm as registered with DCOP and therefore tells us via a
		 * DCOP call. The call of course never reaches KAlarm but the daemon sees it as
		 * successful. The result is that the alarm is never seen.
		 */
		kdDebug(5950) << "KAlarmApp::quitIf(" << exitCode << "): quitting" << endl;
		dcopClient()->registerAs(QCString(aboutData()->appName()) + "-quitting");
		exit(exitCode);
	}
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
* Called when the system tray main window is closed.
*/
void KAlarmApp::removeWindow(TrayWindow*)
{
	mTrayWindow = 0L;
	quitIf();
}

/******************************************************************************
*  Display or close the system tray icon.
*/
bool KAlarmApp::displayTrayIcon(bool show, KAlarmMainWindow* parent)
{
	if (show)
	{
		if (!mTrayWindow)
		{
			if (!mKDEDesktop)
				return false;
			mTrayWindow = new TrayWindow(parent ? parent : KAlarmMainWindow::firstWindow());
			connect(mTrayWindow, SIGNAL(deleted()), this, SIGNAL(trayIconToggled()));
			mTrayWindow->show();
			emit trayIconToggled();
		}
	}
	else if (mTrayWindow)
		delete mTrayWindow;
	return true;
}

/******************************************************************************
*  Display a main window.
*/
void KAlarmApp::displayMainWindow()
{
	KAlarmMainWindow* win = KAlarmMainWindow::firstWindow();
	if (!win)
	{
		if (initCheck())
			(new KAlarmMainWindow)->show();
	}
	else
	{
		// There is already a main window, so make it the active window
		win->showNormal();
		win->raise();
		win->setActiveWindow();
	}
}

KAlarmMainWindow* KAlarmApp::trayMainWindow() const
{
	return mTrayWindow ? mTrayWindow->assocMainWindow() : 0L;
}

/******************************************************************************
* Called when the Alarms Enabled action is selected.
* The alarm daemon is told to stop or start monitoring the calendar file as
* appropriate.
*/
void KAlarmApp::toggleAlarmsEnabled()
{
	if (mDaemonGuiHandler)
		mDaemonGuiHandler->setAlarmsEnabled(!mActionAlarmEnable->alarmsEnabled());
}

/******************************************************************************
*  Called when a Preferences menu item is selected.
*/
void KAlarmApp::slotPreferences()
{
	(new KAlarmPrefDlg(mSettings))->exec();
}

/******************************************************************************
* Called when a Configure Daemon menu item is selected.
* Displays the alarm daemon configuration dialog.
*/
void KAlarmApp::slotDaemonPreferences()
{
	KProcess proc;
	proc << locate("exe", QString::fromLatin1("kcmshell"));
	proc << QString::fromLatin1("alarmdaemonctrl");
	proc.start(KProcess::DontCare);
}

/******************************************************************************
*  Called when KAlarm settings have changed.
*/
void KAlarmApp::slotSettingsChanged()
{
	bool newRunInSysTray = mSettings->runInSystemTray()  &&  mKDEDesktop;
	if (newRunInSysTray != mOldRunInSystemTray)
	{
		// The system tray run mode has changed
		++activeCount;          // prevent the application from quitting
		KAlarmMainWindow* win = mTrayWindow ? mTrayWindow->assocMainWindow() : 0L;
		delete mTrayWindow;     // remove the system tray icon if it is currently shown
		mTrayWindow = 0L;
		mOldRunInSystemTray = newRunInSysTray;
		if (newRunInSysTray)
		{
			if (!KAlarmMainWindow::count())
				new KAlarmMainWindow;
			displayTrayIcon(true);
		}
		else
		{
			if (win  &&  win->isHidden())
				delete win;
			displayTrayIcon(true);
		}
		--activeCount;
	}

	bool newDisableIfStopped = mKDEDesktop && mSettings->runInSystemTray() && mSettings->disableAlarmsIfStopped();
	if (newDisableIfStopped != mDisableAlarmsIfStopped)
	{
		registerWithDaemon();     // re-register with the alarm daemon
		mDisableAlarmsIfStopped = newDisableIfStopped;
	}

	// Change alarm times for date-only alarms if the start of day time has changed
	if (mSettings->startOfDay() != mStartOfDay)
		changeStartOfDay();
}

/******************************************************************************
*  Change alarm times for date-only alarms after the start of day time has changed.
*/
void KAlarmApp::changeStartOfDay()
{
	if (KAlarmEvent::adjustStartOfDay(mCalendar->events()))
	{
		mCalendar->save();
		reloadDaemon();                      // tell the daemon to reread the calendar file
	}
	mSettings->updateStartOfDayCheck();  // now that calendar is updated, set OK flag in config file
	mStartOfDay = mSettings->startOfDay();
}

/******************************************************************************
*  Return whether the program is configured to be running in the system tray.
*/
bool KAlarmApp::runInSystemTray() const
{
	return mSettings->runInSystemTray()  &&  mKDEDesktop;
}

/******************************************************************************
* Called to schedule a new alarm, either in response to a DCOP notification or
* to command line options.
* Reply = true unless there was a parameter error or an error opening calendar file.
*/
bool KAlarmApp::scheduleEvent(const QString& message, const QDateTime& dateTime, const QColor& bg,
                              int flags, const QString& audioFile, KAlarmAlarm::Type type,
                              KAlarmEvent::RecurType recurType, int repeatInterval, int repeatCount,
                              const QDateTime& endTime)
	{
	kdDebug(5950) << "KAlarmApp::scheduleEvent(): " << message << endl;
	if (!dateTime.isValid())
		return false;
	QDateTime now = QDateTime::currentDateTime();
	if ((flags & KAlarmEvent::LATE_CANCEL)  &&  dateTime < now.addSecs(-maxLateness()))
		return true;               // alarm time was already expired too long ago
	QDateTime alarmTime = dateTime;
	// Round down to the nearest minute to avoid scheduling being messed up
	alarmTime.setTime(QTime(alarmTime.time().hour(), alarmTime.time().minute(), 0));
	bool display = (alarmTime <= now);

	KAlarmEvent event(alarmTime, message, bg, type, flags);
	if (!audioFile.isEmpty())
		event.setAudioFile(audioFile);
	switch (recurType)
	{
		case KAlarmEvent::MINUTELY:
			event.setRecurMinutely(repeatInterval, repeatCount, endTime);
			break;
		case KAlarmEvent::DAILY:
			event.setRecurDaily(repeatInterval, repeatCount, endTime.date());
			break;
		case KAlarmEvent::WEEKLY:
		{
			QBitArray days(7);
			days.setBit(QDate::currentDate().dayOfWeek() - 1);
			event.setRecurWeekly(repeatInterval, days, repeatCount, endTime.date());
			break;
		}
		case KAlarmEvent::MONTHLY_DAY:
		{
			QValueList<int> days;
			days.append(QDate::currentDate().day());
			event.setRecurMonthlyByDate(repeatInterval, days, repeatCount, endTime.date());
			break;
		}
		case KAlarmEvent::ANNUAL_DATE:
		{
			QValueList<int> months;
			months.append(QDate::currentDate().month());
			event.setRecurAnnualByDate(repeatInterval, months, repeatCount, endTime.date());
			break;
		}
		default:
			recurType = KAlarmEvent::NO_RECUR;
			break;
	}
	if (display)
	{
		// Alarm is due for display already
		execAlarm(event, event.firstAlarm(), false);
		if (recurType == KAlarmEvent::NO_RECUR
		||  event.setNextOccurrence(now) == KAlarmEvent::NO_OCCURRENCE)
			return true;
	}
	return addEvent(event, 0L);     // event instance will now belong to the calendar
}

/******************************************************************************
* Called in response to a DCOP notification by the alarm daemon that an event
* should be handled, i.e. displayed or cancelled.
* Optionally display the event. Delete the event from the calendar file and
* from every main window instance.
*/
void KAlarmApp::handleEvent(const QString& urlString, const QString& eventID, EventFunc function)
{
	kdDebug(5950) << "KAlarmApp::handleEvent(DCOP): " << eventID << endl;
	if (KURL(urlString).url() != mCalendar->urlString())
		kdError(5950) << "KAlarmApp::handleEvent(DCOP): wrong calendar file " << urlString << endl;
	else
		handleEvent(eventID, function);
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
	kdDebug(5950) << "KAlarmApp::handleEvent(): " << eventID << ", " << (function==EVENT_TRIGGER?"TRIGGER":function==EVENT_CANCEL?"CANCEL":function==EVENT_HANDLE?"HANDLE":"?") << endl;
	Event* kcalEvent = mCalendar->event(eventID);
	if (!kcalEvent)
	{
		kdError(5950) << "KAlarmApp::handleEvent(): event ID not found: " << eventID << endl;
		return false;
	}
	KAlarmEvent event(*kcalEvent);
	AlarmFunc alfunction = ALARM_TRIGGER;
	switch (function)
	{
		case EVENT_TRIGGER:
		{
			// Only trigger one alarm from the event - we don't
			// want multiple identical messages, for example.
			KAlarmAlarm alarm = event.firstAlarm();
			if (alarm.valid())
				handleAlarm(event, alarm, ALARM_TRIGGER, true);
			break;
		}
		case EVENT_CANCEL:
			deleteEvent(event, 0L, false);
			break;

		case EVENT_HANDLE:
		{
			QDateTime now = QDateTime::currentDateTime();
			bool updateCalAndDisplay = false;
			KAlarmAlarm displayAlarm;
			// Check all the alarms in turn.
			// Note that the main alarm is fetched before any other alarms.
			for (KAlarmAlarm alarm = event.firstAlarm();  alarm.valid();  alarm = event.nextAlarm(alarm))
			{
				// Check whether this alarm is due yet
				int secs = alarm.dateTime().secsTo(now);
				if (secs < 0)
				{
					kdDebug(5950) << "KAlarmApp::handleEvent(): alarm " << alarm.id() << ": not due\n";
					continue;
				}
				if (alarm.repeatAtLogin())
				{
					// Alarm is to be displayed at every login.
					// Check if the alarm has only just been set up.
					// (The alarm daemon will immediately notify that it is due
					//  since it is set up with a time in the past.)
					kdDebug(5950) << "KAlarmApp::handleEvent(): alarm " << alarm.id() << ": REPEAT_AT_LOGIN\n";
					if (secs < maxLateness())
						continue;

					// Check if the main alarm is already being displayed.
					// (We don't want to display both at the same time.)
					if (displayAlarm.valid())
						continue;
				}
				if (alarm.lateCancel())
				{
					// Alarm is due, and it is to be cancelled if late.
					kdDebug(5950) << "KAlarmApp::handleEvent(): alarm " << alarm.id() << ": LATE_CANCEL\n";
					bool late = false;
					bool cancel = false;
					if (event.anyTime())
					{
						// The alarm has no time, so cancel it if its date is past
						QDateTime limit(alarm.date().addDays(1), mSettings->startOfDay());
						if (now >= limit)
						{
							// It's too late to display the scheduled occurrence.
							// Find the last previous occurrence of the alarm.
							QDateTime next;
							KAlarmEvent::OccurType type = event.previousOccurrence(now, next);
							switch (type)
							{
								case KAlarmEvent::FIRST_OCCURRENCE:
								case KAlarmEvent::RECURRENCE_DATE:
								case KAlarmEvent::RECURRENCE_DATE_TIME:
								case KAlarmEvent::LAST_OCCURRENCE:
									limit.setDate(next.date().addDays(1));
									limit.setTime(mSettings->startOfDay());
									if (now >= limit)
									{
										if (type == KAlarmEvent::LAST_OCCURRENCE)
											cancel = true;
										else
											late = true;
									}
									break;
								case KAlarmEvent::NO_OCCURRENCE:
								default:
									late = true;
									break;
							}
						}
					}
					else
					{
						// The alarm is timed. Allow it to be just over a minute late before cancelling it.
						int maxlate = maxLateness();
						if (secs > maxlate)
						{
							// It's over the maximum interval late.
							// Find the last previous occurrence of the alarm.
							QDateTime next;
							KAlarmEvent::OccurType type = event.previousOccurrence(now, next);
							switch (type)
							{
								case KAlarmEvent::FIRST_OCCURRENCE:
								case KAlarmEvent::RECURRENCE_DATE:
								case KAlarmEvent::RECURRENCE_DATE_TIME:
								case KAlarmEvent::LAST_OCCURRENCE:
									if (next.secsTo(now) > maxlate)
									{
										if (type == KAlarmEvent::LAST_OCCURRENCE)
											cancel = true;
										else
											late = true;
									}
									break;
								case KAlarmEvent::NO_OCCURRENCE:
								default:
									late = true;
									break;
							}
						}
					}

					if (cancel)
					{
						// All repetitions are finished, so cancel the event
						handleAlarm(event, alarm, ALARM_CANCEL, false);
						updateCalAndDisplay = true;
						continue;
					}
					if (late)
					{
						// The latest repetition was too long ago, so schedule the next one
						handleAlarm(event, alarm, ALARM_RESCHEDULE, false);
						updateCalAndDisplay = true;
						continue;
					}
				}
				if (alfunction == ALARM_TRIGGER)
				{
					kdDebug(5950) << "KAlarmApp::handleEvent(): alarm " << alarm.id() << ": display\n";
					displayAlarm = alarm;             // note the alarm to be displayed
					alfunction = ALARM_RESCHEDULE;    // only trigger one alarm for the event
				}
				else
					kdDebug(5950) << "KAlarmApp::handleEvent(): alarm " << alarm.id() << ": skip\n";
			}

			// If there is an alarm to display, do this last after rescheduling/cancelling
			// any others. This ensures that the updated event is only saved once to the calendar.
			if (displayAlarm.valid())
				handleAlarm(event, displayAlarm, ALARM_TRIGGER, true);
			else if (updateCalAndDisplay)
				updateEvent(event, 0L);     // update the window lists and calendar file
			else
				kdDebug(5950) << "KAlarmApp::handleEvent(): no action\n";
			break;
		}
	}
	return true;
}

/******************************************************************************
* Called when an alarm is displayed to reschedule it for its next repetition.
* If no repetitions remain, cancel it.
*/
void KAlarmApp::rescheduleAlarm(KAlarmEvent& event, int alarmID)
{
	kdDebug(5950) << "KAlarmApp::rescheduleAlarm(): " << event.id() << ":" << alarmID << endl;
	Event* kcalEvent = mCalendar->event(event.id());
	if (!kcalEvent)
		kdError(5950) << "KAlarmApp::rescheduleAlarm(): event ID not found: " << event.id() << endl;
	else
	{
		KAlarmAlarm alarm = event.alarm(alarmID);
		if (!alarm.valid())
			kdError(5950) << "KAlarmApp::rescheduleAlarm(): alarm sequence not found: " << event.id() << ":" << alarmID << endl;
		handleAlarm(event, alarm, ALARM_RESCHEDULE, true);
	}
}

/******************************************************************************
* Either:
* a) Display the alarm and then delete it if it has no outstanding repetitions.
* b) Delete the alarm.
* c) Reschedule the alarm for its next repetition. If none remain, delete it.
* If the alarm is deleted and it is the last alarm for its event, the event is
* removed from the calendar file and from every main window instance.
*/
void KAlarmApp::handleAlarm(KAlarmEvent& event, KAlarmAlarm& alarm, AlarmFunc function, bool updateCalAndDisplay)
{
	switch (function)
	{
		case ALARM_TRIGGER:
			kdDebug(5950) << "KAlarmApp::handleAlarm(): TRIGGER" << endl;
			execAlarm(event, alarm, true);
			break;

		case ALARM_RESCHEDULE:
		{
			// Leave an alarm which repeats at every login until its main alarm is deleted
			kdDebug(5950) << "KAlarmApp::handleAlarm(): RESCHEDULE" << endl;
			bool update = false;
			if (alarm.deferred())
			{
				// It's an extra deferred alarm, so delete it
				event.removeAlarm(alarm.id());
				update = true;
			}
			else if (!alarm.repeatAtLogin())
			{
				switch (event.setNextOccurrence(QDateTime::currentDateTime()))
				{
					case KAlarmEvent::NO_OCCURRENCE:
						// All repetitions are finished, so cancel the event
						if (alarm.id() == KAlarmEvent::MAIN_ALARM_ID  &&  !event.audioFile().isEmpty())
							event.removeAlarm(KAlarmEvent::AUDIO_ALARM_ID);
						handleAlarm(event, alarm, ALARM_CANCEL, updateCalAndDisplay);
						break;
					case KAlarmEvent::RECURRENCE_DATE:
					case KAlarmEvent::RECURRENCE_DATE_TIME:
					case KAlarmEvent::LAST_OCCURRENCE:
						// The event is due by now and repetitions still remain, so rewrite the event
						if (updateCalAndDisplay)
							update = true;
						else
							event.setUpdated();    // note that the calendar file needs to be updated
						break;
					case KAlarmEvent::FIRST_OCCURRENCE:
						// The first occurrence is still due?!?, so don't do anything
					default:
						break;
				}
			}
			else if (updateCalAndDisplay  &&  event.updated())
				update = true;
			if (update)
				updateEvent(event, 0);     // update the window lists and calendar file
			break;
		}
		case ALARM_CANCEL:
		{
			kdDebug(5950) << "KAlarmApp::handleAlarm(): CANCEL" << endl;
			event.removeAlarm(alarm.id());
			if (!event.alarmCount())
				deleteEvent(event, 0L, false);
			else if (updateCalAndDisplay)
				updateEvent(event, 0L);    // update the window lists and calendar file
			break;
		}
		default:
			kdError(5950) << "KAlarmApp::handleAlarm(): unknown function" << endl;
	}
}

/******************************************************************************
* Execute an alarm by displaying its message or file, or executing its command.
* Reply = false if an error message was output.
*/
bool KAlarmApp::execAlarm(KAlarmEvent& event, const KAlarmAlarm& alarm, bool reschedule, bool allowDefer)
{
	bool result = true;
	if (alarm.type() == KAlarmAlarm::COMMAND)
	{
		QString command = event.cleanText();
		kdDebug(5950) << "KAlarmApp::execAlarm(): COMMAND: " << command << endl;
		KShellProcess proc;
		proc << command;
		if (!proc.start(KProcess::DontCare))
		{
			kdDebug(5950) << "KAlarmApp::execAlarm(): failed\n";
			QString msg = i18n("Failed to execute command:");
			(new MessageWin(QString("%1\n%2").arg(msg).arg(command), event, alarm, reschedule))->show();
			result = false;
		}
#if 0
		else (!proc.normalExit())
		{
			kdDebug(5950) << "KAlarmApp::execAlarm(): killed\n";
			(new MessageWin(i18n("Command execution error:"), event, alarm, reschedule))->show();
			result = false;
		}
#endif
		if (reschedule)
			rescheduleAlarm(event, alarm.id());
	}
#ifdef KALARM_EMAIL
	else if (alarm.type() == KAlarmAlarm::EMAIL)
	{
		QString addresses = event.??();
		kdDebug(5950) << "KAlarmApp::execAlarm(): EMAIL: " << command << endl;
		if (??)
		{
			kdDebug(5950) << "KAlarmApp::execAlarm(): failed\n";
			(new MessageWin(i18n("Failed to send email"), event, alarm, reschedule))->show();
			result = false;
		}
		if (reschedule)
			rescheduleAlarm(event, alarm.id());
	}
#endif
	else
	{
		// Display a message or file, provided that the same event isn't already being displayed
		MessageWin* win = MessageWin::findEvent(event.id());
		if (win)
			win->repeat();
		else
			(new MessageWin(event, alarm, reschedule, allowDefer))->show();
	}
	return result;
}

/******************************************************************************
* Add a new alarm.
* Save it in the calendar file and add it to every main window instance.
* Parameters:
*    win  = initiating main window instance (which has already been updated)
*/
bool KAlarmApp::addEvent(const KAlarmEvent& event, KAlarmMainWindow* win)
{
	kdDebug(5950) << "KAlarmApp::addEvent(): " << event.id() << endl;
	if (!initCheck())
		return false;

	// Save the event details in the calendar file, and get the new event ID
	mCalendar->addEvent(event);
	mCalendar->save();

	// Tell the daemon to reread the calendar file
	reloadDaemon();

	// Update the window lists
	KAlarmMainWindow::addEvent(event, win);
	return true;
}

/******************************************************************************
* Modify an alarm in every main window instance.
* The new event will have a different event ID from the old one.
* Parameters:
*    win  = initiating main window instance (which has already been updated)
*/
void KAlarmApp::modifyEvent(const QString& oldEventID, const KAlarmEvent& newEvent, KAlarmMainWindow* win)
{
	kdDebug(5950) << "KAlarmApp::modifyEvent(): '" << oldEventID << endl;

	// Update the event in the calendar file, and get the new event ID
	mCalendar->deleteEvent(oldEventID);
	mCalendar->addEvent(newEvent);
	mCalendar->save();

	// Tell the daemon to reread the calendar file
	reloadDaemon();

	// Update the window lists
	KAlarmMainWindow::modifyEvent(oldEventID, newEvent, win);
}

/******************************************************************************
* Update an alarm in every main window instance.
* The new event will have the same event ID as the old one.
* Parameters:
*    win  = initiating main window instance (which has already been updated)
*/
void KAlarmApp::updateEvent(const KAlarmEvent& event, KAlarmMainWindow* win)
{
	kdDebug(5950) << "KAlarmApp::updateEvent(): " << event.id() << endl;

	// Update the event in the calendar file
	const_cast<KAlarmEvent&>(event).incrementRevision();
	mCalendar->updateEvent(event);
	mCalendar->save();

	// Tell the daemon to reread the calendar file
	reloadDaemon();

	// Update the window lists
	KAlarmMainWindow::modifyEvent(event, win);
}

/******************************************************************************
* Delete an alarm from every main window instance.
* Parameters:
*    win  = initiating main window instance (which has already been updated)
*/
void KAlarmApp::deleteEvent(KAlarmEvent& event, KAlarmMainWindow* win, bool tellDaemon)
{
	kdDebug(5950) << "KAlarmApp::deleteEvent(): " << event.id() << endl;

	// Update the window lists
	KAlarmMainWindow::deleteEvent(event, win);

	// Delete the event from the calendar file
	mCalendar->deleteEvent(event.id());
	mCalendar->save();

	// Tell the daemon to reread the calendar file
	if (tellDaemon)
		reloadDaemon();
}

/******************************************************************************
* Set up the DCOP handlers.
*/
void KAlarmApp::setUpDcop()
{
	if (!mDcopHandler)
	{
		mDcopHandler      = new DcopHandler(QString::fromLatin1(DCOP_OBJECT_NAME));
		mDaemonGuiHandler = new DaemonGuiHandler(QString::fromLatin1(GUI_DCOP_OBJECT_NAME));
	}
}

/******************************************************************************
* If this is the first time through, open the calendar file, optionally start
* the alarm daemon and register with it, and set up the DCOP handler.
*/
bool KAlarmApp::initCheck(bool calendarOnly)
{
	if (!mCalendar->isOpen())
	{
		kdDebug(5950) << "KAlarmApp::initCheck(): opening calendar\n";

		// First time through. Open the calendar file.
		if (!mCalendar->open())
			return false;

		if (!mStartOfDay.isValid())
			changeStartOfDay();   // start of day time has changed, so adjust date-only alarms

		if (!calendarOnly)
			startDaemon();        // make sure the alarm daemon is running
	}
	else if (!mDaemonRegistered)
		startDaemon();

	if (!calendarOnly)
		setUpDcop();            // we're now ready to handle DCOP calls, so set up handlers
	return true;
}

/******************************************************************************
* Start the alarm daemon if necessary, and register this application with it.
*/
void KAlarmApp::startDaemon()
{
	kdDebug(5950) << "KAlarmApp::startDaemon()\n";
	mCalendar->getURL();    // check that the calendar file name is OK - program exit if not
	if (!dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
	{
		// Start the alarm daemon. It is a KUniqueApplication, which means that
		// there is automatically only one instance of the alarm daemon running.
		QString execStr = locate("exe",QString::fromLatin1(DAEMON_APP_NAME));
		system(QFile::encodeName(execStr));
		kdDebug(5950) << "KAlarmApp::startDaemon(): Alarm daemon started" << endl;
	}

	// Register this application with the alarm daemon
	registerWithDaemon();

	// Tell alarm daemon to load the calendar
	{
		QByteArray data;
		QDataStream arg(data, IO_WriteOnly);
		arg << QCString(aboutData()->appName()) << mCalendar->urlString();
		if (!dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "addMsgCal(QCString,QString)", data))
			kdDebug(5950) << "KAlarmApp::startDaemon(): addCal dcop send failed" << endl;
	}

	mDaemonRegistered = true;
	kdDebug(5950) << "KAlarmApp::startDaemon(): started daemon" << endl;
}

/******************************************************************************
* Start the alarm daemon if necessary, and register this application with it.
*/
void KAlarmApp::registerWithDaemon()
{
	kdDebug(5950) << "KAlarmApp::registerWithDaemon()\n";
	QByteArray data;
	QDataStream arg(data, IO_WriteOnly);
	arg << QCString(aboutData()->appName()) << aboutData()->programName()
	    << QCString(DCOP_OBJECT_NAME)
	    << (int)(mDisableAlarmsIfStopped ? ClientInfo::NO_START_NOTIFY : ClientInfo::COMMAND_LINE_NOTIFY)
	    << (Q_INT8)0;
	if (!dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "registerApp(QCString,QString,QCString,int,bool)", data))
		kdDebug(5950) << "KAlarmApp::registerWithDaemon(): registerApp dcop send failed" << endl;
}

/******************************************************************************
* Stop the alarm daemon if it is running.
*/
bool KAlarmApp::stopDaemon()
{
	kdDebug(5950) << "KAlarmApp::stopDaemon()" << endl;
	if (dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
	{
		QByteArray data;
		if (!dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "quit()", data))
		{
			kdError(5950) << "KAlarmApp::stopDaemon(): quit dcop send failed" << endl;
			return false;
		}
	}
	return true;
}

/******************************************************************************
* Reset the alarm daemon and reload the calendar.
* If the daemon is not already running, start it.
*/
void KAlarmApp::resetDaemon()
{
	kdDebug(5950) << "KAlarmApp::resetDaemon()" << endl;
	mCalendar->reload();
	KAlarmMainWindow::refresh();
	if (!dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
		startDaemon();
	else
	{
		QByteArray data;
		QDataStream arg(data, IO_WriteOnly);
		arg << QCString(aboutData()->appName()) << mCalendar->urlString();
		if (!dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "resetMsgCal(QCString,QString)", data))
			kdDebug(5950) << "KAlarmApp::resetDaemon(): addCal dcop send failed" << endl;
	}
}

/******************************************************************************
* Tell the alarm daemon to reread the calendar file.
*/
void KAlarmApp::reloadDaemon()
{
	QByteArray data;
	QDataStream arg(data, IO_WriteOnly);
	arg << QCString(aboutData()->appName()) << mCalendar->urlString();
	if (!dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "reloadMsgCal(QCString,QString)", data))
		kdDebug(5950) << "KAlarmApp::reloadDaemon(): dcop send failed" << endl;
}

/******************************************************************************
* Check whether the alarm daemon is currently running.
*/
bool KAlarmApp::isDaemonRunning()
{
	bool running = dcopClient()->isApplicationRegistered(DAEMON_APP_NAME);
	if (running != mDaemonRunning)
	{
		// Daemon's status has changed
		mDaemonRunning = running;
		if (mDaemonRunning)
			startDaemon();      // re-register with the daemon
	}
	return mDaemonRunning;
}

/******************************************************************************
* Read the alarm daemon's alarm check interval from its config file. If it has
* reduced, any late-cancel alarms which are already due could potentially be
* cancelled erroneously. To avoid this, note the effective last time that it
* will have checked alarms.
*/
void KAlarmApp::readDaemonCheckInterval()
{
	KConfig config(locate("config", DAEMON_APP_NAME_DEF"rc"));
	config.setGroup("General");
	int checkInterval = 60 * config.readNumEntry("CheckInterval", 1);
	if (checkInterval < mDaemonCheckInterval)
	{
		// The daemon check interval has reduced,
		// Note the effective last time that the daemon checked alarms.
		QDateTime now = QDateTime::currentDateTime();
		mLastDaemonCheck = now.addSecs(-mDaemonCheckInterval);
		mNextDaemonCheck = now.addSecs(checkInterval);
	}
	mDaemonCheckInterval = checkInterval;
}

/******************************************************************************
* Find the maximum number of seconds late which a late-cancel alarm is allowed
* to be. This is calculated as the alarm daemon's check interval, plus a few
* seconds leeway to cater for any timing irregularities.
*/
int KAlarmApp::maxLateness()
{
	static const int LATENESS_LEEWAY = 5;

	readDaemonCheckInterval();
	if (mLastDaemonCheck.isValid())
	{
		QDateTime now = QDateTime::currentDateTime();
		if (mNextDaemonCheck > now)
		{
			// Daemon's check interval has just reduced, so allow extra time
			return mLastDaemonCheck.secsTo(now) + LATENESS_LEEWAY;
		}
		mLastDaemonCheck = QDateTime();
	}
	return mDaemonCheckInterval + LATENESS_LEEWAY;
}

/******************************************************************************
*  Read the size for the specified window from the config file, for the
*  current screen resolution.
*  Reply = window size.
*/
QSize KAlarmApp::readConfigWindowSize(const char* window, const QSize& defaultSize)
{
	KConfig* config = KGlobal::config();
	config->setGroup(QString::fromLatin1(window));
	QWidget* desktop = KApplication::desktop();
	return QSize(config->readNumEntry(QString::fromLatin1("Width %1").arg(desktop->width()), defaultSize.width()),
	             config->readNumEntry(QString::fromLatin1("Height %1").arg(desktop->height()), defaultSize.height()));
}

/******************************************************************************
*  Write the size for the specified window to the config file, for the
*  current screen resolution.
*/
void KAlarmApp::writeConfigWindowSize(const char* window, const QSize& size)
{
	KConfig* config = KGlobal::config();
	config->setGroup(QString::fromLatin1(window));
	QWidget* desktop = KApplication::desktop();
	config->writeEntry(QString::fromLatin1("Width %1").arg(desktop->width()), size.width());
	config->writeEntry(QString::fromLatin1("Height %1").arg(desktop->height()), size.height());
	config->sync();
}

/******************************************************************************
* Check whether a file appears to be a text file by looking at its mime type.
* Reply = 0 if not a text file
*       = 1 if a plain text file
*       = 2 if a formatted text file.
*/
int KAlarmApp::isTextFile(const KURL& url)
{
	static const char* applicationTypes[] = {
		"x-shellscript", "x-nawk", "x-awk", "x-perl", "x-python",
		"x-desktop", "x-troff", 0 };
	static const char* formattedTextTypes[] = {
		"html", "xml", 0 };

	QString mimetype = KFileItem(-1, -1, url).mimetype();
	int slash = mimetype.find('/');
	if (slash < 0)
		return 0;
	QString subtype = mimetype.mid(slash + 1);
	if (mimetype.startsWith(QString::fromLatin1("application")))
	{
		for (int i = 0;  applicationTypes[i];  ++i)
			if (!strcmp(subtype, applicationTypes[i]))
				return 1;
	}
	else if (mimetype.startsWith(QString::fromLatin1("text")))
	{
		for (int i = 0;  formattedTextTypes[i];  ++i)
			if (!strcmp(subtype, formattedTextTypes[i]))
				return 2;
		return 1;
	}
	return 0;
}

/******************************************************************************
* This class's function is simply to act as a receiver for DCOP requests.
*/
DcopHandler::DcopHandler(const char* dcopObject)
	: QWidget(),
	  DCOPObject(dcopObject)
{
	kdDebug(5950) << "DcopHandler::DcopHandler()\n";
}

/******************************************************************************
* Process a DCOP request.
*/
bool DcopHandler::process(const QCString& func, const QByteArray& data, QCString& replyType, QByteArray&)
{
	kdDebug(5950) << "DcopHandler::process(): " << func << endl;
	enum
	{
		ERR            = 0,
		OPERATION      = 0x0007,    // mask for main operation
		  HANDLE       = 0x0001,
		  CANCEL       = 0x0002,
		  TRIGGER      = 0x0003,
		  SCHEDULE     = 0x0004,
		ALARM_TYPE     = 0x00F0,    // mask for SCHEDULE alarm type
		  MESSAGE      = 0x0010,
		  FILE         = 0x0020,
		  COMMAND      = 0x0030,
		SCH_FLAGS      = 0x0F00,    // mask for SCHEDULE flags
		  REP_COUNT    = 0x0100,
		  REP_END      = 0x0200,
		OLD            = 0x1000     // old-style deprecated method
	};
	int function;
	if      (func == "handleEvent(const QString&,const QString&)"
	||       func == "handleEvent(QString,QString)")
		function = HANDLE;
	else if (func == "cancelEvent(const QString&,const QString&)"
	||       func == "cancelEvent(QString,QString)"
	||       func == "cancelMessage(const QString&,const QString&)"    // deprecated: backwards compatibility with KAlarm pre-0.6
	||       func == "cancelMessage(QString,QString)")                 // deprecated: backwards compatibility with KAlarm pre-0.6
		function = CANCEL;
	else if (func == "triggerEvent(const QString&,const QString&)"
	||       func == "triggerEvent(QString,QString)"
	||       func == "displayMessage(const QString&,const QString&)"   // deprecated: backwards compatibility with KAlarm pre-0.6
	||       func == "displayMessage(QString,QString)")                // deprecated: backwards compatibility with KAlarm pre-0.6
		function = TRIGGER;

	else if (func == "scheduleMessage(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString)")
		function = SCHEDULE | MESSAGE;
	else if (func == "scheduleFile(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString)")
		function = SCHEDULE | FILE;
	else if (func == "scheduleCommand(const QString&,const QDateTime&,Q_UINT32)"
	||       func == "scheduleCommand(QString,QDateTime,Q_UINT32)")
		function = SCHEDULE | COMMAND;

	else if (func == "scheduleMessage(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | MESSAGE | REP_COUNT;
	else if (func == "scheduleFile(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | FILE | REP_COUNT;
	else if (func == "scheduleCommand(const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleCommand(QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | COMMAND | REP_COUNT;

	else if (func == "scheduleMessage(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | MESSAGE | REP_END;
	else if (func == "scheduleFile(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | FILE | REP_END;
	else if (func == "scheduleCommand(const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleCommand(QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | COMMAND | REP_END;

	// Deprecated methods: backwards compatibility with KAlarm pre-0.7
	else if (func == "scheduleMessage(const QString&,const QDateTime&,QColor,Q_UINT32,Q_INT32,Q_INT32)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | MESSAGE | REP_COUNT | OLD;
	else if (func == "scheduleFile(const QString&,const QDateTime&,QColor,Q_UINT32,Q_INT32,Q_INT32)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | FILE | REP_COUNT | OLD;
	else if (func == "scheduleCommand(const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32)"
	||       func == "scheduleCommand(QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | COMMAND | REP_COUNT | OLD;
	else
	{
		kdDebug(5950) << "DcopHandler::process(): unknown DCOP function" << endl;
		return false;
	}

	switch (function & OPERATION)
	{
		case HANDLE:        // trigger or cancel event with specified ID from calendar file
		case CANCEL:        // cancel event with specified ID from calendar file
		case TRIGGER:       // trigger event with specified ID in calendar file
		{

			QDataStream arg(data, IO_ReadOnly);
			QString urlString, vuid;
			arg >> urlString >> vuid;
			replyType = "void";
			switch (function)
			{
				case HANDLE:
					theApp()->handleEvent(urlString, vuid);
					break;
				case CANCEL:
					theApp()->deleteEvent(urlString, vuid);
					break;
				case TRIGGER:
					theApp()->triggerEvent(urlString, vuid);
					break;
			}
			break;
		}
		case SCHEDULE:      // schedule a new event
		{
			KAlarmAlarm::Type type;
			switch (function & ALARM_TYPE)
			{
				case MESSAGE:  type = KAlarmAlarm::MESSAGE;  break;
				case FILE:     type = KAlarmAlarm::FILE;     break;
				case COMMAND:  type = KAlarmAlarm::COMMAND;  break;
			}
			QDataStream arg(data, IO_ReadOnly);
			QString   text, audioFile;
			QDateTime dateTime, endTime;
			QColor    bgColour;
			Q_UINT32  flags;
			KAlarmEvent::RecurType recurType = KAlarmEvent::NO_RECUR;
			Q_INT32   repeatCount = 0;
			Q_INT32   repeatInterval = 0;
			arg >> text;
			arg.readRawBytes((char*)&dateTime, sizeof(dateTime));
			if (type != KAlarmAlarm::COMMAND)
				arg.readRawBytes((char*)&bgColour, sizeof(bgColour));
			arg >> flags;
			if (!(function & OLD))
				arg >> audioFile;
			if (function & (REP_COUNT | REP_END))
			{
				if (function & OLD)
				{
					// Backwards compatibility with KAlarm pre-0.7
					recurType = KAlarmEvent::MINUTELY;
					arg >> repeatCount >> repeatInterval;
					++repeatCount;
				}
				else
				{
					Q_INT32 type;
					arg >> type >> repeatInterval;
					recurType = KAlarmEvent::RecurType(type);
					switch (recurType)
					{
						case KAlarmEvent::MINUTELY:
						case KAlarmEvent::DAILY:
						case KAlarmEvent::WEEKLY:
						case KAlarmEvent::MONTHLY_DAY:
						case KAlarmEvent::ANNUAL_DATE:
							break;
						default:
							kdDebug(5950) << "DcopHandler::process(): invalid simple repetition type: " << type << endl;
							return false;
					}
					if (function & REP_COUNT)
						arg >> repeatCount;
					else
						arg.readRawBytes((char*)&endTime, sizeof(endTime));

				}
			}
			theApp()->scheduleEvent(text, dateTime, bgColour, flags, audioFile, type, recurType, repeatInterval, repeatCount, endTime);
			break;
		}
	}
	replyType = "void";
	return true;
}

/******************************************************************************
*  Convert the --time parameter string into a date/time or date value.
*  The parameter is in the form [[[yyyy-]mm-]dd-]hh:mm or yyyy-mm-dd.
*  Reply = true if successful.
*/
static bool convWakeTime(const QCString timeParam, QDateTime& dateTime, bool& noTime)
{
	if (timeParam.length() > 19)
		return false;
	char timeStr[20];
	strcpy(timeStr, timeParam);
	int dt[5] = { -1, -1, -1, -1, -1 };
	char* s;
	char* end;
	// Get the minute value
	if ((s = strchr(timeStr, ':')) == 0L)
		noTime = true;
	else
	{
		noTime = false;
		*s++ = 0;
		dt[4] = strtoul(s, &end, 10);
		if (end == s  ||  *end  ||  dt[4] >= 60)
			return false;
		// Get the hour value
		if ((s = strrchr(timeStr, '-')) == 0L)
			s = timeStr;
		else
			*s++ = 0;
		dt[3] = strtoul(s, &end, 10);
		if (end == s  ||  *end  ||  dt[3] >= 24)
			return false;
	}
	bool dateSet = false;
	if (s != timeStr)
	{
		dateSet = true;
		// Get the day value
		if ((s = strrchr(timeStr, '-')) == 0L)
			s = timeStr;
		else
			*s++ = 0;
		dt[2] = strtoul(s, &end, 10);
		if (end == s  ||  *end  ||  dt[2] == 0  ||  dt[2] > 31)
			return false;
		if (s != timeStr)
		{
			// Get the month value
			if ((s = strrchr(timeStr, '-')) == 0L)
				s = timeStr;
			else
				*s++ = 0;
			dt[1] = strtoul(s, &end, 10);
			if (end == s  ||  *end  ||  dt[1] == 0  ||  dt[1] > 12)
				return false;
			if (s != timeStr)
			{
				// Get the year value
				dt[0] = strtoul(timeStr, &end, 10);
				if (end == timeStr  ||  *end)
					return false;
			}
		}
	}

	QDate date(dt[0], dt[1], dt[2]);
	QTime time(0, 0, 0);
	if (noTime)
	{
		// No time was specified, so the full date must have been specified
		if (dt[0] < 0)
			return false;
	}
	else
	{
		// Compile the values into a date/time structure
		QDateTime now = QDateTime::currentDateTime();
		if (dt[0] < 0)
			date.setYMD(now.date().year(),
			            (dt[1] < 0 ? now.date().month() : dt[1]),
			            (dt[2] < 0 ? now.date().day() : dt[2]));
		time.setHMS(dt[3], dt[4], 0);
		if (!dateSet  &&  time < now.time())
			date = date.addDays(1);
	}
	if (!date.isValid())
		return false;
	dateTime.setDate(date);
	dateTime.setTime(time);
	return true;
}
