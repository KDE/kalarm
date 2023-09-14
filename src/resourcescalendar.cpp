/*
 *  resourcescalendar.cpp  -  KAlarm calendar resources access
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "resourcescalendar.h"

#include "eventid.h"
#include "kalarmapp.h"
#include "resources/resources.h"
#include "kalarm_debug.h"

#include <KCalendarCore/CalFormat>

using namespace KAlarmCal;


ResourcesCalendar*             ResourcesCalendar::mInstance {nullptr};
ResourcesCalendar::ResourceMap ResourcesCalendar::mResourceMap;
ResourcesCalendar::EarliestMap ResourcesCalendar::mEarliestAlarm;
ResourcesCalendar::EarliestMap ResourcesCalendar::mEarliestNonDispAlarm;
QSet<QString>                  ResourcesCalendar::mPendingAlarms;
bool                           ResourcesCalendar::mIgnoreAtLogin {false};
bool                           ResourcesCalendar::mHaveDisabledAlarms {false};
QHash<ResourceId, QHash<QString, KernelWakeAlarm>> ResourcesCalendar::mWakeSuspendTimers;


/******************************************************************************
* Initialise the resource alarm calendars, and ensure that their file names are
* different. The resources calendar contains the active alarms, archived alarms
* and alarm templates;
*/
void ResourcesCalendar::initialise(const QByteArray& appName, const QByteArray& appVersion)
{
    if (!mInstance)
    {
        KACalendar::setProductId(appName, appVersion);
        mInstance = new ResourcesCalendar();
    }
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
    connect(theApp(), &KAlarmApp::alarmEnabledToggled, this, &ResourcesCalendar::slotAlarmsEnabledToggled);
    Preferences::connect(&Preferences::wakeFromSuspendAdvanceChanged, this, &ResourcesCalendar::slotWakeFromSuspendAdvanceChanged);

    // Fetch events from all resources which already exist.
    QVector<Resource> allResources = Resources::enabledResources();
    for (Resource& resource : allResources)
        slotResourceAdded(resource);
}

ResourcesCalendar::~ResourcesCalendar()
{
    // Resource map should be empty, but just in case...
    while (!mResourceMap.isEmpty())
        removeKAEvents(mResourceMap.constBegin().key(), true, CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE | CalEvent::DISPLAYING);
}

/******************************************************************************
* Delete a calendar and all its KAEvent instances of specified alarm types from
* the lists.
* Called after the calendar is deleted or alarm types have been disabled, or
* the ResourcesCalendar is closed.
*/
void ResourcesCalendar::removeKAEvents(ResourceId key, bool closing, CalEvent::Types types)
{
    bool removed = false;
    ResourceMap::Iterator rit = mResourceMap.find(key);
    if (rit != mResourceMap.end())
    {
        Resource resource = Resources::resource(key);
        QSet<QString> retained;
        QSet<QString>& eventIds = rit.value();
        for (auto it = eventIds.constBegin();  it != eventIds.constEnd();  ++it)
        {
            const KAEvent event = resource.event(*it, true);
            bool remove = (event.resourceId() != key);
            if (remove)
                qCCritical(KALARM_LOG) << "ResourcesCalendar::removeKAEvents: Event" << event.id() << ", resource" << event.resourceId() << "Indexed under resource" << key;
            else
                remove = event.category() & types;
            if (remove)
                removed = true;
            else
                retained.insert(*it);
        }
        if (retained.isEmpty())
            mResourceMap.erase(rit);
        else
            eventIds.swap(retained);
    }
    if (removed)
    {
        mEarliestAlarm.remove(key);
        mEarliestNonDispAlarm.remove(key);
        // Emit signal only if we're not in the process of closing the calendar
        if (!closing)
        {
            Q_EMIT earliestAlarmChanged();
            if (mHaveDisabledAlarms)
                checkForDisabledAlarms();
        }
    }
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
* Add its KAEvent instances to those held by ResourcesCalendar.
* All events must have their resource ID set.
*/
void ResourcesCalendar::slotResourceAdded(Resource& resource)
{
    slotEventsAdded(resource, resource.events());
}

/******************************************************************************
* Called when events have been added to a resource.
* Record that the event is now usable by the ResourcesCalendar.
* Update the earliest alarm for the resource.
*/
void ResourcesCalendar::slotEventsAdded(Resource& resource, const QList<KAEvent>& events)
{
    for (const KAEvent& event : events)
        slotEventUpdated(resource, event);
}

/******************************************************************************
* Called when an event has been changed in a resource.
* Record that the event is now usable by the ResourcesCalendar.
* Update the earliest alarm for the resource.
*/
void ResourcesCalendar::slotEventUpdated(Resource& resource, const KAEvent& event)
{
    const ResourceId key = resource.id();
    const bool added = !mResourceMap[key].contains(event.id());
    qCDebug(KALARM_LOG) << "ResourcesCalendar::slotEventUpdated: resource" << resource.displayId() << (added ? "added" : "updated") << event.id();
    mResourceMap[key].insert(event.id());

    if ((resource.alarmTypes() & CalEvent::ACTIVE)
    &&  event.category() == CalEvent::ACTIVE)
    {
        // Set/clear wake from suspend timer if needed
        checkKernelWakeSuspend(key, event);

        // Update the earliest alarm to trigger
        const QString earliestId        = mEarliestAlarm.value(key);
        const QString earliestNonDispId = mEarliestNonDispAlarm.value(key);
        if (earliestId == event.id()  ||  earliestNonDispId == event.id())
            findEarliestAlarm(resource);
        else
        {
            const KADateTime dt = event.nextTrigger(KAEvent::Trigger::All).effectiveKDateTime();
            if (dt.isValid())
            {
                bool changed = false;
                DateTime next;
                if (!earliestId.isEmpty())
                    next = resource.event(earliestId).nextTrigger(KAEvent::Trigger::All);
                if (earliestId.isEmpty()  ||  dt < next)
                {
                    mEarliestAlarm[key] = event.id();
                    changed = true;
                }
                if (!(event.actionTypes() & KAEvent::Action::Display))
                {
                    // It is not a display event.
                    DateTime nextNonDisp;
                    if (!earliestNonDispId.isEmpty())
                        nextNonDisp = (earliestId == earliestNonDispId) ? next : resource.event(earliestNonDispId).nextTrigger(KAEvent::Trigger::All);
                    if (earliestNonDispId.isEmpty()  ||  dt < nextNonDisp)
                    {
                        mEarliestNonDispAlarm[key] = event.id();
                        changed = true;
                    }
                }
                if (changed)
                    Q_EMIT earliestAlarmChanged();
            }
        }
    }

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
* Remove the corresponding KAEvent instances held by ResourcesCalendar.
*/
void ResourcesCalendar::slotEventsToBeRemoved(Resource& resource, const QList<KAEvent>& events)
{
    const ResourceId key = resource.id();
    for (const KAEvent& event : events)
    {
        if (mResourceMap.value(key).contains(event.id()))
            deleteEventInternal(event, resource, false);
    }
}

/******************************************************************************
* Called when alarm monitoring has been enabled or disabled (for all alarms).
* Arm or disarm kernel wake alarms for all events which use them.
*/
void ResourcesCalendar::slotAlarmsEnabledToggled(bool enabled)
{
    if (!KernelWakeAlarm::isAvailable())
        return;

    if (enabled)
    {
        // Set kernel wake timers for all events which require them.
        setKernelWakeSuspend();
    }
    else
    {
        // Disarm all kernel wake timers (but don't delete them).
        for (auto itr = mWakeSuspendTimers.begin(), endr = mWakeSuspendTimers.end();  itr != endr;  ++itr)
        {
            auto& resourceHash = itr.value();
            for (auto it = resourceHash.begin(), end = resourceHash.end();  it != end;  ++it)
                it.value().disarm();
        }
    }
}

/******************************************************************************
* Called when the wake-from-suspend wakeup advance interval has changed.
* Revise all kernel wake alarm times.
* Note that the user only has the option to change the wakeup advance if kernel
* wake alarms are used, and not if the RTC wake timer is used.
*/
void ResourcesCalendar::slotWakeFromSuspendAdvanceChanged(unsigned advance)
{
    if (!KernelWakeAlarm::isAvailable()  ||  !theApp()->alarmsEnabled())
        return;

    qCDebug(KALARM_LOG) << "ResourcesCalendar::slotWakeFromSuspendAdvanceChanged:" << advance;
    setKernelWakeSuspend();
}

/******************************************************************************
* This method must only be called from the main KAlarm queue processing loop,
* to prevent asynchronous calendar operations interfering with one another.
*
* Purge a list of archived events from the calendar.
*/
void ResourcesCalendar::purgeEvents(const QVector<KAEvent>& events)
{
    for (const KAEvent& event : events)
    {
        Resource resource = Resources::resource(event.resourceId());
        if (resource.isValid())
            deleteEventInternal(event.id(), event, resource, true);
    }
    if (mHaveDisabledAlarms)
        mInstance->checkForDisabledAlarms();
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
bool ResourcesCalendar::addEvent(KAEvent& evnt, Resource& resource, QWidget* promptParent, AddEventOptions options, bool* cancelled)
{
    bool useEventID = options & UseEventId;
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

    KAEvent event = evnt;
    QString id = event.id();
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
    event.setEventId(id);

    bool ok = false;
    if (!resource.isEnabled(type))
    {
        Resources::DestOptions destOptions {};
        if (options & NoResourcePrompt)
            destOptions |= Resources::NoResourcePrompt;
        resource = Resources::destination(type, promptParent, destOptions, cancelled);
        if (!resource.isValid())
            qCWarning(KALARM_LOG) << "ResourcesCalendar::addEvent: Error! Cannot create" << type << "(No default calendar is defined)";
    }
    if (resource.isValid())
    {
        // Don't add event to mResourceMap yet - its ID is not yet known.
        // It will be added after it is inserted into the data model, when
        // the resource signals eventsAdded().
        ok = resource.addEvent(event);
        if (ok  &&  type == CalEvent::ACTIVE  &&  !event.enabled())
            mInstance->checkForDisabledAlarms(true, false);
        event.setResourceId(resource.id());
    }
    if (ok)
        evnt = event;
    return ok;
}

/******************************************************************************
* Modify the specified event in the calendar with its new contents.
* The new event must have a different event ID from the old one; if it does not
* have an ID, it will be updated with a new ID.
* It is assumed to be of the same event type as the old one (active, etc.)
* Reply = true if 'newEvent' was written to the calendar. 'newEvent' is updated.
*       = false if an error occurred, in which case 'newEvent' is unchanged.
*/
bool ResourcesCalendar::modifyEvent(const EventId& oldEventId, KAEvent& newEvent)
{
    const EventId newId(oldEventId.resourceId(), newEvent.id());
    bool noNewId = newId.isEmpty();
    if (!noNewId  &&  oldEventId == newId)
    {
        qCCritical(KALARM_LOG) << "ResourcesCalendar::modifyEvent: Same IDs" << oldEventId;
        return false;
    }

    // Set the event's ID, and update the old event in the resources calendar.
    if (!mResourceMap.value(oldEventId.resourceId()).contains(oldEventId.eventId()))
    {
        qCCritical(KALARM_LOG) << "ResourcesCalendar::modifyEvent: Old event not found" << oldEventId;
        return false;
    }
    Resource resource = Resources::resource(oldEventId.resourceId());
    if (!resource.isValid())
    {
        qCCritical(KALARM_LOG) << "ResourcesCalendar::modifyEvent: Old event's resource not found" << oldEventId;
        return false;
    }
    const KAEvent oldEvent = resource.event(oldEventId.eventId());
    if (noNewId)
        newEvent.setEventId(KCalendarCore::CalFormat::createUniqueId());
    qCDebug(KALARM_LOG) << "ResourcesCalendar::modifyEvent:" << oldEventId << "->" << newEvent.id();

    // Don't add new event to mResourceMap yet - it will be added when the resource
    // signals eventsAdded().
    if (!resource.addEvent(newEvent))
        return false;
    deleteEventInternal(oldEvent, resource);
    if (mHaveDisabledAlarms)
        mInstance->checkForDisabledAlarms();
    return true;
}

/******************************************************************************
* Update the specified event in the calendar with its new contents.
* The event retains the same ID. The event must be in the resource calendar,
* and must have its resourceId() set correctly.
* Reply = event which has been updated
*       = invalid if error.
*/
KAEvent ResourcesCalendar::updateEvent(const KAEvent& evnt, bool saveIfReadOnly)
{
    if (mResourceMap.value(evnt.resourceId()).contains(evnt.id()))
    {
        Resource resource = Resources::resource(evnt.resourceId());
        if (resource.updateEvent(evnt, saveIfReadOnly))
        {
            // Set/clear wake from suspend timer if needed
            checkKernelWakeSuspend(resource.id(), evnt);
            return evnt;
        }
    }
    qCDebug(KALARM_LOG) << "ResourcesCalendar::updateEvent: error" << evnt.id();
    return {};
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
        mInstance->checkForDisabledAlarms();
    return status != CalEvent::EMPTY;
}

/******************************************************************************
* Internal method to delete the specified event from the calendar and lists.
* Reply = event status, if it was found in the resource calendar/calendar
*         resource or local calendar
*       = CalEvent::EMPTY otherwise.
*/
CalEvent::Type ResourcesCalendar::deleteEventInternal(const KAEvent& event, Resource& resource, bool deleteFromResource)
{
    if (!resource.isValid())
        return CalEvent::EMPTY;
    if (event.resourceId() != resource.id())
    {
        qCCritical(KALARM_LOG) << "ResourcesCalendar::deleteEventInternal: Event" << event.id() << ": resource" << event.resourceId() << "differs from 'resource'" << resource.id();
        return CalEvent::EMPTY;
    }
    return deleteEventInternal(event.id(), event, resource, deleteFromResource);
}

CalEvent::Type ResourcesCalendar::deleteEventInternal(const QString& eventID, const KAEvent& event, Resource& resource, bool deleteFromResource)
{
    const ResourceId key = resource.id();

    if (mWakeSuspendTimers.contains(key))
        mWakeSuspendTimers[key].remove(eventID);   // this cancels the timer

    mResourceMap[key].remove(eventID);
    if (mEarliestAlarm.value(key)        == eventID
    ||  mEarliestNonDispAlarm.value(key) == eventID)
        mInstance->findEarliestAlarm(resource);

    CalEvent::Type status = CalEvent::EMPTY;
    if (deleteFromResource)
    {
        // Delete from the resource.
        CalEvent::Type s = event.category();
        if (resource.deleteEvent(event))
            status = s;
    }
    return status;
}

/******************************************************************************
* Return the event with the specified ID.
* If 'findUniqueId' is true, and the resource ID is invalid, if there is a
* unique event with the given ID, it will be returned.
*/
KAEvent ResourcesCalendar::event(const EventId& uniqueID, bool findUniqueId)
{
    const QString eventId = uniqueID.eventId();
    const ResourceId resourceId = uniqueID.resourceId();
    if (resourceId == -1  &&  findUniqueId)
    {
        // The resource isn't known, but use the event ID if it is unique among
        // all resources.
        const QVector<KAEvent> list = events(eventId);
        if (list.count() > 1)
        {
            qCWarning(KALARM_LOG) << "ResourcesCalendar::event: Multiple events found with ID" << eventId;
            return {};
        }
        if (list.isEmpty())
            return {};
        return list[0];
    }

    // The resource is specified.
    if (!mResourceMap.value(resourceId).contains(eventId))
        return {};
    return Resources::resource(resourceId).event(eventId);
}

/******************************************************************************
* Find the alarm template with the specified name.
* Reply = 0 if not found.
*/
KAEvent ResourcesCalendar::templateEvent(const QString& templateName)
{
    if (templateName.isEmpty())
        return {};
    const QVector<KAEvent> eventlist = events(CalEvent::TEMPLATE);
    for (const KAEvent& event : eventlist)
    {
        if (event.name() == templateName)
            return event;
    }
    return {};
}

/******************************************************************************
* Return all events with the specified ID, from all calendars.
*/
QVector<KAEvent> ResourcesCalendar::events(const QString& uniqueId)
{
    QVector<KAEvent> list;
    for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
    {
        if (rit.value().contains(uniqueId))
            list += Resources::resource(rit.key()).event(uniqueId);
    }
    return list;
}

QVector<KAEvent> ResourcesCalendar::events(const Resource& resource, CalEvent::Types type)
{
    return events(type, resource);
}

QVector<KAEvent> ResourcesCalendar::events(CalEvent::Types type)
{
    Resource resource;
    return events(type, resource);
}

QVector<KAEvent> ResourcesCalendar::events(CalEvent::Types type, const Resource& resource)
{
    QVector<KAEvent> list;
    if (resource.isValid())
    {
        const ResourceId key = resource.id();
        ResourceMap::ConstIterator rit = mResourceMap.constFind(key);
        if (rit == mResourceMap.constEnd())
            return list;
        const QVector<KAEvent> events = eventsForResource(resource, rit.value());
        if (type == CalEvent::EMPTY)
            return events;
        for (const KAEvent& event : events)
            if (type & event.category())
                list += event;
    }
    else
    {
        for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
        {
            const Resource res = Resources::resource(rit.key());
            const QVector<KAEvent> events = eventsForResource(res, rit.value());
            if (type == CalEvent::EMPTY)
                list += events;
            else
            {
                for (const KAEvent& event : events)
                    if (type & event.category())
                        list += event;
            }
        }
    }
    return list;
}

/******************************************************************************
* Called when an alarm's enabled status has changed.
*/
void ResourcesCalendar::disabledChanged(const KAEvent& event)
{
    if (event.category() == CalEvent::ACTIVE)
    {
        bool status = event.enabled();
        mInstance->checkForDisabledAlarms(!status, status);
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
    const QVector<KAEvent> eventlist = events(CalEvent::ACTIVE);
    for (const KAEvent& event : eventlist)
    {
        if (!event.enabled())
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
* Set kernel wake alarm timers for all events which require them.
*/
void ResourcesCalendar::setKernelWakeSuspend()
{
    for (auto itr = mWakeSuspendTimers.begin(), endr = mWakeSuspendTimers.end();  itr != endr;  ++itr)
    {
        const ResourceId resourceId = itr.key();
        Resource resource = Resources::resource(resourceId);
        auto& resourceHash = itr.value();
        for (auto it = resourceHash.begin(), end = resourceHash.end();  it != end;  ++it)
        {
            const KAEvent event = resource.event(it.key());
            checkKernelWakeSuspend(resourceId, event);
        }
    }
}

/******************************************************************************
* Set or clear any kernel wake alarm associated with an event.
*/
void ResourcesCalendar::checkKernelWakeSuspend(ResourceId key, const KAEvent& event)
{
    if (KernelWakeAlarm::isAvailable()  &&  event.enabled()  &&  event.wakeFromSuspend())
    {
        //TODO: mainDateTime() doesn't take account of holidays/work days
        const KADateTime dt = event.mainDateTime().kDateTime();
        if (!dt.isDateOnly())   // can't determine a wakeup time for date-only events
        {
            KernelWakeAlarm& kernelAlarm = mWakeSuspendTimers[key][event.id()];
            if (theApp()->alarmsEnabled())
                kernelAlarm.arm(dt.addSecs(static_cast<int>(Preferences::wakeFromSuspendAdvance()) * -60));
            else
                kernelAlarm.disarm();
        }
    }
    else
    {
        if (mWakeSuspendTimers.contains(key))
            mWakeSuspendTimers[key].remove(event.id());   // this cancels the timer
    }
}

/******************************************************************************
* Find and note the active alarm with the earliest trigger time for a calendar,
* and the non-display active alarm with the earliest trigger time.
*/
void ResourcesCalendar::findEarliestAlarm(const Resource& resource)
{
    ResourceId key = resource.id();
    if (key < 0)
        return;
    if (!(resource.alarmTypes() & CalEvent::ACTIVE))
        return;

    // Invalidate any existing earliest alarms for the resource
    EarliestMap::Iterator eit = mEarliestAlarm.find(key);
    if (eit != mEarliestAlarm.end())
        eit.value() = QString();
    eit = mEarliestNonDispAlarm.find(key);
    if (eit != mEarliestNonDispAlarm.end())
        eit.value() = QString();

    ResourceMap::ConstIterator rit = mResourceMap.constFind(key);
    if (rit == mResourceMap.constEnd())
        return;
    const QVector<KAEvent> events = eventsForResource(resource, rit.value());
    KAEvent earliest, earliestNonDisp;
    KADateTime earliestTime, earliestNonDispTime;
    for (const KAEvent& event : events)
    {
        if (event.category() != CalEvent::ACTIVE
        ||  mPendingAlarms.contains(event.id()))
            continue;
        const KADateTime dt = event.nextTrigger(KAEvent::Trigger::All).effectiveKDateTime();
        if (dt.isValid())
        {
            if (!earliest.isValid() || dt < earliestTime)
            {
                earliestTime = dt;
                earliest = event;
            }
            if (!(event.actionTypes() & KAEvent::Action::Display))
            {
                if (!earliestNonDisp.isValid() || dt < earliestNonDispTime)
                {
                    earliestNonDispTime = dt;
                    earliestNonDisp = event;
                }
            }
        }
    }
    mEarliestAlarm[key]        = earliest.id();
    mEarliestNonDispAlarm[key] = earliestNonDisp.id();
    Q_EMIT earliestAlarmChanged();
}

/******************************************************************************
* Return the active alarm with the earliest trigger time.
* Reply = invalid if none.
*/
KAEvent ResourcesCalendar::earliestAlarm(KADateTime& nextTriggerTime, bool excludeDisplayAlarms)
{
    KAEvent earliest;
    KADateTime earliestTime;
    const EarliestMap& earliestAlarms(excludeDisplayAlarms ? mEarliestNonDispAlarm : mEarliestAlarm);
    for (EarliestMap::ConstIterator eit = earliestAlarms.constBegin();  eit != earliestAlarms.constEnd();  ++eit)
    {
        const QString id = eit.value();
        if (id.isEmpty())
            continue;
        Resource res = Resources::resource(eit.key());
        const KAEvent event = res.event(id);
        if (!event.isValid())
        {
            // Something went wrong: mEarliestAlarm wasn't updated when it should have been!!
            qCCritical(KALARM_LOG) << "ResourcesCalendar::earliestAlarm: resource" << eit.key() << "does not contain" << id;
            mInstance->findEarliestAlarm(res);
            return earliestAlarm(nextTriggerTime, excludeDisplayAlarms);
        }
//TODO: use next trigger calculated in findEarliestAlarm() (allowing for it being out of date)?
        const KADateTime dt = event.nextTrigger(KAEvent::Trigger::All).effectiveKDateTime();
        if (dt.isValid()  &&  (!earliest.isValid() || dt < earliestTime))
        {
            earliestTime = dt;
            earliest = event;
        }
    }
    nextTriggerTime = earliestTime;
    return earliest;
}

/******************************************************************************
* Note that an alarm which has triggered is now being processed. While pending,
* it will be ignored for the purposes of finding the earliest trigger time.
*/
void ResourcesCalendar::setAlarmPending(const KAEvent& event, bool pending)
{
    const QString id = event.id();
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
    mInstance->findEarliestAlarm(Resources::resourceForEvent(event.id()));
}

/******************************************************************************
* Get the events for a list of event IDs.
*/
QVector<KAEvent> ResourcesCalendar::eventsForResource(const Resource& resource, const QSet<QString>& eventIds)
{
    QVector<KAEvent> events;
    events.reserve(eventIds.count());
    for (const QString& eventId : eventIds)
        events += resource.event(eventId);
    return events;
}

// vim: et sw=4:
