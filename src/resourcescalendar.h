/*
 *  resourcescalendar.h  -  KAlarm calendar resources access
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
    static void             initialise(const QByteArray& appName, const QByteArray& appVersion);
    static void             terminate();

    /** Return the active alarm with the earliest trigger time.
     *  @param nextTriggerTime       The next trigger time of the earliest alarm.
     *  @param excludeDisplayAlarms  Ignore display alarms.
     *  @return  The earliest alarm.
     */
    static KAEvent          earliestAlarm(KADateTime& nextTriggerTime, bool excludeDisplayAlarms = false);

    static void             setAlarmPending(const KAEvent&, bool pending = true);
    static bool             haveDisabledAlarms()       { return mHaveDisabledAlarms; }
    static void             disabledChanged(const KAEvent&);
    using QObject::event;
    static KAEvent          event(const EventId& uniqueId, bool findUniqueId = false);
    static KAEvent          templateEvent(const QString& templateName);
    static QVector<KAEvent> events(const QString& uniqueId);
    static QVector<KAEvent> events(const Resource&, CalEvent::Types = CalEvent::EMPTY);
    static QVector<KAEvent> events(CalEvent::Types s = CalEvent::EMPTY);

    /** Options for addEvent(). May be OR'ed together. */
    enum AddEventOption
    {
        NoOption         = 0,
        UseEventId       = 0x01,   // use event ID if it's provided
        NoResourcePrompt = 0x02,   // don't prompt for resource
        UseOnlyResource  = 0x04    // if there is only one enabled resource, use it as the default
    };
    Q_DECLARE_FLAGS(AddEventOptions, AddEventOption)

    static bool             addEvent(KAEvent&, Resource&, QWidget* promptparent = nullptr, AddEventOptions options = NoOption, bool* cancelled = nullptr);
    static bool             modifyEvent(const EventId& oldEventId, KAEvent& newEvent);
    static KAEvent          updateEvent(const KAEvent&, bool saveIfReadOnly = true);
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
    static EarliestMap    mEarliestAlarm;        // alarm with earliest trigger time, by resource
    static EarliestMap    mEarliestNonDispAlarm; // non-display alarm with earliest trigger time, by resource
    static QSet<QString>  mPendingAlarms;      // IDs of alarms which are currently being processed after triggering
    static bool           mIgnoreAtLogin;      // ignore new/updated repeat-at-login alarms
    static bool           mHaveDisabledAlarms; // there is at least one individually disabled alarm
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ResourcesCalendar::AddEventOptions)

#endif // RESOURCESCALENDAR_H

// vim: et sw=4:
