/*
 *  kalarmapp.cpp  -  the KAlarm application object
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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
#include <kstaticdeleter.h>
#include <kdebug.h>

#include <kalarmd/clientinfo.h>

#include <libkcal/icalformat.h>

#include "alarmcalendar.h"
#include "mainwindow.h"
#include "editdlg.h"
#include "messagewin.h"
#include "daemongui.h"
#include "dcophandler.h"
#include "traywindow.h"
#include "kamail.h"
#include "preferences.h"
#include "prefdlg.h"
#include "kalarmapp.moc"

#include <netwm.h>

extern QCString execArguments;

#define       DAEMON_APP_NAME_DEF    "kalarmd"
const char*   DCOP_OBJECT_NAME     = "display";
const char*   GUI_DCOP_OBJECT_NAME = "tray";
const char*   DAEMON_APP_NAME      = DAEMON_APP_NAME_DEF;
const char*   DAEMON_DCOP_OBJECT   = "ad";
const QString ACTIVE_CALENDAR(QString::fromLatin1("calendar.ics"));
const QString ARCHIVE_CALENDAR(QString::fromLatin1("expired.ics"));
const QString DISPLAY_CALENDAR(QString::fromLatin1("displaying.ics"));

int         marginKDE2 = 0;

static bool convWakeTime(const QCString timeParam, QDateTime&, bool& noTime);
static bool convInterval(QCString timeParam, KAlarmEvent::RecurType&, int& timeInterval);

static KStaticDeleter<AlarmCalendar> calendarDeleter;           // ensures destructor is called
static KStaticDeleter<AlarmCalendar> expiredCalendarDeleter;    // ensures destructor is called
static KStaticDeleter<AlarmCalendar> displayCalendarDeleter;    // ensures destructor is called

KAlarmApp*  KAlarmApp::theInstance = 0;
int         KAlarmApp::activeCount = 0;


/******************************************************************************
* Construct the application.
*/
KAlarmApp::KAlarmApp()
	: KUniqueApplication(),
	  mDcopHandler(0),
	  mDaemonGuiHandler(0),
	  mTrayWindow(0),
	  mDaemonCheckInterval(0),
	  mCalendarUpdateCount(0),
	  mDaemonRegistered(false),
	  mCheckingSystemTray(false),
	  mDaemonRunning(false),
	  mSessionClosingDown(false)
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
	KAlarmEvent::setFeb29RecurType();
	readDaemonCheckInterval();
	KConfig* config = kapp->config();
	config->setGroup(QString::fromLatin1("General"));

	/* Initialise the alarm calendars, and ensure that their file names are different.
	 * There are 3 calendars:
	 *  1) A user-independent one containing the active alarms;
	 *  2) A historical one containing expired alarms;
	 *  3) A user-specific one which contains details of alarms which are currently
	 *     being displayed to that user and which have not yet been acknowledged.
	 */
	QRegExp vcsRegExp ( QString::fromLatin1("\\.vcs$") );
	QString ical      = QString::fromLatin1(".ics");
	QString displayCal = locateLocal("appdata", DISPLAY_CALENDAR);
	QString activeKey = QString::fromLatin1("Calendar");
	QString activeCal = config->readPathEntry(activeKey, locateLocal("appdata", ACTIVE_CALENDAR));
	QString activeICal = activeCal;
	activeICal.replace(vcsRegExp, ical);
	if (activeICal == displayCal)
	{
		kdError(5950) << "KAlarmApp::KAlarmApp(): active calendar name = display calendar name\n";
		KMessageBox::error(0, i18n("%1: file name not permitted: %2").arg(activeKey).arg(activeCal), aboutData()->programName());
		exit(1);
	}
	mCalendar = calendarDeleter.setObject(
	                 new AlarmCalendar(activeCal, KAlarmEvent::ACTIVE, activeICal, activeKey));
	if (!mCalendar->valid())
	{
		QString path = mCalendar->path();
		kdError(5950) << "KAlarmApp::KAlarmApp(): invalid name: " << path << endl;
		KMessageBox::error(0, i18n("Invalid calendar file name: %1").arg(path), aboutData()->programName());
		exit(1);
	}
	connect(mCalendar, SIGNAL(calendarSaved(AlarmCalendar*)), SLOT(calendarSaved(AlarmCalendar*)));

	QString expiredKey = QString::fromLatin1("ExpiredCalendar");
	QString expiredCal = config->readPathEntry(expiredKey, locateLocal("appdata", ARCHIVE_CALENDAR));
	QString expiredICal = expiredCal;
	expiredICal.replace(vcsRegExp, ical);
	if (expiredICal == activeICal)
	{
		kdError(5950) << "KAlarmApp::KAlarmApp(): active calendar name = expired calendar name\n";
		KMessageBox::error(0, i18n("%1, %2: file names must be different").arg(activeKey).arg(expiredKey), aboutData()->programName());
		exit(1);
	}
	if (expiredICal == displayCal)
	{
		kdError(5950) << "KAlarmApp::KAlarmApp(): expired calendar name = display calendar name\n";
		KMessageBox::error(0, i18n("%1: file name not permitted: %2").arg(expiredKey).arg(expiredCal), aboutData()->programName());
		exit(1);
	}
	mExpiredCalendar = expiredCalendarDeleter.setObject(
	                        new AlarmCalendar(expiredCal, KAlarmEvent::EXPIRED, expiredICal, expiredKey));
	mDisplayCalendar = displayCalendarDeleter.setObject(
	                        new AlarmCalendar(displayCal, KAlarmEvent::DISPLAYING));

	// Check if it's a KDE desktop by comparing the window manager name to "KWin"
	NETRootInfo nri(qt_xdisplay(), NET::SupportingWMCheck);
	const char* wmname = nri.wmName();
	mKDEDesktop = wmname && !strcmp(wmname, "KWin");

	mNoSystemTray           = config->readBoolEntry(QString::fromLatin1("NoSystemTray"), false);
	mSavedNoSystemTray      = mNoSystemTray;
	mOldRunInSystemTray     = wantRunInSystemTray();
	mDisableAlarmsIfStopped = mOldRunInSystemTray && !mNoSystemTray && preferences->disableAlarmsIfStopped();
	mStartOfDay             = preferences->startOfDay();
	if (preferences->startOfDayChanged())
		mStartOfDay.setHMS(100,0,0);    // start of day time has changed: flag it as invalid
	mOldExpiredColour   = preferences->expiredColour();
	mOldExpiredKeepDays = preferences->expiredKeepDays();

	// Set up actions used by more than one menu
	KActionCollection* actions = new KActionCollection(this);
	mActionAlarmEnable   = new ActionAlarmsEnabled(Qt::CTRL+Qt::Key_E, this, SLOT(toggleAlarmsEnabled()),
	                                               actions, "alarmenable");
	mActionPrefs         = KStdAction::preferences(this, SLOT(slotPreferences()), actions);
#if KDE_VERSION >= 308
	mActionDaemonControl = new KAction(i18n("Control the Alarm Daemon", "Control Alarm &Daemon..."), mActionPrefs->iconSet(),
#else
	mActionDaemonControl = new KAction(i18n("Configure Alarm &Daemon..."), mActionPrefs->iconSet(),
#endif
	                                   0, this, SLOT(slotDaemonControl()), actions, "controldaemon");
	mActionNewAlarm      = createNewAlarmAction(i18n("&New Alarm..."), this, SLOT(slotNewAlarm()), actions);
}

/******************************************************************************
*/
KAlarmApp::~KAlarmApp()
{
	delete mCalendar;         mCalendar = 0;
	delete mExpiredCalendar;  mExpiredCalendar = 0;
	delete mDisplayCalendar;  mDisplayCalendar = 0;
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
	++activeCount;
	if (!initCheck(true))     // open the calendar file (needed for main windows)
	{
		--activeCount;
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

	--activeCount;
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
	++activeCount;
	int exitCode = 0;               // default = success
	static bool firstInstance = true;
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
				if (!mKDEDesktop)
				{
					exitCode = 1;
					break;
				}
				setUpDcop();        // we're now ready to handle DCOP calls, so set up handlers
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
					if (KURL(calendarUrl).url() != mCalendar->urlString())
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
				KAlarmEvent::Action action = KAlarmEvent::MESSAGE;
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
					action = KAlarmEvent::FILE;
				}
				else if (args->isSet("exec"))
				{
					kdDebug(5950)<<"KAlarmApp::newInstance(): exec\n";
					if (args->isSet("mail"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--mail")).arg(QString::fromLatin1("--exec")))
					alMessage = execArguments;
					action = KAlarmEvent::COMMAND;
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
					action = KAlarmEvent::EMAIL;
				}
				else
				{
					kdDebug(5950)<<"KAlarmApp::newInstance(): message\n";
					alMessage = args->arg(0);
				}

				if (action != KAlarmEvent::EMAIL)
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
					KAlarmEvent::RecurType recurType;
					if (!convInterval(args->getOption("interval"), recurType, repeatInterval)
					||  repeatInterval < 0)
						USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("--interval")))
					if (alarmNoTime  &&  recurType == KAlarmEvent::MINUTELY)
						USAGE(i18n("Invalid %1 parameter for date-only alarm").arg(QString::fromLatin1("--interval")))

					// Convert the recurrence parameters into a KCal::Recurrence
					KAlarmEvent::setRecurrence(recurrence, recurType, repeatInterval, repeatCount, endTime);
				}
				else
				{
					if (args->isSet("repeat"))
						USAGE(i18n("%1 requires %2").arg(QString::fromLatin1("--repeat")).arg(QString::fromLatin1("--interval")))
					if (args->isSet("until"))
						USAGE(i18n("%1 requires %2").arg(QString::fromLatin1("--until")).arg(QString::fromLatin1("--interval")))
				}

				QCString audioFile;
				if (args->isSet("sound"))
				{
					// Play a sound with the alarm
					if (args->isSet("beep"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--beep")).arg(QString::fromLatin1("--sound")))
					audioFile = args->getOption("sound");
				}

				int reminderMinutes = 0;
				if (args->isSet("reminder"))
				{
					// Issue a reminder alarm in advance of the main alarm
					if (args->isSet("exec"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--reminder")).arg(QString::fromLatin1("--exec")))
					if (args->isSet("mail"))
						USAGE(i18n("%1 incompatible with %2").arg(QString::fromLatin1("--reminder")).arg(QString::fromLatin1("--mail")))
					KAlarmEvent::RecurType recur;
					bool ok = convInterval(args->getOption("reminder"), recur, reminderMinutes);
					if (ok)
					{
						switch (recur)
						{
							case KAlarmEvent::MINUTELY:
								if (alarmNoTime)
									USAGE(i18n("Invalid %1 parameter for date-only alarm").arg(QString::fromLatin1("--reminder")))
								break;
							case KAlarmEvent::DAILY:     reminderMinutes *= 1440;  break;
							case KAlarmEvent::WEEKLY:    reminderMinutes *= 7*1440;  break;
							default:   ok = false;  break;
						}
					}
					if (!ok)
						USAGE(i18n("Invalid %1 parameter").arg(QString::fromLatin1("--reminder")))
				}

				int flags = KAlarmEvent::DEFAULT_FONT;
				if (args->isSet("ack-confirm"))
					flags |= KAlarmEvent::CONFIRM_ACK;
				if (args->isSet("beep"))
					flags |= KAlarmEvent::BEEP;
				if (args->isSet("late-cancel"))
					flags |= KAlarmEvent::LATE_CANCEL;
				if (args->isSet("login"))
					flags |= KAlarmEvent::REPEAT_AT_LOGIN;
				if (args->isSet("bcc"))
					flags |= KAlarmEvent::EMAIL_BCC;
				if (alarmNoTime)
					flags |= KAlarmEvent::ANY_TIME;
				args->clear();      // free up memory

				// Display or schedule the event
				setUpDcop();        // we're now ready to handle DCOP calls, so set up handlers
				if (!scheduleEvent(alMessage, alarmTime, bgColour, fgColour, QFont(), flags, audioFile,
				                   alAddresses, alSubject, alAttachments, action, recurrence,
				                   reminderMinutes))
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
				if (args->isSet("reminder"))
					usage += QString::fromLatin1("--reminder ");
				if (args->isSet("sound"))
					usage += QString::fromLatin1("--sound ");
				if (args->isSet("subject"))
					usage += QString::fromLatin1("--subject ");
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
	if (firstInstance)
		redisplayAlarms();

	--activeCount;
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
		// Quit regardless
		KAlarmMainWindow::closeAll();
		displayTrayIcon(false);
	}
	else
	{
		// Quit only if there are no more "instances" running
		if (activeCount > 0  ||  MessageWin::instanceCount())
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
* Called when the session manager is about to close down the application.
*/
void KAlarmApp::commitData(QSessionManager& sm)
{
	mSessionClosingDown = true;
	KUniqueApplication::commitData(sm);
	mSessionClosingDown = false;         // reset in case shutdown is cancelled
}

/******************************************************************************
*  Redisplay alarms which were being shown when the program last exited.
*  Normally, these alarms will have been displayed by session restoration, but
*  if the program crashed or was killed, we can redisplay them here so that
*  they won't be lost.
*/
void KAlarmApp::redisplayAlarms()
{
	if (mDisplayCalendar->isOpen())
	{
		Event::List events = mDisplayCalendar->events();
		for (Event::List::ConstIterator it = events.begin();  it != events.end();  ++it)
		{
                        Event* kcalEvent = *it;
			KAlarmEvent event(*kcalEvent);
			event.setUid(KAlarmEvent::ACTIVE);
			if (!MessageWin::findEvent(event.id()))
			{
				// This event should be displayed, but currently isn't being
				kdDebug(5950) << "KAlarmApp::redisplayAlarms(): " << event.id() << endl;
				KAlarmAlarm alarm = event.convertDisplayingAlarm();
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
*  Display a main window with the specified event selected.
*/
KAlarmMainWindow* KAlarmApp::displayMainWindowSelected(const QString& eventID)
{
	KAlarmMainWindow* win = KAlarmMainWindow::firstWindow();
	if (!win)
	{
		if (initCheck())
		{
			win = KAlarmMainWindow::create();
			win->show();
		}
	}
	else
	{
		// There is already a main window, so make it the active window
		if (!win->isVisible())
		{
			win->hide();        // in case it's on a different desktop
			win->showNormal();
		}
		win->raise();
		win->setActiveWindow();
	}
	if (win  &&  !eventID.isEmpty())
		win->selectEvent(eventID);
	return win;
}

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
	KAlarmPrefDlg prefDlg(Preferences::instance());
	prefDlg.exec();
}

/******************************************************************************
* Called when a Control Alarm Daemon menu item is selected.
* Displays the alarm daemon control dialog.
*/
void KAlarmApp::slotDaemonControl()
{
	KProcess proc;
	proc << locate("exe", QString::fromLatin1("kcmshell"));
#if KDE_VERSION >= 308
	proc << QString::fromLatin1("kcmkded");
#else
	proc << QString::fromLatin1("alarmdaemonctrl");
#endif
	proc.start(KProcess::DontCare);
}

/******************************************************************************
*  Called when the New button is clicked to edit a new alarm to add to the list.
*/
void KAlarmApp::slotNewAlarm()
{
	KAlarmMainWindow::executeNew();
}

/******************************************************************************
* Create a New Alarm KAction.
*/
KAction* KAlarmApp::createNewAlarmAction(const QString& label, QObject* receiver, const char* slot, KActionCollection* actions)
{
#if KDE_VERSION >= 310
	return new KAction(label, "filenew2", Qt::Key_Insert, receiver, slot, actions, "new");
#else
	return new KAction(label, "filenew", Qt::Key_Insert, receiver, slot, actions, "new");
#endif
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
		++activeCount;          // prevent the application from quitting
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
		--activeCount;
	}

	bool newDisableIfStopped = wantRunInSystemTray() && !mNoSystemTray && Preferences::instance()->disableAlarmsIfStopped();
	if (newDisableIfStopped != mDisableAlarmsIfStopped)
	{
		mDisableAlarmsIfStopped = newDisableIfStopped;    // N.B. this setting is used by registerWithDaemon()
		Preferences::setNotify(TrayWindow::QUIT_WARN, true, true);   // since mode has changed, re-allow warning messages on Quit
		registerWithDaemon(true);           // re-register with the alarm daemon
	}

	// Change alarm times for date-only alarms if the start of day time has changed
	if (Preferences::instance()->startOfDay() != mStartOfDay)
		changeStartOfDay();

	KAlarmEvent::setFeb29RecurType();    // in case the date for February 29th recurrences has changed

	bool refreshExpired = false;
	if (Preferences::instance()->expiredColour() != mOldExpiredColour)
	{
		// The expired alarms text colour has changed
		refreshExpired = true;
		mOldExpiredColour = Preferences::instance()->expiredColour();
	}

	if (Preferences::instance()->expiredKeepDays() != mOldExpiredKeepDays)
	{
		// Whether or not expired alarms are being kept has changed
		if (mOldExpiredKeepDays < 0
		||  Preferences::instance()->expiredKeepDays() >= 0  &&  Preferences::instance()->expiredKeepDays() < mOldExpiredKeepDays)
		{
			// expired alarms are now being kept for less long
			if (mExpiredCalendar->isOpen()  ||  mExpiredCalendar->open())
				mExpiredCalendar->purge(Preferences::instance()->expiredKeepDays(), true);
			refreshExpired = true;
		}
		else if (!mOldExpiredKeepDays)
			refreshExpired = true;
		mOldExpiredKeepDays = Preferences::instance()->expiredKeepDays();
	}

	if (refreshExpired)
		KAlarmMainWindow::updateExpired();
}

/******************************************************************************
*  Change alarm times for date-only alarms after the start of day time has changed.
*/
void KAlarmApp::changeStartOfDay()
{
	if (KAlarmEvent::adjustStartOfDay(mCalendar->events()))
		calendarSave();
	Preferences::instance()->updateStartOfDayCheck();  // now that calendar is updated, set OK flag in config file
	mStartOfDay = Preferences::instance()->startOfDay();
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
bool KAlarmApp::scheduleEvent(const QString& message, const QDateTime& dateTime, const QColor& bg,
                              const QColor& fg, const QFont& font, int flags, const QString& audioFile,
                              const EmailAddressList& mailAddresses, const QString& mailSubject,
                              const QStringList& mailAttachments, KAlarmEvent::Action action,
                              const KCal::Recurrence& recurrence, int reminderMinutes)
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

	KAlarmEvent event(alarmTime, message, bg, fg, font, action, flags);
	if (reminderMinutes)
		event.setReminder(reminderMinutes);
	if (!audioFile.isEmpty())
		event.setAudioFile(audioFile);
	if (mailAddresses.count())
		event.setEmail(mailAddresses, mailSubject, mailAttachments);
	event.setRecurrence(recurrence);
	if (display)
	{
		// Alarm is due for display already
		execAlarm(event, event.firstAlarm(), false);
		if (event.recurType() == KAlarmEvent::NO_RECUR
		||  event.setNextOccurrence(now) == KAlarmEvent::NO_OCCURRENCE)
			return true;
	}
	return addEvent(event, 0);     // event instance will now belong to the calendar
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
	switch (function)
	{
		case EVENT_TRIGGER:
		{
			// Only trigger one alarm from the event - we don't
			// want multiple identical messages, for example.
			KAlarmAlarm alarm = event.firstAlarm();
			if (alarm.valid())
				execAlarm(event, alarm, true);
			break;
		}
		case EVENT_CANCEL:
			deleteEvent(event, 0, false);
			break;

		case EVENT_HANDLE:
		{
			QDateTime now = QDateTime::currentDateTime();
			bool updateCalAndDisplay = false;
			bool displayAlarmValid = false;
			KAlarmAlarm displayAlarm;
			// Check all the alarms in turn.
			// Note that the main alarm is fetched before any other alarms.
			for (KAlarmAlarm alarm = event.firstAlarm();  alarm.valid();  alarm = event.nextAlarm(alarm))
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
							KAlarmEvent::OccurType type = event.previousOccurrence(now, next);
							switch (type)
							{
								case KAlarmEvent::FIRST_OCCURRENCE:
								case KAlarmEvent::RECURRENCE_DATE:
								case KAlarmEvent::RECURRENCE_DATE_TIME:
								case KAlarmEvent::LAST_OCCURRENCE:
									limit.setDate(next.date().addDays(1));
									limit.setTime(Preferences::instance()->startOfDay());
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
							DateTime next;
							KAlarmEvent::OccurType type = event.previousOccurrence(now, next);
							switch (type)
							{
								case KAlarmEvent::FIRST_OCCURRENCE:
								case KAlarmEvent::RECURRENCE_DATE:
								case KAlarmEvent::RECURRENCE_DATE_TIME:
								case KAlarmEvent::LAST_OCCURRENCE:
									if (next.dateTime().secsTo(now) > maxlate)
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
				updateEvent(event, 0);     // update the window lists and calendar file
			else
				kdDebug(5950) << "KAlarmApp::handleEvent(): no action\n";
			break;
		}
	}
	return true;
}

/******************************************************************************
* Called when an alarm is currently being displayed, to add a copy of the alarm
* with on-display status, and to reschedule it for its next repetition.
* If no repetitions remain, cancel it.
*/
void KAlarmApp::alarmShowing(KAlarmEvent& event, KAlarmAlarm::Type alarmType, const DateTime& alarmTime)
{
	kdDebug(5950) << "KAlarmApp::alarmShowing(" << event.id() << ", " << KAlarmAlarm::debugType(alarmType) << ")\n";
	Event* kcalEvent = mCalendar->event(event.id());
	if (!kcalEvent)
		kdError(5950) << "KAlarmApp::alarmShowing(): event ID not found: " << event.id() << endl;
	else
	{
		KAlarmAlarm alarm = event.alarm(alarmType);
		if (!alarm.valid())
			kdError(5950) << "KAlarmApp::alarmShowing(): alarm type not found: " << event.id() << ":" << alarmType << endl;
		else
		{
			// Copy the alarm to the displaying calendar in case of a crash, etc.
			KAlarmEvent dispEvent;
			dispEvent.setDisplaying(event, alarmType, alarmTime.dateTime());
			if (mDisplayCalendar->open())
			{
				mDisplayCalendar->deleteEvent(dispEvent.id());   // in case it already exists
				mDisplayCalendar->addEvent(dispEvent);
				mDisplayCalendar->save();
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
void KAlarmApp::rescheduleAlarm(KAlarmEvent& event, const KAlarmAlarm& alarm, bool updateCalAndDisplay)
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
			case KAlarmEvent::NO_OCCURRENCE:
				// All repetitions are finished, so cancel the event
				cancelAlarm(event, alarm.type(), updateCalAndDisplay);
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
		if (event.deferred())
		{
			event.removeExpiredAlarm(KAlarmAlarm::DEFERRED_ALARM);
			update = true;
		}
	}
	if (update)
		updateEvent(event, 0);     // update the window lists and calendar file
}

/******************************************************************************
* Delete the alarm. If it is the last alarm for its event, the event is removed
* from the calendar file and from every main window instance.
*/
void KAlarmApp::cancelAlarm(KAlarmEvent& event, KAlarmAlarm::Type alarmType, bool updateCalAndDisplay)
{
	kdDebug(5950) << "KAlarmApp::cancelAlarm()" << endl;
	if (alarmType == KAlarmAlarm::MAIN_ALARM  &&  !event.displaying()  &&  event.toBeArchived())
	{
		// The event is being deleted. Save it in the expired calendar file first.
		QString id = event.id();    // save event ID since archiveEvent() changes it
		archiveEvent(event);
		event.setEventID(id);       // restore event ID
	}
	event.removeExpiredAlarm(alarmType);
	if (!event.alarmCount())
		deleteEvent(event, 0, false, false);
	else if (updateCalAndDisplay)
		updateEvent(event, 0);    // update the window lists and calendar file
}

/******************************************************************************
* Execute an alarm by displaying its message or file, or executing its command.
* Reply = KProcess if command alarm
*       = 0 if an error message was output.
*/
void* KAlarmApp::execAlarm(KAlarmEvent& event, const KAlarmAlarm& alarm, bool reschedule, bool allowDefer)
{
	void* result = (void*)1;
	event.setArchive();
	if (alarm.action() == KAlarmAlarm::COMMAND)
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
			mCommandProcesses.append(new ProcData(proc, new KAlarmEvent(event), new KAlarmAlarm(alarm), shellName));
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
	else if (alarm.action() == KAlarmAlarm::EMAIL)
	{
		kdDebug(5950) << "KAlarmApp::execAlarm(): EMAIL to: " << event.emailAddresses(", ") << endl;
		QString err = KAMail::send(event, (reschedule || allowDefer));
		if (!err.isNull())
		{
			kdDebug(5950) << "KAlarmApp::execAlarm(): failed\n";
			QStringList errmsgs;
			if (err.isEmpty())
				errmsgs += i18n("Failed to send email");
			else
			{
				errmsgs += i18n("Failed to send email:");
				errmsgs += err;
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
		||  (win->alarmType() & KAlarmAlarm::REMINDER_ALARM) && !(alarm.type() & KAlarmAlarm::REMINDER_ALARM))
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
* Fetch an event with the given ID from the appropriate (active or expired) calendar.
*/
const Event* KAlarmApp::getEvent(const QString& eventID)
{
	if (!eventID.isEmpty())
	{
		if (KAlarmEvent::uidStatus(eventID) == KAlarmEvent::EXPIRED)
		{
			if (expiredCalendar())
				return mExpiredCalendar->event(eventID);
		}
		else
			return mCalendar->event(eventID);
	}
	return 0;
}

/******************************************************************************
* Add a new active (non-expired) alarm.
* Save it in the calendar file and add it to every main window instance.
* Parameters:
*    win  = initiating main window instance (which has already been updated)
*/
bool KAlarmApp::addEvent(const KAlarmEvent& event, KAlarmMainWindow* win, bool useEventID)
{
	kdDebug(5950) << "KAlarmApp::addEvent(): " << event.id() << endl;
	if (!initCheck())
		return false;

	// Save the event details in the calendar file, and get the new event ID
	mCalendar->addEvent(event, useEventID);
	calendarSave();

	// Update the window lists
	KAlarmMainWindow::addEvent(event, win);
	return true;
}

/******************************************************************************
* Modify an active (non-expired) alarm in every main window instance.
* The new event will have a different event ID from the old one.
* Parameters:
*    win  = initiating main window instance (which has already been updated)
*/
void KAlarmApp::modifyEvent(KAlarmEvent& oldEvent, const KAlarmEvent& newEvent, KAlarmMainWindow* win)
{
	kdDebug(5950) << "KAlarmApp::modifyEvent(): '" << oldEvent.id() << endl;

	if (!newEvent.valid())
		deleteEvent(oldEvent, win, true);
	else
	{
		// Update the event in the calendar file, and get the new event ID
		mCalendar->deleteEvent(oldEvent.id());
		mCalendar->addEvent(newEvent, true);
		calendarSave();

		// Update the window lists
		KAlarmMainWindow::modifyEvent(oldEvent.id(), newEvent, win);
	}
}

/******************************************************************************
* Update an active (non-expired) alarm in every main window instance.
* The new event will have the same event ID as the old one.
* Parameters:
*    win  = initiating main window instance (which has already been updated)
*/
void KAlarmApp::updateEvent(KAlarmEvent& event, KAlarmMainWindow* win, bool archiveOnDelete)
{
	kdDebug(5950) << "KAlarmApp::updateEvent(): " << event.id() << endl;

	if (!event.valid())
		deleteEvent(event, win, true, archiveOnDelete);
	else
	{
		// Update the event in the calendar file
		event.incrementRevision();
		mCalendar->updateEvent(event);
		calendarSave();

		// Update the window lists
		KAlarmMainWindow::modifyEvent(event, win);
	}
}

/******************************************************************************
* Delete an alarm from every main window instance.
* Parameters:
*    win  = initiating main window instance (which has already been updated)
*/
void KAlarmApp::deleteEvent(KAlarmEvent& event, KAlarmMainWindow* win, bool tellDaemon, bool archive)
{
	kdDebug(5950) << "KAlarmApp::deleteEvent(): " << event.id() << endl;

	// Update the window lists
	KAlarmMainWindow::deleteEvent(event.id(), win);

	// Delete the event from the calendar file
	if (KAlarmEvent::uidStatus(event.id()) == KAlarmEvent::EXPIRED)
	{
		if (expiredCalendar(false))
			mExpiredCalendar->deleteEvent(event.id(), true);   // save calendar after deleting
	}
	else
	{
		QString id = event.id();
		if (archive  &&  event.toBeArchived())
			archiveEvent(event);
		mCalendar->deleteEvent(id);
		calendarSave(tellDaemon);
	}
}

/******************************************************************************
* Delete an alarm from the display calendar.
*/
void KAlarmApp::deleteDisplayEvent(const QString& eventID) const
{
	kdDebug(5950) << "KAlarmApp::deleteDisplayEvent(): " << eventID << endl;

	if (KAlarmEvent::uidStatus(eventID) == KAlarmEvent::DISPLAYING)
	{
		if (mDisplayCalendar->open())
			mDisplayCalendar->deleteEvent(eventID, true);   // save calendar after deleting
	}
}

/******************************************************************************
* Undelete an expired alarm in every main window instance.
* Parameters:
*    win  = initiating main window instance (which has already been updated)
*/
void KAlarmApp::undeleteEvent(KAlarmEvent& event, KAlarmMainWindow* win)
{
	kdDebug(5950) << "KAlarmApp::undeleteEvent(): " << event.id() << endl;

	// Delete the event from the expired calendar file
	if (KAlarmEvent::uidStatus(event.id()) == KAlarmEvent::EXPIRED)
	{
		QString id = event.id();
		mCalendar->addEvent(event);
		calendarSave();

		// Update the window lists
		KAlarmMainWindow::undeleteEvent(id, event, win);

		if (expiredCalendar(false))
			mExpiredCalendar->deleteEvent(id, true);   // save calendar after deleting
	}
}

/******************************************************************************
* Save the event in the expired calendar file.
* The event's ID is changed to an expired ID.
*/
void KAlarmApp::archiveEvent(KAlarmEvent& event)
{
	kdDebug(5950) << "KAlarmApp::archiveEvent(" << event.id() << ")\n";
	if (expiredCalendar(false))
	{
		event.setSaveDateTime(QDateTime::currentDateTime());   // time stamp to control purging
		Event* kcalEvent = mExpiredCalendar->addEvent(event);
		mExpiredCalendar->save();

		if (kcalEvent)
			KAlarmMainWindow::modifyEvent(KAlarmEvent(*kcalEvent), 0);   // update window lists
	}
}

/******************************************************************************
* Open the expired calendar file if necessary, and purge old events from it.
*/
AlarmCalendar* KAlarmApp::expiredCalendar(bool saveIfPurged)
{
	if (Preferences::instance()->expiredKeepDays())
	{
		// Expired events are being kept
		if (mExpiredCalendar->isOpen()  ||  mExpiredCalendar->open())
		{
			if (Preferences::instance()->expiredKeepDays() > 0)
				mExpiredCalendar->purge(Preferences::instance()->expiredKeepDays(), saveIfPurged);
			return mExpiredCalendar;
		}
		kdError(5950) << "KAlarmApp::expiredCalendar(): open error\n";
	}
	return 0;
}

/******************************************************************************
* Flag the start of a group of calendar update calls.
*/
void KAlarmApp::startCalendarUpdate()
{
	if (!mCalendarUpdateCount++)
	{
		mCalendarUpdateSave   = false;
		mCalendarUpdateReload = false;
	}
}

/******************************************************************************
* Flag the end of a group of calendar update calls.
*/
void KAlarmApp::endCalendarUpdate()
{
	if (mCalendarUpdateCount > 0)
		--mCalendarUpdateCount;
	if (!mCalendarUpdateCount)
	{
		if (mCalendarUpdateSave)
		{
			mCalendar->save();
			mCalendarUpdateSave = false;
		}
		if (mCalendarUpdateReload)
			reloadDaemon();
	}
}

/******************************************************************************
* Save the alarm calendar and optionally reload the alarm daemon.
*/
void KAlarmApp::calendarSave(bool reload)
{
	if (reload)
		mCalendarUpdateReload = true;
	if (mCalendarUpdateCount)
		mCalendarUpdateSave = true;
	else
	{
		mCalendar->save();
		mCalendarUpdateSave = false;
		if (mCalendarUpdateReload)
			reloadDaemon();
	}
}

/******************************************************************************
* Called when a calendar has been saved.
* If it's the alarm calendar, notify the alarm daemon.
*/
void KAlarmApp::calendarSaved(AlarmCalendar* cal)
{
	if (cal == mCalendar)
		reloadDaemon();
}

/******************************************************************************
* Set up the DCOP handlers.
*/
void KAlarmApp::setUpDcop()
{
	if (!mDcopHandler)
	{
		mDcopHandler      = new DcopHandler(DCOP_OBJECT_NAME);
		mDaemonGuiHandler = new DaemonGuiHandler(GUI_DCOP_OBJECT_NAME);
	}
}

/******************************************************************************
* If this is the first time through, open the calendar file, optionally start
* the alarm daemon and register with it, and set up the DCOP handler.
*/
bool KAlarmApp::initCheck(bool calendarOnly)
{
	bool startdaemon;
	if (!mCalendar->isOpen())
	{
		kdDebug(5950) << "KAlarmApp::initCheck(): opening calendar\n";

		// First time through. Open the calendar file.
		if (!mCalendar->open())
			return false;

		if (!mStartOfDay.isValid())
			changeStartOfDay();     // start of day time has changed, so adjust date-only alarms

		/* Need to open the display calendar now, since otherwise if the daemon
		 * immediately notifies display alarms, they will often be processed while
		 * redisplayAlarms() is executing open() (but before open() completes),
		 * which causes problems!!
		 */
		mDisplayCalendar->open();

		startdaemon = true;
	}
	else
		startdaemon = !mDaemonRegistered;

	if (!calendarOnly)
	{
		if (startdaemon)
			startDaemon();          // make sure the alarm daemon is running
		setUpDcop();              // we're now ready to handle DCOP calls, so set up handlers
	}
	return true;
}

/******************************************************************************
* Start the alarm daemon if necessary, and register this application with it.
*/
void KAlarmApp::startDaemon()
{
	kdDebug(5950) << "KAlarmApp::startDaemon()\n";
	if (!dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
	{
		// Start the alarm daemon. It is a KUniqueApplication, which means that
		// there is automatically only one instance of the alarm daemon running.
		QString execStr = locate("exe",QString::fromLatin1(DAEMON_APP_NAME));
		kdeinitExecWait(execStr);
		kdDebug(5950) << "KAlarmApp::startDaemon(): Alarm daemon started" << endl;
	}

	// Register this application with the alarm daemon
	registerWithDaemon(false);

	// Tell alarm daemon to load the calendar
	{
		QByteArray data;
		QDataStream arg(data, IO_WriteOnly);
		arg << QCString(aboutData()->appName()) << mCalendar->urlString();
		if (!dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "addMsgCal(QCString,QString)", data))
			kdError(5950) << "KAlarmApp::startDaemon(): addMsgCal dcop send failed" << endl;
	}

	mDaemonRegistered = true;
	kdDebug(5950) << "KAlarmApp::startDaemon(): started daemon" << endl;
}

/******************************************************************************
* Start the alarm daemon if necessary, and register this application with it.
*/
void KAlarmApp::registerWithDaemon(bool reregister)
{
	kdDebug(5950) << (reregister ? "KAlarmApp::reregisterWithDaemon(): " : "KAlarmApp::registerWithDaemon(): ") << (mDisableAlarmsIfStopped ? "NO_START" : "COMMAND_LINE") << endl;
	QByteArray data;
	QDataStream arg(data, IO_WriteOnly);
	arg << QCString(aboutData()->appName()) << aboutData()->programName()
	    << QCString(DCOP_OBJECT_NAME)
	    << static_cast<int>(mDisableAlarmsIfStopped ? ClientInfo::NO_START_NOTIFY : ClientInfo::COMMAND_LINE_NOTIFY)
	    << (Q_INT8)0;
	const char* func = reregister ? "reregisterApp(QCString,QString,QCString,int,bool)" : "registerApp(QCString,QString,QCString,int,bool)";
	if (!dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, func, data))
		kdError(5950) << "KAlarmApp::registerWithDaemon(): registerApp dcop send failed" << endl;
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
	if (mExpiredCalendar->isOpen())
		mExpiredCalendar->reload();
	KAlarmMainWindow::refresh();
	if (!dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
		startDaemon();
	else
	{
		QByteArray data;
		QDataStream arg(data, IO_WriteOnly);
		arg << QCString(aboutData()->appName()) << mCalendar->urlString();
		if (!dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "resetMsgCal(QCString,QString)", data))
			kdError(5950) << "KAlarmApp::resetDaemon(): resetMsgCal dcop send failed" << endl;
	}
}

/******************************************************************************
* Tell the alarm daemon to reread the calendar file.
*/
void KAlarmApp::reloadDaemon()
{
	kdDebug(5950) << "KAlarmApp::reloadDaemon()\n";
	QByteArray data;
	QDataStream arg(data, IO_WriteOnly);
	arg << QCString(aboutData()->appName()) << mCalendar->urlString();
	if (!dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "reloadMsgCal(QCString,QString)", data))
		kdError(5950) << "KAlarmApp::reloadDaemon(): reloadMsgCal dcop send failed" << endl;
	else
		mCalendarUpdateReload = false;
}

/******************************************************************************
* Check whether the alarm daemon is currently running.
*/
bool KAlarmApp::isDaemonRunning(bool startdaemon)
{
	bool running = dcopClient()->isApplicationRegistered(DAEMON_APP_NAME);
	if (running != mDaemonRunning)
	{
		// Daemon's status has changed
		mDaemonRunning = running;
		if (mDaemonRunning  &&  startdaemon)
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
* Check from its mime type whether a file appears to be a text or image file.
* Reply = 0 if not a text or image file
*       = 1 if a plain text file
*       = 2 if a formatted text file
*       = 3 if an application text file
*       = 4 if an image file.
*/
int KAlarmApp::fileType(const QString& mimetype)
{
	static const char* applicationTypes[] = {
		"x-shellscript", "x-nawk", "x-awk", "x-perl", "x-python",
		"x-desktop", "x-troff", 0 };
	static const char* formattedTextTypes[] = {
		"html", "xml", 0 };

	if (mimetype.startsWith(QString::fromLatin1("image/")))
		return 4;
	int slash = mimetype.find('/');
	if (slash < 0)
		return 0;
	QString type = mimetype.mid(slash + 1);
	const char* typel = type.latin1();
	if (mimetype.startsWith(QString::fromLatin1("application")))
	{
		for (int i = 0;  applicationTypes[i];  ++i)
			if (!strcmp(typel, applicationTypes[i]))
				return 3;
	}
	else if (mimetype.startsWith(QString::fromLatin1("text")))
	{
		for (int i = 0;  formattedTextTypes[i];  ++i)
			if (!strcmp(typel, formattedTextTypes[i]))
				return 2;
		return 1;
	}
	return 0;
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
static bool convInterval(QCString timeParam, KAlarmEvent::RecurType& recurType, int& timeInterval)
{
	// Get the recurrence interval
	bool ok = true;
	uint interval = 0;
	uint length = timeParam.length();
	switch (timeParam[length - 1])
	{
		case 'Y':
			recurType = KAlarmEvent::ANNUAL_DATE;
			timeParam = timeParam.left(length - 1);
			break;
		case 'W':
			recurType = KAlarmEvent::WEEKLY;
			timeParam = timeParam.left(length - 1);
			break;
		case 'D':
			recurType = KAlarmEvent::DAILY;
			timeParam = timeParam.left(length - 1);
			break;
		case 'M':
		{
			int i = timeParam.find('H');
			if (i < 0)
				recurType = KAlarmEvent::MONTHLY_DAY;
			else
			{
				recurType = KAlarmEvent::MINUTELY;
				interval = timeParam.left(i).toUInt(&ok) * 60;
				timeParam = timeParam.mid(i + 1, length - i - 2);
			}
			break;
		}
		default:       // should be a digit
			recurType = KAlarmEvent::MINUTELY;
			break;
	}
	if (ok)
		interval += timeParam.toUInt(&ok);
	timeInterval = static_cast<int>(interval);
	return ok;
}


KAlarmApp::ProcData::~ProcData()
{
	delete process;
	delete event;
	delete alarm;
}
