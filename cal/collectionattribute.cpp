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

namespace KAlarm
{

CollectionAttribute::CollectionAttribute(const CollectionAttribute& rhs)
    : mBackgroundColour(rhs.mBackgroundColour),
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
                 + QByteArray::number(mStandard) + ' '
                 + QByteArray::number(mCompatibility) + ' '
                 + QByteArray(mBackgroundColour.isValid() ? "1" : "0");
    if (mBackgroundColour.isValid())
        v += ' '
          + QByteArray::number(mBackgroundColour.red()) + ' '
          + QByteArray::number(mBackgroundColour.green()) + ' '
          + QByteArray::number(mBackgroundColour.blue()) + ' '
          + QByteArray::number(mBackgroundColour.alpha());
    return v;
}

void CollectionAttribute::deserialize(const QByteArray& data)
{
    // Set default values
    mEnabled                         = false;
    mStandard                        = KAlarm::CalEvent::EMPTY;
    KAlarm::CalEvent::Types standard = mStandard;
    mCompatibility                   = KAlarm::Calendar::Incompatible;
    mBackgroundColour                = QColor();
    QColor bgColour                  = mBackgroundColour;

    bool ok;
    int c[4];
    const QList<QByteArray> items = data.simplified().split(' ');
    switch (items.count())
    {
        case 8:    // background color elements
            for (int i = 0;  i < 4;  ++i)
            {
                c[i] = items[i + 4].toInt(&ok);
                if (!ok)
                    return;
            }
            bgColour.setRgb(c[0], c[1], c[2], c[3]);
            // fall through to '4'
        case 4:    // background color valid flag
            c[0] = items[3].toInt(&ok);
            if (!ok)
                return;
            if (c[0])
                mBackgroundColour = bgColour;
            break;
            // fall through to '3'
        case 3:    // calendar format compatibility
            c[0] = items[2].toInt(&ok);
            if (!ok  ||  (c[0] & ~(KAlarm::Calendar::Incompatible | KAlarm::Calendar::Current | KAlarm::Calendar::Convertible)))
                return;
            mCompatibility = static_cast<KAlarm::Calendar::Compat>(c[0]);
            // fall through to '2'
        case 2:    // type(s) of alarms for which the collection is the standard collection
            c[0] = items[1].toInt(&ok);
            if (!ok  ||  (c[0] & ~(KAlarm::CalEvent::ACTIVE | KAlarm::CalEvent::ARCHIVED | KAlarm::CalEvent::TEMPLATE)))
                return;
            standard = static_cast<KAlarm::CalEvent::Type>(c[0]);
            // fall through to '1'
        case 1:    // whether the collection is enabled
            c[0] = items[0].toInt(&ok);
            if (!ok)
                return;
            mEnabled = c[0];
            if (mEnabled)
                mStandard = standard;
            break;

        default:
            break;
    }

    if (!mEnabled  ||  mCompatibility != KAlarm::Calendar::Current)
        mStandard = KAlarm::CalEvent::EMPTY;
}

} // namespace KAlarm

// vim: et sw=4:
