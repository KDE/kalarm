/*
 *  resourcetype.cpp  -  base class for an alarm calendar resource type
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2019-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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

bool ResourceType::failed() const
{
    return mFailed  ||  !isValid();
}

ResourceId ResourceType::displayId() const
{
    return id();
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

    // Find the highest priority alarm type.
    // Note that resources currently only contain a single alarm type.
    CalEvent::Type type;
    QColor colour;        // default to invalid colour
    if (types & CalEvent::ACTIVE)
    {
        type   = CalEvent::ACTIVE;
        colour = KColorScheme(QPalette::Active).foreground(KColorScheme::NormalText).color();
    }
    else if (types & CalEvent::ARCHIVED)
    {
        type   = CalEvent::ARCHIVED;
        colour = Preferences::archivedColour();
    }
    else if (types & CalEvent::TEMPLATE)
    {
        type   = CalEvent::TEMPLATE;
        colour = KColorScheme(QPalette::Active).foreground(KColorScheme::LinkText).color();
    }
    else
        type = CalEvent::EMPTY;

    if (colour.isValid()  &&  !isWritable(type))
        return KColorUtils::lighten(colour, 0.2);
    return colour;
}

bool ResourceType::isCompatible() const
{
    return compatibility() == KACalendar::Current;
}

KACalendar::Compat ResourceType::compatibility() const
{
    QString versionString;
    return compatibilityVersion(versionString);
}

/******************************************************************************
* Return all events belonging to this resource, for enabled alarm types.
*/
QList<KAEvent> ResourceType::events() const
{
    // Remove any events with disabled alarm types.
    const CalEvent::Types types = enabledTypes();
    QList<KAEvent> events;
    for (auto it = mEvents.begin();  it != mEvents.end();  ++it)
    {
        if (it.value().category() & types)
            events += it.value();
    }
    return events;
}

/******************************************************************************
* Return the event with the given ID, provided its alarm type is enabled for
* the resource.
*/
KAEvent ResourceType::event(const QString& eventId, bool allowDisabled) const
{
    auto it = mEvents.constFind(eventId);
    if (it != mEvents.constEnd()
    &&  (allowDisabled || (it.value().category() & enabledTypes())))
        return it.value();
    return KAEvent();
}

/******************************************************************************
* Return whether the resource contains the event whose ID is given, and if the
* event's alarm type is enabled for the resource.
*/
bool ResourceType::containsEvent(const QString& eventId) const
{
    auto it = mEvents.constFind(eventId);
    return it != mEvents.constEnd()
       &&  (it.value().category() & enabledTypes());
}

/******************************************************************************
* Called when the user changes the start-of-day time.
* Adjust the start times of all date-only alarms' recurrences.
*/
void ResourceType::adjustStartOfDay()
{
    KAEvent::List events;
    for (auto it = mEvents.begin();  it != mEvents.end();  ++it)
        events += &it.value();
    KAEvent::adjustStartOfDay(events);
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

void ResourceType::removeResource(ResourceId id)
{
    // Set the resource instance invalid, to ensure that any other references
    // to it now see an invalid resource.
    Resource res = Resources::resource(id);
    ResourceType* tres = res.resource<ResourceType>();
    if (tres)
        tres->mId = -1;   // set the resource instance invalid
    Resources::removeResource(id);
}

/******************************************************************************
* To be called when the resource has loaded, to update the list of loaded
* events for the resource.
* Added, updated and deleted events are notified, only for enabled alarm types.
*/
void ResourceType::setLoadedEvents(QHash<QString, KAEvent>& newEvents)
{
    qCDebug(KALARM_LOG) << "ResourceType::setLoadedEvents: count" << newEvents.count();
    const CalEvent::Types types = enabledTypes();

    // Replace existing events with the new ones, and find events which no
    // longer exist.
    QStringList    eventsToDelete;
    QList<KAEvent> eventsToNotifyDelete;
    for (auto it = mEvents.begin();  it != mEvents.end();  ++it)
    {
        const QString& id = it.key();
        auto newit = newEvents.find(id);
        if (newit == newEvents.end())
        {
            eventsToDelete << id;
            if (it.value().category() & types)
                eventsToNotifyDelete << it.value();   // this event no longer exists
        }
        else
        {
            KAEvent& event = it.value();
            bool changed = !event.compare(newit.value(), KAEvent::Compare::Id | KAEvent::Compare::CurrentState);
            event = newit.value();   // update existing event
            event.setResourceId(mId);
            newEvents.erase(newit);
            if (changed  &&  (event.category() & types))
                Resources::notifyEventUpdated(this, event);
        }
    }

    // Delete events which no longer exist.
    if (!eventsToNotifyDelete.isEmpty())
        Resources::notifyEventsToBeRemoved(this, eventsToNotifyDelete);
    for (const QString& id : std::as_const(eventsToDelete))
        mEvents.remove(id);
    if (!eventsToNotifyDelete.isEmpty())
        Resources::notifyEventsRemoved(this, eventsToNotifyDelete);

    // Add new events.
    for (auto newit = newEvents.begin();  newit != newEvents.end(); )
    {
        newit.value().setResourceId(mId);
        mEvents[newit.key()] = newit.value();
        if (newit.value().category() & types)
            ++newit;
        else
            newit = newEvents.erase(newit);   // remove disabled event from notification list
    }
    if (!newEvents.isEmpty())
        Resources::notifyEventsAdded(this, newEvents.values());

    newEvents.clear();
    setLoaded(true);
}

/******************************************************************************
* To be called when events have been created or updated, to amend them in the
* resource's list.
*/
void ResourceType::setUpdatedEvents(const QList<KAEvent>& events, bool notify)
{
    const CalEvent::Types types = enabledTypes();
    mEventsAdded.clear();
    mEventsUpdated.clear();
    for (const KAEvent& event : events)
    {
        auto it = mEvents.find(event.id());
        if (it == mEvents.end())
        {
            KAEvent& ev = mEvents[event.id()];
            ev = event;
            ev.setResourceId(mId);
            if (event.category() & types)
                mEventsAdded += ev;
        }
        else
        {
            KAEvent& ev = it.value();
            bool changed = !ev.compare(event, KAEvent::Compare::Id | KAEvent::Compare::CurrentState);
            ev = event;   // update existing event
            ev.setResourceId(mId);
            if (changed  &&  (event.category() & types))
            {
                if (notify)
                    Resources::notifyEventUpdated(this, event);
                else
                    mEventsUpdated += event;
            }
        }
    }
    if (notify  &&  !mEventsAdded.isEmpty())
        Resources::notifyEventsAdded(this, mEventsAdded);
}

/******************************************************************************
* Notifies added and updated events, after setUpdatedEvents() was called with
* notify = false.
*/
void ResourceType::notifyUpdatedEvents()
{
    for (const KAEvent& event : std::as_const(mEventsUpdated))
        Resources::notifyEventUpdated(this, event);
    mEventsUpdated.clear();

    if (!mEventsAdded.isEmpty())
        Resources::notifyEventsAdded(this, mEventsAdded);
    mEventsAdded.clear();
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
        if (mEvents.constFind(event.id()) != mEvents.constEnd())
        {
            eventsToDelete += event.id();
            if (event.category() & types)
                eventsToNotify += event;
        }
    }
    if (!eventsToNotify.isEmpty())
        Resources::notifyEventsToBeRemoved(this, eventsToNotify);
    for (const QString& id : std::as_const(eventsToDelete))
        mEvents.remove(id);
    if (!eventsToNotify.isEmpty())
        Resources::notifyEventsRemoved(this, eventsToNotify);
}

void ResourceType::setLoaded(bool loaded) const
{
    if (loaded != mLoaded)
    {
        mLoaded = loaded;
        if (loaded)
            Resources::notifyResourcePopulated(this);
    }
}

void ResourceType::setFailed()
{
    mFailed = true;
}

QString ResourceType::storageTypeString(StorageType type)
{
    switch (type)
    {
        case File:
        case Directory:
            return storageTypeStr(true, (type == File), true);
        default:
            return QString();
    }
}

QString ResourceType::storageTypeStr(bool description, bool file, bool local)
{
    if (description)
        return file ? i18nc("@item:inlistbox", "KAlarm Calendar File") : i18nc("@item:inlistbox", "KAlarm Calendar Directory");
    return (file && local)  ? i18nc("@item:intext What a resource is stored in", "File")
         : (file && !local) ? i18nc("@item:intext What a resource is stored in", "URL")
         : (!file && local) ? i18nc("@item:intext What a resource is stored in (directory in filesystem)", "Directory")
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
