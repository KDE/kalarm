/*
 *  eventattribute.cpp  -  per-user attributes for individual events
 *  Program:  kalarm
 *  Copyright © 2010 by David Jarvie <djarvie@kde.org>
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

namespace KAlarm
{

EventAttribute* EventAttribute::clone() const
{
    return new EventAttribute(*this);
}

QByteArray EventAttribute::serialized() const
{
kDebug()<<QByteArray::number(mCommandError);
    return QByteArray::number(mCommandError);
}

void EventAttribute::deserialize(const QByteArray& data)
{
    // Set default values
    mCommandError = KAEvent::CMD_NO_ERROR;

    bool ok;
    int c[1];
    const QList<QByteArray> items = data.simplified().split(' ');
    switch (items.count())
    {
        case 1:
            c[0] = items[0].toInt(&ok);
            if (!ok  ||  (c[0] & ~(KAEvent::CMD_ERROR | KAEvent::CMD_ERROR_PRE | KAEvent::CMD_ERROR_POST)))
                return;
            mCommandError = static_cast<KAEvent::CmdErrType>(c[0]);
kDebug()<<"command error="<<mCommandError;
            break;

        default:
            break;
    }
}

} // namespace KAlarm

// vim: et sw=4:
