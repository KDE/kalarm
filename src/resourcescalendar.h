/*
 *  resourcescalendar.h  -  KAlarm calendar resources access
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

#include "resources/resource.h"

#include <KAlarmCal/KAEvent>

#include <QHash>
#include <QObject>

class EventId;

using namespace KAlarmCal;


/** Provides read and write access to resource calendars.
 *  This class provides the definitive access to events for the application.
 *  When events are added, modified or deleted, additional processing is
 *  performed beyond what the raw Resource classes do, to:
 *  - keep track of which events are to be triggered first in each resource.
 *  - keep track of whether any events are disabled.
 *  - control the triggering of repeat-at-login alarms.
 */
class ResourcesCalendar : public QObject
{
    Q_OBJECT
public:
    ~ResourcesCalendar() override;

    /** Create the resources calendar, to enable signal/slot connections to be made to it. */
    static ResourcesCalendar* create(const QByteArray& appName, const QByteArray& appVersion);

    /** Start resource calendar processing. */
    void                    start();

    static void             terminate();
    static KAEvent          earliestAlarm();
    static void             setAlarmPending(const KAEvent&, bool pending = true);
    static bool             haveDisabledAlarms()       { return mHaveDisabledAlarms; }
    static void             disabledChanged(const KAEvent&);
    using QObject::event;
    static KAEvent          event(const EventId& uniqueId, bool findUniqueId = false);
    static KAEvent          templateEvent(const QString& templateName);
    static QVector<KAEvent> events(const QString& uniqueId);
    static QVector<KAEvent> events(const Resource&, CalEvent::Types = CalEvent::EMPTY);
    static QVector<KAEvent> events(CalEvent::Types s = CalEvent::EMPTY);
    static bool             addEvent(KAEvent&, Resource&, QWidget* promptparent = nullptr, bool useEventID = false, bool noPrompt = false, bool* cancelled = nullptr);
    static bool             modifyEvent(const EventId& oldEventId, KAEvent& newEvent);
    static KAEvent          updateEvent(const KAEvent&);
    static bool             deleteEvent(const KAEvent&, Resource&, bool save = false);
    static void             purgeEvents(const QVector<KAEvent>&);
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
    static CalEvent::Type deleteEventInternal(const KAEvent&, Resource&, bool deleteFromResource = true);
    static CalEvent::Type deleteEventInternal(const QString& eventID, const KAEvent&, Resource&,
                                              bool deleteFromResource = true);
    void                  removeKAEvents(ResourceId, bool closing = false,
                                         CalEvent::Types = CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
    static QVector<KAEvent> events(CalEvent::Types, const Resource&);
    void                  findEarliestAlarm(const Resource&);
    void                  checkForDisabledAlarms();
    void                  checkForDisabledAlarms(bool oldEnabled, bool newEnabled);
    static QVector<KAEvent> eventsForResource(const Resource&, const QSet<QString>& eventIds);

    static ResourcesCalendar* mInstance;   // the unique instance

    typedef QHash<ResourceId, QSet<QString>> ResourceMap;  // event IDs for each resource
    typedef QHash<ResourceId, QString> EarliestMap;  // event ID of earliest alarm, for each resource

    static ResourceMap    mResourceMap;
    static EarliestMap    mEarliestAlarm;      // alarm with earliest trigger time, by resource
    static QSet<QString>  mPendingAlarms;      // IDs of alarms which are currently being processed after triggering
    static bool           mIgnoreAtLogin;      // ignore new/updated repeat-at-login alarms
    static bool           mHaveDisabledAlarms; // there is at least one individually disabled alarm
};

#endif // RESOURCESCALENDAR_H

// vim: et sw=4:
