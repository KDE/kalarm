/*
 *  kalarmapp.cpp  -  the KAlarm application object
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

#include "kalarm.h"
#include "kalarmapp.moc"

#include "alarmcalendar.h"
#include "eventlistmodel.h"
#include "alarmlistview.h"
#include "editdlg.h"
#include "daemon.h"
#include "dbushandler.h"
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

#include <stdlib.h>
#include <ctype.h>
#include <iostream>

#include <QObject>
#include <QTimer>
#include <QRegExp>
#include <QFile>
#include <QByteArray>
#include <QTextStream>

#include <kcmdlineargs.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <ktemporaryfile.h>
#include <kfileitem.h>
#include <kglobal.h>
#include <kstandardguiitem.h>
#include <kservicetypetrader.h>
#include <netwm.h>
#include <kdebug.h>
#include <kshell.h>

static bool convWakeTime(const QByteArray& timeParam, KDateTime&, const KDateTime& defaultDt = KDateTime());
static bool convInterval(const QByteArray& timeParam, KARecurrence::Type&, int& timeInterval, bool allowMonthYear = true);

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
	  mDBusHandler(new DBusHandler()),
	  mTrayWindow(0),
	  mArchivedPurgeDays(-1),      // default to not purging
	  mPurgeDaysQueued(-1),
	  mPendingQuit(false),
	  mProcessingQueue(false),
	  mSessionClosingDown(false),
	  mSpeechEnabled(false)
{
	Preferences::self()->readConfig();
	Preferences::connect(SIGNAL(preferencesChanged()), this, SLOT(slotPreferencesChanged()));
	Preferences::connect(SIGNAL(archivedKeepDaysChanged(int)), this, SLOT(setArchivePurgeDays()));
	KARecurrence::setDefaultFeb29Type(Preferences::defaultFeb29Type());

	if (AlarmCalendar::initialiseCalendars())
	{
		KConfigGroup config(KGlobal::config(), "General");
		mNoSystemTray           = config.readEntry("NoSystemTray", false);
		mOldRunInSystemTray     = wantRunInSystemTray();
		mDisableAlarmsIfStopped = mOldRunInSystemTray && !mNoSystemTray && Preferences::disableAlarmsIfStopped();
		mStartOfDay             = Preferences::startOfDay();
		if (Preferences::hasStartOfDayChanged())
			mStartOfDay.setHMS(100,0,0);    // start of day time has changed: flag it as invalid
		DateTime::setStartOfDay(mStartOfDay);
		mPrefsArchivedColour   = Preferences::archivedColour();
		mPrefsShowTime         = Preferences::showAlarmTime();
		mPrefsShowTimeTo       = Preferences::showTimeToAlarm();
	}

	// Check if the speech synthesis daemon is installed
	mSpeechEnabled = (KServiceTypeTrader::self()->query("DBUS/Text-to-Speech", "Name == 'KTTSD'").count() > 0);
	if (!mSpeechEnabled)
		kDebug(5950) << "KAlarmApp::KAlarmApp(): speech synthesis disabled (KTTSD not found)";
	// Check if KOrganizer is installed
	QString korg = QLatin1String("korganizer");
	mKOrganizerEnabled = !KStandardDirs::locate("exe", korg).isNull()  ||  !KStandardDirs::findExe(korg).isNull();
	if (!mKOrganizerEnabled)
		kDebug(5950) << "KAlarmApp::KAlarmApp(): KOrganizer options disabled (KOrganizer not found)";
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
		else
		{
			// This is here instead of in the constructor to avoid recursion
			Daemon::initialise();    // calendars must be initialised before calling this
			Daemon::connectRegistered(AlarmCalendar::resources(), SLOT(slotDaemonRegistered(bool)));
		}
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
	kDebug(5950) << "KAlarmApp::restoreSession(): Restoring";
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
	initCheck();           // register with the alarm daemon

	// Try to display the system tray icon if it is configured to be autostarted,
	// or if we're in run-in-system-tray mode.
	if (Preferences::autostartTrayIcon()
	||  MainWindow::count()  &&  wantRunInSystemTray())
	{
		displayTrayIcon(true, trayParent);
		// Occasionally for no obvious reason, the main main window is
		// shown when it should be hidden, so hide it just to be sure.
		if (trayParent)
			trayParent->hide();
	}

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
	kDebug(5950) << "KAlarmApp::newInstance()";
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
				kDebug(5950) << "KAlarmApp::newInstance(): stop";
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
				kDebug(5950) << "KAlarmApp::newInstance(): reset";
				args->clear();         // free up memory
				Daemon::reset();
				dontRedisplay = true;  // exit program if no other instances running
			}
			else
			if (args->isSet("tray"))
			{
				// Display only the system tray icon
				kDebug(5950) << "KAlarmApp::newInstance(): tray";
				args->clear();      // free up memory
				if (!KSystemTrayIcon::isSystemTrayAvailable())
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
			if (args->isSet("handleEvent")  ||  args->isSet("triggerEvent")  ||  args->isSet("cancelEvent"))
			{
				// Display or delete the event with the specified event ID
				kDebug(5950) << "KAlarmApp::newInstance(): handle event";
				EventFunc function = EVENT_HANDLE;
				int count = 0;
				const char* option = 0;
				if (args->isSet("handleEvent"))   { function = EVENT_HANDLE;   option = "handleEvent";   ++count; }
				if (args->isSet("triggerEvent"))  { function = EVENT_TRIGGER;  option = "triggerEvent";  ++count; }
				if (args->isSet("cancelEvent"))   { function = EVENT_CANCEL;   option = "cancelEvent";   ++count; }
				if (count > 1)
					USAGE(i18nc("@info:shell", "<icode>%1</icode>, <icode>%2</icode>, <icode>%3</icode> mutually exclusive", QLatin1String("--handleEvent"), QLatin1String("--triggerEvent"), QLatin1String("--cancelEvent")));
				if (!initCheck(true))   // open the calendar, don't register with daemon yet
				{
					exitCode = 1;
					break;
				}
				QString eventID = args->getOption(option);
				args->clear();      // free up memory
				if (eventID.startsWith(QLatin1String("ad:")))
				{
					// It's a notification from the alarm daemon
					eventID = eventID.mid(3);
					Daemon::queueEvent(eventID);
				}
				setUpDcop();        // start processing DCOP calls
				if (!handleEvent(eventID, function))
				{
					exitCode = 1;
					break;
				}
			}
			else
			if (args->isSet("edit"))
			{
				QString eventID = args->getOption("edit");
				if (!initCheck())
				{
					exitCode = 1;
					break;
				}
				if (!KAlarm::editAlarm(eventID))
				{
					USAGE(i18nc("@info:shell", "<icode>%1</icode>: Event <resource>%2</resource> not found, or not editable", QString::fromLatin1("--edit"), eventID))
					exitCode = 1;
					break;
				}
			}
			else
			if (args->isSet("edit-new")  ||  args->isSet("edit-new-preset"))
			{
				QString templ;
				if (args->isSet("edit-new-preset"))
					templ = args->getOption("edit-new-preset");
				if (!initCheck())
				{
					exitCode = 1;
					break;
				}
				KAlarm::editNewAlarm(templ);
			}
			else
			if (args->isSet("file")  ||  args->isSet("exec")  ||  args->isSet("mail")  ||  args->count())
			{
				// Display a message or file, execute a command, or send an email
				KAEvent::Action action = KAEvent::MESSAGE;
				QString          alMessage;
				QString          alFromID;
				EmailAddressList alAddresses;
				QStringList      alAttachments;
				QString          alSubject;
				if (args->isSet("file"))
				{
					kDebug(5950) << "KAlarmApp::newInstance(): file";
					if (args->isSet("exec"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", QLatin1String("--exec"), QLatin1String("--file")))
					if (args->isSet("mail"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", QLatin1String("--mail"), QLatin1String("--file")))
					if (args->count())
						USAGE(i18nc("@info:shell", "message incompatible with <icode>%1</icode>", QLatin1String("--file")))
					alMessage = args->getOption("file");
					action = KAEvent::FILE;
				}
				else if (args->isSet("exec"))
				{
					kDebug(5950) << "KAlarmApp::newInstance(): exec";
					if (args->isSet("mail"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", QLatin1String("--mail"), QLatin1String("--exec")))
					alMessage = args->getOption("exec");
					int n = args->count();
					for (int i = 0;  i < n;  ++i)
					{
						alMessage += ' ';
						alMessage += args->arg(i);
					}
					action = KAEvent::COMMAND;
				}
				else if (args->isSet("mail"))
				{
					kDebug(5950) << "KAlarmApp::newInstance(): mail";
					if (args->isSet("subject"))
						alSubject = args->getOption("subject");
					if (args->isSet("from-id"))
						alFromID = args->getOption("from-id");
					QStringList params = args->getOptionList("mail");
					for (QStringList::Iterator i = params.begin();  i != params.end();  ++i)
					{
						QString addr = *i;
						if (!KAMail::checkAddress(addr))
							USAGE(i18nc("@info:shell", "<icode>%1</icode>: invalid email address", QLatin1String("--mail")))
						alAddresses += KCal::Person(QString(), addr);
					}
					params = args->getOptionList("attach");
					for (QStringList::Iterator i = params.begin();  i != params.end();  ++i)
						alAttachments += *i;
					alMessage = args->arg(0);
					action = KAEvent::EMAIL;
				}
				else
				{
					kDebug(5950) << "KAlarmApp::newInstance(): message";
					alMessage = args->arg(0);
				}

				if (action != KAEvent::EMAIL)
				{
					if (args->isSet("subject"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode>", QLatin1String("--subject"), QLatin1String("--mail")))
					if (args->isSet("from-id"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode>", QLatin1String("--from-id"), QLatin1String("--mail")))
					if (args->isSet("attach"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode>", QLatin1String("--attach"), QLatin1String("--mail")))
					if (args->isSet("bcc"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode>", QLatin1String("--bcc"), QLatin1String("--mail")))
				}

				KDateTime alarmTime, endTime;
				QColor    bgColour = Preferences::defaultBgColour();
				QColor    fgColour = Preferences::defaultFgColour();
				KARecurrence recurrence;
				int       repeatCount    = 0;
				int       repeatInterval = 0;
				if (args->isSet("color"))
				{
					// Background colour is specified
					QString colourText = args->getOption("color");
					if (colourText[0] == '0' && colourText[1].toLower() == 'x')
						colourText.replace(0, 2, "#");
					bgColour.setNamedColor(colourText);
					if (!bgColour.isValid())
						USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter", QLatin1String("--color")))
				}
				if (args->isSet("colorfg"))
				{
					// Foreground colour is specified
					QString colourText = args->getOption("colorfg");
					if (colourText[0] == '0' && colourText[1].toLower() == 'x')
						colourText.replace(0, 2, "#");
					fgColour.setNamedColor(colourText);
					if (!fgColour.isValid())
						USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter", QLatin1String("--colorfg")))
				}

				if (args->isSet("time"))
				{
					QByteArray dateTime = args->getOption("time").toLocal8Bit();
					if (!convWakeTime(dateTime, alarmTime))
						USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter", QLatin1String("--time")))
				}
				else
					alarmTime = KDateTime::currentLocalDateTime();

				bool haveRecurrence = args->isSet("recurrence");
				if (haveRecurrence)
				{
					if (args->isSet("login"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", QLatin1String("--login"), QLatin1String("--recurrence")))
					if (args->isSet("until"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", QLatin1String("--until"), QLatin1String("--recurrence")))
					QString rule = args->getOption("recurrence");
					recurrence.set(rule);
				}
				if (args->isSet("interval"))
				{
					// Repeat count is specified
					int count;
					if (args->isSet("login"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", QLatin1String("--login"), QLatin1String("--interval")))
					bool ok;
					if (args->isSet("repeat"))
					{
						count = args->getOption("repeat").toInt(&ok);
						if (!ok || !count || count < -1 || (count < 0 && haveRecurrence))
							USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter", QLatin1String("--repeat")))
					}
					else if (haveRecurrence)
						USAGE(i18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode>", QLatin1String("--interval"), QLatin1String("--repeat")))
					else if (args->isSet("until"))
					{
						count = 0;
						QByteArray dateTime = args->getOption("until").toLocal8Bit();
						bool ok;
						if (args->isSet("time"))
							ok = convWakeTime(dateTime, endTime, alarmTime);
						else
							ok = convWakeTime(dateTime, endTime);
						if (!ok)
							USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter", QLatin1String("--until")))
						if (alarmTime.isDateOnly()  &&  !endTime.isDateOnly())
							USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter for date-only alarm", QLatin1String("--until")))
						if (!alarmTime.isDateOnly()  &&  endTime.isDateOnly())
							endTime.setTime(QTime(23,59,59));
						if (endTime < alarmTime)
							USAGE(i18nc("@info:shell", "<icode>%1</icode> earlier than <icode>%2</icode>", QLatin1String("--until"), QLatin1String("--time")))
					}
					else
						count = -1;

					// Get the recurrence interval
					int interval;
					KARecurrence::Type recurType;
					if (!convInterval(args->getOption("interval").toLocal8Bit(), recurType, interval, !haveRecurrence)
					||  interval < 0)
						USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter", QLatin1String("--interval")))
					if (alarmTime.isDateOnly()  &&  recurType == KARecurrence::MINUTELY)
						USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter for date-only alarm", QLatin1String("--interval")))

					if (haveRecurrence)
					{
						// There is a also a recurrence specified, so set up a sub-repetition
						int longestInterval = recurrence.longestInterval();
						if (count * interval > longestInterval)
							USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> and <icode>%2</icode> parameters: repetition is longer than <icode>%3</icode> interval", QLatin1String("--interval"), QLatin1String("--repeat"), QLatin1String("--recurrence")));
						repeatCount    = count;
						repeatInterval = interval;
					}
					else
					{
						// There is no other recurrence specified, so convert the repetition
						// parameters into a KCal::Recurrence
						recurrence.set(recurType, interval, count, alarmTime, endTime);
					}
				}
				else
				{
					if (args->isSet("repeat"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode>", QLatin1String("--repeat"), QLatin1String("--interval")))
					if (args->isSet("until"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode>", QLatin1String("--until"), QLatin1String("--interval")))
				}

				QString    audioFile;
				float      audioVolume = -1;
				bool       audioRepeat = args->isSet("play-repeat");
				if (audioRepeat  ||  args->isSet("play"))
				{
					// Play a sound with the alarm
					if (audioRepeat  &&  args->isSet("play"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", QLatin1String("--play"), QLatin1String("--play-repeat")))
					if (args->isSet("beep"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", QLatin1String("--beep"), QLatin1String(audioRepeat ? "--play-repeat" : "--play")))
					if (args->isSet("speak"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", QLatin1String("--speak"), QLatin1String(audioRepeat ? "--play-repeat" : "--play")))
					audioFile = args->getOption(audioRepeat ? "play-repeat" : "play");
					if (args->isSet("volume"))
					{
						bool ok;
						int volumepc = args->getOption("volume").toInt(&ok);
						if (!ok  ||  volumepc < 0  ||  volumepc > 100)
							USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter", QLatin1String("--volume")))
						audioVolume = static_cast<float>(volumepc) / 100;
					}
				}
				else if (args->isSet("volume"))
					USAGE(i18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode> or <icode>%3</icode>", QLatin1String("--volume"), QLatin1String("--play"), QLatin1String("--play-repeat")))
				if (args->isSet("speak"))
				{
					if (args->isSet("beep"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", QLatin1String("--beep"), QLatin1String("--speak")))
					if (!mSpeechEnabled)
						USAGE(i18nc("@info:shell", "<icode>%1</icode> requires speech synthesis to be configured using KTTSD", QLatin1String("--speak")))
				}
				int reminderMinutes = 0;
				bool onceOnly = args->isSet("reminder-once");
				if (args->isSet("reminder")  ||  onceOnly)
				{
					// Issue a reminder alarm in advance of the main alarm
					if (onceOnly  &&  args->isSet("reminder"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", QLatin1String("--reminder"), QLatin1String("--reminder-once")))
					QString opt = onceOnly ? QLatin1String("--reminder-once") : QLatin1String("--reminder");
					if (args->isSet("exec"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", opt, QLatin1String("--exec")))
					if (args->isSet("mail"))
						USAGE(i18nc("@info:shell", "<icode>%1</icode> incompatible with <icode>%2</icode>", opt, QLatin1String("--mail")))
					KARecurrence::Type recurType;
					QString optval = args->getOption(onceOnly ? "reminder-once" : "reminder");
					bool ok = convInterval(args->getOption(onceOnly ? "reminder-once" : "reminder").toLocal8Bit(), recurType, reminderMinutes);
					if (ok)
					{
						switch (recurType)
						{
							case KARecurrence::MINUTELY:
								if (alarmTime.isDateOnly())
									USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter for date-only alarm", opt))
								break;
							case KARecurrence::DAILY:     reminderMinutes *= 1440;  break;
							case KARecurrence::WEEKLY:    reminderMinutes *= 7*1440;  break;
							default:   ok = false;  break;
						}
					}
					if (!ok)
						USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter", opt))
				}

				int lateCancel = 0;
				if (args->isSet("late-cancel"))
				{
					KARecurrence::Type recurType;
					bool ok = convInterval(args->getOption("late-cancel").toLocal8Bit(), recurType, lateCancel, false);
					if (!ok  ||  lateCancel <= 0)
						USAGE(i18nc("@info:shell", "Invalid <icode>%1</icode> parameter", QLatin1String("late-cancel")))
				}
				else if (args->isSet("auto-close"))
					USAGE(i18nc("@info:shell", "<icode>%1</icode> requires <icode>%2</icode>", QLatin1String("--auto-close"), QLatin1String("--late-cancel")))

				int flags = KAEvent::DEFAULT_FONT;
				if (args->isSet("ack-confirm"))
					flags |= KAEvent::CONFIRM_ACK;
				if (args->isSet("auto-close"))
					flags |= KAEvent::AUTO_CLOSE;
				if (args->isSet("beep"))
					flags |= KAEvent::BEEP;
				if (args->isSet("speak"))
					flags |= KAEvent::SPEAK;
				if (args->isSet("korganizer"))
					flags |= KAEvent::COPY_KORGANIZER;
				if (args->isSet("disable"))
					flags |= KAEvent::DISABLED;
				if (audioRepeat)
					flags |= KAEvent::REPEAT_SOUND;
				if (args->isSet("login"))
					flags |= KAEvent::REPEAT_AT_LOGIN;
				if (args->isSet("bcc"))
					flags |= KAEvent::EMAIL_BCC;
				if (alarmTime.isDateOnly())
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
				kDebug(5950) << "KAlarmApp::newInstance(): interactive";
				if (args->isSet("ack-confirm"))
					usage += QLatin1String("--ack-confirm ");
				if (args->isSet("attach"))
					usage += QLatin1String("--attach ");
				if (args->isSet("auto-close"))
					usage += QLatin1String("--auto-close ");
				if (args->isSet("bcc"))
					usage += QLatin1String("--bcc ");
				if (args->isSet("beep"))
					usage += QLatin1String("--beep ");
				if (args->isSet("color"))
					usage += QLatin1String("--color ");
				if (args->isSet("colorfg"))
					usage += QLatin1String("--colorfg ");
				if (args->isSet("disable"))
					usage += QLatin1String("--disable ");
				if (args->isSet("from-id"))
					usage += QLatin1String("--from-id ");
				if (args->isSet("korganizer"))
					usage += QLatin1String("--korganizer ");
				if (args->isSet("late-cancel"))
					usage += QLatin1String("--late-cancel ");
				if (args->isSet("login"))
					usage += QLatin1String("--login ");
				if (args->isSet("play"))
					usage += QLatin1String("--play ");
				if (args->isSet("play-repeat"))
					usage += QLatin1String("--play-repeat ");
				if (args->isSet("reminder"))
					usage += QLatin1String("--reminder ");
				if (args->isSet("reminder-once"))
					usage += QLatin1String("--reminder-once ");
				if (args->isSet("speak"))
					usage += QLatin1String("--speak ");
				if (args->isSet("subject"))
					usage += QLatin1String("--subject ");
				if (args->isSet("time"))
					usage += QLatin1String("--time ");
				if (args->isSet("volume"))
					usage += QLatin1String("--volume ");
				if (!usage.isEmpty())
				{
					usage += i18nc("@info:shell", ": option(s) only valid with a message/<icode>%1</icode>/<icode>%2</icode>", QLatin1String("--file"), QLatin1String("--exec"));
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
			std::cerr << usage.toLocal8Bit().data()
			          << i18nc("@info:shell", "\nUse --help to get a list of available command line options.\n").toLocal8Bit().data();
			exitCode = 1;
		}
	}
	if (firstInstance  &&  !dontRedisplay  &&  !exitCode)
		MessageWin::redisplayAlarms();

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
		if (!mDcopQueue.isEmpty()  ||  !mCommandProcesses.isEmpty())
		{
			// Don't quit yet if there are outstanding actions on the DCOP queue
			mPendingQuit = true;
			mPendingQuitCode = exitCode;
			return;
		}
	}

	// This was the last/only running "instance" of the program, so exit completely.
	kDebug(5950) << "KAlarmApp::quitIf(" << exitCode << "): quitting";
	exit(exitCode);
}

/******************************************************************************
* Called when the Quit menu item is selected.
* Closes the system tray window and all main windows, but does not exit the
* program if other windows are still open.
*/
void KAlarmApp::doQuit(QWidget* parent)
{
	kDebug(5950) << "KAlarmApp::doQuit()";
	if (mDisableAlarmsIfStopped
	&&  MessageBox::warningContinueCancel(parent, KMessageBox::Cancel,
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
		kDebug(5950) << "KAlarmApp::processQueue()";
		mProcessingQueue = true;

		// Reset the alarm daemon if it's been queued
		KAlarm::resetDaemonIfQueued();

		// Process DCOP calls
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
			quitIf(mPendingQuitCode);

		mProcessingQueue = false;
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
			if (!KSystemTrayIcon::isSystemTrayAvailable())
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
*  Check whether the system tray icon has been housed in the system tray.
*  If the system tray doesn't exist, tell the alarm daemon to notify us of
*  alarms regardless of whether we're running.
*/
bool KAlarmApp::checkSystemTray()
{
	if (!mTrayWindow)
		return true;
	if (KSystemTrayIcon::isSystemTrayAvailable() == mNoSystemTray)
	{
		kDebug(5950) << "KAlarmApp::checkSystemTray(): changed ->" << mNoSystemTray;
		mNoSystemTray = !mNoSystemTray;

		// Store the new setting in the config file, so that if KAlarm exits and is then
		// next activated by the daemon to display a message, it will register with the
		// daemon with the correct NOTIFY type. If that happened when there was no system
		// tray and alarms are disabled when KAlarm is not running, registering with
		// NO_START_NOTIFY could result in alarms never being seen.
		KConfigGroup config(KGlobal::config(), "General");
		config.writeEntry("NoSystemTray", mNoSystemTray);
		config.sync();

		// Update other settings and reregister with the alarm daemon
		slotPreferencesChanged();
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

	bool newDisableIfStopped = wantRunInSystemTray() && !mNoSystemTray && Preferences::disableAlarmsIfStopped();
	if (newDisableIfStopped != mDisableAlarmsIfStopped)
	{
		mDisableAlarmsIfStopped = newDisableIfStopped;    // N.B. this setting is used by Daemon::reregister()
		Preferences::setQuitWarn(true);   // since mode has changed, re-allow warning messages on Quit
		Daemon::reregister();           // re-register with the alarm daemon
	}

	// Change alarm times for date-only alarms if the start of day time has changed
	if (Preferences::startOfDay() != mStartOfDay)
		changeStartOfDay();

	// In case the date for February 29th recurrences has changed
	KARecurrence::setDefaultFeb29Type(Preferences::defaultFeb29Type());

	if (Preferences::showAlarmTime()   != mPrefsShowTime
	||  Preferences::showTimeToAlarm() != mPrefsShowTimeTo)
	{
		// The default alarm list time columns selection has changed
		MainWindow::updateTimeColumns(mPrefsShowTime, mPrefsShowTimeTo);
		mPrefsShowTime   = Preferences::showAlarmTime();
		mPrefsShowTimeTo = Preferences::showTimeToAlarm();
	}
}

/******************************************************************************
*  Change alarm times for date-only alarms after the start of day time has changed.
*/
void KAlarmApp::changeStartOfDay()
{
	QTime sod = Preferences::startOfDay();
	DateTime::setStartOfDay(sod);
	AlarmCalendar* cal = AlarmCalendar::resources();
	if (KAEvent::adjustStartOfDay(cal->events(KCalEvent::ACTIVE)))
		cal->save();
	Preferences::updateStartOfDayCheck(sod);  // now that calendar is updated, set OK flag in config file
	mStartOfDay = sod;
}

/******************************************************************************
* Return whether the program is configured to be running in the system tray.
*/
bool KAlarmApp::wantRunInSystemTray() const
{
	return Preferences::runInSystemTray()  &&  KSystemTrayIcon::isSystemTrayAvailable();
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
* Called to schedule a new alarm, either in response to a DCOP notification or
* to command line options.
* Reply = true unless there was a parameter error or an error opening calendar file.
*/
bool KAlarmApp::scheduleEvent(KAEvent::Action action, const QString& text, const KDateTime& dateTime,
                              int lateCancel, int flags, const QColor& bg, const QColor& fg, const QFont& font,
                              const QString& audioFile, float audioVolume, int reminderMinutes,
                              const KARecurrence& recurrence, int repeatInterval, int repeatCount,
                              const QString& mailFromID, const EmailAddressList& mailAddresses,
                              const QString& mailSubject, const QStringList& mailAttachments)
{
	kDebug(5950) << "KAlarmApp::scheduleEvent():" << text;
	if (!dateTime.isValid())
		return false;
	KDateTime now = KDateTime::currentUtcDateTime();
	if (lateCancel  &&  dateTime < now.addSecs(-maxLateness(lateCancel)))
		return true;               // alarm time was already archived too long ago
	KDateTime alarmTime = dateTime;
	// Round down to the nearest minute to avoid scheduling being messed up
	if (!dateTime.isDateOnly())
		alarmTime.setTime(QTime(alarmTime.time().hour(), alarmTime.time().minute(), 0));

	KAEvent event(alarmTime, text, bg, fg, font, action, lateCancel, flags);
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
		||  event.setNextOccurrence(now) == KAEvent::NO_OCCURRENCE)
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
* Called in response to a DCOP notification by the alarm daemon that an event
* should be handled, i.e. displayed or cancelled.
* Optionally display the event. Delete the event from the calendar file and
* from every main window instance.
*/
bool KAlarmApp::dbusHandleEvent(const QString& eventID, EventFunc function)
{
	kDebug(5950) << "KAlarmApp::dbusHandleEvent(" << eventID << ")";
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
	kDebug(5950) << "KAlarmApp::handleEvent():" << eventID << "," << (function==EVENT_TRIGGER?"TRIGGER":function==EVENT_CANCEL?"CANCEL":function==EVENT_HANDLE?"HANDLE":"?");
	KCal::Event* kcalEvent = AlarmCalendar::resources()->event(eventID);
	if (!kcalEvent)
	{
		kWarning(5950) << "KAlarmApp::handleEvent(): event ID not found:" << eventID;
		Daemon::eventHandled(eventID);
		return false;
	}
	KAEvent event(kcalEvent);
	switch (function)
	{
		case EVENT_CANCEL:
			KAlarm::deleteEvent(event, true);
			break;

		case EVENT_TRIGGER:    // handle it if it's due, else execute it regardless
		case EVENT_HANDLE:     // handle it if it's due
		{
			KDateTime now = KDateTime::currentUtcDateTime();
			DateTime  repeatDT;
			bool updateCalAndDisplay = false;
			bool alarmToExecuteValid = false;
			KAAlarm alarmToExecute;
			// Check all the alarms in turn.
			// Note that the main alarm is fetched before any other alarms.
			for (KAAlarm alarm = event.firstAlarm();  alarm.valid();  alarm = event.nextAlarm(alarm))
			{
				if (alarm.deferred()  &&  event.repeatCount()
				&&  repeatDT.isValid()  &&  alarm.dateTime() > repeatDT)
				{
					// This deferral of a repeated alarm is later than the last previous
					// occurrence of the main alarm, so use the deferral alarm instead.
					// If the deferral is not yet due, this prevents the main alarm being
					// triggered repeatedly. If the deferral is due, this triggers it
					// in preference to the main alarm.
					alarmToExecute      = KAAlarm();
					alarmToExecuteValid = false;
					updateCalAndDisplay = false;
				}
				// Check if the alarm is due yet.
				KDateTime nextDT = alarm.dateTime(true).kDateTime();
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
						kDebug(5950) << "KAlarmApp::handleEvent(): alarm" << alarm.type() << ": not due";
						continue;
					}
				}
				bool reschedule = false;
				if (event.workTimeOnly()  &&  !alarm.deferred())
				{
					// The alarm is restricted to working hours (apart from reminders and
					// deferrals). This needs to be re-evaluated every time it triggers,
					// since working hours could change.
					reschedule = !KAlarm::isWorkingTime(nextDT);
					if (reschedule)
						kDebug(5950) << "KAlarmApp::handleEvent(): not during working hours";
				}
				if (!reschedule  &&  alarm.repeatAtLogin())
				{
					// Alarm is to be displayed at every login.
					// Check if the alarm has only just been set up.
					// (The alarm daemon will immediately notify that it is due
					//  since it is set up with a time in the past.)
					kDebug(5950) << "KAlarmApp::handleEvent(): REPEAT_AT_LOGIN";
					if (secs < maxLateness(1))
						continue;

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
					kDebug(5950) << "KAlarmApp::handleEvent(): LATE_CANCEL";
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
							KAEvent::OccurType type = event.previousOccurrence(now, next, true);
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
										||  type == KAEvent::FIRST_OR_ONLY_OCCURRENCE && !event.recurs())
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
						int maxlate = maxLateness(alarm.lateCancel());
						if (secs > maxlate)
						{
							// It's over the maximum interval late.
							// Find the most recent occurrence of the alarm.
							DateTime next;
							KAEvent::OccurType type = event.previousOccurrence(now, next, true);
							switch (type & ~KAEvent::OCCURRENCE_REPEAT)
							{
								case KAEvent::FIRST_OR_ONLY_OCCURRENCE:
								case KAEvent::RECURRENCE_DATE:
								case KAEvent::RECURRENCE_DATE_TIME:
								case KAEvent::LAST_RECURRENCE:
									if (next.effectiveKDateTime().secsTo(now) > maxlate)
									{
										if (type == KAEvent::LAST_RECURRENCE
										||  type == KAEvent::FIRST_OR_ONLY_OCCURRENCE && !event.recurs())
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
						cancelAlarm(event, alarm.type(), false);
						updateCalAndDisplay = true;
						continue;
					}
				}
				if (reschedule)
				{
					// The latest repetition was too long ago, so schedule the next one
					rescheduleAlarm(event, alarm, false);
					updateCalAndDisplay = true;
					continue;
				}
				if (!alarmToExecuteValid)
				{
					kDebug(5950) << "KAlarmApp::handleEvent(): alarm" << alarm.type() << ": execute";
					alarmToExecute = alarm;             // note the alarm to be displayed
					alarmToExecuteValid = true;         // only trigger one alarm for the event
				}
				else
					kDebug(5950) << "KAlarmApp::handleEvent(): alarm" << alarm.type() << ": skip";
			}

			// If there is an alarm to execute, do this last after rescheduling/cancelling
			// any others. This ensures that the updated event is only saved once to the calendar.
			if (alarmToExecute.valid())
				execAlarm(event, alarmToExecute, true, !alarmToExecute.repeatAtLogin());
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
					KAlarm::updateEvent(event);     // update the window lists and calendar file
				else if (function != EVENT_TRIGGER)
				{
					kDebug(5950) << "KAlarmApp::handleEvent(): no action";
					Daemon::eventHandled(eventID);
				}
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
	if (!event.postAction().isEmpty()  &&  ShellProcess::authorised())
	{
		QString command = event.postAction();
		kDebug(5950) << "KAlarmApp::alarmCompleted(" << event.id() << "):" << command;
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
	kDebug(5950) << "KAlarmApp::rescheduleAlarm()";
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
		// Reschedule the alarm for its next occurrence.
		KAEvent::OccurType type = event.setNextOccurrence(KDateTime::currentUtcDateTime());
		switch (type)
		{
			case KAEvent::NO_OCCURRENCE:
				// All repetitions are finished, so cancel the event
				cancelAlarm(event, alarm.type(), updateCalAndDisplay);
				break;
			default:
				if (!(type & KAEvent::OCCURRENCE_REPEAT))
					break;
				// Next occurrence is a repeat, so fall through to recurrence handling
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
			case KAEvent::FIRST_OR_ONLY_OCCURRENCE:
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
	if (update)
	{
		event.cancelCancelledDeferral();
		KAlarm::updateEvent(event);     // update the window lists and calendar file
	}
}

/******************************************************************************
* Delete the alarm. If it is the last alarm for its event, the event is removed
* from the calendar file and from every main window instance.
*/
void KAlarmApp::cancelAlarm(KAEvent& event, KAAlarm::Type alarmType, bool updateCalAndDisplay)
{
	kDebug(5950) << "KAlarmApp::cancelAlarm()";
	event.cancelCancelledDeferral();
	if (alarmType == KAAlarm::MAIN_ALARM  &&  !event.displaying()  &&  event.toBeArchived())
	{
		// The event is being deleted. Save it in the archived resources first.
		QString id = event.id();    // save event ID since KAlarm::addArchivedEvent() changes it
		KAlarm::addArchivedEvent(event);
		event.setEventID(id);       // restore event ID
	}
	event.removeExpiredAlarm(alarmType);
	if (!event.alarmCount())
		KAlarm::deleteEvent(event, false);
	else if (updateCalAndDisplay)
		KAlarm::updateEvent(event);    // update the window lists and calendar file
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
			// Find if we're changing a reminder message to the real message
			bool reminder = (alarm.type() & KAAlarm::REMINDER_ALARM);
			bool replaceReminder = !reminder && win && (win->alarmType() & KAAlarm::REMINDER_ALARM);
			if (!reminder  &&  !event.deferred()
			&&  (replaceReminder || !win)  &&  !noPreAction
			&&  !event.preAction().isEmpty()  &&  ShellProcess::authorised())
			{
				// It's not a reminder or a deferred alarm, and there is no message window
				// (other than a reminder window) currently displayed for this alarm,
				// and we need to execute a command before displaying the new window.
				// Check whether the command is already being executed for this alarm.
				for (int i = 0, end = mCommandProcesses.count();  i < end;  ++i)
				{
					ProcData* pd = mCommandProcesses[i];
					if (pd->event->id() == event.id()  &&  (pd->flags & ProcData::PRE_ACTION))
					{
						kDebug(5950) << "KAlarmApp::execAlarm(): already executing pre-DISPLAY command";
						return pd->process;   // already executing - don't duplicate the action
					}
				}

				QString command = event.preAction();
				kDebug(5950) << "KAlarmApp::execAlarm(): pre-DISPLAY command:" << command;
				int flags = (reschedule ? ProcData::RESCHEDULE : 0) | (allowDefer ? ProcData::ALLOW_DEFER : 0);
				if (doShellCommand(command, event, &alarm, (flags | ProcData::PRE_ACTION)))
					return result;     // display the message after the command completes
				// Error executing command - display the message even though it failed
			}
			if (!event.enabled())
				delete win;        // event is disabled - close its window
			else if (!win
			     ||  !win->hasDefer() && !alarm.repeatAtLogin()
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
				(new MessageWin(event, alarm, flags))->show();
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
				kDebug(5950) << "KAlarmApp::execAlarm(): COMMAND: (script)";
				QString tmpfile = createTempScriptFile(command, false, event, alarm);
				if (tmpfile.isEmpty())
					result = 0;
				else
					result = doShellCommand(tmpfile, event, &alarm, (flags | ProcData::TEMP_FILE));
			}
			else
			{
				kDebug(5950) << "KAlarmApp::execAlarm(): COMMAND:" << command;
				result = doShellCommand(command, event, &alarm, flags);
			}
			if (reschedule)
				rescheduleAlarm(event, alarm, true);
			break;
		}
		case KAAlarm::EMAIL:
		{
			kDebug(5950) << "KAlarmApp::execAlarm(): EMAIL to:" << event.emailAddresses(",");
			QStringList errmsgs;
			KAMail::JobData data(event, alarm, reschedule, (reschedule || allowDefer));
			int ans = KAMail::send(data, errmsgs);
			if (ans)
			{
				// The email has either been sent or failed - not queued
				if (ans < 0)
					result = 0;  // failure
				emailSent(data, errmsgs, (ans > 0));
			}
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
			kDebug(5950) << "KAlarmApp::execAlarm(): copy error:" << errmsgs[1];
		else
			kDebug(5950) << "KAlarmApp::execAlarm(): failed:" << errmsgs[1];
		MessageWin::showError(data.event, data.alarm.dateTime(), errmsgs);
	}
	if (data.reschedule)
		rescheduleAlarm(data.event, data.alarm, true);
}

/******************************************************************************
* Execute a shell command line specified by an alarm.
* If the PRE_ACTION bit of 'flags' is set, the alarm will be executed via
* execAlarm() once the command completes, the execAlarm() parameters being
* derived from the remaining bits in 'flags'.
*/
ShellProcess* KAlarmApp::doShellCommand(const QString& command, const KAEvent& event, const KAAlarm* alarm, int flags)
{
	kDebug(5950) << "KAlarmApp::doShellCommand(" << command << "," << event.id() << ")";
	QIODevice::OpenMode mode = QIODevice::WriteOnly;
	QString cmd;
	QString tmpXtermFile;
	if (flags & ProcData::EXEC_IN_XTERM)
	{
		// Execute the command in a terminal window.
		cmd = Preferences::cmdXTermCommand();
		cmd.replace("%t", KGlobal::mainComponent().aboutData()->programName());     // set the terminal window title
		if (cmd.indexOf("%C") >= 0)
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
		else if (cmd.indexOf("%W") >= 0)
		{
			// Execute the command from a temporary script file,
			// with a sleep after the command is executed
			tmpXtermFile = createTempScriptFile(command + QLatin1String("\nsleep 86400\n"), true, event, *alarm);
			if (tmpXtermFile.isEmpty())
				return 0;
			cmd.replace("%W", tmpXtermFile);    // %w indicates where to insert the command
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
	}
	else
	{
		cmd = command;
		mode = QIODevice::ReadWrite;
	}
	ShellProcess* proc = new ShellProcess(cmd);
	proc->setOutputChannelMode(KProcess::MergedChannels);   // combine stdout & stderr
	connect(proc, SIGNAL(shellExited(ShellProcess*)), SLOT(slotCommandExited(ShellProcess*)));
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
		QFile logfile(QFile::encodeName(event.logFile()));
		if (logfile.open(QIODevice::Append | QIODevice::Text))
		{
			QTextStream out(&logfile);
			out << heading;
			logfile.close();
		}
		proc->setStandardOutputFile(event.logFile(), QIODevice::Append);
	}
	ProcData* pd = new ProcData(proc, new KAEvent(event), (alarm ? new KAAlarm(*alarm) : 0), flags);
	if (flags & ProcData::TEMP_FILE)
		pd->tempFiles += command;
	if (!tmpXtermFile.isEmpty())
		pd->tempFiles += tmpXtermFile;
	mCommandProcesses.append(pd);
	if (proc->start(mode))
		return proc;

	// Error executing command - report it
	kError(5950) << "KAlarmApp::doShellCommand(): command failed to start";
	commandErrorMsg(proc, event, alarm, flags);
	mCommandProcesses.removeAt(mCommandProcesses.indexOf(pd));
	delete pd;
	return 0;
}

/******************************************************************************
* Create a temporary script file containing the specified command string.
* Reply = path of temporary file, or null string if error.
*/
QString KAlarmApp::createTempScriptFile(const QString& command, bool insertShell, const KAEvent& event, const KAAlarm& alarm)
{
	KTemporaryFile tmpFile;
	tmpFile.setAutoRemove(false);     // don't delete file when it is destructed
	if (!tmpFile.open())
		kError(5950) << "KAlarmApp::createTempScript(): Unable to create a temporary script file";
	else
	{
		tmpFile.setPermissions(QFile::ReadUser | QFile::WriteUser | QFile::ExeUser);
		QTextStream stream(&tmpFile);
		if (insertShell)
			stream << "#!" << ShellProcess::shellPath() << "\n";
		stream << command;
		stream.flush();
		if (tmpFile.error() != QFile::NoError)
			kError(5950) << "KAlarmApp::createTempScript(): Error" << tmpFile.errorString() << " writing to temporary script file";
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
	kDebug(5950) << "KAlarmApp::slotCommandExited()";
	// Find this command in the command list
	for (int i = 0, end = mCommandProcesses.count();  i < end;  ++i)
	{
		ProcData* pd = mCommandProcesses[i];
		if (pd->process == proc)
		{
			// Found the command. Check its exit status.
			if (!proc->exitStatus() == QProcess::NormalExit)
			{
				QString errmsg = proc->errorMessage();
				kWarning(5950) << "KAlarmApp::slotCommandExited(" << pd->event->cleanText() << "):" << errmsg;
				if (pd->messageBoxParent)
				{
					// Close the existing informational KMessageBox for this process
					QList<KDialog*> dialogs = pd->messageBoxParent->findChildren<KDialog*>();
					if (!dialogs.isEmpty())
					    delete dialogs[0];
					if (!pd->tempFile())
					{
						errmsg += '\n';
						errmsg += proc->command();
					}
					KMessageBox::error(pd->messageBoxParent, errmsg);
				}
				else
					commandErrorMsg(proc, *pd->event, pd->alarm, pd->flags);
			}
			if (pd->preAction())
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
* Output an error message for a shell command.
*/
void KAlarmApp::commandErrorMsg(const ShellProcess* proc, const KAEvent& event, const KAAlarm* alarm, int flags)
{
	QStringList errmsgs;
	QString dontShowAgain;
	if (flags & ProcData::PRE_ACTION)
	{
		errmsgs += i18nc("@info", "Pre-alarm action:");
		dontShowAgain = QLatin1String("Pre");
	}
	else if (flags & ProcData::POST_ACTION)
	{
		errmsgs += i18nc("@info", "Post-alarm action:");
		dontShowAgain = QLatin1String("Post");
	}
	else
		dontShowAgain = QLatin1String("Exec");
	errmsgs += proc->errorMessage();
	if (!(flags & ProcData::TEMP_FILE))
		errmsgs += proc->command();
	MessageWin::showError(event, (alarm ? alarm->dateTime() : DateTime()), errmsgs, dontShowAgain + QString::number(proc->status()));
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
	static bool firstTime = true;
	bool startdaemon;
	if (firstTime)
	{
		if (!mStartOfDay.isValid())
			changeStartOfDay();     // start of day time has changed, so adjust date-only alarms

		/* Need to open the display calendar now, since otherwise if the daemon
		 * immediately notifies display alarms, they will often be processed while
		 * MessageWin::redisplayAlarms() is executing open() (but before open()
		 * completes), which causes problems!!
		 */
		AlarmCalendar::displayCalendar()->open();

		AlarmCalendar::resources()->open();
		setArchivePurgeDays();

		startdaemon = true;
		firstTime = false;
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
*  Convert the --time parameter string into a local date/time or date value.
*  The parameter is in the form [[[yyyy-]mm-]dd-]hh:mm or yyyy-mm-dd.
*  Reply = true if successful.
*/
static bool convWakeTime(const QByteArray& timeParam, KDateTime& dateTime, const KDateTime& defaultDt)
{
#define MAX_DT_LEN 19
	int i = timeParam.indexOf(' ');
	if (i > MAX_DT_LEN)
		return false;
	QString zone = (i >= 0) ? QString::fromLatin1(timeParam.mid(i)) : QString();
	char timeStr[MAX_DT_LEN+1];
	strcpy(timeStr, timeParam.left(i >= 0 ? i : MAX_DT_LEN));
	int dt[5] = { -1, -1, -1, -1, -1 };
	char* s;
	char* end;
	bool noTime;
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
	bool noDate = true;
	if (s != timeStr)
	{
		noDate = false;
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
		if (dt[0] < 0  ||  !date.isValid())
			return false;
		dateTime = KAlarm::applyTimeZone(zone, date, time, false, defaultDt);
	}
	else
	{
		// Compile the values into a date/time structure
		time.setHMS(dt[3], dt[4], 0);
		if (dt[0] < 0)
		{
			// Some or all of the date was omitted.
			// Use the default date/time if provided.
			if (defaultDt.isValid())
			{
				dt[0] = defaultDt.date().year();
				date.setYMD(dt[0],
				            (dt[1] < 0 ? defaultDt.date().month() : dt[1]),
				            (dt[2] < 0 ? defaultDt.date().day() : dt[2]));
			}
			else
				date.setYMD(2000, 1, 1);  // temporary substitute for date
		}
		dateTime = KAlarm::applyTimeZone(zone, date, time, true, defaultDt);
		if (!dateTime.isValid())
			return false;
		if (dt[0] < 0)
		{
			// Some or all of the date was omitted.
			// Use the current date in the specified time zone as default.
			KDateTime now = KDateTime::currentDateTime(dateTime.timeSpec());
			date = dateTime.date();
			date.setYMD(now.date().year(),
			            (dt[1] < 0 ? now.date().month() : dt[1]),
			            (dt[2] < 0 ? now.date().day() : dt[2]));
			if (!date.isValid())
				return false;
			if (noDate  &&  time < now.time())
				date = date.addDays(1);
			dateTime.setDate(date);
		}
	}
	return dateTime.isValid();
}

/******************************************************************************
*  Convert a time interval command line parameter.
*  Reply = true if successful.
*/
static bool convInterval(const QByteArray& timeParam, KARecurrence::Type& recurType, int& timeInterval, bool allowMonthYear)
{
	QByteArray timeString = timeParam;
	// Get the recurrence interval
	bool ok = true;
	uint interval = 0;
	bool negative = (timeString[0] == '-');
	if (negative)
		timeString = timeString.right(1);
	uint length = timeString.length();
	switch (timeString[length - 1])
	{
		case 'Y':
			if (!allowMonthYear)
				ok = false;
			recurType = KARecurrence::ANNUAL_DATE;
			timeString = timeString.left(length - 1);
			break;
		case 'W':
			recurType = KARecurrence::WEEKLY;
			timeString = timeString.left(length - 1);
			break;
		case 'D':
			recurType = KARecurrence::DAILY;
			timeString = timeString.left(length - 1);
			break;
		case 'M':
		{
			int i = timeString.indexOf('H');
			if (i < 0)
			{
				if (!allowMonthYear)
					ok = false;
				recurType = KARecurrence::MONTHLY_DAY;
				timeString = timeString.left(length - 1);
			}
			else
			{
				recurType = KARecurrence::MINUTELY;
				interval = timeString.left(i).toUInt(&ok) * 60;
				timeString = timeString.mid(i + 1, length - i - 2);
			}
			break;
		}
		default:       // should be a digit
			recurType = KARecurrence::MINUTELY;
			break;
	}
	if (ok)
		interval += timeString.toUInt(&ok);
	timeInterval = static_cast<int>(interval);
	if (negative)
		timeInterval = -timeInterval;
	return ok;
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
