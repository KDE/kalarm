/*
 *  alarmcalendar.cpp  -  KAlarm calendar file access
 *  Program:  kalarm
 *  Copyright © 2001-2014 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"
#include "alarmcalendar.h"

#ifdef USE_AKONADI
#include "collectionmodel.h"
#else
#include "alarmresources.h"
#include "calendarcompat.h"
#include "eventlistmodel.h"
#endif
#include "filedialog.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "preferences.h"

#ifdef USE_AKONADI
#include <KCalCore/MemoryCalendar>
#include <KCalCore/ICalFormat>
#else
#include <kcal/calendarlocal.h>
#include <kcal/icalformat.h>
#endif

#include <kglobal.h>
#include <klocale.h>
#include <kstandarddirs.h>
#ifndef USE_AKONADI
#include <kconfig.h>
#endif
#include <kaboutdata.h>
#include <kio/netaccess.h>
#include <kfileitem.h>
#include <ktemporaryfile.h>
#include <kdebug.h>

#ifdef USE_AKONADI
using namespace Akonadi;
using namespace KCalCore;
#else
using namespace KCal;
#endif
using namespace KAlarmCal;

#ifdef USE_AKONADI
static KACalendar::Compat fix(const KCalCore::FileStorage::Ptr&);
#endif

static const QString displayCalendarName = QLatin1String("displaying.ics");
#ifdef USE_AKONADI
static const Collection::Id DISPLAY_COL_ID = -1;   // collection ID used for displaying calendar
#endif

AlarmCalendar* AlarmCalendar::mResourcesCalendar = 0;
AlarmCalendar* AlarmCalendar::mDisplayCalendar = 0;


/******************************************************************************
* Initialise the alarm calendars, and ensure that their file names are different.
* There are 2 calendars:
*  1) A resources calendar containing the active alarms, archived alarms and
*     alarm templates;
*  2) A user-specific one which contains details of alarms which are currently
*     being displayed to that user and which have not yet been acknowledged;
* Reply = true if success, false if calendar name error.
*/
bool AlarmCalendar::initialiseCalendars()
{
    QString displayCal = KStandardDirs::locateLocal("appdata", displayCalendarName);
#ifdef USE_AKONADI
    AkonadiModel::instance();
    CollectionControlModel::setAskDestinationPolicy(Preferences::askResource());
    Preferences::setBackend(Preferences::Akonadi);
#else
    AlarmResources::setDebugArea(5951);
    AlarmResources::setReservedFile(displayCal);
    AlarmResources* resources = AlarmResources::create(Preferences::timeZone(true), false);
    if (!resources)
    {
        KAlarmApp::displayFatalError(AlarmResources::creationError());
        return false;
    }
    resources->setAskDestinationPolicy(Preferences::askResource());
    resources->showProgress(true);
    Preferences::setBackend(Preferences::Kresources);
#endif
    Preferences::self()->writeConfig();
    mResourcesCalendar = new AlarmCalendar();
    mDisplayCalendar = new AlarmCalendar(displayCal, CalEvent::DISPLAYING);
    KACalendar::setProductId(KALARM_NAME, KALARM_VERSION);
    CalFormat::setApplication(QLatin1String(KALARM_NAME), QString::fromLatin1(KACalendar::icalProductId()));
    return true;
}

/******************************************************************************
* Terminate access to all calendars.
*/
void AlarmCalendar::terminateCalendars()
{
    delete mResourcesCalendar;
    mResourcesCalendar = 0;
    delete mDisplayCalendar;
    mDisplayCalendar = 0;
}

/******************************************************************************
* Return the display calendar, opening it first if necessary.
*/
AlarmCalendar* AlarmCalendar::displayCalendarOpen()
{
    if (mDisplayCalendar->open())
        return mDisplayCalendar;
    kError() << "Open error";
    return 0;
}

/******************************************************************************
* Find and return the event with the specified ID.
* The calendar searched is determined by the calendar identifier in the ID.
*/
#ifdef USE_AKONADI
KAEvent* AlarmCalendar::getEvent(const EventId& eventId)
{
    if (eventId.eventId().isEmpty())
        return 0;
    return mResourcesCalendar->event(eventId);
}
#else
KAEvent* AlarmCalendar::getEvent(const QString& uniqueId)
{
    if (uniqueId.isEmpty())
        return 0;
    KAEvent* event = mResourcesCalendar->event(uniqueId);
    if (!event)
        event = mDisplayCalendar->event(uniqueId);
    return event;
}
#endif

/******************************************************************************
* Constructor for the resources calendar.
*/
AlarmCalendar::AlarmCalendar()
    :
#ifndef USE_AKONADI
      mCalendar(0),
#endif
      mCalType(RESOURCES),
      mEventType(CalEvent::EMPTY),
      mOpen(false),
      mUpdateCount(0),
      mUpdateSave(false),
      mHaveDisabledAlarms(false)
{
#ifdef USE_AKONADI
    AkonadiModel* model = AkonadiModel::instance();
    connect(model, SIGNAL(eventsAdded(AkonadiModel::EventList)), SLOT(slotEventsAdded(AkonadiModel::EventList)));
    connect(model, SIGNAL(eventsToBeRemoved(AkonadiModel::EventList)), SLOT(slotEventsToBeRemoved(AkonadiModel::EventList)));
    connect(model, SIGNAL(eventChanged(AkonadiModel::Event)), SLOT(slotEventChanged(AkonadiModel::Event)));
    connect(model, SIGNAL(collectionStatusChanged(Akonadi::Collection,AkonadiModel::Change,QVariant,bool)),
                   SLOT(slotCollectionStatusChanged(Akonadi::Collection,AkonadiModel::Change,QVariant,bool)));
#else
    AlarmResources* resources = AlarmResources::instance();
    resources->setCalIDFunction(&KACalendar::setKAlarmVersion);
    resources->setFixFunction(&CalendarCompat::fix);
    resources->setCustomEventFunction(&updateResourceKAEvents);
    connect(resources, SIGNAL(resourceStatusChanged(AlarmResource*,AlarmResources::Change)), SLOT(slotResourceChange(AlarmResource*,AlarmResources::Change)));
    connect(resources, SIGNAL(cacheDownloaded(AlarmResource*)), SLOT(slotCacheDownloaded(AlarmResource*)));
    connect(resources, SIGNAL(resourceLoaded(AlarmResource*,bool)), SLOT(slotResourceLoaded(AlarmResource*,bool)));
#endif
    Preferences::connect(SIGNAL(askResourceChanged(bool)), this, SLOT(setAskResource(bool)));
}

/******************************************************************************
* Constructor for a calendar file.
*/
AlarmCalendar::AlarmCalendar(const QString& path, CalEvent::Type type)
    :
#ifndef USE_AKONADI
      mCalendar(0),
#endif
      mEventType(type),
      mOpen(false),
      mUpdateCount(0),
      mUpdateSave(false),
      mHaveDisabledAlarms(false)
{
    switch (type)
    {
        case CalEvent::ACTIVE:
        case CalEvent::ARCHIVED:
        case CalEvent::TEMPLATE:
        case CalEvent::DISPLAYING:
            break;
        default:
            Q_ASSERT(false);   // invalid event type for a calendar
            break;
    }
    mUrl.setPath(path);       // N.B. constructor mUrl(path) doesn't work with UNIX paths
    QString icalPath = path;
    icalPath.replace(QLatin1String("\\.vcs$"), QLatin1String(".ics"));
    mICalUrl.setPath(icalPath);
    mCalType = (path == icalPath) ? LOCAL_ICAL : LOCAL_VCAL;    // is the calendar in ICal or VCal format?
}

AlarmCalendar::~AlarmCalendar()
{
    close();
}

/******************************************************************************
* Check whether the calendar is open.
*/
bool AlarmCalendar::isOpen()
{
#ifndef USE_AKONADI
    if (mOpen  &&  mCalType == RESOURCES  &&  !AlarmResources::instance())
    {
        mCalendar = 0;
        mOpen = false;
    }
#endif
    return mOpen;
}

/******************************************************************************
* Open the calendar if not already open, and load it into memory.
*/
bool AlarmCalendar::open()
{
    if (isOpen())
        return true;
    if (mCalType == RESOURCES)
    {
#ifdef USE_AKONADI
        mOpen = true;
#else
        kDebug() << "RESOURCES";
        mCalendar = AlarmResources::instance();
        load();
#endif
    }
    else
    {
        if (!mUrl.isValid())
            return false;

        kDebug() << mUrl.prettyUrl();
#ifdef USE_AKONADI
        if (!mCalendarStorage)
        {
            MemoryCalendar::Ptr calendar(new MemoryCalendar(Preferences::timeZone(true)));
            mCalendarStorage = FileStorage::Ptr(new FileStorage(calendar));
        }
#else
        if (!mCalendar)
            mCalendar = new CalendarLocal(Preferences::timeZone(true));
#endif

        // Check for file's existence, assuming that it does exist when uncertain,
        // to avoid overwriting it.
        if (!KIO::NetAccess::exists(mUrl, KIO::NetAccess::SourceSide, MainWindow::mainMainWindow())
        ||  load() == 0)
        {
            // The calendar file doesn't yet exist, or it's zero length, so create a new one
            bool created = false;
            if (mICalUrl.isLocalFile())
                created = saveCal(mICalUrl.toLocalFile());
            else
            {
                KTemporaryFile tmpFile;
                tmpFile.setAutoRemove(false);
                tmpFile.open();
                created = saveCal(tmpFile.fileName());
            }
            if (created)
                load();
        }
    }
    if (!mOpen)
    {
#ifdef USE_AKONADI
        mCalendarStorage->calendar().clear();
        mCalendarStorage.clear();
#else
        delete mCalendar;
        mCalendar = 0;
#endif
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
int AlarmCalendar::load()
{
    if (mCalType == RESOURCES)
    {
#ifndef USE_AKONADI
        kDebug() << "RESOURCES";
        static_cast<AlarmResources*>(mCalendar)->load();
#endif
    }
    else
    {
#ifdef USE_AKONADI
        if (!mCalendarStorage)
            return -2;
#else
        if (!mCalendar)
            return -2;
        CalendarLocal* calendar = static_cast<CalendarLocal*>(mCalendar);
#endif

        kDebug() << mUrl.prettyUrl();
        QString tmpFile;
        if (!KIO::NetAccess::download(mUrl, tmpFile, MainWindow::mainMainWindow()))
        {
            kError() << "Download failure";
            KAMessageBox::error(MainWindow::mainMainWindow(),
                                i18nc("@info", "Cannot download calendar: <filename>%1</filename>", mUrl.prettyUrl()));
            return -1;
        }
        kDebug() << "--- Downloaded to" << tmpFile;
#ifdef USE_AKONADI
        mCalendarStorage->calendar()->setTimeSpec(Preferences::timeZone(true));
        mCalendarStorage->setFileName(tmpFile);
        if (!mCalendarStorage->load())
#else
        calendar->setTimeSpec(Preferences::timeZone(true));
        if (!calendar->load(tmpFile))
#endif
        {
            // Check if the file is zero length
            KIO::NetAccess::removeTempFile(tmpFile);
            KIO::UDSEntry uds;
            KIO::NetAccess::stat(mUrl, uds, MainWindow::mainMainWindow());
            KFileItem fi(uds, mUrl);
            if (!fi.size())
                return 0;     // file is zero length
            kError() << "Error loading calendar file '" << tmpFile <<"'";
            KAMessageBox::error(MainWindow::mainMainWindow(),
                                i18nc("@info", "<para>Error loading calendar:</para><para><filename>%1</filename></para><para>Please fix or delete the file.</para>", mUrl.prettyUrl()));
            // load() could have partially populated the calendar, so clear it out
#ifdef USE_AKONADI
            mCalendarStorage->calendar()->close();
            mCalendarStorage->calendar().clear();
            mCalendarStorage.clear();
#else
            calendar->close();
            delete mCalendar;
            mCalendar = 0;
#endif
            mOpen = false;
            return -1;
        }
        if (!mLocalFile.isEmpty())
            KIO::NetAccess::removeTempFile(mLocalFile);   // removes it only if it IS a temporary file
        mLocalFile = tmpFile;
#ifdef USE_AKONADI
        fix(mCalendarStorage);   // convert events to current KAlarm format for when calendar is saved
        updateDisplayKAEvents();
#else
        CalendarCompat::fix(*calendar, mLocalFile);   // convert events to current KAlarm format for when calendar is saved
        updateKAEvents(0, calendar);
#endif
    }
    mOpen = true;
    return 1;
}

/******************************************************************************
* Reload the calendar file into memory.
*/
bool AlarmCalendar::reload()
{
#ifdef USE_AKONADI
    if (mCalType == RESOURCES)
        return true;
    if (!mCalendarStorage)
        return false;
#else
    if (!mCalendar)
        return false;
    if (mCalType == RESOURCES)
    {
        kDebug() << "RESOURCES";
        return mCalendar->reload();
    }
    else
#endif
    {
        kDebug() << mUrl.prettyUrl();
        close();
        return open();
    }
}

/******************************************************************************
* Save the calendar from memory to file.
* If a filename is specified, create a new calendar file.
*/
bool AlarmCalendar::saveCal(const QString& newFile)
{
#ifdef USE_AKONADI
    if (mCalType == RESOURCES)
        return true;
    if (!mCalendarStorage)
        return false;
#else
    if (!mCalendar)
        return false;
    if (mCalType == RESOURCES)
    {
        kDebug() << "RESOURCES";
        mCalendar->save();    // this emits signals resourceSaved(ResourceCalendar*)
    }
    else
#endif
    {
        if (!mOpen && newFile.isNull())
            return false;

        kDebug() << "\"" << newFile << "\"," << mEventType;
        QString saveFilename = newFile.isNull() ? mLocalFile : newFile;
        if (mCalType == LOCAL_VCAL  &&  newFile.isNull()  &&  mUrl.isLocalFile())
            saveFilename = mICalUrl.toLocalFile();
#ifdef USE_AKONADI
        mCalendarStorage->setFileName(saveFilename);
        mCalendarStorage->setSaveFormat(new ICalFormat);
        if (!mCalendarStorage->save())
#else
        if (!static_cast<CalendarLocal*>(mCalendar)->save(saveFilename, new ICalFormat))
#endif
        {
            kError() << "Saving" << saveFilename << "failed.";
            KAMessageBox::error(MainWindow::mainMainWindow(),
                                i18nc("@info", "Failed to save calendar to <filename>%1</filename>", mICalUrl.prettyUrl()));
            return false;
        }

        if (!mICalUrl.isLocalFile())
        {
            if (!KIO::NetAccess::upload(saveFilename, mICalUrl, MainWindow::mainMainWindow()))
            {
                kError() << saveFilename << "upload failed.";
                KAMessageBox::error(MainWindow::mainMainWindow(),
                                    i18nc("@info", "Cannot upload calendar to <filename>%1</filename>", mICalUrl.prettyUrl()));
                return false;
            }
        }

        if (mCalType == LOCAL_VCAL)
        {
            // The file was in vCalendar format, but has now been saved in iCalendar format.
            mUrl  = mICalUrl;
            mCalType = LOCAL_ICAL;
        }
        emit calendarSaved(this);
    }

    mUpdateSave = false;
    return true;
}

/******************************************************************************
* Delete any temporary file at program exit.
*/
void AlarmCalendar::close()
{
    if (mCalType != RESOURCES)
    {
        if (!mLocalFile.isEmpty())
        {
            KIO::NetAccess::removeTempFile(mLocalFile);   // removes it only if it IS a temporary file
            mLocalFile = QLatin1String("");
        }
    }
    // Flag as closed now to prevent removeKAEvents() doing silly things
    // when it's called again
    mOpen = false;
#ifdef USE_AKONADI
    if (mCalendarStorage)
    {
        mCalendarStorage->calendar()->close();
        mCalendarStorage->calendar().clear();
        mCalendarStorage.clear();
    }
#else
    if (mCalendar)
    {
        mCalendar->close();
        delete mCalendar;
        mCalendar = 0;
    }
#endif
    // Resource map should be empty, but just in case...
    while (!mResourceMap.isEmpty())
#ifdef USE_AKONADI
        removeKAEvents(mResourceMap.begin().key(), true, CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE | CalEvent::DISPLAYING);
#else
        removeKAEvents(mResourceMap.begin().key(), true);
#endif
}

#ifndef USE_AKONADI
/******************************************************************************
* Load a single resource. If the resource is cached, the cache is refreshed.
*/
void AlarmCalendar::loadResource(AlarmResource* resource, QWidget*)
{
    if (!AlarmResources::instance()->load(resource, ResourceCached::SyncCache))
        slotResourceLoaded(resource, false);
}

/******************************************************************************
* Called when a remote resource cache has completed loading.
*/
void AlarmCalendar::slotCacheDownloaded(AlarmResource* resource)
{
    slotResourceLoaded(resource, false);
}
#endif

/******************************************************************************
* Update whether to prompt for the resource to store new alarms in.
*/
void AlarmCalendar::setAskResource(bool ask)
{
#ifdef USE_AKONADI
    CollectionControlModel::setAskDestinationPolicy(ask);
#else
    AlarmResources::instance()->setAskDestinationPolicy(ask);
#endif
}

#ifndef USE_AKONADI
/******************************************************************************
* Create a KAEvent instance corresponding to each KCal::Event in a resource.
* Called after the resource has completed loading.
* The event list is simply cleared if 'cal' is null.
*/
void AlarmCalendar::updateResourceKAEvents(AlarmResource* resource, KCal::CalendarLocal* cal)
{
    mResourcesCalendar->updateKAEvents(resource, cal);
}
#endif

#ifdef USE_AKONADI
void AlarmCalendar::updateDisplayKAEvents()
#else
void AlarmCalendar::updateKAEvents(AlarmResource* resource, KCal::CalendarLocal* cal)
#endif
{
#ifdef USE_AKONADI
    if (mCalType == RESOURCES)
        return;
    kDebug();
    const Collection::Id key = DISPLAY_COL_ID;
#else
    kDebug() << (resource ? resource->resourceName() : "0");
    AlarmResource* key = resource;
#endif
    KAEvent::List& events = mResourceMap[key];
    int i, end;
    for (i = 0, end = events.count();  i < end;  ++i)
    {
        KAEvent* event = events[i];
#ifdef USE_AKONADI
        mEventMap.remove(EventId(key, event->id()));
#else
        mEventMap.remove(event->id());
#endif
        delete event;
    }
    events.clear();
    mEarliestAlarm[key] = 0;
#ifdef USE_AKONADI
    Calendar::Ptr cal = mCalendarStorage->calendar();
#endif
    if (!cal)
        return;

#ifndef USE_AKONADI
    KConfigGroup config(KGlobal::config(), KAEvent::commandErrorConfigGroup());
#endif
    Event::List kcalevents = cal->rawEvents();
    for (i = 0, end = kcalevents.count();  i < end;  ++i)
    {
#ifdef USE_AKONADI
        Event::Ptr kcalevent = kcalevents[i];
#else
        const Event* kcalevent = kcalevents[i];
#endif
        if (kcalevent->alarms().isEmpty())
            continue;    // ignore events without alarms

        KAEvent* event = new KAEvent(kcalevent);
        if (!event->isValid())
        {
            kWarning() << "Ignoring unusable event" << kcalevent->uid();
            delete event;
            continue;    // ignore events without usable alarms
        }
#ifdef USE_AKONADI
        event->setCollectionId(key);
        events += event;
        mEventMap[EventId(key, kcalevent->uid())] = event;
#else
        event->setResource(resource);
        events += event;
        mEventMap[kcalevent->uid()] = event;

        // Set any command execution error flags for the alarm.
        // These are stored in the KAlarm config file, not the alarm
        // calendar, since they are specific to the user's local system.
        QString cmdErr = config.readEntry(event->id());
        if (!cmdErr.isEmpty())
            event->setCommandError(cmdErr);
#endif
    }

#ifndef USE_AKONADI
    // Now scan the list of alarms to find the earliest one to trigger
    findEarliestAlarm(resource);
    checkForDisabledAlarms();
#endif
}

#ifdef USE_AKONADI
/******************************************************************************
* Delete a calendar and all its KAEvent instances of specified alarm types from
* the lists.
* Called after the calendar is deleted or alarm types have been disabled, or
* the AlarmCalendar is closed.
*/
void AlarmCalendar::removeKAEvents(Collection::Id key, bool closing, CalEvent::Types types)
{
    bool removed = false;
    ResourceMap::Iterator rit = mResourceMap.find(key);
    if (rit != mResourceMap.end())
    {
        bool empty = true;
        KAEvent::List& events = rit.value();
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            KAEvent* event = events[i];
            bool remove = (event->collectionId() != key);
            if (remove)
            {
                if (key != DISPLAY_COL_ID)
                    kError() << "Event" << event->id() << ", collection" << event->collectionId() << "Indexed under collection" << key;
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
                empty = false;
        }
        if (empty)
            mResourceMap.erase(rit);
    }
    if (removed)
    {
        mEarliestAlarm.remove(key);
        // Emit signal only if we're not in the process of closing the calendar
        if (!closing  &&  mOpen)
        {
            emit earliestAlarmChanged();
            if (mHaveDisabledAlarms)
                checkForDisabledAlarms();
        }
    }
}
#else
/******************************************************************************
* Delete a calendar and all its KAEvent instances from the lists.
* Called after the calendar is deleted or disabled, or the AlarmCalendar is
* closed.
*/
void AlarmCalendar::removeKAEvents(AlarmResource* key, bool closing)
{
    ResourceMap::Iterator rit = mResourceMap.find(key);
    if (rit != mResourceMap.end())
    {
        KAEvent::List& events = rit.value();
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            KAEvent* event = events[i];
            mEventMap.remove(event->id());
            delete event;
        }
        mResourceMap.erase(rit);
    }
    mEarliestAlarm.remove(key);
    // Emit signal only if we're not in the process of closing the calendar
    if (!closing  &&  mOpen)
    {
        emit earliestAlarmChanged();
        if (mHaveDisabledAlarms)
            checkForDisabledAlarms();
    }
}
#endif

#ifdef USE_AKONADI
/******************************************************************************
* Called when the enabled or read-only status of a collection has changed.
* If the collection is now disabled, remove its events from the calendar.
*/
void AlarmCalendar::slotCollectionStatusChanged(const Collection& collection, AkonadiModel::Change change, const QVariant& value, bool inserted)
{
    if (!inserted  &&  change == AkonadiModel::Enabled)
    {
        // For each alarm type which has been disabled, remove the collection's
        // events from the map, but not from AkonadiModel.
        CalEvent::Types enabled = static_cast<CalEvent::Types>(value.toInt());
        CalEvent::Types disabled = ~enabled & (CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
        removeKAEvents(collection.id(), false, disabled);
    }
}

/******************************************************************************
* Called when events have been added to AkonadiModel.
* Add corresponding KAEvent instances to those held by AlarmCalendar.
*/
void AlarmCalendar::slotEventsAdded(const AkonadiModel::EventList& events)
{
    for (int i = 0, count = events.count();  i < count;  ++i)
        slotEventChanged(events[i]);
}

/******************************************************************************
* Called when an event has been changed in AkonadiModel.
* Change the corresponding KAEvent instance held by AlarmCalendar.
*/
void AlarmCalendar::slotEventChanged(const AkonadiModel::Event& event)
{
    if (!event.isConsistent())
    {
        kError() << "Inconsistent AkonadiModel::Event: event:" << event.event.collectionId() << ", collection" << event.collection.id();
        return;
    }

    bool added = true;
    bool updated = false;
    KAEventMap::Iterator it = mEventMap.find(event.eventId());
    if (it != mEventMap.end())
    {
        // The event ID already exists - remove the existing event first
        KAEvent* storedEvent = it.value();
        if (event.event.category() == storedEvent->category())
        {
            // The existing event is the same type - update it in place
            *storedEvent = event.event;
            addNewEvent(event.collection, storedEvent, true);
            updated = true;
        }
        else
            delete storedEvent;
        added = false;
    }
    if (!updated)
        addNewEvent(event.collection, new KAEvent(event.event));

    bool enabled = event.event.enabled();
    checkForDisabledAlarms(!enabled, enabled);
    if (added  &&  enabled  &&  event.event.category() == CalEvent::ACTIVE
    &&  event.event.repeatAtLogin())
        emit atLoginEventAdded(event.event);
}

/******************************************************************************
* Called when events are about to be removed from AkonadiModel.
* Remove the corresponding KAEvent instances held by AlarmCalendar.
*/
void AlarmCalendar::slotEventsToBeRemoved(const AkonadiModel::EventList& events)
{
    for (int i = 0, count = events.count();  i < count;  ++i)
    {
        if (!events[i].isConsistent())
            kError() << "Inconsistent AkonadiModel::Event: event:" << events[i].event.collectionId() << ", collection" << events[i].collection.id();
        else if (mEventMap.contains(events[i].eventId()))
            deleteEventInternal(events[i].event, events[i].collection, false);
    }
}
#else

void AlarmCalendar::slotResourceChange(AlarmResource* resource, AlarmResources::Change change)
{
    switch (change)
    {
        case AlarmResources::Enabled:
            if (resource->isActive())
                return;
            kDebug() << "Enabled (inactive)";
            break;
        case AlarmResources::Invalidated:
            kDebug() << "Invalidated";
            break;
        case AlarmResources::Deleted:
            kDebug() << "Deleted";
            break;
        default:
            return;
    }
    // Ensure the data model is notified before deleting the KAEvent instances
    EventListModel::resourceStatusChanged(resource, change);
    removeKAEvents(resource);
}

/******************************************************************************
* Called when a resource has completed loading.
*/
void AlarmCalendar::slotResourceLoaded(AlarmResource* resource, bool success)
{
}

/******************************************************************************
* Reload a resource from its cache file, without refreshing the cache first.
*/
void AlarmCalendar::reloadFromCache(const QString& resourceID)
{
    kDebug() << resourceID;
    if (mCalendar  &&  mCalType == RESOURCES)
    {
        AlarmResource* resource = static_cast<AlarmResources*>(mCalendar)->resourceWithId(resourceID);
        if (resource)
            resource->load(ResourceCached::NoSyncCache);    // reload from cache
    }
}
#endif

/******************************************************************************
* Import alarms from an external calendar and merge them into KAlarm's calendar.
* The alarms are given new unique event IDs.
* Parameters: parent = parent widget for error message boxes
* Reply = true if all alarms in the calendar were successfully imported
*       = false if any alarms failed to be imported.
*/
#ifdef USE_AKONADI
bool AlarmCalendar::importAlarms(QWidget* parent, Collection* collection)
#else
bool AlarmCalendar::importAlarms(QWidget* parent, AlarmResource* resource)
#endif
{
    kDebug();
    KUrl url = KFileDialog::getOpenUrl(KUrl("filedialog:///importalarms"),
                                       QString::fromLatin1("*.vcs *.ics|%1").arg(i18nc("@info/plain", "Calendar Files")), parent);
    if (url.isEmpty())
    {
        kError() << "Empty URL";
        return false;
    }
    if (!url.isValid())
    {
        kDebug() << "Invalid URL";
        return false;
    }
    kDebug() << url.prettyUrl();

    bool success = true;
    QString filename;
    bool local = url.isLocalFile();
    if (local)
    {
        filename = url.toLocalFile();
        if (!KStandardDirs::exists(filename))
        {
            kDebug() << "File '" << url.prettyUrl() <<"' not found";
            KAMessageBox::error(parent, i18nc("@info", "Could not load calendar <filename>%1</filename>.", url.prettyUrl()));
            return false;
        }
    }
    else
    {
        if (!KIO::NetAccess::download(url, filename, MainWindow::mainMainWindow()))
        {
            kError() << "Download failure";
            KAMessageBox::error(parent, i18nc("@info", "Cannot download calendar: <filename>%1</filename>", url.prettyUrl()));
            return false;
        }
        kDebug() << "--- Downloaded to" << filename;
    }

    // Read the calendar and add its alarms to the current calendars
#ifdef USE_AKONADI
    MemoryCalendar::Ptr cal(new MemoryCalendar(Preferences::timeZone(true)));
    FileStorage::Ptr calStorage(new FileStorage(cal, filename));
    success = calStorage->load();
#else
    CalendarLocal cal(Preferences::timeZone(true));
    success = cal.load(filename);
#endif
    if (!success)
    {
        kDebug() << "Error loading calendar '" << filename <<"'";
        KAMessageBox::error(parent, i18nc("@info", "Could not load calendar <filename>%1</filename>.", url.prettyUrl()));
    }
    else
    {
#ifdef USE_AKONADI
        KACalendar::Compat caltype = fix(calStorage);
        CalEvent::Types wantedTypes = collection && collection->isValid() ? CalEvent::types(collection->contentMimeTypes()) : CalEvent::EMPTY;
        Collection activeColl, archiveColl, templateColl;
#else
        KACalendar::Compat caltype = CalendarCompat::fix(cal, filename);
        CalEvent::Type wantedType = resource ? resource->alarmType() : CalEvent::EMPTY;
        AlarmResources* resources = AlarmResources::instance();
        AlarmResource* activeRes   = 0;
        AlarmResource* archivedRes = 0;
        AlarmResource* templateRes = 0;
        bool saveRes = false;
        bool enabled = true;
#endif
        KAEvent::List newEvents;
#ifdef USE_AKONADI
        Event::List events = cal->rawEvents();
#else
        Event::List events = cal.rawEvents();
#endif
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
#ifdef USE_AKONADI
            Event::Ptr event = events[i];
#else
            const Event* event = events[i];
#endif
            if (event->alarms().isEmpty()  ||  !KAEvent(event).isValid())
                continue;    // ignore events without alarms, or usable alarms
            CalEvent::Type type = CalEvent::status(event);
            if (type == CalEvent::TEMPLATE)
            {
                // If we know the event was not created by KAlarm, don't treat it as a template
                if (caltype == KACalendar::Incompatible)
                    type = CalEvent::ACTIVE;
            }
#ifdef USE_AKONADI
            Collection* coll;
            if (collection  &&  collection->isValid())
            {
                if (!(type & wantedTypes))
                    continue;
                coll = collection;
            }
            else
            {
                switch (type)
                {
                    case CalEvent::ACTIVE:    coll = &activeColl;  break;
                    case CalEvent::ARCHIVED:  coll = &archiveColl;  break;
                    case CalEvent::TEMPLATE:  coll = &templateColl;  break;
                    default:  continue;
                }
                if (!coll->isValid())
                    *coll = CollectionControlModel::destination(type);
            }

            Event::Ptr newev(new Event(*event));
#else
            AlarmResource** res;
            if (resource)
            {
                if (type != wantedType)
                    continue;
                res = &resource;
            }
            else
            {
                switch (type)
                {
                    case CalEvent::ACTIVE:    res = &activeRes;  break;
                    case CalEvent::ARCHIVED:  res = &archivedRes;  break;
                    case CalEvent::TEMPLATE:  res = &templateRes;  break;
                    default:  continue;
                }
                if (!*res)
                    *res = resources->destination(type);
            }

            Event* newev = new Event(*event);
#endif

            // If there is a display alarm without display text, use the event
            // summary text instead.
            if (type == CalEvent::ACTIVE  &&  !newev->summary().isEmpty())
            {
                const Alarm::List& alarms = newev->alarms();
                for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
                {
#ifdef USE_AKONADI
                    Alarm::Ptr alarm = alarms[ai];
#else
                    Alarm* alarm = alarms[ai];
#endif
                    if (alarm->type() == Alarm::Display  &&  alarm->text().isEmpty())
                        alarm->setText(newev->summary());
                }
                newev->setSummary(QString());   // KAlarm only uses summary for template names
            }

            // Give the event a new ID and add it to the calendars
            newev->setUid(CalEvent::uid(CalFormat::createUniqueId(), type));
#ifdef USE_AKONADI
            KAEvent* newEvent = new KAEvent(newev);
            if (!AkonadiModel::instance()->addEvent(*newEvent, *coll))
                success = false;
#else
            if (resources->addEvent(newev, *res))
            {
                saveRes = true;
                KAEvent* ev = mResourcesCalendar->addEvent(*res, newev);
                if (type != CalEvent::TEMPLATE)
                    newEvents += ev;
                if (type == CalEvent::ACTIVE  &&  !ev->enabled())
                    enabled = false;
            }
            else
                success = false;
#endif
        }

#ifndef USE_AKONADI
        // Save the resources if they have been modified
        if (saveRes)
        {
            resources->save();
            EventListModel::alarms()->addEvents(newEvents);
            if (!enabled)
                mResourcesCalendar->checkForDisabledAlarms(true, enabled);
        }
#endif
    }
    if (!local)
        KIO::NetAccess::removeTempFile(filename);
    return success;
}

/******************************************************************************
* Export all selected alarms to an external calendar.
* The alarms are given new unique event IDs.
* Parameters: parent = parent widget for error message boxes
* Reply = true if all alarms in the calendar were successfully exported
*       = false if any alarms failed to be exported.
*/
bool AlarmCalendar::exportAlarms(const KAEvent::List& events, QWidget* parent)
{
    bool append;
    QString file = FileDialog::getSaveFileName(KUrl("kfiledialog:///exportalarms"),
                                               QString::fromLatin1("*.ics|%1").arg(i18nc("@info/plain", "Calendar Files")),
                                               parent, i18nc("@title:window", "Choose Export Calendar"),
                                               &append);
    if (file.isEmpty())
        return false;
    KUrl url;
    url.setPath(file);
    if (!url.isValid())
    {
        kDebug() << "Invalid URL";
        return false;
    }
    kDebug() << url.prettyUrl();

#ifdef USE_AKONADI
    MemoryCalendar::Ptr calendar(new MemoryCalendar(Preferences::timeZone(true)));
    FileStorage::Ptr calStorage(new FileStorage(calendar, file));
    if (append  &&  !calStorage->load())
#else
    CalendarLocal calendar(Preferences::timeZone(true));
    if (append  &&  !calendar.load(file))
#endif
    {
        KIO::UDSEntry uds;
        KIO::NetAccess::stat(url, uds, parent);
        KFileItem fi(uds, url);
        if (fi.size())
        {
            kError() << "Error loading calendar file" << file << "for append";
            KAMessageBox::error(MainWindow::mainMainWindow(),
                                i18nc("@info", "Error loading calendar to append to:<nl/><filename>%1</filename>", url.prettyUrl()));
            return false;
        }
    }
    KACalendar::setKAlarmVersion(calendar);

    // Add the alarms to the calendar
    bool success = true;
    bool exported = false;
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
        const KAEvent* event = events[i];
#ifdef USE_AKONADI
        Event::Ptr kcalEvent(new Event);
#else
        Event* kcalEvent = new Event;
#endif
        CalEvent::Type type = event->category();
        QString id = CalEvent::uid(kcalEvent->uid(), type);
        kcalEvent->setUid(id);
        event->updateKCalEvent(kcalEvent, KAEvent::UID_IGNORE);
#ifdef USE_AKONADI
        if (calendar->addEvent(kcalEvent))
#else
        if (calendar.addEvent(kcalEvent))
#endif
            exported = true;
        else
            success = false;
    }

    if (exported)
    {
        // One or more alarms have been exported to the calendar.
        // Save the calendar to file.
        KTemporaryFile* tempFile = 0;
        bool local = url.isLocalFile();
        if (!local)
        {
            tempFile = new KTemporaryFile;
            file = tempFile->fileName();
        }
#ifdef USE_AKONADI
        calStorage->setFileName(file);
        calStorage->setSaveFormat(new ICalFormat);
        if (!calStorage->save())
#else
        if (!calendar.save(file, new ICalFormat))
#endif
        {
            kError() << file << ": failed";
            KAMessageBox::error(MainWindow::mainMainWindow(),
                                i18nc("@info", "Failed to save new calendar to:<nl/><filename>%1</filename>", url.prettyUrl()));
            success = false;
        }
        else if (!local  &&  !KIO::NetAccess::upload(file, url, parent))
        {
            kError() << file << ": upload failed";
            KAMessageBox::error(MainWindow::mainMainWindow(),
                                i18nc("@info", "Cannot upload new calendar to:<nl/><filename>%1</filename>", url.prettyUrl()));
            success = false;
        }
        delete tempFile;
    }
#ifdef USE_AKONADI
    calendar->close();
#else
    calendar.close();
#endif
    return success;
}

/******************************************************************************
* Flag the start of a group of calendar update calls.
* The purpose is to avoid multiple calendar saves during a group of operations.
*/
void AlarmCalendar::startUpdate()
{
    ++mUpdateCount;
}

/******************************************************************************
* Flag the end of a group of calendar update calls.
* The calendar is saved if appropriate.
*/
bool AlarmCalendar::endUpdate()
{
    if (mUpdateCount > 0)
        --mUpdateCount;
    if (!mUpdateCount)
    {
        if (mUpdateSave)
            return saveCal();
    }
    return true;
}

/******************************************************************************
* Save the calendar, or flag it for saving if in a group of calendar update calls.
* Note that this method has no effect for Akonadi calendars.
*/
bool AlarmCalendar::save()
{
    if (mUpdateCount)
    {
        mUpdateSave = true;
        return true;
    }
    else
        return saveCal();
}

/******************************************************************************
* This method must only be called from the main KAlarm queue processing loop,
* to prevent asynchronous calendar operations interfering with one another.
*
* Purge a list of archived events from the calendar.
*/
void AlarmCalendar::purgeEvents(const KAEvent::List& events)
{
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
#ifdef USE_AKONADI
        deleteEventInternal(*events[i]);
#else
        deleteEventInternal(events[i]->id());
#endif
    }
    if (mHaveDisabledAlarms)
        checkForDisabledAlarms();
    saveCal();
}

/******************************************************************************
* Add the specified event to the calendar.
* If it is an active event and 'useEventID' is false, a new event ID is
* created. In all other cases, the event ID is taken from 'event' (if non-null).
* 'event' is updated with the actual event ID.
* The event is added to 'resource' if specified; otherwise the default resource
* is used or the user is prompted, depending on policy. If 'noPrompt' is true,
* the user will not be prompted so that if no default resource is defined, the
* function will fail.
* Reply = true if 'event' was written to the calendar, in which case (not
*              Akonadi) ownership of 'event' is taken by the calendar. 'event'
*              is updated.
*       = false if an error occurred, in which case 'event' is unchanged.
*/
#ifdef USE_AKONADI
bool AlarmCalendar::addEvent(KAEvent& evnt, QWidget* promptParent, bool useEventID, Collection* collection, bool noPrompt, bool* cancelled)
#else
bool AlarmCalendar::addEvent(KAEvent* event, QWidget* promptParent, bool useEventID, AlarmResource* resource, bool noPrompt, bool* cancelled)
#endif
{
    if (cancelled)
        *cancelled = false;
    if (!mOpen)
        return false;
    // Check that the event type is valid for the calendar
#ifdef USE_AKONADI
    kDebug() << evnt.id();
    CalEvent::Type type = evnt.category();
#else
    kDebug() << event->id();
    CalEvent::Type type = event->category();
#endif
    if (type != mEventType)
    {
        switch (type)
        {
            case CalEvent::ACTIVE:
            case CalEvent::ARCHIVED:
            case CalEvent::TEMPLATE:
                if (mEventType == CalEvent::EMPTY)
                    break;
                // fall through to default
            default:
                return false;
        }
    }

#ifdef USE_AKONADI
    Collection::Id key = (collection && collection->isValid()) ? collection->id() : -1;
    Event::Ptr kcalEvent((mCalType == RESOURCES) ? (Event*)0 : new Event);
    KAEvent* event = new KAEvent(evnt);
#else
    AlarmResource* key = resource;
    Event* kcalEvent = new Event;
    KAEvent oldEvent(*event);    // so that we can reinstate it if there's an error
#endif
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
#ifdef USE_AKONADI
        id = (mCalType == RESOURCES) ? CalFormat::createUniqueId() : kcalEvent->uid();
#else
        id = kcalEvent->uid();
#endif
    if (useEventID)
    {
        id = CalEvent::uid(id, type);
#ifdef USE_AKONADI
        if (kcalEvent)
#endif
            kcalEvent->setUid(id);
    }
    event->setEventId(id);
#ifndef USE_AKONADI
    event->updateKCalEvent(kcalEvent, KAEvent::UID_IGNORE);
#endif
    bool ok = false;
    bool remove = false;
    if (mCalType == RESOURCES)
    {
#ifdef USE_AKONADI
        Collection col;
        if (collection  &&  CollectionControlModel::isEnabled(*collection, type))
            col = *collection;
        else
            col = CollectionControlModel::destination(type, promptParent, noPrompt, cancelled);
        if (col.isValid())
#else
        if (!resource)
            resource = AlarmResources::instance()->destination(type, promptParent, noPrompt, cancelled);
        if (resource  &&  addEvent(resource, event))
#endif
        {
#ifdef USE_AKONADI
            // Don't add event to mEventMap yet - its Akonadi item id is not yet known.
            // It will be added once it is inserted into AkonadiModel.
            ok = AkonadiModel::instance()->addEvent(*event, col);
            remove = ok;   // if success, delete the local event instance on exit
#else
            ok = AlarmResources::instance()->addEvent(kcalEvent, resource);
            kcalEvent = 0;  // if there was an error, kcalEvent is deleted by AlarmResources::addEvent()
            remove = !ok;
#endif
            if (ok  &&  type == CalEvent::ACTIVE  &&  !event->enabled())
                checkForDisabledAlarms(true, false);
        }
    }
    else
    {
        // It's the display calendar
#ifdef USE_AKONADI
        event->updateKCalEvent(kcalEvent, KAEvent::UID_IGNORE);
        key = DISPLAY_COL_ID;
        if (!mEventMap.contains(EventId(key, event->id())))
        {
            addNewEvent(Collection(), event);
            ok = mCalendarStorage->calendar()->addEvent(kcalEvent);
            remove = !ok;
        }
#else
        key = 0;
        if (addEvent(0, event))
        {
            ok = mCalendar->addEvent(kcalEvent);
            remove = !ok;
        }
#endif
    }
    if (!ok)
    {
        if (remove)
        {
            // Adding to mCalendar failed, so undo AlarmCalendar::addEvent()
#ifdef USE_AKONADI
            mEventMap.remove(EventId(key, event->id()));
#else
            mEventMap.remove(event->id());
#endif
            KAEvent::List& events = mResourceMap[key];
            int i = events.indexOf(event);
            if (i >= 0)
                events.remove(i);
            if (mEarliestAlarm[key] == event)
                findEarliestAlarm(key);
        }
#ifdef USE_AKONADI
        delete event;
#else
        *event = oldEvent;
        delete kcalEvent;
#endif
        return false;
    }
#ifdef USE_AKONADI
    evnt = *event;
    if (remove)
        delete event;
#endif
    return true;
}

#ifndef USE_AKONADI
/******************************************************************************
* Internal method to add an event to the calendar.
* The calendar takes ownership of 'event'.
* Reply = true if success
*       = false if error because the event ID already exists.
*/
bool AlarmCalendar::addEvent(AlarmResource* resource, KAEvent* event)
{
    kDebug() << "KAEvent:" << event->id();
    if (mEventMap.contains(event->id()))
        return false;
    addNewEvent(resource, event);
    return true;
}

/******************************************************************************
* Internal method to add an event to the calendar.
* Reply = event as stored in calendar
*       = 0 if error because the event ID already exists.
*/
KAEvent* AlarmCalendar::addEvent(AlarmResource* resource, const Event* kcalEvent)
{
    kDebug() << "Event:" << kcalEvent->uid();
    if (mEventMap.contains(kcalEvent->uid()))
        return 0;
    // Create a new event
    KAEvent* ev = new KAEvent(kcalEvent);
    addNewEvent(resource, ev);
    return ev;
}
#endif

/******************************************************************************
* Internal method to add an already checked event to the calendar.
* mEventMap takes ownership of the KAEvent.
* If 'replace' is true, an existing event is being updated (NOTE: its category()
* must remain the same).
*/
#ifdef USE_AKONADI
void AlarmCalendar::addNewEvent(const Collection& collection, KAEvent* event, bool replace)
#else
void AlarmCalendar::addNewEvent(AlarmResource* resource, KAEvent* event)
#endif
{
#ifdef USE_AKONADI
    Collection::Id key = collection.isValid() ? collection.id() : -1;
    event->setCollectionId(key);
    if (!replace)
    {
        mResourceMap[key] += event;
        mEventMap[EventId(key, event->id())] = event;
    }
    if (collection.isValid()  &&  (AkonadiModel::types(collection) & CalEvent::ACTIVE)
    &&  event->category() == CalEvent::ACTIVE)
#else
    AlarmResource* key = resource;
    mResourceMap[key] += event;
    mEventMap[event->id()] = event;
    if (resource  &&  resource->alarmType() == CalEvent::ACTIVE
    &&  event->category() == CalEvent::ACTIVE)
#endif
    {
        // Update the earliest alarm to trigger
        KAEvent* earliest = mEarliestAlarm.value(key, (KAEvent*)0);
#ifdef USE_AKONADI
        if (replace  &&  earliest == event)
            findEarliestAlarm(key);
        else
#endif
        {
            KDateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
            if (dt.isValid()
            &&  (!earliest  ||  dt < earliest->nextTrigger(KAEvent::ALL_TRIGGER)))
            {
                mEarliestAlarm[key] = event;
                emit earliestAlarmChanged();
            }
        }
    }
}

/******************************************************************************
* Modify the specified event in the calendar with its new contents.
* The new event must have a different event ID from the old one.
* It is assumed to be of the same event type as the old one (active, etc.)
* Reply = true if 'newEvent' was written to the calendar, in which case (not
*              Akonadi) ownership of 'newEvent' is taken by the calendar.
*              'newEvent' is updated.
*       = false if an error occurred, in which case 'newEvent' is unchanged.
*/
#ifdef USE_AKONADI
bool AlarmCalendar::modifyEvent(const EventId& oldEventId, KAEvent& newEvent)
#else
bool AlarmCalendar::modifyEvent(const QString& oldEventId, KAEvent* newEvent)
#endif
{
#ifdef USE_AKONADI
    EventId newId(oldEventId.collectionId(), newEvent.id());
#else
    QString newId = newEvent->id();
#endif
    kDebug() << oldEventId << "->" << newId;
    bool noNewId = newId.isEmpty();
    if (!noNewId  &&  oldEventId == newId)
    {
        kError() << "Same IDs";
        return false;
    }
    if (!mOpen)
        return false;
    if (mCalType == RESOURCES)
    {
#ifdef USE_AKONADI
        // Set the event's ID and Akonadi ID, and update the old
        // event in Akonadi.
        KAEvent* storedEvent = event(oldEventId);
        if (!storedEvent)
        {
            kError() << "Old event not found";
            return false;
        }
        if (noNewId)
            newEvent.setEventId(CalFormat::createUniqueId());
        Collection c = AkonadiModel::instance()->collectionById(oldEventId.collectionId());
        if (!c.isValid())
            return false;
        // Don't add new event to mEventMap yet - its Akonadi item id is not yet known
        if (!AkonadiModel::instance()->addEvent(newEvent, c))
            return false;
        // Note: deleteEventInternal() will delete storedEvent before using the
        // event parameter, so need to pass a copy as the parameter.
        deleteEventInternal(KAEvent(*storedEvent), c);
        if (mHaveDisabledAlarms)
            checkForDisabledAlarms();
#else
        // Create a new KCal::Event, keeping any custom properties from the old event.
        // Ensure it has a new ID.
        Event* kcalEvent = createKCalEvent(newEvent, oldEventId);
        if (noNewId)
            kcalEvent->setUid(CalFormat::createUniqueId());
        AlarmResources* resources = AlarmResources::instance();
        AlarmResource* resource = resources->resourceForIncidence(oldEventId);
        if (!resources->addEvent(kcalEvent, resource))
            return false;    // kcalEvent has been deleted by AlarmResources::addEvent()
        if (noNewId)
            newEvent->setEventId(kcalEvent->uid());
        addEvent(resource, newEvent);
        deleteEvent(oldEventId);   // this calls checkForDisabledAlarms()
#endif
    }
    else
    {
#ifdef USE_AKONADI
        // This functionality isn't needed for the display calendar.
        // The calendar would take ownership of newEvent.
        return false;
#else
        if (!addEvent(newEvent, 0, true))
            return false;
        deleteEvent(oldEventId);   // this calls checkForDisabledAlarms()
#endif
    }
    return true;
}

/******************************************************************************
* Update the specified event in the calendar with its new contents.
* The event retains the same ID. The event must be in the resource calendar.
* Reply = event which has been updated
*       = 0 if error.
*/
KAEvent* AlarmCalendar::updateEvent(const KAEvent& evnt)
{
    return updateEvent(&evnt);
}
KAEvent* AlarmCalendar::updateEvent(const KAEvent* evnt)
{
    if (!mOpen  ||  mCalType != RESOURCES)
        return 0;
#ifdef USE_AKONADI
    KAEvent* kaevnt = event(EventId(*evnt));
    if (kaevnt)
    {
        KAEvent newEvnt(*evnt);
        newEvnt.setItemId(evnt->itemId());
        if (AkonadiModel::instance()->updateEvent(newEvnt))
        {
            *kaevnt = newEvnt;
            return kaevnt;
        }
    }
#else
    QString id = evnt->id();
    KAEvent* kaevnt = event(id);
    Event* kcalEvent = mCalendar ? mCalendar->event(id) : 0;
    if (kaevnt  &&  kcalEvent)
    {
        evnt->updateKCalEvent(kcalEvent, KAEvent::UID_CHECK);
        bool oldEnabled = kaevnt->enabled();
        if (kaevnt != evnt)
            *kaevnt = *evnt;   // update the event instance in our lists, keeping the same pointer
        findEarliestAlarm(AlarmResources::instance()->resource(kcalEvent));
        if (mCalType == RESOURCES  &&  evnt->category() == CalEvent::ACTIVE)
            checkForDisabledAlarms(oldEnabled, evnt->enabled());
        return kaevnt;
    }
#endif
    kDebug() << "error";
    return 0;
}


#ifdef USE_AKONADI
/******************************************************************************
* Delete the specified event from the resource calendar, if it exists.
* The calendar is then optionally saved.
*/
bool AlarmCalendar::deleteEvent(const KAEvent& event, bool saveit)
{
    if (mOpen  &&  mCalType == RESOURCES)
    {
        CalEvent::Type status = deleteEventInternal(event);
        if (mHaveDisabledAlarms)
            checkForDisabledAlarms();
        if (status != CalEvent::EMPTY)
        {
            if (saveit)
                return save();
            return true;
        }
    }
    return false;
}
#endif

/******************************************************************************
* Delete the specified event from the calendar, if it exists.
* The calendar is then optionally saved.
*/
#ifdef USE_AKONADI
bool AlarmCalendar::deleteDisplayEvent(const QString& eventID, bool saveit)
#else
bool AlarmCalendar::deleteEvent(const QString& eventID, bool saveit)
#endif
{
#ifdef USE_AKONADI
    if (mOpen  &&  mCalType != RESOURCES)
#else
    if (mOpen)
#endif
    {
        CalEvent::Type status = deleteEventInternal(eventID);
        if (mHaveDisabledAlarms)
            checkForDisabledAlarms();
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
* Reply = event status, if it was found in the resource calendar/collection or
*         local calendar
*       = CalEvent::EMPTY otherwise.
*/
#ifdef USE_AKONADI
CalEvent::Type AlarmCalendar::deleteEventInternal(const KAEvent& event, bool deleteFromAkonadi)
{
    Collection collection = AkonadiModel::instance()->collectionById(event.collectionId());
    if (!collection.isValid())
        return CalEvent::EMPTY;
    return deleteEventInternal(event.id(), event, collection, deleteFromAkonadi);
}

CalEvent::Type AlarmCalendar::deleteEventInternal(const KAEvent& event, const Collection& collection, bool deleteFromAkonadi)
{
    if (!collection.isValid())
        return CalEvent::EMPTY;
    if (event.collectionId() != collection.id())
    {
        kError() << "Event" << event.id() << ": collection" << event.collectionId() << "differs from 'collection'" << collection.id();
        return CalEvent::EMPTY;
    }
    return deleteEventInternal(event.id(), event, collection, deleteFromAkonadi);
}

CalEvent::Type AlarmCalendar::deleteEventInternal(const QString& eventID, const KAEvent& event, const Collection& collection, bool deleteFromAkonadi)
#else
CalEvent::Type AlarmCalendar::deleteEventInternal(const QString& eventID)
#endif
{
    // Make a copy of the KAEvent and the ID QString, since the supplied
    // references might be destructed when the event is deleted below.
    const QString id = eventID;
#ifdef USE_AKONADI
    const KAEvent paramEvent = event;

    Event::Ptr kcalEvent;
    if (mCalendarStorage)
        kcalEvent = mCalendarStorage->calendar()->event(id);
    Collection::Id key = collection.isValid() ? collection.id() : -1;
    KAEventMap::Iterator it = mEventMap.find(EventId(key, id));
#else
    Event* kcalEvent = mCalendar ? mCalendar->event(id) : 0;
    KAEventMap::Iterator it = mEventMap.find(id);
#endif
    if (it != mEventMap.end())
    {
        KAEvent* ev = it.value();
        mEventMap.erase(it);
#ifndef USE_AKONADI
        AlarmResource* key = AlarmResources::instance()->resource(kcalEvent);
#endif
        KAEvent::List& events = mResourceMap[key];
        int i = events.indexOf(ev);
        if (i >= 0)
            events.remove(i);
        delete ev;
        if (mEarliestAlarm[key] == ev)
#ifdef USE_AKONADI
            findEarliestAlarm(collection);
#else
            findEarliestAlarm(key);
#endif
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
    if (kcalEvent)
    {
        status = CalEvent::status(kcalEvent);
#ifdef USE_AKONADI
        mCalendarStorage->calendar()->deleteEvent(kcalEvent);
#else
        mCalendar->deleteEvent(kcalEvent);
#endif
    }
#ifdef USE_AKONADI
    else if (deleteFromAkonadi)
    {
        // It's an Akonadi event
        CalEvent::Type s = paramEvent.category();
        if (AkonadiModel::instance()->deleteEvent(paramEvent))
            status = s;
    }
#else

    // Delete any command execution error flags for the alarm.
    KConfigGroup config(KGlobal::config(), KAEvent::commandErrorConfigGroup());
    if (config.hasKey(id))
    {
            config.deleteEntry(id);
        config.sync();
    }
#endif
    return status;
}

#ifndef USE_AKONADI
/******************************************************************************
* Return a new KCal::Event representing the specified KAEvent.
* If the event exists in the calendar, custom properties are copied from there.
* The caller takes ownership of the returned KCal::Event. Note that the ID of
* the returned KCal::Event may be the same as an existing calendar event, so
* be careful not to end up duplicating IDs.
* If it's an archived alarm, the event start date/time is adjusted to its
* original value instead of its next occurrence, and the expired main alarm is
* reinstated.
*/
Event* AlarmCalendar::createKCalEvent(const KAEvent* ev, const QString& baseID) const
{
    if (mCalType != RESOURCES)
        kFatal() << "AlarmCalendar::createKCalEvent(KAEvent): invalid for display calendar";
    // If the event exists in the calendar, we want to keep any custom
    // properties. So copy the calendar KCal::Event to base the new one on.
    QString id = baseID.isEmpty() ? ev->id() : baseID;
    Event* calEvent = id.isEmpty() ? 0 : AlarmResources::instance()->event(id);
    Event* newEvent = calEvent ? new Event(*calEvent) : new Event;
    ev->updateKCalEvent(newEvent, KAEvent::UID_SET);
    return newEvent;
}
#endif

/******************************************************************************
* Return the event with the specified ID.
* If 'checkDuplicates' is true, and the collection ID is invalid, if there is
* a unique event with the given ID, it will be returned.
*/
#ifdef USE_AKONADI
KAEvent* AlarmCalendar::event(const EventId& uniqueID, bool checkDuplicates)
#else
KAEvent* AlarmCalendar::event(const QString& uniqueID)
#endif
{
    if (!isValid())
        return 0;
#ifdef USE_AKONADI
    const QString eventId = uniqueID.eventId();
    if (uniqueID.collectionId() == -1  &&  checkDuplicates)
    {
        // The collection isn't known, but use the event ID if it is
        // unique among all collections.
        KAEvent::List list = events(eventId);
        if (list.count() > 1)
        {
            kWarning() << "Multiple events found with ID" << eventId;
            return 0;
        }
        if (list.isEmpty())
            return 0;
        return list[0];
    }
#endif
    KAEventMap::ConstIterator it = mEventMap.constFind(uniqueID);
    if (it == mEventMap.constEnd())
        return 0;
    return it.value();
}

/******************************************************************************
* Return the event with the specified ID.
* For the Akonadi version, this method is for the display calendar only.
*/
#ifdef USE_AKONADI
Event::Ptr AlarmCalendar::kcalEvent(const QString& uniqueID)
{
    Q_ASSERT(mCalType != RESOURCES);   // only allowed for display calendar
    if (!mCalendarStorage)
        return Event::Ptr();
    return mCalendarStorage->calendar()->event(uniqueID);
}
#else
Event* AlarmCalendar::kcalEvent(const QString& uniqueID)
{
    return mCalendar ? mCalendar->event(uniqueID) : 0;
}
#endif

/******************************************************************************
* Find the alarm template with the specified name.
* Reply = 0 if not found.
*/
KAEvent* AlarmCalendar::templateEvent(const QString& templateName)
{
    if (templateName.isEmpty())
        return 0;
    KAEvent::List eventlist = events(CalEvent::TEMPLATE);
    for (int i = 0, end = eventlist.count();  i < end;  ++i)
    {
        if (eventlist[i]->templateName() == templateName)
            return eventlist[i];
    }
    return 0;
}

#ifdef USE_AKONADI
/******************************************************************************
* Return all events with the specified ID, from all calendars.
*/
KAEvent::List AlarmCalendar::events(const QString& uniqueId) const
{
    KAEvent::List list;
    if (mCalType == RESOURCES  &&  isValid())
    {
        for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
        {
            const Collection::Id id = rit.key();
            KAEventMap::ConstIterator it = mEventMap.constFind(EventId(id, uniqueId));
            if (it != mEventMap.constEnd())
                list += it.value();
        }
    }
    return list;
}
#endif

/******************************************************************************
* Return all events in the calendar which contain alarms.
* Optionally the event type can be filtered, using an OR of event types.
*/
#ifdef USE_AKONADI
KAEvent::List AlarmCalendar::events(const Collection& collection, CalEvent::Types type) const
#else
KAEvent::List AlarmCalendar::events(AlarmResource* resource, CalEvent::Types type) const
#endif
{
    KAEvent::List list;
#ifdef USE_AKONADI
    if (mCalType != RESOURCES  &&  (!mCalendarStorage || collection.isValid()))
        return list;
    if (collection.isValid())
#else
    if (!mCalendar  ||  (resource && mCalType != RESOURCES))
        return list;
    if (resource)
#endif
    {
#ifdef USE_AKONADI
        Collection::Id key = collection.isValid() ? collection.id() : -1;
        ResourceMap::ConstIterator rit = mResourceMap.constFind(key);
#else
        ResourceMap::ConstIterator rit = mResourceMap.constFind(resource);
#endif
        if (rit == mResourceMap.constEnd())
            return list;
        const KAEvent::List events = rit.value();
        if (type == CalEvent::EMPTY)
            return events;
        for (int i = 0, end = events.count();  i < end;  ++i)
            if (type & events[i]->category())
                list += events[i];
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
                for (int i = 0, end = events.count();  i < end;  ++i)
                    if (type & events[i]->category())
                        list += events[i];
            }
        }
    }
    return list;
}

/******************************************************************************
* Return all events in the calendar which contain usable alarms.
* For the Akonadi version, this method is for the display calendar only.
* Optionally the event type can be filtered, using an OR of event types.
*/
#ifdef USE_AKONADI
Event::List AlarmCalendar::kcalEvents(CalEvent::Type type)
#else
Event::List AlarmCalendar::kcalEvents(AlarmResource* resource, CalEvent::Type type)
#endif
{
    Event::List list;
#ifdef USE_AKONADI
    Q_ASSERT(mCalType != RESOURCES);   // only allowed for display calendar
    if (!mCalendarStorage)
        return list;
    list = mCalendarStorage->calendar()->rawEvents();
#else
    if (!mCalendar  ||  (resource && mCalType != RESOURCES))
        return list;
    list = resource ? AlarmResources::instance()->rawEvents(resource) : mCalendar->rawEvents();
#endif
    for (int i = 0;  i < list.count();  )
    {
#ifdef USE_AKONADI
        Event::Ptr event = list[i];
#else
        Event* event = list[i];
#endif
        if (event->alarms().isEmpty()
        ||  (type != CalEvent::EMPTY  &&  !(type & CalEvent::status(event)))
        ||  !KAEvent(event).isValid())
#ifdef USE_AKONADI
            list.remove(i);
#else
            list.removeAt(i);
#endif
        else
            ++i;
    }
    return list;
}

#ifndef USE_AKONADI
/******************************************************************************
* Return all events which have alarms falling within the specified time range.
* 'type' is the OR'ed desired event types.
*/
KAEvent::List AlarmCalendar::events(const KDateTime& from, const KDateTime& to, CalEvent::Types type)
{
    kDebug() << from << "-" << to;
    KAEvent::List evnts;
    if (!isValid())
        return evnts;
    KDateTime dt;
    AlarmResources* resources = AlarmResources::instance();
    KAEvent::List allEvents = events(type);
    for (int i = 0, end = allEvents.count();  i < end;  ++i)
    {
        KAEvent* event = allEvents[i];
        Event* e = resources->event(event->id());
        bool recurs = e->recurs();
        int  endOffset = 0;
        bool endOffsetValid = false;
        const Alarm::List& alarms = e->alarms();
        for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
        {
            Alarm* alarm = alarms[ai];
            if (alarm->enabled())
            {
                if (recurs)
                {
                    if (alarm->hasTime())
                        dt = alarm->time();
                    else
                    {
                        // The alarm time is defined by an offset from the event start or end time.
                        // Find the offset from the event start time, which is also used as the
                        // offset from the recurrence time.
                        int offset = 0;
                        if (alarm->hasStartOffset())
                            offset = alarm->startOffset().asSeconds();
                        else if (alarm->hasEndOffset())
                        {
                            if (!endOffsetValid)
                            {
                                endOffset = e->hasDuration() ? e->duration().asSeconds() : e->hasEndDate() ? e->dtStart().secsTo(e->dtEnd()) : 0;
                                endOffsetValid = true;
                            }
                            offset = alarm->endOffset().asSeconds() + endOffset;
                        }
                        // Adjust the 'from' date/time and find the next recurrence at or after it
                        KDateTime pre = from.addSecs(-offset - 1);
                        if (e->allDay()  &&  pre.time() < Preferences::startOfDay())
                            pre = pre.addDays(-1);    // today's recurrence (if today recurs) is still to come
                        dt = e->recurrence()->getNextDateTime(pre);
                        if (!dt.isValid())
                            continue;
                        dt = dt.addSecs(offset);
                    }
                }
                else
                    dt = alarm->time();
                if (dt >= from  &&  dt <= to)
                {
                    kDebug() << "'" << e->summary() << "':" << dt;
                    evnts.append(event);
                    break;
                }
            }
        }
    }
    return evnts;
}
#endif

/******************************************************************************
* Return whether an event is read-only.
* Display calendar events are always returned as read-only.
*/
#ifdef USE_AKONADI
bool AlarmCalendar::eventReadOnly(Item::Id id) const
{
    if (mCalType != RESOURCES)
        return true;
    AkonadiModel* model = AkonadiModel::instance();
    Collection collection = model->collectionForItem(id);
    KAEvent event = model->event(id);
    if (!CollectionControlModel::isWritableEnabled(collection, event.category()))
        return true;
    return !event.isValid()  ||  event.isReadOnly();
    //   ||  compatibility(event) != KACalendar::Current;
}
#else
bool AlarmCalendar::eventReadOnly(const QString& uniqueID) const
{
    if (!mCalendar  ||  mCalType != RESOURCES)
        return true;
    AlarmResources* resources = AlarmResources::instance();
    const Event* event = resources->event(uniqueID);
    AlarmResource* resource = resources->resource(event);
    if (!resource)
        return true;
    return !resource->writable(event);
}
#endif

#ifdef USE_AKONADI
/******************************************************************************
* Return the collection containing a specified event.
*/
Collection AlarmCalendar::collectionForEvent(Item::Id itemId) const
{
    if (mCalType != RESOURCES)
        return Collection();
    return AkonadiModel::instance()->collectionForItem(itemId);
}
#else
/******************************************************************************
* Return the resource containing a specified event.
*/
AlarmResource* AlarmCalendar::resourceForEvent(const QString& eventID) const
{
    if (!mCalendar  ||  mCalType != RESOURCES)
        return 0;
    return AlarmResources::instance()->resourceForIncidence(eventID);
}
#endif

/******************************************************************************
* Called when an alarm's enabled status has changed.
*/
void AlarmCalendar::disabledChanged(const KAEvent* event)
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
void AlarmCalendar::checkForDisabledAlarms(bool oldEnabled, bool newEnabled)
{
    if (mCalType == RESOURCES  &&  newEnabled != oldEnabled)
    {
        if (newEnabled  &&  mHaveDisabledAlarms)
            checkForDisabledAlarms();
        else if (!newEnabled  &&  !mHaveDisabledAlarms)
        {
            mHaveDisabledAlarms = true;
            emit haveDisabledAlarmsChanged(true);
        }
    }
}

/******************************************************************************
* Check whether there are any individual disabled alarms.
*/
void AlarmCalendar::checkForDisabledAlarms()
{
    if (mCalType != RESOURCES)
        return;
    bool disabled = false;
    KAEvent::List eventlist = events(CalEvent::ACTIVE);
    for (int i = 0, end = eventlist.count();  i < end;  ++i)
    {
        if (!eventlist[i]->enabled())
        {
            disabled = true;
            break;
        }
    }
    if (disabled != mHaveDisabledAlarms)
    {
        mHaveDisabledAlarms = disabled;
        emit haveDisabledAlarmsChanged(disabled);
    }
}

/******************************************************************************
* Return a list of all active at-login alarms.
*/
KAEvent::List AlarmCalendar::atLoginAlarms() const
{
    KAEvent::List atlogins;
#ifdef USE_AKONADI
    if (mCalType != RESOURCES)
        return atlogins;
    AkonadiModel* model = AkonadiModel::instance();
    if (!mCalendarStorage  ||  mCalType != RESOURCES)
        return atlogins;
#else
    if (!mCalendar  ||  mCalType != RESOURCES)
        return atlogins;
#endif
    for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
    {
#ifdef USE_AKONADI
        const Collection::Id id = rit.key();
        if (id < 0
        ||  !(AkonadiModel::types(model->collectionById(id)) & CalEvent::ACTIVE))
            continue;
#else
        const AlarmResource* resource = rit.key();
        if (!resource  ||  resource->alarmType() != CalEvent::ACTIVE)
            continue;
#endif
        const KAEvent::List& events = rit.value();
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            KAEvent* event = events[i];
            if (event->category() == CalEvent::ACTIVE  &&  event->repeatAtLogin())
                atlogins += event;
        }
    }
    return atlogins;
}

/******************************************************************************
* Find and note the active alarm with the earliest trigger time for a calendar.
*/
#ifdef USE_AKONADI
void AlarmCalendar::findEarliestAlarm(const Collection& collection)
{
    if (mCalType != RESOURCES)
        return;
    if (!collection.isValid()
    ||  !(AkonadiModel::types(collection) & CalEvent::ACTIVE))
        return;
    findEarliestAlarm(collection.id());
}

void AlarmCalendar::findEarliestAlarm(Collection::Id key)
#else
void AlarmCalendar::findEarliestAlarm(AlarmResource* key)
#endif
{
    EarliestMap::Iterator eit = mEarliestAlarm.find(key);
    if (eit != mEarliestAlarm.end())
        eit.value() = 0;
#ifdef USE_AKONADI
    if (mCalType != RESOURCES
    ||  key < 0)
        return;
#else
    if (!mCalendar  ||  mCalType != RESOURCES
    ||  !key  ||  key->alarmType() != CalEvent::ACTIVE)
        return;
#endif
    ResourceMap::ConstIterator rit = mResourceMap.constFind(key);
    if (rit == mResourceMap.constEnd())
        return;
    const KAEvent::List& events = rit.value();
    KAEvent* earliest = 0;
    KDateTime earliestTime;
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
        KAEvent* event = events[i];
        if (event->category() != CalEvent::ACTIVE
        ||  mPendingAlarms.contains(event->id()))
            continue;
        KDateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
        if (dt.isValid()  &&  (!earliest || dt < earliestTime))
        {
            earliestTime = dt;
            earliest = event;
        }
    }
    mEarliestAlarm[key] = earliest;
    emit earliestAlarmChanged();
}

/******************************************************************************
* Return the active alarm with the earliest trigger time.
* Reply = 0 if none.
*/
KAEvent* AlarmCalendar::earliestAlarm() const
{
    KAEvent* earliest = 0;
    KDateTime earliestTime;
    for (EarliestMap::ConstIterator eit = mEarliestAlarm.constBegin();  eit != mEarliestAlarm.constEnd();  ++eit)
    {
        KAEvent* event = eit.value();
        if (!event)
            continue;
        KDateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
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
void AlarmCalendar::setAlarmPending(KAEvent* event, bool pending)
{
    QString id = event->id();
    bool wasPending = mPendingAlarms.contains(id);
    kDebug() << id << "," << pending << "(was" << wasPending << ")";
    if (pending)
    {
        if (wasPending)
            return;
        mPendingAlarms.append(id);
    }
    else
    {
        if (!wasPending)
            return;
        mPendingAlarms.removeAll(id);
    }
    // Now update the earliest alarm to trigger for its calendar
#ifdef USE_AKONADI
    findEarliestAlarm(AkonadiModel::instance()->collection(*event));
#else
    findEarliestAlarm(AlarmResources::instance()->resourceForIncidence(id));
#endif
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

#ifdef USE_AKONADI
/******************************************************************************
* Find the version of KAlarm which wrote the calendar file, and do any
* necessary conversions to the current format.
*/
KACalendar::Compat fix(const FileStorage::Ptr& fileStorage)
{
    QString versionString;
    int version = KACalendar::updateVersion(fileStorage, versionString);
    if (version == KACalendar::IncompatibleFormat)
        return KACalendar::Incompatible;  // calendar was created by another program, or an unknown version of KAlarm
    return KACalendar::Current;
}
#endif
#include "moc_alarmcalendar.cpp"

// vim: et sw=4:
