/*
 *  kalarm.h  -  global header file
 *  Program:  kalarm
 *  Copyright © 2001-2014 by David Jarvie <djarvie@kde.org>
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

#undef QT3_SUPPORT

#ifdef USE_AKONADI
#define VERSION_SUFFIX "-ak"
#else
#define VERSION_SUFFIX "-res"
#endif
#define KALARM_VERSION "2.10.8" VERSION_SUFFIX

#define KALARM_NAME "KAlarm"
#define KALARM_DBUS_SERVICE  "org.kde.kalarm"  // D-Bus service name of KAlarm application

#include <kdeversion.h>

namespace KAlarm
{
/** Return current KAlarm version number as an integer. */
int Version();
}

#endif // KALARM_H

