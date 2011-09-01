/*
 *  compatibilityattribute.cpp  -  Akonadi attribute holding Collection compatibility
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
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

#include "compatibilityattribute.h"

#include <kdebug.h>

namespace KAlarm
{

CompatibilityAttribute::CompatibilityAttribute(const CompatibilityAttribute& rhs)
    : mCompatibility(rhs.mCompatibility),
      mVersion(rhs.mVersion)
{
}

CompatibilityAttribute* CompatibilityAttribute::clone() const
{
    return new CompatibilityAttribute(*this);
}

QByteArray CompatibilityAttribute::serialized() const
{
    QByteArray v = QByteArray::number(mCompatibility) + ' '
                 + QByteArray::number(mVersion);
kDebug(5950)<<v;
    return v;
}

void CompatibilityAttribute::deserialize(const QByteArray& data)
{
    // Set default values
    mCompatibility = KAlarm::Calendar::Incompatible;
    mVersion       = KAlarm::IncompatibleFormat;

    bool ok;
    const QList<QByteArray> items = data.simplified().split(' ');
    int count = items.count();
    int index = 0;
kDebug(5950)<<"Size="<<count<<", data="<<data;
    if (count > index)
    {
        // 0: calendar format compatibility
        int c = items[index++].toInt(&ok);
        KAlarm::Calendar::Compat AllCompat(KAlarm::Calendar::Current | KAlarm::Calendar::Converted | KAlarm::Calendar::Convertible | KAlarm::Calendar::Incompatible | KAlarm::Calendar::Unknown);
        if (!ok  ||  (c & AllCompat) != c)
        {
            kError() << "Invalid compatibility:" << c;
            return;
        }
        mCompatibility = static_cast<KAlarm::Calendar::Compat>(c);
    }
    if (count > index)
    {
        // 1: KAlarm calendar version number
        int c = items[index++].toInt(&ok);
        if (!ok)
        {
            kError() << "Invalid version:" << c;
            return;
        }
        mVersion = c;
    }
}

} // namespace KAlarm

// vim: et sw=4:
