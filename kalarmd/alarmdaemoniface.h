/*
    This file is part of the KDE alarm daemon.
    Copyright (c) 1997-1999 Preston Brown
    Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
    Copyright (c) 2001 David Jarvie <software@astrojar.org.uk>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    As a special exception, permission is given to link this program
    with any edition of Qt, and distribute the resulting executable,
    without including the source code for Qt in the source distribution.
*/
#ifndef ALARMDAEMONIFACE_H
#define ALARMDAEMONIFACE_H
// $Id$

#include <dcopobject.h>
#include <qstringlist.h>

class AlarmDaemonIface : virtual public DCOPObject
{
    K_DCOP
  k_dcop:
    virtual ASYNC enableAutoStart(bool enable) = 0;
    virtual ASYNC enableCal(const QString& urlString, bool enable) = 0;
    virtual ASYNC addCal(const QCString& appname, const QString& urlString) = 0;
    virtual ASYNC addMsgCal(const QCString& appname, const QString& urlString) = 0;
    virtual ASYNC reloadCal(const QCString& appname, const QString& urlString) = 0;
    virtual ASYNC reloadMsgCal(const QCString& appname, const QString& urlString) = 0;
    virtual ASYNC removeCal(const QString& urlString) = 0;
    virtual ASYNC resetMsgCal(const QCString& appname, const QString& urlString) = 0;
    virtual ASYNC registerApp(const QCString& appName, const QString& appTitle,
                              const QCString& dcopObject, int notificationType,
                              bool displayCalendarName) = 0;
    virtual ASYNC reregisterApp(const QCString& appName, const QString& appTitle,
                              const QCString& dcopObject, int notificationType,
                              bool displayCalendarName) = 0;
    virtual ASYNC registerGui(const QCString& appName, const QCString& dcopObject) = 0;
    virtual ASYNC readConfig() = 0;
    virtual ASYNC quit() = 0;

    virtual ASYNC forceAlarmCheck() = 0;
    virtual ASYNC dumpDebug() = 0;
    virtual QStringList dumpAlarms() = 0;
};

enum AlarmGuiChangeType    // parameters to GUI client notification
{
  CHANGE_STATUS,           // change of alarm daemon or calendar status
  CHANGE_CLIENT,           // change to client application list
  CHANGE_GUI,              // change to GUI client list
  ADD_CALENDAR,            // addition to calendar list (KOrganizer-type calendar)
  ADD_MSG_CALENDAR,        // addition to calendar list (KAlarm-type calendar)
  DELETE_CALENDAR,         // deletion from calendar list
  ENABLE_CALENDAR,         // calendar is now being monitored
  DISABLE_CALENDAR,        // calendar is available but not being monitored
  CALENDAR_UNAVAILABLE     // calendar is unavailable for monitoring
};

#endif
