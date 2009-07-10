/*
 *  kalarm.h  -  global header file
 *  Program:  kalarm
 *  Copyright © 2001-2009 by David Jarvie <djarvie@kde.org>
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

#define KALARM_VERSION "2.2.4"
#define KALARM_NAME "KAlarm"
#define KALARM_DBUS_SERVICE  "org.kde.kalarm"  // D-Bus service name of KAlarm application

#include <kdeversion.h>

class QString;
namespace KAlarm
{
/** Return current KAlarm version number as an integer. */
int        Version();
/** Return a specified version as an integer. */
inline int Version(int major, int minor, int rev)     { return major*10000 + minor*100 + rev; }
/** Convert a version string to an integer. */
int        getVersionNumber(const QString& version, QString* subVersion = 0);
}

#endif // KALARM_H

