/*
    Calendar access for KAlarm Alarm Daemon.

    This file is part of the KAlarm alarm daemon.
    Copyright (c) 2001 David Jarvie <software@astrojar.org.uk>

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
#ifndef ADCALENDAR_H
#define ADCALENDAR_H

#include "adcalendarbase.h"

// Alarm Daemon calendar access
class ADCalendar : public ADCalendarBase
{
  public:
    ADCalendar(const QString& url, const QCString& appname);
    ~ADCalendar()  { }
    ADCalendar *create(const QString& url, const QString& appname);

    void           setEnabled( bool enabled ) { enabled_ = enabled; }
    bool           enabled() const     { return enabled_ && !unregistered(); }

    void           setAvailable( bool ) {}
    bool           available() const   { return loaded() && !unregistered(); }

    bool           eventHandled(const Event*, const QValueList<QDateTime> &);
    void           setEventHandled(const Event*,
                                   const QValueList<QDateTime> &);
    static void    clearEventsHandled(const QString& calendarURL);

    bool           loadFile()          { return loadFile_(); }

  public:
    bool           available_;
    bool           enabled_;       // events are currently manually enabled
};

class ADCalendarFactory : public ADCalendarBaseFactory
{
  public:
    ADCalendar *create(const QString& url, const QCString& appname);
};

#endif
