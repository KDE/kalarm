/*
 *  kalarmresource.cpp  -  Akonadi resource for KAlarm
 *  Program:  kalarm
 *  Copyright © 2009,2010 by David Jarvie <djarvie@kde.org>
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

#include "kalarmresource.h"
#include "collectionattribute.h"
#include "eventattribute.h"
#include "kacalendar.h"
#include "kaevent.h"
#include "settingsadaptor.h"

#include <akonadi/agentfactory.h>
#include <akonadi/attributefactory.h>
#include <akonadi/collectionmodifyjob.h>

#include <kcalcore/memorycalendar.h>
#include <kcalcore/incidence.h>

#include <klocale.h>
#include <kdebug.h>

using namespace Akonadi;
using KAlarm::CollectionAttribute;
using KAlarm::EventAttribute;


KAlarmResource::KAlarmResource(const QString& id)
    : ICalResourceBase(id),
      mCompatibility(KAlarm::Calendar::Incompatible)
{
    // Set a default start-of-day time for date-only alarms.
    KAEvent::setStartOfDay(QTime(0,0,0));

    QStringList mimeTypes;
    if (id.contains("_active"))
        mimeTypes << KAlarm::MIME_ACTIVE;
    else if (id.contains("_archived"))
        mimeTypes << KAlarm::MIME_ARCHIVED;
    else if (id.contains("_template"))
        mimeTypes << KAlarm::MIME_TEMPLATE;
    else
        mimeTypes << KAlarm::MIME_BASE
                  << KAlarm::MIME_ACTIVE << KAlarm::MIME_ARCHIVED << KAlarm::MIME_TEMPLATE;
    initialise(mimeTypes, "kalarm");

    AttributeFactory::registerAttribute<CollectionAttribute>();
    AttributeFactory::registerAttribute<EventAttribute>();
}

KAlarmResource::~KAlarmResource()
{
}

/******************************************************************************
* Customize the configuration dialog before it is displayed.
*/
void KAlarmResource::customizeConfigDialog(SingleFileResourceConfigDialog<Settings>* dlg)
{
    ICalResourceBase::customizeConfigDialog(dlg);
    dlg->setMonitorEnabled(false);
    QString title;
    if (identifier().contains("_active"))
        title = i18nc("@title:window", "Select Active Alarm Calendar");
    else if (identifier().contains("_archived"))
        title = i18nc("@title:window", "Select Archived Alarm Calendar");
    else if (identifier().contains("_template"))
        title = i18nc("@title:window", "Select Alarm Template Calendar");
    else
        return;
    dlg->setCaption(title);
}

/******************************************************************************
* Reimplemented to read data from the given file.
* The file is always local; loading from the network is done automatically if
* needed.
*/
bool KAlarmResource::readFromFile(const QString& fileName)
{
    if (!ICalResourceBase::readFromFile(fileName))
        return false;
    if (calendar()->incidences().isEmpty())
    {
        // It's a new file. Set up the KAlarm custom property.
        KAlarm::Calendar::setKAlarmVersion(calendar());
    }
    QString versionString;
    int version = KAlarm::Calendar::checkCompatibility(fileStorage(), versionString);
    mCompatibility = (version < 0) ? KAlarm::Calendar::Incompatible  // calendar is not in KAlarm format, or is in a future format
                   : (version > 0) ? KAlarm::Calendar::Convertible   // calendar is in an out of date format
                   :                 KAlarm::Calendar::Current;      // calendar is in the current format
    return true;
}

/******************************************************************************
* Reimplemented to write data to the given file.
* The file is always local.
*/
bool KAlarmResource::writeToFile(const QString& fileName)
{
#ifdef __GNUC__
#warning Crashes if not a local file
#endif
    if (calendar()->incidences().isEmpty())
    {
        // It's an empty file. Set up the KAlarm custom property.
        KAlarm::Calendar::setKAlarmVersion(calendar());
    }
    return ICalResourceBase::writeToFile(fileName);
}

/******************************************************************************
* Retrieve an event from the calendar, whose uid and Akonadi id are given by
* 'item' (item.remoteId() and item.id() respectively).
* Set the event into a new item's payload, and signal its retrieval by calling
* itemRetrieved(newitem).
*/
bool KAlarmResource::doRetrieveItem(const Akonadi::Item& item, const QSet<QByteArray>& parts)
{
    Q_UNUSED(parts);
    const QString rid = item.remoteId();
    const KCalCore::Event::Ptr kcalEvent = calendar()->event(rid);
    if (!kcalEvent)
    {
        kWarning() << "Event not found:" << rid;
        emit error(i18n("Event with uid '%1' not found.", rid));
        return false;
    }

    if (kcalEvent->alarms().isEmpty())
    {
        kWarning() << "KCalCore::Event has no alarms:" << rid;
        emit error(i18n("Event with uid '%1' contains no usable alarms.", rid));
        return false;
    }

    KAEvent event(kcalEvent);
    QString mime = KAlarm::CalEvent::mimeType(event.category());
    if (mime.isEmpty())
    {
        kWarning() << "KAEvent has no alarms:" << rid;
        emit error(i18n("Event with uid '%1' contains no usable alarms.", rid));
        return false;
    }
    event.setItemId(item.id());
    if (item.hasAttribute<EventAttribute>())
        event.setCommandError(item.attribute<EventAttribute>()->commandError());

    Item i = item;
    i.setMimeType(mime);
    i.setPayload<KAEvent>(event);
    itemRetrieved(i);
    return true;
}

/******************************************************************************
* Called when an item has been added to the collection.
* Store the event in the calendar, and set its Akonadi remote ID to the
* KAEvent's UID.
*/
void KAlarmResource::itemAdded(const Akonadi::Item& item, const Akonadi::Collection&)
{
    if (!checkItemAddedChanged<KAEvent>(item, CheckForAdded))
        return;
    if (mCompatibility != KAlarm::Calendar::Current)
    {
        cancelTask(i18nc("@info", "Calendar is not in current KAlarm format."));
        return;
    }
    KAEvent event = item.payload<KAEvent>();
    KCalCore::Event::Ptr kcalEvent(new KCalCore::Event);
    event.updateKCalEvent(kcalEvent, KAEvent::UID_SET);
    calendar()->addIncidence(kcalEvent);

    Item it(item);
    it.setRemoteId(kcalEvent->uid());
    scheduleWrite();
    changeCommitted(it);
}

/******************************************************************************
* Called when an item has been changed.
* Store the changed event in the calendar, and delete the original event.
*/
void KAlarmResource::itemChanged(const Akonadi::Item& item, const QSet<QByteArray>& parts)
{
    Q_UNUSED(parts)
    if (!checkItemAddedChanged<KAEvent>(item, CheckForChanged))
        return;
    if (mCompatibility != KAlarm::Calendar::Current)
    {
        kWarning() << "Calendar not in current format";
        cancelTask(i18nc("@info", "Calendar is not in current KAlarm format."));
        return;
    }
    KAEvent event = item.payload<KAEvent>();
    if (item.remoteId() != event.id())
    {
        kWarning() << "Item ID" << item.remoteId() << "differs from payload ID" << event.id();
        cancelTask(i18n("Item ID %1 differs from payload ID %2.", item.remoteId(), event.id()));
        return;
    }
    KCalCore::Incidence::Ptr incidence = calendar()->incidence(item.remoteId());
    if (incidence)
    {
        if (incidence->isReadOnly())
        {
            kWarning() << "Event is read only:" << event.id();
            cancelTask(i18nc("@info", "Event with uid '%1' is read only", event.id()));
            return;
        }
        if (incidence->type() == KCalCore::Incidence::TypeEvent)
        {
            calendar()->deleteIncidence(incidence);   // it's not an Event
            incidence.clear();
        }
        else
        {
            KCalCore::Event::Ptr ev(incidence.staticCast<KCalCore::Event>());
            event.updateKCalEvent(ev, KAEvent::UID_SET);
kDebug()<<"KAEvent enabled="<<event.enabled();
            calendar()->setModified(true);
        }
    }
    if (!incidence)
    {
        // not in the calendar yet, should not happen -> add it
        KCalCore::Event::Ptr kcalEvent(new KCalCore::Event);
        event.updateKCalEvent(kcalEvent, KAEvent::UID_SET);
        calendar()->addIncidence(kcalEvent);
    }
    scheduleWrite();
    changeCommitted(item);
}

/******************************************************************************
* Retrieve all events from the calendar, and set each into a new item's
* payload. Items are identified by their remote IDs. The Akonadi ID is not
* used.
* Signal the retrieval of the items by calling itemsRetrieved(items), which
* updates Akonadi with any changes to the items. itemsRetrieved() compares
* the new and old items, matching them on the remoteId(). If the flags or
* payload have changed, or the Item has any new Attributes, the Akonadi
* storage is updated.
*/
void KAlarmResource::doRetrieveItems(const Akonadi::Collection& collection)
{
    // Set the collection's compatibility status
    Collection col = collection;
    CollectionAttribute* attr = col.attribute<CollectionAttribute>(Collection::AddIfMissing);
    attr->setCompatibility(mCompatibility);
    CollectionModifyJob* job = new CollectionModifyJob(col, this);
    connect(job, SIGNAL(result(KJob*)), this, SLOT(modifyCollectionJobDone(KJob*)));

    // Retrieve events from the calendar
    KCalCore::Event::List events = calendar()->events();
    Item::List items;
    foreach (const KCalCore::Event::Ptr& kcalEvent, events)
    {
        if (kcalEvent->alarms().isEmpty())
        {
            kWarning() << "KCalCore::Event has no alarms:" << kcalEvent->uid();
            continue;    // ignore events without alarms
        }

        KAEvent event(kcalEvent);
        QString mime = KAlarm::CalEvent::mimeType(event.category());
        if (mime.isEmpty())
        {
            kWarning() << "KAEvent has no alarms:" << event.id();
            continue;   // event has no usable alarms
        }
 
        Item item(mime);
        item.setRemoteId(kcalEvent->uid());
        item.setPayload(event);
        items << item;
    }
    itemsRetrieved(items);
}

/******************************************************************************
* Called when a collection modification job has completed, to report any error.
*/
void KAlarmResource::modifyCollectionJobDone(KJob* j)
{
    if (j->error())
    {
        Collection collection = static_cast<CollectionModifyJob*>(j)->collection();
        kError() << "Error: collection id" << collection.id() << ":" << j->errorString();
    }
}

AKONADI_AGENT_FACTORY(KAlarmResource, akonadi_kalarm_resource)

#include "kalarmresource.moc"

// vim: et sw=4:
