/*
 *  eventattribute.cpp  -  per-user attributes for individual events
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

#include "eventattribute.h"

#include <QList>
#include <QByteArray>

namespace KAlarmCal
{

class EventAttribute::Private
{
public:
    Private() : mCommandError(KAEvent::CMD_NO_ERROR) {}

    KAEvent::CmdErrType mCommandError;         // the last command execution error for the alarm
};

EventAttribute::EventAttribute()
    : d(new Private)
{
}

EventAttribute::EventAttribute(const EventAttribute &rhs)
    : Akonadi::Attribute(rhs),
      d(new Private(*rhs.d))
{
}

EventAttribute::~EventAttribute()
{
    delete d;
}

EventAttribute &EventAttribute::operator=(const EventAttribute &other)
{
    if (&other != this) {
        Attribute::operator=(other);
        *d = *other.d;
    }
    return *this;
}

KAEvent::CmdErrType EventAttribute::commandError() const
{
    return d->mCommandError;
}

void EventAttribute::setCommandError(KAEvent::CmdErrType err)
{
    d->mCommandError = err;
}

QByteArray EventAttribute::type() const
{
    return "KAlarmEvent";
}

EventAttribute *EventAttribute::clone() const
{
    return new EventAttribute(*this);
}

QByteArray EventAttribute::serialized() const
{
    QByteArray v = QByteArray::number(d->mCommandError);
    qDebug() << v;
    return v;
}

void EventAttribute::deserialize(const QByteArray &data)
{
    qDebug() << data;

    // Set default values
    d->mCommandError = KAEvent::CMD_NO_ERROR;

    bool ok;
    int c[1];
    const QList<QByteArray> items = data.simplified().split(' ');
    switch (items.count()) {
    case 1:
        c[0] = items[0].toInt(&ok);
        if (!ok  || (c[0] & ~(KAEvent::CMD_ERROR | KAEvent::CMD_ERROR_PRE | KAEvent::CMD_ERROR_POST))) {
            return;
        }
        d->mCommandError = static_cast<KAEvent::CmdErrType>(c[0]);
        break;

    default:
        break;
    }
}

} // namespace KAlarmCal

