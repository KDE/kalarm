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
using KAlarmResourceCommon::errorMessage;


KAlarmDirResource::KAlarmDirResource(const QString& id)
    : ResourceBase(id),
      mSettings(new Settings(componentData().config())),
      mCompatibility(KAlarm::Calendar::Incompatible)
{
    kDebug(5950) << id;
    KAlarmResourceCommon::initialise(this);

    // Set up the resource
    new KAlarmDirSettingsAdaptor(mSettings);
    DBusConnectionPool::threadConnection().registerObject(QLatin1String("/Settings"),
                                mSettings, QDBusConnection::ExportAdaptors);

    changeRecorder()->itemFetchScope().fetchFullPayload();
}

KAlarmDirResource::~KAlarmDirResource()
{
    kDebug(5950);
}

void KAlarmDirResource::aboutToQuit()
{
    kDebug(5950);
    mSettings->writeConfig();
}

void KAlarmDirResource::configure(WId windowId)
{
    kDebug(5950);
    SettingsDialog dlg(windowId, mSettings);
    if (dlg.exec())
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

// Load and parse data from each file in the directory.
bool KAlarmDirResource::loadFiles()
{
    kDebug(5950);
    mEvents.clear();

    QDirIterator it(directoryName());
    while (it.hasNext())
    {
        it.next();
        QString fileName = it.fileName();
        if (fileName != "." && fileName != ".." && fileName != "WARNING_README.txt")
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
    kDebug(5950);
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
    if (cancelIfReadOnly())
        return;
    if (mCompatibility != KAlarm::Calendar::Current)
    {
        kWarning() << "Calendar not in current format";
        cancelTask(errorMessage(KAlarmResourceCommon::NotCurrentFormat));
        return;
    }

    KAEvent event;
    if (item.hasPayload<KAEvent>())
        event = item.payload<KAEvent>();
    if (!event.isValid())
    {
        changeProcessed();
        return;
    }
    event.setCompatibility(KAlarm::Calendar::Current);

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
    if (cancelIfReadOnly())
        return;
    QMap<QString, KAEvent>::Iterator it = mEvents.find(item.remoteId());
    if (it != mEvents.end()
    &&  (it.value().isReadOnly() || it.value().compatibility() != KAlarm::Calendar::Current))
    {
        kWarning() << "Event is read only:" << item.remoteId();
        cancelTask(errorMessage(KAlarmResourceCommon::EventReadOnly, item.remoteId()));
        return;
    }

    QString errorMsg;
    KAEvent event = KAlarmResourceCommon::checkItemChanged(item, mCompatibility, errorMsg);
    if (!event.isValid())
    {
        if (errorMsg.isEmpty())
            changeProcessed();
        else
            cancelTask(errorMsg);
        return;
    }
    event.setCompatibility(KAlarm::Calendar::Current);

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
    if (cancelIfReadOnly())
        return;

    mEvents.remove(item.remoteId());
    QFile::remove(directoryFileName(item.remoteId()));

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
        kWarning(5950) << "Calendar is read-only:" << directoryName();
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
    kDebug(5950)<<fileName;
    FileStorage::Ptr fileStorage(new FileStorage(calendar, fileName, new ICalFormat()));
    if (!fileStorage->save())
    {
        emit error(i18nc("@info", "Failed to save event file: %1", fileName));
        cancelTask();
        return false;
    }
    return true;
}

void KAlarmDirResource::retrieveCollections()
{
    Collection c;
    c.setParentCollection(Collection::root());
    c.setRemoteId(directoryName());
    const QString display = mSettings->displayName();
    c.setName(display.isEmpty() ? name() : display );
    c.setContentMimeTypes(KAlarmResourceCommon::mimeTypes(identifier()));
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

        Item item(mime);
        item.setRemoteId(event.id());
        item.setPayload(event);
        items.append(item);
    }

    itemsRetrieved(items);
}

void KAlarmDirResource::collectionChanged(const Akonadi::Collection& collection)
{
    kDebug(5950);
    QString newName;
    EntityDisplayAttribute* attr = 0;
    if (collection.hasAttribute<EntityDisplayAttribute>())
    {
        attr = collection.attribute<EntityDisplayAttribute>();
        newName = attr->displayName();
    }
    const QString oldName = mSettings->displayName();
    if (newName != oldName)
    {
        mSettings->setDisplayName(newName);
        mSettings->writeConfig();
    }

    newName = collection.name();
    if (attr && !attr->displayName().isEmpty())
        newName = attr->displayName();
    if (newName != name())
        setName(newName);

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

void KAlarmDirResource::initializeDirectory() const
{
    QDir dir(directoryName());
    QString dirPath = dir.absolutePath();

    // If folder does not exist, create it
    if (!dir.exists())
    {
        kDebug(5950) << "Creating" << dirPath;
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

AKONADI_AGENT_FACTORY(KAlarmDirResource, akonadi_kalarm_dir_resource)

#include "kalarmdirresource.moc"

// vim: et sw=4:
