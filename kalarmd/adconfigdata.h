/*
    KDE Alarm Daemon.

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
#ifndef ADCONFIGDATARW_H
#define ADCONFIGDATARW_H

#include <ksimpleconfig.h>

#include "clientinfo.h"
#include "adcalendar.h"
#include "adconfigdatabase.h"

// Provides read-write access to the Alarm Daemon config files
class ADConfigDataRW : public ADConfigDataBase
{
  public:
    ADConfigDataRW()  : ADConfigDataBase(true) { }
    virtual ~ADConfigDataRW() {}
    void readDaemonData(bool sessionStarting);
    void writeConfigClient(const QCString& appName, const ClientInfo&);
    void writeConfigClientGui(const QCString& appName, const QString& dcopObject);
    void addConfigClient(KSimpleConfig&, const QCString& appName, const QString& key);
    void addConfigCalendar(const QCString& appName, ADCalendarBase*);
    void writeConfigCalendar(const ADCalendarBase*);
    virtual void   deleteConfigCalendar(const ADCalendarBase*);
    void sync();

    typedef QMap<QCString, QCString> GuiMap;  // maps GUI client names against DCOP object names

    GuiMap         mGuis;                // client GUI application names and data

  private:
    void writeConfigCalendar(const ADCalendarBase*, KSimpleConfig&);
};

#endif
