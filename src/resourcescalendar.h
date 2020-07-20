/*
 *  resourcescalendar.h  -  KAlarm resources calendar file access
 *  Program:  kalarm
 *  Copyright © 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef RESOURCESCALENDAR_H
#define RESOURCESCALENDAR_H

#include "eventid.h"
#include "resources/resource.h"

#include <KAlarmCal/KAEvent>

#include <QHash>
#include <QObject>

using namespace KAlarmCal;


/** Provides read and write access to resource calendars.
 *  Either vCalendar or iCalendar files may be read, but the calendar is saved
 *  only in iCalendar format to avoid information loss.
 */
class ResourcesCalendar : public QObject
{
    Q_OBJECT
public:
    ~ResourcesCalendar() override;
    KAEvent               earliestAlarm();
    void                  setAlarmPending(const KAEvent&, bool pending = true);
    bool                  haveDisabledAlarms() const   { return mHaveDisabledAlarms; }
    void                  disabledChanged(const KAEvent&);
    using QObject::event;
    KAEvent               event(const EventId& uniqueId, bool findUniqueId = false);
    KAEvent               templateEvent(const QString& templateName);
    QVector<KAEvent>      events(const QString& uniqueId) const;
    QVector<KAEvent>      events(const Resource&, CalEvent::Types = CalEvent::EMPTY) const;
    QVector<KAEvent>      events(CalEvent::Types s = CalEvent::EMPTY) const;
    bool                  eventReadOnly(const QString& eventId) const;
    bool                  addEvent(KAEvent&, Resource&, QWidget* promptparent = nullptr, bool useEventID = false, bool noPrompt = false, bool* cancelled = nullptr);
    bool                  modifyEvent(const EventId& oldEventId, KAEvent& newEvent);
    KAEvent               updateEvent(const KAEvent&);
    bool                  deleteEvent(const KAEvent&, Resource&, bool save = false);
    void                  purgeEvents(const QVector<KAEvent>&);

    static void           initialise();
    static void           terminate();
    static ResourcesCalendar* instance()     { return mInstance; }

Q_SIGNALS:
    void                  earliestAlarmChanged();
    void                  haveDisabledAlarmsChanged(bool haveDisabled);
    void                  atLoginEventAdded(const KAEvent&);

private Q_SLOTS:
    void                  slotResourceSettingsChanged(Resource&, ResourceType::Changes);
    void                  slotResourcesPopulated();
    void                  slotResourceAdded(Resource&);
    void                  slotEventsAdded(Resource&, const QList<KAEvent>&);
    void                  slotEventsToBeRemoved(Resource&, const QList<KAEvent>&);
    void                  slotEventUpdated(Resource&, const KAEvent&);
private:
    ResourcesCalendar();
    CalEvent::Type        deleteEventInternal(const KAEvent&, Resource&, bool deleteFromResource = true);
    CalEvent::Type        deleteEventInternal(const QString& eventID, const KAEvent&, Resource&,
                                              bool deleteFromResource = true);
    void                  removeKAEvents(ResourceId, bool closing = false, CalEvent::Types = CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
    QVector<KAEvent>      events(CalEvent::Types, const Resource&) const;
    void                  findEarliestAlarm(const Resource&);
    void                  checkForDisabledAlarms();
    void                  checkForDisabledAlarms(bool oldEnabled, bool newEnabled);
    QVector<KAEvent>      eventsForResource(const Resource&, const QSet<QString>& eventIds) const;

    static ResourcesCalendar* mInstance;   // the unique instance

    typedef QHash<ResourceId, QSet<QString>> ResourceMap;  // event IDs for each resource
    typedef QHash<ResourceId, QString> EarliestMap;  // event ID of earliest alarm, for each resource

    ResourceMap           mResourceMap;
    EarliestMap           mEarliestAlarm;      // alarm with earliest trigger time, by resource
    QSet<QString>         mPendingAlarms;      // IDs of alarms which are currently being processed after triggering
    bool                  mIgnoreAtLogin {false}; // ignore new/updated repeat-at-login alarms
    bool                  mHaveDisabledAlarms {false}; // there is at least one individually disabled alarm
};

#endif // RESOURCESCALENDAR_H

// vim: et sw=4:
