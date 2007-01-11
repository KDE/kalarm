/*
 *  alarmdaemon.cpp  -  alarm daemon control routines
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  Copyright © 2001,2004-2006 by David Jarvie <software@astrojar.org.uk>
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

#include <kapplication.h>
#include <kstandarddirs.h>
#include <kprocess.h>
#include <kconfig.h>
#include <ksystemtimezone.h>
#include <kdatetime.h>
#include <kdebug.h>

#include <kcal/calendarlocal.h>
#include <kcal/icalformat.h>
#include <ktoolinvocation.h>

#include "alarmguiiface.h"
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
static const char* NOTIFY_DBUS_IFACE   = "org.kde.kalarm.notify";
static const char* NOTIFY_DBUS_OBJECT  = "/notify";    // D-Bus object path of KAlarm's notification interface


AlarmDaemon::EventsMap  AlarmDaemon::mEventsHandled;
AlarmDaemon::EventsMap  AlarmDaemon::mEventsPending;


AlarmDaemon::AlarmDaemon(bool autostart, QObject *parent)
	: QObject(parent),
	  mDBusNotify(0),
	  mAlarmTimer(0),
	  mEnabled(true)
{
	kDebug(5900) << "AlarmDaemon::AlarmDaemon()" << endl;
	QDBusConnection::sessionBus().registerObject(DAEMON_DBUS_OBJECT, this, QDBusConnection::ExportScriptableSlots);
	AlarmDaemon::readConfig();
	enableAutoStart(true);    // switch autostart on whenever the program is run

	// Open the alarm resources, ignoring archived alarms and alarm templates.
	// The alarm daemon is responsible for downloading remote resources (i.e. for updating
	// their cache files), while KAlarm simply loads them from cache. This prevents useless
	// duplication of potentially time-consuming downloads.
	AlarmResources::setDebugArea(5902);
	AlarmResources* resources = AlarmResources::create(timeSpec(), true);   // load active alarms only
	resources->setPassiveClient(true);   // prevent resource changes being written to config file
	resources->setNoGui(true);           // dont' try to display messages, or we'll crash
	// The daemon is responsible for loading calendars (including downloading to cache for remote
	// resources), which KAlarm is responsible for all updates.
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
		kaconfig.setGroup("General");
		autostart = kaconfig.readEntry("AutostartTray", false);
		if (autostart)
		{
			kDebug(5900) << "AlarmDaemon::AlarmDaemon(): wait to autostart KAlarm\n";
			QTimer::singleShot(KALARM_AUTOSTART_TIMEOUT * 1000, this, SLOT(autostartKAlarm()));
		}
	}
	if (!autostart)
#endif
		startMonitoring();    // otherwise, start monitoring alarms now
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
	kDebug(5900) << "AlarmDaemon::quit()" << endl;
	exit(0);
}

/******************************************************************************
* Send a notification to KAlarm, without waiting for a reply.
*/
bool AlarmDaemon::kalarmNotify(const QString& method, const QList<QVariant>& args)
{
	if (!mDBusNotify)
		mDBusNotify = new QDBusInterface(KALARM_DBUS_SERVICE, NOTIFY_DBUS_OBJECT, NOTIFY_DBUS_IFACE);
	QDBusError err = mDBusNotify->callWithArgumentList(QDBus::NoBlock, method, args);
	if (err.isValid())
	{
		kError(5900) << "AlarmDaemon::kalarmNotify(" << method << "): D-Bus call failed: " << err.message() << endl;
		return false;
	}
	return true;
}

/******************************************************************************
* Called after a timer delay to autostart KAlarm in the system tray.
*/
void AlarmDaemon::autostartKAlarm()
{
#ifdef AUTOSTART_KALARM
	if (mAlarmTimer)
	{
		kDebug(5900) << "AlarmDaemon::autostartKAlarm(): KAlarm already registered\n";
		return;    // KAlarm has already registered with us
	}
	kDebug(5900) << "AlarmDaemon::autostartKAlarm(): starting KAlarm\n";
	QStringList args;
	args << QLatin1String("--tray");
	KToolInvocation::kdeinitExec(QLatin1String("kalarm"), args);

	startMonitoring();
#endif
}

/******************************************************************************
* Start monitoring alarms.
*/
void AlarmDaemon::startMonitoring()
{
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
	kDebug(5900) << "AlarmDaemon::enable()" << endl;
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
		kDebug(5900) << "AlarmDaemon::resourceActive(" << id << ", " << active << ")" << endl;
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
		kDebug(5900) << "AlarmDaemon::resourceLocation(" << id << ", " << locn << ")" << endl;
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
		kDebug(5900) << "AlarmDaemon::reloadResource(ALL)" << endl;
		if (reset)
			clearEventsHandled();
		// Don't call reload() since that saves the calendar
		resources->load();
	}
	else
	{
		kDebug(5900) << "AlarmDaemon::reloadResource(" << id << ")" << endl;
		AlarmResource* resource = resources->resourceWithId(id);
		if (resource  &&  resource->isActive())
			reloadResource(resource, reset);
		else
			kError(5900) << "AlarmDaemon::reloadResource(" << id << "): active resource not found" << endl;
	}
}

/******************************************************************************
* Reload, and optionally reset, the specified resource.
* If 'reset' is true, the data associated with the resource is reset.
*/
void AlarmDaemon::reloadResource(AlarmResource* resource, bool reset)
{
	kDebug(5900) << "AlarmDaemon::reloadResource()" << endl;
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
	QList<QVariant> args;
	args << resource->identifier();
	kalarmNotify(QLatin1String("cacheDownloaded"), args);
	kDebug(5900) << "AlarmDaemon::cacheDownloaded(" << resource->identifier() << ")\n";
}

/******************************************************************************
*  Called when a resource has completed loading.
*/
void AlarmDaemon::resourceLoaded(AlarmResource* res)
{
	kDebug(5900) << "Resource " << res->identifier() << " (" << res->resourceName() << ") loaded" << endl;
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
	kDebug(5900) << "AlarmDaemon::eventHandled()" << (reload ? ": reload" : "") << endl;
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
void AlarmDaemon::registerApp(const QString& appName, const QString& dbusObject, bool startClient)
{
	kDebug(5900) << "AlarmDaemon::registerApp(" << appName << ", " <<  dbusObject << ", " << startClient << ")" << endl;
	registerApp(appName, dbusObject, startClient, true);
}

/******************************************************************************
* D-Bus call to change whether KAlarm should be started when an event needs to
* be notified to it.
* N.B. This method must not return a bool because DCOPClient::call() can cause
*      a hang if the daemon happens to send a notification to KAlarm at the
*      same time as KAlarm calls this DCCOP method.
*/
void AlarmDaemon::registerChange(const QString& appName, bool startClient)
{
	kDebug(5900) << "AlarmDaemon::registerChange(" << appName << ", " << startClient << ")" << endl;
	registerApp(mClientName, mClientDBusObj, startClient, false);
}

/******************************************************************************
* D-Bus call to register an application as the client application, and write it
* to the config file.
* N.B. This method must not return a bool because DCOPClient::call() can cause
*      a hang if the daemon happens to send a notification to KAlarm at the
*      same time as KAlarm calls this D-Bus method.
*/
void AlarmDaemon::registerApp(const QString& appName, const QString& dbusObject, bool startClient, bool init)
{
	kDebug(5900) << "AlarmDaemon::registerApp(" << appName << ", " <<  dbusObject << ", " << startClient << ")" << endl;
	KAlarmd::RegisterResult result = KAlarmd::SUCCESS;
	if (appName.isEmpty())
		result = KAlarmd::FAILURE;
	else if (startClient)
	{
		QString exe = KStandardDirs::findExe(appName);
		if (exe.isNull())
		{
			kError(5900) << "AlarmDaemon::registerApp(): '" << appName << "' not found" << endl;
			result = KAlarmd::NOT_FOUND;
		}
		mClientExe = exe;
	}
	if (result == KAlarmd::SUCCESS)
	{
		mClientStart   = startClient;
		mClientName    = appName;
		mClientDBusObj = dbusObject;
		mClientStart   = startClient;
		KConfig* config = KGlobal::config();
		config->setGroup(CLIENT_GROUP);
		config->writeEntry(CLIENT_KEY, mClientName);
		config->writeEntry(DCOP_OBJECT_KEY, mClientDBusObj);
		config->writeEntry(START_CLIENT_KEY, mClientStart);
		if (init)
			enableAutoStart(true, false);
		config->sync();
		if (init)
		{
			setTimerStatus();
			notifyCalStatus();
		}
	}

	// Notify the client of whether the call succeeded.
	QList<QVariant> args;
	args << false << result;
	kalarmNotify(QLatin1String("registered"), args);
	kDebug(5900) << "AlarmDaemon::registerApp() -> " << result << endl;
}

/******************************************************************************
* D-Bus call to set autostart at login on or off.
*/
void AlarmDaemon::enableAutoStart(bool on, bool sync)
{
        kDebug(5900) << "AlarmDaemon::enableAutoStart(" << on << ")\n";
        KConfig* config = KGlobal::config();
	config->reparseConfiguration();
        config->setGroup(QLatin1String(DAEMON_AUTOSTART_SECTION));
        config->writeEntry(QLatin1String(DAEMON_AUTOSTART_KEY), on);
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
	kDebug(5901) << "AlarmDaemon::checkAlarmsSlot()" << endl;
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
			kDebug(5900) << "Resynching alarm timer" << endl;
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
	kDebug(5901) << "AlarmDaemon::checkAlarms()" << endl;
	AlarmResources* resources = AlarmResources::instance();
	if (!mEnabled  ||  !resources->loadedState(AlarmResource::ACTIVE))
		return;

	KDateTime now  = KDateTime::currentUtcDateTime();
	KDateTime now1 = now.addSecs(1);
	kDebug(5901) << "  To: " << now << endl;
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
		kDebug(5901) << "AlarmDaemon::checkAlarms(): event " << eventID  << endl;

		// Check which of the alarms for this event are due.
		// The times in 'alarmtimes' corresponding to due alarms are set.
		// The times for non-due alarms are set invalid in 'alarmtimes'.
		QList<KDateTime> alarmtimes;
		KCal::Alarm::List alarms = event->alarms();
		for (KCal::Alarm::List::ConstIterator al = alarms.begin();  al != alarms.end();  ++al)
		{
			KCal::Alarm* alarm = *al;
			KDateTime dt;
			if (alarm->enabled())
				dt = alarm->previousRepetition(now1);   // get latest due repetition (if any)
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
	kDebug(5900) << "AlarmDaemon::notifyEvent(" << eventID << "): notification type=" << mClientStart << endl;
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
				kDebug(5900) << "AlarmDaemon::notifyEvent(): client not ready\n";
			else
				kDebug(5900) << "AlarmDaemon::notifyEvent(): don't start client\n";
			return;
		}

		// Start KAlarm, using the command line to specify the alarm
		KProcess p;
		if (mClientExe.isEmpty())
		{
			kDebug(5900) << "AlarmDaemon::notifyEvent(): '" << mClientName << "' not found" << endl;
			return;
		}
		p << mClientExe;
		p << "--handleEvent" << id;
		p.start(KProcess::DontCare);
		kDebug(5900) << "AlarmDaemon::notifyEvent(): used command line" << endl;
	}
	else
	{
		// Notify the client by telling it the event ID
		QList<QVariant> args;
		args << id;
		if (!kalarmNotify(QLatin1String("handleEvent"), args))
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
		// But first synchronise to one second after the minute boundary.
		int firstInterval = DAEMON_CHECK_INTERVAL + 1 - QTime::currentTime().second();
		mAlarmTimer->start(1000 * firstInterval);
		mAlarmTimerSyncing = (firstInterval != DAEMON_CHECK_INTERVAL);
		kDebug(5900) << "Started alarm timer" << endl;
	}
	else if (mAlarmTimer->isActive()  &&  !loaded)
	{
		mAlarmTimer->stop();
		kDebug(5900) << "Stopped alarm timer" << endl;
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
		kDebug(5900) << "AlarmDaemon::notifyCalStatus() sending:" << mClientName << " -> " << change << endl;
		QList<QVariant> args;
		args << change;
		kalarmNotify(QLatin1String("alarmDaemonUpdate"), args);
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
	kDebug(5900) << "AlarmDaemon::setEventHandled(" << eventID << ")\n";
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
		kDebug(5900) << "AlarmDaemon::setEventPending(" << event->uid() << ")\n";
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
	KConfig* config = KGlobal::config();
	config->setGroup(CLIENT_GROUP);
	QString client = config->readEntry(CLIENT_KEY);
	mClientDBusObj = config->readEntry(DCOP_OBJECT_KEY).toLocal8Bit();
	mClientStart   = config->readEntry(START_CLIENT_KEY, false);

	// Verify the configuration
	mClientName.clear();
	if (client.isEmpty()  ||  KStandardDirs::findExe(client).isNull())
		kError(5900) << "AlarmDaemon::readConfig(): '" << client << "': client app not found\n";
	else if (mClientDBusObj.isEmpty())
		kError(5900) << "AlarmDaemon::readConfig(): no D-Bus object specified for '" << client << "'\n";
	else
	{
		mClientName = client;
		kDebug(5900) << "AlarmDaemon::readConfig(): client " << mClientName << endl;
	}

	// Remove obsolete CheckInterval entry (if it exists)
        config->setGroup("General");
	config->deleteEntry("CheckInterval");
	config->sync();
}

/******************************************************************************
* Read the timezone to use. Try to read it from KAlarm's config file. If the
* entry there is blank, use local clock time.
*/
KDateTime::Spec AlarmDaemon::timeSpec()
{
	KConfig kaconfig(KStandardDirs::locate("config", "kalarmrc"));
	kaconfig.setGroup("General");
	QString zone = kaconfig.readEntry("Timezone", QString());
	if (zone.isEmpty())
		return KDateTime::ClockTime;
	const KTimeZone* tz = KSystemTimeZones::zone(zone);
	return tz ? tz : KSystemTimeZones::local();
}

/******************************************************************************
* Checks whether the client application is running.
*/
bool AlarmDaemon::isClientRegistered() const
{
	QDBusReply<bool> isRegistered = QDBusConnection::sessionBus().interface()->isServiceRegistered(mClientName);
	return isRegistered.isValid() ? isRegistered.value() : false;
}
