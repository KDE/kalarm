/*
    Calendar and client access for KDE Alarm Daemon.

    This file is part of the KDE alarm daemon.
    Copyright (c) 2001 David Jarvie <software@astrojar.org.uk>
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

    As a special exception, permission is given to link this program
    with any edition of Qt, and distribute the resulting executable,
    without including the source code for Qt in the source distribution.
*/

#ifndef ADCONFIGDATABASE_H
#define ADCONFIGDATABASE_H

#include "adcalendarbase.h"
#include "clientinfo.h"

// Provides read-only access to the Alarm Daemon config data files
class ADConfigDataBase
{
  public:
    explicit ADConfigDataBase(bool daemon);
    virtual ~ADConfigDataBase() {}

    ClientInfo  getClientInfo(const QCString& appName);
    void        removeClientInfo( const QCString &appName );
    ClientList  clients() const { return mClients; }
    int         clientCount() const     { return mClients.count(); }

    CalendarList calendars() const { return mCalendars; }
    int calendarCount() const { return mCalendars.count(); }

  protected:
    QString           readConfigData(bool sessionStarting, bool& deletedClients,
                                     bool& deletedCalendars,
                                     ADCalendarBaseFactory *);
    virtual void      deleteConfigCalendar(const ADCalendarBase*);
    ADCalendarBase*   getCalendar(const QString& calendarURL);
    static QString    expandURL(const QString& urlString);
    const QString&    clientDataFile() const  { return mClientDataFile; }
    static const QDateTime& baseDateTime();

    static const QCString CLIENT_KEY;
    static const QString CLIENTS_KEY;
    static const QCString GUI_KEY;
    static const QString GUIS_KEY;
    static const QString CLIENT_CALENDAR_KEY;
    static const QString CLIENT_TITLE_KEY;
    static const QString CLIENT_DCOP_OBJECT_KEY;
    static const QString CLIENT_NOTIFICATION_KEY;
    static const QString CLIENT_DISP_CAL_KEY;

    ClientList        mClients;             // client application names and data
    CalendarList      mCalendars;           // the calendars being monitored

  private:
    bool              mIsAlarmDaemon;       // true if the instance is being used by the alarm daemon

    QString           mClientDataFile;      // path of file containing client data
};

#endif
