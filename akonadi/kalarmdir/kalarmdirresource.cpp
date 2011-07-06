/*
 *  kalarmdirresource.cpp  -  Akonadi directory resource for KAlarm
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
 *  Copyright (c) 2008 Tobias Koenig <tokoe@kde.org>
 *  Copyright (c) 2008 Bertjan Broeksema <broeksema@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "kalarmdirresource.h"
#include "kalarmresourcecommon.h"
#include "autoqpointer.h"
#include "kacalendar.h"

#include "kalarmdirsettingsadaptor.h"
#include "settingsdialog.h"

#include <kcalcore/filestorage.h>
#include <kcalcore/icalformat.h>
#include <kcalcore/memorycalendar.h>
#include <akonadi/agentfactory.h>
#include <akonadi/changerecorder.h>
#include <akonadi/dbusconnectionpool.h>
#include <akonadi/entitydisplayattribute.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/collectionmodifyjob.h>
#include <akonadi/itemfetchscope.h>
#include <akonadi/itemcreatejob.h>
#include <akonadi/itemdeletejob.h>
#include <akonadi/itemmodifyjob.h>

#include <kdirwatch.h>
#include <klocale.h>
#include <kdebug.h>

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

using namespace Akonadi;
using namespace KCalCore;
using namespace Akonadi_KAlarm_Dir_Resource;
using KAlarmResourceCommon::errorMessage;

static bool isFileValid(const QString& file);

#define DEBUG_DATA \
kDebug()<<"ID:Files:"; \
foreach (const QString& id, mEvents.uniqueKeys()) { kDebug()<<id<<":"<<mEvents[id].files; } \
kDebug()<<"File:IDs:"; \
foreach (const QString& f, mFileEventIds.uniqueKeys()) { kDebug()<<f<<":"<<mFileEventIds[f]; }


KAlarmDirResource::KAlarmDirResource(const QString& id)
    : ResourceBase(id),
      mSettings(new Settings(componentData().config())),
      mCollectionId(-1),
      mCompatibility(KAlarm::Calendar::Incompatible),
      mCollectionFetched(true)
{
    kDebug() << id;
    KAlarmResourceCommon::initialise(this);

    // Set up the resource
    new KAlarmDirSettingsAdaptor(mSettings);
    DBusConnectionPool::threadConnection().registerObject(QLatin1String("/Settings"),
                                mSettings, QDBusConnection::ExportAdaptors);
    connect(mSettings, SIGNAL(configChanged()), SLOT(settingsChanged()));

    changeRecorder()->itemFetchScope().fetchFullPayload();
    changeRecorder()->fetchCollection(true);

    connect(KDirWatch::self(), SIGNAL(created(const QString&)), SLOT(fileCreated(const QString&)));
    connect(KDirWatch::self(), SIGNAL(dirty(const QString&)), SLOT(fileChanged(const QString&)));
    connect(KDirWatch::self(), SIGNAL(deleted(const QString&)), SLOT(fileDeleted(const QString&)));

    // Find the collection which this resource manages
    CollectionFetchJob* job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::FirstLevel);
    job->fetchScope().setResource(identifier());
    connect(job, SIGNAL(collectionsReceived(const Akonadi::Collection::List&)),
                 SLOT(collectionsReceived(const Akonadi::Collection::List&)));
    connect(job, SIGNAL(result(KJob*)), SLOT(collectionFetchResult(KJob*)));

    QTimer::singleShot(0, this, SLOT(loadFiles()));
}

KAlarmDirResource::~KAlarmDirResource()
{
}

void KAlarmDirResource::aboutToQuit()
{
    mSettings->writeConfig();
}

void KAlarmDirResource::collectionsReceived(const Akonadi::Collection::List& collections)
{
    kDebug();
    int count = collections.count();
    kDebug() << "Count:" << count;
    if (!count)
        kError() << "Cannot retrieve this resource's collection";
    else
    {
        if (count > 1)
            kError() << "Multiple collections for this resource:" << count;
        for (int i = 0;  i < count;  ++i)
        {
            if (collections[i].remoteId() == mSettings->path())
            {
                mCollectionId = collections[i].id();

                // Set collection's format compatibility flag now that the collection
                // and its attributes have been fetched.
                KAlarmResourceCommon::setCollectionCompatibility(collections[i], mCompatibility,
                                                                 (mCompatibility == KAlarm::Calendar::Current ? KAlarm::Calendar::CurrentFormat
                                                                                                              : KAlarm::Calendar::MixedFormat));
                break;
            }
        }
    }
    mCollectionFetched = true;
}

void KAlarmDirResource::collectionFetchResult(KJob* j)
{
    mCollectionFetched = true;
    if (j->error())
        kError() << "CollectionFetchJob error: " << j->errorString();
}

void KAlarmDirResource::configure(WId windowId)
{
    kDebug();
    // Keep note of the old configuration settings
    QString     path     = mSettings->path();
    QString     name     = mSettings->displayName();
    bool        readOnly = mSettings->readOnly();
    QStringList types    = mSettings->alarmTypes();
    // Note: mSettings->monitorFiles() can't change here

    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<SettingsDialog> dlg = new SettingsDialog(windowId, mSettings);
    if (dlg->exec())
    {
        if (path.isEmpty())
        {
            // Creating a new resource
            clearCache();   // this deletes any existing collection
            initializeDirectory();   // should only be needed for new resource, but just in case ...
            loadFiles(true);
            synchronizeCollectionTree();
        }
        else if (mSettings->path() != path)
        {
            // Directory path change is not allowed for existing resources
            emit configurationDialogRejected();
            return;
        }
        else if (mSettings->alarmTypes() != types)
        {
            // Settings have changed which might affect the alarm configuration
            initializeDirectory();   // should only be needed for new resource, but just in case ...
            KAlarm::CalEvent::Types newTypes = KAlarm::CalEvent::types(mSettings->alarmTypes());
            KAlarm::CalEvent::Types oldTypes = KAlarm::CalEvent::types(types);
            changeAlarmTypes(~newTypes & oldTypes);
        }
        else if (mSettings->readOnly() != readOnly
             ||  mSettings->displayName() != name)
        {
            // Need to change the collection's rights or name
            Collection c(mCollectionId);
            c.setRemoteId(directoryName());
            setNameRights(c);
            CollectionModifyJob* job = new CollectionModifyJob(c);
            connect(job, SIGNAL(result(KJob*)), SLOT(jobDone(KJob*)));
        }
        emit configurationDialogAccepted();
    }
    else
    {
        emit configurationDialogRejected();
    }
}

/******************************************************************************
* Add/remove events to ensure that they match the changed alarm types for the
* resource.
*/
void KAlarmDirResource::changeAlarmTypes(KAlarm::CalEvent::Types removed)
{
DEBUG_DATA;
    const QString dirPath = directoryName();
    kDebug() << dirPath;
    const QDir dir(dirPath);

    // Read and parse each file in turn
    QDirIterator it(dir);
    while (it.hasNext())
    {
        it.next();
        int removeIfInvalid = 0;
        QString fileEventId;
        const QString file = it.fileName();
        if (!isFileValid(file))
            continue;
        QHash<QString, QString>::iterator fit = mFileEventIds.find(file);
        if (fit != mFileEventIds.end())
        {
            // The file is in the existing file list
            fileEventId = fit.value();
            QHash<QString, EventFile>::ConstIterator it = mEvents.constFind(fileEventId);
            if (it != mEvents.constEnd())
            {
                // And its event is in the existing events list
                const EventFile& data = it.value();
                if (data.files[0] == file)
                {
                    // It's the file for a used event
                    if (data.event.category() & removed)
                    {
                        // The event's type is no longer wanted, so remove it
                        deleteItem(data.event);
                        removeEvent(data.event.id(), false);
                    }
                    continue;
                }
                else
                {
                    // The file's event is not currently used - load the
                    // file and use its event if appropriate.
                    removeIfInvalid = 0x03;   // remove from mEvents and mFileEventIds
                }
            }
            else
            {
                // The file's event isn't in the list of current valid
                // events - this shouldn't ever happen
                removeIfInvalid = 0x01;   // remove from mFileEventIds
            }
        }

        // Load the file and use its event if appropriate.
        const QString path = filePath(file);
        if (QFileInfo(path).isFile())
        {
            if (createItemAndIndex(path, file))
                continue;
        }
        // The event wasn't wanted, so remove from lists
        if (removeIfInvalid & 0x01)
            mFileEventIds.erase(fit);
        if (removeIfInvalid & 0x02)
            removeEventFile(fileEventId, file);
    }
DEBUG_DATA;
    setCompatibility();

    // Update the Akonadi server with the new alarm types
    Collection c(mCollectionId);
    c.setContentMimeTypes(mSettings->alarmTypes());
    CollectionModifyJob* job = new CollectionModifyJob(c);
    connect(job, SIGNAL(result(KJob*)), SLOT(jobDone(KJob*)));
}

/******************************************************************************
* Called when the resource settings have changed.
* Update the display name if it has changed.
* Stop monitoring the directory if 'monitorFiles' is now false.
* Update the storage format if UpdateStorageFormat setting = true.
* NOTE: no provision is made for changes to the directory path, since this is
*       not permitted (would need remote ID changed, plus other complications).
*/
void KAlarmDirResource::settingsChanged()
{
    kDebug();
    const QString display = mSettings->displayName();
    if (display != name())
        setName(display);

    const QString dirPath = mSettings->path();
    if (!dirPath.isEmpty())
    {
        const bool monitoring = KDirWatch::self()->contains(dirPath);
        if (monitoring  &&  !mSettings->monitorFiles())
            KDirWatch::self()->removeDir(dirPath);
        else if (!monitoring  &&  mSettings->monitorFiles())
            KDirWatch::self()->addDir(dirPath, KDirWatch::WatchFiles);
#if 0
        if (mSettings->monitorFiles() && !monitor)
        {
            // Settings have changed which might affect the alarm configuration
kDebug()<<"Monitored changed";
            initializeDirectory();   // should only be needed for new resource, but just in case ...
            loadFiles(true);
//              synchronizeCollectionTree();
        }
#endif
    }

    if (mSettings->updateStorageFormat())
    {
        // This is a flag to request that the backend calendar storage format should
        // be updated to the current KAlarm format.
        KAlarm::Calendar::Compat okCompat(KAlarm::Calendar::Current | KAlarm::Calendar::Convertible);
        if (mCompatibility & ~okCompat)
            kWarning() << "Either incompatible storage format or nothing to update";
        else if (mSettings->readOnly())
            kWarning() << "Cannot update storage format for a read-only resource";
        else
        {
            // Update the backend storage format to the current KAlarm format
            bool ok = true;
            for (QHash<QString, EventFile>::iterator it = mEvents.begin();  it != mEvents.end();  ++it)
            {
                KAEvent& event = it.value().event;
                if (event.compatibility() == KAlarm::Calendar::Convertible)
                {
                    if (writeToFile(event))
                        event.setCompatibility(KAlarm::Calendar::Current);
                    else
                    {
                        kWarning() << "Error updating storage format for event id" << event.id();
                        ok = false;
                    }
                }
            }
            if (ok)
            {
                mCompatibility = KAlarm::Calendar::Current;
                const Collection c(mCollectionId);
                if (c.isValid())
                    KAlarmResourceCommon::setCollectionCompatibility(c, mCompatibility, KAlarm::Calendar::CurrentFormat);
            }
        }
        mSettings->setUpdateStorageFormat(false);
        mSettings->writeConfig();
    }
}

/******************************************************************************
* Load and parse data from each file in the directory.
* The events are cached in mEvents.
*/
bool KAlarmDirResource::loadFiles(bool sync)
{
    const QString dirPath = directoryName();
    kDebug() << dirPath;
    const QDir dir(dirPath);

    mEvents.clear();
    mFileEventIds.clear();

    // Set the resource display name to the configured name, else the directory
    // name, if not already set.
    QString display = mSettings->displayName();
    if (display.isEmpty()  &&  (name().isEmpty() || name() == identifier()))
        display = dir.dirName();
    if (!display.isEmpty())
        setName(display);

    // Read and parse each file in turn
    QDirIterator it(dir);
    while (it.hasNext())
    {
        it.next();
        const QString file = it.fileName();
        if (isFileValid(file))
        {
            const QString path = filePath(file);
            if (QFileInfo(path).isFile())
            {
                const KAEvent event = loadFile(path, file);
                if (event.isValid())
                {
                    addEventFile(event, file);
                    mFileEventIds.insert(file, event.id());
                }
            }
        }
    }
DEBUG_DATA;

    setCompatibility(false);   // don't write compatibility - no collection exists yet

    if (mSettings->monitorFiles())
    {
        // Monitor the directory for changes to the files
        if (!KDirWatch::self()->contains(dirPath))
            KDirWatch::self()->addDir(dirPath, KDirWatch::WatchFiles);
    }

    if (sync)
    {
        // Ensure the Akonadi server is updated with the current list of events
        synchronize();
    }

    emit status(Idle);
    return true;
}

/******************************************************************************
* Load and parse data a single file in the directory.
*/
KAEvent KAlarmDirResource::loadFile(const QString& path, const QString& file)
{
    kDebug() << path;
    MemoryCalendar::Ptr calendar(new MemoryCalendar(QLatin1String("UTC")));
    FileStorage::Ptr fileStorage(new FileStorage(calendar, path, new ICalFormat()));
    if (!fileStorage->load())
    {
        kWarning() << "Error loading" << path;
        return KAEvent();
    }
    const Event::List events = calendar->events();
    if (events.count() > 1)
    {
        kWarning() << "Deleting" << events.count() - 1 << "excess events found in file" << path;
        for (int i = 1;  i < events.count();  ++i)
            calendar->deleteEvent(events[i]);
    }
    const Event::Ptr kcalEvent(events[0]);
    if (kcalEvent->uid() != file)
        kWarning() << "File" << path << ": event id differs from file name";
    if (kcalEvent->alarms().isEmpty())
    {
        kWarning() << "File" << path << ": event contains no alarms";
        return KAEvent();
    }
    // Convert event in memory to current KAlarm format if possible
    int version;
    KAlarm::Calendar::Compat compat = KAlarmResourceCommon::getCompatibility(fileStorage, version);
    KAEvent event(kcalEvent);
    const QString mime = KAlarm::CalEvent::mimeType(event.category());
    if (mime.isEmpty())
    {
        kWarning() << "KAEvent has no usable alarms:" << event.id();
        return KAEvent();
    }
    if (!mSettings->alarmTypes().contains(mime))
    {
        kWarning() << "KAEvent has wrong alarm type for resource:" << mime;
        return KAEvent();
    }
    event.setCompatibility(compat);
    return event;
}

/******************************************************************************
* After a file/event has been removed, load the next file in the list for the
* event ID.
* Reply = new event, or invalid if none.
*/
KAEvent KAlarmDirResource::loadNextFile(const QString& eventId, const QString& file)
{
    QString nextFile = file;
    while (!nextFile.isEmpty())
    {
        // There is another file with the same ID - load it
        const KAEvent event = loadFile(filePath(nextFile), nextFile);
        if (event.isValid())
        {
            addEventFile(event, nextFile);
            mFileEventIds.insert(nextFile, event.id());
            return event;
        }
        mFileEventIds.remove(nextFile);
        nextFile = removeEventFile(eventId, nextFile);
    }
    return KAEvent();
}

/******************************************************************************
* Retrieve an event from the calendar, whose uid and Akonadi id are given by
* 'item' (item.remoteId() and item.id() respectively).
* Set the event into a new item's payload, and signal its retrieval by calling
* itemRetrieved(newitem).
*/
bool KAlarmDirResource::retrieveItem(const Akonadi::Item& item, const QSet<QByteArray>&)
{
    const QString rid = item.remoteId();
    QHash<QString, EventFile>::ConstIterator it = mEvents.constFind(rid);
    if (it == mEvents.constEnd())
    {
        kWarning() << "Event not found:" << rid;
        emit error(errorMessage(KAlarmResourceCommon::UidNotFound, rid));
        return false;
    }

    KAEvent event(it.value().event);
    const Item newItem = KAlarmResourceCommon::retrieveItem(item, event);
    itemRetrieved(newItem);
    return true;
}

/******************************************************************************
* Called when an item has been added to the collection.
* Store the event in a file, and set its Akonadi remote ID to the KAEvent's UID.
*/
void KAlarmDirResource::itemAdded(const Akonadi::Item& item, const Akonadi::Collection&)
{
    kDebug() << item.id();
    if (cancelIfReadOnly())
        return;

    KAEvent event;
    if (item.hasPayload<KAEvent>())
        event = item.payload<KAEvent>();
    if (!event.isValid())
    {
        changeProcessed();
        return;
    }
    event.setCompatibility(KAlarm::Calendar::Current);
    setCompatibility();

    if (!writeToFile(event))
        return;

    addEventFile(event, event.id());

    Item newItem(item);
    newItem.setRemoteId(event.id());
//    scheduleWrite();    //???? is this needed?
    changeCommitted(newItem);
}

/******************************************************************************
* Called when an item has been changed.
* Store the changed event in a file.
*/
void KAlarmDirResource::itemChanged(const Akonadi::Item& item, const QSet<QByteArray>&)
{
    kDebug() << item.id() << ", remote ID:" << item.remoteId();
    if (cancelIfReadOnly())
        return;
    QHash<QString, EventFile>::iterator it = mEvents.find(item.remoteId());
    if (it != mEvents.end())
    {
        if (it.value().event.isReadOnly())
        {
            kWarning() << "Event is read only:" << item.remoteId();
            cancelTask(errorMessage(KAlarmResourceCommon::EventReadOnly, item.remoteId()));
            return;
        }
        if (it.value().event.compatibility() != KAlarm::Calendar::Current)
        {
            kWarning() << "Event not in current format:" << item.remoteId();
            cancelTask(errorMessage(KAlarmResourceCommon::EventNotCurrentFormat, item.remoteId()));
            return;
        }
    }

    KAEvent event;
    if (item.hasPayload<KAEvent>())
         event = item.payload<KAEvent>();
    if (!event.isValid())
    {
        changeProcessed();
        return;
    }
#if 0
    QString errorMsg;
    KAEvent event = KAlarmResourceCommon::checkItemChanged(item, errorMsg);
    if (!event.isValid())
    {
        if (errorMsg.isEmpty())
            changeProcessed();
        else
            cancelTask(errorMsg);
        return;
    }
#endif
    event.setCompatibility(KAlarm::Calendar::Current);
    if (mCompatibility != KAlarm::Calendar::Current)
        setCompatibility();

    if (!writeToFile(event))
        return;

    it.value().event = event;

    changeCommitted(item);
}

/******************************************************************************
* Called when an item has been deleted.
* Delete the item's file.
*/
void KAlarmDirResource::itemRemoved(const Akonadi::Item& item)
{
    kDebug() << item.id();
    if (cancelIfReadOnly())
        return;

    QString nextFile;
    removeEvent(item.remoteId(), true);
    setCompatibility();
    changeProcessed();
}

/******************************************************************************
* Remove an event from the indexes, and optionally delete its file.
*/
void KAlarmDirResource::removeEvent(const QString& eventId, bool deleteFile)
{
    QString file = eventId;
    QString nextFile;
    QHash<QString, EventFile>::iterator it = mEvents.find(eventId);
    if (it != mEvents.end())
    {
        file = it.value().files[0];
        nextFile = removeEventFile(eventId, file);
        mFileEventIds.remove(file);
DEBUG_DATA;
    }
    if (deleteFile)
        QFile::remove(filePath(file));

    loadNextFile(eventId, nextFile);   // load any other file with the same event ID
}

/******************************************************************************
* If the resource is read-only, cancel the task and emit an error.
* Reply = true if cancelled.
*/
bool KAlarmDirResource::cancelIfReadOnly()
{
    if (mSettings->readOnly())
    {
        kWarning() << "Calendar is read-only:" << directoryName();
        emit error(i18nc("@info", "Trying to write to a read-only calendar: '%1'", directoryName()));
        cancelTask();
        return true;
    }
    return false;
}

/******************************************************************************
* Write an event to a file. The file name is the event's id.
*/
bool KAlarmDirResource::writeToFile(const KAEvent& event)
{
    Event::Ptr kcalEvent(new Event);
    event.updateKCalEvent(kcalEvent, KAEvent::UID_SET);
    MemoryCalendar::Ptr calendar(new MemoryCalendar(QLatin1String("UTC")));
    KAlarm::Calendar::setKAlarmVersion(calendar);   // set the KAlarm custom property
    calendar->addIncidence(kcalEvent);

    mChangedFiles += event.id();    // suppress KDirWatch processing for this write

    const QString path = filePath(event.id());
    kDebug() << event.id() << " File:" << path;
    FileStorage::Ptr fileStorage(new FileStorage(calendar, path, new ICalFormat()));
    if (!fileStorage->save())
    {
        emit error(i18nc("@info", "Failed to save event file: %1", path));
        cancelTask();
        return false;
    }
    return true;
}

/******************************************************************************
* Create the resource's collection.
*/
void KAlarmDirResource::retrieveCollections()
{
    kDebug();
    Collection c;
    c.setParentCollection(Collection::root());
    c.setRemoteId(directoryName());
    c.setContentMimeTypes(mSettings->alarmTypes());
    setNameRights(c);

    EntityDisplayAttribute* attr = c.attribute<EntityDisplayAttribute>(Collection::AddIfMissing);
    attr->setIconName("kalarm");
    // Don't update CollectionAttribute here, since it hasn't yet been fetched
    // from Akonadi database.

    Collection::List list;
    list << c;
    collectionsRetrieved(list);
}

/******************************************************************************
* Set the collection's name and rights.
* It is the caller's responsibility to notify the Akonadi server.
*/
void KAlarmDirResource::setNameRights(Collection& c)
{
    kDebug();
    const QString display = mSettings->displayName();
    c.setName(display.isEmpty() ? name() : display);
    EntityDisplayAttribute* attr = c.attribute<EntityDisplayAttribute>(Collection::AddIfMissing);
    attr->setDisplayName(name());
    if (mSettings->readOnly())
    {
        c.setRights(Collection::CanChangeCollection);
    }
    else
    {
        Collection::Rights rights = Collection::ReadOnly;
        rights |= Collection::CanChangeItem;
        rights |= Collection::CanCreateItem;
        rights |= Collection::CanDeleteItem;
        rights |= Collection::CanChangeCollection;
        c.setRights(rights);
    }
}

/******************************************************************************
* Retrieve all events from the directory, and set each into a new item's
* payload. Items are identified by their remote IDs. The Akonadi ID is not
* used.
* Signal the retrieval of the items by calling itemsRetrieved(items), which
* updates Akonadi with any changes to the items. itemsRetrieved() compares
* the new and old items, matching them on the remoteId(). If the flags or
* payload have changed, or the Item has any new Attributes, the Akonadi
* storage is updated.
*/
void KAlarmDirResource::retrieveItems(const Akonadi::Collection& collection)
{
    mCollectionId = collection.id();   // note the one and only collection for this resource
    kDebug() << "Collection id:" << mCollectionId;

    // Set the collection's compatibility status
    KAlarmResourceCommon::setCollectionCompatibility(collection, mCompatibility,
                                                     (mCompatibility == KAlarm::Calendar::Current ? KAlarm::Calendar::CurrentFormat
                                                                                                  : KAlarm::Calendar::MixedFormat));

    // Fetch the list of valid mime types
    const QStringList mimeTypes = mSettings->alarmTypes();

    // Retrieve events
    Item::List items;
    foreach (const EventFile& data, mEvents)
    {
        const KAEvent& event = data.event;
        const QString mime = KAlarm::CalEvent::mimeType(event.category());
        if (mime.isEmpty())
        {
            kWarning() << "KAEvent has no alarms:" << event.id();
            continue;   // event has no usable alarms
        }
        if (!mimeTypes.contains(mime))
            continue;   // restrict alarms returned to the defined types

        Item item(mime);
        item.setRemoteId(event.id());
        item.setPayload(event);
        items.append(item);
    }

    itemsRetrieved(items);
}

/******************************************************************************
* Called when the collection has been changed.
* Set its display name if that has changed.
*/
void KAlarmDirResource::collectionChanged(const Akonadi::Collection& collection)
{
    kDebug();
    // If the collection has a new display name, set the resource's display
    // name the same, and save to the settings.
    QString newName = collection.name();
    EntityDisplayAttribute* attr = 0;
    if (collection.hasAttribute<EntityDisplayAttribute>())
    {
        attr = collection.attribute<EntityDisplayAttribute>();
        if (!attr->displayName().isEmpty())
            newName = attr->displayName();
    }
    if (!newName.isEmpty()  &&  newName != name())
        setName(newName);
    if (newName != mSettings->displayName())
    {
        mSettings->setDisplayName(newName);
        mSettings->writeConfig();
    }

    changeCommitted(collection);
}

/******************************************************************************
* Called when a file has been created in the directory.
*/
void KAlarmDirResource::fileCreated(const QString& path)
{
    kDebug() << path;
    if (path == directoryName())
    {
        // The directory has been created. Load all files in it, and
        // tell the Akonadi server to create an Item for each event.
        loadFiles(true);
        foreach (const EventFile& data, mEvents)
        {
            createItem(data.event);
        }
    }
    else
    {
        const QString file = fileName(path);
        int i = mChangedFiles.indexOf(file);
        if (i >= 0)
            mChangedFiles.removeAt(i);   // the file was updated by this resource
        else if (isFileValid(file))
        {
            if (createItemAndIndex(path, file))
                setCompatibility();
DEBUG_DATA;
        }
    }
}


/******************************************************************************
* Called when a file has changed in the directory.
*/
void KAlarmDirResource::fileChanged(const QString& path)
{
    if (path != directoryName())
    {
        kDebug() << path;
        const QString file = fileName(path);
        int i = mChangedFiles.indexOf(file);
        if (i >= 0)
            mChangedFiles.removeAt(i);   // the file was updated by this resource
        else if (isFileValid(file))
        {
            QString nextFile, oldId;
            KAEvent oldEvent;
            const KAEvent event = loadFile(path, file);
            // Get the file's old event ID
            QHash<QString, QString>::iterator fit = mFileEventIds.find(file);
            if (fit != mFileEventIds.end())
            {
                oldId = fit.value();
                if (event.id() != oldId)
                {
                    // The file's event ID has changed - remove the old event
                    nextFile = removeEventFile(oldId, file, &oldEvent);
                    if (event.isValid())
                        fit.value() = event.id();
                    else
                        mFileEventIds.erase(fit);
                }
            }
            else if (event.isValid())
            {
                // The file didn't contain an event before. Save details of the new event.
                mFileEventIds.insert(file, event.id());
            }
            addEventFile(event, file);

            KAEvent e = loadNextFile(oldId, nextFile);   // load any other file with the same event ID
            setCompatibility();

            // Tell the Akonadi server to amend the Item for the event
            if (event.id() != oldId)
            {
                if (e.isValid())
                    modifyItem(e);
                else
                    deleteItem(oldEvent);
                createItem(event);   // create a new Item for the new event ID
            }
            else
                modifyItem(event);
DEBUG_DATA;
        }
    }
}

/******************************************************************************
* Called when a file has been deleted in the directory.
*/
void KAlarmDirResource::fileDeleted(const QString& path)
{
    kDebug() << path;
    if (path == directoryName())
    {
        // The directory has been deleted
        mEvents.clear();
        mFileEventIds.clear();

        // Tell the Akonadi server to delete all Items in the collection
        Collection c(mCollectionId);
        ItemDeleteJob* job = new ItemDeleteJob(c);
        connect(job, SIGNAL(result(KJob*)), SLOT(jobDone(KJob*)));
    }
    else
    {
        // A single file has been deleted
        const QString file = fileName(path);
        if (isFileValid(file))
        {
            QHash<QString, QString>::iterator fit = mFileEventIds.find(file);
            if (fit != mFileEventIds.end())
            {
                QString eventId = fit.value();
                KAEvent event;
                QString nextFile = removeEventFile(eventId, file, &event);
                mFileEventIds.erase(fit);

                KAEvent e = loadNextFile(eventId, nextFile);   // load any other file with the same event ID
                setCompatibility();

                if (e.isValid())
                {
                    // Tell the Akonadi server to amend the Item for the event
                    modifyItem(e);
                }
                else
                {
                    // Tell the Akonadi server to delete the Item for the event
                    deleteItem(event);
                }
DEBUG_DATA;
            }
        }
    }
}

/******************************************************************************
* Tell the Akonadi server to create an Item for a given file's event, and add
* it to the indexes.
*/
bool KAlarmDirResource::createItemAndIndex(const QString& path, const QString& file)
{
    const KAEvent event = loadFile(path, file);
    if (event.isValid())
    {
        // Tell the Akonadi server to create an Item for the event
        if (createItem(event))
        {
            addEventFile(event, file);
            mFileEventIds.insert(file, event.id());

            return true;
        }
    }
    return false;
}

/******************************************************************************
* Tell the Akonadi server to create an Item for a given event.
*/
bool KAlarmDirResource::createItem(const KAEvent& event)
{
    Item item;
    if (!event.setItemPayload(item, mSettings->alarmTypes()))
    {
        kWarning() << "Invalid mime type for collection";
        return false;
    }
    Collection c(mCollectionId);
    item.setParentCollection(c);
    item.setRemoteId(event.id());
    ItemCreateJob* job = new ItemCreateJob(item, c);
    connect(job, SIGNAL(result(KJob*)), SLOT(jobDone(KJob*)));
    return true;
}

/******************************************************************************
* Tell the Akonadi server to amend the Item for a given event.
*/
bool KAlarmDirResource::modifyItem(const KAEvent& event)
{
    Item item;
    if (!event.setItemPayload(item, mSettings->alarmTypes()))
    {
        kWarning() << "Invalid mime type for collection";
        return false;
    }
    Collection c(mCollectionId);
    item.setParentCollection(c);
    item.setRemoteId(event.id());
    ItemModifyJob* job = new ItemModifyJob(item);
    job->disableRevisionCheck();
    connect(job, SIGNAL(result(KJob*)), SLOT(jobDone(KJob*)));
    return true;
}

/******************************************************************************
* Tell the Akonadi server to delete the Item for a given event.
*/
void KAlarmDirResource::deleteItem(const KAEvent& event)
{
    Item item(KAlarm::CalEvent::mimeType(event.category()));
    Collection c(mCollectionId);
    item.setParentCollection(c);
    item.setRemoteId(event.id());
    ItemDeleteJob* job = new ItemDeleteJob(item);
    connect(job, SIGNAL(result(KJob*)), SLOT(jobDone(KJob*)));
}

/******************************************************************************
* Called when a collection or item job has completed.
* Checks for any error.
*/
void KAlarmDirResource::jobDone(KJob* j)
{
    if (j->error())
        kError() << j->metaObject()->className() << "error:" << j->errorString();
}

/******************************************************************************
* Create the directory if it doesn't already exist, and ensure that it
* contains a WARNING_README.txt file.
*/
void KAlarmDirResource::initializeDirectory() const
{
    kDebug();
    const QDir dir(directoryName());
    const QString dirPath = dir.absolutePath();

    // If folder does not exist, create it
    if (!dir.exists())
    {
        kDebug() << "Creating" << dirPath;
        QDir::root().mkpath(dirPath);
    }

    // Check whether warning file is in place...
    QFile file(dirPath + QDir::separator() + "WARNING_README.txt");
    if (!file.exists())
    {
        // ... if not, create it
        file.open(QIODevice::WriteOnly);
        file.write("Important Warning!!!\n\n"
                   "Do not create or copy items inside this folder manually: they are managed by the Akonadi framework!\n");
        file.close();
    }
}

QString KAlarmDirResource::directoryName() const
{
    return mSettings->path();
}

QString KAlarmDirResource::filePath(const QString& file) const
{
    return mSettings->path() + QDir::separator() + file;
}

/******************************************************************************
* Strip the directory path from a file name.
*/
QString KAlarmDirResource::fileName(const QString& path) const
{
    const QFileInfo fi(path);
    if (fi.isDir()  ||  fi.isBundle())
        return QString();
    if (fi.path() == mSettings->path())
        return fi.fileName();
    return path;
}

/******************************************************************************
* Evaluate the version compatibility status of the calendar. This is the OR of
* the statuses of the individual events.
*/
void KAlarmDirResource::setCompatibility(bool writeAttr)
{
    static const KAlarm::Calendar::Compat AllCompat(KAlarm::Calendar::Current | KAlarm::Calendar::Convertible | KAlarm::Calendar::Incompatible);

    const KAlarm::Calendar::Compat oldCompatibility = mCompatibility;
    if (mEvents.isEmpty())
        mCompatibility = KAlarm::Calendar::Current;
    else
    {
        mCompatibility = KAlarm::Calendar::Unknown;
        foreach (const EventFile& data, mEvents)
        {
            const KAEvent& event = data.event;
            mCompatibility |= event.compatibility();
            if ((mCompatibility & AllCompat) == AllCompat)
                break;
        }
    }
    if (writeAttr  &&  mCompatibility != oldCompatibility)
    {
        const Collection c(mCollectionId);
        if (c.isValid())
            KAlarmResourceCommon::setCollectionCompatibility(c, mCompatibility,
                                                             (mCompatibility == KAlarm::Calendar::Current ? KAlarm::Calendar::CurrentFormat
                                                                                                          : KAlarm::Calendar::MixedFormat));
    }
}

/******************************************************************************
* Add an event/file combination to the mEvents map.
*/
void KAlarmDirResource::addEventFile(const KAEvent& event, const QString& file)
{
    if (event.isValid())
    {
        QHash<QString, EventFile>::iterator it = mEvents.find(event.id());
        if (it != mEvents.end())
        {
            EventFile& data = it.value();
            data.event = event;
            data.files.removeAll(file);   // in case it isn't the first file
            data.files.prepend(file);
        }
        else
            mEvents.insert(event.id(), EventFile(event, QStringList(file)));
    }
}

/******************************************************************************
* Remove an event ID/file combination from the mEvents map.
* Reply = next file with the same event ID.
*/
QString KAlarmDirResource::removeEventFile(const QString& eventId, const QString& file, KAEvent* event)
{
    QHash<QString, EventFile>::iterator it = mEvents.find(eventId);
    if (it != mEvents.end())
    {
        if (event)
            *event = it.value().event;
        it.value().files.removeAll(file);
        if (!it.value().files.isEmpty())
            return it.value().files[0];
        mEvents.erase(it);
    }
    else if (event)
        *event = KAEvent();
    return QString();
}

/******************************************************************************
* Check whether a file is to be ignored.
* Reply = false if file is to be ignored.
*/
bool isFileValid(const QString& file)
{
    return !file.isEmpty()
        &&  !file.startsWith('.')  &&  !file.endsWith('~')
        &&  file != QLatin1String("WARNING_README.txt");
}

AKONADI_AGENT_FACTORY(KAlarmDirResource, akonadi_kalarm_dir_resource)

#include "kalarmdirresource.moc"

// vim: et sw=4:
