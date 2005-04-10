/*
 *  kalarmapp.cpp  -  the KAlarm application object
 *  Program:  kalarm
 *  (C) 2001 - 2005 by David Jarvie <software@astrojar.org.uk>
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

#include "kalarm.h"

#include <stdlib.h>
#include <ctype.h>
#include <iostream>

#include <qobjectlist.h>
#include <qtimer.h>
#include <qregexp.h>
#include <qfile.h>

#include <kcmdlineargs.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <dcopclient.h>
#include <kprocess.h>
#include <ktempfile.h>
#include <kfileitem.h>
#include <kstdguiitem.h>
#include <ktrader.h>
#include <kstaticdeleter.h>
#include <kdebug.h>

#include <kalarmd/clientinfo.h>

#include <libkcal/icalformat.h>

#include "alarmcalendar.h"
#include "alarmlistview.h"
#include "editdlg.h"
#include "daemon.h"
#include "dcophandler.h"
#include "functions.h"
#include "kamail.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "messagewin.h"
#include "preferences.h"
#include "prefdlg.h"
#include "shellprocess.h"
#include "traywindow.h"
#include "kalarmapp.moc"

#include <netwm.h>

int         marginKDE2 = 0;

static bool convWakeTime(const QCString timeParam, QDateTime&, bool& noTime);
static bool convInterval(QCString timeParam, KAEvent::RecurType&, int& timeInterval, bool allowMonthYear = true);

/******************************************************************************
* Find the maximum number of seconds late which a late-cancel alarm is allowed
* to be. This is calculated as the alarm daemon's check interval, plus a few
* seconds leeway to cater for any timing irregularities.
*/
static inline int maxLateness(int lateCancel)
{
	static const int LATENESS_LEEWAY = 5;
	int lc = (lateCancel >= 1) ? (lateCancel - 1)*60 : 0;
	return Daemon::maxTimeSinceCheck() + LATENESS_LEEWAY + lc;
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
	  mDcopHandler(new DcopHandler()),
#ifdef OLD_DCOP
	  mDcopHandlerOld(new DcopHandlerOld()),
#endif
	  mTrayWindow(0),
	  mPendingQuit(false),
	  mProcessingQueue(false),
	  mCheckingSystemTray(false),
	  mSessionClosingDown(false),
	  mRefreshExpiredAlarms(false),
	  mSpeechEnabled(false)
{
#if KDE_VERSION < 290
	marginKDE2 = KDialog::marginHint();
#endif
	mCommandProcesses.setAutoDelete(true);
	Preferences* preferences = Preferences::instance();
	connect(preferences, SIGNAL(preferencesChanged()), SLOT(slotPreferencesChanged()));
	KCal::CalFormat::setApplication(aboutData()->programName(),
	                          QString::fromLatin1("-//K Desktop Environment//NONSGML %1 " KALARM_VERSION "//EN")
	                                       .arg(aboutData()->programName()));
	KAEvent::setFeb29RecurType();

	// Check if it's a KDE desktop by comparing the window manager name to "KWin"
	NETRootInfo nri(qt_xdisplay(), NET::SupportingWMCheck);
	const char* wmname = nri.wmName();
	mKDEDesktop = wmname && !strcmp(wmname, "KWin");

	if (AlarmCalendar::initialiseCalendars())
	{
		connect(AlarmCalendar::expiredCalendar(), SIGNAL(purged()), SLOT(slotExpiredPurged()));

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
	}

	// Check if the speech synthesis daemon is installed
	mSpeechEnabled = (KTrader::self()->query("DCOP/Text-to-Speech", "Name == 'KTTSD'").count() > 0);
	if (!mSpeechEnabled)
		kdDebug(5950) << "KAlarmApp::KAlarmApp(): speech synthesis disabled (KTTSD not found)" << endl;
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

		if (mFatalError)
			theInstance->quitFatal();
		else
		{
			// This is here instead of in the constructor to avoid recursion
			Daemon::initialise();    // calendars must be initialised before calling this
		}
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
	if (mFatalError)
	{
		quitFatal();
		return false;
	}

	// Process is being restored by session management.
	kdDebug(5950) << "KAlarmApp::restoreSession(): Restoring\n";
	++mActiveCount;
	if (!initCheck(true))     // open the calendar file (needed for main windows)
	{
		--mActiveCount;
		quitIf(1, true);    // error opening the main calendar - quit
		return true;
	}
	MainWindow* trayParent = 0;
	for (int i = 1;  KMainWindow::canBeRestored(i);  ++i)
	{
		QString type = KMainWindow::classNameOfToplevel(i);
		if (type == QString::fromLatin1("MainWindow"))
		{
			MainWindow* win = MainWindow::create(true);
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
			if (win->isValid())
				win->show();
			else
				delete win;
		}
	}
	initCheck();           // register with the alarm daemon

	// Try to display the system tray icon if it is configured to be autostarted,
	// or if we're in run-in-system-tray mode.
	if (Preferences::instance()->autostartTrayIcon()
	||  MainWindow::count()  &&  wantRunInSystemTray())
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
	if (mFatalError)
	{
		quitFatal();
		return 1;
	}
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
				setUpDcop();        // start processing DCOP calls
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
				QCString         alFromID;
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
					if (args->isSet("from-id"))
						alFromID = args->getOption("from-id");
					QCStringList params = args->getOptionList("mail");
					for (QCStringList::Iterator i = params.begin();  i != params.end();  ++i)
					{
						QString addr = QString::fromLocal8Bit(*i);
						if (!KAMail::checkAddress(addr))
							USAGE(i18n("%1: invalid email address").arg(QString::fromLatin1("--mail")))
						alAddresses += KCal::Person(QString::null, addr);
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
					if (args->isSet("from-id"))
						USAGE(i18n("%1 requires %2").arg(QString::fromLatin1("--from-id")).arg(QString::fromLatin1("--mail")))
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
				int       repeatCount    = 0;
				int       repeatInterval = 0;
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

				bool haveRecurrence = args->isSet("recurrence");
				if (haveRecurrence)
				{
					if (args->isSet("login"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--login")).arg(QString::fromLatin1("--recurrence")))
					if (args->isSet("until"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--until")).arg(QString::fromLatin1("--recurrence")))
					QCString rule = args->getOption("recurrence");
					KCal::ICalFormat format;
					format.fromString(&recurrence, QString::fromLocal8Bit((const char*)rule));
				}
				if (args->isSet("interval"))
				{
					// Repeat count is specified
					int count;
					if (args->isSet("login"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--login")).arg(QString::fromLatin1("--interval")))
					bool ok;
					if (args->isSet("repeat"))
					{
						count = args->getOption("repeat").toInt(&ok);
						if (!ok || !count || count < -1 || (count < 0 && haveRecurrence))
							USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("--repeat")))
					}
					else if (haveRecurrence)
						USAGE(i18n("%1 requires %2").arg(QString::fromLatin1("--interval")).arg(QString::fromLatin1("--repeat")))
					else if (args->isSet("until"))
					{
						count = 0;
						QCString dateTime = args->getOption("until");
						if (!convWakeTime(dateTime, endTime, alarmNoTime))
							USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("--until")))
						if (endTime < alarmTime)
							USAGE(i18n("%1 earlier than %2").arg(QString::fromLatin1("--until")).arg(QString::fromLatin1("--time")))
					}
					else
						count = -1;

					// Get the recurrence interval
					int interval;
					KAEvent::RecurType recurType;
					if (!convInterval(args->getOption("interval"), recurType, interval, !haveRecurrence)
					||  interval < 0)
						USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("--interval")))
					if (alarmNoTime  &&  recurType == KAEvent::MINUTELY)
						USAGE(i18n("Invalid %1 parameter for date-only alarm").arg(QString::fromLatin1("--interval")))

					if (haveRecurrence)
					{
						// There is a also a recurrence specified, so set up a simple repetition
						int longestInterval = KAEvent::longestRecurrenceInterval(recurrence);
						if (count * interval > longestInterval)
							USAGE(i18n("Invalid %1 and %2 parameters: repetition is longer than %3 interval").arg(QString::fromLatin1("--interval")).arg(QString::fromLatin1("--repeat")).arg(QString::fromLatin1("--recurrence")));
						repeatCount    = count;
						repeatInterval = interval;
					}
					else
					{
						// There is no other recurrence specified, so convert the repetition
						// parameters into a KCal::Recurrence
						KAEvent::setRecurrence(recurrence, recurType, interval, count, DateTime(alarmTime, alarmNoTime), endTime);
					}
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
					if (args->isSet("speak"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--speak")).arg(QString::fromLatin1(audioRepeat ? "--play-repeat" : "--play")))
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
				if (args->isSet("speak"))
				{
					if (args->isSet("beep"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--beep")).arg(QString::fromLatin1("--speak")))
					if (!mSpeechEnabled)
						USAGE(i18n("%1 requires speech synthesis to be configured using KTTSD").arg(QString::fromLatin1("--speak")))
				}
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

				int lateCancel = 0;
				if (args->isSet("late-cancel"))
				{
					KAEvent::RecurType recur;
					bool ok = convInterval(args->getOption("late-cancel"), recur, lateCancel, false);
					if (!ok  ||  lateCancel <= 0)
						USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("late-cancel")))
				}
				else if (args->isSet("auto-close"))
					USAGE(i18n("%1 requires %2").arg(QString::fromLatin1("--auto-close")).arg(QString::fromLatin1("--late-cancel")))

				int flags = KAEvent::DEFAULT_FONT;
				if (args->isSet("ack-confirm"))
					flags |= KAEvent::CONFIRM_ACK;
				if (args->isSet("auto-close"))
					flags |= KAEvent::AUTO_CLOSE;
				if (args->isSet("beep"))
					flags |= KAEvent::BEEP;
				if (args->isSet("speak"))
					flags |= KAEvent::SPEAK;
				if (args->isSet("disable"))
					flags |= KAEvent::DISABLED;
				if (audioRepeat)
					flags |= KAEvent::REPEAT_SOUND;
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
				if (!scheduleEvent(action, alMessage, alarmTime, lateCancel, flags, bgColour, fgColour, QFont(), audioFile,
				                   audioVolume, reminderMinutes, recurrence, repeatInterval, repeatCount,
				                   alFromID, alAddresses, alSubject, alAttachments))
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
				if (args->isSet("auto-close"))
					usage += QString::fromLatin1("--auto-close ");
				if (args->isSet("bcc"))
					usage += QString::fromLatin1("--bcc ");
				if (args->isSet("beep"))
					usage += QString::fromLatin1("--beep ");
				if (args->isSet("color"))
					usage += QString::fromLatin1("--color ");
				if (args->isSet("colorfg"))
					usage += QString::fromLatin1("--colorfg ");
				if (args->isSet("disable"))
					usage += QString::fromLatin1("--disable ");
				if (args->isSet("from-id"))
					usage += QString::fromLatin1("--from-id ");
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
				if (args->isSet("speak"))
					usage += QString::fromLatin1("--speak ");
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

				(MainWindow::create())->show();
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
		MainWindow::closeAll();
		displayTrayIcon(false);
		if (MessageWin::instanceCount())
			return;
	}
	else
	{
		// Quit only if there are no more "instances" running
		mPendingQuit = false;
		if (mActiveCount > 0  ||  MessageWin::instanceCount())
			return;
		int mwcount = MainWindow::count();
		MainWindow* mw = mwcount ? MainWindow::firstWindow() : 0;
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
		if (mDcopQueue.count()  ||  mCommandProcesses.count())
		{
			// Don't quit yet if there are outstanding actions on the DCOP queue
			mPendingQuit = true;
			mPendingQuitCode = exitCode;
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
	&&  MessageBox::warningContinueCancel(parent, KMessageBox::Cancel,
	                                      i18n("Quitting will disable alarms\n(once any alarm message windows are closed)."),
	                                      QString::null, KStdGuiItem::quit(), Preferences::QUIT_WARN
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
	if (mInitialised  &&  !mProcessingQueue)
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
			{
				// It's a new alarm
				switch (entry.function)
				{
				case EVENT_TRIGGER:
					execAlarm(entry.event, entry.event.firstAlarm(), false);
					break;
				case EVENT_HANDLE:
					KAlarm::addEvent(entry.event, 0);
					break;
				case EVENT_CANCEL:
					break;
				}
			}
			else
				handleEvent(entry.eventId, entry.function);
			mDcopQueue.pop_front();
		}

		// Purge the expired alarms calendar if it's time to do so
		AlarmCalendar::expiredCalendar()->purgeIfQueued();

		// Now that the queue has been processed, quit if a quit was queued
		if (mPendingQuit)
			quitIf(mPendingQuitCode);

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
		KCal::Event::List events = cal->events();
		for (KCal::Event::List::ConstIterator it = events.begin();  it != events.end();  ++it)
		{
                        KCal::Event* kcalEvent = *it;
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
bool KAlarmApp::displayTrayIcon(bool show, MainWindow* parent)
{
	static bool creating = false;
	if (show)
	{
		if (!mTrayWindow  &&  !creating)
		{
			if (!mKDEDesktop)
				return false;
			if (!MainWindow::count()  &&  wantRunInSystemTray())
			{
				creating = true;    // prevent main window constructor from creating an additional tray icon
				parent = MainWindow::create();
				creating = false;
			}
			mTrayWindow = new TrayWindow(parent ? parent : MainWindow::firstWindow());
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
MainWindow* KAlarmApp::trayMainWindow() const
{
	return mTrayWindow ? mTrayWindow->assocMainWindow() : 0;
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
		MainWindow* win = mTrayWindow ? mTrayWindow->assocMainWindow() : 0;
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
		MainWindow::updateTimeColumns(mPrefsShowTime, mPrefsShowTimeTo);
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
		MainWindow::updateExpired();
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
	MainWindow::updateExpired();
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
                              int lateCancel, int flags, const QColor& bg, const QColor& fg, const QFont& font,
                              const QString& audioFile, float audioVolume, int reminderMinutes,
                              const KCal::Recurrence& recurrence, int repeatInterval, int repeatCount,
                              const QString& mailFromID, const EmailAddressList& mailAddresses,
                              const QString& mailSubject, const QStringList& mailAttachments)
{
	kdDebug(5950) << "KAlarmApp::scheduleEvent(): " << text << endl;
	if (!dateTime.isValid())
		return false;
	QDateTime now = QDateTime::currentDateTime();
	if (lateCancel  &&  dateTime < now.addSecs(-maxLateness(lateCancel)))
		return true;               // alarm time was already expired too long ago
	QDateTime alarmTime = dateTime;
	// Round down to the nearest minute to avoid scheduling being messed up
	alarmTime.setTime(QTime(alarmTime.time().hour(), alarmTime.time().minute(), 0));

	KAEvent event(alarmTime, text, bg, fg, font, action, lateCancel, flags);
	if (reminderMinutes)
	{
		bool onceOnly = (reminderMinutes < 0);
		event.setReminder((onceOnly ? -reminderMinutes : reminderMinutes), onceOnly);
	}
	if (!audioFile.isEmpty())
		event.setAudioFile(audioFile, audioVolume, -1, 0);
	if (mailAddresses.count())
		event.setEmail(mailFromID, mailAddresses, mailSubject, mailAttachments);
	event.setRecurrence(recurrence);
	event.setFirstRecurrence();
	event.setRepetition(repeatInterval, repeatCount - 1);
	if (alarmTime <= now)
	{
		// Alarm is due for display already.
		// First execute it once without adding it to the calendar file.
		if (!mInitialised)
			mDcopQueue.append(DcopQEntry(event, EVENT_TRIGGER));
		else
			execAlarm(event, event.firstAlarm(), false);
		// If it's a recurring alarm, reschedule it for its next occurrence
		if (!event.recurs()
		||  event.setNextOccurrence(now, true) == KAEvent::NO_OCCURRENCE)
			return true;
		// It has recurrences in the future
	}

	// Queue the alarm for insertion into the calendar file
	mDcopQueue.append(DcopQEntry(event));
	if (mInitialised)
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
	AlarmCalendar* cal = AlarmCalendar::activeCalendar();     // this can be called before calendars have been initialised
	if (cal  &&  KURL(urlString).url() != cal->urlString())
	{
		kdError(5950) << "KAlarmApp::handleEvent(DCOP): wrong calendar file " << urlString << endl;
		return false;
	}
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
	kdDebug(5950) << "KAlarmApp::handleEvent(): " << eventID << ", " << (function==EVENT_TRIGGER?"TRIGGER":function==EVENT_CANCEL?"CANCEL":function==EVENT_HANDLE?"HANDLE":"?") << endl;
	KCal::Event* kcalEvent = AlarmCalendar::activeCalendar()->event(eventID);
	if (!kcalEvent)
	{
		kdError(5950) << "KAlarmApp::handleEvent(): event ID not found: " << eventID << endl;
		return false;
	}
	KAEvent event(*kcalEvent);
	switch (function)
	{
		case EVENT_CANCEL:
			KAlarm::deleteEvent(event, true);
			break;

		case EVENT_TRIGGER:    // handle it if it's due, else execute it regardless
		case EVENT_HANDLE:     // handle it if it's due
		{
			QDateTime now = QDateTime::currentDateTime();
			DateTime  repeatDT;
			bool updateCalAndDisplay = false;
			bool displayAlarmValid = false;
			KAAlarm displayAlarm;
			// Check all the alarms in turn.
			// Note that the main alarm is fetched before any other alarms.
			for (KAAlarm alarm = event.firstAlarm();  alarm.valid();  alarm = event.nextAlarm(alarm))
			{
				if (alarm.deferred()  &&  event.repeatCount()
				&&  repeatDT.isValid()  &&  alarm.dateTime() > repeatDT)
				{
					// This deferral of a repeated alarm is later than the last occurrence
					// of the main alarm, so use the deferral alarm instead.
					// If the deferral is not yet due, this prevents the main alarm being
					// triggered repeatedly. If the deferral is due, this triggers it
					// in preference to the main alarm.
					displayAlarm        = KAAlarm();
					displayAlarmValid   = false;
					updateCalAndDisplay = false;
				}
				int secs = alarm.dateTime().secsTo(now);
				if (secs < 0)
				{
					// This alarm is not due yet
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
					if (secs < maxLateness(1))
						continue;

					// Check if the main alarm is already being displayed.
					// (We don't want to display both at the same time.)
					if (displayAlarm.valid())
						continue;

					// Set the time to be shown if it's a display alarm
					alarm.setTime(now);
				}
				if (event.repeatCount()  &&  alarm.type() == KAAlarm::MAIN_ALARM)
				{
					// Alarm has a simple repetition. Since its time in the calendr remains the same
					// until its repetitions are finished, adjust its time to the correct repetition
					KAEvent::OccurType type = event.previousOccurrence(now.addSecs(1), repeatDT, true);
					if (type & KAEvent::OCCURRENCE_REPEAT)
					{
						alarm.setTime(repeatDT);
						secs = repeatDT.secsTo(now);
					}
				}
				if (alarm.lateCancel())
				{
					// Alarm is due, and it is to be cancelled if too late.
					kdDebug(5950) << "KAlarmApp::handleEvent(): LATE_CANCEL\n";
					bool late = false;
					bool cancel = false;
					if (alarm.dateTime().isDateOnly())
					{
						// The alarm has no time, so cancel it if its date is too far past
						int maxlate = alarm.lateCancel() / 1440;    // maximum lateness in days
						QDateTime limit(alarm.date().addDays(maxlate + 1), Preferences::instance()->startOfDay());
						if (now >= limit)
						{
							// It's too late to display the scheduled occurrence.
							// Find the last previous occurrence of the alarm.
							DateTime next;
							KAEvent::OccurType type = event.previousOccurrence(now, next, true);
							switch (type & ~KAEvent::OCCURRENCE_REPEAT)
							{
								case KAEvent::FIRST_OCCURRENCE:
								case KAEvent::RECURRENCE_DATE:
								case KAEvent::RECURRENCE_DATE_TIME:
								case KAEvent::LAST_RECURRENCE:
									limit.setDate(next.date().addDays(maxlate + 1));
									limit.setTime(Preferences::instance()->startOfDay());
									if (now >= limit)
									{
										if (type == KAEvent::LAST_RECURRENCE)
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
						int maxlate = maxLateness(alarm.lateCancel());
						if (secs > maxlate)
						{
							// It's over the maximum interval late.
							// Find the most recent occurrence of the alarm.
							DateTime next;
							KAEvent::OccurType type = event.previousOccurrence(now, next, true);
							switch (type & ~KAEvent::OCCURRENCE_REPEAT)
							{
								case KAEvent::FIRST_OCCURRENCE:
								case KAEvent::RECURRENCE_DATE:
								case KAEvent::RECURRENCE_DATE_TIME:
								case KAEvent::LAST_RECURRENCE:
									if (next.dateTime().secsTo(now) > maxlate)
									{
										if (type == KAEvent::LAST_RECURRENCE)
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
			else
			{
				if (function == EVENT_TRIGGER)
				{
					// The alarm is to be executed regardless of whether it's due.
					// Only trigger one alarm from the event - we don't want multiple
					// identical messages, for example.
					KAAlarm alarm = event.firstAlarm();
					if (alarm.valid())
						execAlarm(event, alarm, false);
				}
				if (updateCalAndDisplay)
					KAlarm::updateEvent(event, 0);     // update the window lists and calendar file
				else if (function != EVENT_TRIGGER)
					kdDebug(5950) << "KAlarmApp::handleEvent(): no action\n";
			}
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
	KCal::Event* kcalEvent = AlarmCalendar::activeCalendar()->event(event.id());
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
* Called when an alarm action has completed, to perform any post-alarm actions.
*/
void KAlarmApp::alarmCompleted(const KAEvent& event)
{
	if (!event.postAction().isEmpty()  &&  ShellProcess::authorised())
	{
		QString command = event.postAction();
		kdDebug(5950) << "KAlarmApp::alarmCompleted(" << event.id() << "): " << command << endl;
		doShellCommand(command, event, 0, ProcData::POST_ACTION);
	}
}

/******************************************************************************
* Reschedule the alarm for its next recurrence. If none remain, delete it.
* If the alarm is deleted and it is the last alarm for its event, the event is
* removed from the calendar file and from every main window instance.
*/
void KAlarmApp::rescheduleAlarm(KAEvent& event, const KAAlarm& alarm, bool updateCalAndDisplay)
{
	kdDebug(5950) << "KAlarmApp::rescheduleAlarm()" << endl;
	bool update        = false;
	bool updateDisplay = false;
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
		QDateTime now = QDateTime::currentDateTime();
		if (event.repeatCount()  &&  event.mainEndRepeatTime() > now)
			updateDisplay = true;    // there are more repetitions to come, so just update time in alarm list
		else
		{
			// The alarm's repetitions (if any) are finished.
			// Reschedule it for its next recurrence.
			switch (event.setNextOccurrence(now))
			{
				case KAEvent::NO_OCCURRENCE:
					// All repetitions are finished, so cancel the event
					cancelAlarm(event, alarm.type(), updateCalAndDisplay);
					break;
				case KAEvent::RECURRENCE_DATE:
				case KAEvent::RECURRENCE_DATE_TIME:
				case KAEvent::LAST_RECURRENCE:
					// The event is due by now and repetitions still remain, so rewrite the event
					if (updateCalAndDisplay)
						update = true;
					else
					{
						event.cancelCancelledDeferral();
						event.setUpdated();    // note that the calendar file needs to be updated
					}
					break;
				case KAEvent::FIRST_OCCURRENCE:
					// The first occurrence is still due?!?, so don't do anything
				default:
					break;
			}
		}
		if (event.deferred())
		{
			// Just in case there's also a deferred alarm, ensure it's removed
			event.removeExpiredAlarm(KAAlarm::DEFERRED_ALARM);
			update = true;
		}
	}
	if (update)
	{
		event.cancelCancelledDeferral();
		KAlarm::updateEvent(event, 0);     // update the window lists and calendar file
	}
	else if (updateDisplay)
		AlarmListView::modifyEvent(event, 0);
}

/******************************************************************************
* Delete the alarm. If it is the last alarm for its event, the event is removed
* from the calendar file and from every main window instance.
*/
void KAlarmApp::cancelAlarm(KAEvent& event, KAAlarm::Type alarmType, bool updateCalAndDisplay)
{
	kdDebug(5950) << "KAlarmApp::cancelAlarm()" << endl;
	event.cancelCancelledDeferral();
	if (alarmType == KAAlarm::MAIN_ALARM  &&  !event.displaying()  &&  event.toBeArchived())
	{
		// The event is being deleted. Save it in the expired calendar file first.
		QString id = event.id();    // save event ID since KAlarm::addExpiredEvent() changes it
		KAlarm::addExpiredEvent(event);
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
* Reply = ShellProcess instance if a command alarm
*       != 0 if successful
*       = 0 if the alarm is disabled, or if an error message was output.
*/
void* KAlarmApp::execAlarm(KAEvent& event, const KAAlarm& alarm, bool reschedule, bool allowDefer, bool noPreAction)
{
	if (!event.enabled())
	{
		// The event is disabled.
		if (reschedule)
			rescheduleAlarm(event, alarm, true);
		return 0;
	}

	void* result = (void*)1;
	event.setArchive();
	switch (alarm.action())
	{
		case KAAlarm::MESSAGE:
		case KAAlarm::FILE:
		{
			// Display a message or file, provided that the same event isn't already being displayed
			MessageWin* win = MessageWin::findEvent(event.id());
			if (!win  &&  !noPreAction  &&  !event.preAction().isEmpty()  &&  ShellProcess::authorised())
			{
				// There is no message window currently displayed for this alarm,
				// and we need to execute a command before displaying the new window.
				QString command = event.preAction();
				kdDebug(5950) << "KAlarmApp::execAlarm(): pre-DISPLAY command: " << command << endl;
				int flags = (reschedule ? ProcData::RESCHEDULE : 0) | (allowDefer ? ProcData::ALLOW_DEFER : 0);
				if (doShellCommand(command, event, &alarm, (flags | ProcData::PRE_ACTION)))
					return result;     // display the message after the command completes
				// Error executing command - display the message even though it failed
			}
			if (!event.enabled())
				delete win;        // event is disabled - close its window
			else if (!win
			     ||  !win->hasDefer() && !alarm.repeatAtLogin()
			     ||  (win->alarmType() & KAAlarm::REMINDER_ALARM) && !(alarm.type() & KAAlarm::REMINDER_ALARM))
			{
				// Either there isn't already a message for this event,
				// or there is a repeat-at-login message with no Defer
				// button, which needs to be replaced with a new message,
				// or the caption needs to be changed from "Reminder" to "Message".
				if (win)
					win->setRecreating();    // prevent post-alarm actions
				delete win;
				(new MessageWin(event, alarm, reschedule, allowDefer))->show();
			}
			else
			{
				// Raise the existing message window and replay any sound
				win->repeat(alarm);    // N.B. this reschedules the alarm
			}
			break;
		}
		case KAAlarm::COMMAND:
		{
			int flags = event.commandXterm() ? ProcData::EXEC_IN_XTERM : 0;
			QString command = event.cleanText();
			if (event.commandScript())
			{
				// Store the command script in a temporary file for execution
				kdDebug(5950) << "KAlarmApp::execAlarm(): COMMAND: (script)" << endl;
				QString tmpfile = createTempScriptFile(command, false, event, alarm);
				if (tmpfile.isEmpty())
				{
					QStringList errmsgs(i18n("Error creating temporary script file"));
					(new MessageWin(event, alarm.dateTime(), errmsgs))->show();
					result = 0;
				}
				else
					result = doShellCommand(tmpfile, event, &alarm, (flags | ProcData::TEMP_FILE));
			}
			else
			{
				kdDebug(5950) << "KAlarmApp::execAlarm(): COMMAND: " << command << endl;
				result = doShellCommand(command, event, &alarm, flags);
			}
			if (reschedule)
				rescheduleAlarm(event, alarm, true);
			break;
		}
		case KAAlarm::EMAIL:
		{
			kdDebug(5950) << "KAlarmApp::execAlarm(): EMAIL to: " << event.emailAddresses(", ") << endl;
			QStringList errmsgs;
			if (!KAMail::send(event, errmsgs, (reschedule || allowDefer)))
				result = 0;
			if (errmsgs.count())
			{
				// Some error occurred, although the email may have been sent successfully
				if (result)
					kdDebug(5950) << "KAlarmApp::execAlarm(): copy error: " << errmsgs[1] << endl;
				else
					kdDebug(5950) << "KAlarmApp::execAlarm(): failed: " << errmsgs[1] << endl;
				(new MessageWin(event, alarm.dateTime(), errmsgs))->show();
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
* Execute a shell command specified by an alarm.
* If the PRE_ACTION bit of 'flags' is set, the alarm will be executed via
* execAlarm() once the command completes, the execAlarm() parameters being
* derived from the remaining bits in 'flags'.
*/
ShellProcess* KAlarmApp::doShellCommand(const QString& command, const KAEvent& event, const KAAlarm* alarm, int flags)
{
	QString cmd;
	QString tmpXtermFile;
	if (flags & ProcData::EXEC_IN_XTERM)
	{
		// Execute the command in a terminal window.
		cmd = Preferences::instance()->cmdXTermCommand();
		cmd.replace("%t", aboutData()->programName());     // set the terminal window title
		if (cmd.find("%C") >= 0)
		{
			// Execute the command from a temporary script file
			if (flags & ProcData::TEMP_FILE)
				cmd.replace("%C", command);    // the command is already calling a temporary file
			else
			{
				tmpXtermFile = createTempScriptFile(command, true, event, *alarm);
				if (tmpXtermFile.isEmpty())
					return 0;
				cmd.replace("%C", tmpXtermFile);    // %C indicates where to insert the command
			}
		}
		else if (cmd.find("%W") >= 0)
		{
			// Execute the command from a temporary script file,
			// with a sleep after the command is executed
			tmpXtermFile = createTempScriptFile(command + QString::fromLatin1("\nsleep 86400\n"), true, event, *alarm);
			if (tmpXtermFile.isEmpty())
				return 0;
			cmd.replace("%W", tmpXtermFile);    // %w indicates where to insert the command
		}
		else if (cmd.find("%w") >= 0)
		{
			// Append a sleep to the command.
			// Quote the command in case it contains characters such as [>|;].
			QString exec = KShellProcess::quote(command + QString::fromLatin1("; sleep 86400"));
			cmd.replace("%w", exec);    // %w indicates where to insert the command string
		}
		else
		{
			// Set the command to execute.
			// Put it in quotes in case it contains characters such as [>|;].
			QString exec = KShellProcess::quote(command);
			if (cmd.find("%c") >= 0)
				cmd.replace("%c", exec);    // %c indicates where to insert the command string
			else
				cmd.append(exec);           // otherwise, simply append the command string
		}
	}
	else
		cmd = command;
	ShellProcess* proc = new ShellProcess(cmd);
	connect(proc, SIGNAL(shellExited(ShellProcess*)), SLOT(slotCommandExited(ShellProcess*)));
	ProcData* pd = new ProcData(proc, new KAEvent(event), (alarm ? new KAAlarm(*alarm) : 0), flags);
	if (flags & ProcData::TEMP_FILE)
		pd->tempFiles += command;
	pd->tempFiles += tmpXtermFile;
	mCommandProcesses.append(pd);
	if (proc->start())
		return proc;

	// Error executing command - report it
	kdError(5950) << "KAlarmApp::doShellCommand(): command failed to start\n";
	commandErrorMsg(proc, event, alarm, flags);
	mCommandProcesses.removeRef(pd);
	return 0;
}

/******************************************************************************
* Create a temporary script file containing the specified command string.
* Reply = path of temporary file, or null string if error.
*/
QString KAlarmApp::createTempScriptFile(const QString& command, bool insertShell, const KAEvent& event, const KAAlarm& alarm)
{
	KTempFile tmpFile(QString::null, QString::null, 0700);
	tmpFile.setAutoDelete(false);     // don't delete file when it is destructed
	QTextStream* stream = tmpFile.textStream();
	if (!stream)
		kdError(5950) << "KAlarmApp::createTempScript(): Unable to create a temporary script file" << endl;
	else
	{
		if (insertShell)
			;
		*stream << command;
		tmpFile.close();
		if (tmpFile.status())
			kdError(5950) << "KAlarmApp::createTempScript(): Error " << tmpFile.status() << " writing to temporary script file" << endl;
		else
			return tmpFile.name();
	}

	QStringList errmsgs(i18n("Error creating temporary script file"));
	(new MessageWin(event, alarm.dateTime(), errmsgs))->show();
	return QString::null;
}

/******************************************************************************
* Called when a command alarm execution completes.
*/
void KAlarmApp::slotCommandExited(ShellProcess* proc)
{
	kdDebug(5950) << "KAlarmApp::slotCommandExited()\n";
	// Find this command in the command list
	for (ProcData* pd = mCommandProcesses.first();  pd;  pd = mCommandProcesses.next())
	{
		if (pd->process == proc)
		{
			// Found the command. Check its exit status.
			if (!proc->normalExit())
			{
				QString errmsg = proc->errorMessage();
				kdWarning(5950) << "KAlarmApp::slotCommandExited(" << pd->event->cleanText() << "): " << errmsg << endl;
				if (pd->messageBoxParent)
				{
					// Close the existing informational KMessageBox for this process
					QObjectList* dialogs = pd->messageBoxParent->queryList("KDialogBase", 0, false, true);
					KDialogBase* dialog = (KDialogBase*)dialogs->getFirst();
					delete dialog;
					delete dialogs;
					if (!pd->tempFile())
					{
						errmsg += "\n";
						errmsg += proc->command();
					}
					KMessageBox::error(pd->messageBoxParent, errmsg);
				}
				else
					commandErrorMsg(proc, *pd->event, pd->alarm, pd->flags);
			}
			if (pd->preAction())
				execAlarm(*pd->event, *pd->alarm, pd->reschedule(), pd->allowDefer(), true);
			mCommandProcesses.remove();
		}
	}

	// Now that there are no executing shell commands, quit if a quit was queued
	if (mPendingQuit  &&  !mCommandProcesses.count())
		quitIf(mPendingQuitCode);
}

/******************************************************************************
* Output an error message for a shell command.
*/
void KAlarmApp::commandErrorMsg(const ShellProcess* proc, const KAEvent& event, const KAAlarm* alarm, int flags)
{
	QStringList errmsgs;
	if (flags & ProcData::PRE_ACTION)
		errmsgs += i18n("Pre-alarm action:");
	else if (flags & ProcData::POST_ACTION)
		errmsgs += i18n("Post-alarm action:");
	errmsgs += proc->errorMessage();
	if (!(flags & ProcData::TEMP_FILE))
		errmsgs += proc->command();
	(new MessageWin(event, (alarm ? alarm->dateTime() : DateTime()), errmsgs))->show();
}

/******************************************************************************
* Notes that an informational KMessageBox is displayed for this process.
*/
void KAlarmApp::commandMessage(ShellProcess* proc, QWidget* parent)
{
	// Find this command in the command list
	for (ProcData* pd = mCommandProcesses.first();  pd;  pd = mCommandProcesses.next())
	{
		if (pd->process == proc)
			pd->messageBoxParent = parent;
	}
}

/******************************************************************************
* Set up remaining DCOP handlers and start processing DCOP calls.
*/
void KAlarmApp::setUpDcop()
{
	if (!mInitialised)
	{
		mInitialised = true;      // we're now ready to handle DCOP calls
		Daemon::createDcopHandler();
		QTimer::singleShot(0, this, SLOT(processQueue()));    // process anything already queued
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
		setUpDcop();      // start processing DCOP calls
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
static bool convInterval(QCString timeParam, KAEvent::RecurType& recurType, int& timeInterval, bool allowMonthYear)
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
			if (!allowMonthYear)
				ok = false;
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
			{
				if (!allowMonthYear)
					ok = false;
				recurType = KAEvent::MONTHLY_DAY;
				timeParam = timeParam.left(length - 1);
			}
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
	while (tempFiles.count())
	{
		// Delete the temporary file called by the XTerm command
		QFile f(tempFiles.first());
		f.remove();
		tempFiles.remove(tempFiles.begin());
	}
	delete process;
	delete event;
	delete alarm;
}
