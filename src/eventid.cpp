/*
 *  eventid.cpp  -  KAlarm unique event identifier for resources
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2012-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "eventid.h"

#include "resources/resources.h"

#include <QRegularExpression>

/** Set by event ID prefixed by optional resource ID, in the format "[rid:]eid". */
EventId::EventId(const QString& resourceEventId)
{
    const QString resourceIdString = extractIDs(resourceEventId, mEventId);
    if (!resourceIdString.isEmpty())
        mResourceId = getResourceId(resourceIdString);  // convert the resource ID string
}

bool EventId::operator==(const EventId& other) const
{
    return mEventId == other.mEventId  &&  mResourceId == other.mResourceId;
}

ResourceId EventId::resourceDisplayId() const
{
    return (mResourceId > 0) ? (mResourceId & ~ResourceType::IdFlag) : mResourceId;
}

QString EventId::extractIDs(const QString& resourceEventId, QString& eventId)
{
    QRegularExpression rx(QStringLiteral("^(\\w+):(.*)$"));
    QRegularExpressionMatch rxmatch = rx.match(resourceEventId);
    if (!rxmatch.hasMatch())
    {
        eventId = resourceEventId;   // no resource ID supplied
        return QString();
    }

    // A resource ID has been supplied
    eventId = rxmatch.captured(2);
    return rxmatch.captured(1);
}

ResourceId EventId::getResourceId(const QString& resourceIdString)
{
    // Check if a resource configuration name has been supplied.
    Resource res = Resources::resourceForConfigName(resourceIdString);
    if (res.isValid())
        return res.id();

    // Check if a resource ID number has been supplied.
    bool ok;
    const ResourceId id = resourceIdString.toLongLong(&ok);
    if (ok)
    {
        res = Resources::resourceFromDisplayId(id);
        if (res.isValid())
            return res.id();
    }
    return -1;
}

// vim: et sw=4:
