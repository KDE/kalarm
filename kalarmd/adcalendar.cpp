/*
    Calendar access for KAlarm Alarm Daemon.

    This file is part of the KAlarm alarm daemon.
    Copyright (c) 2001, 2004 David Jarvie <software@astrojar.org.uk>

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

    As a special exception, permission is given to link this program
    with any edition of Qt, and distribute the resulting executable,
    without including the source code for Qt in the source distribution.
*/

#include <kdebug.h>

#include "adcalendar.h"

ADCalendar::ADCalendar(const QString& url, const QCString& appname)
  : ADCalendarBase(url, appname),
    available_( false ),
    enabled_(true)
{
  loadFile();
}

ADCalendar *ADCalendarFactory::create(const QString& url, const QCString& appname)
{
  return new ADCalendar(url, appname);
}

/*
 * Check whether all the alarms for the event with the given ID have already
 * been handled.
 */
bool ADCalendar::eventHandled(const Event* event, const QValueList<QDateTime>& alarmtimes)
{
  EventsMap::ConstIterator it = eventsHandled_.find(event->uid());
  if (it == eventsHandled_.end())
    return false;

  int oldCount = it.data().alarmTimes.count();
  int count = alarmtimes.count();
  for (int i = 0;  i < count;  ++i) {
    if (alarmtimes[i].isValid()) {
      if (i >= oldCount                              // is it an additional alarm?
      ||  !it.data().alarmTimes[i].isValid()         // or has it just become due?
      ||  it.data().alarmTimes[i].isValid()          // or has it changed?
       && alarmtimes[i] != it.data().alarmTimes[i])
        return false;     // this alarm has changed
    }
  }
  return true;
}

/*
 * Remember that the specified alarms for the event with the given ID have been
 * handled.
 */
void ADCalendar::setEventHandled(const Event* event, const QValueList<QDateTime>& alarmtimes)
{
  if (event)
  {
    kdDebug(5900) << "ADCalendar::setEventHandled(" << event->uid() << ")\n";
    EventsMap::Iterator it = eventsHandled_.find(event->uid());
    if (it != eventsHandled_.end())
    {
      // Update the existing entry for the event
      it.data().alarmTimes = alarmtimes;
      it.data().eventSequence = event->revision();
    }
    else
      eventsHandled_.insert(event->uid(),EventItem(urlString(),
                                                   event->revision(),
                                                   alarmtimes));
  }
}

/*
 * Clear all memory of events handled for the specified calendar.
 */
void ADCalendar::clearEventsHandled(const QString& calendarURL)
{
  for (EventsMap::Iterator it = eventsHandled_.begin();  it != eventsHandled_.end();  )
  {
    if (it.data().calendarURL == calendarURL)
    {
      EventsMap::Iterator i = it;
      ++it;                      // prevent iterator becoming invalid with remove()
      eventsHandled_.remove(i);
    }
    else
      ++it;
  }
}
