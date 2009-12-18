/*
 *  akonadi_serializer_kalarm.cpp  -  Akonadi resource serializer for KAlarm
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

#include "akonadi_serializer_kalarm.h"
#include "kalarmmimetypevisitor.h"
#include "kaeventdata.h"

#include <QtCore/qplugin.h>

#include <akonadi/item.h>
#include <kdebug.h>
#include <boost/shared_ptr.hpp>

static QLatin1String MIME_ACTIVE("application/x-vnd.kde.alarms.active");
static QLatin1String MIME_ARCHIVED("application/x-vnd.kde.alarms.archived");
static QLatin1String MIME_TEMPLATE("application/x-vnd.kde.alarms.template");

typedef boost::shared_ptr<KAEventData> EventPtr;

using namespace Akonadi;

bool SerializerPluginKAlarm::deserialize(Item& item, const QByteArray& label, QIODevice& data, int version)
{
    Q_UNUSED(version);

    if (label != Item::FullPayload)
        return false;

    KCal::Incidence* i = mFormat.fromString(QString::fromUtf8(data.readAll()));
    if (!i)
    {
        kWarning(5263) << "Failed to parse incidence!";
        data.seek(0);
        kWarning(5263) << QString::fromUtf8(data.readAll());
        return false;
    }
    KAlarmMimeTypeVisitor mv;
    if (!mv.isEvent(i))
    {
        kWarning(5263) << "Incidence with uid" << i->uid() << "is not an Event!";
        data.seek(0);
        return false;
    }
    KAEventData* event = new KAEventData(0, static_cast<KCal::Event*>(i));
    QString mime = mimeType(event);
    if (mime.isEmpty())
    {
        kWarning(5263) << "Event with uid" << i->uid() << "contains no usable alarms!";
        delete event;
        data.seek(0);
        return false;
    }

    item.setMimeType(mime);
    item.setPayload<EventPtr>(EventPtr(event));
    return true;
}

void SerializerPluginKAlarm::serialize(const Item& item, const QByteArray& label, QIODevice& data, int& version)
{
    Q_UNUSED(version);

    if (label != Item::FullPayload || !item.hasPayload<EventPtr>())
        return;
    EventPtr e = item.payload<EventPtr>();
    KCal::Event* kcalEvent = new KCal::Event;
#ifdef __GNUC__
#warning Should updateKCalEvent() third parameter be true for archived events?
#endif
    e->updateKCalEvent(kcalEvent, false, false);
    QByteArray head = "BEGIN:VCALENDAR\nPRODID:";
    head += KAEventData::icalProductId();
    head += "\nVERSION:2.0\nX-KDE-KALARM-VERSION:";
    head += KAEventData::currentCalendarVersionString();
    head += '\n';
    data.write(head);
    data.write(mFormat.toString(kcalEvent).toUtf8());
    data.write("\nEND:VCALENDAR");
}

QString SerializerPluginKAlarm::mimeType(const KAEventData* event)
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

Q_EXPORT_PLUGIN2(akonadi_serializer_kalarm, SerializerPluginKAlarm)

#include "akonadi_serializer_kalarm.moc"

// vim: et sw=4:
