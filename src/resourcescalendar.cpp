/*
 *  resourcescalendar.cpp  -  KAlarm resources calendar file access
 *  Program:  kalarm
 *  Copyright © 2001-2020 David Jarvie <djarvie@kde.org>
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

#include "resourcescalendar.h"

#include "kalarm.h"
#include "resources/resources.h"
#include "kalarm_debug.h"

#include <KCalendarCore/ICalFormat>

using namespace KAlarmCal;


ResourcesCalendar* ResourcesCalendar::mInstance = nullptr;


/******************************************************************************
* Initialise the resource alarm calendars, and ensure that their file names are
* different. The resources calendar contains the active alarms, archived alarms
* and alarm templates;
*/
void ResourcesCalendar::initialise()
{
    KACalendar::setProductId(KALARM_NAME, KALARM_VERSION);
    KCalendarCore::CalFormat::setApplication(QStringLiteral(KALARM_NAME), QString::fromLatin1(KACalendar::icalProductId()));
    mInstance = new ResourcesCalendar();
}

/******************************************************************************
* Terminate access to the resource calendars.
*/
void ResourcesCalendar::terminate()
{
    delete mInstance;
    mInstance = nullptr;
}

/******************************************************************************
* Constructor for the resources calendar.
*/
ResourcesCalendar::ResourcesCalendar()
{
    Resources* resources = Resources::instance();
    connect(resources, &Resources::resourceAdded, this, &ResourcesCalendar::slotResourceAdded);
    connect(resources, &Resources::eventsAdded, this, &ResourcesCalendar::slotEventsAdded);
    connect(resources, &Resources::eventsToBeRemoved, this, &ResourcesCalendar::slotEventsToBeRemoved);
    connect(resources, &Resources::eventUpdated, this, &ResourcesCalendar::slotEventUpdated);
    connect(resources, &Resources::resourcesPopulated, this, &ResourcesCalendar::slotResourcesPopulated);
    connect(resources, &Resources::settingsChanged, this, &ResourcesCalendar::slotResourceSettingsChanged);

    // Fetch events from all resources which already exist.
    QVector<Resource> allResources = Resources::enabledResources();
    for (Resource& resource : allResources)
        slotResourceAdded(resource);
}

ResourcesCalendar::~ResourcesCalendar()
{
    // Resource map should be empty, but just in case...
    while (!mResourceMap.isEmpty())
        removeKAEvents(mResourceMap.begin().key(), true, CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE | CalEvent::DISPLAYING);
}

/******************************************************************************
* Delete a calendar and all its KAEvent instances of specified alarm types from
* the lists.
* Called after the calendar is deleted or alarm types have been disabled, or
* the AlarmCalendar is closed.
*/
bool ResourcesCalendar::removeKAEvents(ResourceId key, bool closing, CalEvent::Types types)
{
    bool removed = false;
    ResourceMap::Iterator rit = mResourceMap.find(key);
    if (rit != mResourceMap.end())
    {
        KAEvent::List retained;
        KAEvent::List& events = rit.value();
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            KAEvent* event = events[i];
            bool remove = (event->resourceId() != key);
            if (remove)
                qCCritical(KALARM_LOG) << "ResourcesCalendar::removeKAEvents: Event" << event->id() << ", resource" << event->resourceId() << "Indexed under resource" << key;
            else
                remove = event->category() & types;
            if (remove)
            {
                mEventMap.remove(EventId(key, event->id()));
                delete event;
                removed = true;
            }
            else
                retained.push_back(event);
        }
        if (retained.empty())
            mResourceMap.erase(rit);
        else
            events.swap(retained);
    }
    if (!removed)
        return false;

    mEarliestAlarm.remove(key);
    // Emit signal only if we're not in the process of closing the calendar
    if (!closing)
    {
        Q_EMIT earliestAlarmChanged();
        if (mHaveDisabledAlarms)
            checkForDisabledAlarms();
    }
    return true;
}

/******************************************************************************
* Called when the enabled or read-only status of a resource has changed.
* If the resource is now disabled, remove its events from the calendar.
*/
void ResourcesCalendar::slotResourceSettingsChanged(Resource& resource, ResourceType::Changes change)
{
    if (change & ResourceType::Enabled)
    {
        if (resource.isValid())
        {
            // For each alarm type which has been disabled, remove the
            // resource's events from the map, but not from the resource.
            const CalEvent::Types enabled = resource.enabledTypes();
            const CalEvent::Types disabled = ~enabled & (CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
            removeKAEvents(resource.id(), false, disabled);

            // For each alarm type which has been enabled, add the resource's
            // events to the map.
            if (enabled != CalEvent::EMPTY)
                slotEventsAdded(resource, resource.events());
        }
    }
}

/******************************************************************************
* Called when all resources have been populated for the first time.
*/
void ResourcesCalendar::slotResourcesPopulated()
{
    // Now that all calendars have been processed, all repeat-at-login alarms
    // will have been triggered. Prevent any new or updated repeat-at-login
    // alarms (e.g. when they are edited by the user) triggering from now on.
    mIgnoreAtLogin = true;
}

/******************************************************************************
* Called when a resource has been added.
* Add its KAEvent instances to those held by AlarmCalendar.
* All events must have their resource ID set.
*/
void ResourcesCalendar::slotResourceAdded(Resource& resource)
{
    slotEventsAdded(resource, resource.events());
}

/******************************************************************************
* Called when events have been added to a resource.
* Add corresponding KAEvent instances to those held by AlarmCalendar.
* All events must have their resource ID set.
*/
void ResourcesCalendar::slotEventsAdded(Resource& resource, const QList<KAEvent>& events)
{
    for (const KAEvent& event : events)
        slotEventUpdated(resource, event);
}

/******************************************************************************
* Called when an event has been changed in a resource.
* Change the corresponding KAEvent instance held by AlarmCalendar.
* The event must have its resource ID set.
*/
void ResourcesCalendar::slotEventUpdated(Resource& resource, const KAEvent& event)
{
    bool added = true;
    bool updated = false;
    KAEventMap::Iterator it = mEventMap.find(EventId(event));
    if (it != mEventMap.end())
    {
        // The event ID already exists - remove the existing event first
        KAEvent* storedEvent = it.value();
        if (event.category() == storedEvent->category())
        {
            // The existing event is the same type - update it in place
            *storedEvent = event;
            addNewEvent(resource, storedEvent, true);
            updated = true;
        }
        else
            delete storedEvent;
        added = false;
    }
    if (!updated)
        addNewEvent(resource, new KAEvent(event));

    if (event.category() == CalEvent::ACTIVE)
    {
        bool enabled = event.enabled();
        checkForDisabledAlarms(!enabled, enabled);
        if (!mIgnoreAtLogin  &&  added  &&  enabled  &&  event.repeatAtLogin())
            Q_EMIT atLoginEventAdded(event);
    }
}

/******************************************************************************
* Called when events are about to be removed from a resource.
* Remove the corresponding KAEvent instances held by AlarmCalendar.
*/
void ResourcesCalendar::slotEventsToBeRemoved(Resource& resource, const QList<KAEvent>& events)
{
    for (const KAEvent& event : events)
    {
        if (mEventMap.contains(EventId(event)))
            deleteEventInternal(event, resource, false);
    }
}

/******************************************************************************
* This method must only be called from the main KAlarm queue processing loop,
* to prevent asynchronous calendar operations interfering with one another.
*
* Purge a list of archived events from the calendar.
*/
void ResourcesCalendar::purgeEvents(const KAEvent::List& events)
{
    for (const KAEvent* event : events)
    {
        Resource resource = Resources::resource(event->resourceId());
        if (resource.isValid())
            deleteEventInternal(event->id(), *event, resource, true);
    }
    if (mHaveDisabledAlarms)
        checkForDisabledAlarms();
}

/******************************************************************************
* Add the specified event to the calendar.
* If it is an active event and 'useEventID' is false, a new event ID is
* created. In all other cases, the event ID is taken from 'evnt' (if non-null).
* 'evnt' is updated with the actual event ID.
* The event is added to 'resource' if specified; otherwise the default resource
* is used or the user is prompted, depending on policy, and 'resource' is
* updated with the actual resource used. If 'noPrompt' is true, the user will
* not be prompted so that if no default resource is defined, the function will
* fail.
* Reply = true if 'evnt' was written to the calendar. 'evnt' is updated.
*       = false if an error occurred, in which case 'evnt' is unchanged.
*/
bool ResourcesCalendar::addEvent(KAEvent& evnt, Resource& resource, QWidget* promptParent, bool useEventID, bool noPrompt, bool* cancelled)
{
    if (cancelled)
        *cancelled = false;
    qCDebug(KALARM_LOG) << "ResourcesCalendar::addEvent:" << evnt.id() << ", resource" << resource.displayId();

    // Check that the event type is valid for the calendar
    const CalEvent::Type type = evnt.category();
    switch (type)
    {
        case CalEvent::ACTIVE:
        case CalEvent::ARCHIVED:
        case CalEvent::TEMPLATE:
            break;
        default:
            return false;
    }

    const ResourceId key = resource.id();
    KAEvent* event = new KAEvent(evnt);
    QString id = event->id();
    if (type == CalEvent::ACTIVE)
    {
        if (id.isEmpty())
            useEventID = false;
        else if (!useEventID)
            id.clear();
    }
    else
        useEventID = true;
    if (id.isEmpty())
        id = KCalendarCore::CalFormat::createUniqueId();
    if (useEventID)
        id = CalEvent::uid(id, type);   // include the alarm type tag in the ID
    event->setEventId(id);

    bool ok = false;
    bool remove = false;
    if (!resource.isEnabled(type))
    {
        resource = Resources::destination(type, promptParent, noPrompt, cancelled);
        if (!resource.isValid())
            qCWarning(KALARM_LOG) << "ResourcesCalendar::addEvent: Error! Cannot create" << type << "(No default calendar is defined)";
    }
    if (resource.isValid())
    {
        // Don't add event to mEventMap yet - its ID is not yet known.
        // It will be added after it is inserted into the data model, when
        // the resource signals eventsAdded().
        ok = resource.addEvent(*event);
        remove = ok;   // if success, delete the local event instance on exit
        if (ok  &&  type == CalEvent::ACTIVE  &&  !event->enabled())
            checkForDisabledAlarms(true, false);
        event->setResourceId(resource.id());
    }
    if (!ok)
    {
        if (remove)
        {
            // Adding to mCalendar failed, so undo AlarmCalendar::addEvent()
            mEventMap.remove(EventId(key, event->id()));
            KAEvent::List& events = mResourceMap[key];
            int i = events.indexOf(event);
            if (i >= 0)
                events.remove(i);
            if (mEarliestAlarm[key] == event)
                findEarliestAlarm(key);
        }
        delete event;
        return false;
    }
    evnt = *event;
    if (remove)
        delete event;
    return true;
}

/******************************************************************************
* Internal method to add an already checked event to the calendar.
* mEventMap takes ownership of the KAEvent.
* If 'replace' is true, an existing event is being updated (NOTE: its category()
* must remain the same).
*/
void ResourcesCalendar::addNewEvent(const Resource& resource, KAEvent* event, bool replace)
{
    const ResourceId key = resource.id();
    event->setResourceId(key);
    if (!replace)
    {
        mResourceMap[key] += event;
        mEventMap[EventId(key, event->id())] = event;
    }

    if ((resource.alarmTypes() & CalEvent::ACTIVE)
    &&  event->category() == CalEvent::ACTIVE)
    {
        // Update the earliest alarm to trigger
        const ResourceId key = resource.id();
        const KAEvent* earliest = mEarliestAlarm.value(key, (KAEvent*)nullptr);
        if (replace  &&  earliest == event)
            findEarliestAlarm(key);
        else
        {
            const KADateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
            if (dt.isValid()
            &&  (!earliest  ||  dt < earliest->nextTrigger(KAEvent::ALL_TRIGGER)))
            {
                mEarliestAlarm[key] = event;
                Q_EMIT earliestAlarmChanged();
            }
        }
    }
}

/******************************************************************************
* Modify the specified event in the calendar with its new contents.
* The new event must have a different event ID from the old one.
* It is assumed to be of the same event type as the old one (active, etc.)
* Reply = true if 'newEvent' was written to the calendar. 'newEvent' is updated.
*       = false if an error occurred, in which case 'newEvent' is unchanged.
*/
bool ResourcesCalendar::modifyEvent(const EventId& oldEventId, KAEvent& newEvent)
{
    const EventId newId(oldEventId.resourceId(), newEvent.id());
    qCDebug(KALARM_LOG) << "ResourcesCalendar::modifyEvent:" << oldEventId << "->" << newId;
    bool noNewId = newId.isEmpty();
    if (!noNewId  &&  oldEventId == newId)
    {
        qCCritical(KALARM_LOG) << "ResourcesCalendar::modifyEvent: Same IDs";
        return false;
    }

    // Set the event's ID, and update the old event in the resources calendar.
    const KAEvent* storedEvent = event(oldEventId);
    if (!storedEvent)
    {
        qCCritical(KALARM_LOG) << "ResourcesCalendar::modifyEvent: Old event not found";
        return false;
    }
    if (noNewId)
        newEvent.setEventId(KCalendarCore::CalFormat::createUniqueId());
    Resource resource = Resources::resource(oldEventId.resourceId());
    if (!resource.isValid())
        return false;
    // Don't add new event to mEventMap yet - it will be added when the resource
    // signals eventsAdded().
    if (!resource.addEvent(newEvent))
        return false;
    // Note: deleteEventInternal() will delete storedEvent before using the
    // event parameter, so need to pass a copy as the parameter.
    deleteEventInternal(KAEvent(*storedEvent), resource);
    if (mHaveDisabledAlarms)
        checkForDisabledAlarms();
    return true;
}

/******************************************************************************
* Update the specified event in the calendar with its new contents.
* The event retains the same ID. The event must be in the resource calendar,
* and must have its resourceId() set correctly.
* Reply = event which has been updated
*       = 0 if error.
*/
KAEvent* ResourcesCalendar::updateEvent(const KAEvent& evnt)
{
    KAEvent* kaevnt = event(EventId(evnt));
    if (kaevnt)
    {
        Resource resource = Resources::resource(evnt.resourceId());
        if (resource.updateEvent(evnt))
        {
            *kaevnt = evnt;
            return kaevnt;
        }
    }
    qCDebug(KALARM_LOG) << "ResourcesCalendar::updateEvent: error";
    return nullptr;
}


/******************************************************************************
* Delete the specified event from the resource calendar, if it exists.
* The calendar is then optionally saved.
*/
bool ResourcesCalendar::deleteEvent(const KAEvent& event, Resource& resource, bool saveit)
{
    Q_UNUSED(saveit);

    if (!resource.isValid())
    {
        resource = Resources::resource(event.resourceId());
        if (!resource.isValid())
        {
            qCDebug(KALARM_LOG) << "ResourcesCalendar::deleteEvent: Resource not found for" << event.id();
            return false;
        }
    }
    else if (!resource.containsEvent(event.id()))
    {
        qCDebug(KALARM_LOG) << "ResourcesCalendar::deleteEvent: Event" << event.id() << "not in resource" << resource.displayId();
        return false;
    }
    qCDebug(KALARM_LOG) << "ResourcesCalendar::deleteEvent:" << event.id();
    const CalEvent::Type status = deleteEventInternal(event.id(), event, resource, true);
    if (mHaveDisabledAlarms)
        checkForDisabledAlarms();
    return status != CalEvent::EMPTY;
}

/******************************************************************************
* Internal method to delete the specified event from the calendar and lists.
* Reply = event status, if it was found in the resource calendar/calendar
*         resource or local calendar
*       = CalEvent::EMPTY otherwise.
*/
CalEvent::Type ResourcesCalendar::deleteEventInternal(const KAEvent& event, Resource& resource, bool deleteFromResources)
{
    if (!resource.isValid())
        return CalEvent::EMPTY;
    if (event.resourceId() != resource.id())
    {
        qCCritical(KALARM_LOG) << "ResourcesCalendar::deleteEventInternal: Event" << event.id() << ": resource" << event.resourceId() << "differs from 'resource'" << resource.id();
        return CalEvent::EMPTY;
    }
    return deleteEventInternal(event.id(), event, resource, deleteFromResources);
}

CalEvent::Type ResourcesCalendar::deleteEventInternal(const QString& eventID, const KAEvent& event, Resource& resource, bool deleteFromResources)
{
    // Make a copy of the KAEvent and the ID QString, since the supplied
    // references might be destructed when the event is deleted below.
    const QString id = eventID;
    const KAEvent paramEvent = event;

    const ResourceId key = resource.id();
    KAEventMap::Iterator it = mEventMap.find(EventId(key, id));
    if (it != mEventMap.end())
    {
        KAEvent* ev = it.value();
        mEventMap.erase(it);
        KAEvent::List& events = mResourceMap[key];
        int i = events.indexOf(ev);
        if (i >= 0)
            events.remove(i);
        delete ev;
        if (mEarliestAlarm[key] == ev)
            findEarliestAlarm(resource);
    }
    else
    {
        for (EarliestMap::Iterator eit = mEarliestAlarm.begin();  eit != mEarliestAlarm.end();  ++eit)
        {
            KAEvent* ev = eit.value();
            if (ev  &&  ev->id() == id)
            {
                findEarliestAlarm(eit.key());
                break;
            }
        }
    }

    CalEvent::Type status = CalEvent::EMPTY;
    if (deleteFromResources)
    {
        // Delete from the resource.
        CalEvent::Type s = paramEvent.category();
        if (resource.deleteEvent(paramEvent))
            status = s;
    }
    return status;
}

/******************************************************************************
* Return the event with the specified ID.
* If 'findUniqueId' is true, and the resource ID is invalid, if there is a
* unique event with the given ID, it will be returned.
*/
KAEvent* ResourcesCalendar::event(const EventId& uniqueID, bool findUniqueId)
{
    if (uniqueID.resourceId() == -1  &&  findUniqueId)
    {
        // The resource isn't known, but use the event ID if it is unique among
        // all resources.
        const QString eventId = uniqueID.eventId();
        const KAEvent::List list = events(eventId);
        if (list.count() > 1)
        {
            qCWarning(KALARM_LOG) << "ResourcesCalendar::event: Multiple events found with ID" << eventId;
            return nullptr;
        }
        if (list.isEmpty())
            return nullptr;
        return list[0];
    }
    KAEventMap::ConstIterator it = mEventMap.constFind(uniqueID);
    if (it == mEventMap.constEnd())
        return nullptr;
    return it.value();
}

/******************************************************************************
* Find the alarm template with the specified name.
* Reply = 0 if not found.
*/
KAEvent* ResourcesCalendar::templateEvent(const QString& templateName)
{
    if (templateName.isEmpty())
        return nullptr;
    const KAEvent::List eventlist = events(CalEvent::TEMPLATE);
    for (KAEvent* event : eventlist)
    {
        if (event->templateName() == templateName)
            return event;
    }
    return nullptr;
}

/******************************************************************************
* Return all events with the specified ID, from all calendars.
*/
KAEvent::List ResourcesCalendar::events(const QString& uniqueId) const
{
    KAEvent::List list;
    for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
    {
        const ResourceId id = rit.key();
        KAEventMap::ConstIterator it = mEventMap.constFind(EventId(id, uniqueId));
        if (it != mEventMap.constEnd())
            list += it.value();
    }
    return list;
}

KAEvent::List ResourcesCalendar::events(const Resource& resource, CalEvent::Types type) const
{
    return events(type, resource);
}

KAEvent::List ResourcesCalendar::events(CalEvent::Types type) const
{
    Resource resource;
    return events(type, resource);
}

KAEvent::List ResourcesCalendar::events(CalEvent::Types type, const Resource& resource) const
{
    KAEvent::List list;
    if (resource.isValid())
    {
        const ResourceId key = resource.id();
        ResourceMap::ConstIterator rit = mResourceMap.constFind(key);
        if (rit == mResourceMap.constEnd())
            return list;
        const KAEvent::List events = rit.value();
        if (type == CalEvent::EMPTY)
            return events;
        for (KAEvent* const event : events)
            if (type & event->category())
                list += event;
    }
    else
    {
        for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
        {
            const KAEvent::List events = rit.value();
            if (type == CalEvent::EMPTY)
                list += events;
            else
            {
                for (KAEvent* const event : events)
                    if (type & event->category())
                        list += event;
            }
        }
    }
    return list;
}

/******************************************************************************
* Return whether an event is read-only.
* Display calendar events are always returned as read-only.
*/
bool ResourcesCalendar::eventReadOnly(const QString& eventId) const
{
    KAEvent event;
    const Resource resource = Resources::resourceForEvent(eventId, event);
    return !event.isValid()  ||  event.isReadOnly()
       ||  !resource.isWritable(event.category());
}

/******************************************************************************
* Called when an alarm's enabled status has changed.
*/
void ResourcesCalendar::disabledChanged(const KAEvent* event)
{
    if (event->category() == CalEvent::ACTIVE)
    {
        bool status = event->enabled();
        checkForDisabledAlarms(!status, status);
    }
}

/******************************************************************************
* Check whether there are any individual disabled alarms, following an alarm
* creation or modification. Must only be called for an ACTIVE alarm.
*/
void ResourcesCalendar::checkForDisabledAlarms(bool oldEnabled, bool newEnabled)
{
    if (newEnabled != oldEnabled)
    {
        if (newEnabled  &&  mHaveDisabledAlarms)
            checkForDisabledAlarms();
        else if (!newEnabled  &&  !mHaveDisabledAlarms)
        {
            mHaveDisabledAlarms = true;
            Q_EMIT haveDisabledAlarmsChanged(true);
        }
    }
}

/******************************************************************************
* Check whether there are any individual disabled alarms.
*/
void ResourcesCalendar::checkForDisabledAlarms()
{
    bool disabled = false;
    const KAEvent::List eventlist = events(CalEvent::ACTIVE);
    for (const KAEvent* const event : eventlist)
    {
        if (!event->enabled())
        {
            disabled = true;
            break;
        }
    }
    if (disabled != mHaveDisabledAlarms)
    {
        mHaveDisabledAlarms = disabled;
        Q_EMIT haveDisabledAlarmsChanged(disabled);
    }
}

/******************************************************************************
* Find and note the active alarm with the earliest trigger time for a calendar.
*/
void ResourcesCalendar::findEarliestAlarm(const Resource& resource)
{
    if (!(resource.alarmTypes() & CalEvent::ACTIVE))
        return;
    findEarliestAlarm(resource.id());
}

void ResourcesCalendar::findEarliestAlarm(ResourceId key)
{
    EarliestMap::Iterator eit = mEarliestAlarm.find(key);
    if (eit != mEarliestAlarm.end())
        eit.value() = nullptr;
    if (key < 0)
        return;
    ResourceMap::ConstIterator rit = mResourceMap.constFind(key);
    if (rit == mResourceMap.constEnd())
        return;
    const KAEvent::List& events = rit.value();
    KAEvent* earliest = nullptr;
    KADateTime earliestTime;
    for (KAEvent* event : events)
    {
        if (event->category() != CalEvent::ACTIVE
        ||  mPendingAlarms.contains(event->id()))
            continue;
        const KADateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
        if (dt.isValid()  &&  (!earliest || dt < earliestTime))
        {
            earliestTime = dt;
            earliest = event;
        }
    }
    mEarliestAlarm[key] = earliest;
    Q_EMIT earliestAlarmChanged();
}

/******************************************************************************
* Return the active alarm with the earliest trigger time.
* Reply = 0 if none.
*/
KAEvent* ResourcesCalendar::earliestAlarm() const
{
    KAEvent* earliest = nullptr;
    KADateTime earliestTime;
    for (EarliestMap::ConstIterator eit = mEarliestAlarm.constBegin();  eit != mEarliestAlarm.constEnd();  ++eit)
    {
        KAEvent* event = eit.value();
        if (!event)
            continue;
        const KADateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
        if (dt.isValid()  &&  (!earliest || dt < earliestTime))
        {
            earliestTime = dt;
            earliest = event;
        }
    }
    return earliest;
}

/******************************************************************************
* Note that an alarm which has triggered is now being processed. While pending,
* it will be ignored for the purposes of finding the earliest trigger time.
*/
void ResourcesCalendar::setAlarmPending(KAEvent* event, bool pending)
{
    const QString id = event->id();
    bool wasPending = mPendingAlarms.contains(id);
    qCDebug(KALARM_LOG) << "ResourcesCalendar::setAlarmPending:" << id << "," << pending << "(was" << wasPending << ")";
    if (pending)
    {
        if (wasPending)
            return;
        mPendingAlarms += id;
    }
    else
    {
        if (!wasPending)
            return;
        mPendingAlarms.remove(id);
    }
    // Now update the earliest alarm to trigger for its calendar
    findEarliestAlarm(Resources::resourceForEvent(event->id()));
}

/******************************************************************************
* Called when the user changes the start-of-day time.
* Adjust the start times of all date-only alarms' recurrences.
*/
void ResourcesCalendar::adjustStartOfDay()
{
    for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
        KAEvent::adjustStartOfDay(rit.value());
}

// vim: et sw=4:
