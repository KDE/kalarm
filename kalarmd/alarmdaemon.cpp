/*
    KAlarm Alarm Daemon.

    This file is part of the KAlarm alarm daemon.
    Copyright (c) 2001, 2004 David Jarvie <software@astrojar.org.uk>
    Based on the original, (c) 1998, 1999 Preston Brown

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <unistd.h>
#include <stdlib.h>

#include <qtimer.h>
#include <qfile.h>
#include <qdatetime.h>

#include <kapplication.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <klocale.h>
#include <ksimpleconfig.h>
#include <kprocess.h>
#include <kio/netaccess.h>
#include <dcopclient.h>

#include <libkcal/calendarlocal.h>
#include <libkcal/icalformat.h>

#include "alarmguiiface.h"
#include "alarmguiiface_stub.h"
#include "alarmapp.h"

#include "alarmdaemon.h"
#include "alarmdaemon.moc"


AlarmDaemon::AlarmDaemon(QObject *parent, const char *name)
  : DCOPObject(name), QObject(parent, name)
{
  kdDebug(5900) << "AlarmDaemon::AlarmDaemon()" << endl;

  readCheckInterval();
  readDaemonData(false);

  enableAutoStart(true);    // switch autostart on whenever the program is run

  // set up the alarm timer
  mAlarmTimer = new QTimer(this);
  connect( mAlarmTimer, SIGNAL( timeout() ), SLOT( checkAlarmsSlot() ));
  setTimerStatus();
  checkAlarms();
}

AlarmDaemon::~AlarmDaemon()
{
}

/*
 * DCOP call to quit the program.
 */
void AlarmDaemon::quit()
{
  kdDebug(5900) << "AlarmDaemon::quit()" << endl;
  exit(0);
}

void AlarmDaemon::dumpDebug()
{
  kdDebug(5900) << "AlarmDaemon::dumpDebug()" << endl;

  for( ADCalendarBase *cal = mCalendars.first(); cal; cal = mCalendars.next() ) {
    cal->dump();
  }

  kdDebug(5900) << "AlarmDaemon::dumpDebug() done" << endl;
}

/*
 * DCOP call to enable or disable monitoring of a calendar.
 */
void AlarmDaemon::enableCal_(const QString& urlString, bool enable)
{
  kdDebug(5900) << "AlarmDaemon::enableCal_(" << urlString << ")" << endl;

  ADCalendarBase* cal = getCalendar(urlString);
  if (cal)
  {
    cal->setEnabled( enable );
    notifyGuiCalStatus(cal);    // notify any other GUI applications
  }
}

/*
 * DCOP call to add a new calendar file to the list of monitored calendars.
 * If the calendar file is already in the list, the request is ignored.
 */
void AlarmDaemon::addCal_(const QCString& appname, const QString& urlString)
{
  kdDebug(5900) << "AlarmDaemon::addCal_(" << urlString << ")" << endl;

  ADCalendarBase* cal = getCalendar(urlString);
  if (cal)
  {
    // Calendar is already being monitored
    if (!cal->unregistered())
      return;
    if (cal->appName() == appname)
    {
      cal->setUnregistered( false );
      reloadCal_(cal);
      return;
    }
    // The calendar used to belong to another application!
    mCalendars.remove(cal);
  }

  // Load the calendar
  cal = new ADCalendar(urlString, appname);
  mCalendars.append(cal);

  addConfigCalendar(appname, cal);

  if (cal->loaded())
    notifyGui(ADD_MSG_CALENDAR, cal->urlString(), appname);
  kdDebug(5900) << "AlarmDaemon::addCal_(): calendar added" << endl;

  setTimerStatus();
  checkAlarms(cal);
}

/*
 * DCOP call to reload the specified calendar.
 * The calendar is first added to the list of monitored calendars if necessary.
 */
void AlarmDaemon::reloadCal_(const QCString& appname, const QString& urlString)
{
  kdDebug(5900) << "AlarmDaemon::reloadCal_(" << urlString << ")" << endl;

  if (!urlString.isEmpty())
  {
    ADCalendarBase* cal = getCalendar(urlString);
    if (cal)
      reloadCal_(cal);
    else
    {
      // Calendar wasn't in the list, so add it
      if (!appname.isEmpty())
        addCal_(appname, urlString);
    }
  }
}

/*
 * Reload the specified calendar.
 */
void AlarmDaemon::reloadCal_(ADCalendarBase* cal)
{
  kdDebug(5900) << "AlarmDaemon::reloadCal_(): calendar" << endl;

  if (cal && !cal->downloading())
  {
    cal->close();
    if (!cal->setLoadedConnected()) {
      connect( cal, SIGNAL( loaded(ADCalendarBase*, bool) ),
               SLOT( calendarLoaded(ADCalendarBase*, bool) ) );
    }
    cal->loadFile();
  }
}

void AlarmDaemon::calendarLoaded(ADCalendarBase* cal, bool success)
{
    if (success)
      kdDebug(5900) << "Calendar reloaded" << endl;
    notifyGuiCalStatus(cal);
    setTimerStatus();
    checkAlarms(cal);
}

/*
 * DCOP call to reload the specified calendar and reset the data associated with it.
 * The calendar is first added to the list of monitored calendars if necessary.
 */
void AlarmDaemon::resetMsgCal_(const QCString& appname, const QString& urlString)
{
  kdDebug(5900) << "AlarmDaemon::resetMsgCal_(" << urlString << ")\n";

  if (!urlString.isEmpty())
  {
    reloadCal_(appname, urlString);
    ADCalendar::clearEventsHandled(urlString);
    ADCalendarBase* cal = getCalendar(urlString);
    if (cal)
      checkAlarms(cal);
  }
}

/* Remove a calendar file from the list of monitored calendars */
void AlarmDaemon::removeCal_(const QString& urlString)
{
  kdDebug(5900) << "AlarmDaemon::removeCal_(" << urlString << ")\n";

  ADCalendarBase* cal = getCalendar(urlString);
  if (cal)
  {
    deleteConfigCalendar(cal);
    mCalendars.remove(cal);
    kdDebug(5900) << "AlarmDaemon::removeCal_(): calendar removed" << endl;
    notifyGui(DELETE_CALENDAR, urlString);
    setTimerStatus();
  }
}

/*
 * DCOP call to add an application to the list of client applications,
 * and add it to the config file.
 */
void AlarmDaemon::registerApp_(const QCString& appName, const QString& appTitle,
                              const QCString& dcopObject, int notificationType,
                              bool displayCalendarName, bool reregister)
{
  kdDebug(5900) << "AlarmDaemon::registerApp_(" << appName << ", " << appTitle << ", "
                <<  dcopObject << ", " << notificationType << ", " << reregister << ")" << endl;
  AlarmGuiIface::RegResult result = AlarmGuiIface::SUCCESS;
  if (appName.isEmpty())
    result = AlarmGuiIface::FAILURE;
  else if ((notificationType == ClientInfo::DCOP_START_NOTIFY
              ||  notificationType == ClientInfo::COMMAND_LINE_NOTIFY)
       &&  KStandardDirs::findExe(appName).isNull()) {
    kdError() << "AlarmDaemon::registerApp(): app not found\n";
    result = AlarmGuiIface::NOT_FOUND;
  }
  else {
      ClientInfo c = getClientInfo(appName);
      if (c.isValid())
      {
        // The application is already in the clients list.
        if (!reregister) {
          // Mark all its calendar files as unregistered and remove it from the list.
          for (ADCalendarBase* cal = mCalendars.first();  cal;  cal = mCalendars.next())
          {
            if (cal->appName() == appName)
              cal->setUnregistered( true );
          }
        }
        removeClientInfo(appName);
      }
      ClientInfo cinfo(appName, appTitle, dcopObject, notificationType,
                        displayCalendarName);
      mClients.append(cinfo);

      writeConfigClient(appName, cinfo);

      enableAutoStart(true);
      notifyGui(CHANGE_CLIENT);
      setTimerStatus();
    }
  /*
   * Notify the client of whether registration succeeded.
   * N.B. This method must not return a bool because DCOPClient::call()
   *      can cause hangs if daemon and client both happen to call each
   *      other at the same time.
   */
  AlarmGuiIface_stub stub( appName, dcopObject );
  stub.registered( reregister, result );
  kdDebug(5900) << "AlarmDaemon::registerApp_() -> " << result << endl;
}

/*
 * DCOP call to set autostart at login on or off.
 */
void AlarmDaemon::enableAutoStart(bool on)
{
  kdDebug(5900) << "AlarmDaemon::enableAutoStart(" << (int)on << ")\n";
  KConfig* config = kapp->config();
  config->setGroup("General");
  config->writeEntry("Autostart", on);
  config->sync();
  notifyGui(CHANGE_STATUS);
}

/*
 * DCOP call to tell the daemon to re-read its config file.
 */
void AlarmDaemon::readConfig()
{
  kdDebug(5900) << "AlarmDaemon::readConfig()\n";
  kapp->config()->reparseConfiguration();
  int oldCheckInterval = mCheckInterval;
  readCheckInterval();
  if (mCheckInterval != oldCheckInterval) {
    mAlarmTimer->stop();
    setTimerStatus();     // change the alarm timer's interval
    notifyGui(CHANGE_STATUS);
    // The timer has been restarted, so check alarms now to avoid the interval
    // between the last and next checks being longer than either the old or
    // new check interval.
    // Do this AFTER notifying client applications about the change, in case
    // they need to take special action first.
    checkAlarms();
  }
}

/*
 * Read the alarm check interval from the config file.
 */
void AlarmDaemon::readCheckInterval()
{
  KConfig* config = kapp->config();
  config->setGroup("General");
  mCheckInterval = config->readNumEntry("CheckInterval", 1);
  if (mCheckInterval < 1)
    mCheckInterval = 1;
}

/*
 * Check if any alarms are pending for any enabled calendar, and
 * display the pending alarms.
 * Called by the alarm timer.
 */
void AlarmDaemon::checkAlarmsSlot()
{
  kdDebug(5901) << "AlarmDaemon::checkAlarmsSlot()" << endl;

  if (mAlarmTimerSyncing)
  {
    // We've synced to the minute boundary. Now set timer to the check interval.
    mAlarmTimer->changeInterval(mCheckInterval * 60 * 1000);
    mAlarmTimerSyncing = false;
  }
  checkAlarms();
}

/*
 * Check if any alarms are pending for any enabled calendar, and
 * display the pending alarms.
 */
void AlarmDaemon::checkAlarms()
{
  kdDebug(5901) << "AlarmDaemon::checkAlarms()" << endl;

  for( ADCalendarBase *cal = mCalendars.first(); cal; cal = mCalendars.next() ) {
    checkAlarms( cal );
  }
}

/*
 * Check if any alarms are pending for any enabled calendar
 * belonging to a specified client, and display the pending alarms.
 */
void AlarmDaemon::checkAlarms(const QCString& appName)
{
  for (ADCalendarBase* cal = mCalendars.first();  cal;  cal = mCalendars.next()) {
    if (cal->appName() == appName) {
      checkAlarms( cal );
    }
  }
}

/*
 * Check if any alarms are pending for a specified calendar, and
 * display the pending alarms.
 * Reply = true if the calendar check time was updated.
 */
bool AlarmDaemon::checkAlarms( ADCalendarBase* cal )
{
  kdDebug(5901) << "AlarmDaemons::checkAlarms(" << cal->urlString() << ")" << endl;

  if ( !cal->loaded()  ||  !cal->enabled() )
    return false;

  QDateTime to = QDateTime::currentDateTime();

  QPtrList<Event> alarmEvents;
  QValueList<Alarm*> alarms;
  QValueList<Alarm*>::ConstIterator it;
  kdDebug(5901) << "  To: " << to.toString() << endl;
  alarms = cal->alarmsTo( to );
  if (alarms.count()) {
    kdDebug(5901) << "Kalarm alarms=" << alarms.count() << endl;
    for ( it = alarms.begin(); it != alarms.end(); ++it ) {
      Event *event = dynamic_cast<Event *>( (*it)->parent() );
      if ( event ) {
        const QString& eventID = event->uid();
        kdDebug(5901) << "AlarmDaemon::checkAlarms(): KALARM event " << eventID  << endl;
        QValueList<QDateTime> alarmtimes;
        checkEventAlarms(*event, alarmtimes);
        if (!cal->eventHandled(event, alarmtimes)) {
          if (notifyEvent(cal, eventID))
            cal->setEventHandled(event, alarmtimes);
          else
            ;  // don't need to store last check time for this calendar type
        }
      }
    }
  }

  return false;
}

/*
 * Check which of the alarms for the given event are due.
 * The times in 'alarmtimes' corresponding to due alarms are set.
 * The times for non-due alarms are set invalid in 'alarmtimes'.
 */
void AlarmDaemon::checkEventAlarms(const Event& event, QValueList<QDateTime>& alarmtimes)
{
  alarmtimes.clear();
  QDateTime now1 = QDateTime::currentDateTime().addSecs(1);
  Alarm::List alarms = event.alarms();
  Alarm::List::ConstIterator it;
  for ( it = alarms.begin(); it != alarms.end(); ++it ) {
    Alarm *alarm = *it;
    QDateTime dt;
    if ( alarm->enabled() ) {
      // Find the alarm's latest due repetition (if any)
      dt = alarm->previousRepetition( now1 );
    }
    alarmtimes.append( dt );
  }
}

/*
 * Send a DCOP message to a client application telling it that an alarm
 * should now be handled.
 * Reply = false if the event should be held pending until the client
 *         application can be started.
 */
bool AlarmDaemon::notifyEvent(ADCalendarBase* calendar, const QString& eventID)
{
  kdDebug(5900) << "AlarmDaemon::notifyEvent(" << eventID << ")\n";
  if (calendar)
  {
    ClientInfo client = getClientInfo(calendar->appName());
    kdDebug(5900) << "  appName: " << calendar->appName()
                  << "  notification type=" << client.notificationType << endl;
    if (!client.isValid()) {
      kdDebug(5900) << "AlarmDaemon::notifyEvent(): unknown client" << endl;
      return false;
    }
    if (client.waitForRegistration)
    {
      // Don't start the client application if the session manager is still
      // starting the session, since if we start the client before the
      // session manager does, a KUniqueApplication client will not then be
      // able to restore its session.
      // And don't contact a client which was started by the login session
      // until it's ready to handle DCOP calls.
      kdDebug(5900) << "AlarmDaemon::notifyEvent(): wait for session startup" << endl;
      return false;
    }

    bool registered = kapp->dcopClient()->isApplicationRegistered(static_cast<const char*>(calendar->appName()));
    bool ready = registered;
    if (registered)
    {
      QCStringList objects = kapp->dcopClient()->remoteObjects(calendar->appName());
      if (objects.find(client.dcopObject) == objects.end())
        ready = false;
    }
    if (!ready)
    {
      // The client application is not running, or is not yet ready
      // to receive notifications.
      if (client.notificationType == ClientInfo::DCOP_NOTIFY
      ||  client.notificationType == ClientInfo::DCOP_COPY_NOTIFY) {
        if (registered)
          kdDebug(5900) << "AlarmDaemon::notifyEvent(): client not ready\n";
        else
          kdDebug(5900) << "AlarmDaemon::notifyEvent(): don't start client\n";
        return false;
      }

      // Start the client application
      KProcess p;
      QString cmd = locate("exe", calendar->appName());
      if (cmd.isEmpty()) {
        kdDebug(5900) << "AlarmDaemon::notifyEvent(): '"
                      << calendar->appName() << "' not found" << endl;
        return true;
      }
      p << cmd;
      if (client.notificationType == ClientInfo::COMMAND_LINE_NOTIFY)
      {
        // Use the command line to tell the client about the alarm
        p << "--handleEvent" << eventID << "--calendarURL" << calendar->urlString();
        p.start(KProcess::Block);
        kdDebug(5900) << "AlarmDaemon::notifyEvent(): used command line" << endl;
        return true;
      }

      // Notification type = DCOP_START_NOTIFY: start client and then use DCOP
      p.start(KProcess::Block);
      kdDebug(5900) << "AlarmDaemon::notifyEvent(): started " << cmd << endl;
        return false;
    }

    if (client.notificationType == ClientInfo::DCOP_COPY_NOTIFY)
    {
      // Notify the client by sending a copy of the incidence
      Incidence *incidence = calendar->event( eventID );
      if (!incidence) {
        incidence = calendar->todo( eventID );
        if(!incidence) {
          kdDebug(5900) << "AlarmDaemon::notifyEvent(): null incidence\n";
          return true;
        }
      }

      kdDebug() << "--- DCOP send: handleEvent(): " << incidence->summary() << endl;

      CalendarLocal cal;
      cal.addIncidence( incidence->clone() );

      ICalFormat format;

      AlarmGuiIface_stub stub( calendar->appName(), client.dcopObject );
      stub.handleEvent( format.toString( &cal ) );
      if ( !stub.ok() ) {
        kdDebug(5900) << "AlarmDaemon::notifyEvent(): dcop send failed" << endl;
        return false;
      }
    }
    else
    {
      // Notify the client by telling it the calendar URL and event ID
      AlarmGuiIface_stub stub( calendar->appName(), client.dcopObject );
      stub.handleEvent( calendar->urlString(), eventID );
      if ( !stub.ok() ) {
        kdDebug(5900) << "AlarmDaemon::notifyEvent(): dcop send failed" << endl;
        return false;
      }
    }
  }
  return true;
}

/*
 * Starts or stops the alarm timer as necessary after a calendar is enabled/disabled.
 */
void AlarmDaemon::setTimerStatus()
{
  // Count the number of currently loaded calendars whose names should be displayed
  int nLoaded = 0;
  for (ADCalendarBase* cal = mCalendars.first();  cal;  cal = mCalendars.next()) {
    if (cal->loaded())
      ++nLoaded;
  }

  // Start or stop the alarm timer if necessary
  if (!mAlarmTimer->isActive() && nLoaded)
  {
    // Timeout every mCheckInterval minutes.
    // But first synchronise to one second after the minute boundary.
    int checkInterval = mCheckInterval * 60;
    int firstInterval = checkInterval + 1 - QTime::currentTime().second();
    mAlarmTimer->start(1000 * firstInterval);
    mAlarmTimerSyncing = (firstInterval != checkInterval);
    kdDebug(5900) << "Started alarm timer" << endl;
  }
  else if (mAlarmTimer->isActive() && !nLoaded)
  {
    mAlarmTimer->stop();
    kdDebug(5900) << "Stopped alarm timer" << endl;
  }
}

/*
 * DCOP call to add an application to the list of GUI applications,
 * and add it to the config file.
 */
void AlarmDaemon::registerGui(const QCString& appName, const QCString& dcopObject)
{
  kdDebug(5900) << "AlarmDaemon::registerGui(" << appName << ")\n";
  if (appName.isEmpty())
    return;
    const GuiInfo* g = getGuiInfo(appName);
    if (g)
      mGuis.remove(appName);   // the application is already in the GUI list
    mGuis.insert(appName, GuiInfo(dcopObject));

    writeConfigClientGui(appName, dcopObject);

    for (ADCalendarBase* cal = mCalendars.first();  cal;  cal = mCalendars.next()) {
      notifyGuiCalStatus(cal);
    }
}

/*
 * Send a DCOP message to all GUI interface applications, notifying them of a change
 * in calendar status.
 */
void AlarmDaemon::notifyGuiCalStatus(const ADCalendarBase* cal)
{
   notifyGui((cal->available() ? (cal->enabled() ? ENABLE_CALENDAR : DISABLE_CALENDAR) : CALENDAR_UNAVAILABLE),
             cal->urlString());
}

/*
 * Send a DCOP message to all GUI interface applications, notifying them of a change.
 */
void AlarmDaemon::notifyGui(AlarmGuiChangeType change, const QString& calendarURL)
{
  notifyGui( change, calendarURL, "" );
}

void AlarmDaemon::notifyGui(AlarmGuiChangeType change, const QString& calendarURL, const QCString& appName)
{
  kdDebug(5900) << "AlarmDaemon::notifyGui(" << change << ")\n";

  for (GuiMap::ConstIterator g = mGuis.begin();  g != mGuis.end();  ++g)
  {
    QCString dcopObject = g.data().dcopObject;
    if (kapp->dcopClient()->isApplicationRegistered(static_cast<const char*>(g.key())))
    {
      kdDebug(5900)<<"AlarmDaemon::notifyGui() sending:" << g.key()<<" ->" << dcopObject <<endl;

      AlarmGuiIface_stub stub( g.key(), dcopObject );
      stub.alarmDaemonUpdate( change, calendarURL, appName );
      if ( !stub.ok() )
        kdDebug(5900) << "AlarmDaemon::guiNotify(): dcop send failed:" << g.key() << endl;
    }
  }
}

/* Return the GuiInfo structure for the specified GUI application */
const AlarmDaemon::GuiInfo* AlarmDaemon::getGuiInfo(const QCString& appName) const
{
  if (!appName.isEmpty())
  {
    GuiMap::ConstIterator g = mGuis.find(appName);
    if (g != mGuis.end())
      return &g.data();
  }
  return 0;
}

QStringList AlarmDaemon::dumpAlarms()
{
  QDateTime start = QDateTime( QDateTime::currentDateTime().date(),
                               QTime( 0, 0 ) );
  QDateTime end = start.addDays( 1 ).addSecs( -1 );

  QStringList lst;
  // Don't translate, this is for debugging purposes.
  lst << QString("AlarmDeamon::dumpAlarms() from ")+start.toString()+ " to " + end.toString();

  CalendarList cals = calendars();
  ADCalendarBase *cal;
  for( cal = cals.first(); cal; cal = cals.next() ) {
    lst << QString("  Cal: ") + cal->urlString();
    QValueList<Alarm*> alarms = cal->alarms( start, end );
    QValueList<Alarm*>::ConstIterator it;
    for( it = alarms.begin(); it != alarms.end(); ++it ) {
      Alarm *a = *it;
      lst << QString("    ") + a->parent()->summary() + " ("
                + a->time().toString() + ")";
    }
  }
  return lst;
}
