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
#include <akonadi/itemfetchscope.h>

#include <klocale.h>
#include <kdebug.h>

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>

using namespace Akonadi;
using namespace KCalCore;
using namespace Akonadi_KAlarm_Dir_Resource;
using KAlarm::CollectionAttribute;
using KAlarmResourceCommon::errorMessage;


KAlarmDirResource::KAlarmDirResource(const QString& id)
    : ResourceBase(id),
      mSettings(new Settings(componentData().config())),
      mCompatibility(KAlarm::Calendar::Incompatible)
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
}

KAlarmDirResource::~KAlarmDirResource()
{
}

void KAlarmDirResource::aboutToQuit()
{
    mSettings->writeConfig();
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
*/
void KAlarmDirResource::settingsChanged()
{
    kDebug();
    QString display = mSettings->displayName();
    if (display != name())
        setName(display);
}

/******************************************************************************
* Load and parse data from each file in the directory.
* The events are cached in mEvents.
*/
bool KAlarmDirResource::loadFiles()
{
    kDebug() << directoryName();
    mEvents.clear();

    QDir dir(directoryName());

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
        QString fileName = it.fileName();
        if (fileName != "." && fileName != ".."
        &&  !fileName.endsWith('~')  &&  fileName != "WARNING_README.txt")
        {
            MemoryCalendar::Ptr calendar(new MemoryCalendar(QLatin1String("UTC")));
            FileStorage::Ptr fileStorage(new FileStorage(calendar, fileName, new ICalFormat()));
            if (!fileStorage->load())
            {
                kWarning() << "Error loading" << fileName;
                continue;
            }
            const Event::List events = calendar->events();
            if (events.count() > 1)
                kWarning() << events.count() << "events found in file" << fileName;
            const Event::Ptr kcalEvent(events[0]);
            if (kcalEvent->alarms().isEmpty())
            {
                kWarning() << "File" << fileName << ": event contains no alarms";
                continue;
            }
            KAEvent event(kcalEvent);
            QString mime = KAlarm::CalEvent::mimeType(event.category());
            if (mime.isEmpty())
            {
                kWarning() << "KAEvent has no usable alarms:" << event.id();
                continue;
            }
            event.setCompatibility(KAlarmResourceCommon::getCompatibility(fileStorage));
            mEvents.insert(event.id(), event);
        }
    }

    setCompatibility(false);

    emit status(Idle);
    return true;
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
    QMap<QString, KAEvent>::ConstIterator it = mEvents.constFind(rid);
    if (it == mEvents.constEnd())
    {
        kWarning() << "Event not found:" << rid;
        emit error(errorMessage(KAlarmResourceCommon::UidNotFound, rid));
        return false;
    }

    KAEvent event(it.value());
    Item newItem = KAlarmResourceCommon::retrieveItem(item, event);
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
    if (mCompatibility != KAlarm::Calendar::Current)
        mCompatibility = KAlarm::Calendar::ByEvent;

    if (!writeToFile(event))
        return;

    mEvents.insert(event.id(), event);

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
    QMap<QString, KAEvent>::Iterator it = mEvents.find(item.remoteId());
    if (it != mEvents.end())
    {
        if (it.value().isReadOnly())
        {
            kWarning() << "Event is read only:" << item.remoteId();
            cancelTask(errorMessage(KAlarmResourceCommon::EventReadOnly, item.remoteId()));
            return;
        }
        if (it.value().compatibility() != KAlarm::Calendar::Current)
        {
            kWarning() << "Event not in current format:" << item.remoteId();
            cancelTask(errorMessage(KAlarmResourceCommon::EventNotCurrentFormat, item.remoteId()));
            return;
        }
    }

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
    event.setCompatibility(KAlarm::Calendar::Current);
    if (mCompatibility != KAlarm::Calendar::Current)
        setCompatibility();

    if (!writeToFile(event))
        return;

    it.value() = event;

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

    mEvents.remove(item.remoteId());
    QFile::remove(directoryFileName(item.remoteId()));

    if (mCompatibility == KAlarm::Calendar::ByEvent)
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
* Write an event to a file.
*/
bool KAlarmDirResource::writeToFile(const KAEvent& event)
{
    Event::Ptr kcalEvent(new Event);
    event.updateKCalEvent(kcalEvent, KAEvent::UID_SET);
    MemoryCalendar::Ptr calendar(new MemoryCalendar(QLatin1String("UTC")));
    KAlarm::Calendar::setKAlarmVersion(calendar);   // set the KAlarm custom property
    calendar->addIncidence(kcalEvent);

    QString fileName = directoryFileName(event.id());
    kDebug() << event.id() << " File:" << fileName;
    FileStorage::Ptr fileStorage(new FileStorage(calendar, fileName, new ICalFormat()));
    if (!fileStorage->save())
    {
        emit error(i18nc("@info", "Failed to save event file: %1", fileName));
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
    QStringList mimeTypes = mSettings->alarmTypes();

    // Retrieve events
    Item::List items;
    foreach (const KAEvent& event, mEvents)
    {
        QString mime = KAlarm::CalEvent::mimeType(event.category());
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

QString KAlarmDirResource::directoryName() const
{
    return mSettings->path();
}

QString KAlarmDirResource::directoryFileName(const QString& file) const
{
    return mSettings->path() + QDir::separator() + file;
}

/******************************************************************************
* Create the directory if it doesn't already exist, and ensure that it
* contains a WARNING_README.txt file.
*/
void KAlarmDirResource::initializeDirectory() const
{
    QDir dir(directoryName());
    QString dirPath = dir.absolutePath();

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

/******************************************************************************
* Evaluate the version compatibility status of the calendar. This is either the
* status of the individual events if they are all the same, or 'ByEvent'
* otherwise.
*/
void KAlarmDirResource::setCompatibility(bool writeAttr)
{
    KAlarm::Calendar::Compat oldCompatibility = mCompatibility;
    if (mEvents.isEmpty())
        mCompatibility = KAlarm::Calendar::Current;
    else
    {
        bool first = true;
        foreach (const KAEvent& event, mEvents)
        {
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
        Collection c(mCollectionId);
        if (c.isValid())
            KAlarmResourceCommon::setCollectionCompatibility(c, mCompatibility);
    }
}

AKONADI_AGENT_FACTORY(KAlarmDirResource, akonadi_kalarm_dir_resource)

#include "kalarmdirresource.moc"

// vim: et sw=4:
