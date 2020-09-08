/*
 *  eventattribute.cpp  -  per-user attributes for individual events
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  SPDX-FileCopyrightText: 2010-2011 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "eventattribute.h"
#include "kalarmcal_debug.h"

#include <QList>
#include <QByteArray>

namespace KAlarmCal
{

class Q_DECL_HIDDEN EventAttribute::Private
{
public:
    KAEvent::CmdErrType mCommandError = KAEvent::CMD_NO_ERROR;         // the last command execution error for the alarm
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
    static const QByteArray attType("KAlarmEvent");
    return attType;
}

EventAttribute *EventAttribute::clone() const
{
    return new EventAttribute(*this);
}

QByteArray EventAttribute::serialized() const
{
    const QByteArray v = QByteArray::number(d->mCommandError);
    qCDebug(KALARMCAL_LOG) << v;
    return v;
}

void EventAttribute::deserialize(const QByteArray &data)
{
    qCDebug(KALARMCAL_LOG) << data;

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

// vim: et sw=4:
