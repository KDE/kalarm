/*
 *  eventid.cpp  -  KAlarm unique event identifier for Akonadi
 *  Program:  kalarm
 *  Copyright © 2012 by David Jarvie <djarvie@kde.org>
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

#ifdef USE_AKONADI

#include "eventid.h"
#include "collectionmodel.h"

#include <kdebug.h>
#include <QString>
#include <QRegExp>

/** Set by event ID and optional resource ID, in the format "[rid:]eid". */
EventId::EventId(const QString& resourceEventId)
{
    bool resourceOk = false;
    QRegExp rx(QLatin1String("^\\w+:"));
    if (rx.indexIn(resourceEventId) == 0)
    {
        // A resource ID has been supplied, so use it
        int n = rx.matchedLength();
        Akonadi::Collection c = CollectionControlModel::collectionForResource(resourceEventId.left(n - 1));
        {
            first  = c.id();
            second = resourceEventId.mid(n);
            resourceOk = true;
        }
    }
    if (!resourceOk)
    {
        // Only an event ID has been supplied (or the syntax was invalid)
        first  = -1;
        second = resourceEventId;
    }
}

#endif

// vim: et sw=4:
