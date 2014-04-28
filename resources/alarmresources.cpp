/*
 *  alarmresources.cpp  -  alarm calendar resources
 *  Program:  kalarm
 *  Copyright © 2006-2011 by David Jarvie <djarvie@kde.org>
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

#include "alarmresource.h"
#include "resourcelocal.h"
#include "resourceremote.h"
#include "alarmresources.moc"

#include <KAlarmCal/kacalendar.h>

#include <kresources/selectdialog.h>
#include <kcal/calformat.h>
#include <kcal/event.h>

#include <kconfiggroup.h>
#include <kstandarddirs.h>
#include <klocale.h>
#include <kapplication.h>
#include <kurl.h>
#include <kdebug.h>

using namespace KCal;
using namespace KAlarmCal;

AlarmResources* AlarmResources::mInstance = 0;
QString         AlarmResources::mReservedFile;
QString         AlarmResources::mConstructionError;

AlarmResources* AlarmResources::create(const KDateTime::Spec& timeSpec, bool activeOnly, bool passiveClient)
{
    if (!mInstance)
    {
        AlarmResources* cal = new AlarmResources(timeSpec, activeOnly, passiveClient);
        if (!mConstructionError.isEmpty())
            delete cal;
        else
            mInstance = cal;
    }
    return mInstance;
}

AlarmResources::AlarmResources(const KDateTime::Spec& timeSpec, bool activeOnly, bool passiveClient)
    : Calendar(timeSpec),
      mActiveOnly(activeOnly),
      mPassiveClient(passiveClient),
      mNoGui(false),
      mAskDestination(false),
      mShowProgress(false),
      mOpen(false),
      mClosing(false)
{
    mManager = new AlarmResourceManager(QString::fromLatin1("alarms"));
    mManager->addObserver(this);
    mAskDestination = true;    // prompt the user for a resource every time an alarm is saved

    mManager->readConfig(0);
    for (AlarmResourceManager::Iterator it = mManager->begin();  it != mManager->end();  ++it)
    {
        if (!mActiveOnly  ||  (*it)->alarmType() == CalEvent::ACTIVE)
            connectResource(*it);
    }

    if (!mPassiveClient  &&  mManager->isEmpty())
    {
        KConfigGroup config(KGlobal::config(), "General");
        AlarmResource* resource;
        resource = addDefaultResource(config, CalEvent::ACTIVE);
        setStandardResource(resource);
        if (!mActiveOnly)
        {
            resource = addDefaultResource(config, CalEvent::ARCHIVED);
            setStandardResource(resource);
            resource = addDefaultResource(config, CalEvent::TEMPLATE);
            setStandardResource(resource);
        }

#ifndef NDEBUG
        kDebug(KARES_DEBUG) << "AlarmResources used:";
        for (AlarmResourceManager::Iterator it = mManager->begin();  it != mManager->end();  ++it)
            kDebug(KARES_DEBUG) << (*it)->resourceName();
#endif
    }
}

AlarmResources::~AlarmResources()
{
    kDebug(KARES_DEBUG);
    close();
    delete mManager;
    mManager = 0;
    mInstance = 0;
}

void AlarmResources::setNoGui(bool noGui)
{
    mNoGui = noGui;
    if (mNoGui)
        mShowProgress = false;
    AlarmResource::setNoGui(mNoGui);
}

AlarmResource* AlarmResources::addDefaultResource(CalEvent::Type type)
{
    KConfigGroup config(KGlobal::config(), "General");
    return addDefaultResource(config, type);
}

AlarmResource* AlarmResources::addDefaultResource(const KConfigGroup& config, CalEvent::Type type)
{
    QString configKey, defaultFile, title;
    switch (type)
    {
        case CalEvent::ACTIVE:
            configKey   = QString::fromLatin1("Calendar");
            defaultFile = QString::fromLatin1("calendar.ics");
            title       = i18nc("@info/plain", "Active Alarms");
            break;
        case CalEvent::TEMPLATE:
            configKey   = QString::fromLatin1("TemplateCalendar");
            defaultFile = QString::fromLatin1("template.ics");
            title       = i18nc("@info/plain", "Alarm Templates");
            break;
        case CalEvent::ARCHIVED:
            configKey   = QString::fromLatin1("ExpiredCalendar");
            defaultFile = QString::fromLatin1("expired.ics");
            title       = i18nc("@info/plain", "Archived Alarms");
            break;
        default:
            return 0;
    }
    AlarmResource* resource = 0;
    QString fileName = config.readPathEntry(configKey, QString());
    if (!fileName.isEmpty())
    {
        // Calendar is specified in KAlarm config file
        KUrl url(fileName);
        if (!url.isValid())
        {
            kError(KARES_DEBUG) << configKey << ": invalid name:" << fileName;
            mConstructionError = i18nc("@info", "%1: invalid calendar file name: <filename>%2</filename>", configKey, fileName);
            return 0;
        }
        if (!url.isLocalFile())
            resource = new KAResourceRemote(type, url);
        else if (fileName == mReservedFile)
        {
            kError(KARES_DEBUG) << configKey << ": name not allowed:" << fileName;
            mConstructionError = i18nc("@info", "%1: file name not permitted: <filename>%2</filename>", configKey, fileName);
            return 0;
        }
        else
            resource = new KAResourceLocal(type, url.toLocalFile());
    }
    if (!resource)
    {
        // No calendar is specified in config file, or the calendar
        // specified is invalid - use default file
        fileName = KStandardDirs::locateLocal("appdata", defaultFile);
        resource = new KAResourceLocal(type, fileName);
    }

    resource->setTimeSpec(timeSpec());
    resource->setResourceName(title);
    resourceManager()->add(resource);
    connectResource(resource);
    return resource;
}

AlarmResources::Result AlarmResources::addEvent(Event* event, CalEvent::Type type, QWidget* promptParent, bool noPrompt)
{
    kDebug(KARES_DEBUG) << event->uid();
    bool cancelled;
    AlarmResource* resource = destination(type, promptParent, noPrompt, &cancelled);
    if (!resource)
    {
        delete event;
        if (cancelled)
            return Cancelled;
        kDebug(KARES_DEBUG) << "No resource";
        return Failed;
    }
    if (!addEvent(event, resource))
    {
        kDebug(KARES_DEBUG) << "Failed";
        return Failed;    // event was deleted by addEvent()
    }
    return Success;
}

AlarmResource* AlarmResources::getStandardResource(CalEvent::Type type)
{
    switch (type)
    {
        case CalEvent::ACTIVE:
        {
            AlarmResource* std = mManager->standardResource();
            if (std  &&  std->standardResource()  &&  std->alarmType() == CalEvent::ACTIVE  &&  !std->readOnly())
                return std;
            break;
        }
        case CalEvent::ARCHIVED:
        case CalEvent::TEMPLATE:
            if (mActiveOnly)
                return 0;
            for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
            {
                AlarmResource* r = *it;
                if (r->alarmType() == type  &&  r->standardResource())
                    return r;
            }
            break;
        default:
            return 0;
    }

    // There's no nominated default alarm resource of the right type.
    // If there's only one read/write alarm resource, use it.
    AlarmResource* std = 0;
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
    {
        AlarmResource* r = *it;
        if (!r->readOnly()  &&  r->alarmType() == type)
        {
            if (std)
                return 0;   // there's more than one candidate
            std = r;
        }
    }
    if (std  &&  type == CalEvent::ACTIVE  &&  !mPassiveClient)
        setStandardResource(std);   // mark it as the standard resource
    return std;
}

void AlarmResources::setStandardResource(AlarmResource* resource)
{
    if (resource->standardResource())
        return;    // it's already the standard resource for its alarm type
    CalEvent::Type type = resource->alarmType();
    bool active = (type == CalEvent::ACTIVE);
    for (AlarmResourceManager::Iterator it = mManager->begin();  it != mManager->end();  ++it)
    {
        AlarmResource* r = *it;
        if (r->alarmType() == type  &&  r->standardResource())
        {
            r->setStandardResource(false);
            if (!active  &&  !mPassiveClient)
                mManager->change(r);   // save resource's new configuration
        }
    }
    resource->setStandardResource(true);
    if (active)
    {
        mManager->setStandardResource(resource);
        if (!mPassiveClient)
            mManager->writeConfig();
    }
    else if (!mPassiveClient)
        mManager->change(resource);   // save resource's new configuration
    emit standardResourceChange(type);
}

void AlarmResources::writeConfig()
{
    if (!mPassiveClient)
        mManager->writeConfig();
}

int AlarmResources::activeCount(CalEvent::Type type, bool writable)
{
    int count = 0;
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
    {
        AlarmResource* resource = *it;
        if (resource->alarmType() == type
        &&  (!writable || !resource->readOnly()))
            ++count;
    }
    return count;
}

AlarmResource* AlarmResources::destination(Incidence* incidence, QWidget* promptParent, bool* cancelled)
{
    Event* event = dynamic_cast<Event*>(incidence);
    CalEvent::Type type = event ? CalEvent::status(event) : CalEvent::ACTIVE;
    return destination(type, promptParent, false, cancelled);
}

AlarmResource* AlarmResources::destination(CalEvent::Type type, QWidget* promptParent, bool noPrompt, bool* cancelled)
{
    if (cancelled)
        *cancelled = false;
    AlarmResource* standard;
    switch (type)
    {
        case CalEvent::ACTIVE:
            break;
        case CalEvent::TEMPLATE:
            if (mActiveOnly)
                return 0;
            break;
        case CalEvent::ARCHIVED:
            if (mActiveOnly)
                return 0;
            // Archived alarms are always saved in the default resource
            return getStandardResource(CalEvent::ARCHIVED);
        default:
            return 0;
    }
    standard = getStandardResource(type);
    if (noPrompt  ||  (!mAskDestination  &&  standard))
        return standard;
    QList<KRES::Resource*> list;
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
    {
        AlarmResource* resource = *it;
        if (!resource->readOnly()  &&  resource->alarmType() == type)
        {
            // Insert the standard resource at the beginning so as to be the default
            if (resource == standard)
                list.insert(0, resource);
            else
                list.append(resource);
        }
    }
    switch (list.count())
    {
        case 0:
            return 0;
        case 1:
//            return static_cast<AlarmResource*>(list.first());
        default:
        {
            KRES::Resource* r = KRES::SelectDialog::getResource(list, promptParent);
            if (!r  &&  cancelled)
                *cancelled = true;
            return static_cast<AlarmResource*>(r);
        }
    }
}

int AlarmResources::loadedState(CalEvent::Type type) const
{
    if (!mOpen)
        return 0;
    bool loaded = false;
    bool notloaded = false;
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
    {
        AlarmResource* res = *it;
        if (res->alarmType() == type)
        {
            if (res->isLoaded())
            {
                if (notloaded)
                    return 1;    // some loaded, some not loaded
                loaded = true;
            }
            else
            {
                if (loaded)
                    return 1;    // some loaded, some not loaded
                notloaded = true;
            }
        }
    }
    return !loaded ? 0 : notloaded ? 1 : 2;
}

bool AlarmResources::isLoading(CalEvent::Type type) const
{
    if (mOpen)
    {
        for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
        {
            AlarmResource* res = *it;
            if (res->alarmType() == type  &&  res->isLoading())
                return true;
        }
    }
    return false;
}

void AlarmResources::load(ResourceCached::CacheAction action)
{
    kDebug(KARES_DEBUG);
    if (!mManager->standardResource())
        kDebug(KARES_DEBUG) << "Warning! No standard resource yet.";

    // Set the timezone for all resources. Otherwise we'll have those terrible tz troubles ;-((
    // Open all active resources
    QList<AlarmResource*> failed;
    for (AlarmResourceManager::Iterator it = mManager->begin();  it != mManager->end();  ++it)
    {
        AlarmResource* resource = *it;
        if (!mActiveOnly  ||  resource->alarmType() == CalEvent::ACTIVE)
        {
            resource->setTimeSpec(timeSpec());
            if (resource->isActive())
            {
                if (!load(resource, action))
                    failed.append(resource);
            }
        }
    }
    for (int i = 0, end = failed.count();  i < end;  ++i)
    {
        failed[i]->setActive(false);
        emit signalResourceModified(failed[i]);
    }

    // Ensure that if there is only one active alarm resource,
    // it is marked as the standard resource.
    getStandardResource(CalEvent::ACTIVE);

    mOpen = true;
}

bool AlarmResources::load(AlarmResource* resource, ResourceCached::CacheAction action)
{
    switch (action)
    {
        case ResourceCached::SyncCache:
        case ResourceCached::NoSyncCache:
            break;
        default:
            action = ResourceCached::SyncCache;
            break;
    }
    return resource->load(action);
}

// Called whenever a remote resource download has completed.
void AlarmResources::slotCacheDownloaded(AlarmResource* resource)
{
    if (resource->isActive())
        emit cacheDownloaded(resource);
}

void AlarmResources::remap(AlarmResource* resource)
{
    for (ResourceMap::Iterator it = mResourceMap.begin();  it != mResourceMap.end();  )
    {
        if (it.value() == resource)
            it = mResourceMap.erase(it);
        else
            ++it;
    }
    Event::List events = resource->rawEvents();
    for (int i = 0, end = events.count();  i < end;  ++i)
        mResourceMap[events[i]] = resource;
}

bool AlarmResources::reload()
{
    save();
    close();
    load();
    return true;
}

void AlarmResources::close()
{
    if (mOpen  &&  !mClosing)
    {
        kDebug(KARES_DEBUG);
        mClosing = true;
        for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
            (*it)->close();
        setModified(false);
        mOpen = false;
        mClosing = false;
    }
}

bool AlarmResources::save()
{
    kDebug(KARES_DEBUG);
    if (!mOpen)
        return false;
    bool saved = false;
    if (isModified())
    {
        for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
        {
            if ((!mActiveOnly  ||  (*it)->alarmType() == CalEvent::ACTIVE)
            &&  (*it)->hasChanges())
            {
                kDebug(KARES_DEBUG) << "Saving modified resource" << (*it)->identifier();
                (*it)->save();
                saved = true;
            }
        }
        setModified(false);
    }
    if (!saved)
        kDebug(KARES_DEBUG) << "No modified resources to save";
        return true;
}

bool AlarmResources::isSaving()
{
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
    {
        if ((*it)->isSaving())
            return true;
    }
    return false;
}

void AlarmResources::showProgress(bool show)
{
    if (show != mShowProgress)
    {
        mShowProgress = show;
        for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
            (*it)->showProgress(show);
    }
}

bool AlarmResources::addEvent(Event* event, AlarmResource* resource)
{
    bool validRes = false;
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
    {
        if ((*it) == resource)
            validRes = true;
    }
    AlarmResource* oldResource = mResourceMap.contains(event) ? mResourceMap[event] : 0;
    mResourceMap[event] = resource;
    if (validRes  &&  resource->addIncidence(event))
    {
        event->registerObserver(this);
        notifyIncidenceAdded(event);
        setModified(true);
        return true;
    }
    if (oldResource)
        mResourceMap[event] = oldResource;
    else
        mResourceMap.remove(event);
    delete event;
    return false;
}

AlarmResources::Result AlarmResources::addEvent(Event* event, QWidget* promptParent)
{
    kDebug(KARES_DEBUG) << this;
    bool cancelled;
    AlarmResource* resource = destination(event, promptParent, &cancelled);
    if (resource)
    {
        mResourceMap[event] = resource;
        if (resource->addIncidence(event))
        {
            event->registerObserver(this);
            notifyIncidenceAdded(event);
            mResourceMap[event] = resource;
            setModified(true);
            return Success;
        }
        mResourceMap.remove(event);
    }
    else if (cancelled)
        return Cancelled;
    else
        kDebug(KARES_DEBUG) << "No resource";
    return Failed;
}

bool AlarmResources::deleteEvent(Event *event)
{
    kDebug(KARES_DEBUG) << event->uid();
    bool status = false;
    ResourceMap::Iterator rit = mResourceMap.find(event);
    if (rit != mResourceMap.end())
    {
        status = rit.value()->deleteEvent(event);
        if (status)
            mResourceMap.erase(rit);
    }
    else
    {
        for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
            status = (*it)->deleteEvent(event) || status;
    }
    setModified(status);
    return status;
}

Event* AlarmResources::event(const QString& uid)
{
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
    {
        Event* event = (*it)->event(uid);
        if (event)
        {
            mResourceMap[event] = *it;
            return event;
        }
    }
    return 0;
}

Alarm::List AlarmResources::alarmsTo(const KDateTime &to)
{
    Alarm::List result;
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
        result += (*it)->alarmsTo(to);
    return result;
}

Alarm::List AlarmResources::alarms(const KDateTime &from, const KDateTime &to)
{
    Alarm::List result;
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
        result += (*it)->alarms(from, to);
    return result;
}

Event::List AlarmResources::rawEventsForDate(const QDate &date, const KDateTime::Spec& timespec, EventSortField sortField, SortDirection sortDirection)
{
    Event::List result;
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
        appendEvents(result, (*it)->rawEventsForDate(date, timespec), *it);
    return sortEvents(&result, sortField, sortDirection);
}

Event::List AlarmResources::rawEvents(const QDate& start, const QDate& end, const KDateTime::Spec& timespec, bool inclusive)
{
    kDebug(KARES_DEBUG) << "(start,end,inclusive)";
    Event::List result;
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
        appendEvents(result, (*it)->rawEvents(start, end, timespec, inclusive), *it);
    return result;
}

Event::List AlarmResources::rawEventsForDate(const KDateTime& dt)
{
    kDebug(KARES_DEBUG) << "(dt)";
    Event::List result;
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
        appendEvents(result, (*it)->rawEventsForDate(dt), *it);
    return result;
}

Event::List AlarmResources::rawEvents(EventSortField sortField, SortDirection sortDirection)
{
    kDebug(KARES_DEBUG);
    Event::List result;
    for (AlarmResourceManager::ActiveIterator it = mManager->activeBegin();  it != mManager->activeEnd();  ++it)
        appendEvents(result, (*it)->rawEvents(EventSortUnsorted), *it);
    return sortEvents(&result, sortField, sortDirection);
}

Event::List AlarmResources::rawEvents(AlarmResource* resource, EventSortField sortField, SortDirection sortDirection)
{
    kDebug(KARES_DEBUG) << "(resource)";
    Event::List result;
    if (!resource->isActive())
        return result;
    appendEvents(result, resource->rawEvents(EventSortUnsorted), resource);
    return sortEvents(&result, sortField, sortDirection);
}

void AlarmResources::appendEvents(Event::List& result, const Event::List& events, AlarmResource* resource)
{
    result += events;
    for (int i = 0, end = events.count();  i < end;  ++i)
        mResourceMap[events[i]] = resource;
}

// Called whenever a resource is added to those managed by the AlarmResources,
// to initialise it and connect its signals.
void AlarmResources::connectResource(AlarmResource* resource)
{
    kDebug(KARES_DEBUG) << resource->resourceName();
    resource->disconnect(this);   // just in case we're called twice
    connect(resource, SIGNAL(enabledChanged(AlarmResource*)), SLOT(slotActiveChanged(AlarmResource*)));
    connect(resource, SIGNAL(readOnlyChanged(AlarmResource*)), SLOT(slotReadOnlyChanged(AlarmResource*)));
    connect(resource, SIGNAL(wrongAlarmTypeChanged(AlarmResource*)), SLOT(slotWrongTypeChanged(AlarmResource*)));
    connect(resource, SIGNAL(locationChanged(AlarmResource*)), SLOT(slotLocationChanged(AlarmResource*)));
    connect(resource, SIGNAL(colourChanged(AlarmResource*)), SLOT(slotColourChanged(AlarmResource*)));
    connect(resource, SIGNAL(invalidate(AlarmResource*)), SLOT(slotResourceInvalidated(AlarmResource*)));
    connect(resource, SIGNAL(loaded(AlarmResource*)), SLOT(slotResourceLoaded(AlarmResource*)));
    connect(resource, SIGNAL(cacheDownloaded(AlarmResource*)), SLOT(slotCacheDownloaded(AlarmResource*)));
//    connect(resource, SIGNAL(downloading(AlarmResource*,ulong)),
//                      SLOT(slotResourceDownloading(AlarmResource*,ulong)));
    connect(resource, SIGNAL(resourceSaved(AlarmResource*)), SLOT(slotResourceSaved(AlarmResource*)));
    connect(resource, SIGNAL(resourceChanged(ResourceCalendar*)), SLOT(slotResourceChanged(ResourceCalendar*)));
    connect(resource, SIGNAL(resourceLoadError(ResourceCalendar*,QString)),
                      SLOT(slotLoadError(ResourceCalendar*,QString)));
    connect(resource, SIGNAL(resourceSaveError(ResourceCalendar*,QString)),
                      SLOT(slotSaveError(ResourceCalendar*,QString)));
}

void AlarmResources::slotResourceInvalidated(AlarmResource* resource)
{
    emit resourceStatusChanged(resource, Invalidated);
}

void AlarmResources::slotResourceLoaded(AlarmResource* resource)
{
    remap(resource);
    Incidence::List incidences = resource->rawIncidences();
    for (int i = 0, end = incidences.count();  i < end;  ++i)
    {
        incidences[i]->registerObserver(this);
        notifyIncidenceAdded(incidences[i]);
    }
    emit resourceLoaded(resource, resource->isActive());
}

void AlarmResources::slotResourceSaved(AlarmResource* resource)
{
    if (resource->isActive())
        emit resourceSaved(resource);
}

/*void AlarmResources::slotResourceDownloading(AlarmResource* resource, unsigned long percent)
{
    if (resource->isActive())
        emit downloading(resource, percent);
}*/

void AlarmResources::slotResourceChanged(ResourceCalendar* resource)
{
    if (resource->isActive())
        emit calendarChanged();
}

void AlarmResources::slotLoadError(ResourceCalendar* resource, const QString& err)
{
    if (resource->isActive())
        emit signalErrorMessage(err);
}

void AlarmResources::slotSaveError(ResourceCalendar* resource, const QString& err)
{
    if (resource->isActive())
        emit signalErrorMessage(err);
}

void AlarmResources::slotResourceStatusChanged(AlarmResource* resource, Change change)
{
    kDebug(KARES_DEBUG) << resource->resourceName() << ", " << (change == Added ? "Added" : change == Enabled ? "Enabled" : change == ReadOnly ? "ReadOnly" : change == WrongType ? "WrongType" : change == Location ? "Location" : "Colour");
    if (!resource->writable())
    {
        // The resource is no longer writable, so it can't be a standard resource
        // N.B. Setting manager's standard resource to 0 does nothing.
        if (resource->standardResource())
            resource->setStandardResource(false);
    }
    if (!mPassiveClient)
        mManager->change(resource);   // save resource's new configuration
    emit resourceStatusChanged(resource, change);
    if (change == Location  &&  resource->isActive())
        load(resource);
}

AlarmResource* AlarmResources::resourceWithId(const QString& resourceID) const
{
    for (AlarmResourceManager::Iterator it = mManager->begin();  it != mManager->end();  ++it)
    {
        if ((*it)->identifier() == resourceID)
            return *it;
    }
    return 0;
}

AlarmResource* AlarmResources::resourceForIncidence(const QString& incidenceID)
{
    return resource(incidence(incidenceID));
}

AlarmResource* AlarmResources::resource(const Incidence* incidence) const
{
    if (!incidence)
        return 0;
    ResourceMap::ConstIterator it = mResourceMap.find(const_cast<Incidence*>(incidence));
    return (it != mResourceMap.constEnd()) ? it.value() : 0;
}

// Called by the resource manager when a resource is added to the collection
void AlarmResources::resourceAdded(AlarmResource* resource)
{
    kDebug(KARES_DEBUG) << resource->resourceName();
    connectResource(resource);
    if (resource->isActive())
        load(resource);
    emit resourceStatusChanged(resource, Added);
}

void AlarmResources::resourceDeleted(AlarmResource* resource)
{
    kDebug(KARES_DEBUG);
    emit resourceStatusChanged(resource, Deleted);
}

/******************************************************************************
* Set the time zone for all resources.
*/
void AlarmResources::doSetTimeSpec(const KDateTime::Spec& timeSpec)
{
    AlarmResourceManager::Iterator i1;
    for (i1 = mManager->begin(); i1 != mManager->end(); ++i1)
        (*i1)->setTimeSpec(timeSpec);
}

// vim: et sw=4:
