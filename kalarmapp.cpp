/*
 *  kalarmapp.cpp  -  the KAlarm application object
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

#include "kalarm.h"

#include <stdlib.h>
#include <ctype.h>
#include <iostream>

#include <qobjectlist.h>
#include <qtimer.h>
#include <qregexp.h>

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
#include <kstdguiitem.h>
#include <kstaticdeleter.h>
#include <kdebug.h>

#include <kalarmd/clientinfo.h>

#include <libkcal/icalformat.h>

#include "alarmcalendar.h"
#include "alarmlistview.h"
#include "mainwindow.h"
#include "editdlg.h"
#include "messagewin.h"
#include "daemon.h"
#include "daemongui.h"
#include "dcophandler.h"
#include "functions.h"
#include "traywindow.h"
#include "kamail.h"
#include "preferences.h"
#include "prefdlg.h"
#include "kalarmapp.moc"

#include <netwm.h>

int         marginKDE2 = 0;

static bool convWakeTime(const QCString timeParam, QDateTime&, bool& noTime);
static bool convInterval(QCString timeParam, KAEvent::RecurType&, int& timeInterval);

/******************************************************************************
* Find the maximum number of seconds late which a late-cancel alarm is allowed
* to be. This is calculated as the alarm daemon's check interval, plus a few
* seconds leeway to cater for any timing irregularities.
*/
static inline int maxLateness()
{
	static const int LATENESS_LEEWAY = 5;
	return Daemon::maxTimeSinceCheck() + LATENESS_LEEWAY;
}


KAlarmApp*  KAlarmApp::theInstance  = 0;
int         KAlarmApp::mActiveCount = 0;


/******************************************************************************
* Construct the application.
*/
KAlarmApp::KAlarmApp()
	: KUniqueApplication(),
	  mDcopHandler(0),
	  mDaemonGuiHandler(0),
	  mTrayWindow(0),
	  mQueueQuit(false),
	  mProcessingQueue(false),
	  mCheckingSystemTray(false),
	  mSessionClosingDown(false),
	  mRefreshExpiredAlarms(false)
{
#if KDE_VERSION >= 290
	mNoShellAccess = !authorize("shell_access");
#else
	mNoShellAccess = false;
	marginKDE2 = KDialog::marginHint();
#endif
	mCommandProcesses.setAutoDelete(true);
	Preferences* preferences = Preferences::instance();
	connect(preferences, SIGNAL(preferencesChanged()), SLOT(slotPreferencesChanged()));
	CalFormat::setApplication(aboutData()->programName(),
	                          QString::fromLatin1("-//K Desktop Environment//NONSGML %1 " KALARM_VERSION "//EN")
	                                       .arg(aboutData()->programName()));
	KAEvent::setFeb29RecurType();

	if (!AlarmCalendar::initialiseCalendars())
		exit(1);    // calendar names clash, so give up
	connect(AlarmCalendar::expiredCalendar(), SIGNAL(purged()), SLOT(slotExpiredPurged()));

	// Check if it's a KDE desktop by comparing the window manager name to "KWin"
	NETRootInfo nri(qt_xdisplay(), NET::SupportingWMCheck);
	const char* wmname = nri.wmName();
	mKDEDesktop = wmname && !strcmp(wmname, "KWin");

	KConfig* config = kapp->config();
	config->setGroup(QString::fromLatin1("General"));
	mNoSystemTray           = config->readBoolEntry(QString::fromLatin1("NoSystemTray"), false);
	mSavedNoSystemTray      = mNoSystemTray;
	mOldRunInSystemTray     = wantRunInSystemTray();
	mDisableAlarmsIfStopped = mOldRunInSystemTray && !mNoSystemTray && preferences->disableAlarmsIfStopped();
	mStartOfDay             = preferences->startOfDay();
	if (preferences->hasStartOfDayChanged())
		mStartOfDay.setHMS(100,0,0);    // start of day time has changed: flag it as invalid
	mPrefsExpiredColour   = preferences->expiredColour();
	mPrefsExpiredKeepDays = preferences->expiredKeepDays();
	mPrefsShowTime        = preferences->showAlarmTime();
	mPrefsShowTimeTo      = preferences->showTimeToAlarm();

	// Set up actions used by more than one menu
	mActionCollection  = new KActionCollection(this);
	mActionAlarmEnable = new ActionAlarmsEnabled(Qt::CTRL+Qt::Key_A, this, SLOT(toggleAlarmsEnabled()),
	                                             mActionCollection, "alarmenable");
	mActionPrefs       = KStdAction::preferences(this, SLOT(slotPreferences()), mActionCollection);
	mActionNewAlarm    = KAlarm::createNewAlarmAction(i18n("&New Alarm..."), this, SLOT(slotNewAlarm()), mActionCollection);
}

/******************************************************************************
*/
KAlarmApp::~KAlarmApp()
{
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

		// This is here instead of in the constructor to avoid recursion
		Daemon::initialise(theInstance->mActionCollection);    // calendars and actions must be initialised before calling this
	}
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
	++mActiveCount;
	if (!initCheck(true))     // open the calendar file (needed for main windows)
	{
		--mActiveCount;
		quitIf(1, true);    // error opening the main calendar - quit
		return true;
	}
	KAlarmMainWindow* trayParent = 0;
	for (int i = 1;  KMainWindow::canBeRestored(i);  ++i)
	{
		QString type = KMainWindow::classNameOfToplevel(i);
		if (type == QString::fromLatin1("KAlarmMainWindow"))
		{
			KAlarmMainWindow* win = KAlarmMainWindow::create(true);
			win->restore(i, false);
			if (win->isHiddenTrayParent())
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

	// Try to display the system tray icon if it is configured to be autostarted,
	// or if we're in run-in-system-tray mode.
	if (Preferences::instance()->autostartTrayIcon()
	||  KAlarmMainWindow::count()  &&  wantRunInSystemTray())
		displayTrayIcon(true, trayParent);

	--mActiveCount;
	quitIf(0);           // quit if no windows are open
	return true;
}

/******************************************************************************
* Called for a KUniqueApplication when a new instance of the application is
* started.
*/
int KAlarmApp::newInstance()
{
	kdDebug(5950)<<"KAlarmApp::newInstance()\n";
	++mActiveCount;
	int exitCode = 0;               // default = success
	static bool firstInstance = true;
	bool dontRedisplay = false;
	if (!firstInstance || !isRestored())
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
			#define USAGE(message)  { usage = message; break; }
			if (args->isSet("stop"))
			{
				// Stop the alarm daemon
				kdDebug(5950)<<"KAlarmApp::newInstance(): stop\n";
				args->clear();         // free up memory
				if (!Daemon::stop())
				{
					exitCode = 1;
					break;
				}
				dontRedisplay = true;  // exit program if no other instances running
			}
			else
			if (args->isSet("reset"))
			{
				// Reset the alarm daemon, if it's running.
				// (If it's not running, it will reset automatically when it eventually starts.)
				kdDebug(5950)<<"KAlarmApp::newInstance(): reset\n";
				args->clear();         // free up memory
				Daemon::reset();
				dontRedisplay = true;  // exit program if no other instances running
			}
			else
			if (args->isSet("tray"))
			{
				// Display only the system tray icon
				kdDebug(5950)<<"KAlarmApp::newInstance(): tray\n";
				args->clear();      // free up memory
				if (!mKDEDesktop)
				{
					exitCode = 1;
					break;
				}
				if (!initCheck())   // open the calendar, register with daemon
				{
					exitCode = 1;
					break;
				}
				if (!displayTrayIcon(true))
				{
					exitCode = 1;
					break;
				}
			}
			else
			if (args->isSet("handleEvent")  ||  args->isSet("triggerEvent")  ||  args->isSet("cancelEvent")  ||  args->isSet("calendarURL"))
			{
				// Display or delete the event with the specified event ID
				kdDebug(5950)<<"KAlarmApp::newInstance(): handle event\n";
				EventFunc function = EVENT_HANDLE;
				int count = 0;
				const char* option = 0;
				if (args->isSet("handleEvent"))   { function = EVENT_HANDLE;  option = "handleEvent";  ++count; }
				if (args->isSet("triggerEvent"))  { function = EVENT_TRIGGER;  option = "triggerEvent";  ++count; }
				if (args->isSet("cancelEvent"))   { function = EVENT_CANCEL;  option = "cancelEvent";  ++count; }
				if (!count)
					USAGE(i18n("%1 requires %2, %3 or %4").arg(QString::fromLatin1("--calendarURL")).arg(QString::fromLatin1("--handleEvent")).arg(QString::fromLatin1("--triggerEvent")).arg(QString::fromLatin1("--cancelEvent")))
				if (count > 1)
					USAGE(i18n("%1, %2, %3 mutually exclusive").arg(QString::fromLatin1("--handleEvent")).arg(QString::fromLatin1("--triggerEvent")).arg(QString::fromLatin1("--cancelEvent")));
				if (!initCheck(true))   // open the calendar, don't register with daemon yet
				{
					exitCode = 1;
					break;
				}
				if (args->isSet("calendarURL"))
				{
					QString calendarUrl = args->getOption("calendarURL");
					if (KURL(calendarUrl).url() != AlarmCalendar::activeCalendar()->urlString())
						USAGE(i18n("%1: wrong calendar file").arg(QString::fromLatin1("--calendarURL")))
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
			if (args->isSet("file")  ||  args->isSet("exec")  ||  args->isSet("mail")  ||  args->count())
			{
				// Display a message or file, execute a command, or send an email
				KAEvent::Action action = KAEvent::MESSAGE;
				QCString         alMessage;
				EmailAddressList alAddresses;
				QStringList      alAttachments;
				QCString         alSubject;
				if (args->isSet("file"))
				{
					kdDebug(5950)<<"KAlarmApp::newInstance(): file\n";
					if (args->isSet("exec"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--exec")).arg(QString::fromLatin1("--file")))
					if (args->isSet("mail"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--mail")).arg(QString::fromLatin1("--file")))
					if (args->count())
						USAGE(i18n("message incompatible with %1").arg(QString::fromLatin1("--file")))
					alMessage = args->getOption("file");
					action = KAEvent::FILE;
				}
				else if (args->isSet("exec"))
				{
					kdDebug(5950)<<"KAlarmApp::newInstance(): exec\n";
					if (args->isSet("mail"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--mail")).arg(QString::fromLatin1("--exec")))
					alMessage = args->getOption("exec");
					action = KAEvent::COMMAND;
				}
				else if (args->isSet("mail"))
				{
					kdDebug(5950)<<"KAlarmApp::newInstance(): mail\n";
					if (args->isSet("subject"))
						alSubject = args->getOption("subject");
					QCStringList params = args->getOptionList("mail");
					for (QCStringList::Iterator i = params.begin();  i != params.end();  ++i)
					{
						QString addr = QString::fromLocal8Bit(*i);
						if (!KAMail::checkAddress(addr))
							USAGE(i18n("%1: invalid email address").arg(QString::fromLatin1("--mail")))
						alAddresses += Person(QString::null, addr);
					}
					params = args->getOptionList("attach");
					for (QCStringList::Iterator i = params.begin();  i != params.end();  ++i)
						alAttachments += QString::fromLocal8Bit(*i);
					alMessage = args->arg(0);
					action = KAEvent::EMAIL;
				}
				else
				{
					kdDebug(5950)<<"KAlarmApp::newInstance(): message\n";
					alMessage = args->arg(0);
				}

				if (action != KAEvent::EMAIL)
				{
					if (args->isSet("subject"))
						USAGE(i18n("%1 requires %2").arg(QString::fromLatin1("--subject")).arg(QString::fromLatin1("--mail")))
					if (args->isSet("attach"))
						USAGE(i18n("%1 requires %2").arg(QString::fromLatin1("--attach")).arg(QString::fromLatin1("--mail")))
					if (args->isSet("bcc"))
						USAGE(i18n("%1 requires %2").arg(QString::fromLatin1("--bcc")).arg(QString::fromLatin1("--mail")))
				}

				bool      alarmNoTime = false;
				QDateTime alarmTime, endTime;
				QColor    bgColour = Preferences::instance()->defaultBgColour();
				QColor    fgColour = Preferences::instance()->defaultFgColour();
				KCal::Recurrence recurrence(0);
				if (args->isSet("color"))
				{
					// Background colour is specified
					QCString colourText = args->getOption("color");
					if (static_cast<const char*>(colourText)[0] == '0'
					&&  tolower(static_cast<const char*>(colourText)[1]) == 'x')
						colourText.replace(0, 2, "#");
					bgColour.setNamedColor(colourText);
					if (!bgColour.isValid())
						USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("--color")))
				}
				if (args->isSet("colorfg"))
				{
					// Foreground colour is specified
					QCString colourText = args->getOption("colorfg");
					if (static_cast<const char*>(colourText)[0] == '0'
					&&  tolower(static_cast<const char*>(colourText)[1]) == 'x')
						colourText.replace(0, 2, "#");
					fgColour.setNamedColor(colourText);
					if (!fgColour.isValid())
						USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("--colorfg")))
				}

				if (args->isSet("time"))
				{
					QCString dateTime = args->getOption("time");
					if (!convWakeTime(dateTime, alarmTime, alarmNoTime))
						USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("--time")))
				}
				else
					alarmTime = QDateTime::currentDateTime();

				if (args->isSet("recurrence"))
				{
					if (args->isSet("login"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--login")).arg(QString::fromLatin1("--recurrence")))
					if (args->isSet("interval"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--interval")).arg(QString::fromLatin1("--recurrence")))
					if (args->isSet("repeat"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--repeat")).arg(QString::fromLatin1("--recurrence")))
					if (args->isSet("until"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--until")).arg(QString::fromLatin1("--recurrence")))
					QCString rule = args->getOption("recurrence");
					KCal::ICalFormat format;
					format.fromString(&recurrence, QString::fromLocal8Bit((const char*)rule));
				}
				else if (args->isSet("interval"))
				{
					// Repeat count is specified
					int repeatCount;
					if (args->isSet("login"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--login")).arg(QString::fromLatin1("--interval")))
					bool ok;
					if (args->isSet("repeat"))
					{
						repeatCount = args->getOption("repeat").toInt(&ok);
						if (!ok || !repeatCount || repeatCount < -1)
							USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("--repeat")))
					}
					else if (args->isSet("until"))
					{
						repeatCount = 0;
						QCString dateTime = args->getOption("until");
						if (!convWakeTime(dateTime, endTime, alarmNoTime))
							USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("--until")))
						if (endTime < alarmTime)
							USAGE(i18n("%1 earlier than %2").arg(QString::fromLatin1("--until")).arg(QString::fromLatin1("--time")))
					}
					else
						repeatCount = -1;

					// Get the recurrence interval
					int repeatInterval;
					KAEvent::RecurType recurType;
					if (!convInterval(args->getOption("interval"), recurType, repeatInterval)
					||  repeatInterval < 0)
						USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("--interval")))
					if (alarmNoTime  &&  recurType == KAEvent::MINUTELY)
						USAGE(i18n("Invalid %1 parameter for date-only alarm").arg(QString::fromLatin1("--interval")))

					// Convert the recurrence parameters into a KCal::Recurrence
					KAEvent::setRecurrence(recurrence, recurType, repeatInterval, repeatCount, endTime);
				}
				else
				{
					if (args->isSet("repeat"))
						USAGE(i18n("%1 requires %2").arg(QString::fromLatin1("--repeat")).arg(QString::fromLatin1("--interval")))
					if (args->isSet("until"))
						USAGE(i18n("%1 requires %2").arg(QString::fromLatin1("--until")).arg(QString::fromLatin1("--interval")))
				}

				QCString audioFile;
				float    audioVolume = -1;
#ifdef WITHOUT_ARTS
				bool     audioRepeat = false;
#else
				bool     audioRepeat = args->isSet("play-repeat");
#endif
				if (audioRepeat  ||  args->isSet("play"))
				{
					// Play a sound with the alarm
					if (audioRepeat  &&  args->isSet("play"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--play")).arg(QString::fromLatin1("--play-repeat")))
					if (args->isSet("beep"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--beep")).arg(QString::fromLatin1(audioRepeat ? "--play-repeat" : "--play")))
					audioFile = args->getOption(audioRepeat ? "play-repeat" : "play");
					if (args->isSet("volume"))
					{
						bool ok;
						int volumepc = args->getOption("volume").toInt(&ok);
						if (!ok  ||  volumepc < 0  ||  volumepc > 100)
							USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("--volume")))
						audioVolume = static_cast<float>(volumepc) / 100;
					}
				}
				else if (args->isSet("volume"))
					USAGE(i18n("%1 requires %2 or %3").arg(QString::fromLatin1("--volume")).arg(QString::fromLatin1("--play")).arg(QString::fromLatin1("--play-repeat")))

				int reminderMinutes = 0;
				bool onceOnly = args->isSet("reminder-once");
				if (args->isSet("reminder")  ||  onceOnly)
				{
					// Issue a reminder alarm in advance of the main alarm
					if (onceOnly  &&  args->isSet("reminder"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--reminder")).arg(QString::fromLatin1("--reminder-once")))
					QString opt = onceOnly ? QString::fromLatin1("--reminder-once") : QString::fromLatin1("--reminder");
					if (args->isSet("exec"))
						USAGE(i18n("%1 incompatible with %2").arg(opt).arg(QString::fromLatin1("--exec")))
					if (args->isSet("mail"))
						USAGE(i18n("%1 incompatible with %2").arg(opt).arg(QString::fromLatin1("--mail")))
					KAEvent::RecurType recur;
					QString optval = args->getOption(onceOnly ? "reminder-once" : "reminder");
					bool ok = convInterval(args->getOption(onceOnly ? "reminder-once" : "reminder"), recur, reminderMinutes);
					if (ok)
					{
						switch (recur)
						{
							case KAEvent::MINUTELY:
								if (alarmNoTime)
									USAGE(i18n("Invalid %1 parameter for date-only alarm").arg(opt))
								break;
							case KAEvent::DAILY:     reminderMinutes *= 1440;  break;
							case KAEvent::WEEKLY:    reminderMinutes *= 7*1440;  break;
							default:   ok = false;  break;
						}
					}
					if (!ok)
						USAGE(i18n("Invalid %1 parameter").arg(opt))
				}

				int flags = KAEvent::DEFAULT_FONT;
				if (args->isSet("ack-confirm"))
					flags |= KAEvent::CONFIRM_ACK;
				if (args->isSet("beep"))
					flags |= KAEvent::BEEP;
				if (audioRepeat)
					flags |= KAEvent::REPEAT_SOUND;
				if (args->isSet("late-cancel"))
					flags |= KAEvent::LATE_CANCEL;
				if (args->isSet("login"))
					flags |= KAEvent::REPEAT_AT_LOGIN;
				if (args->isSet("bcc"))
					flags |= KAEvent::EMAIL_BCC;
				if (alarmNoTime)
					flags |= KAEvent::ANY_TIME;
				args->clear();      // free up memory

				// Display or schedule the event
				if (!initCheck())
				{
					exitCode = 1;
					break;
				}
				if (!scheduleEvent(action, alMessage, alarmTime, flags, bgColour, fgColour, QFont(), audioFile,
				                   audioVolume, reminderMinutes, recurrence, alAddresses, alSubject, alAttachments))
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
				if (args->isSet("attach"))
					usage += QString::fromLatin1("--attach ");
				if (args->isSet("bcc"))
					usage += QString::fromLatin1("--bcc ");
				if (args->isSet("beep"))
					usage += QString::fromLatin1("--beep ");
				if (args->isSet("color"))
					usage += QString::fromLatin1("--color ");
				if (args->isSet("colorfg"))
					usage += QString::fromLatin1("--colorfg ");
				if (args->isSet("late-cancel"))
					usage += QString::fromLatin1("--late-cancel ");
				if (args->isSet("login"))
					usage += QString::fromLatin1("--login ");
				if (args->isSet("play"))
					usage += QString::fromLatin1("--play ");
#ifndef WITHOUT_ARTS
				if (args->isSet("play-repeat"))
					usage += QString::fromLatin1("--play-repeat ");
#endif
				if (args->isSet("reminder"))
					usage += QString::fromLatin1("--reminder ");
				if (args->isSet("reminder-once"))
					usage += QString::fromLatin1("--reminder-once ");
				if (args->isSet("subject"))
					usage += QString::fromLatin1("--subject ");
				if (args->isSet("time"))
					usage += QString::fromLatin1("--time ");
#ifndef WITHOUT_ARTS
				if (args->isSet("volume"))
					usage += QString::fromLatin1("--volume ");
#endif
				if (!usage.isEmpty())
				{
					usage += i18n(": option(s) only valid with a message/%1/%2").arg(QString::fromLatin1("--file")).arg(QString::fromLatin1("--exec"));
					break;
				}

				args->clear();      // free up memory
				if (!initCheck())
				{
					exitCode = 1;
					break;
				}

				(KAlarmMainWindow::create())->show();
			}
		} while (0);    // only execute once

		if (!usage.isEmpty())
		{
			// Note: we can't use args->usage() since that also quits any other
			// running 'instances' of the program.
			std::cerr << usage.local8Bit().data()
			          << i18n("\nUse --help to get a list of available command line options.\n").local8Bit().data();
			exitCode = 1;
		}
	}
	if (firstInstance  &&  !dontRedisplay  &&  !exitCode)
		redisplayAlarms();

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
*/
void KAlarmApp::quitIf(int exitCode, bool force)
{
	if (force)
	{
		// Quit regardless, except for message windows
		KAlarmMainWindow::closeAll();
		displayTrayIcon(false);
	}
	else
	{
		// Quit only if there are no more "instances" running
		if (mActiveCount > 0  ||  MessageWin::instanceCount())
			return;
		int mwcount = KAlarmMainWindow::count();
		KAlarmMainWindow* mw = mwcount ? KAlarmMainWindow::firstWindow() : 0;
		if (mwcount > 1  ||  mwcount && (!mw->isHidden() || !mw->isTrayParent()))
			return;
		// There are no windows left except perhaps a main window which is a hidden tray icon parent
		if (mTrayWindow)
		{
			// There is a system tray icon.
			// Don't exit unless the system tray doesn't seem to exist.
			if (checkSystemTray())
				return;
		}
		if (mDcopQueue.count())
		{
			// Don't quit yet if there are outstanding actions on the DCOP queue
			mQueueQuit = true;
			mQueueQuitCode = exitCode;
			return;
		}
	}

	/* This was the last/only running "instance" of the program, so exit completely.
	 * First, change the name which we are registered with at the DCOP server. This is
	 * to ensure that the alarm daemon immediately sees us as not running. It prevents
	 * the following situation which has been observed:
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

/******************************************************************************
* Called when the Quit menu item is selected.
* Closes the system tray window and all main windows, but does not exit the
* program if other windows are still open.
*/
void KAlarmApp::doQuit(QWidget* parent)
{
	kdDebug(5950) << "KAlarmApp::doQuit()\n";
	if (mDisableAlarmsIfStopped
	&&  KMessageBox::warningYesNo(parent, i18n("Quitting will disable alarms\n"
	                                           "(once any alarm message windows are closed)."),
	                              QString::null, KStdGuiItem::quit(), KStdGuiItem::cancel(),
	                              Preferences::QUIT_WARN
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
* The main processing loop for KAlarm.
* All KAlarm operations involving opening or updating calendar files are called
* from this loop to ensure that only one operation is active at any one time.
* This precaution is necessary because KAlarm's activities are mostly
* asynchronous, being in response to DCOP calls from the alarm daemon (or other
* programs) or timer events, any of which can be received in the middle of
* performing another operation. If a calendar file is opened or updated while
* another calendar operation is in progress, the program has been observed to
* hang, or the first calendar call has failed with data loss - clearly
* unacceptable!!
*/
void KAlarmApp::processQueue()
{
	if (!mProcessingQueue)
	{
		kdDebug(5950) << "KAlarmApp::processQueue()\n";
		mProcessingQueue = true;

		// Reset the alarm daemon if it's been queued
		KAlarm::resetDaemonIfQueued();

		// Process DCOP calls
		while (mDcopQueue.count())
		{
			DcopQEntry& entry = mDcopQueue.first();
			if (entry.eventId.isEmpty())
				KAlarm::addEvent(entry.event, 0);
			else
				handleEvent(entry.eventId, entry.function);
			mDcopQueue.pop_front();
		}

		// Purge the expired alarms calendar if it's time to do so
		AlarmCalendar::expiredCalendar()->purgeIfQueued();

		// Now that the queue has been processed, quit if a quit was queued
		if (mQueueQuit)
		{
			mQueueQuit = false;
			quitIf(mQueueQuitCode);
		}

		mProcessingQueue = false;
	}
}

/******************************************************************************
*  Redisplay alarms which were being shown when the program last exited.
*  Normally, these alarms will have been displayed by session restoration, but
*  if the program crashed or was killed, we can redisplay them here so that
*  they won't be lost.
*/
void KAlarmApp::redisplayAlarms()
{
	AlarmCalendar* cal = AlarmCalendar::displayCalendar();
	if (cal->isOpen())
	{
		Event::List events = cal->events();
		for (Event::List::ConstIterator it = events.begin();  it != events.end();  ++it)
		{
                        Event* kcalEvent = *it;
			KAEvent event(*kcalEvent);
			event.setUid(KAEvent::ACTIVE);
			if (!MessageWin::findEvent(event.id()))
			{
				// This event should be displayed, but currently isn't being
				kdDebug(5950) << "KAlarmApp::redisplayAlarms(): " << event.id() << endl;
				KAAlarm alarm = event.convertDisplayingAlarm();
				(new MessageWin(event, alarm, false, !alarm.repeatAtLogin()))->show();
			}
		}
	}
}

/******************************************************************************
* Called when the system tray main window is closed.
*/
void KAlarmApp::removeWindow(TrayWindow*)
{
	mTrayWindow = 0;
	quitIf();
}

/******************************************************************************
*  Display or close the system tray icon.
*/
bool KAlarmApp::displayTrayIcon(bool show, KAlarmMainWindow* parent)
{
	static bool creating = false;
	if (show)
	{
		if (!mTrayWindow  &&  !creating)
		{
			if (!mKDEDesktop)
				return false;
			if (!KAlarmMainWindow::count()  &&  wantRunInSystemTray())
			{
				creating = true;    // prevent main window constructor from creating an additional tray icon
				parent = KAlarmMainWindow::create();
				creating = false;
			}
			mTrayWindow = new TrayWindow(parent ? parent : KAlarmMainWindow::firstWindow());
			connect(mTrayWindow, SIGNAL(deleted()), SIGNAL(trayIconToggled()));
			mTrayWindow->show();
			emit trayIconToggled();

			// Set up a timer so that we can check after all events in the window system's
			// event queue have been processed, whether the system tray actually exists
			mCheckingSystemTray = true;
			mSavedNoSystemTray  = mNoSystemTray;
			mNoSystemTray       = false;
			QTimer::singleShot(0, this, SLOT(slotSystemTrayTimer()));
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
*  Called by a timer to check whether the system tray icon has been housed in
*  the system tray. Because there is a delay between the system tray icon show
*  event and the icon being reparented by the system tray, we have to use a
*  timer to check whether the system tray has actually grabbed it, or whether
*  the system tray probably doesn't exist.
*/
void KAlarmApp::slotSystemTrayTimer()
{
	mCheckingSystemTray = false;
	if (!checkSystemTray())
		quitIf(0);    // exit the application if there are no open windows
}

/******************************************************************************
*  Check whether the system tray icon has been housed in the system tray.
*  If the system tray doesn't seem to exist, tell the alarm daemon to notify us
*  of alarms regardless of whether we're running.
*/
bool KAlarmApp::checkSystemTray()
{
	if (mCheckingSystemTray  ||  !mTrayWindow)
		return true;
	if (mTrayWindow->inSystemTray() != !mSavedNoSystemTray)
	{
		kdDebug(5950) << "KAlarmApp::checkSystemTray(): changed -> " << mSavedNoSystemTray << endl;
		mNoSystemTray = mSavedNoSystemTray = !mSavedNoSystemTray;

		// Store the new setting in the config file, so that if KAlarm exits and is then
		// next activated by the daemon to display a message, it will register with the
		// daemon with the correct NOTIFY type. If that happened when there was no system
		// tray and alarms are disabled when KAlarm is not running, registering with
		// NO_START_NOTIFY could result in alarms never being seen.
		KConfig* config = kapp->config();
		config->setGroup(QString::fromLatin1("General"));
		config->writeEntry(QString::fromLatin1("NoSystemTray"), mNoSystemTray);
		config->sync();

		// Update other settings and reregister with the alarm daemon
		slotPreferencesChanged();
	}
	else
	{
		kdDebug(5950) << "KAlarmApp::checkSystemTray(): no change = " << !mSavedNoSystemTray << endl;
		mNoSystemTray = mSavedNoSystemTray;
	}
	return !mNoSystemTray;
}

/******************************************************************************
* Return the main window associated with the system tray icon.
*/
KAlarmMainWindow* KAlarmApp::trayMainWindow() const
{
	return mTrayWindow ? mTrayWindow->assocMainWindow() : 0;
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
	KAlarmPrefDlg prefDlg;
	prefDlg.exec();
}

/******************************************************************************
*  Called when the New menu item is clicked to edit a new alarm to add to the list.
*/
void KAlarmApp::slotNewAlarm()
{
	KAlarmMainWindow::executeNew();
}

/******************************************************************************
*  Called when KAlarm preferences have changed.
*/
void KAlarmApp::slotPreferencesChanged()
{
	Preferences* preferences = Preferences::instance();
	bool newRunInSysTray = wantRunInSystemTray();
	if (newRunInSysTray != mOldRunInSystemTray)
	{
		// The system tray run mode has changed
		++mActiveCount;         // prevent the application from quitting
		KAlarmMainWindow* win = mTrayWindow ? mTrayWindow->assocMainWindow() : 0;
		delete mTrayWindow;     // remove the system tray icon if it is currently shown
		mTrayWindow = 0;
		mOldRunInSystemTray = newRunInSysTray;
		if (!newRunInSysTray)
		{
			if (win  &&  win->isHidden())
				delete win;
		}
		displayTrayIcon(true);
		--mActiveCount;
	}

	bool newDisableIfStopped = wantRunInSystemTray() && !mNoSystemTray && Preferences::instance()->disableAlarmsIfStopped();
	if (newDisableIfStopped != mDisableAlarmsIfStopped)
	{
		mDisableAlarmsIfStopped = newDisableIfStopped;    // N.B. this setting is used by Daemon::reregister()
		preferences->setQuitWarn(true);   // since mode has changed, re-allow warning messages on Quit
		Daemon::reregister();           // re-register with the alarm daemon
	}

	// Change alarm times for date-only alarms if the start of day time has changed
	if (preferences->startOfDay() != mStartOfDay)
		changeStartOfDay();

	KAEvent::setFeb29RecurType();    // in case the date for February 29th recurrences has changed

	if (preferences->showAlarmTime()   != mPrefsShowTime
	||  preferences->showTimeToAlarm() != mPrefsShowTimeTo)
	{
		// The default alarm list time columns selection has changed
		KAlarmMainWindow::updateTimeColumns(mPrefsShowTime, mPrefsShowTimeTo);
		mPrefsShowTime   = preferences->showAlarmTime();
		mPrefsShowTimeTo = preferences->showTimeToAlarm();
	}

	if (preferences->expiredColour() != mPrefsExpiredColour)
	{
		// The expired alarms text colour has changed
		mRefreshExpiredAlarms = true;
		mPrefsExpiredColour = preferences->expiredColour();
	}

	if (preferences->expiredKeepDays() != mPrefsExpiredKeepDays)
	{
		// How long expired alarms are being kept has changed.
		// N.B. This also adjusts for any change in start-of-day time.
		mPrefsExpiredKeepDays = preferences->expiredKeepDays();
		AlarmCalendar::expiredCalendar()->setPurgeDays(mPrefsExpiredKeepDays);
	}

	if (mRefreshExpiredAlarms)
	{
		mRefreshExpiredAlarms = false;
		KAlarmMainWindow::updateExpired();
	}
}

/******************************************************************************
*  Change alarm times for date-only alarms after the start of day time has changed.
*/
void KAlarmApp::changeStartOfDay()
{
	AlarmCalendar* cal = AlarmCalendar::activeCalendar();
	if (KAEvent::adjustStartOfDay(cal->events()))
		cal->save();
	Preferences::instance()->updateStartOfDayCheck();  // now that calendar is updated, set OK flag in config file
	mStartOfDay = Preferences::instance()->startOfDay();
}

/******************************************************************************
*  Called when the expired alarms calendar has been purged.
*  Updates the alarm list in all main windows.
*/
void KAlarmApp::slotExpiredPurged()
{
	mRefreshExpiredAlarms = false;
	KAlarmMainWindow::updateExpired();
}

/******************************************************************************
*  Return whether the program is configured to be running in the system tray.
*/
bool KAlarmApp::wantRunInSystemTray() const
{
	return Preferences::instance()->runInSystemTray()  &&  mKDEDesktop;
}

/******************************************************************************
* Called to schedule a new alarm, either in response to a DCOP notification or
* to command line options.
* Reply = true unless there was a parameter error or an error opening calendar file.
*/
bool KAlarmApp::scheduleEvent(KAEvent::Action action, const QString& text, const QDateTime& dateTime,
                              int flags, const QColor& bg, const QColor& fg, const QFont& font,
                              const QString& audioFile, float audioVolume, int reminderMinutes,
                              const KCal::Recurrence& recurrence, const EmailAddressList& mailAddresses,
                              const QString& mailSubject, const QStringList& mailAttachments)
	{
	kdDebug(5950) << "KAlarmApp::scheduleEvent(): " << text << endl;
	if (!dateTime.isValid())
		return false;
	QDateTime now = QDateTime::currentDateTime();
	if ((flags & KAEvent::LATE_CANCEL)  &&  dateTime < now.addSecs(-maxLateness()))
		return true;               // alarm time was already expired too long ago
	QDateTime alarmTime = dateTime;
	// Round down to the nearest minute to avoid scheduling being messed up
	alarmTime.setTime(QTime(alarmTime.time().hour(), alarmTime.time().minute(), 0));
	bool display = (alarmTime <= now);

	KAEvent event(alarmTime, text, bg, fg, font, action, flags);
	if (reminderMinutes)
	{
		bool onceOnly = (reminderMinutes < 0);
		event.setReminder((onceOnly ? -reminderMinutes : reminderMinutes), onceOnly);
	}
	if (!audioFile.isEmpty())
		event.setAudioFile(audioFile, audioVolume);
	if (mailAddresses.count())
		event.setEmail(mailAddresses, mailSubject, mailAttachments);
	event.setRecurrence(recurrence);
	if (display)
	{
		// Alarm is due for display already
		execAlarm(event, event.firstAlarm(), false);
		if (!event.recurs()
		||  event.setNextOccurrence(now) == KAEvent::NO_OCCURRENCE)
			return true;
	}
	mDcopQueue.append(DcopQEntry(event));
	QTimer::singleShot(0, this, SLOT(processQueue()));
	return true;
}

/******************************************************************************
* Called in response to a DCOP notification by the alarm daemon that an event
* should be handled, i.e. displayed or cancelled.
* Optionally display the event. Delete the event from the calendar file and
* from every main window instance.
*/
bool KAlarmApp::handleEvent(const QString& urlString, const QString& eventID, EventFunc function)
{
	kdDebug(5950) << "KAlarmApp::handleEvent(DCOP): " << eventID << endl;
	if (KURL(urlString).url() != AlarmCalendar::activeCalendar()->urlString())
	{
		kdError(5950) << "KAlarmApp::handleEvent(DCOP): wrong calendar file " << urlString << endl;
		return false;
	}
	mDcopQueue.append(DcopQEntry(function, eventID));
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
	kdDebug(5950) << "KAlarmApp::handleEvent(): " << eventID << ", " << (function==EVENT_TRIGGER?"TRIGGER":function==EVENT_CANCEL?"CANCEL":function==EVENT_HANDLE?"HANDLE":"?") << endl;
	Event* kcalEvent = AlarmCalendar::activeCalendar()->event(eventID);
	if (!kcalEvent)
	{
		kdError(5950) << "KAlarmApp::handleEvent(): event ID not found: " << eventID << endl;
		return false;
	}
	KAEvent event(*kcalEvent);
	switch (function)
	{
		case EVENT_TRIGGER:
		{
			// Only trigger one alarm from the event - we don't
			// want multiple identical messages, for example.
			KAAlarm alarm = event.firstAlarm();
			if (alarm.valid())
				execAlarm(event, alarm, true);
			break;
		}
		case EVENT_CANCEL:
			KAlarm::deleteEvent(event, true);
			break;

		case EVENT_HANDLE:
		{
			QDateTime now = QDateTime::currentDateTime();
			bool updateCalAndDisplay = false;
			bool displayAlarmValid = false;
			KAAlarm displayAlarm;
			// Check all the alarms in turn.
			// Note that the main alarm is fetched before any other alarms.
			for (KAAlarm alarm = event.firstAlarm();  alarm.valid();  alarm = event.nextAlarm(alarm))
			{
				// Check whether this alarm is due yet
				int secs = alarm.dateTime().secsTo(now);
				if (secs < 0)
				{
					kdDebug(5950) << "KAlarmApp::handleEvent(): alarm " << alarm.type() << ": not due\n";
					continue;
				}
				if (alarm.repeatAtLogin())
				{
					// Alarm is to be displayed at every login.
					// Check if the alarm has only just been set up.
					// (The alarm daemon will immediately notify that it is due
					//  since it is set up with a time in the past.)
					kdDebug(5950) << "KAlarmApp::handleEvent(): REPEAT_AT_LOGIN\n";
					if (secs < maxLateness())
						continue;

					// Check if the main alarm is already being displayed.
					// (We don't want to display both at the same time.)
					if (displayAlarm.valid())
						continue;

					// Set the time to be shown if it's a display alarm
					alarm.setTime(now);
				}
				if (alarm.lateCancel())
				{
					// Alarm is due, and it is to be cancelled if late.
					kdDebug(5950) << "KAlarmApp::handleEvent(): LATE_CANCEL\n";
					bool late = false;
					bool cancel = false;
					if (alarm.dateTime().isDateOnly())
					{
						// The alarm has no time, so cancel it if its date is past
						QDateTime limit(alarm.date().addDays(1), Preferences::instance()->startOfDay());
						if (now >= limit)
						{
							// It's too late to display the scheduled occurrence.
							// Find the last previous occurrence of the alarm.
							DateTime next;
							KAEvent::OccurType type = event.previousOccurrence(now, next);
							switch (type)
							{
								case KAEvent::FIRST_OCCURRENCE:
								case KAEvent::RECURRENCE_DATE:
								case KAEvent::RECURRENCE_DATE_TIME:
								case KAEvent::LAST_OCCURRENCE:
									limit.setDate(next.date().addDays(1));
									limit.setTime(Preferences::instance()->startOfDay());
									if (now >= limit)
									{
										if (type == KAEvent::LAST_OCCURRENCE)
											cancel = true;
										else
											late = true;
									}
									break;
								case KAEvent::NO_OCCURRENCE:
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
							DateTime next;
							KAEvent::OccurType type = event.previousOccurrence(now, next);
							switch (type)
							{
								case KAEvent::FIRST_OCCURRENCE:
								case KAEvent::RECURRENCE_DATE:
								case KAEvent::RECURRENCE_DATE_TIME:
								case KAEvent::LAST_OCCURRENCE:
									if (next.dateTime().secsTo(now) > maxlate)
									{
										if (type == KAEvent::LAST_OCCURRENCE)
											cancel = true;
										else
											late = true;
									}
									break;
								case KAEvent::NO_OCCURRENCE:
								default:
									late = true;
									break;
							}
						}
					}

					if (cancel)
					{
						// All repetitions are finished, so cancel the event
						event.setArchive();
						cancelAlarm(event, alarm.type(), false);
						updateCalAndDisplay = true;
						continue;
					}
					if (late)
					{
						// The latest repetition was too long ago, so schedule the next one
						rescheduleAlarm(event, alarm, false);
						updateCalAndDisplay = true;
						continue;
					}
				}
				if (!displayAlarmValid)
				{
					kdDebug(5950) << "KAlarmApp::handleEvent(): alarm " << alarm.type() << ": display\n";
					displayAlarm = alarm;             // note the alarm to be displayed
					displayAlarmValid = true;         // only trigger one alarm for the event
				}
				else
					kdDebug(5950) << "KAlarmApp::handleEvent(): alarm " << alarm.type() << ": skip\n";
			}

			// If there is an alarm to display, do this last after rescheduling/cancelling
			// any others. This ensures that the updated event is only saved once to the calendar.
			if (displayAlarm.valid())
				execAlarm(event, displayAlarm, true, !displayAlarm.repeatAtLogin());
			else if (updateCalAndDisplay)
				KAlarm::updateEvent(event, 0);     // update the window lists and calendar file
			else
				kdDebug(5950) << "KAlarmApp::handleEvent(): no action\n";
			break;
		}
	}
	return true;
}

/******************************************************************************
* Called when an alarm is currently being displayed, to store a copy of the
* alarm in the displaying calendar, and to reschedule it for its next repetition.
* If no repetitions remain, cancel it.
*/
void KAlarmApp::alarmShowing(KAEvent& event, KAAlarm::Type alarmType, const DateTime& alarmTime)
{
	kdDebug(5950) << "KAlarmApp::alarmShowing(" << event.id() << ", " << KAAlarm::debugType(alarmType) << ")\n";
	Event* kcalEvent = AlarmCalendar::activeCalendar()->event(event.id());
	if (!kcalEvent)
		kdError(5950) << "KAlarmApp::alarmShowing(): event ID not found: " << event.id() << endl;
	else
	{
		KAAlarm alarm = event.alarm(alarmType);
		if (!alarm.valid())
			kdError(5950) << "KAlarmApp::alarmShowing(): alarm type not found: " << event.id() << ":" << alarmType << endl;
		else
		{
			// Copy the alarm to the displaying calendar in case of a crash, etc.
			KAEvent dispEvent;
			dispEvent.setDisplaying(event, alarmType, alarmTime.dateTime());
			AlarmCalendar* cal = AlarmCalendar::displayCalendarOpen();
			if (cal)
			{
				cal->deleteEvent(dispEvent.id());   // in case it already exists
				cal->addEvent(dispEvent);
				cal->save();
			}

			rescheduleAlarm(event, alarm, true);
		}
	}
}

/******************************************************************************
* Reschedule the alarm for its next repetition. If none remain, delete it.
* If the alarm is deleted and it is the last alarm for its event, the event is
* removed from the calendar file and from every main window instance.
*/
void KAlarmApp::rescheduleAlarm(KAEvent& event, const KAAlarm& alarm, bool updateCalAndDisplay)
{
	kdDebug(5950) << "KAlarmApp::rescheduleAlarm()" << endl;
	bool update = false;
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
		switch (event.setNextOccurrence(QDateTime::currentDateTime()))
		{
			case KAEvent::NO_OCCURRENCE:
				// All repetitions are finished, so cancel the event
				cancelAlarm(event, alarm.type(), updateCalAndDisplay);
				break;
			case KAEvent::RECURRENCE_DATE:
			case KAEvent::RECURRENCE_DATE_TIME:
			case KAEvent::LAST_OCCURRENCE:
				// The event is due by now and repetitions still remain, so rewrite the event
				if (updateCalAndDisplay)
					update = true;
				else
					event.setUpdated();    // note that the calendar file needs to be updated
				break;
			case KAEvent::FIRST_OCCURRENCE:
				// The first occurrence is still due?!?, so don't do anything
			default:
				break;
		}
		if (event.deferred())
		{
			event.removeExpiredAlarm(KAAlarm::DEFERRED_ALARM);
			update = true;
		}
	}
	if (update)
		KAlarm::updateEvent(event, 0);     // update the window lists and calendar file
}

/******************************************************************************
* Delete the alarm. If it is the last alarm for its event, the event is removed
* from the calendar file and from every main window instance.
*/
void KAlarmApp::cancelAlarm(KAEvent& event, KAAlarm::Type alarmType, bool updateCalAndDisplay)
{
	kdDebug(5950) << "KAlarmApp::cancelAlarm()" << endl;
	if (alarmType == KAAlarm::MAIN_ALARM  &&  !event.displaying()  &&  event.toBeArchived())
	{
		// The event is being deleted. Save it in the expired calendar file first.
		QString id = event.id();    // save event ID since KAlarm::archiveEvent() changes it
		KAlarm::archiveEvent(event);
		event.setEventID(id);       // restore event ID
	}
	event.removeExpiredAlarm(alarmType);
	if (!event.alarmCount())
		KAlarm::deleteEvent(event, false);
	else if (updateCalAndDisplay)
		KAlarm::updateEvent(event, 0);    // update the window lists and calendar file
}

/******************************************************************************
* Execute an alarm by displaying its message or file, or executing its command.
* Reply = KProcess if command alarm
*       = 0 if an error message was output.
*/
void* KAlarmApp::execAlarm(KAEvent& event, const KAAlarm& alarm, bool reschedule, bool allowDefer)
{
	void* result = (void*)1;
	event.setArchive();
	if (alarm.action() == KAAlarm::COMMAND)
	{
		QString command = event.cleanText();
		kdDebug(5950) << "KAlarmApp::execAlarm(): COMMAND: " << command << endl;
		if (mNoShellAccess)
		{
			kdError(5950) << "KAlarmApp::execAlarm(): failed\n";
			QStringList errmsgs;
			errmsgs.append(i18n("Failed to execute command (shell access not authorized):"));
			errmsgs.append(command);
			(new MessageWin(event, alarm, errmsgs, reschedule))->show();
			result = 0;
		}
		else
		{
			// Find which shell to use.
			// This is a duplication of what KShellProcess does, but we need to know
			// which shell is used in order to decide what its exit code means.
			QCString shell = "/bin/sh";
			QCString envshell = QCString(getenv("SHELL")).stripWhiteSpace();
			if (!envshell.isEmpty())
			{
				struct stat fileinfo;
				if (stat(envshell.data(), &fileinfo) != -1  // ensure file exists
				&&  !S_ISDIR(fileinfo.st_mode)              // and it's not a directory
				&&  !S_ISCHR(fileinfo.st_mode)              // and it's not a character device
				&&  !S_ISBLK(fileinfo.st_mode)              // and it's not a block device
#ifdef S_ISSOCK
				&&  !S_ISSOCK(fileinfo.st_mode)             // and it's not a socket
#endif
				&&  !S_ISFIFO(fileinfo.st_mode)             // and it's not a fifo
				&&  !access(envshell.data(), X_OK))         // and it's executable
					shell = envshell;
			}
			// Get the shell filename with the path stripped
			QCString shellName = shell;
			int i = shellName.findRev('/');
			if (i >= 0)
				shellName = shellName.mid(i + 1);

			// Execute the command
			KShellProcess* proc = new KShellProcess(shell);
			*proc << command;
			connect(proc, SIGNAL(processExited(KProcess*)), SLOT(slotCommandExited(KProcess*)));
			mCommandProcesses.append(new ProcData(proc, new KAEvent(event), new KAAlarm(alarm), shellName));
			result = proc;
			if (!proc->start(KProcess::NotifyOnExit))
			{
				kdError(5950) << "KAlarmApp::execAlarm(): failed\n";
				QStringList errmsgs;
				errmsgs += i18n("Failed to execute command:");
				errmsgs += command;
				(new MessageWin(event, alarm, errmsgs, reschedule))->show();
				result = 0;
			}
		}
		if (reschedule)
			rescheduleAlarm(event, alarm, true);
	}
	else if (alarm.action() == KAAlarm::EMAIL)
	{
		kdDebug(5950) << "KAlarmApp::execAlarm(): EMAIL to: " << event.emailAddresses(", ") << endl;
		QString err = KAMail::send(event, (reschedule || allowDefer));
		if (!err.isNull())
		{
			QStringList errmsgs;
			if (err.isEmpty())
			{
				errmsgs += i18n("Failed to send email");
				kdDebug(5950) << "KAlarmApp::execAlarm(): failed\n";
			}
			else
			{
				errmsgs += i18n("Failed to send email:");
				errmsgs += err;
				kdDebug(5950) << "KAlarmApp::execAlarm(): failed: " << err << endl;
			}
			(new MessageWin(event, alarm, errmsgs, reschedule))->show();
			result = 0;
		}
		if (reschedule)
			rescheduleAlarm(event, alarm, true);
	}
	else
	{
		// Display a message or file, provided that the same event isn't already being displayed
		MessageWin* win = MessageWin::findEvent(event.id());
		if (!win
		||  !win->hasDefer() && !alarm.repeatAtLogin()
		||  (win->alarmType() & KAAlarm::REMINDER_ALARM) && !(alarm.type() & KAAlarm::REMINDER_ALARM))
		{
			// Either there isn't already a message for this event,
			// or there is a repeat-at-login message with no Defer
			// button, which needs to be replaced with a new message,
			// or the caption needs to be changed from "Reminder" to "Message".
			delete win;
			(new MessageWin(event, alarm, reschedule, allowDefer))->show();
		}
		else
		{
			// Update the existing message window
			win->repeat(alarm);    // N.B. this reschedules the alarm
		}
	}
	return result;
}

/******************************************************************************
* Called when a command alarm execution completes.
*/
void KAlarmApp::slotCommandExited(KProcess* proc)
{
	kdDebug(5950) << "KAlarmApp::slotCommandExited()\n";
	// Find this command in the command list
	for (ProcData* pd = mCommandProcesses.first();  pd;  pd = mCommandProcesses.next())
	{
		if (pd->process == proc)
		{
			// Found the command. Check its exit status.
			QString errmsg;
			if (!proc->normalExit())
			{
				kdWarning(5950) << "KAlarmApp::slotCommandExited(" << pd->event->cleanText() << "): killed\n";
				errmsg = i18n("Command execution error:");
			}
			else
			{
				// Some shells report if the command couldn't be found, or is not executable
				int status = proc->exitStatus();
				if (pd->shell == "bash"  && (status == 126 || status == 127)
				||  pd->shell == "ksh"  &&  status == 127)
				{
					kdWarning(5950) << "KAlarmApp::slotCommandExited(" << pd->event->cleanText() << ") " << pd->shell << ": not found or not executable\n";
					errmsg = i18n("Failed to execute command:");
				}
			}
			if (!errmsg.isNull())
			{
				if (pd->messageBoxParent)
				{
					// Close the existing informational message box for this process
					QObjectList* dialogs = pd->messageBoxParent->queryList("KDialogBase", 0, false, true);
					KDialogBase* dialog = (KDialogBase*)dialogs->getFirst();
					delete dialog;
					delete dialogs;
					errmsg += "\n";
					errmsg += pd->event->cleanText();
					KMessageBox::error(pd->messageBoxParent, errmsg);
				}
				else
				{
					QStringList errmsgs;
					errmsgs.append(errmsg);
					errmsgs.append(pd->event->cleanText());
					(new MessageWin(*pd->event, *pd->alarm, errmsgs, false))->show();
				}
			}
			mCommandProcesses.remove();
		}
	}
}

/******************************************************************************
* Notes that a informational KMessageBox is displayed for this process.
*/
void KAlarmApp::commandMessage(KProcess* proc, QWidget* parent)
{
	// Find this command in the command list
	for (ProcData* pd = mCommandProcesses.first();  pd;  pd = mCommandProcesses.next())
	{
		if (pd->process == proc)
			pd->messageBoxParent = parent;
	}
}

/******************************************************************************
* Set up the DCOP handlers.
*/
void KAlarmApp::setUpDcop()
{
	if (!mDcopHandler)
	{
		mDcopHandler      = new DcopHandler();
		mDaemonGuiHandler = new DaemonGuiHandler();
	}
}

/******************************************************************************
* If this is the first time through, open the calendar file, optionally start
* the alarm daemon and register with it, and set up the DCOP handler.
*/
bool KAlarmApp::initCheck(bool calendarOnly)
{
	bool startdaemon;
	AlarmCalendar* cal = AlarmCalendar::activeCalendar();
	if (!cal->isOpen())
	{
		kdDebug(5950) << "KAlarmApp::initCheck(): opening active calendar\n";

		// First time through. Open the calendar file.
		if (!cal->open())
			return false;

		if (!mStartOfDay.isValid())
			changeStartOfDay();     // start of day time has changed, so adjust date-only alarms

		/* Need to open the display calendar now, since otherwise if the daemon
		 * immediately notifies display alarms, they will often be processed while
		 * redisplayAlarms() is executing open() (but before open() completes),
		 * which causes problems!!
		 */
		AlarmCalendar::displayCalendar()->open();

		/* Need to open the expired alarm calendar now, since otherwise if the daemon
		 * immediately notifies multiple alarms, the second alarm is likely to be
		 * processed while the calendar is executing open() (but before open() completes),
		 * which causes a hang!!
		 */
		AlarmCalendar::expiredCalendar()->open();
		AlarmCalendar::expiredCalendar()->setPurgeDays(theInstance->mPrefsExpiredKeepDays);

		startdaemon = true;
	}
	else
		startdaemon = !Daemon::isRegistered();

	if (!calendarOnly)
	{
		setUpDcop();              // we're now ready to handle DCOP calls, so set up handlers
		if (startdaemon)
			Daemon::start();  // make sure the alarm daemon is running
	}
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
	if ((s = strchr(timeStr, ':')) == 0)
		noTime = true;
	else
	{
		noTime = false;
		*s++ = 0;
		dt[4] = strtoul(s, &end, 10);
		if (end == s  ||  *end  ||  dt[4] >= 60)
			return false;
		// Get the hour value
		if ((s = strrchr(timeStr, '-')) == 0)
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
		if ((s = strrchr(timeStr, '-')) == 0)
			s = timeStr;
		else
			*s++ = 0;
		dt[2] = strtoul(s, &end, 10);
		if (end == s  ||  *end  ||  dt[2] == 0  ||  dt[2] > 31)
			return false;
		if (s != timeStr)
		{
			// Get the month value
			if ((s = strrchr(timeStr, '-')) == 0)
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

/******************************************************************************
*  Convert a time interval command line parameter.
*  Reply = true if successful.
*/
static bool convInterval(QCString timeParam, KAEvent::RecurType& recurType, int& timeInterval)
{
	// Get the recurrence interval
	bool ok = true;
	uint interval = 0;
	bool negative = (timeParam[0] == '-');
	if (negative)
		timeParam = timeParam.right(1);
	uint length = timeParam.length();
	switch (timeParam[length - 1])
	{
		case 'Y':
			recurType = KAEvent::ANNUAL_DATE;
			timeParam = timeParam.left(length - 1);
			break;
		case 'W':
			recurType = KAEvent::WEEKLY;
			timeParam = timeParam.left(length - 1);
			break;
		case 'D':
			recurType = KAEvent::DAILY;
			timeParam = timeParam.left(length - 1);
			break;
		case 'M':
		{
			int i = timeParam.find('H');
			if (i < 0)
				recurType = KAEvent::MONTHLY_DAY;
			else
			{
				recurType = KAEvent::MINUTELY;
				interval = timeParam.left(i).toUInt(&ok) * 60;
				timeParam = timeParam.mid(i + 1, length - i - 2);
			}
			break;
		}
		default:       // should be a digit
			recurType = KAEvent::MINUTELY;
			break;
	}
	if (ok)
		interval += timeParam.toUInt(&ok);
	timeInterval = static_cast<int>(interval);
	if (negative)
		timeInterval = -timeInterval;
	return ok;
}


KAlarmApp::ProcData::~ProcData()
{
	delete process;
	delete event;
	delete alarm;
}
