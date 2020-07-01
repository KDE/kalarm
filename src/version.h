/*
 *  version.h  -  program version functions
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  SPDX-FileCopyrightText: 2005, 2009-2011 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KALARM_VERSION_H
#define KALARM_VERSION_H

#include "kalarmcal_export.h"
#include <QString>

namespace KAlarmCal
{

/** Return a specified version as an integer. */
KALARMCAL_EXPORT int Version(int major, int minor, int rev);

/** Convert a version string to an integer. */
KALARMCAL_EXPORT int getVersionNumber(const QString &version, QString *subVersion = nullptr);

/** Convert a version integer to a string. */
KALARMCAL_EXPORT QString getVersionString(int version);

}

#endif // KALARM_VERSION_H

// vim: et sw=4:
