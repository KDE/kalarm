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
    : mCompatibility(rhs.mCompatibility)
{
}

CompatibilityAttribute* CompatibilityAttribute::clone() const
{
    return new CompatibilityAttribute(*this);
}

QByteArray CompatibilityAttribute::serialized() const
{
kDebug(5950)<<mCompatibility;
    return QByteArray::number(mCompatibility);
}

void CompatibilityAttribute::deserialize(const QByteArray& data)
{
    // Set default values
    mCompatibility = KAlarm::Calendar::Incompatible;

    const QList<QByteArray> items = data.simplified().split(' ');
kDebug(5950)<<"Data="<<data;
    if (!items.isEmpty())
    {
        // 0: calendar format compatibility
        bool ok;
        int c = items[0].toInt(&ok);
        if (!ok
        ||  (c != KAlarm::Calendar::Incompatible
          && c != KAlarm::Calendar::Current
          && c != KAlarm::Calendar::Convertible
          && c != KAlarm::Calendar::ByEvent))
        {
            kError() << "Invalid compatibility:" << c;
            return;
        }
        mCompatibility = static_cast<KAlarm::Calendar::Compat>(c);
    }
}

} // namespace KAlarm

// vim: et sw=4:
