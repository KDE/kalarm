/*
 *  collectionattribute.cpp  -  Akonadi attribute holding Collection characteristics
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  Copyright © 2010-2011 by David Jarvie <djarvie@kde.org>
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

#include "collectionattribute.h"

#include <qdebug.h>

namespace KAlarmCal
{

class CollectionAttribute::Private
{
public:
    Private()
      : mEnabled(CalEvent::EMPTY),
        mStandard(CalEvent::EMPTY),
        mKeepFormat(false)  {}

    QColor           mBackgroundColour; // background color for collection and its alarms
    CalEvent::Types  mEnabled;          // which alarm types the collection is enabled for
    CalEvent::Types  mStandard;         // whether the collection is a standard collection
    bool             mKeepFormat;       // whether user has chosen to keep old calendar storage format
};

CollectionAttribute::CollectionAttribute()
    : d(new Private)
{
}

CollectionAttribute::CollectionAttribute(const CollectionAttribute &rhs)
    : Akonadi::Attribute(rhs),
      d(new Private(*rhs.d))
{
}

CollectionAttribute::~CollectionAttribute()
{
    delete d;
}

CollectionAttribute &CollectionAttribute::operator=(const CollectionAttribute &other)
{
    if (&other != this) {
        Attribute::operator=(other);
        *d = *other.d;
    }
    return *this;
}

CollectionAttribute *CollectionAttribute::clone() const
{
    return new CollectionAttribute(*this);
}

bool CollectionAttribute::isEnabled(CalEvent::Type type) const
{
    return d->mEnabled & type;
}

CalEvent::Types CollectionAttribute::enabled() const
{
    return d->mEnabled;
}

void CollectionAttribute::setEnabled(CalEvent::Type type, bool enabled)
{
    switch (type) {
        case CalEvent::ACTIVE:
        case CalEvent::ARCHIVED:
        case CalEvent::TEMPLATE:
            break;
        default:
            return;
    }
    if (enabled) {
        d->mEnabled |= type;
    } else {
        d->mEnabled  &= ~type;
        d->mStandard &= ~type;
    }
}

void CollectionAttribute::setEnabled(CalEvent::Types types)
{
    d->mEnabled  = types & (CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
    d->mStandard &= d->mEnabled;
}

bool CollectionAttribute::isStandard(CalEvent::Type type) const
{
    switch (type) {
        case CalEvent::ACTIVE:
        case CalEvent::ARCHIVED:
        case CalEvent::TEMPLATE:
            return d->mStandard & type;
        default:
            return false;
    }
}

CalEvent::Types CollectionAttribute::standard() const
{
    return d->mStandard;
}

void CollectionAttribute::setStandard(CalEvent::Type type, bool standard)
{
    switch (type) {
        case CalEvent::ACTIVE:
        case CalEvent::ARCHIVED:
        case CalEvent::TEMPLATE:
            if (standard) {
                d->mStandard = static_cast<CalEvent::Types>(d->mStandard | type);
            } else {
                d->mStandard = static_cast<CalEvent::Types>(d->mStandard & ~type);
            }
            break;
        default:
            break;
    }
}

void CollectionAttribute::setStandard(CalEvent::Types types)
{
    d->mStandard = types & (CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
}

QColor CollectionAttribute::backgroundColor() const
{
    return d->mBackgroundColour;
}

void CollectionAttribute::setBackgroundColor(const QColor &c)
{
    d->mBackgroundColour = c;
}

bool CollectionAttribute::keepFormat() const
{
    return d->mKeepFormat;
}

void CollectionAttribute::setKeepFormat(bool keep)
{
    d->mKeepFormat = keep;
}

QByteArray CollectionAttribute::type() const
{
    return name();
}

QByteArray CollectionAttribute::name()
{
    return "KAlarmCollection";
}

QByteArray CollectionAttribute::serialized() const
{
    QByteArray v = QByteArray::number(d->mEnabled) + ' '
                 + QByteArray::number(d->mStandard) + ' '
                 + QByteArray(d->mKeepFormat ? "1" : "0") + ' '
                 + QByteArray(d->mBackgroundColour.isValid() ? "1" : "0");
    if (d->mBackgroundColour.isValid())
        v += ' '
           + QByteArray::number(d->mBackgroundColour.red()) + ' '
           + QByteArray::number(d->mBackgroundColour.green()) + ' '
           + QByteArray::number(d->mBackgroundColour.blue()) + ' '
           + QByteArray::number(d->mBackgroundColour.alpha());
    qDebug() << v;
    return v;
}

void CollectionAttribute::deserialize(const QByteArray &data)
{
    qDebug() << data;

    // Set default values
    d->mEnabled          = CalEvent::EMPTY;
    d->mStandard         = CalEvent::EMPTY;
    d->mBackgroundColour = QColor();
    d->mKeepFormat       = false;

    bool ok;
    int c[4];
    const QList<QByteArray> items = data.simplified().split(' ');
    const int count = items.count();
    int index = 0;
    if (count > index) {
        // 0: type(s) of alarms for which the collection is enabled
        c[0] = items[index++].toInt(&ok);
        if (!ok  || (c[0] & ~(CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE))) {
            qCritical() << "Invalid alarm types:" << c[0];
            return;
        }
        d->mEnabled = static_cast<CalEvent::Types>(c[0]);
    }
    if (count > index) {
        // 1: type(s) of alarms for which the collection is the standard collection
        c[0] = items[index++].toInt(&ok);
        if (!ok  || (c[0] & ~(CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE))) {
            qCritical() << "Invalid alarm types:" << c[0];
            return;
        }
        if (d->mEnabled) {
            d->mStandard = static_cast<CalEvent::Types>(c[0]);
        }
    }
    if (count > index) {
        // 2: keep old calendar storage format
        c[0] = items[index++].toInt(&ok);
        if (!ok) {
            return;
        }
        d->mKeepFormat = c[0];
    }
    if (count > index) {
        // 3: background color valid flag
        c[0] = items[index++].toInt(&ok);
        if (!ok) {
            return;
        }
        if (c[0]) {
            if (count < index + 4) {
                qCritical() << "Invalid number of background color elements";
                return;
            }
            // 4-7: background color elements
            for (int i = 0;  i < 4;  ++i) {
                c[i] = items[index++].toInt(&ok);
                if (!ok) {
                    return;
                }
            }
            d->mBackgroundColour.setRgb(c[0], c[1], c[2], c[3]);
        }
    }
}

} // namespace KAlarmCal
