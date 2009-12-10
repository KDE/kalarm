/*
 *  kalarmresource.cpp  -  Akonadi resource for KAlarm
 *  Program:  kalarm
 *  Copyright © 2009 by David Jarvie <djarvie@kde.org>
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
#include "kalarmmimetypevisitor.h"
#include "kaeventdata.h"

#include <kcal/calendarlocal.h>
#include <kcal/incidence.h>

#include <klocale.h>
#include <kdebug.h>

#include <boost/shared_ptr.hpp>

static QLatin1String MIME_ACTIVE("application/x-vnd.kde.alarms.active");
static QLatin1String MIME_ARCHIVED("application/x-vnd.kde.alarms.archived");
static QLatin1String MIME_TEMPLATE("application/x-vnd.kde.alarms.template");

using namespace Akonadi;
using namespace KCal;

typedef boost::shared_ptr<KAEventData> EventPtr;

KAlarmResource::KAlarmResource(const QString& id)
    : ICalResourceBase(id, i18nc("Filedialog filter for *.ics *.ical", "KAlarm Calendar File")),
      mMimeVisitor(new KAlarmMimeTypeVisitor())
{
    // Set a default start-of-day time for date-only alarms.
    KAEventData::setStartOfDay(QTime(0,0,0));

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
}

KAlarmResource::~KAlarmResource()
{
}

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

    KAEventData* event = new KAEventData(0, kcalEvent);
    QString mime = mimeType(event);
    if (mime.isEmpty())
    {
        emit error(i18n("Event with uid '%1' contains no usable alarms.", rid));
        delete event;
        return false;
    }

    Item i = item;
    i.setMimeType(mime);
    i.setPayload<EventPtr>(EventPtr(event));
    itemRetrieved(i);
    return true;
}

void KAlarmResource::itemAdded(const Akonadi::Item&  item, const Akonadi::Collection&)
{
    if (!checkItemAddedChanged<EventPtr>(item, CheckForAdded))
        return;
    EventPtr e = item.payload<EventPtr>();
    KCal::Event* kcalEvent = new KCal::Event;
#ifdef __GNUC__
#warning Should updateKCalEvent() third parameter be true for archived events?
#endif
    e->updateKCalEvent(kcalEvent, false, false);
    calendar()->addIncidence(kcalEvent);

    Item it(item);
    it.setRemoteId(kcalEvent->uid());
    scheduleWrite();
    changeCommitted(it);
}

void KAlarmResource::itemChanged(const Akonadi::Item& item, const QSet<QByteArray>& parts)
{
    Q_UNUSED(parts)
    if (!checkItemAddedChanged<EventPtr>(item, CheckForChanged))
        return;
    EventPtr payload = item.payload<EventPtr>();
    if (item.remoteId() != payload->id())
    {
        cancelTask(i18n("Item ID %1 differs from payload ID %2.").arg(item.remoteId()).arg(payload->id()));
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
#ifdef __GNUC__
#warning Should updateKCalEvent() third parameter be true for archived events?
#endif
            payload->updateKCalEvent(static_cast<KCal::Event*>(incidence), false, false);
            calendar()->setModified(true);
        }
    }
    if (!incidence)
    {
        // not in the calendar yet, should not happen -> add it
        KCal::Event* kcalEvent = new KCal::Event;
#ifdef __GNUC__
#warning Should updateKCalEvent() third parameter be true for archived events?
#endif
        payload->updateKCalEvent(kcalEvent, false, false);
        calendar()->addIncidence(kcalEvent);
    }
    scheduleWrite();
    changeCommitted(item);
}

void KAlarmResource::doRetrieveItems(const Akonadi::Collection&)
{
    Event::List events = calendar()->events();
    Item::List items;
    foreach (Event* kcalEvent, events)
    {
        if (kcalEvent->alarms().isEmpty())
            continue;    // ignore events without alarms

        KAEventData* event = new KAEventData(0, kcalEvent);
        QString mime = mimeType(event);
        if (mime.isEmpty())
            continue;   // event has no usable alarms

        Item item(mime);
        item.setRemoteId(kcalEvent->uid());
        item.setPayload(EventPtr(event));
        items << item;
    }
    itemsRetrieved(items);
}

QString KAlarmResource::mimeType(const KAEventData* event)
{
    if (event->valid())
    {
        switch (event->category())
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
