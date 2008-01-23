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

#include <qtimer.h>
#include <qfile.h>
#include <qdatetime.h>

#include <kapplication.h>
#include <kstandarddirs.h>
#include <kprocess.h>
#include <kio/netaccess.h>
#include <dcopclient.h>
#include <kconfig.h>
#include <kdebug.h>

#include <libkcal/calendarlocal.h>
#include <libkcal/icalformat.h>

#include "adcalendar.h"
#include "adconfigdata.h"
#include "alarmguiiface.h"
#include "alarmguiiface_stub.h"
#include "alarmdaemon.moc"


#ifdef AUTOSTART_KALARM
// Number of seconds to wait before autostarting KAlarm.
// Allow plenty of time for session restoration to happen first.
static const int KALARM_AUTOSTART_TIMEOUT = 30;
#endif
static const int SECS_PER_DAY = 3600 * 24;

// KAlarm config file keys
static const QString START_OF_DAY(QString::fromLatin1("StartOfDay"));
static const QString AUTOSTART_TRAY(QString::fromLatin1("AutostartTray"));


AlarmDaemon::AlarmDaemon(bool autostart, QObject *parent, const char *name)
	: DCOPObject(name),
	  QObject(parent, name),
	  mAlarmTimer(0)
{
	kdDebug(5900) << "AlarmDaemon::AlarmDaemon()" << endl;
	ADConfigData::readConfig();

	ADConfigData::enableAutoStart(true);    // switch autostart on whenever the program is run

	readKAlarmConfig();       // read time-related KAlarm config items

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
		KConfig kaconfig(locate("config", "kalarmrc"));
		kaconfig.setGroup(QString::fromLatin1("General"));
		autostart = kaconfig.readBoolEntry(AUTOSTART_TRAY, false);
		if (autostart)
		{
			kdDebug(5900) << "AlarmDaemon::AlarmDaemon(): wait to autostart KAlarm\n";
			QTimer::singleShot(KALARM_AUTOSTART_TIMEOUT * 1000, this, SLOT(autostartKAlarm()));
		}
	}
	if (!autostart)
#endif
		startMonitoring();    // otherwise, start monitoring alarms now
}

/******************************************************************************
* DCOP call to quit the program.
*/
void AlarmDaemon::quit()
{
	kdDebug(5900) << "AlarmDaemon::quit()" << endl;
	exit(0);
}

/******************************************************************************
* Called after a timer delay to autostart KAlarm in the system tray.
*/
void AlarmDaemon::autostartKAlarm()
{
#ifdef AUTOSTART_KALARM
	if (mAlarmTimer)
	{
		kdDebug(5900) << "AlarmDaemon::autostartKAlarm(): KAlarm already registered\n";
		return;    // KAlarm has already registered with us
	}
	kdDebug(5900) << "AlarmDaemon::autostartKAlarm(): starting KAlarm\n";
	QStringList args;
	args << QString::fromLatin1("--tray");
	int ret = KApplication::kdeinitExec(QString::fromLatin1("kalarm"), args);
	if (ret)
		kdError(5900) << "AlarmDaemon::autostartKAlarm(): error=" << ret << endl;
	else
		kdDebug(5900) << "AlarmDaemon::autostartKAlarm(): success" << endl;

	startMonitoring();
#endif
}

/******************************************************************************
* Start monitoring alarms.
*/
void AlarmDaemon::startMonitoring()
{
	// Set up the alarm timer
	mAlarmTimer = new QTimer(this);
	connect(mAlarmTimer, SIGNAL(timeout()), SLOT(checkAlarmsSlot()));
	setTimerStatus();

	// Start monitoring calendar files.
	// They are monitored until their client application registers, upon which
	// monitoring ceases until KAlarm tells the daemon to monitor it.
	checkAlarms();
}

/******************************************************************************
* DCOP call to enable or disable monitoring of a calendar.
*/
void AlarmDaemon::enableCal(const QString& urlString, bool enable)
{
	kdDebug(5900) << "AlarmDaemon::enableCal(" << urlString << ")" << endl;
	ADCalendar* cal = ADCalendar::getCalendar(urlString);
	if (cal)
	{
		cal->setEnabled(enable);
		notifyCalStatus(cal);    // notify KAlarm
	}
}

/******************************************************************************
* DCOP call to reload, and optionally reset, the specified calendar.
*/
void AlarmDaemon::reloadCal(const QCString& appname, const QString& urlString, bool reset)
{
	kdDebug(5900) << "AlarmDaemon::reloadCal(" << urlString << ")" << endl;
	ADCalendar* cal = ADCalendar::getCalendar(urlString);
	if (!cal  ||  cal->appName() != appname)
		return;
	reloadCal(cal, reset);
}

/******************************************************************************
* Reload the specified calendar.
* If 'reset' is true, the data associated with the calendar is reset.
*/
void AlarmDaemon::reloadCal(ADCalendar* cal, bool reset)
{
	kdDebug(5900) << "AlarmDaemon::reloadCal(): calendar" << endl;
	if (!cal)
		return;
	if (!cal->downloading())
	{
		cal->close();
		if (!cal->setLoadedConnected())
			connect(cal, SIGNAL(loaded(ADCalendar*, bool)), SLOT(calendarLoaded(ADCalendar*, bool)));
		cal->loadFile(reset);
	}
	else if (reset)
		cal->clearEventsHandled();
}

void AlarmDaemon::calendarLoaded(ADCalendar* cal, bool success)
{
	if (success)
		kdDebug(5900) << "Calendar reloaded" << endl;
	notifyCalStatus(cal);       // notify KAlarm
	setTimerStatus();
	checkAlarms(cal);
}

/******************************************************************************
* DCOP call to notify the daemon that an event has been handled, and optionally
* to tell it to reload the calendar.
*/
void AlarmDaemon::eventHandled(const QCString& appname, const QString& calendarUrl, const QString& eventID, bool reload)
{
	QString urlString = expandURL(calendarUrl);
	kdDebug(5900) << "AlarmDaemon::eventHandled(" << urlString << (reload ? "): reload" : ")") << endl;
	ADCalendar* cal = ADCalendar::getCalendar(urlString);
	if (!cal  ||  cal->appName() != appname)
		return;
	cal->setEventHandled(eventID);
	if (reload)
		reloadCal(cal, false);
}

/******************************************************************************
* DCOP call to add an application to the list of client applications,
* and add it to the config file.
* N.B. This method must not return a bool because DCOPClient::call() can cause
*      a hang if the daemon happens to send a notification to KAlarm at the
*      same time as KAlarm calls this DCCOP method.
*/
void AlarmDaemon::registerApp(const QCString& appName, const QString& appTitle,
                              const QCString& dcopObject, const QString& calendarUrl,
			      bool startClient)
{
	kdDebug(5900) << "AlarmDaemon::registerApp(" << appName << ", " << appTitle << ", "
	              <<  dcopObject << ", " << startClient << ")" << endl;
	KAlarmd::RegisterResult result;
	if (appName.isEmpty())
		result = KAlarmd::FAILURE;
	else if (startClient  &&  KStandardDirs::findExe(appName).isNull())
	{
		kdError() << "AlarmDaemon::registerApp(): app not found" << endl;
		result = KAlarmd::NOT_FOUND;
	}
	else
	{
		ADCalendar* keepCal = 0;
		ClientInfo* client = ClientInfo::get(appName);
		if (client)
		{
			// The application is already a client.
			// If it's the same calendar file, don't delete its calendar object.
			if (client->calendar()  &&  client->calendar()->urlString() == calendarUrl)
			{
				keepCal = client->calendar();
				client->detachCalendar();
			}
			ClientInfo::remove(appName);    // this deletes the calendar if not detached
		}

		if (keepCal)
			client = new ClientInfo(appName, appTitle, dcopObject, keepCal, startClient);
		else
			client = new ClientInfo(appName, appTitle, dcopObject, calendarUrl, startClient);
		client->calendar()->setUnregistered(false);
		ADConfigData::writeClient(appName, client);

		ADConfigData::enableAutoStart(true);
		setTimerStatus();
		notifyCalStatus(client->calendar());
		result = KAlarmd::SUCCESS;
	}

	// Notify the client of whether the call succeeded.
	AlarmGuiIface_stub stub(appName, dcopObject);
	stub.registered(false, result, DAEMON_VERSION_NUM);
	kdDebug(5900) << "AlarmDaemon::registerApp() -> " << result << endl;
}

/******************************************************************************
* DCOP call to change whether KAlarm should be started when an event needs to
* be notified to it.
* N.B. This method must not return a bool because DCOPClient::call() can cause
*      a hang if the daemon happens to send a notification to KAlarm at the
*      same time as KAlarm calls this DCCOP method.
*/
void AlarmDaemon::registerChange(const QCString& appName, bool startClient)
{
	kdDebug(5900) << "AlarmDaemon::registerChange(" << appName << ", " << startClient << ")" << endl;
	KAlarmd::RegisterResult result;
	ClientInfo* client = ClientInfo::get(appName);
	if (!client)
		return;    // can't access client to tell it the result
	if (startClient  &&  KStandardDirs::findExe(appName).isNull())
	{
		kdError() << "AlarmDaemon::registerChange(): app not found" << endl;
		result = KAlarmd::NOT_FOUND;
	}
	else
	{
		client->setStartClient(startClient);
		ADConfigData::writeClient(appName, client);
		result = KAlarmd::SUCCESS;
	}

	// Notify the client of whether the call succeeded.
	AlarmGuiIface_stub stub(appName, client->dcopObject());
	stub.registered(true, result, DAEMON_VERSION_NUM);
	kdDebug(5900) << "AlarmDaemon::registerChange() -> " << result << endl;
}

/******************************************************************************
* DCOP call to set autostart at login on or off.
*/
void AlarmDaemon::enableAutoStart(bool on)
{
	ADConfigData::enableAutoStart(on);
}
 
/******************************************************************************
* Check if any alarms are pending for any enabled calendar, and display the
* pending alarms.
* Called by the alarm timer.
*/
void AlarmDaemon::checkAlarmsSlot()
{
	kdDebug(5901) << "AlarmDaemon::checkAlarmsSlot()" << endl;
	if (mAlarmTimerSyncing)
	{
		// We've synched to the minute boundary. Now set timer to the check interval.
		mAlarmTimer->changeInterval(DAEMON_CHECK_INTERVAL * 1000);
		mAlarmTimerSyncing = false;
		mAlarmTimerSyncCount = 10;    // resynch every 10 minutes, in case of glitches
	}
	else if (--mAlarmTimerSyncCount <= 0)
	{
		int interval = DAEMON_CHECK_INTERVAL + 1 - QTime::currentTime().second();
		if (interval < DAEMON_CHECK_INTERVAL - 1)
		{
			// Need to re-synch to 1 second past the minute
			mAlarmTimer->changeInterval(interval * 1000);
			mAlarmTimerSyncing = true;
			kdDebug(5900) << "Resynching alarm timer" << endl;
		}
		else
			mAlarmTimerSyncCount = 10;
	}
	checkAlarms();
}

/******************************************************************************
* Check if any alarms are pending for any enabled calendar, and display the
* pending alarms.
*/
void AlarmDaemon::checkAlarms()
{
	kdDebug(5901) << "AlarmDaemon::checkAlarms()" << endl;
	for (ADCalendar::ConstIterator it = ADCalendar::begin();  it != ADCalendar::end();  ++it)
		checkAlarms(*it);
}

/******************************************************************************
* Check if any alarms are pending for a specified calendar, and display the
* pending alarms.
*/
void AlarmDaemon::checkAlarms(ADCalendar* cal)
{
	kdDebug(5901) << "AlarmDaemons::checkAlarms(" << cal->urlString() << ")" << endl;
	if (!cal->loaded()  ||  !cal->enabled())
		return;

	QDateTime now  = QDateTime::currentDateTime();
	kdDebug(5901) << "  To: " << now.toString() << endl;
	QValueList<KCal::Alarm*> alarms = cal->alarmsTo(now);
	if (!alarms.count())
		return;
	QValueList<KCal::Event*> eventsDone;
	for (QValueList<KCal::Alarm*>::ConstIterator it = alarms.begin();  it != alarms.end();  ++it)
	{
		KCal::Event* event = dynamic_cast<KCal::Event*>((*it)->parent());
		if (!event  ||  eventsDone.find(event) != eventsDone.end())
			continue;   // either not an event, or the event has already been processed
		eventsDone += event;
		const QString& eventID = event->uid();
		kdDebug(5901) << "AlarmDaemon::checkAlarms(): event " << eventID  << endl;

		// Check which of the alarms for this event are due.
		// The times in 'alarmtimes' corresponding to due alarms are set.
		// The times for non-due alarms are set invalid in 'alarmtimes'.
		bool recurs = event->doesRecur();
		const QStringList cats = event->categories();
		bool floats = (cats.find(QString::fromLatin1("DATE")) != cats.end());
		QDateTime nextDateTime = event->dtStart();
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
						nextDateTime = d;
					else if (!floats  &&  prop.length() == 15  &&  prop[8] == QChar('T'))
					{
						QTime t(prop.mid(9,2).toInt(), prop.mid(11,2).toInt(), prop.mid(13,2).toInt());
						if (t.isValid())
							nextDateTime = QDateTime(d, t);
					}
				}
			}
		}
		if (floats)
			nextDateTime.setTime(mStartOfDay);
		QValueList<QDateTime> alarmtimes;
		KCal::Alarm::List alarms = event->alarms();
		for (KCal::Alarm::List::ConstIterator al = alarms.begin();  al != alarms.end();  ++al)
		{
			KCal::Alarm* alarm = *al;
			QDateTime dt;
			if (alarm->enabled())
			{
				QDateTime dt1;
				if (!alarm->hasTime())
				{
					// Find the latest recurrence for the alarm.
					// Need to do this for alarms with offsets in order to detect
					// reminders due for recurrences.
					int offset = alarm->hasStartOffset() ? alarm->startOffset().asSeconds()
					           : alarm->endOffset().asSeconds() + event->dtStart().secsTo(event->dtEnd());
					if (offset)
					{
						dt1 = nextDateTime.addSecs(floats ? (offset/SECS_PER_DAY)*SECS_PER_DAY : offset);
						if (dt1 > now)
							dt1 = QDateTime();
					}
				}
				// Get latest due repetition, or the recurrence time if none
				dt = nextDateTime;
				if (nextDateTime <= now  &&  alarm->repeatCount() > 0)
				{
					int snoozeSecs = alarm->snoozeTime() * 60;
					int offset = alarm->repeatCount() * snoozeSecs;
					QDateTime lastRepetition = nextDateTime.addSecs(floats ? (offset/SECS_PER_DAY)*SECS_PER_DAY : offset);
					if (lastRepetition <= now)
						dt = lastRepetition;
					else
					{
						int repetition = nextDateTime.secsTo(now) / snoozeSecs;
						int offset = repetition * snoozeSecs;
						dt = nextDateTime.addSecs(floats ? (offset/SECS_PER_DAY)*SECS_PER_DAY : offset);
					}
				}
				if (!dt.isValid()  ||  dt > now
				||  dt1.isValid()  &&  dt1 > dt)  // already tested dt1 <= now
					dt = dt1;
			}
			alarmtimes.append(dt);
		}
		if (!cal->eventHandled(event, alarmtimes))
		{
			if (notifyEvent(cal, eventID))
				cal->setEventPending(event, alarmtimes);
		}
	}
}

/******************************************************************************
* Send a DCOP message to KAlarm telling it that an alarm should now be handled.
* Reply = false if the event should be held pending until KAlarm can be started.
*/
bool AlarmDaemon::notifyEvent(ADCalendar* calendar, const QString& eventID)
{
	if (!calendar)
		return true;
	QCString appname = calendar->appName();
	const ClientInfo* client = ClientInfo::get(appname);
	if (!client)
	{
		kdDebug(5900) << "AlarmDaemon::notifyEvent(" << appname << "): unknown client" << endl;
		return false;
	}
	kdDebug(5900) << "AlarmDaemon::notifyEvent(" << appname << ", " << eventID << "): notification type=" << client->startClient() << endl;
	QString id = QString::fromLatin1("ad:") + eventID;    // prefix to indicate that the notification if from the daemon

	// Check if the client application is running and ready to receive notification
	bool registered = kapp->dcopClient()->isApplicationRegistered(static_cast<const char*>(appname));
	bool ready = registered;
	if (registered)
	{
		// It's running, but check if it has created our DCOP interface yet
		QCStringList objects = kapp->dcopClient()->remoteObjects(appname);
		if (objects.find(client->dcopObject()) == objects.end())
			ready = false;
	}
	if (!ready)
	{
		// KAlarm is not running, or is not yet ready to receive notifications.
		if (!client->startClient())
		{
			if (registered)
				kdDebug(5900) << "AlarmDaemon::notifyEvent(): client not ready\n";
			else
				kdDebug(5900) << "AlarmDaemon::notifyEvent(): don't start client\n";
			return false;
		}

		// Start KAlarm, using the command line to specify the alarm
		KProcess p;
		QString cmd = locate("exe", appname);
		if (cmd.isEmpty())
		{
			kdDebug(5900) << "AlarmDaemon::notifyEvent(): '" << appname << "' not found" << endl;
			return true;
		}
		p << cmd;
		p << "--handleEvent" << id << "--calendarURL" << calendar->urlString();
		p.start(KProcess::DontCare);
		kdDebug(5900) << "AlarmDaemon::notifyEvent(): used command line" << endl;
		return true;
	}

	// Notify the client by telling it the calendar URL and event ID
	AlarmGuiIface_stub stub(appname, client->dcopObject());
	stub.handleEvent(calendar->urlString(), id);
	if (!stub.ok())
	{
		kdDebug(5900) << "AlarmDaemon::notifyEvent(): dcop send failed" << endl;
		return false;
	}
	return true;
}

/******************************************************************************
* Starts or stops the alarm timer as necessary after a calendar is enabled/disabled.
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
	// Count the number of currently loaded calendars whose names should be displayed
	int nLoaded = 0;
	for (ADCalendar::ConstIterator it = ADCalendar::begin();  it != ADCalendar::end();  ++it)
		if ((*it)->loaded())
			++nLoaded;

	// Start or stop the alarm timer if necessary
	if (!mAlarmTimer->isActive()  &&  nLoaded)
	{
		// Timeout every minute.
		// But first synchronise to one second after the minute boundary.
		int firstInterval = DAEMON_CHECK_INTERVAL + 1 - QTime::currentTime().second();
		mAlarmTimer->start(1000 * firstInterval);
		mAlarmTimerSyncing = (firstInterval != DAEMON_CHECK_INTERVAL);
		kdDebug(5900) << "Started alarm timer" << endl;
	}
	else if (mAlarmTimer->isActive()  &&  !nLoaded)
	{
		mAlarmTimer->stop();
		kdDebug(5900) << "Stopped alarm timer" << endl;
	}
}

/******************************************************************************
* Send a DCOP message to to the client which owns the specified calendar,
* notifying it of a change in calendar status.
*/
void AlarmDaemon::notifyCalStatus(const ADCalendar* cal)
{
	ClientInfo* client = ClientInfo::get(cal);
	if (!client)
		return;
	QCString appname = client->appName();
	if (kapp->dcopClient()->isApplicationRegistered(static_cast<const char*>(appname)))
	{
		KAlarmd::CalendarStatus change = cal->available() ? (cal->enabled() ? KAlarmd::CALENDAR_ENABLED : KAlarmd::CALENDAR_DISABLED)
		                                                  : KAlarmd::CALENDAR_UNAVAILABLE;
		kdDebug(5900) << "AlarmDaemon::notifyCalStatus() sending:" << appname << " -> " << change << endl;
		AlarmGuiIface_stub stub(appname, client->dcopObject());
		stub.alarmDaemonUpdate(change, cal->urlString());
		if (!stub.ok())
			kdError(5900) << "AlarmDaemon::notifyCalStatus(): dcop send failed:" << appname << endl;
	}
}

/******************************************************************************
* Read all relevant items from KAlarm config.
* Executed on DCOP call to notify a time related value change in the KAlarm
* config file.
*/
void AlarmDaemon::readKAlarmConfig()
{
	KConfig config(locate("config", "kalarmrc"));
	config.setGroup(QString::fromLatin1("General"));
	QDateTime defTime(QDate(1900,1,1), QTime());
	mStartOfDay = config.readDateTimeEntry(START_OF_DAY, &defTime).time();
	kdDebug(5900) << "AlarmDaemon::readKAlarmConfig()" << endl;
}

/******************************************************************************
* Expand a DCOP call parameter URL to a full URL.
* (We must store full URLs in the calendar data since otherwise later calls to
* reload or remove calendars won't necessarily find a match.)
*/
QString AlarmDaemon::expandURL(const QString& urlString)
{
	if (urlString.isEmpty())
		return QString();
	return KURL(urlString).url();
}
