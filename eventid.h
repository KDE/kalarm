/*
 *  eventid.h  -  KAlarm unique event identifier for Akonadi
 *  Program:  kalarm
 *  Copyright © 2012,2014 by David Jarvie <djarvie@kde.org>
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

#ifndef EVENTID_H
#define EVENTID_H

#include <kalarmcal/kaevent.h>
#include <QPair>
#include <QDebug>

using namespace KAlarmCal;

/**
 * Unique event identifier for Akonadi.
 * This consists of the event UID within the individual calendar,
 * plus the collection ID.
 *
 * Note that the collection ID of the display calendar is -1, since
 * it is not an Akonadi calendar.
 */
struct EventId : public QPair<Akonadi::Collection::Id, QString>
{
    EventId() {}
    EventId(Akonadi::Collection::Id c, const QString& e)
        : QPair<Akonadi::Collection::Id, QString>(c, e) {}
    explicit EventId(const KAEvent& event)
        : QPair<Akonadi::Collection::Id, QString>(event.collectionId(), event.id()) {}
    /** Set by event ID and optional resource ID, in the format "[rid:]eid". */
    explicit EventId(const QString& resourceEventId);
    void clear()          { first = -1; second.clear(); }
    /** Return whether the instance contains any data. */
    bool isEmpty() const  { return second.isEmpty(); }

    Akonadi::Collection::Id collectionId() const  { return first; }
    QString                 eventId() const       { return second; }
    void                    setCollectionId(Akonadi::Collection::Id id)  { first = id; }
};

// Declare as a movable type (note that QString is movable).
Q_DECLARE_TYPEINFO(EventId, Q_MOVABLE_TYPE);

inline QDebug operator<<(QDebug s, const EventId& id)
{
    s.nospace() << "\"" << id.collectionId() << "::" << id.eventId().toLatin1().constData() << "\"";
    return s.space();
}

#endif // EVENTID_H

// vim: et sw=4:
