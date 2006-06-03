/*
 *  daemon.cpp  -  interface with alarm daemon
 *  Program:  kalarm
 *  Copyright © 2001-2006 by David Jarvie <software@astrojar.org.uk>
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

#include <QTimer>
#include <QIcon>
#include <QByteArray>

#include <kstandarddirs.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <dcopclient.h>
#include <ktoolinvocation.h>
#include <kdebug.h>

#include "kalarmd/kalarmd.h"
#include "kalarmd/alarmdaemoniface.h"
#include "alarmdaemoniface_stub.h"
#include "kalarmd/alarmguiiface.h"

#include "alarmcalendar.h"
#include "alarmresources.h"
#include "kalarmapp.h"
#include "preferences.h"
#include "daemon.moc"


static const int    REGISTER_TIMEOUT = 20;     // seconds to wait before assuming registration with daemon has failed
static const char*  NOTIFY_DCOP_OBJECT  = "notify";    // DCOP name of KAlarm's interface for notification by alarm daemon


/*=============================================================================
=  Class: NotificationHandler
=  Handles the the alarm daemon's client notification DCOP interface.
=============================================================================*/

class NotificationHandler : public QObject, virtual public AlarmGuiIface
{
	public:
		NotificationHandler();
	private:
		// DCOP interface
		void  alarmDaemonUpdate(int calendarStatus);
		void  handleEvent(const QString& eventID);
		void  registered(bool reregister, int result);
		void  cacheDownloaded(const QString& resourceID);
};


Daemon*              Daemon::mInstance = 0;
NotificationHandler* Daemon::mDcopHandler = 0;
QList<QString>       Daemon::mQueuedEvents;
QList<QString>       Daemon::mSavingEvents;
QTimer*              Daemon::mStartTimer = 0;
QTimer*              Daemon::mRegisterTimer = 0;
QTimer*              Daemon::mStatusTimer = 0;
int                  Daemon::mStatusTimerCount = 0;
int                  Daemon::mStatusTimerInterval;
int                  Daemon::mStartTimeout = 0;
Daemon::Status       Daemon::mStatus = Daemon::STOPPED;
bool                 Daemon::mInitialised = false;
bool                 Daemon::mRunning = false;
bool                 Daemon::mCalendarDisabled = false;
bool                 Daemon::mEnableCalPending = false;
bool                 Daemon::mRegisterFailMsg = false;

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
	connect(AlarmResources::instance(), SIGNAL(resourceSaved(AlarmResource*)), mInstance, SLOT(slotCalendarSaved(AlarmResource*)));
	connect(AlarmResources::instance(), SIGNAL(resourceStatusChanged(AlarmResource*, AlarmResources::Change)), mInstance, SLOT(slotResourceStatusChanged(AlarmResource*, AlarmResources::Change)));
}

/******************************************************************************
* Initialise the daemon status timer.
*/
void Daemon::createDcopHandler()
{
	if (mDcopHandler)
		return;
	mDcopHandler = new NotificationHandler();
	// Check if the alarm daemon is running, but don't start it yet, since
	// the program is still initialising.
	mRunning = isRunning(false);

	mStatusTimerInterval = Preferences::daemonTrayCheckInterval();
	Preferences::connect(SIGNAL(preferencesChanged()), mInstance, SLOT(slotPreferencesChanged()));

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
	kDebug(5950) << "Daemon::start()\n";
	updateRegisteredStatus();
	switch (mStatus)
	{
		case STOPPED:
		{
			if (mStartTimer)
				return true;     // we're currently waiting for the daemon to start
			// Start the alarm daemon. It is a KUniqueApplication, which means that
			// there is automatically only one instance of the alarm daemon running.
			QString execStr = locate("exe", QLatin1String(DAEMON_APP_NAME));
			if (execStr.isEmpty())
			{
				KMessageBox::error(0, i18n("Alarm daemon not found."));
				kError() << "Daemon::startApp(): " DAEMON_APP_NAME " not found" << endl;
				return false;
			}
			KToolInvocation::kdeinitExec(execStr);
			kDebug(5950) << "Daemon::start(): Alarm daemon started" << endl;
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
* Register this application with the alarm daemon, and tell it to load the
* calendar.
* Set 'reregister' true in order to notify the daemon of a change in the
* 'disable alarms if stopped' setting.
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
	kDebug(5950) << (reregister ? "Daemon::reregisterWith(): " : "Daemon::registerWith(): ") << (disabledIfStopped ? "NO_START" : "COMMAND_LINE") << endl;
	QByteArray appname  = kapp->aboutData()->appName();
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	if (reregister)
		s.registerChange(appname, !disabledIfStopped);
	else
		s.registerApp(appname, QByteArray(NOTIFY_DCOP_OBJECT), !disabledIfStopped);
	if (!s.ok())
	{
		registrationResult(reregister, KAlarmd::FAILURE);
		return false;
	}
	mRegisterTimer = new QTimer(mInstance);
	connect(mRegisterTimer, SIGNAL(timeout()), mInstance, SLOT(registerTimerExpired()));
	mRegisterTimer->start(REGISTER_TIMEOUT * 1000);     // wait for the reply
	return true;
}

/******************************************************************************
* Called when the daemon has notified us of the result of the register() DCOP call.
*/
void Daemon::registrationResult(bool reregister, int result)
{
	kDebug(5950) << "Daemon::registrationResult(" << reregister << ")\n";
	delete mRegisterTimer;
	mRegisterTimer = 0;
	bool firstTime = !mInitialised;
	mInitialised = true;
	switch (result)
	{
		case KAlarmd::SUCCESS:
			break;
		case KAlarmd::NOT_FOUND:
			// We've successfully registered with the daemon, but the daemon can't
			// find the KAlarm executable so won't be able to restart KAlarm if
			// KAlarm exits.
			kError(5950) << "Daemon::registrationResult(" << reregister << "): registerApp dcop call: " << kapp->aboutData()->appName() << " not found\n";
			KMessageBox::error(0, i18n("Alarms will be disabled if you stop KAlarm.\n"
			                           "(Installation or configuration error: %1 cannot locate %2 executable.)",
			                            QLatin1String(DAEMON_APP_NAME),
			                            kapp->aboutData()->appName()));
			break;
		case KAlarmd::FAILURE:
		default:
			kError(5950) << "Daemon::registrationResult(" << reregister << "): registerApp dcop call failed -> " << result << endl;
			if (!reregister)
			{
				if (mStatus == REGISTERED)
					setStatus(READY);
				if (!mRegisterFailMsg)
				{
					mRegisterFailMsg = true;
					KMessageBox::error(0, i18n("Cannot enable alarms:\nFailed to register with Alarm Daemon (%1)",
					                            QLatin1String(DAEMON_APP_NAME)));
				}
			}
			if (firstTime)
			{
				// This is the first time we've tried to register with the
				// daemon, so notify the result. On success, setStatus() does the
				// notification, but we need to do it manually here on failure.
				emit mInstance->registered(false);
			}
			return;
	}

	if (!reregister)
	{
		// The alarm daemon has loaded the calendar
		setStatus(REGISTERED);
		mRegisterFailMsg = false;
		kDebug(5950) << "Daemon::start(): daemon startup complete" << endl;
	}
}

/******************************************************************************
* Check whether the alarm daemon has started yet, and if so, register with it.
*/
void Daemon::checkIfStarted()
{
	updateRegisteredStatus();
	bool err = false;
	switch (mStatus)
	{
		case STOPPED:
			if (--mStartTimeout > 0)
				return;     // wait a bit more to check again
			// Output error message, but delete timer first to prevent
			// multiple messages.
			err = true;
			break;
		case RUNNING:
		case READY:
		case REGISTERED:
			break;
	}
	delete mStartTimer;
	mStartTimer = 0;
	if (err)
	{
		kError(5950) << "Daemon::checkIfStarted(): failed to start daemon" << endl;
		KMessageBox::error(0, i18n("Cannot enable alarms:\nFailed to start Alarm Daemon (%1)", QLatin1String(DAEMON_APP_NAME)));
	}
}

/******************************************************************************
* Check whether the alarm daemon has started yet, and if so, whether it is
* ready to accept DCOP calls.
*/
void Daemon::updateRegisteredStatus(bool timeout)
{
	Status oldStatus = mStatus;
	if (!kapp->dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
	{
		setStatus(STOPPED);
		mRegisterFailMsg = false;
	}
	else
	{
		switch (mStatus)
		{
			case STOPPED:
				// The daemon has newly been detected as registered with DCOP.
				// Wait for a short time to ensure that it is ready for DCOP calls.
				setStatus(RUNNING);
				QTimer::singleShot(startCheckInterval, mInstance, SLOT(slotStarted()));
				break;
			case RUNNING:
				if (timeout)
				{
					setStatus(READY);
					start();
				}
				break;
			case READY:
			case REGISTERED:
				break;
		}
	}
	if (mStatus != oldStatus)
		kDebug(5950) << "Daemon::updateRegisteredStatus() -> " << (mStatus==STOPPED?"STOPPED":mStatus==RUNNING?"RUNNING":mStatus==READY?"READY":mStatus==REGISTERED?"REGISTERED":"??") << endl;
}

/******************************************************************************
* Set a new registration status. If appropriate, emit a signal.
*/
void Daemon::setStatus(Status newStatus)
{
	bool oldreg = (mStatus == REGISTERED);
	mStatus = newStatus;
	bool newreg = (newStatus != REGISTERED);
	if (newreg  &&  !oldreg
	||  !newreg  &&  oldreg)
	{
		// The status has toggled between REGISTERED and another state
		emit mInstance->registered(newreg);
	}
}

/******************************************************************************
* Connect the registered() signal to a slot.
*/
void Daemon::connectRegistered(QObject* receiver, const char* slot)
{
	connect(mInstance, SIGNAL(registered(bool)), receiver, slot);
}

/******************************************************************************
* Stop the alarm daemon if it is running.
*/
bool Daemon::stop()
{
	kDebug(5950) << "Daemon::stop()" << endl;
	if (kapp->dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
	{
		AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
		s.quit();
		if (!s.ok())
		{
			kError(5950) << "Daemon::stop(): dcop call failed" << endl;
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
	kDebug(5950) << "Daemon::reset()" << endl;
	if (!kapp->dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
		return false;
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.resetResource(QString());
	if (!s.ok())
		kError(5950) << "Daemon::reset(): resetResource dcop send failed" << endl;
	return true;
}

/******************************************************************************
* Tell the alarm daemon to reread all calendar resources.
*/
void Daemon::reload()
{
	kDebug(5950) << "Daemon::reload()\n";
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.reloadResource(QString());
	if (!s.ok())
		kError(5950) << "Daemon::reload(): reloadResource dcop send failed" << endl;
}

/******************************************************************************
* Tell the alarm daemon to reread one calendar resource.
*/
void Daemon::reloadResource(const QString& resourceID)
{
	kDebug(5950) << "Daemon::reloadResource(" << resourceID << ")\n";
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.reloadResource(resourceID);
	if (!s.ok())
		kError(5950) << "Daemon::reloadResource(): reloadResource(" << resourceID << ") dcop send failed" << endl;
}

/******************************************************************************
* Tell the alarm daemon to enable/disable monitoring of the calendar file.
*/
void Daemon::enableCalendar(bool enable)
{
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.enable(enable);
	mEnableCalPending = false;
}

/******************************************************************************
* Tell the alarm daemon to enable/disable autostart at login.
*/
void Daemon::enableAutoStart(bool enable)
{
	// Tell the alarm daemon in case it is running.
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.enableAutoStart(enable);
	if (!s.ok())
	{
		// Failure - the daemon probably isn't running, so rewrite its config file for it
		KConfig adconfig(locate("config", DAEMON_APP_NAME"rc"));
		adconfig.setGroup(DAEMON_AUTOSTART_SECTION);
		adconfig.writeEntry(DAEMON_AUTOSTART_KEY, enable);
		adconfig.sync();
	}
}

/******************************************************************************
* Read the alarm daemon's autostart-at-login setting.
*/
bool Daemon::autoStart(bool defaultAutoStart)
{
	KConfig adconfig(locate("config", DAEMON_APP_NAME"rc"));
	adconfig.setGroup(DAEMON_AUTOSTART_SECTION);
	return adconfig.readEntry(DAEMON_AUTOSTART_KEY, defaultAutoStart);
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
	kDebug(5950) << "Daemon::setAlarmsEnabled(" << enable << ")\n";
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
		mStatusTimer->start(mStatusTimerInterval * 1000);
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
		mStatusTimer->start(mStatusTimerInterval * 1000);   // exit from fast checking
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
	int newInterval = Preferences::daemonTrayCheckInterval();
	if (newInterval != mStatusTimerInterval)
	{
		// Daemon check interval has changed
		mStatusTimerInterval = newInterval;
		if (mStatusTimerCount <= 0)   // don't change if on fast rate
			mStatusTimer->start(mStatusTimerInterval * 1000);
	}
}

/******************************************************************************
* Create an "Alarms Enabled/Enable Alarms" action.
*/
AlarmEnableAction* Daemon::createAlarmEnableAction(KActionCollection* actions)
{
	AlarmEnableAction* a = new AlarmEnableAction(actions, QLatin1String("alEnable"));
	connect(a, SIGNAL(userClicked(bool)), mInstance, SLOT(setAlarmsEnabled(bool)));
	connect(mInstance, SIGNAL(daemonRunning(bool)), a, SLOT(setCheckedActual(bool)));
	return a;
}

/******************************************************************************
* Called when a resource has been saved.
* If it's the active alarm resource, notify the alarm daemon.
*/
void Daemon::slotResourceSaved(AlarmResource* resource)
{
	if (resource->alarmType() == AlarmResource::ACTIVE)
	{
		int n = mSavingEvents.count();
		if (n)
		{
			// We have just saved a modified event originally triggered by the daemon.
			// Notify the daemon of the event, and tell it to reload the calendar.
			for (int i = 0;  i < n - 1;  ++i)
				notifyEventHandled(mSavingEvents[i], false);
			notifyEventHandled(mSavingEvents[n - 1], true);
			mSavingEvents.clear();
		}
		else
			reloadResource(resource->identifier());
	}
}

/******************************************************************************
* Called when a resource's status has changed. Notify the alarm daemon.
*/
void Daemon::slotResourceStatusChanged(AlarmResource* resource, AlarmResources::Change change)
{
	switch (change)
	{
		case AlarmResources::Enabled:
		{
			AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
			s.resourceActive(resource->identifier(), resource->isActive());
			if (!s.ok())
				kError(5950) << "Daemon::slotResourceStatusChanged(): resourceActive dcop send failed" << endl;
			break;
		}
		case AlarmResources::Location:
		{
			QStringList locs = resource->location();
			if (locs.isEmpty())
				break;
			locs += QString();    // to avoid having to check index
			AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
			s.resourceLocation(resource->identifier(), locs[0], locs[1]);
			if (!s.ok())
				kError(5950) << "Daemon::slotResourceStatusChanged(): resourceLocation dcop send failed" << endl;
			break;
		}
		default:
			break;
	}
}

/******************************************************************************
* Note an event ID which has been triggered by the alarm daemon.
*/
void Daemon::queueEvent(const QString& eventId)
{
	mQueuedEvents += eventId;
}

/******************************************************************************
* Note an event ID which is currently being saved in the calendar file, if the
* event was originally triggered by the alarm daemon.
*/
void Daemon::savingEvent(const QString& eventId)
{
	int i = mQueuedEvents.indexOf(eventId);
	if (i >= 0)
	{
		mQueuedEvents.removeAt(i);
		mSavingEvents += eventId;
	}
}

/******************************************************************************
* If the event ID has been triggered by the alarm daemon, tell the daemon that
* it has been processed.
*/
void Daemon::eventHandled(const QString& eventId)
{
	int i = mQueuedEvents.indexOf(eventId);
	if (i >= 0)
	{
		mQueuedEvents.removeAt(i);
		notifyEventHandled(eventId, false);    // it's a daemon event, so tell daemon that it's been handled
	}
}

/******************************************************************************
* Tell the daemon that an event has been processed, and whether to reload its
* calendar.
*/
void Daemon::notifyEventHandled(const QString& eventId, bool reloadCal)
{
	kDebug(5950) << "Daemon::notifyEventHandled(" << eventId << (reloadCal ? "): reload" : ")") << endl;
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.eventHandled(eventId, reloadCal);
	if (!s.ok())
		kError(5950) << "Daemon::notifyEventHandled(): eventHandled dcop send failed" << endl;
}

/******************************************************************************
* Return the maximum time (in seconds) elapsed since the last time the alarm
* daemon must have checked alarms.
*/
int Daemon::maxTimeSinceCheck()
{
	return DAEMON_CHECK_INTERVAL;
}


/*=============================================================================
=  Class: NotificationHandler
=============================================================================*/

NotificationHandler::NotificationHandler()
	: DCOPObject(NOTIFY_DCOP_OBJECT), 
	  QObject()
{
	kDebug(5950) << "NotificationHandler::NotificationHandler()\n";
}

/******************************************************************************
 * DCOP call from the alarm daemon to notify a change.
 * The daemon notifies calendar statuses when we first register as a GUI, and whenever
 * a calendar status changes. So we don't need to read its config files.
 */
void NotificationHandler::alarmDaemonUpdate(int calendarStatus)
{
	kDebug(5950) << "NotificationHandler::alarmDaemonUpdate(" << calendarStatus << ")\n";
	KAlarmd::CalendarStatus status = KAlarmd::CalendarStatus(calendarStatus);
	bool enabled = false;
	switch (status)
	{
		case KAlarmd::CALENDAR_UNAVAILABLE:
			// Calendar is not available for monitoring
			kDebug(5950) << "NotificationHandler::alarmDaemonUpdate(CALENDAR_UNAVAILABLE)\n";
			break;
		case KAlarmd::CALENDAR_DISABLED:
			// Calendar is available for monitoring but is not currently being monitored
			kDebug(5950) << "NotificationHandler::alarmDaemonUpdate(DISABLE_CALENDAR)\n";
			break;
		case KAlarmd::CALENDAR_ENABLED:
			// Calendar is currently being monitored
			kDebug(5950) << "NotificationHandler::alarmDaemonUpdate(ENABLE_CALENDAR)\n";
			enabled = true;
			break;
		default:
			return;
	}
	Daemon::calendarIsEnabled(enabled);
}

/******************************************************************************
 * DCOP call to request that an alarm should be triggered if it is due.
 */
void NotificationHandler::handleEvent(const QString& eventId)
{
	QString id = eventId;
	if (id.startsWith(QLatin1String("ad:")))
	{
		// It's a notification from the alarm deamon
		id = id.mid(3);
		Daemon::queueEvent(id);
	}
	theApp()->dcopHandleEvent(id);
}

/******************************************************************************
 * DCOP call from the alarm daemon to notify the success or failure of a
 * registration request from KAlarm.
 */
void NotificationHandler::registered(bool reregister, int result)
{
	Daemon::registrationResult(reregister, result);
}

/******************************************************************************
 * DCOP call from the alarm daemon to notify that a remote resource's cache
 * has been downloaded.
 */
void NotificationHandler::cacheDownloaded(const QString& resourceID)
{
	AlarmCalendar::resources()->reloadFromCache(resourceID);
}


/*=============================================================================
=  Class: AlarmEnableAction
=============================================================================*/

AlarmEnableAction::AlarmEnableAction(KActionCollection* parent, const QString& name)
	: KToggleAction(parent, name),
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
	kDebug(5950) << "AlarmEnableAction::setCheckedActual(" << running << ")\n";
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
	kDebug(5950) << "AlarmEnableAction::setChecked(" << check << ")\n";
	if (check != isChecked())
	{
		if (check)
			Daemon::allowRegisterFailMsg();
		emit userClicked(check);
	}
}
