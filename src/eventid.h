/*
 *  eventid.h  -  KAlarm unique event identifier for resources
 *  Program:  kalarm
 *  Copyright © 2012-2020 David Jarvie <djarvie@kde.org>
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

#include "kalarm_debug.h"

#include <KAlarmCal/KAEvent>

using namespace KAlarmCal;

/**
 * Unique event identifier for resources.
 * This consists of the event UID within the individual calendar, plus the
 * resource ID.
 *
 * Note that the resource ID of the display calendar is -1, since it is not a
 * resources calendar.
 */
class EventId
{
public:
    EventId()   {}
    EventId(ResourceId c, const QString& e)
        : mEventId(e)
        , mResourceId(c)
    {}
    explicit EventId(const KAEvent& event)
        : mEventId(event.id())
        , mResourceId(event.resourceId())
    {}

    /** Set by event ID prefixed by optional resource ID, in the format "[rid:]eid".
     *  "rid" can be the resource configuration name, or the resource ID number in
     *  string format.
     *  @note  Resources must have been created before calling this method;
     *         otherwise, the resource ID will be invalid (-1).
     */
    explicit EventId(const QString& resourceEventId);

    bool operator==(const EventId&) const;
    bool operator!=(const EventId& other) const   { return !operator==(other); }

    void clear()          { mResourceId = -1; mEventId.clear(); }

    /** Return whether the instance contains any data. */
    bool isEmpty() const  { return mEventId.isEmpty(); }

    ResourceId resourceId() const            { return mResourceId; }
    ResourceId resourceDisplayId() const;
    QString    eventId() const               { return mEventId; }
    void       setResourceId(ResourceId id)  { mResourceId = id; }

    /** Extract the resource and event ID strings from an ID in the format "[rid:]eid".
     *  "rid" can be the resource configuration name, or the resource ID number in
     *  string format.
     *  @param resourceEventId  Full ID "[rid:]eid"
     *  @param eventId          Receives the event ID "eid"
     *  @return  The resource ID "rid".
     */
    static QString extractIDs(const QString& resourceEventId, QString& eventId);

    /** Get the numerical resource ID from a resource ID string.
     *  The string can be the resource configuration name, or the resource ID
     *  number in string format.
     *  @note  Resources must have been created before calling this function;
     *         otherwise, the returned resource ID will be invalid (-1).
     *
     *  @param resourceIdString  Resource ID string "rid"
     *  @return  The resource ID, or -1 if not found.
     */
    static ResourceId getResourceId(const QString& resourceIdString);

private:
    QString    mEventId;
    ResourceId mResourceId {-1};
};

// Declare as a movable type (note that QString is movable).
Q_DECLARE_TYPEINFO(EventId, Q_MOVABLE_TYPE);

inline uint qHash(const EventId& eid, uint seed)
{
    uint h1 = qHash(eid.eventId(), seed);
    uint h2 = qHash(eid.resourceId(), seed);
    return ((h1 << 16) | (h1 >> 16)) ^ h2 ^ seed;

}

inline QDebug operator<<(QDebug s, const EventId& id)
{
    s.nospace() << "\"" << id.resourceDisplayId() << "::" << id.eventId().toLatin1().constData() << "\"";
    return s.space();
}

#endif // EVENTID_H

// vim: et sw=4:
