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
#include <QUrl>

using namespace KAlarmCal;


/** Provides read and write access to calendar files and resources.
 *  Either vCalendar or iCalendar files may be read, but the calendar is saved
 *  only in iCalendar format to avoid information loss.
 */
class AlarmCalendar : public QObject
{
        Q_OBJECT
    public:
        ~AlarmCalendar() override;
        bool                  open();
        int                   load();
        bool                  reload();
        bool                  save();
        void                  close();
        void                  startUpdate();
        bool                  endUpdate();
        KAEvent*              earliestAlarm() const;
        void                  setAlarmPending(KAEvent*, bool pending = true);
        bool                  haveDisabledAlarms() const   { return mHaveDisabledAlarms; }
        void                  disabledChanged(const KAEvent*);
        KAEvent*              event(const EventId& uniqueId, bool findUniqueId = false);
        KAEvent*              templateEvent(const QString& templateName);
        KAEvent::List         events(const QString& uniqueId) const;
        KAEvent::List         events(CalEvent::Types s = CalEvent::EMPTY) const  { return events(Resource(), s); }
        KAEvent::List         events(const Resource&, CalEvent::Types = CalEvent::EMPTY) const;
        KCalendarCore::Event::Ptr  kcalEvent(const QString& uniqueID);   // display calendar only
        KCalendarCore::Event::List kcalEvents(CalEvent::Type s = CalEvent::EMPTY);   // display calendar only
        bool                  eventReadOnly(const QString& eventId) const;
        bool                  addEvent(KAEvent&, QWidget* promptparent = nullptr, bool useEventID = false, Resource* = nullptr, bool noPrompt = false, bool* cancelled = nullptr);
        bool                  modifyEvent(const EventId& oldEventId, KAEvent& newEvent);
        KAEvent*              updateEvent(const KAEvent&);
        KAEvent*              updateEvent(const KAEvent*);
        bool                  deleteEvent(const KAEvent&, bool save = false);
        bool                  deleteDisplayEvent(const QString& eventID, bool save = false);
        void                  purgeEvents(const KAEvent::List&);
        bool                  isOpen();
        void                  adjustStartOfDay();

        static bool           initialiseCalendars();
        static void           terminateCalendars();
        static AlarmCalendar* resources()            { return mResourcesCalendar; }
        static AlarmCalendar* displayCalendar()      { return mDisplayCalendar; }
        static AlarmCalendar* displayCalendarOpen();
        static KAEvent*       getEvent(const EventId&);

    Q_SIGNALS:
        void                  earliestAlarmChanged();
        void                  haveDisabledAlarmsChanged(bool haveDisabled);
        void                  atLoginEventAdded(const KAEvent&);
        void                  calendarSaved(AlarmCalendar*);

    private Q_SLOTS:
        void                  slotResourceSettingsChanged(Resource&, ResourceType::Changes);
        void                  slotResourcesPopulated();
        void                  slotResourceAdded(Resource&);
        void                  slotEventsAdded(Resource&, const QList<KAEvent>&);
        void                  slotEventsToBeRemoved(Resource&, const QList<KAEvent>&);
        void                  slotEventUpdated(Resource&, const KAEvent&);
    private:
        enum CalType { RESOURCES, LOCAL_ICAL, LOCAL_VCAL };
        typedef QMap<ResourceId, KAEvent::List> ResourceMap;  // id = invalid for display calendar
        typedef QMap<ResourceId, KAEvent*> EarliestMap;
        typedef QHash<EventId, KAEvent*> KAEventMap;  // indexed by resource and event UID

        AlarmCalendar();
        AlarmCalendar(const QString& file, CalEvent::Type);
        bool                  saveCal(const QString& newFile = QString());
        bool                  isValid() const   { return mCalType == RESOURCES || mDisplayCalStorage; }
        void                  addNewEvent(const Resource&, KAEvent*, bool replace = false);
        CalEvent::Type        deleteEventInternal(const KAEvent&, bool deleteFromResources = true);
        CalEvent::Type        deleteEventInternal(const KAEvent&, Resource&, bool deleteFromResources = true);
        CalEvent::Type        deleteEventInternal(const QString& eventID, const KAEvent&, Resource&,
                                                  bool deleteFromResources = true);
        void                  updateDisplayKAEvents();
        void                  removeKAEvents(ResourceId, bool closing = false, CalEvent::Types = CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
        void                  findEarliestAlarm(const Resource&);
        void                  findEarliestAlarm(ResourceId);  //deprecated
        void                  checkForDisabledAlarms();
        void                  checkForDisabledAlarms(bool oldEnabled, bool newEnabled);

        static AlarmCalendar* mResourcesCalendar;  // the calendar resources
        static AlarmCalendar* mDisplayCalendar;    // the display calendar
        
        ResourceMap           mResourceMap;
        KAEventMap            mEventMap;           // lookup of all events by UID
        EarliestMap           mEarliestAlarm;      // alarm with earliest trigger time, by resource
        QSet<QString>         mPendingAlarms;      // IDs of alarms which are currently being processed after triggering
        KCalendarCore::FileStorage::Ptr mDisplayCalStorage; // for display calendar; null if resources calendar
        QString               mDisplayCalPath;     // path of display calendar file
        QString               mDisplayICalPath;    // path of display iCalendar file
        CalType               mCalType;            // what type of calendar mCalendar is (resources/ical/vcal)
        CalEvent::Type        mEventType;          // what type of events the calendar file is for
        bool                  mOpen {false};       // true if the calendar file is open
        bool                  mIgnoreAtLogin {false}; // ignore new/updated repeat-at-login alarms
        int                   mUpdateCount {0};    // nesting level of group of calendar update calls
        bool                  mUpdateSave {false}; // save() was called while mUpdateCount > 0
        bool                  mHaveDisabledAlarms {false}; // there is at least one individually disabled alarm

        using QObject::event;   // prevent "hidden" warning
};

#endif // ALARMCALENDAR_H

// vim: et sw=4:
