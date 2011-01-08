/*
 *  kalarmresource.cpp  -  Akonadi resource for KAlarm
 *  Program:  kalarm
 *  Copyright © 2009-2011 by David Jarvie <djarvie@kde.org>
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
#include "kalarmresourcecommon.h"
#include "collectionattribute.h"
#include "eventattribute.h"
#include "kacalendar.h"
#include "kaevent.h"

#include <akonadi/agentfactory.h>
#include <akonadi/attributefactory.h>
#include <akonadi/collectionmodifyjob.h>

#include <kcalcore/memorycalendar.h>
#include <kcalcore/incidence.h>

#include <klocale.h>
#include <kdebug.h>

using namespace Akonadi;
using namespace Akonadi_KAlarm_Resource;
using KAlarmResourceCommon::errorMessage;
using KAlarm::CollectionAttribute;
using KAlarm::EventAttribute;


KAlarmResource::KAlarmResource(const QString& id)
    : ICalResourceBase(id),
      mCompatibility(KAlarm::Calendar::Incompatible)
{
    kDebug(5950) << id;
    KAlarmResourceCommon::initialise(this);
    initialise(KAlarmResourceCommon::mimeTypes(id), "kalarm");
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
    mCompatibility = KAlarmResourceCommon::getCompatibility(fileStorage());
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
        emit error(errorMessage(KAlarmResourceCommon::UidNotFound, rid));
        return false;
    }

    if (kcalEvent->alarms().isEmpty())
    {
        kWarning() << "KCalCore::Event has no alarms:" << rid;
        emit error(errorMessage(KAlarmResourceCommon::EventNoAlarms, rid));
        return false;
    }

    KAEvent event(kcalEvent);
    QString mime = KAlarm::CalEvent::mimeType(event.category());
    if (mime.isEmpty())
    {
        kWarning() << "KAEvent has no alarms:" << rid;
        emit error(errorMessage(KAlarmResourceCommon::EventNoAlarms, rid));
        return false;
    }
    event.setCompatibility(mCompatibility);
    Item newItem = KAlarmResourceCommon::retrieveItem(item, event);
    itemRetrieved(newItem);
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
        kWarning() << "Calendar not in current format";
        cancelTask(errorMessage(KAlarmResourceCommon::NotCurrentFormat));
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

    KCalCore::Incidence::Ptr incidence = calendar()->incidence(item.remoteId());
    if (incidence)
    {
        if (incidence->isReadOnly())
        {
            kWarning() << "Event is read only:" << event.id();
            cancelTask(errorMessage(KAlarmResourceCommon::EventReadOnly, event.id()));
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
    KAlarmResourceCommon::setCollectionCompatibility(collection, mCompatibility);

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

AKONADI_AGENT_FACTORY(KAlarmResource, akonadi_kalarm_resource)

#include "kalarmresource.moc"

// vim: et sw=4:
