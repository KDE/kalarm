/*
 *  eventid.h  -  KAlarm unique event identifier for resources
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2012-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

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
     *  @note  The resource ID number is the ID as used in the config and as known
     *         to the user, not the internal ID (where different).
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
     *  @note  The resource ID number is the ID as used in the config and as known
     *         to the user, not the internal ID (where different).
     *
     *  @param resourceEventId  Full ID "[rid:]eid"
     *  @param eventId          Receives the event ID "eid"
     *  @return  The resource ID "rid" (see note above).
     */
    static QString extractIDs(const QString& resourceEventId, QString& eventId);

    /** Get the numerical resource ID from a resource ID string.
     *  The string can be the resource configuration name, or the resource ID
     *  number in string format.
     *  @note  The resource ID number is the ID as used in the config and as known
     *         to the user, not the internal ID (where different).
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


// vim: et sw=4:
