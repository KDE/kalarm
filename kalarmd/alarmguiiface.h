/*
    This file is part of the KDE alarm daemon GUI.
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
#ifndef DAEMONGUIIFACE_H
#define DAEMONGUIIFACE_H
// $Id$

#include <dcopobject.h>
// Don't use #include "alarmdaemoniface.h" so that programs other than kalarmd can compile
#include <kalarm/kalarmd/alarmdaemoniface.h>

/**
 * Client applications should inherit from this class to receive notifications
 * from the alarm daemon.
 */
class AlarmGuiIface : virtual public DCOPObject
{
    K_DCOP
  k_dcop:
    virtual ASYNC alarmDaemonUpdate(int alarmGuiChangeType,
                                    const QString& calendarURL,
                                    const QCString& appName) = 0;
    virtual ASYNC handleEvent(const QString& calendarURL,
                              const QString& eventID) = 0;
    virtual ASYNC handleEvent( const QString &iCalendarString ) = 0;
    /** Called to indicate success/failure of (re)registerApp() call.
        @param reregister true if the call was registerApp(), false if registerApp().
        @param result success/failure code. Value is of type AlarmGuiRegResult.
     */
    virtual ASYNC registered( bool reregister, int result ) = 0;

  public:
    enum RegResult   // result code of registerApp() DCOP call
    {
      FAILURE   = 0,
      SUCCESS   = 1,
      NOT_FOUND = 2    // notification type requires client start, but client executable not found
    };
};

#endif
