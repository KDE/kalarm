/*
    Client access for KDE Alarm Daemon.

    This file is part of the KDE alarm daemon.
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

#include "clientinfo.h"

ClientInfo::ClientInfo(const QCString &_appName, const QString& titl,
                       const QCString& dcopObj, int notifyType, bool disp,
                       bool wait)
  : appName( _appName ),
    title(titl),
    dcopObject(dcopObj),
    displayCalName(disp),
    waitForRegistration(wait),
    menuIndex(0),
    mValid( true )
{
  setNotificationType(notifyType);
}

void ClientInfo::setNotificationType(int type)
{
  switch (type)
  {
    case DCOP_START_NOTIFY:
    case DCOP_COPY_NOTIFY:
    case COMMAND_LINE_NOTIFY:
      notificationType = (ClientInfo::NotificationType)type;
      break;
    case DCOP_NOTIFY:
    default:
      notificationType = DCOP_NOTIFY;
      break;
  }
}
