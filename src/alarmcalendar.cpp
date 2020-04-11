/*
 *  alarmcalendar.cpp  -  KAlarm calendar file access
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

#include "alarmcalendar.h"

#include "kalarm.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "preferences.h"
#include "resources/datamodel.h"
#include "resources/resources.h"
#include "lib/messagebox.h"
#include "kalarm_debug.h"

#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/ICalFormat>

#include <KLocalizedString>
#include <KIO/StatJob>
#include <KIO/StoredTransferJob>
#include <KJobWidgets>
#include <KFileItem>
#include <KSharedConfig>
#include <kio_version.h>

#include <QTemporaryFile>
#include <QStandardPaths>
#include <QDir>

using namespace KCalendarCore;
using namespace KAlarmCal;


static const QString displayCalendarName = QStringLiteral("displaying.ics");
static const ResourceId DISPLAY_RES_ID = -1;   // resource ID used for displaying calendar

ResourcesCalendar* ResourcesCalendar::mInstance = nullptr;
DisplayCalendar*  DisplayCalendar::mInstance = nullptr;


/******************************************************************************
* Initialise backend calendar access.
*/
void AlarmCalendar::initialise()
{
    Preferences::setBackend(Preferences::Akonadi);
    Preferences::self()->save();
    KACalendar::setProductId(KALARM_NAME, KALARM_VERSION);
    CalFormat::setApplication(QStringLiteral(KALARM_NAME), QString::fromLatin1(KACalendar::icalProductId()));
}

/******************************************************************************
* Initialise the resource alarm calendars, and ensure that their file names are
* different. The resources calendar contains the active alarms, archived alarms
* and alarm templates;
*/
void ResourcesCalendar::initialise()
{
    AlarmCalendar::initialise();
    mInstance = new ResourcesCalendar();
}

/******************************************************************************
* Initialise the display alarm calendar.
* It is user-specific, and contains details of alarms which are currently being
* displayed to that user and which have not yet been acknowledged;
*/
void DisplayCalendar::initialise()
{
    QDir dir;
    dir.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    const QString displayCal = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1Char('/') + displayCalendarName;
    mInstance = new DisplayCalendar(displayCal);
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
* Terminate access to the display calendar.
*/
void DisplayCalendar::terminate()
{
    delete mInstance;
    mInstance = nullptr;
}

/******************************************************************************
* Return the display calendar, opening it first if necessary.
*/
DisplayCalendar* DisplayCalendar::instanceOpen()
{
    if (mInstance->open())
        return mInstance;
    qCCritical(KALARM_LOG) << "DisplayCalendar::instanceOpen: Open error";
    return nullptr;
}

/******************************************************************************
* Find and return the event with the specified ID.
* The calendar searched is determined by the calendar identifier in the ID.
*/
KAEvent* ResourcesCalendar::getEvent(const EventId& eventId)
{
    if (eventId.eventId().isEmpty())
        return nullptr;
    return mInstance->event(eventId);
}

/******************************************************************************
* Constructor for the resources calendar.
*/
AlarmCalendar::AlarmCalendar()
{
}

/******************************************************************************
* Constructor for the resources calendar.
*/
ResourcesCalendar::ResourcesCalendar()
    : AlarmCalendar()
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

/******************************************************************************
* Constructor for the display calendar file.
*/
DisplayCalendar::DisplayCalendar(const QString& path)
    : AlarmCalendar()
    , mDisplayCalPath(path)
    , mDisplayICalPath(path)
{
    mDisplayICalPath.replace(QStringLiteral("\\.vcs$"), QStringLiteral(".ics"));
    mCalType = (mDisplayCalPath == mDisplayICalPath) ? LOCAL_ICAL : LOCAL_VCAL;    // is the calendar in ICal or VCal format?
}

ResourcesCalendar::~ResourcesCalendar()
{
    close();
}

DisplayCalendar::~DisplayCalendar()
{
    close();
}

/******************************************************************************
* Open the calendar if not already open, and load it into memory.
*/
bool DisplayCalendar::open()
{
    if (isOpen())
        return true;

    // Open the display calendar.
    qCDebug(KALARM_LOG) << "DisplayCalendar::open:" << mDisplayCalPath;
    if (!mCalendarStorage)
    {
        MemoryCalendar::Ptr calendar(new MemoryCalendar(Preferences::timeSpecAsZone()));
        mCalendarStorage = FileStorage::Ptr(new FileStorage(calendar, mDisplayCalPath));
    }

    // Check for file's existence, assuming that it does exist when uncertain,
    // to avoid overwriting it.
    QFileInfo fi(mDisplayCalPath);
    if (!fi.exists()  ||  !fi.isFile()  ||  load() == 0)
    {
        // The calendar file doesn't yet exist, or it's zero length, so create a new one
        if (saveCal(mDisplayICalPath))
            load();
    }

    if (!mOpen)
    {
        mCalendarStorage->calendar().clear();
        mCalendarStorage.clear();
    }
    return isOpen();
}

/******************************************************************************
* Load the calendar into memory.
* Reply = 1 if success
*       = 0 if zero-length file exists.
*       = -1 if failure to load calendar file
*       = -2 if instance uninitialised.
*/
int DisplayCalendar::load()
{
    // Load the display calendar.
    if (!mCalendarStorage)
        return -2;

    qCDebug(KALARM_LOG) << "DisplayCalendar::load:" << mDisplayCalPath;
    if (!mCalendarStorage->load())
    {
        // Load error. Check if the file is zero length
        QFileInfo fi(mDisplayCalPath);
        if (fi.exists()  &&  !fi.size())
            return 0;     // file is zero length

        qCCritical(KALARM_LOG) << "DisplayCalendar::load: Error loading calendar file '" << mDisplayCalPath <<"'";
        KAMessageBox::error(MainWindow::mainMainWindow(),
                            xi18nc("@info", "<para>Error loading calendar:</para><para><filename>%1</filename></para><para>Please fix or delete the file.</para>", mDisplayCalPath));
        // load() could have partially populated the calendar, so clear it out
        mCalendarStorage->calendar()->close();
        mCalendarStorage->calendar().clear();
        mCalendarStorage.clear();
        mOpen = false;
        return -1;
    }
    QString versionString;
    KACalendar::updateVersion(mCalendarStorage, versionString);   // convert events to current KAlarm format for when calendar is saved
    updateKAEvents();

    mOpen = true;
    return 1;
}

/******************************************************************************
* Reload the calendar resources into memory.
*/
bool ResourcesCalendar::reload()
{
    qCDebug(KALARM_LOG) << "ResourcesCalendar::reload";
    bool ok = true;
    QVector<Resource> resources = Resources::enabledResources();
    for (Resource& resource : resources)
        ok = ok && resource.reload();
    return ok;
}

/******************************************************************************
* Reload the calendar file into memory.
*/
bool DisplayCalendar::reload()
{
    if (!mCalendarStorage)
        return false;

    qCDebug(KALARM_LOG) << "DisplayCalendar::reload:" << mDisplayCalPath;
    close();
    return open();
}

/******************************************************************************
* Save the calendar.
*/
bool ResourcesCalendar::save()
{
    bool ok = true;
    // Get all enabled, writable resources.
    QVector<Resource> resources = Resources::enabledResources(CalEvent::EMPTY, true);
    for (Resource& resource : resources)
        ok = ok && resource.save();
    return ok;
}

/******************************************************************************
* Save the calendar.
*/
bool DisplayCalendar::save()
{
    return saveCal();
}

/******************************************************************************
* Save the calendar from memory to file.
* If a filename is specified, create a new calendar file.
*/
bool DisplayCalendar::saveCal(const QString& newFile)
{
    if (!mCalendarStorage)
        return false;
    if (!mOpen  &&  newFile.isEmpty())
        return false;

    qCDebug(KALARM_LOG) << "DisplayCalendar::saveCal:" << "\"" << newFile;
    QString saveFilename = newFile.isEmpty() ? mDisplayCalPath : newFile;
    if (mCalType == LOCAL_VCAL  &&  newFile.isNull())
        saveFilename = mDisplayICalPath;
    mCalendarStorage->setFileName(saveFilename);
    mCalendarStorage->setSaveFormat(new ICalFormat);
    if (!mCalendarStorage->save())
    {
        qCCritical(KALARM_LOG) << "DisplayCalendar::saveCal: Saving" << saveFilename << "failed.";
        KAMessageBox::error(MainWindow::mainMainWindow(),
                            xi18nc("@info", "Failed to save calendar to <filename>%1</filename>", mDisplayICalPath));
        return false;
    }

    if (mCalType == LOCAL_VCAL)
    {
        // The file was in vCalendar format, but has now been saved in iCalendar format.
        mDisplayCalPath = mDisplayICalPath;
        mCalType = LOCAL_ICAL;
    }
    return true;
}

/******************************************************************************
* Close display calendar file at program exit.
*/
void ResourcesCalendar::close()
{
    // Resource map should be empty, but just in case...
    while (!mResourceMap.isEmpty())
        removeKAEvents(mResourceMap.begin().key(), true, CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE | CalEvent::DISPLAYING);
}

/******************************************************************************
* Close display calendar file at program exit.
*/
void DisplayCalendar::close()
{
    if (mCalendarStorage)
    {
        mCalendarStorage->calendar()->close();
        mCalendarStorage->calendar().clear();
        mCalendarStorage.clear();
    }

    // Flag as closed now to prevent removeKAEvents() doing silly things
    // when it's called again
    mOpen = false;

    // Resource map should be empty, but just in case...
    while (!mResourceMap.isEmpty())
        removeKAEvents(mResourceMap.begin().key(), CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE | CalEvent::DISPLAYING);
}

/******************************************************************************
* Create a KAEvent instance corresponding to each KCalendarCore::Event in the
* display calendar, and store them in the event map in place of the old set.
* Called after the display calendar has completed loading.
*/
void DisplayCalendar::updateKAEvents()
{
    qCDebug(KALARM_LOG) << "DisplayCalendar::updateKAEvents";
    const ResourceId key = DISPLAY_RES_ID;
    KAEvent::List& events = mResourceMap[key];
    for (KAEvent* event : events)
    {
        mEventMap.remove(EventId(key, event->id()));
        delete event;
    }
    events.clear();
    Calendar::Ptr cal = mCalendarStorage->calendar();
    if (!cal)
        return;

    const Event::List kcalevents = cal->rawEvents();
    for (Event::Ptr kcalevent : kcalevents)
    {
        if (kcalevent->alarms().isEmpty())
            continue;    // ignore events without alarms

        KAEvent* event = new KAEvent(kcalevent);
        if (!event->isValid())
        {
            qCWarning(KALARM_LOG) << "DisplayCalendar::updateKAEvents: Ignoring unusable event" << kcalevent->uid();
            delete event;
            continue;    // ignore events without usable alarms
        }
        event->setResourceId(key);
        events += event;
        mEventMap[EventId(key, kcalevent->uid())] = event;
    }

}

/******************************************************************************
* Delete a calendar and all its KAEvent instances of specified alarm types from
* the lists.
* Called after the calendar is deleted or alarm types have been disabled, or
* the AlarmCalendar is closed.
*/
bool AlarmCalendar::removeKAEvents(ResourceId key, CalEvent::Types types)
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
            {
                if (key != DISPLAY_RES_ID)
                    qCCritical(KALARM_LOG) << "AlarmCalendar::removeKAEvents: Event" << event->id() << ", resource" << event->resourceId() << "Indexed under resource" << key;
            }
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
    return removed;
}

bool ResourcesCalendar::removeKAEvents(ResourceId key, bool closing, CalEvent::Types types)
{
    if (!AlarmCalendar::removeKAEvents(key, types))
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
        deleteEventInternal(*event);
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
* is used or the user is prompted, depending on policy. If 'noPrompt' is true,
* the user will not be prompted so that if no default resource is defined, the
* function will fail.
* Reply = true if 'evnt' was written to the calendar. 'evnt' is updated.
*       = false if an error occurred, in which case 'evnt' is unchanged.
*/
bool ResourcesCalendar::addEvent(KAEvent& evnt, QWidget* promptParent, bool useEventID, Resource* resourceptr, bool noPrompt, bool* cancelled)
{
    if (cancelled)
        *cancelled = false;
    Resource nullresource;
    Resource& resource(resourceptr ? *resourceptr : nullresource);
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
        id = CalFormat::createUniqueId();
    if (useEventID)
        id = CalEvent::uid(id, type);   // include the alarm type tag in the ID
    event->setEventId(id);

    bool ok = false;
    bool remove = false;
    Resource res;
    if (resource.isEnabled(type))
        res = resource;
    else
    {
        res = Resources::destination(type, promptParent, noPrompt, cancelled);
        if (!res.isValid())
        {
            const char* typeStr = (type == CalEvent::ACTIVE) ? "Active alarm" : (type == CalEvent::ARCHIVED) ? "Archived alarm" : "alarm Template";
            qCWarning(KALARM_LOG) << "ResourcesCalendar::addEvent: Error! Cannot create" << typeStr << "(No default calendar is defined)";
        }
    }
    if (res.isValid())
    {
        // Don't add event to mEventMap yet - its Akonadi item id is not yet known.
        // It will be added once it is inserted into AkonadiDataModel.
        ok = res.addEvent(*event);
        remove = ok;   // if success, delete the local event instance on exit
        if (ok  &&  type == CalEvent::ACTIVE  &&  !event->enabled())
            checkForDisabledAlarms(true, false);
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
* Add the specified event to the calendar.
* Reply = true if 'evnt' was written to the calendar. 'evnt' is updated.
*       = false if an error occurred, in which case 'evnt' is unchanged.
*/
bool DisplayCalendar::addEvent(KAEvent& evnt)
{
    if (!mOpen)
        return false;
    qCDebug(KALARM_LOG) << "DisplayCalendar::addEvent:" << evnt.id();
    // Check that the event type is valid for the calendar
    const CalEvent::Type type = evnt.category();
    if (type != CalEvent::DISPLAYING)
        return false;

    Event::Ptr kcalEvent(new Event);
    KAEvent* event = new KAEvent(evnt);
    QString id = event->id();
    if (id.isEmpty())
        id = kcalEvent->uid();
    id = CalEvent::uid(id, type);   // include the alarm type tag in the ID
    kcalEvent->setUid(id);
    event->setEventId(id);
    event->updateKCalEvent(kcalEvent, KAEvent::UID_IGNORE);

    bool ok = false;
    bool remove = false;
    const ResourceId key = DISPLAY_RES_ID;
    if (!mEventMap.contains(EventId(key, event->id())))
    {
        addNewEvent(Resource(), event);
        ok = mCalendarStorage->calendar()->addEvent(kcalEvent);
        remove = !ok;
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
void AlarmCalendar::addNewEvent(const Resource& resource, KAEvent* event, bool replace)
{
    const ResourceId key = resource.id();
    event->setResourceId(key);
    if (!replace)
    {
        mResourceMap[key] += event;
        mEventMap[EventId(key, event->id())] = event;
    }
}

/******************************************************************************
* Internal method to add an already checked event to the calendar.
* mEventMap takes ownership of the KAEvent.
* If 'replace' is true, an existing event is being updated (NOTE: its category()
* must remain the same).
*/
void ResourcesCalendar::addNewEvent(const Resource& resource, KAEvent* event, bool replace)
{
    AlarmCalendar::addNewEvent(resource, event, replace);

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
        newEvent.setEventId(CalFormat::createUniqueId());
    Resource resource = Resources::resource(oldEventId.resourceId());
    if (!resource.isValid())
        return false;
    // Don't add new event to mEventMap yet - its Akonadi item id is not yet known
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
* The event retains the same ID. The event must be in the resource calendar.
* Reply = event which has been updated
*       = 0 if error.
*/
KAEvent* ResourcesCalendar::updateEvent(const KAEvent& evnt)
{
    KAEvent* kaevnt = event(EventId(evnt));
    if (kaevnt)
    {
        Resource resource = Resources::resourceForEvent(evnt.id());
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
bool ResourcesCalendar::deleteEvent(const KAEvent& event, bool saveit)
{
    Q_UNUSED(saveit);

    const CalEvent::Type status = deleteEventInternal(event);
    if (mHaveDisabledAlarms)
        checkForDisabledAlarms();
    return status != CalEvent::EMPTY;
}

/******************************************************************************
* Delete the specified event from the calendar, if it exists.
* The calendar is then optionally saved.
*/
bool DisplayCalendar::deleteEvent(const QString& eventID, bool saveit)
{
    if (mOpen)
    {
        Event::Ptr kcalEvent;
        if (mCalendarStorage)
            kcalEvent = mCalendarStorage->calendar()->event(eventID);   // display calendar

        Resource resource;
        deleteEventBase(eventID, resource);

        CalEvent::Type status = CalEvent::EMPTY;
        if (kcalEvent)
        {
            status = CalEvent::status(kcalEvent);
            mCalendarStorage->calendar()->deleteEvent(kcalEvent);
        }

        if (status != CalEvent::EMPTY)
        {
            if (saveit)
                return save();
            return true;
        }
    }
    return false;
}

/******************************************************************************
* Internal method to delete the specified event from the calendar and lists.
* Reply = event status, if it was found in the resource calendar/calendar
*         resource or local calendar
*       = CalEvent::EMPTY otherwise.
*/
CalEvent::Type ResourcesCalendar::deleteEventInternal(const KAEvent& event, bool deleteFromResources)
{
    Resource resource = Resources::resource(event.resourceId());
    if (!resource.isValid())
        return CalEvent::EMPTY;
    return deleteEventInternal(event.id(), event, resource, deleteFromResources);
}

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
    KAEvent* ev = deleteEventBase(eventID, resource);
    if (ev)
    {
        if (mEarliestAlarm[key] == ev)
            findEarliestAlarm(resource);
    }
    else
    {
        for (EarliestMap::Iterator eit = mEarliestAlarm.begin();  eit != mEarliestAlarm.end();  ++eit)
        {
            ev = eit.value();
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
        // Delete from the resources calendar
        CalEvent::Type s = paramEvent.category();
        if (resource.deleteEvent(paramEvent))
            status = s;
    }
    return status;
}

/******************************************************************************
* Internal method to delete the specified event from the calendar and lists.
* Reply = event which was deleted, or null if not found.
*/
KAEvent* AlarmCalendar::deleteEventBase(const QString& eventID, Resource& resource)
{
    // Make a copy of the ID QString, since the supplied reference might be
    // destructed when the event is deleted below.
    const QString id = eventID;

    const ResourceId key = resource.id();
    KAEventMap::Iterator it = mEventMap.find(EventId(key, id));
    if (it == mEventMap.end())
        return nullptr;

    KAEvent* ev = it.value();
    mEventMap.erase(it);
    KAEvent::List& events = mResourceMap[key];
    int i = events.indexOf(ev);
    if (i >= 0)
        events.remove(i);
    delete ev;
    return ev;
}

/******************************************************************************
* Return the event with the specified ID.
* If 'findUniqueId' is true, and the resource ID is invalid, if there is a
* unique event with the given ID, it will be returned.
*/
KAEvent* AlarmCalendar::event(const EventId& uniqueID)
{
    if (!isValid())
        return nullptr;
    KAEventMap::ConstIterator it = mEventMap.constFind(uniqueID);
    if (it == mEventMap.constEnd())
        return nullptr;
    return it.value();
}

/******************************************************************************
* Return the event with the specified ID.
* If 'findUniqueId' is true, and the resource ID is invalid, if there is a
* unique event with the given ID, it will be returned.
*/
KAEvent* ResourcesCalendar::event(const EventId& uniqueID, bool findUniqueId)
{
    if (!isValid())
        return nullptr;
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
    if (isValid())
    {
        for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
        {
            const ResourceId id = rit.key();
            KAEventMap::ConstIterator it = mEventMap.constFind(EventId(id, uniqueId));
            if (it != mEventMap.constEnd())
                list += it.value();
        }
    }
    return list;
}

KAEvent::List ResourcesCalendar::events(const Resource& resource, CalEvent::Types type) const
{
    return AlarmCalendar::events(type, resource);
}

KAEvent::List ResourcesCalendar::events(CalEvent::Types type) const
{
    Resource resource;
    return AlarmCalendar::events(type, resource);
}


/******************************************************************************
* Return all events in the calendar which contain alarms.
* Optionally the event type can be filtered, using an OR of event types.
*/
KAEvent::List DisplayCalendar::events(CalEvent::Types type) const
{
    if (!mCalendarStorage)
        return KAEvent::List();
    Resource resource;
    return AlarmCalendar::events(type, resource);
}

KAEvent::List AlarmCalendar::events(CalEvent::Types type, const Resource& resource) const
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
* Return the event with the specified ID.
* This method is for the display calendar only.
*/
Event::Ptr DisplayCalendar::kcalEvent(const QString& uniqueID)
{
    if (!mCalendarStorage)
        return Event::Ptr();
    return mCalendarStorage->calendar()->event(uniqueID);
}

/******************************************************************************
* Return all events in the calendar which contain usable alarms.
* This method is for the display calendar only.
* Optionally the event type can be filtered, using an OR of event types.
*/
Event::List DisplayCalendar::kcalEvents(CalEvent::Type type)
{
    Event::List list;
    if (!mCalendarStorage)
        return list;
    list = mCalendarStorage->calendar()->rawEvents();
    for (int i = 0;  i < list.count();  )
    {
        Event::Ptr event = list.at(i);
        if (event->alarms().isEmpty()
        ||  (type != CalEvent::EMPTY  &&  !(type & CalEvent::status(event)))
        ||  !KAEvent(event).isValid())
            list.remove(i);
        else
            ++i;
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
//TODO   ||  compatibility(event) != KACalendar::Current;
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
void AlarmCalendar::adjustStartOfDay()
{
    if (!isValid())
        return;
    for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
        KAEvent::adjustStartOfDay(rit.value());
}

// vim: et sw=4:
