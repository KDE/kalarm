/*
 *  kalarmd.h  -  global header file
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef KALARMD_H
#define KALARMD_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define DAEMON_VERSION            "4.0"         // kalarmd version number
#define DAEMON_APP_NAME           "kalarmd"     // DCOP name of alarm daemon application
#define DAEMON_DCOP_OBJECT        "ad"          // DCOP name of kalarmd's DCOP interface

#define DAEMON_CHECK_INTERVAL     60            // the daemon checks calendar files every minute

#define DAEMON_AUTOSTART_SECTION  "General"     // daemon's config file section for autostart-at-login
#define DAEMON_AUTOSTART_KEY      "Autostart"   // daemon's config file entry for autostart-at-login

#endif // KALARMD_H
