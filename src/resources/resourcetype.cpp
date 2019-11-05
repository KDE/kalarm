/*
 *  resourcetype.cpp  -  base class for an alarm calendar resource type
 *  Program:  kalarm
 *  Copyright © 2019 David Jarvie <djarvie@kde.org>
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

#include "resourcetype.h"

#include "resources.h"
#include "preferences.h"
#include "kalarm_debug.h"

#include <KColorScheme>
#include <KColorUtils>
#include <KLocalizedString>

ResourceType::ResourceType(ResourceId id)
    : mId(id)
{
}

ResourceType::~ResourceType()
{
}

bool ResourceType::isEnabled(CalEvent::Type type) const
{
    return (type == CalEvent::EMPTY) ? enabledTypes() : enabledTypes() & type;
}

bool ResourceType::isWritable(CalEvent::Type type) const
{
    return writableStatus(type) == 1;
}

/******************************************************************************
* Return the foreground colour for displaying a resource, based on the alarm
* types which it contains, and on whether it is fully writable.
*/
QColor ResourceType::foregroundColour(CalEvent::Types types) const
{
    if (types == CalEvent::EMPTY)
        types = alarmTypes();
    else
        types &= alarmTypes();

//TODO: Should this look for the first writable alarm type?
    CalEvent::Type type;
    if (types & CalEvent::ACTIVE)
        type = CalEvent::ACTIVE;
    else if (types & CalEvent::ARCHIVED)
        type = CalEvent::ARCHIVED;
    else if (types & CalEvent::TEMPLATE)
        type = CalEvent::TEMPLATE;
    else
        type = CalEvent::EMPTY;

    QColor colour;
    switch (type)
    {
        case CalEvent::ACTIVE:
            colour = KColorScheme(QPalette::Active).foreground(KColorScheme::NormalText).color();
            break;
        case CalEvent::ARCHIVED:
            colour = Preferences::archivedColour();
            break;
        case CalEvent::TEMPLATE:
            colour = KColorScheme(QPalette::Active).foreground(KColorScheme::LinkText).color();
            break;
        default:
            break;
    }
    if (colour.isValid()  &&  !isWritable(type))
        return KColorUtils::lighten(colour, 0.2);
    return colour;
}

bool ResourceType::isCompatible() const
{
    return compatibility() == KACalendar::Current;
}

/******************************************************************************
* Return all events belonging to this resource, for enabled alarm types.
*/
QList<KAEvent> ResourceType::events() const
{
    // Remove any events with disabled alarm types.
qDebug()<<"ResourceType::events(): total"<<mEvents.count();
    const CalEvent::Types types = enabledTypes();
    QList<KAEvent> events;
    for (auto it = mEvents.begin();  it != mEvents.end();  ++it)
    {
        if (it.value().category() & types)
            events += it.value();
    }
qDebug()<<"ResourceType::events(): enabled"<<events.count();
    return events;
}

void ResourceType::notifyDeletion()
{
    mBeingDeleted = true;
}

bool ResourceType::isBeingDeleted() const
{
    return mBeingDeleted;
}

bool ResourceType::addResource(ResourceType* type, Resource& resource)
{
    return Resources::addResource(type, resource);
}

/******************************************************************************
* To be called when the resource has loaded, to update the list of loaded
* events for the resource.
*/
void ResourceType::setLoadedEvents(QHash<QString, KAEvent>& newEvents)
{
    // Replace existing events with the new ones, and find events which no
    // longer exist.
    QList<KAEvent> eventsToDelete;
    QVector<decltype(mEvents)::iterator> iteratorsToDelete;
    for (auto it = mEvents.begin();  it != mEvents.end();  ++it)
    {
        const QString& id = it.key();
        auto newit = newEvents.find(id);
        if (newit == newEvents.end())
        {
            eventsToDelete << it.value();   // this event no longer exists
            iteratorsToDelete << it;
        }
        else
        {
            KAEvent& event = it.value();
            bool changed = !event.compare(newit.value(), KAEvent::Compare::Id | KAEvent::Compare::CurrentState);
            event = newit.value();   // update existing event
            newEvents.erase(newit);
            if (changed)
                Resources::notifyEventUpdated(this, event);
        }
    }

    // Delete events which no longer exist.
    Resources::notifyEventsToBeRemoved(this, eventsToDelete);
    for (auto it : qAsConst(iteratorsToDelete))
        mEvents.erase(it);

    // Add new events.
    for (auto newit = newEvents.constBegin();  newit != newEvents.constEnd();  ++newit)
        mEvents[newit.key()] = newit.value();
qDebug()<<"ResourceType::setLoadedEvents:"<<id()<<" count:"<<mEvents.count()<<"(newEvents:"<<newEvents.count()<<")";
    Resources::notifyEventsAdded(this, newEvents.values());

    newEvents.clear();
    setLoaded(true);
    Resources::notifyResourceLoaded(this);
}

/******************************************************************************
* To be called when events have been created or updated, to amend them in the
* resource's list.
*/
void ResourceType::setUpdatedEvents(const QList<KAEvent>& events)
{
    const CalEvent::Types types = enabledTypes();
    QList<KAEvent> eventsAdded;
    for (const KAEvent& event : events)
    {
        auto it = mEvents.find(event.id());
        if (it == mEvents.end())
        {
            mEvents[event.id()] = event;
            if (event.category() & types)
                eventsAdded += event;
        }
        else
        {
            KAEvent& ev = it.value();
            bool changed = !ev.compare(event, KAEvent::Compare::Id | KAEvent::Compare::CurrentState);
            ev = event;   // update existing event
            if (changed  &&  (event.category() & types))
                Resources::notifyEventUpdated(this, event);
        }
    }
    if (!eventsAdded.isEmpty())
        Resources::notifyEventsAdded(this, eventsAdded);
}

/******************************************************************************
* To be called when events have been deleted, to delete them from the
* resource's list.
*/
void ResourceType::setDeletedEvents(const QList<KAEvent>& events)
{
    const CalEvent::Types types = enabledTypes();
    QStringList eventsToDelete;
    QList<KAEvent> eventsToNotify;
    for (const KAEvent& event : events)
    {
        QHash<QString, KAEvent>::iterator it = mEvents.find(event.id());
        if (it != mEvents.end())
        {
            eventsToDelete += event.id();
            if (event.category() & types)
                eventsToNotify += event;
        }
    }
    Resources::notifyEventsToBeRemoved(this, eventsToNotify);
    for (const QString& id : eventsToDelete)
        mEvents.remove(id);
}

void ResourceType::setLoaded(bool loaded) const
{
    mLoaded = loaded;
    Resources::notifyResourceLoaded(this);
}

QString ResourceType::storageTypeStr(bool description, bool file, bool local) const
{
    if (description)
        return file ? i18nc("@info", "KAlarm Calendar File") : i18nc("@info", "KAlarm Calendar Directory");
    return (file && local)  ? i18nc("@info", "File")
         : (file && !local) ? i18nc("@info", "URL")
         : (!file && local) ? i18nc("@info Directory in filesystem", "Directory")
         : QString();
}

ResourceType* ResourceType::data(Resource& resource)
{
    return resource.mResource.data();
}

const ResourceType* ResourceType::data(const Resource& resource)
{
    return resource.mResource.data();
}

// vim: et sw=4:
