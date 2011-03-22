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
#include "collectionattribute.h"
#include "eventattribute.h"

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
using KAlarm::CollectionAttribute;
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
    int count = collections.count();
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
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<SettingsDialog> dlg = new SettingsDialog(windowId, mSettings);
    if (dlg->exec())
    {
        clearCache();
        initializeDirectory();
        loadFiles();
        synchronizeCollectionTree();
        emit configurationDialogAccepted();
    }
    else
    {
        emit configurationDialogRejected();
    }
}

/******************************************************************************
* Called when the resource settings have changed.
* Update the display name if it has changed.
* Stop monitoring the directory if 'monitorFiles' is now false.
* NOTE: no provision is made for changes to the directory path.
*/
void KAlarmDirResource::settingsChanged()
{
    kDebug();
    const QString display = mSettings->displayName();
    if (display != name())
        setName(display);

    const QString dirPath = mSettings->path();
    bool monitoring = KDirWatch::self()->contains(dirPath);
    if (monitoring  &&  !mSettings->monitorFiles())
        KDirWatch::self()->removeDir(dirPath);
    else if (!monitoring  &&  mSettings->monitorFiles())
        KDirWatch::self()->addDir(dirPath, KDirWatch::WatchFiles);
}

/******************************************************************************
* Load and parse data from each file in the directory.
* The events are cached in mEvents.
*/
bool KAlarmDirResource::loadFiles()
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
            const QFileInfo fi(path);
            if (fi.isFile())
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

#ifdef __GNUC__
#warning Need to synchronise if not calling from configure()
#endif

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
        kWarning() << events.count() << "events found in file" << path;
    const Event::Ptr kcalEvent(events[0]);
    if (kcalEvent->uid() != file)
        kWarning() << "File" << path << ": event id differs from file name";
    if (kcalEvent->alarms().isEmpty())
    {
        kWarning() << "File" << path << ": event contains no alarms";
        return KAEvent();
    }
    KAEvent event(kcalEvent);
    const QString mime = KAlarm::CalEvent::mimeType(event.category());
    if (mime.isEmpty())
    {
        kWarning() << "KAEvent has no usable alarms:" << event.id();
        return KAEvent();
    }
    event.setCompatibility(KAlarmResourceCommon::getCompatibility(fileStorage));
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
    QString eventId = item.remoteId();
    QString file = eventId;
    QHash<QString, EventFile>::iterator it = mEvents.find(eventId);
    if (it != mEvents.end())
    {
        file = it.value().files[0];
        nextFile = removeEventFile(eventId, file);
        mFileEventIds.remove(file);
DEBUG_DATA;
    }
    QFile::remove(filePath(file));

    loadNextFile(eventId, nextFile);   // load any other file with the same event ID
    setCompatibility();

    changeProcessed();
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
    const QString display = mSettings->displayName();
    c.setName(display.isEmpty() ? name() : display);
    c.setContentMimeTypes(mSettings->alarmTypes());
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

    EntityDisplayAttribute* attr = c.attribute<EntityDisplayAttribute>(Collection::AddIfMissing);
    attr->setDisplayName(name());
    attr->setIconName("kalarm");

    CollectionAttribute* cattr = c.attribute<CollectionAttribute>(Collection::AddIfMissing);
    cattr->setCompatibility(mCompatibility);

    mCollectionId = c.id();   // note the one and only collection for this resource

    Collection::List list;
    list << c;
    collectionsRetrieved(list);
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
    // Set the collection's compatibility status
    KAlarmResourceCommon::setCollectionCompatibility(collection, mCompatibility);

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
    if (path != directoryName())
    {
        const QString file = fileName(path);
        int i = mChangedFiles.indexOf(file);
        if (i >= 0)
            mChangedFiles.removeAt(i);   // the file was updated by this resource
        else if (isFileValid(file))
        {
            KAEvent event = loadFile(path, file);
            if (event.isValid())
            {
                Item item;
                if (event.setItemPayload(item, mSettings->alarmTypes()))
                {
                    // The event's type is compatible with the collection's mime types
                    item.setRemoteId(event.id());
                    event.setItemId(item.id());

                    addEventFile(event, file);
                    mFileEventIds.insert(file, event.id());

                    setCompatibility();

                    // Tell the Akonadi server to create an Item for the event
                    Collection c(mCollectionId);
                    item.setParentCollection(c);
                    ItemCreateJob* job = new ItemCreateJob(item, c);
                    connect(job, SIGNAL(result(KJob*)), SLOT(itemJobDone(KJob*)));
                    job->start();
DEBUG_DATA;
                }
            }
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
            const KAEvent event = loadFile(path, file);
            // Get the file's old event ID
            QHash<QString, QString>::iterator fit = mFileEventIds.find(file);
            if (fit != mFileEventIds.end())
            {
                oldId = fit.value();
                if (event.id() != oldId)
                {
                    // The file's event ID has changed - remove the old event
                    nextFile = removeEventFile(oldId, file);
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

#ifdef __GNUC__
#warning ????
#endif
            loadNextFile(oldId, nextFile);   // load any other file with the same event ID
            setCompatibility();

            // Tell the Akonadi server to amend the Item for the event
            Item item;
            if (!event.setItemPayload(item, mSettings->alarmTypes()))
                kWarning() << "Invalid mime type for collection";
            else
            {
                Collection c(mCollectionId);
                item.setParentCollection(c);
                item.setRemoteId(event.id());
                ItemModifyJob* job = new ItemModifyJob(item);
                job->disableRevisionCheck();
                connect(job, SIGNAL(result(KJob*)), SLOT(itemJobDone(KJob*)));
            }
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
//???        synchronize();
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

#ifdef __GNUC__
#warning ????
#endif
                loadNextFile(eventId, nextFile);   // load any other file with the same event ID
                setCompatibility();

                // Tell the Akonadi server to delete the Item for the event
                Collection c(mCollectionId);
                Item item(KAlarm::CalEvent::mimeType(event.category()));
                item.setParentCollection(c);
                item.setRemoteId(eventId);
                ItemDeleteJob* job = new ItemDeleteJob(item);
                connect(job, SIGNAL(result(KJob*)), SLOT(itemJobDone(KJob*)));
DEBUG_DATA;
            }
        }
    }
}

/******************************************************************************
* Called when an item job has completed.
* Checks for any error.
*/
void KAlarmDirResource::itemJobDone(KJob* j)
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
* Evaluate the version compatibility status of the calendar. This is either the
* status of the individual events if they are all the same, or 'ByEvent'
* otherwise.
*/
void KAlarmDirResource::setCompatibility(bool writeAttr)
{
    const KAlarm::Calendar::Compat oldCompatibility = mCompatibility;
    if (mEvents.isEmpty())
        mCompatibility = KAlarm::Calendar::Current;
    else
    {
        bool first = true;
        foreach (const EventFile& data, mEvents)
        {
            const KAEvent& event = data.event;
            if (first)
            {
                mCompatibility = event.compatibility();
                first = false;
            }
            else if (event.compatibility() != mCompatibility)
            {
                mCompatibility = KAlarm::Calendar::ByEvent;
                break;
            }
        }
    }
    if (writeAttr  &&  mCompatibility != oldCompatibility)
    {
        const Collection c(mCollectionId);
        if (c.isValid())
            KAlarmResourceCommon::setCollectionCompatibility(c, mCompatibility);
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
