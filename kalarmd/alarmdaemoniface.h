/*
 *  alarmdaemoniface.h  -  DCOP request interface
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  (C) 2001, 2004 by David Jarvie <software@astrojar.org.uk>
 *  Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
 *  Copyright (c) 1997-1999 Preston Brown
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef ALARMDAEMONIFACE_H
#define ALARMDAEMONIFACE_H

#include <dcopobject.h>


class AlarmDaemonIface : virtual public DCOPObject
{
    K_DCOP
  k_dcop:
    virtual ASYNC enableAutoStart(bool enable) = 0;
    virtual ASYNC enableCalendar(const QString& urlString, bool enable) = 0;
    virtual ASYNC reloadCalendar(const QCString& appname, const QString& urlString) = 0;
    virtual ASYNC resetCalendar(const QCString& appname, const QString& urlString) = 0;
    virtual ASYNC registerApp(const QCString& appName, const QString& appTitle,
                              const QCString& dcopObject, const QString& calendarUrl, bool startClient) = 0;
    virtual ASYNC registerChange(const QCString& appName, bool startClient) = 0;
    virtual ASYNC quit() = 0;
};

#endif
