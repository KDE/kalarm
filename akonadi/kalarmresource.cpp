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
#include "eventattribute.h"
#include "kalarmmimetypevisitor.h"
#include "kaevent.h"

#include <akonadi/attributefactory.h>

#include <kcal/calendarlocal.h>
#include <kcal/incidence.h>

#include <klocale.h>
#include <kdebug.h>

static QLatin1String MIME_ACTIVE("application/x-vnd.kde.alarms.active");
static QLatin1String MIME_ARCHIVED("application/x-vnd.kde.alarms.archived");
static QLatin1String MIME_TEMPLATE("application/x-vnd.kde.alarms.template");

using namespace Akonadi;
using namespace KCal;


KAlarmResource::KAlarmResource(const QString& id)
    : ICalResourceBase(id),
      mMimeVisitor(new KAlarmMimeTypeVisitor())
{
    // Set a default start-of-day time for date-only alarms.
    KAEvent::setStartOfDay(QTime(0,0,0));

    QStringList mimeTypes;
    if (id.contains("_active"))
        mimeTypes << MIME_ACTIVE;
    else if (id.contains("_archived"))
        mimeTypes << MIME_ARCHIVED;
    else if (id.contains("_template"))
        mimeTypes << MIME_TEMPLATE;
    else
        mimeTypes << QLatin1String("application/x-vnd.kde.alarms")
                  << MIME_ACTIVE << MIME_ARCHIVED << MIME_TEMPLATE;
    initialise(mimeTypes, "kalarm");

    AttributeFactory::registerAttribute<EventAttribute>();
}

KAlarmResource::~KAlarmResource()
{
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
        event.setCommandError(item.attribute<EventAttribute>()->commandError(), false);

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
    KAEvent event = item.payload<KAEvent>();
    if (item.remoteId() != event.id())
    {
        cancelTask(i18n("Item ID %1 differs from payload ID %2.", item.remoteId(), event.id()));
        return;
    }
    KCal::Incidence* incidence = calendar()->incidence(item.remoteId());
    if (incidence)
    {
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
* payload. The Akonadi id for each item will be a new one.
* Signal the retrieval of the items by calling itemsRetrieved(items), which
* updates Akonadi with the new item list, based on each item's remoteId().
*/
void KAlarmResource::doRetrieveItems(const Akonadi::Collection&)
{
    Event::List events = calendar()->events();
    Item::List items;
    foreach (Event* kcalEvent, events)
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
    if (event.valid())
    {
        switch (event.category())
        {
            case KCalEvent::ACTIVE:    return MIME_ACTIVE;
            case KCalEvent::ARCHIVED:  return MIME_ARCHIVED;
            case KCalEvent::TEMPLATE:  return MIME_TEMPLATE;
            default:
                break;
        }
    }
    return QString();
}

AKONADI_RESOURCE_MAIN(KAlarmResource)

#include "kalarmresource.moc"

// vim: et sw=4:
