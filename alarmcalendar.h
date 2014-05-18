/*
 *  alarmcalendar.h  -  KAlarm calendar file access
 *  Program:  kalarm
 *  Copyright © 2001-2012 by David Jarvie <djarvie@kde.org>
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

#include "akonadimodel.h"
#include "eventid.h"

#include <kalarmcal/kaevent.h>

#include <AkonadiCore/collection.h>
#include <KCalCore/FileStorage>
#include <KCalCore/Event>
#include <QHash>
#include <kurl.h>
#include <QObject>


using namespace KAlarmCal;


/** Provides read and write access to calendar files and resources.
 *  Either vCalendar or iCalendar files may be read, but the calendar is saved
 *  only in iCalendar format to avoid information loss.
 */
class AlarmCalendar : public QObject
{
        Q_OBJECT
    public:
        virtual ~AlarmCalendar();
        bool                  valid() const         { return (mCalType == RESOURCES) || mUrl.isValid(); }
        CalEvent::Type        type() const          { return (mCalType == RESOURCES) ? CalEvent::EMPTY : mEventType; }
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
        KAEvent::List         atLoginAlarms() const;
        KCalCore::Event::Ptr  kcalEvent(const QString& uniqueID);   // if Akonadi, display calendar only
        KAEvent*              event(const EventId& uniqueId, bool checkDuplicates = false);
        KAEvent*              templateEvent(const QString& templateName);
        KAEvent::List         events(const QString& uniqueId) const;
        KAEvent::List         events(CalEvent::Types s = CalEvent::EMPTY) const  { return events(Akonadi::Collection(), s); }
        KAEvent::List         events(const Akonadi::Collection&, CalEvent::Types = CalEvent::EMPTY) const;
        KCalCore::Event::List kcalEvents(CalEvent::Type s = CalEvent::EMPTY);   // display calendar only
        bool                  eventReadOnly(Akonadi::Item::Id) const;
        Akonadi::Collection   collectionForEvent(Akonadi::Item::Id) const;
        bool                  addEvent(KAEvent&, QWidget* promptParent = 0, bool useEventID = false, Akonadi::Collection* = 0, bool noPrompt = false, bool* cancelled = 0);
        bool                  modifyEvent(const EventId& oldEventId, KAEvent& newEvent);
        KAEvent*              updateEvent(const KAEvent&);
        KAEvent*              updateEvent(const KAEvent*);
        bool                  deleteEvent(const KAEvent&, bool save = false);
        bool                  deleteDisplayEvent(const QString& eventID, bool save = false);
        void                  purgeEvents(const KAEvent::List&);
        bool                  isOpen();
        QString               path() const           { return (mCalType == RESOURCES) ? QString() : mUrl.prettyUrl(); }
        QString               urlString() const      { return (mCalType == RESOURCES) ? QString() : mUrl.url(); }
        void                  adjustStartOfDay();

        static bool           initialiseCalendars();
        static void           terminateCalendars();
        static AlarmCalendar* resources()            { return mResourcesCalendar; }
        static AlarmCalendar* displayCalendar()      { return mDisplayCalendar; }
        static AlarmCalendar* displayCalendarOpen();
        static KAEvent*       getEvent(const EventId&);
        static bool           importAlarms(QWidget*, Akonadi::Collection* = 0);
        static bool           exportAlarms(const KAEvent::List&, QWidget* parent);

    signals:
        void                  earliestAlarmChanged();
        void                  haveDisabledAlarmsChanged(bool haveDisabled);
        void                  atLoginEventAdded(const KAEvent&);
        void                  calendarSaved(AlarmCalendar*);

    private slots:
        void                  setAskResource(bool ask);
        void                  slotCollectionStatusChanged(const Akonadi::Collection&, AkonadiModel::Change,
                                                          const QVariant& value, bool inserted);
        void                  slotEventsAdded(const AkonadiModel::EventList&);
        void                  slotEventsToBeRemoved(const AkonadiModel::EventList&);
        void                  slotEventChanged(const AkonadiModel::Event&);
    private:
        enum CalType { RESOURCES, LOCAL_ICAL, LOCAL_VCAL };
        typedef QMap<Akonadi::Collection::Id, KAEvent::List> ResourceMap;  // id = invalid for display calendar
        typedef QMap<Akonadi::Collection::Id, KAEvent*> EarliestMap;
        typedef QHash<EventId, KAEvent*> KAEventMap;  // indexed by collection and event UID

        AlarmCalendar();
        AlarmCalendar(const QString& file, CalEvent::Type);
        bool                  saveCal(const QString& newFile = QString());
        bool                  isValid() const   { return mCalType == RESOURCES || mCalendarStorage; }
        void                  addNewEvent(const Akonadi::Collection&, KAEvent*, bool replace = false);
        CalEvent::Type        deleteEventInternal(const KAEvent&, bool deleteFromAkonadi = true);
        CalEvent::Type        deleteEventInternal(const KAEvent&, const Akonadi::Collection&,
                                                   bool deleteFromAkonadi = true);
        CalEvent::Type        deleteEventInternal(const QString& eventID, const KAEvent& = KAEvent(),
                                                   const Akonadi::Collection& = Akonadi::Collection(), bool deleteFromAkonadi = true);
        void                  updateDisplayKAEvents();
        void                  removeKAEvents(Akonadi::Collection::Id, bool closing = false, CalEvent::Types = CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
        void                  findEarliestAlarm(const Akonadi::Collection&);
        void                  findEarliestAlarm(Akonadi::Collection::Id);  //deprecated
        void                  checkForDisabledAlarms();
        void                  checkForDisabledAlarms(bool oldEnabled, bool newEnabled);

        static AlarmCalendar* mResourcesCalendar;  // the calendar resources
        static AlarmCalendar* mDisplayCalendar;    // the display calendar

        KCalCore::FileStorage::Ptr mCalendarStorage; // null pointer for Akonadi
        ResourceMap           mResourceMap;
        KAEventMap            mEventMap;           // lookup of all events by UID
        EarliestMap           mEarliestAlarm;      // alarm with earliest trigger time, by resource
        QList<QString>        mPendingAlarms;      // IDs of alarms which are currently being processed after triggering
        KUrl                  mUrl;                // URL of current calendar file
        KUrl                  mICalUrl;            // URL of iCalendar file
        QString               mLocalFile;          // calendar file, or local copy if it's a remote file
        CalType               mCalType;            // what type of calendar mCalendar is (resources/ical/vcal)
        CalEvent::Type        mEventType;         // what type of events the calendar file is for
        bool                  mOpen;               // true if the calendar file is open
        int                   mUpdateCount;        // nesting level of group of calendar update calls
        bool                  mUpdateSave;         // save() was called while mUpdateCount > 0
        bool                  mHaveDisabledAlarms; // there is at least one individually disabled alarm

        using QObject::event;   // prevent "hidden" warning
};

#endif // ALARMCALENDAR_H

// vim: et sw=4:
