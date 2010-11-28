/*
 *  version.h  -  program version functions
 *  Program:  kalarm
 *  Copyright © 2005,2009-2010 by David Jarvie <djarvie@kde.org>
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

#ifndef VERSION_H
#define VERSION_H

#include "kalarm_cal_export.h"

class QString;

namespace KAlarm
{

/** Return a specified version as an integer. */
inline int Version(int major, int minor, int rev)     { return major*10000 + minor*100 + rev; }

/** Convert a version string to an integer. */
KALARM_CAL_EXPORT int getVersionNumber(const QString& version, QString* subVersion = 0);

}

#endif // VERSION_H

// vim: et sw=4:
