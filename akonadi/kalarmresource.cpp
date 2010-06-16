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
#include "kalarmmimetypevisitor.h"
#include "kaevent.h"

#include <akonadi/attributefactory.h>

#include <kcal/calendarlocal.h>
#include <kcal/incidence.h>

#include <klocale.h>
#include <kdebug.h>

using namespace Akonadi;
using namespace KCal;
using KAlarm::CollectionAttribute;
using KAlarm::EventAttribute;


KAlarmResource::KAlarmResource(const QString& id)
    : ICalResourceBase(id),
      mMimeVisitor(new KAlarmMimeTypeVisitor()),
      mCompatibility(KACalendar::Incompatible)
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
* Reimplemented to read data from the given file.
* The file is always local; loading from the network is done automatically if
* needed.
*/
bool KAlarmResource::readFromFile(const QString& fileName)
{
    if (!ICalResourceBase::readFromFile(fileName))
        return false;
    QString versionString;
    int version = KACalendar::checkCompatibility(*calendar(), fileName, versionString);
    mCompatibility = (version < 0) ? KACalendar::Incompatible  // calendar is not in KAlarm format, or is in a future format
                   : (version > 0) ? KACalendar::Convertible   // calendar is in an out of date format
                   :                 KACalendar::Current;      // calendar is in the current format
    return true;
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
    const KCal::Event* kcalEvent = calendar()->event(rid);
    if (!kcalEvent)
    {
        emit error(i18n("Event with uid '%1' not found.", rid));
        return false;
    }

    if (kcalEvent->alarms().isEmpty())
    {
        emit error(i18n("Event with uid '%1' contains no usable alarms.", rid));
        return false;
    }

    KAEvent event(kcalEvent);
    QString mime = mimeType(event);
    if (mime.isEmpty())
    {
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
* Store the event in the calendar.
*/
void KAlarmResource::itemAdded(const Akonadi::Item& item, const Akonadi::Collection&)
{
    if (!checkItemAddedChanged<KAEvent>(item, CheckForAdded))
        return;
    if (mCompatibility != KACalendar::Current)
    {
        cancelTask(i18nc("@info", "Calendar is not in current KAlarm format."));
        return;
    }
    KAEvent event = item.payload<KAEvent>();
    KCal::Event* kcalEvent = new KCal::Event;
    event.updateKCalEvent(kcalEvent, false);
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
    if (mCompatibility != KACalendar::Current)
    {
        cancelTask(i18nc("@info", "Calendar is not in current KAlarm format."));
        return;
    }
    KAEvent event = item.payload<KAEvent>();
    if (item.remoteId() != event.id())
    {
        cancelTask(i18n("Item ID %1 differs from payload ID %2.", item.remoteId(), event.id()));
        return;
    }
    KCal::Incidence* incidence = calendar()->incidence(item.remoteId());
    if (incidence)
    {
        if (incidence->isReadOnly())
        {
            cancelTask(i18nc("@info", "Event with uid '%1' is read only", event.id()));
            return;
        }
        if (!mMimeVisitor->isEvent(incidence))
        {
            calendar()->deleteIncidence(incidence);   // it's not an Event
            incidence = 0;
        }
        else
        {
            event.updateKCalEvent(static_cast<KCal::Event*>(incidence), false);
            calendar()->setModified(true);
        }
    }
    if (!incidence)
    {
        // not in the calendar yet, should not happen -> add it
        KCal::Event* kcalEvent = new KCal::Event;
        event.updateKCalEvent(kcalEvent, false);
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

    // Retrieve events from the calendar
    Event::List events = calendar()->events();
    Item::List items;
    foreach (const Event* kcalEvent, events)
    {
        if (kcalEvent->alarms().isEmpty())
            continue;    // ignore events without alarms

        KAEvent event(kcalEvent);
        QString mime = mimeType(event);
        if (mime.isEmpty())
            continue;   // event has no usable alarms
 
        Item item(mime);
        item.setRemoteId(kcalEvent->uid());
        item.setPayload(event);
#ifdef __GNUC__
#warning Check that commandError value is retained (in EventAttribute)
#endif
        items << item;
    }
    itemsRetrieved(items);
}

QString KAlarmResource::mimeType(const KAEvent& event)
{
    if (event.isValid())
    {
        switch (event.category())
        {
            case KACalEvent::ACTIVE:    return KAlarm::MIME_ACTIVE;
            case KACalEvent::ARCHIVED:  return KAlarm::MIME_ARCHIVED;
            case KACalEvent::TEMPLATE:  return KAlarm::MIME_TEMPLATE;
            default:
                break;
        }
    }
    return QString();
}

AKONADI_RESOURCE_MAIN(KAlarmResource)

#include "kalarmresource.moc"

// vim: et sw=4:
