/*
 *  alarmdaemon.cpp  -  alarm daemon control routines
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  Copyright © 2001,2004-2007 by David Jarvie <software@astrojar.org.uk>
 *  Based on the original, (c) 1998, 1999 Preston Brown
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

#include "kalarmd.h"

#include <unistd.h>
#include <stdlib.h>

#include <QTimer>
#include <QByteArray>
#include <QtDBus>
#include <QProcess>
#include <kapplication.h>
#include <kstandarddirs.h>
#include <kconfig.h>
#include <ksystemtimezone.h>
#include <kdatetime.h>
#include <kdebug.h>

#include <kcal/calendarlocal.h>
#include <kcal/icalformat.h>
#include <ktoolinvocation.h>

#include "alarmguiiface.h"
#include "notifyinterface.h"
#include "resources/alarmresources.h"
#include "alarmdaemon.moc"


#ifdef AUTOSTART_KALARM
// Number of seconds to wait before autostarting KAlarm.
// Allow plenty of time for session restoration to happen first.
static const int KALARM_AUTOSTART_TIMEOUT = 30;
#endif

// Config file key strings
const char* CLIENT_GROUP     = "Client";
// Client data file key strings
const char* CLIENT_KEY       = "Client";
const char* DCOP_OBJECT_KEY  = "DCOP object";
const char* START_CLIENT_KEY = "Start";

static const char* KALARM_DBUS_SERVICE = "org.kde.kalarm";
static const char* NOTIFY_DBUS_OBJECT  = "/notify";    // D-Bus object path of KAlarm's notification interface


AlarmDaemon::EventsMap  AlarmDaemon::mEventsHandled;
AlarmDaemon::EventsMap  AlarmDaemon::mEventsPending;


AlarmDaemon::AlarmDaemon(bool autostart, QObject *parent)
	: QObject(parent),
	  mDBusNotify(0),
	  mAlarmTimer(0),
#ifdef AUTOSTART_KALARM
	  mAutoStarting(true),
#endif
	  mEnabled(true)
{
	kDebug(5900) <<"AlarmDaemon::AlarmDaemon()";
	QDBusConnection::sessionBus().registerObject(DAEMON_DBUS_OBJECT, this, QDBusConnection::ExportScriptableSlots);
	AlarmDaemon::readConfig();
	enableAutoStart(true);    // switch autostart on whenever the program is run

	/* Open the alarm resources, ignoring archived alarms and alarm templates.
	 * The alarm daemon is responsible for downloading remote resources (i.e. for updating
	 * their cache files), while KAlarm simply loads them from cache. This prevents useless
	 * duplication of potentially time-consuming downloads.
	 * Open to load active alarms only, and prevent resource changes being written to the
	 * config file.
	 */
	AlarmResources::setDebugArea(5902);
	AlarmResources* resources = AlarmResources::create(timeSpec(), true, true);
	resources->setNoGui(true);           // don't try to display messages, or we'll crash
	// The daemon is responsible for loading calendars (including downloading to cache for remote
	// resources), while KAlarm is responsible for all updates.
	resources->setInhibitSave(true);
	connect(resources, SIGNAL(resourceLoaded(AlarmResource*, bool)), SLOT(resourceLoaded(AlarmResource*)));
	resources->load();
	connect(resources, SIGNAL(cacheDownloaded(AlarmResource*)), SLOT(cacheDownloaded(AlarmResource*)));

#ifdef AUTOSTART_KALARM
	if (autostart)
	{
		/* The alarm daemon is being autostarted.
		 * Check if KAlarm needs to be autostarted in the system tray.
		 * This should ideally be handled internally by KAlarm, but is done by kalarmd
		 * for the following reason:
		 * KAlarm needs to be both session restored and autostarted, but KDE doesn't
		 * currently cater properly for this - there is no guarantee that the session
		 * restoration activation will come before the autostart activation. If they
		 * come in the wrong order, KAlarm won't know that it is supposed to restore
		 * itself and instead will simply open a new window.
		 */
		KConfig kaconfig(KStandardDirs::locate("config", "kalarmrc"));
		const KConfigGroup group = kaconfig.group("General");
		autostart = group.readEntry("AutostartTray", false);
		if (autostart)
		{
			kDebug(5900) <<"AlarmDaemon::AlarmDaemon(): wait to autostart KAlarm";
			QTimer::singleShot(KALARM_AUTOSTART_TIMEOUT * 1000, this, SLOT(autostartKAlarm()));
		}
	}
	if (!autostart)
	{
		mAutoStarting = false;
		startMonitoring();    // otherwise, start monitoring alarms now
	}
#else
	startMonitoring();    // otherwise, start monitoring alarms now
#endif
}

AlarmDaemon::~AlarmDaemon()
{
	delete mDBusNotify;
}

/******************************************************************************
* D-Bus call to quit the program.
*/
void AlarmDaemon::quit()
{
	kDebug(5900) <<"AlarmDaemon::quit()";
	exit(0);
}

OrgKdeKalarmNotifyInterface* AlarmDaemon::kalarmNotifyDBus()
{
	if (!mDBusNotify)
		mDBusNotify = new org::kde::kalarm::notify(KALARM_DBUS_SERVICE, NOTIFY_DBUS_OBJECT, QDBusConnection::sessionBus());
	return mDBusNotify;
}

/******************************************************************************
* Check for and handle any D-Bus error on the last operation.
* Reply = true if ok, false if error.
*/
bool AlarmDaemon::checkDBusResult(const char* funcname)
{
	QDBusError err = mDBusNotify->lastError();
	if (!err.isValid())
		return true;    // no error
	kError(5950) <<"AlarmDaemon:" << funcname <<"() D-Bus call failed:" << err.message();
	return false;
}

/******************************************************************************
* Called after a timer delay to autostart KAlarm in the system tray.
*/
void AlarmDaemon::autostartKAlarm()
{
#ifdef AUTOSTART_KALARM
	if (mAlarmTimer)
	{
		kDebug(5900) <<"AlarmDaemon::autostartKAlarm(): KAlarm already registered";
		return;    // KAlarm has already registered with us
	}
	kDebug(5900) <<"AlarmDaemon::autostartKAlarm(): starting KAlarm";
	QStringList args;
	args << QLatin1String("--tray");
	KToolInvocation::kdeinitExec(QLatin1String("kalarm"), args);

	mAutoStarting = false;
	startMonitoring();
#endif
}

/******************************************************************************
* Start monitoring alarms.
*/
void AlarmDaemon::startMonitoring()
{
#ifdef AUTOSTART_KALARM
	if (mAutoStarting)
		return;
#endif
	if (mClientName.isEmpty())
		return;

	// Set up the alarm timer
	if (!mAlarmTimer)
	{
		mAlarmTimer = new QTimer(this);
		connect(mAlarmTimer, SIGNAL(timeout()), SLOT(checkAlarmsSlot()));
	}
	setTimerStatus();

	// Start monitoring alarms.
	checkAlarms();
}

/******************************************************************************
* D-Bus call to enable or disable alarm monitoring.
*/
void AlarmDaemon::enable(bool enable)
{
	kDebug(5900) <<"AlarmDaemon::enable()";
	mEnabled = enable;
	notifyCalStatus();    // notify KAlarm
}

/******************************************************************************
* D-Bus call to tell the daemon that the active status of a resource has changed.
* This shouldn't be needed, but KRES::ManagerObserver::resourceModified()
* which is called when KAlarm has changed the status, doesn't report the new
* status when it's called in kalarmd. Crap!
*/
void AlarmDaemon::resourceActive(const QString& id, bool active)
{
	AlarmResource* resource = AlarmResources::instance()->resourceWithId(id);
	if (resource  &&  active != resource->isActive())
	{
		kDebug(5900) <<"AlarmDaemon::resourceActive(" << id <<"," << active <<")";
		resource->setEnabled(active);
		if (active)
			reloadResource(resource, true);
		else
			resource->close();
	}
}

void AlarmDaemon::resourceLocation(const QString& id, const QString& locn, const QString& locn2)
{
	AlarmResource* resource = AlarmResources::instance()->resourceWithId(id);
	if (resource)
	{
		kDebug(5900) <<"AlarmDaemon::resourceLocation(" << id <<"," << locn <<")";
		resource->setLocation(locn, locn2);
	}
}

/******************************************************************************
* D-Bus call to reload, and optionally reset, the specified resource or all
* resources.
* If 'reset' is true, the data associated with the resource is reset.
*/
void AlarmDaemon::reloadResource(const QString& id, bool check, bool reset)
{
// FIXME: I don't think this check is possible with dbus
#if 0
	if (check  &&  kapp->dcopClient()->senderId() != mClientName)
		return;
#endif
	AlarmResources* resources = AlarmResources::instance();
	if (id.isEmpty())
	{
		// Reload all resources
		kDebug(5900) <<"AlarmDaemon::reloadResource(ALL)";
		if (reset)
			clearEventsHandled();
		// Don't call reload() since that saves the calendar
		resources->load();
	}
	else
	{
		kDebug(5900) <<"AlarmDaemon::reloadResource(" << id <<")";
		AlarmResource* resource = resources->resourceWithId(id);
		if (resource  &&  resource->isActive())
			reloadResource(resource, reset);
		else
			kError(5900) <<"AlarmDaemon::reloadResource(" << id <<"): active resource not found";
	}
}

/******************************************************************************
* Reload, and optionally reset, the specified resource.
* If 'reset' is true, the data associated with the resource is reset.
*/
void AlarmDaemon::reloadResource(AlarmResource* resource, bool reset)
{
	kDebug(5900) <<"AlarmDaemon::reloadResource()";
	if (reset)
		clearEventsHandled(resource);
	// Don't call reload() since that saves the calendar.
	// For remote resources, we don't need to download them since KAlarm
	// has just updated the cache. So just load from cache.
	resource->load(KCal::ResourceCached::NoSyncCache);
}

/******************************************************************************
*  Called when a remote resource's cache has completed downloading.
*  Tell KAlarm.
*/
void AlarmDaemon::cacheDownloaded(AlarmResource* resource)
{
	kalarmNotifyDBus()->cacheDownloaded(resource->identifier());
	checkDBusResult("cacheDownloaded");
	kDebug(5900) <<"AlarmDaemon::cacheDownloaded(" << resource->identifier() <<")";
}

/******************************************************************************
*  Called when a resource has completed loading.
*/
void AlarmDaemon::resourceLoaded(AlarmResource* res)
{
	kDebug(5900) <<"Resource" << res->identifier() <<" (" << res->resourceName() <<") loaded";
	clearEventsHandled(res, true);   // remove all its events which no longer exist from handled list
	notifyCalStatus();       // notify KAlarm
	setTimerStatus();
	checkAlarms();
}

/******************************************************************************
* D-Bus call to notify the daemon that an event has been handled, and optionally
* to tell it to reload the resource containing the event.
*/
void AlarmDaemon::eventHandled(const QString& eventID, bool reload)
{
#ifdef __GNUC__
#warning Check client ID
#endif
// FIXME I don't think this check can be done with DBus
#if 0
	if (kapp->dcopClient()->senderId() != mClientName)
		return;
#endif
	kDebug(5900) <<"AlarmDaemon::eventHandled()" << (reload ?": reload" :"");
	setEventHandled(eventID);
	if (reload)
	{
		AlarmResource* resource = AlarmResources::instance()->resourceForIncidence(eventID);
		if (resource)
			reloadResource(resource, false);
	}
}

/******************************************************************************
* D-Bus call to register an application as the client application, and write it
* to the config file.
* N.B. This method must not return a bool because DCOPClient::call() can cause
*      a hang if the daemon happens to send a notification to KAlarm at the
*      same time as KAlarm calls this D-Bus method.
*/
void AlarmDaemon::registerApp(const QString& appName, const QString& serviceName, const QString& dbusObject, bool startClient)
{
	kDebug(5900) <<"AlarmDaemon::registerApp(" << appName <<"," << serviceName <<"," <<  dbusObject <<"," << startClient <<")";
	registerApp(appName, serviceName, dbusObject, startClient, true);
}

/******************************************************************************
* D-Bus call to change whether KAlarm should be started when an event needs to
* be notified to it.
* N.B. This method must not return a bool because DCOPClient::call() can cause
*      a hang if the daemon happens to send a notification to KAlarm at the
*      same time as KAlarm calls this DCCOP method.
*/
void AlarmDaemon::registerChange(const QString& appName, const QString& serviceName, bool startClient)
{
	kDebug(5900) <<"AlarmDaemon::registerChange(" << serviceName <<"," << startClient <<")";
	if (serviceName == mClientName)
		registerApp(appName, mClientName, mClientDBusObj, startClient, false);
}

/******************************************************************************
* D-Bus call to register an application as the client application, and write it
* to the config file.
* N.B. This method must not return a bool because DCOPClient::call() can cause
*      a hang if the daemon happens to send a notification to KAlarm at the
*      same time as KAlarm calls this D-Bus method.
*/
void AlarmDaemon::registerApp(const QString& appName, const QString& serviceName, const QString& dbusObject, bool startClient, bool init)
{
	kDebug(5900) <<"AlarmDaemon::registerApp(" << appName <<"," << serviceName <<"," <<  dbusObject <<"," << startClient <<")";
	KAlarmd::RegisterResult result = KAlarmd::SUCCESS;
	if (serviceName.isEmpty())
		result = KAlarmd::FAILURE;
	else if (startClient)
	{
		QString exe = KStandardDirs::findExe(appName);
		if (exe.isNull())
		{
			kError(5900) <<"AlarmDaemon::registerApp(): '" << appName <<"' not found";
			result = KAlarmd::NOT_FOUND;
		}
		mClientExe = exe;
	}
	if (result == KAlarmd::SUCCESS)
	{
		mClientStart   = startClient;
		mClientName    = serviceName;
		mClientDBusObj = dbusObject;
		mClientStart   = startClient;
		KConfigGroup config(KGlobal::config(), CLIENT_GROUP);
		config.writeEntry(CLIENT_KEY, mClientName);
		config.writeEntry(DCOP_OBJECT_KEY, mClientDBusObj);
		config.writeEntry(START_CLIENT_KEY, mClientStart);
		if (init)
			enableAutoStart(true, false);
		config.sync();
		if (init)
		{
			setTimerStatus();
			notifyCalStatus();
		}
	}

	// Notify the client of whether the call succeeded.
	kalarmNotifyDBus()->registered(false, result);
	checkDBusResult("registered");
	kDebug(5900) <<"AlarmDaemon::registerApp() ->" << result;
}

/******************************************************************************
* D-Bus call to set autostart at login on or off.
*/
void AlarmDaemon::enableAutoStart(bool on, bool sync)
{
        kDebug(5900) <<"AlarmDaemon::enableAutoStart(" << on <<")";
        KSharedConfig::Ptr config = KGlobal::config();
	config->reparseConfiguration();
	KConfigGroup group(config, DAEMON_AUTOSTART_SECTION);
        group.writeEntry(DAEMON_AUTOSTART_KEY, on);
	if (sync)
		config->sync();
}

/******************************************************************************
* Check if any alarms are pending for any enabled calendar, and display the
* pending alarms.
* Called by the alarm timer.
*/
void AlarmDaemon::checkAlarmsSlot()
{
	kDebug(5901) <<"AlarmDaemon::checkAlarmsSlot()";
	if (mAlarmTimerSyncing)
	{
		// We've synched to the minute boundary. Now set timer to the check interval.
		mAlarmTimer->start(DAEMON_CHECK_INTERVAL * 1000);
		mAlarmTimerSyncing = false;
		mAlarmTimerSyncCount = 10;    // resynch every 10 minutes, in case of glitches
	}
	else if (--mAlarmTimerSyncCount <= 0)
	{
		int interval = DAEMON_CHECK_INTERVAL + 1 - QTime::currentTime().second();
		if (interval < DAEMON_CHECK_INTERVAL - 1)
		{
			// Need to re-synch to 1 second past the minute
			mAlarmTimer->start(interval * 1000);
			mAlarmTimerSyncing = true;
			kDebug(5900) <<"Resynching alarm timer";
		}
		else
			mAlarmTimerSyncCount = 10;
	}
	checkAlarms();
}

/******************************************************************************
* Check if any alarms are pending, and trigger the pending alarms.
*/
void AlarmDaemon::checkAlarms()
{
	kDebug(5901) <<"AlarmDaemon::checkAlarms()";
	AlarmResources* resources = AlarmResources::instance();
	if (!mEnabled  ||  !resources->loadedState(AlarmResource::ACTIVE))
		return;

	KDateTime now  = KDateTime::currentUtcDateTime();
	KDateTime now1 = now.addSecs(1);
	kDebug(5901) <<"  To:" << now;
	QList<KCal::Alarm*> alarms = resources->alarmsTo(now);
	if (alarms.isEmpty())
		return;
	QList<KCal::Event*> eventsDone;
	for (int i = 0, end = alarms.count();  i < end;  ++i)
	{
		KCal::Event* event = dynamic_cast<KCal::Event*>(alarms[i]->parent());
		if (!event  ||  eventsDone.contains(event))
			continue;   // either not an event, or the event has already been processed
		eventsDone += event;
		const QString& eventID = event->uid();
		kDebug(5901) <<"AlarmDaemon::checkAlarms(): event" << eventID;

		// Check which of the alarms for this event are due.
		// The times in 'alarmtimes' corresponding to due alarms are set.
		// The times for non-due alarms are set invalid in 'alarmtimes'.
		bool recurs = event->recurs();
		QStringList flags = event->customProperty("KALARM", "FLAGS").split(QLatin1Char(';'), QString::SkipEmptyParts);
		bool floats = flags.contains(QString::fromLatin1("DATE"));
		KDateTime nextDateTime = event->dtStart();
		nextDateTime.setDateOnly(floats);
		if (recurs)
		{
			QString prop = event->customProperty("KALARM", "NEXTRECUR");
			if (prop.length() >= 8)
			{
				// The next due recurrence time is specified
				QDate d(prop.left(4).toInt(), prop.mid(4,2).toInt(), prop.mid(6,2).toInt());
				if (d.isValid())
				{
					if (floats  &&  prop.length() == 8)
						nextDateTime.setDate(d);
					else if (!floats  &&  prop.length() == 15  &&  prop[8] == QChar('T'))
					{
						QTime t(prop.mid(9,2).toInt(), prop.mid(11,2).toInt(), prop.mid(13,2).toInt());
						if (t.isValid())
						{
							nextDateTime.setDate(d);
							nextDateTime.setTime(t);
						}
					}
				}
			}
		}
		QList<KDateTime> alarmtimes;
		KCal::Alarm::List alarms = event->alarms();
		for (KCal::Alarm::List::ConstIterator al = alarms.begin();  al != alarms.end();  ++al)
		{
			KCal::Alarm* alarm = *al;
			KDateTime dt;
			if (alarm->enabled())
			{
				KDateTime dt1;
				if (recurs  &&  !alarm->hasTime())
				{
					// Find the latest recurrence for the alarm.
					// Need to do this for alarms with offsets in order to detect
					// reminders due for recurrences.
					int offset = alarm->hasStartOffset() ? alarm->startOffset().asSeconds()
					           : alarm->endOffset().asSeconds() + event->dtStart().secsTo(event->dtEnd());
					if (offset)
					{
						dt1 = nextDateTime.addSecs(offset);
						if (dt1 > now)
							dt1 = KDateTime();
					}
				}
				// Get latest due repetition, or the recurrence time if none
				dt = nextDateTime;
				if (nextDateTime <= now  &&  alarm->repeatCount() > 0)
				{
					int snoozeSecs = alarm->snoozeTime() * 60;
					int repetition = nextDateTime.secsTo_long(now) / snoozeSecs;
					if (repetition > alarm->repeatCount())
						repetition = alarm->repeatCount();
					dt = nextDateTime.addSecs(repetition * snoozeSecs);
				}
				if (!dt.isValid()  ||  dt > now
				||  dt1.isValid()  &&  dt1 > dt)  // already tested dt1 <= now
					dt = dt1;
			}
			alarmtimes.append(dt);
		}
		if (!eventHandled(event, alarmtimes))
			notifyEvent(eventID, event, alarmtimes);
	}
}

/******************************************************************************
* If not already handled, send a D-Bus message to KAlarm telling it that an
* alarm should now be handled.
*/
void AlarmDaemon::notifyEvent(const QString& eventID, const KCal::Event* event, const QList<KDateTime>& alarmtimes)
{
	kDebug(5900) <<"AlarmDaemon::notifyEvent(" << eventID <<"): notification type=" << mClientStart;
	QString id = QLatin1String("ad:") + eventID;    // prefix to indicate that the notification if from the daemon

	// Check if the client application is running and ready to receive notification
	bool registered = isClientRegistered();
	bool ready = registered;
	if (registered)
	{
		// It's running, but check if it has created our D-Bus interface yet
#ifdef __GNUC__
#warning Check if KAlarm D-Bus interface has been created yet
#endif
#if 0
		DCOPCStringList objects = kapp->dcopClient()->remoteObjects(mClientName);
		if (objects.indexOf(mClientDBusObj) < 0)
			ready = false;
#endif
	}
	if (!ready)
	{
		// KAlarm is not running, or is not yet ready to receive notifications.
		if (!mClientStart)
		{
			if (registered)
				kDebug(5900) <<"AlarmDaemon::notifyEvent(): client not ready";
			else
				kDebug(5900) <<"AlarmDaemon::notifyEvent(): don't start client";
			return;
		}

		// Start KAlarm, using the command line to specify the alarm
		if (mClientExe.isEmpty())
		{
			kDebug(5900) <<"AlarmDaemon::notifyEvent(): '" << mClientName <<"' not found";
			return;
		}
		QStringList lst;
		lst << "--handleEvent" << id;
		QProcess::startDetached(mClientExe,lst);
		kDebug(5900) <<"AlarmDaemon::notifyEvent(): used command line";
	}
	else
	{
		// Notify the client by telling it the event ID
		kalarmNotifyDBus()->handleEvent(id);
		if (!checkDBusResult("handleEvent"))
			return;
	}
	setEventPending(event, alarmtimes);
}

/******************************************************************************
* Starts or stops the alarm timer as necessary after the calendar is enabled/disabled.
*/
void AlarmDaemon::setTimerStatus()
{
#ifdef AUTOSTART_KALARM
	if (mAutoStarting)
		return;
        if (!mAlarmTimer)
        {
                // KAlarm is now running, so start monitoring alarms
		startMonitoring();
		return;    // startMonitoring() calls this method
	}

#endif
	// Start or stop the alarm timer if necessary
	bool loaded = AlarmResources::instance()->loadedState(AlarmResource::ACTIVE);
	if (!mAlarmTimer->isActive()  &&  loaded)
	{
		// Timeout every minute.
		// But first synchronize to one second after the minute boundary.
		int firstInterval = DAEMON_CHECK_INTERVAL + 1 - QTime::currentTime().second();
		mAlarmTimer->start(1000 * firstInterval);
		mAlarmTimerSyncing = (firstInterval != DAEMON_CHECK_INTERVAL);
		kDebug(5900) <<"Started alarm timer";
	}
	else if (mAlarmTimer->isActive()  &&  !loaded)
	{
		mAlarmTimer->stop();
		kDebug(5900) <<"Stopped alarm timer";
	}
}

/******************************************************************************
* Send a D-Bus message to the client, notifying it of a change in calendar status.
*/
void AlarmDaemon::notifyCalStatus()
{
	if (mClientName.isEmpty())
		return;
	if (isClientRegistered())
	{
		bool unloaded = !AlarmResources::instance()->loadedState(AlarmResource::ACTIVE);   // if no resources are loaded
		KAlarmd::CalendarStatus change = unloaded ? KAlarmd::CALENDAR_UNAVAILABLE
		                               : mEnabled ? KAlarmd::CALENDAR_ENABLED : KAlarmd::CALENDAR_DISABLED;
		kDebug(5900) <<"AlarmDaemon::notifyCalStatus() sending:" << mClientName <<" ->" << change;
		kalarmNotifyDBus()->alarmDaemonUpdate(change);
		checkDBusResult("alarmDaemonUpdate");
	}
}

/******************************************************************************
* Check whether all the alarms for the event with the given ID have already
* been handled for this client.
*/
bool AlarmDaemon::eventHandled(const KCal::Event* event, const QList<KDateTime>& alarmtimes)
{
	EventsMap::ConstIterator it = mEventsHandled.find(event->uid());
	if (it == mEventsHandled.end())
		return false;

	int oldCount = it.value().alarmTimes.count();
	int count = alarmtimes.count();
	for (int i = 0;  i < count;  ++i)
	{
		if (alarmtimes[i].isValid()
		&&  (i >= oldCount                             // is it an additional alarm?
		     || !it.value().alarmTimes[i].isValid()     // or has it just become due?
		     || it.value().alarmTimes[i].isValid()      // or has it changed?
		        && alarmtimes[i] != it.value().alarmTimes[i]))
			return false;     // this alarm has changed
	}
	return true;
}

/******************************************************************************
* Remember that the event with the given ID has been handled for this client.
* It must already be in the pending list.
*/
void AlarmDaemon::setEventHandled(const QString& eventID)
{
	kDebug(5900) <<"AlarmDaemon::setEventHandled(" << eventID <<")";
	// Remove it from the pending list, and add it to the handled list
	EventsMap::Iterator it = mEventsPending.find(eventID);
	if (it != mEventsPending.end())
	{
		setEventInMap(mEventsHandled, eventID, it.value().alarmTimes, it.value().eventSequence);
		mEventsPending.erase(it);
	}
}

/******************************************************************************
* Remember that the specified alarms for the event with the given ID have been
* notified to KAlarm, but no reply has come back yet.
*/
void AlarmDaemon::setEventPending(const KCal::Event* event, const QList<KDateTime>& alarmtimes)
{
	if (event)
	{
		kDebug(5900) <<"AlarmDaemon::setEventPending(" << event->uid() <<")";
		setEventInMap(mEventsPending, event->uid(), alarmtimes, event->revision());
	}
}

/******************************************************************************
* Add a specified entry to the events pending or handled list.
*/
void AlarmDaemon::setEventInMap(EventsMap& map, const QString& eventID, const QList<KDateTime>& alarmtimes, int sequence)
{
	EventsMap::Iterator it = map.find(eventID);
	if (it != map.end())
	{
		// Update the existing entry for the event
		it.value().alarmTimes = alarmtimes;
		it.value().eventSequence = sequence;
	}
	else
		map.insert(eventID, EventItem(sequence, alarmtimes));
}

/******************************************************************************
* Clear all memory of events pending or handled for this client.
*/
void AlarmDaemon::clearEventsHandled(AlarmResource* resource, bool nonexistentOnly)
{
	clearEventMap(mEventsPending, resource, nonexistentOnly);
	clearEventMap(mEventsHandled, resource, nonexistentOnly);
}

/******************************************************************************
* Clear either the events pending or events handled list for this client.
* If 'nonexistentOnly' is true, only events which no longer exist are cleared.
*/
void AlarmDaemon::clearEventMap(EventsMap& map, AlarmResource* resource, bool nonexistentOnly)
{
	if (!resource  &&  !nonexistentOnly)
		map.clear();
	else
	{
		AlarmResources* resources = AlarmResources::instance();
		for (EventsMap::Iterator it = map.begin();  it != map.end();  )
		{
			const KCal::Event* evnt = resources->event(it.key());
			if (!evnt
			||  (!nonexistentOnly  &&  (!resource || resources->resource(evnt) == resource)))
				it = map.erase(it);
			else
				++it;
		}
	}
}

/******************************************************************************
* Read the client information from the configuration file.
*/
void AlarmDaemon::readConfig()
{
	KConfigGroup config(KGlobal::config(), CLIENT_GROUP);
	QString client = config.readEntry(CLIENT_KEY);
	mClientDBusObj = config.readEntry(DCOP_OBJECT_KEY).toLocal8Bit();
	mClientStart   = config.readEntry(START_CLIENT_KEY, false);

	// Verify the configuration
	mClientName.clear();
	if (client.isEmpty()  ||  KStandardDirs::findExe(client).isNull())
		kError(5900) <<"AlarmDaemon::readConfig(): '" << client <<"': client app not found";
	else if (mClientDBusObj.isEmpty())
		kError(5900) <<"AlarmDaemon::readConfig(): no D-Bus object specified for '" << client <<"'";
	else
	{
		mClientName = client;
		kDebug(5900) <<"AlarmDaemon::readConfig(): client" << mClientName;
	}

	// Remove obsolete CheckInterval entry (if it exists)
        config.changeGroup("General");
	config.deleteEntry("CheckInterval");
	config.sync();
}

/******************************************************************************
* Read the timezone to use. Try to read it from KAlarm's config file. If the
* entry there is blank, use local clock time.
*/
KDateTime::Spec AlarmDaemon::timeSpec()
{
	KConfig kaconfig(KStandardDirs::locate("config", "kalarmrc"));
	const KConfigGroup group = kaconfig.group("General");
	QString zone = group.readEntry("Timezone", QString());
	if (zone.isEmpty())
		return KDateTime::ClockTime;
	KTimeZone tz = KSystemTimeZones::zone(zone);
	return tz.isValid() ? tz : KSystemTimeZones::local();
}

/******************************************************************************
* Checks whether the client application is running.
*/
bool AlarmDaemon::isClientRegistered() const
{
	QDBusReply<bool> isRegistered = QDBusConnection::sessionBus().interface()->isServiceRegistered(mClientName);
	return isRegistered.isValid() ? isRegistered.value() : false;
}
