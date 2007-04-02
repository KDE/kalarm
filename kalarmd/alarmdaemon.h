/*
 *  alarmdaemon.h  -  alarm daemon control routines
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

#ifndef ALARMDAEMON_H
#define ALARMDAEMON_H

#include <QTimer>

#include <kcal/calendarlocal.h>

class OrgKdeKalarmNotifyInterface;
class AlarmResource;


class AlarmDaemon : public QObject
{
		Q_OBJECT
		Q_CLASSINFO("D-Bus Interface", "org.kde.kalarm.kalarmd.Daemon")
	public:
		explicit AlarmDaemon(bool autostart, QObject* parent = 0);
		~AlarmDaemon();

	public Q_SLOTS:
		// D-Bus interface
		Q_SCRIPTABLE void enableAutoStart(bool enable)        { enableAutoStart(enable, true); }
		Q_SCRIPTABLE void enable(bool);
		Q_SCRIPTABLE void reloadResource(const QString& id)   { reloadResource(id, true, false); }
		Q_SCRIPTABLE void resetResource(const QString& id)    { reloadResource(id, true, true); }
		Q_SCRIPTABLE void resourceActive(const QString& id, bool active);
		Q_SCRIPTABLE void resourceLocation(const QString& id, const QString& locn, const QString& locn2);
		Q_SCRIPTABLE void registerApp(const QString& appName, const QString& serviceName, const QString& dbusObject, bool startClient);
		Q_SCRIPTABLE void registerChange(const QString& appName, const QString& serviceName, bool startClient);
		Q_SCRIPTABLE void eventHandled(const QString& eventID, bool reload);
		Q_SCRIPTABLE void quit();

	private slots:
//#ifdef AUTOSTART_KALARM
		void    autostartKAlarm();
//#endif
		void    cacheDownloaded(AlarmResource*);
		void    resourceLoaded(AlarmResource*);
		void    checkAlarmsSlot();
		void    checkAlarms();

	private:
		struct EventItem
		{
			EventItem() : eventSequence(0) { }
			EventItem(int seqno, const QList<KDateTime>& alarmtimes)
			        : eventSequence(seqno), alarmTimes(alarmtimes) {}
			int                   eventSequence;
			QList<KDateTime> alarmTimes;
		};
		typedef QMap<QString, EventItem>  EventsMap;    // event ID, sequence no/times

		void    readConfig();
		KDateTime::Spec timeSpec();
		void    startMonitoring();
		void    registerApp(const QString& appName, const QString& serviceName, const QString& dbusObject, bool startClient, bool init);
		void    enableAutoStart(bool on, bool sync);
		void    reloadResource(const QString& id, bool check, bool reset);
		void    reloadResource(AlarmResource*, bool reset);
		void    notifyEvent(const QString& eventID, const KCal::Event*, const QList<KDateTime>& alarmtimes);
		void    notifyCalStatus();
		bool    isClientRegistered() const;
		void    setTimerStatus();
		OrgKdeKalarmNotifyInterface* kalarmNotifyDBus();
		bool    checkDBusResult(const char* funcname);

		void    setEventPending(const KCal::Event*, const QList<KDateTime>&);
		void    setEventHandled(const QString& eventID);
		void    clearEventsHandled(AlarmResource* = 0, bool nonexistentOnly = false);
		bool    eventHandled(const KCal::Event*, const QList<KDateTime>&);

		void    clearEventMap(EventsMap&, AlarmResource*, bool nonexistentOnly);
		void    setEventInMap(EventsMap&, const QString& eventID, const QList<KDateTime>& alarmtimes, int sequence);

		static EventsMap  mEventsHandled;  // IDs of already triggered events which have been processed by KAlarm
		static EventsMap  mEventsPending;  // IDs of already triggered events not yet processed by KAlarm

		OrgKdeKalarmNotifyInterface* mDBusNotify;     // client's notification D-Bus interface
		QString    mClientName;          // client's executable and DCOP name
		QString    mClientDBusObj;       // object path to receive D-Bus messages
		QString    mClientExe;           // client executable path (if mClientStart true)
		QTimer*    mAlarmTimer;
		int        mAlarmTimerSyncCount; // countdown to re-synching the alarm timer
		bool       mAlarmTimerSyncing;   // true while alarm timer interval < 1 minute
		bool       mAutoStarting;        // true while waiting to autostart KAlarm
		bool       mClientStart;         // whether to notify events via command line if client app isn't running
		bool       mEnabled;             // alarms are currently enabled
};

#endif // ALARMDAEMON_H
