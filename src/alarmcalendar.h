/*
 *  alarmcalendar.h  -  KAlarm calendar file access
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

#ifndef ALARMCALENDAR_H
#define ALARMCALENDAR_H

#include "eventid.h"
#include "resources/resource.h"

#include <KAlarmCal/KAEvent>

#include <KCalendarCore/FileStorage>
#include <KCalendarCore/Event>

#include <QHash>
#include <QObject>

using namespace KAlarmCal;


/** Base class to provide read and write access to calendar files and resources.
 *  Either vCalendar or iCalendar files may be read, but the calendar is saved
 *  only in iCalendar format to avoid information loss.
 */
class AlarmCalendar : public QObject
{
    Q_OBJECT
public:
    KAEvent*              event(const EventId& uniqueId);
    bool                  addEvent(KAEvent&, QWidget* promptparent = nullptr, bool useEventID = false, Resource* = nullptr, bool noPrompt = false, bool* cancelled = nullptr);
    void                  adjustStartOfDay();

    static void           initialise();

protected:
    typedef QMap<ResourceId, KAEvent::List> ResourceMap;  // id = invalid for display calendar
    typedef QHash<EventId, KAEvent*> KAEventMap;  // indexed by resource and event UID

    AlarmCalendar();
    virtual bool          isValid() const = 0;
    KAEvent::List         events(CalEvent::Types, const Resource&) const;
    virtual void          addNewEvent(const Resource&, KAEvent*, bool replace = false);
    KAEvent*              deleteEventBase(const QString& eventID, Resource&);
    bool                  removeKAEvents(ResourceId, CalEvent::Types);

    ResourceMap           mResourceMap;
    KAEventMap            mEventMap;           // lookup of all events by UID

    using QObject::event;   // prevent "hidden" warning
};

/** Provides read and write access to resource calendars.
 */
class ResourcesCalendar : public AlarmCalendar
{
    Q_OBJECT
public:
    ~ResourcesCalendar() override;
    bool                  reload();
    bool                  save();
    void                  close();
    KAEvent*              earliestAlarm() const;
    void                  setAlarmPending(KAEvent*, bool pending = true);
    bool                  haveDisabledAlarms() const   { return mHaveDisabledAlarms; }
    void                  disabledChanged(const KAEvent*);
    using AlarmCalendar::event;
    KAEvent*              event(const EventId& uniqueId, bool findUniqueId);
    KAEvent*              templateEvent(const QString& templateName);
    KAEvent::List         events(const QString& uniqueId) const;
    KAEvent::List         events(const Resource&, CalEvent::Types = CalEvent::EMPTY) const;
    KAEvent::List         events(CalEvent::Types s = CalEvent::EMPTY) const;
    bool                  eventReadOnly(const QString& eventId) const;
    bool                  addEvent(KAEvent&, QWidget* promptparent = nullptr, bool useEventID = false, Resource* = nullptr, bool noPrompt = false, bool* cancelled = nullptr);
    bool                  modifyEvent(const EventId& oldEventId, KAEvent& newEvent);
    KAEvent*              updateEvent(const KAEvent&);
    bool                  deleteEvent(const KAEvent&, bool save = false);
    void                  purgeEvents(const KAEvent::List&);

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
    typedef QMap<ResourceId, KAEvent*> EarliestMap;

    ResourcesCalendar();
    bool                  isValid() const override  { return true; }
    void                  addNewEvent(const Resource&, KAEvent*, bool replace = false) override;
    CalEvent::Type        deleteEventInternal(const KAEvent&, bool deleteFromResources = true);
    CalEvent::Type        deleteEventInternal(const KAEvent&, Resource&, bool deleteFromResources = true);
    CalEvent::Type        deleteEventInternal(const QString& eventID, const KAEvent&, Resource&,
                                              bool deleteFromResources = true);
    bool                  removeKAEvents(ResourceId, bool closing = false, CalEvent::Types = CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
    void                  findEarliestAlarm(const Resource&);
    void                  findEarliestAlarm(ResourceId);
    void                  checkForDisabledAlarms();
    void                  checkForDisabledAlarms(bool oldEnabled, bool newEnabled);

    static ResourcesCalendar* mInstance;   // the unique instance

    EarliestMap           mEarliestAlarm;      // alarm with earliest trigger time, by resource
    QSet<QString>         mPendingAlarms;      // IDs of alarms which are currently being processed after triggering
    bool                  mIgnoreAtLogin {false}; // ignore new/updated repeat-at-login alarms
    bool                  mHaveDisabledAlarms {false}; // there is at least one individually disabled alarm
};

/** Provides read and write access to the display calendar.
 */
class DisplayCalendar : public AlarmCalendar
{
    Q_OBJECT
public:
    ~DisplayCalendar() override;
    bool                  open();
    int                   load();
    bool                  reload();
    bool                  save();
    void                  close();
    KAEvent::List         events(CalEvent::Types = CalEvent::EMPTY) const;
    KCalendarCore::Event::Ptr  kcalEvent(const QString& uniqueID);
    KCalendarCore::Event::List kcalEvents(CalEvent::Type s = CalEvent::EMPTY);
    bool                  addEvent(KAEvent&);
    bool                  deleteEvent(const QString& eventID, bool save = false);
    bool                  isOpen() const         { return mOpen; }

    static void           initialise();
    static void           terminate();
    static DisplayCalendar* instance()      { return mInstance; }
    static DisplayCalendar* instanceOpen();

private:
    enum CalType { LOCAL_ICAL, LOCAL_VCAL };

    explicit DisplayCalendar(const QString& file);
    bool                  saveCal(const QString& newFile = QString());
    bool                  isValid() const override   { return mCalendarStorage; }
    void                  updateKAEvents();

    static DisplayCalendar* mInstance;    // the unique instance

    KCalendarCore::FileStorage::Ptr mCalendarStorage;
    QString               mDisplayCalPath;     // path of display calendar file
    QString               mDisplayICalPath;    // path of display iCalendar file
    CalType               mCalType;            // what type of calendar mCalendar is (ical/vcal)
    bool                  mOpen {false};       // true if the calendar file is open
};

#endif // ALARMCALENDAR_H

// vim: et sw=4:
