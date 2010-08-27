/*
 *  collectionattribute.cpp  -  Akonadi attribute holding Collection characteristics
 *  Program:  kalarm
 *  Copyright © 2010 by David Jarvie <djarvie@kde.org>
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

#include "collectionattribute.h"

#include <kdebug.h>

namespace KAlarm
{

CollectionAttribute::CollectionAttribute(const CollectionAttribute& rhs)
    : mBackgroundColour(rhs.mBackgroundColour),
      mStandard(rhs.mStandard),
      mCompatibility(rhs.mCompatibility),
      mEnabled(rhs.mEnabled)
{
}

CollectionAttribute* CollectionAttribute::clone() const
{
    return new CollectionAttribute(*this);
}

void CollectionAttribute::setEnabled(bool enabled)
{
    mEnabled = enabled;
    if (!enabled)
        mStandard = KAlarm::CalEvent::EMPTY;
}

bool CollectionAttribute::isStandard(KAlarm::CalEvent::Type type) const
{
    switch (type)
    {
        case KAlarm::CalEvent::ACTIVE:
        case KAlarm::CalEvent::ARCHIVED:
        case KAlarm::CalEvent::TEMPLATE:
            return mStandard & type;
        default:
            return false;
    }
}

void CollectionAttribute::setStandard(KAlarm::CalEvent::Type type, bool standard)
{
    switch (type)
    {
        case KAlarm::CalEvent::ACTIVE:
        case KAlarm::CalEvent::ARCHIVED:
        case KAlarm::CalEvent::TEMPLATE:
            if (standard)
                mStandard = static_cast<KAlarm::CalEvent::Types>(mStandard | type);
            else
                mStandard = static_cast<KAlarm::CalEvent::Types>(mStandard & ~type);
            break;
        default:
            break;
    }
}

QByteArray CollectionAttribute::serialized() const
{
    QByteArray v = QByteArray(mEnabled ? "1" : "0") + ' '
                 + QByteArray::number(mCompatibility) + ' '
                 + QByteArray::number(mStandard) + ' '
                 + QByteArray(mBackgroundColour.isValid() ? "1" : "0");
    if (mBackgroundColour.isValid())
        v += ' '
          + QByteArray::number(mBackgroundColour.red()) + ' '
          + QByteArray::number(mBackgroundColour.green()) + ' '
          + QByteArray::number(mBackgroundColour.blue()) + ' '
          + QByteArray::number(mBackgroundColour.alpha());
kDebug(5950)<<v;
    return v;
}

void CollectionAttribute::deserialize(const QByteArray& data)
{
    // Set default values
    mEnabled          = false;
    mStandard         = KAlarm::CalEvent::EMPTY;
    mCompatibility    = KAlarm::Calendar::Incompatible;
    mBackgroundColour = QColor();

    bool ok;
    int c[4];
    const QList<QByteArray> items = data.simplified().split(' ');
    int count = items.count();
kDebug(5950)<<"Size="<<count<<", data="<<data;
    if (count >= 1)
    {
        // 0: whether the collection is enabled
        c[0] = items[0].toInt(&ok);
        if (!ok)
        {
            kError() << "Invalid enabled:" << c[0];
            return;
        }
        mEnabled = c[0];
kDebug()<<"Enabled ->"<<mEnabled;
    }
    if (count >= 2)
    {
        // 1: calendar format compatibility
        c[0] = items[2].toInt(&ok);
        if (!ok  ||  (c[0] & ~(KAlarm::Calendar::Incompatible | KAlarm::Calendar::Current | KAlarm::Calendar::Convertible)))
        {
            kError() << "Invalid compatibility:" << c[0];
            return;
        }
        mCompatibility = static_cast<KAlarm::Calendar::Compat>(c[0]);
    }
    if (count >= 3)
    {
        // 2: type(s) of alarms for which the collection is the standard collection
        c[0] = items[1].toInt(&ok);
        if (!ok  ||  (c[0] & ~(KAlarm::CalEvent::ACTIVE | KAlarm::CalEvent::ARCHIVED | KAlarm::CalEvent::TEMPLATE)))
        {
            kError() << "Invalid alarm types:" << c[0];
            return;
        }
        if (mEnabled  &&  mCompatibility == KAlarm::Calendar::Current)
            mStandard = static_cast<KAlarm::CalEvent::Type>(c[0]);
    }
    if (count >= 4)
    {
        // 3: background color valid flag
        c[0] = items[3].toInt(&ok);
        if (!ok)
            return;
        if (c[0])
        {
            if (count < 8)
            {
                kError() << "Invalid number of background color elements";
                return;
            }
            // 4-7: background color elements
            for (int i = 0;  i < 4;  ++i)
            {
                c[i] = items[i + 4].toInt(&ok);
                if (!ok)
                    return;
            }
            mBackgroundColour.setRgb(c[0], c[1], c[2], c[3]);
        }
    }
}

} // namespace KAlarm

// vim: et sw=4:
