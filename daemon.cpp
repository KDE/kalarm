/*
 *  daemon.cpp  -  interface with alarm daemon
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
 */

#include "kalarm.h"

#include <qtimer.h>
#include <qiconset.h>

#include <kstandarddirs.h>
#include <kconfig.h>
#include <kprocess.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kaboutdata.h>
#include <kmessagebox.h>
#include <dcopclient.h>
#include <kdebug.h>

#include <kalarmd/alarmdaemoniface_stub.h>
#include <kalarmd/alarmguiiface.h>
#include <kalarmd/clientinfo.h>

#include "alarmcalendar.h"
#include "kalarmapp.h"
#include "preferences.h"
#include "daemon.moc"


#define             DAEMON_APP_NAME_DEF   "kalarmd"    // DCOP name of alarm daemon application
static const char*  DAEMON_APP_NAME     = DAEMON_APP_NAME_DEF;
static const char*  DAEMON_DCOP_OBJECT  = "ad";        // DCOP name of kalarmd's DCOP interface
static const char*  NOTIFY_DCOP_OBJECT  = "notify";    // DCOP name of KAlarm's interface for notification by alarm daemon

static QString expandURL(const QString& urlString);


/*=============================================================================
=  Class: DaemonGuiHandler
=  Handles the the alarm daemon's GUI client DCOP interface.
=============================================================================*/

class DaemonGuiHandler : public QObject, virtual public AlarmGuiIface
{
	public:
		explicit DaemonGuiHandler();
		static void  registerWith();
	private:
		// DCOP interface
		void         alarmDaemonUpdate(int alarmGuiChangeType,
		                               const QString& calendarURL, const QCString& appName);
		void         handleEvent(const QString& calendarURL, const QString& eventID);
		void         handleEvent(const QString& /*iCalendarString*/)  { }   // not used by KAlarm
		void         registered(bool reregister, int result);
};


Daemon*           Daemon::mInstance = 0;
DaemonGuiHandler* Daemon::mDcopHandler = 0;
QTimer*           Daemon::mStartTimer = 0;
QTimer*           Daemon::mRegisterTimer = 0;
QTimer*           Daemon::mStatusTimer = 0;
int               Daemon::mStatusTimerCount = 0;
int               Daemon::mStatusTimerInterval;
QDateTime         Daemon::mLastCheck;
QDateTime         Daemon::mNextCheck;
int               Daemon::mCheckInterval = 0;
int               Daemon::mStartTimeout = 0;
Daemon::Status    Daemon::mStatus = Daemon::STOPPED;
bool              Daemon::mRunning = false;
bool              Daemon::mCalendarDisabled = false;
bool              Daemon::mEnableCalPending = false;
bool              Daemon::mRegisterFailMsg = false;

// How frequently to check the daemon's status after starting it.
// This is equal to the length of time we wait after the daemon is registered with DCOP
// before we assume that it is ready to accept DCOP calls.
static const int startCheckInterval = 500;     // 500 milliseconds


/******************************************************************************
* Initialise.
* A Daemon instance needs to be constructed only in order for slots to work.
* All external access is via static methods.
*/
void Daemon::initialise()
{
	if (!mInstance)
		mInstance = new Daemon();
	connect(AlarmCalendar::activeCalendar(), SIGNAL(calendarSaved(AlarmCalendar*)), mInstance, SLOT(slotCalendarSaved(AlarmCalendar*)));
	readCheckInterval();
}

/******************************************************************************
* Initialise the daemon status timer.
*/
void Daemon::createDcopHandler()
{
	if (mDcopHandler)
		return;
	mDcopHandler = new DaemonGuiHandler();
	// Check if the alarm daemon is running, but don't start it yet, since
	// the program is still initialising.
	mRunning = isRunning(false);

	mStatusTimerInterval = Preferences::instance()->daemonTrayCheckInterval();
	connect(Preferences::instance(), SIGNAL(preferencesChanged()), mInstance, SLOT(slotPreferencesChanged()));

	mStatusTimer = new QTimer(mInstance);
	connect(mStatusTimer, SIGNAL(timeout()), mInstance, SLOT(timerCheckIfRunning()));
	mStatusTimer->start(mStatusTimerInterval * 1000);  // check regularly if daemon is running
}

/******************************************************************************
* Start the alarm daemon if necessary, and register this application with it.
* Reply = false if the daemon definitely couldn't be started or registered with.
*/
bool Daemon::start()
{
	kdDebug(5950) << "Daemon::start()\n";
	updateRegisteredStatus();
	switch (mStatus)
	{
		case STOPPED:
		{
			if (mStartTimer)
				return true;     // we're currently waiting for the daemon to start
			// Start the alarm daemon. It is a KUniqueApplication, which means that
			// there is automatically only one instance of the alarm daemon running.
			QString execStr = locate("exe", QString::fromLatin1(DAEMON_APP_NAME));
			if (execStr.isEmpty())
			{
				KMessageBox::error(0, i18n("Alarm Daemon not found"));
				kdError() << "Daemon::startApp(): " DAEMON_APP_NAME_DEF " not found" << endl;
				return false;
			}
			KApplication::kdeinitExec(execStr);
			kdDebug(5950) << "Daemon::start(): Alarm daemon started" << endl;
			mStartTimeout = 5000/startCheckInterval + 1;    // check daemon status for 5 seconds before giving up
			mStartTimer = new QTimer(mInstance);
			connect(mStartTimer, SIGNAL(timeout()), mInstance, SLOT(checkIfStarted()));
			mStartTimer->start(startCheckInterval);
			mInstance->checkIfStarted();
			return true;
		}
		case RUNNING:
			return true;     // we're waiting for the daemon to be completely ready
		case READY:
			// Daemon is ready. Register this application with it.
			if (!registerWith(false))
				return false;
			break;
		case REGISTERED:
			break;
	}
	return true;
}

/******************************************************************************
* Register this application with the alarm daemon.
*/
bool Daemon::registerWith(bool reregister)
{
	if (mRegisterTimer)
		return true;
	if (mStatus == STOPPED  ||  mStatus == RUNNING)
		return false;
	if (mStatus == REGISTERED  &&  !reregister)
		return true;

	bool disabledIfStopped = theApp()->alarmsDisabledIfStopped();
	kdDebug(5950) << (reregister ? "Daemon::reregisterWith(): " : "Daemon::registerWith(): ") << (disabledIfStopped ? "NO_START" : "COMMAND_LINE") << endl;
	QByteArray data;
	QDataStream arg(data, IO_WriteOnly);
	arg << QCString(kapp->aboutData()->appName()) << kapp->aboutData()->programName()
	    << QCString(NOTIFY_DCOP_OBJECT)
	    << static_cast<int>(disabledIfStopped ? ClientInfo::DCOP_NOTIFY : ClientInfo::COMMAND_LINE_NOTIFY)
	    << (Q_INT8)0;
	const char* func = reregister ? "reregisterApp(QCString,QString,QCString,int,bool)" : "registerApp(QCString,QString,QCString,int,bool)";
	if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, func, data))
	{
		registrationResult(reregister, AlarmGuiIface::FAILURE);
		return false;
	}
	mRegisterTimer = new QTimer(mInstance);
	connect(mRegisterTimer, SIGNAL(timeout()), mInstance, SLOT(registerTimerExpired()));
	mRegisterTimer->start(10000);     // wait up to 10 seconds for the reply

	// Register as a GUI with the alarm daemon
	DaemonGuiHandler::registerWith();
	return true;
}

/******************************************************************************
* Called when the daemon has notified us of the result of the register() DCOP call.
*/
void Daemon::registrationResult(bool reregister, int result)
{
	kdDebug(5950) << "Daemon::registrationResult(" << reregister << ")\n";
	delete mRegisterTimer;
	mRegisterTimer = 0;
	switch (result)
	{
		case AlarmGuiIface::SUCCESS:
			break;
		case AlarmGuiIface::NOT_FOUND:
			kdError(5950) << "Daemon::registrationResult(" << reregister << "): registerApp dcop call: " << kapp->aboutData()->appName() << " not found\n";
			KMessageBox::error(0, i18n("Alarms will be disabled if you stop %1.\n"
			                           "(Installation or configuration error: %2 cannot locate %3 executable.)")
					           .arg(kapp->aboutData()->programName())
			                           .arg(QString::fromLatin1(DAEMON_APP_NAME))
			                           .arg(kapp->aboutData()->appName()));
			break;
		case AlarmGuiIface::FAILURE:
		default:
			kdError(5950) << "Daemon::registrationResult(" << reregister << "): registerApp dcop call failed -> " << result << endl;
			if (!reregister)
			{
				if (mStatus == REGISTERED)
					mStatus = READY;
				if (!mRegisterFailMsg)
				{
					mRegisterFailMsg = true;
					KMessageBox::error(0, i18n("Cannot enable alarms:\nFailed to register with Alarm Daemon (%1)")
					                           .arg(QString::fromLatin1(DAEMON_APP_NAME)));
				}
			}
			return;
	}

	if (!reregister)
	{
		// Tell alarm daemon to load the calendar
		QByteArray data;
		QDataStream arg(data, IO_WriteOnly);
		arg << QCString(kapp->aboutData()->appName()) << AlarmCalendar::activeCalendar()->urlString();
		if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "addMsgCal(QCString,QString)", data))
			kdError(5950) << "Daemon::start(): addMsgCal dcop send failed" << endl;
		else
		{
			mStatus = REGISTERED;
			mRegisterFailMsg = false;
			kdDebug(5950) << "Daemon::start(): daemon startup complete" << endl;
		}
	}
}

/******************************************************************************
* Check whether the alarm daemon has started yet, and if so, register with it.
*/
void Daemon::checkIfStarted()
{
	updateRegisteredStatus();
	switch (mStatus)
	{
		case STOPPED:
			if (--mStartTimeout > 0)
				return;     // wait a bit more to check again
			kdError(5950) << "Daemon::checkIfStarted(): failed to start daemon" << endl;
			KMessageBox::error(0, i18n("Cannot enable alarms:\nFailed to start Alarm Daemon (%1)").arg(QString::fromLatin1(DAEMON_APP_NAME)));
			break;
		case RUNNING:
		case READY:
		case REGISTERED:
			break;
	}
	delete mStartTimer;
	mStartTimer = 0;
}

/******************************************************************************
* Check whether the alarm daemon has started yet, and if so, whether it is
* ready to accept DCOP calls.
*/
void Daemon::updateRegisteredStatus(bool timeout)
{
	if (!kapp->dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
	{
		mStatus = STOPPED;
		mRegisterFailMsg = false;
	}
	else
	{
		switch (mStatus)
		{
			case STOPPED:
				// The daemon has newly been detected as registered with DCOP.
				// Wait for a short time to ensure that it is ready for DCOP calls.
				mStatus = RUNNING;
				QTimer::singleShot(startCheckInterval, mInstance, SLOT(slotStarted()));
				break;
			case RUNNING:
				if (timeout)
				{
					mStatus = READY;
					start();
				}
				break;
			case READY:
			case REGISTERED:
				break;
		}
	}
	kdDebug(5950) << "Daemon::updateRegisteredStatus() -> " << mStatus << endl;
}

/******************************************************************************
* Stop the alarm daemon if it is running.
*/
bool Daemon::stop()
{
	kdDebug(5950) << "Daemon::stop()" << endl;
	if (kapp->dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
	{
		QByteArray data;
		if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "quit()", data))
		{
			kdError(5950) << "Daemon::stop(): quit dcop send failed" << endl;
			return false;
		}
	}
	return true;
}

/******************************************************************************
* Reset the alarm daemon.
* Reply = true if daemon was told to reset
*       = false if daemon is not running.
*/
bool Daemon::reset()
{
	kdDebug(5950) << "Daemon::reset()" << endl;
	if (!kapp->dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
		return false;
	QByteArray data;
	QDataStream arg(data, IO_WriteOnly);
	arg << QCString(kapp->aboutData()->appName()) << AlarmCalendar::activeCalendar()->urlString();
	if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "resetMsgCal(QCString,QString)", data))
		kdError(5950) << "Daemon::reset(): resetMsgCal dcop send failed" << endl;
	return true;
}

/******************************************************************************
* Tell the alarm daemon to reread the calendar file.
*/
void Daemon::reload()
{
	kdDebug(5950) << "Daemon::reload()\n";
	QByteArray data;
	QDataStream arg(data, IO_WriteOnly);
	arg << QCString(kapp->aboutData()->appName()) << AlarmCalendar::activeCalendar()->urlString();
	if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "reloadMsgCal(QCString,QString)", data))
		kdError(5950) << "Daemon::reload(): reloadMsgCal dcop send failed" << endl;
}

/******************************************************************************
* Tell the alarm daemon to enable/disable monitoring of the calendar file.
*/
void Daemon::enableCalendar(bool enable)
{
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.enableCal(AlarmCalendar::activeCalendar()->urlString(), enable);
	mEnableCalPending = false;
}

/******************************************************************************
* Notification that the alarm daemon has enabled/disabled monitoring of the
* calendar file.
*/
void Daemon::calendarIsEnabled(bool enabled)
{
	mCalendarDisabled = !enabled;
	emit mInstance->daemonRunning(enabled);
}

/******************************************************************************
* Tell the alarm daemon to stop or start monitoring the calendar file as
* appropriate.
*/
void Daemon::setAlarmsEnabled(bool enable)
{
	kdDebug(5950) << "Daemon::setAlarmsEnabled(" << enable << ")\n";
	if (enable  &&  !checkIfRunning())
	{
		// The daemon is not running, so start it
		if (!start())
		{
			emit daemonRunning(false);
			return;
		}
		mEnableCalPending = true;
		setFastCheck();
	}

	// If the daemon is now running, tell it to enable/disable the calendar
	if (checkIfRunning())
		enableCalendar(enable);
}

/******************************************************************************
* Return whether the alarm daemon is monitoring alarms.
*/
bool Daemon::monitoringAlarms()
{
	bool ok = !mCalendarDisabled  &&  isRunning();
	emit mInstance->daemonRunning(ok);
	return ok;
}

/******************************************************************************
* Check whether the alarm daemon is currently running and available.
*/
bool Daemon::isRunning(bool startdaemon)
{
	static bool runState = false;
	updateRegisteredStatus();
	bool newRunState = (mStatus == READY  ||  mStatus == REGISTERED);
	if (newRunState != runState)
	{
		// Daemon's status has changed
		runState = newRunState;
		if (runState  &&  startdaemon)
			start();      // re-register with the daemon
	}
	return runState  &&  (mStatus == REGISTERED);
}

/******************************************************************************
* Called by the timer to check whether the daemon is running.
*/
void Daemon::timerCheckIfRunning()
{
	checkIfRunning();
	// Limit how long we check at the fast rate
	if (mStatusTimerCount > 0  &&  --mStatusTimerCount <= 0)
		mStatusTimer->changeInterval(mStatusTimerInterval * 1000);
}

/******************************************************************************
* Check whether the alarm daemon is currently running.
* If its status has changed, trigger GUI updates.
*/
bool Daemon::checkIfRunning()
{
	bool newstatus = isRunning();
	if (newstatus != mRunning)
	{
		mRunning = newstatus;
		int status = mRunning  &&  !mCalendarDisabled;
		emit mInstance->daemonRunning(status);
		mStatusTimer->changeInterval(mStatusTimerInterval * 1000);   // exit from fast checking
		mStatusTimerCount = 0;
		if (mRunning)
		{
			// The alarm daemon has started up
			if (mEnableCalPending)
				enableCalendar(true);  // tell it to monitor the calendar, if appropriate
		}
	}
	return mRunning;
}

/******************************************************************************
* Starts checking at a faster rate whether the daemon is running.
*/
void Daemon::setFastCheck()
{
	mStatusTimer->start(500);    // check new status every half second
	mStatusTimerCount = 20;      // don't check at this rate for more than 10 seconds
}

/******************************************************************************
* Called when a program setting has changed.
* If the system tray icon update interval has changed, reset the timer.
*/
void Daemon::slotPreferencesChanged()
{
	int newInterval = Preferences::instance()->daemonTrayCheckInterval();
	if (newInterval != mStatusTimerInterval)
	{
		// Daemon check interval has changed
		mStatusTimerInterval = newInterval;
		if (mStatusTimerCount <= 0)   // don't change if on fast rate
			mStatusTimer->changeInterval(mStatusTimerInterval * 1000);
	}
}

/******************************************************************************
* Create an "Alarms Enabled/Enable Alarms" action.
*/
AlarmEnableAction* Daemon::createAlarmEnableAction(KActionCollection* actions, const char* name)
{
	AlarmEnableAction* a = new AlarmEnableAction(Qt::CTRL+Qt::Key_A, actions, name);
	connect(a, SIGNAL(userClicked(bool)), mInstance, SLOT(setAlarmsEnabled(bool)));
	connect(mInstance, SIGNAL(daemonRunning(bool)), a, SLOT(setCheckedActual(bool)));
	return a;
}

/******************************************************************************
* Create a "Control/Configure Alarm Daemon" action.
*/
KAction* Daemon::createControlAction(KActionCollection* actions, const char* name)
{
	if (!mInstance)
		return 0;
#if KDE_VERSION >= 308
	return new KAction(i18n("Control the Alarm Daemon", "Control Alarm &Daemon..."),
	                   0, mInstance, SLOT(slotControl()), actions, name);
#else
	KAction* prefs = KStdAction::preferences(mInstance, "x", actions);
	KAction* action = new KAction(i18n("Configure Alarm &Daemon..."), prefs->iconSet(),
	                              0, mInstance, SLOT(slotControl()), actions, name);
	delete prefs;
	return action;
#endif
}

/******************************************************************************
* Called when a Control Alarm Daemon menu item is selected.
* Displays the alarm daemon control dialog.
*/
void Daemon::slotControl()
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
* Called when a calendar has been saved.
* If it's the active alarm calendar, notify the alarm daemon.
*/
void Daemon::slotCalendarSaved(AlarmCalendar* cal)
{
	if (cal == AlarmCalendar::activeCalendar())
		reload();
}

/******************************************************************************
* Read the alarm daemon's alarm check interval from its config file. If it has
* reduced, any late-cancel alarms which are already due could potentially be
* cancelled erroneously. To avoid this, note the effective last time that it
* will have checked alarms.
*/
void Daemon::readCheckInterval()
{
	KConfig config(locate("config", DAEMON_APP_NAME_DEF"rc"));
	config.setGroup("General");
	int checkInterval = 60 * config.readNumEntry("CheckInterval", 1);
	if (checkInterval < mCheckInterval)
	{
		// The daemon check interval has reduced,
		// Note the effective last time that the daemon checked alarms.
		QDateTime now = QDateTime::currentDateTime();
		mLastCheck = now.addSecs(-mCheckInterval);
		mNextCheck = now.addSecs(checkInterval);
	}
	mCheckInterval = checkInterval;
}

/******************************************************************************
* Return the maximum time (in seconds) elapsed since the last time the alarm
* daemon must have checked alarms.
*/
int Daemon::maxTimeSinceCheck()
{
	readCheckInterval();
	if (mLastCheck.isValid())
	{
		QDateTime now = QDateTime::currentDateTime();
		if (mNextCheck > now)
		{
			// Daemon's check interval has just reduced, so allow extra time
			return mLastCheck.secsTo(now);
		}
		mLastCheck = QDateTime();
	}
	return mCheckInterval;
}


/*=============================================================================
=  Class: DaemonGuiHandler
=============================================================================*/

DaemonGuiHandler::DaemonGuiHandler()
	: DCOPObject(NOTIFY_DCOP_OBJECT), 
	  QObject()
{
	kdDebug(5950) << "DaemonGuiHandler::DaemonGuiHandler()\n";
}

/******************************************************************************
 * DCOP call from the alarm daemon to notify a change.
 * The daemon notifies calendar statuses when we first register as a GUI, and whenever
 * a calendar status changes. So we don't need to read its config files.
 */
void DaemonGuiHandler::alarmDaemonUpdate(int alarmGuiChangeType,
                                         const QString& calendarURL, const QCString& /*appName*/)
{
	kdDebug(5950) << "DaemonGuiHandler::alarmDaemonUpdate(" << alarmGuiChangeType << ")\n";
	AlarmGuiChangeType changeType = AlarmGuiChangeType(alarmGuiChangeType);
	switch (changeType)
	{
		case CHANGE_STATUS:    // daemon status change
			Daemon::readCheckInterval();
			return;
		case CHANGE_CLIENT:    // change to daemon's client application list
			return;
		default:
		{
			// It must be a calendar-related change
			if (expandURL(calendarURL) != AlarmCalendar::activeCalendar()->urlString())
				return;     // it's not a notification about KAlarm's calendar
			bool enabled = false;
			switch (changeType)
			{
				case DELETE_CALENDAR:
					kdDebug(5950) << "DaemonGuiHandler::alarmDaemonUpdate(DELETE_CALENDAR)\n";
					break;
				case CALENDAR_UNAVAILABLE:
					// Calendar is not available for monitoring
					kdDebug(5950) << "DaemonGuiHandler::alarmDaemonUpdate(CALENDAR_UNAVAILABLE)\n";
					break;
				case DISABLE_CALENDAR:
					// Calendar is available for monitoring but is not currently being monitored
					kdDebug(5950) << "DaemonGuiHandler::alarmDaemonUpdate(DISABLE_CALENDAR)\n";
					break;
				case ENABLE_CALENDAR:
					// Calendar is currently being monitored
					kdDebug(5950) << "DaemonGuiHandler::alarmDaemonUpdate(ENABLE_CALENDAR)\n";
					enabled = true;
					break;
				case ADD_CALENDAR:        // add a KOrganizer-type calendar
				case ADD_MSG_CALENDAR:    // add a KAlarm-type calendar
				default:
					return;
			}
			Daemon::calendarIsEnabled(enabled);
			break;
		}
	}
}

/******************************************************************************
 * DCOP call from the alarm daemon to notify that an alarm is due.
 */
void DaemonGuiHandler::handleEvent(const QString& url, const QString& eventId)
{
	theApp()->handleEvent(url, eventId);
}

/******************************************************************************
 * DCOP call from the alarm daemon to notify the success or failure of a
 * registration request from KAlarm.
 */
void DaemonGuiHandler::registered(bool reregister, int result)
{
	Daemon::registrationResult(reregister, result);
}

/******************************************************************************
* Register as a GUI with the alarm daemon.
*/
void DaemonGuiHandler::registerWith()
{
	kdDebug(5950) << "DaemonGuiHandler::registerWith()\n";
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.registerGui(kapp->aboutData()->appName(), NOTIFY_DCOP_OBJECT);
}


/*=============================================================================
=  Class: AlarmEnableAction
=============================================================================*/

AlarmEnableAction::AlarmEnableAction(int accel, QObject* parent, const char* name)
	: KToggleAction(QString::null, accel, parent, name),
	  mInitialised(false)
{
	setCheckedActual(false);    // set the correct text
	mInitialised = true;
}

/******************************************************************************
*  Set the checked status and the correct text for the Alarms Enabled action.
*/
void AlarmEnableAction::setCheckedActual(bool running)
{
	kdDebug(5950) << "AlarmEnableAction::setCheckedActual(" << running << ")\n";
	if (running != isChecked()  ||  !mInitialised)
	{
		setText(running ? i18n("&Alarms Enabled") : i18n("Enable &Alarms"));
		KToggleAction::setChecked(running);
		emit switched(running);
	}
}

/******************************************************************************
*  Request a change in the checked status.
*  The status is only actually changed when the alarm daemon run state changes.
*/
void AlarmEnableAction::setChecked(bool check)
{
	kdDebug(5950) << "AlarmEnableAction::setChecked(" << check << ")\n";
	if (check != isChecked())
	{
		if (check)
			Daemon::allowRegisterFailMsg();
		emit userClicked(check);
	}
}


/******************************************************************************
 * Expand a DCOP call parameter URL to a full URL.
 * (We must store full URLs in the calendar data since otherwise later calls to
 *  reload or remove calendars won't necessarily find a match.)
 */
QString expandURL(const QString& urlString)
{
	if (urlString.isEmpty())
		return QString();
	return KURL(urlString).url();
}
