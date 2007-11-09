/*
 *  kalarmd.h  -  global header file
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  Copyright © 2004-2007 by David Jarvie <software@astrojar.org.uk>
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

#ifndef KALARMD_H
#define KALARMD_H


#define DAEMON_VERSION            "5.1"         // kalarmd version number string
#define DAEMON_VERSION_NUM        50100         // kalarmd version number integer
#define DAEMON_APP_NAME           "kalarmd"     // executable name of alarm daemon application
#define DAEMON_DBUS_SERVICE       "org.kde.kalarm.kalarmd"  // D-Bus service name of alarm daemon application
#define DAEMON_DBUS_OBJECT        "/daemon"     // object path of kalarmd's D-Bus interface

#define DAEMON_CHECK_INTERVAL     60            // the daemon checks calendar files every minute

#define DAEMON_AUTOSTART_SECTION  "General"     // daemon's config file section for autostart-at-login
#define DAEMON_AUTOSTART_KEY      "Autostart"   // daemon's config file entry for autostart-at-login

#define AUTOSTART_KALARM   // temporary? fix for KAlarm autostart before session restoration

#endif // KALARMD_H
