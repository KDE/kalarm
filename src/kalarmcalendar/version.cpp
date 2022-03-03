/*
 *  version.cpp  -  program version functions
 *  This file is part of kalarmprivate library, which provides access to KAlarm
 *  calendar data.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2007, 2011, 2013 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "version.h"

namespace KAlarmCal
{

int Version(int major, int minor, int rev)
{
    return major * 10000 + minor * 100 + rev;
}

/******************************************************************************
* Convert the supplied KAlarm version string to a version number.
* Reply = version number (double digit for each of major, minor & issue number,
*         e.g. 010203 for 1.2.3
*       = 0 if invalid version string.
*/
int getVersionNumber(const QString &version, QString *subVersion)
{
    // N.B. Remember to change  Version(int major, int minor, int rev)
    //      if the representation returned by this method changes.
    if (subVersion) {
        subVersion->clear();
    }
    const int count = version.count(QLatin1Char('.')) + 1;
    if (count < 2) {
        return 0;
    }
    bool ok;
    unsigned vernum = version.section(QLatin1Char('.'), 0, 0).toUInt(&ok) * 10000;  // major version
    if (!ok) {
        return 0;
    }
    unsigned v = version.section(QLatin1Char('.'), 1, 1).toUInt(&ok);               // minor version
    if (!ok) {
        return 0;
    }
    vernum += (v < 99 ? v : 99) * 100;
    if (count >= 3) {
        // Issue number: allow other characters to follow the last digit
        const QString issue = version.section(QLatin1Char('.'), 2);
        const int n = issue.length();
        if (!n  ||  !issue[0].isDigit()) {
            return 0;
        }
        int i;
        for (i = 0; i < n && issue[i].isDigit(); ++i) {
            ;
        }
        if (subVersion) {
            *subVersion = issue.mid(i);
        }
        v = QStringView(issue).left(i).toUInt();   // issue number
        vernum += (v < 99 ? v : 99);
    }
    return vernum;
}

QString getVersionString(int version)
{
    return QStringLiteral("%1.%2.%3").arg(version / 10000).arg((version % 10000) / 100).arg(version % 100);
}

} // namespace KAlarmCal

// vim: et sw=4:
