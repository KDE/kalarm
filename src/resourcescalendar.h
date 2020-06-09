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
    KAEvent*              earliestAlarm() const;
    void                  setAlarmPending(KAEvent*, bool pending = true);
    bool                  haveDisabledAlarms() const   { return mHaveDisabledAlarms; }
    void                  disabledChanged(const KAEvent*);
    using QObject::event;
    KAEvent*              event(const EventId& uniqueId, bool findUniqueId = false);
    KAEvent*              templateEvent(const QString& templateName);
    KAEvent::List         events(const QString& uniqueId) const;
    KAEvent::List         events(const Resource&, CalEvent::Types = CalEvent::EMPTY) const;
    KAEvent::List         events(CalEvent::Types s = CalEvent::EMPTY) const;
    bool                  eventReadOnly(const QString& eventId) const;
    bool                  addEvent(KAEvent&, Resource&, QWidget* promptparent = nullptr, bool useEventID = false, bool noPrompt = false, bool* cancelled = nullptr);
    bool                  modifyEvent(const EventId& oldEventId, KAEvent& newEvent);
    KAEvent*              updateEvent(const KAEvent&);
    bool                  deleteEvent(const KAEvent&, Resource&, bool save = false);
    void                  purgeEvents(const KAEvent::List&);
    void                  adjustStartOfDay();

    static void           initialise();
    static void           terminate();
    static ResourcesCalendar* instance()     { return mInstance; }
    static KAEvent*       getEvent(const EventId&);

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
    typedef QMap<ResourceId, KAEvent::List> ResourceMap;  // id = invalid for display calendar
    typedef QHash<EventId, KAEvent*> KAEventMap;  // indexed by resource and event UID
    typedef QMap<ResourceId, KAEvent*> EarliestMap;

    ResourcesCalendar();
    void                  addNewEvent(const Resource&, KAEvent*, bool replace = false);
    CalEvent::Type        deleteEventInternal(const KAEvent&, Resource&, bool deleteFromResources = true);
    CalEvent::Type        deleteEventInternal(const QString& eventID, const KAEvent&, Resource&,
                                              bool deleteFromResources = true);
    bool                  removeKAEvents(ResourceId, bool closing = false, CalEvent::Types = CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
    KAEvent::List         events(CalEvent::Types, const Resource&) const;
    void                  findEarliestAlarm(const Resource&);
    void                  findEarliestAlarm(ResourceId);
    void                  checkForDisabledAlarms();
    void                  checkForDisabledAlarms(bool oldEnabled, bool newEnabled);

    static ResourcesCalendar* mInstance;   // the unique instance

    ResourceMap           mResourceMap;
    KAEventMap            mEventMap;           // lookup of all events by UID
    EarliestMap           mEarliestAlarm;      // alarm with earliest trigger time, by resource
    QSet<QString>         mPendingAlarms;      // IDs of alarms which are currently being processed after triggering
    bool                  mIgnoreAtLogin {false}; // ignore new/updated repeat-at-login alarms
    bool                  mHaveDisabledAlarms {false}; // there is at least one individually disabled alarm
};

#endif // RESOURCESCALENDAR_H

// vim: et sw=4:
