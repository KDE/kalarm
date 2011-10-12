/*
 *  compatibilityattribute.cpp  -  Akonadi attribute holding Collection compatibility
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "compatibilityattribute.h"

#include <kdebug.h>

namespace KAlarm
{


class CompatibilityAttribute::Private
{
    public:
        Private()
            : mCompatibility(KACalendar::Incompatible),
              mVersion(IncompatibleFormat)
            {}

        KACalendar::Compat mCompatibility;    // calendar compatibility with current KAlarm format
        int                mVersion;          // KAlarm calendar format version
};
 
    
CompatibilityAttribute::CompatibilityAttribute()
    : d(new Private)
{
}

CompatibilityAttribute::CompatibilityAttribute(const CompatibilityAttribute& rhs)
    : Akonadi::Attribute(rhs),
      d(new Private(*rhs.d))
{
}

CompatibilityAttribute::~CompatibilityAttribute()
{
    delete d;
}

CompatibilityAttribute& CompatibilityAttribute::operator=(const CompatibilityAttribute& other)
{
    if (&other != this)
    {
        Attribute::operator=(other);
        *d = *other.d;
    }
    return *this;
}

CompatibilityAttribute* CompatibilityAttribute::clone() const
{
    return new CompatibilityAttribute(*this);
}

KACalendar::Compat CompatibilityAttribute::compatibility() const
{
    return d->mCompatibility;
}

void CompatibilityAttribute::setCompatibility(KACalendar::Compat c)
{
    d->mCompatibility = c;
}

int CompatibilityAttribute::version() const
{
    return d->mVersion;
}

void CompatibilityAttribute::setVersion(int v)
{
    d->mVersion = v;
}

QByteArray CompatibilityAttribute::type() const
{
    return name();
}

QByteArray CompatibilityAttribute::name()
{
    return "KAlarmCompatibility";
}

QByteArray CompatibilityAttribute::serialized() const
{
    QByteArray v = QByteArray::number(d->mCompatibility) + ' '
                 + QByteArray::number(d->mVersion);
    kDebug() << v;
    return v;
}

void CompatibilityAttribute::deserialize(const QByteArray& data)
{
    kDebug() << data;

    // Set default values
    d->mCompatibility = KACalendar::Incompatible;
    d->mVersion       = IncompatibleFormat;

    bool ok;
    const QList<QByteArray> items = data.simplified().split(' ');
    int count = items.count();
    int index = 0;
    if (count > index)
    {
        // 0: calendar format compatibility
        int c = items[index++].toInt(&ok);
        KACalendar::Compat AllCompat(KACalendar::Current | KACalendar::Converted | KACalendar::Convertible | KACalendar::Incompatible | KACalendar::Unknown);
        if (!ok  ||  (c & AllCompat) != c)
        {
            kError() << "Invalid compatibility:" << c;
            return;
        }
        d->mCompatibility = static_cast<KACalendar::Compat>(c);
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
        d->mVersion = c;
    }
}

} // namespace KAlarm

// vim: et sw=4:
