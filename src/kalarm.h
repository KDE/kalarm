/*
 *  kalarm.h  -  global header file
 *  Program:  kalarm
 *  Copyright © 2001-2020 David Jarvie <djarvie@kde.org>
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

#ifndef KALARM_H
#define KALARM_H

#include "config-kalarm.h"
#if FILE_RESOURCES
#define VERSION_SUFFIX "-F"
#else
#define VERSION_SUFFIX "-A"
#endif
#define KALARM_VERSION       VERSION VERSION_SUFFIX
#define KALARM_FULL_VERSION  KALARM_VERSION " (KDE Apps " RELEASE_SERVICE_VERSION ")"

#define KALARM_NAME "KAlarm"
#define KALARM_DBUS_SERVICE  "org.kde.kalarm"  // D-Bus service name of KAlarm application

#endif // KALARM_H

