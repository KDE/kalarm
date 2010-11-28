/*
 *  version.cpp  -  program version functions
 *  Program:  kalarm
 *  Copyright © 2002-2007 by David Jarvie <djarvie@kde.org>
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

#include "version.h"
#include <QString>

namespace KAlarm
{

/******************************************************************************
* Convert the supplied KAlarm version string to a version number.
* Reply = version number (double digit for each of major, minor & issue number,
*         e.g. 010203 for 1.2.3
*       = 0 if invalid version string.
*/
int getVersionNumber(const QString& version, QString* subVersion)
{
    // N.B. Remember to change  Version(int major, int minor, int rev)
    //      if the representation returned by this method changes.
    if (subVersion)
        subVersion->clear();
    int count = version.count(QChar('.')) + 1;
    if (count < 2)
        return 0;
    bool ok;
    unsigned vernum = version.section('.', 0, 0).toUInt(&ok) * 10000;  // major version
    if (!ok)
        return 0;
    unsigned v = version.section('.', 1, 1).toUInt(&ok);               // minor version
    if (!ok)
        return 0;
    vernum += (v < 99 ? v : 99) * 100;
    if (count >= 3)
    {
        // Issue number: allow other characters to follow the last digit
        QString issue = version.section('.', 2);
        int n = issue.length();
        if (!n  ||  !issue[0].isDigit())
            return 0;
        int i;
        for (i = 0;  i < n && issue[i].isDigit();  ++i) ;
        if (subVersion)
            *subVersion = issue.mid(i);
        v = issue.left(i).toUInt();   // issue number
        vernum += (v < 99 ? v : 99);
    }
    return vernum;
}

} // namespace KAlarm

// vim: et sw=4:
