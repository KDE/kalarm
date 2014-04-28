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

#ifdef USE_AKONADI
#include "akonadimodel.h"
#include "eventid.h"
#else
#include "alarmresources.h"
#endif

#include <kalarmcal/kaevent.h>

#ifdef USE_AKONADI
#include <AkonadiCore/collection.h>
#include <KCalCore/filestorage.h>
#include <KCalCore/event.h>
#include <QHash>
#endif
#include <kurl.h>
#include <QObject>

#ifndef USE_AKONADI
namespace KCal {
    class Calendar;
    class CalendarLocal;
}
class AlarmResource;
class ProgressDialog;
#endif

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
#ifndef USE_AKONADI
        void                  loadResource(AlarmResource*, QWidget* parent);
        void                  reloadFromCache(const QString& resourceID);
#endif
        bool                  save();
        void                  close();
        void                  startUpdate();
        bool                  endUpdate();
        KAEvent*              earliestAlarm() const;
        void                  setAlarmPending(KAEvent*, bool pending = true);
        bool                  haveDisabledAlarms() const   { return mHaveDisabledAlarms; }
        void                  disabledChanged(const KAEvent*);
        KAEvent::List         atLoginAlarms() const;
#ifdef USE_AKONADI
        KCalCore::Event::Ptr  kcalEvent(const QString& uniqueID);   // if Akonadi, display calendar only
        KAEvent*              event(const EventId& uniqueId, bool checkDuplicates = false);
#else
        KCal::Event*          createKCalEvent(const KAEvent* e) const
                                                     { return createKCalEvent(e, QString()); }
        KCal::Event*          kcalEvent(const QString& uniqueId);   // if Akonadi, display calendar only
        KAEvent*              event(const QString& uniqueId);
#endif
        KAEvent*              templateEvent(const QString& templateName);
#ifdef USE_AKONADI
        KAEvent::List         events(const QString& uniqueId) const;
        KAEvent::List         events(CalEvent::Types s = CalEvent::EMPTY) const  { return events(Akonadi::Collection(), s); }
        KAEvent::List         events(const Akonadi::Collection&, CalEvent::Types = CalEvent::EMPTY) const;
#else
        KAEvent::List         events(CalEvent::Types s = CalEvent::EMPTY) const  { return events(0, s); }
        KAEvent::List         events(AlarmResource*, CalEvent::Types = CalEvent::EMPTY) const;
        KAEvent::List         events(const KDateTime& from, const KDateTime& to, CalEvent::Types);
#endif
#ifdef USE_AKONADI
        KCalCore::Event::List kcalEvents(CalEvent::Type s = CalEvent::EMPTY);   // display calendar only
        bool                  eventReadOnly(Akonadi::Item::Id) const;
        Akonadi::Collection   collectionForEvent(Akonadi::Item::Id) const;
        bool                  addEvent(KAEvent&, QWidget* promptParent = 0, bool useEventID = false, Akonadi::Collection* = 0, bool noPrompt = false, bool* cancelled = 0);
        bool                  modifyEvent(const EventId& oldEventId, KAEvent& newEvent);
#else
        KCal::Event::List     kcalEvents(CalEvent::Type s = CalEvent::EMPTY)   { return kcalEvents(0, s); }
        KCal::Event::List     kcalEvents(AlarmResource*, CalEvent::Type = CalEvent::EMPTY);
        bool                  eventReadOnly(const QString& uniqueID) const;
        AlarmResource*        resourceForEvent(const QString& eventID) const;
        bool                  addEvent(KAEvent*, QWidget* promptParent = 0, bool useEventID = false, AlarmResource* = 0, bool noPrompt = false, bool* cancelled = 0);
        bool                  modifyEvent(const QString& oldEventId, KAEvent* newEvent);
#endif
        KAEvent*              updateEvent(const KAEvent&);
        KAEvent*              updateEvent(const KAEvent*);
#ifdef USE_AKONADI
        bool                  deleteEvent(const KAEvent&, bool save = false);
        bool                  deleteDisplayEvent(const QString& eventID, bool save = false);
#else
        bool                  deleteEvent(const QString& eventID, bool save = false);
#endif
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
#ifdef USE_AKONADI
        static KAEvent*       getEvent(const EventId&);
        static bool           importAlarms(QWidget*, Akonadi::Collection* = 0);
#else
        static KAEvent*       getEvent(const QString& uniqueId);
        static bool           importAlarms(QWidget*, AlarmResource* = 0);
#endif
        static bool           exportAlarms(const KAEvent::List&, QWidget* parent);

    signals:
        void                  earliestAlarmChanged();
        void                  haveDisabledAlarmsChanged(bool haveDisabled);
#ifdef USE_AKONADI
        void                  atLoginEventAdded(const KAEvent&);
#endif
        void                  calendarSaved(AlarmCalendar*);

    private slots:
        void                  setAskResource(bool ask);
#ifdef USE_AKONADI
        void                  slotCollectionStatusChanged(const Akonadi::Collection&, AkonadiModel::Change,
                                                          const QVariant& value, bool inserted);
        void                  slotEventsAdded(const AkonadiModel::EventList&);
        void                  slotEventsToBeRemoved(const AkonadiModel::EventList&);
        void                  slotEventChanged(const AkonadiModel::Event&);
#else
        void                  slotCacheDownloaded(AlarmResource*);
        void                  slotResourceLoaded(AlarmResource*, bool success);
        void                  slotResourceChange(AlarmResource*, AlarmResources::Change);
#endif

    private:
        enum CalType { RESOURCES, LOCAL_ICAL, LOCAL_VCAL };
#ifdef USE_AKONADI
        typedef QMap<Akonadi::Collection::Id, KAEvent::List> ResourceMap;  // id = invalid for display calendar
        typedef QMap<Akonadi::Collection::Id, KAEvent*> EarliestMap;
        typedef QHash<EventId, KAEvent*> KAEventMap;  // indexed by collection and event UID
#else
        typedef QMap<AlarmResource*, KAEvent::List> ResourceMap;  // resource = null for display calendar
        typedef QMap<AlarmResource*, KAEvent*> EarliestMap;
        typedef QMap<QString, KAEvent*> KAEventMap;  // indexed by event UID
#endif

        AlarmCalendar();
        AlarmCalendar(const QString& file, CalEvent::Type);
        bool                  saveCal(const QString& newFile = QString());
#ifdef USE_AKONADI
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
#else
        bool                  isValid() const   { return mCalendar; }
        bool                  addEvent(AlarmResource*, KAEvent*);
        KAEvent*              addEvent(AlarmResource*, const KCal::Event*);
        void                  addNewEvent(AlarmResource*, KAEvent*);
        CalEvent::Type        deleteEventInternal(const QString& eventID);
        void                  updateKAEvents(AlarmResource*, KCal::CalendarLocal*);
        static void           updateResourceKAEvents(AlarmResource*, KCal::CalendarLocal*);
        void                  removeKAEvents(AlarmResource*, bool closing = false);
        void                  findEarliestAlarm(AlarmResource*);
        KCal::Event*          createKCalEvent(const KAEvent*, const QString& baseID) const;
#endif
        void                  checkForDisabledAlarms();
        void                  checkForDisabledAlarms(bool oldEnabled, bool newEnabled);

        static AlarmCalendar* mResourcesCalendar;  // the calendar resources
        static AlarmCalendar* mDisplayCalendar;    // the display calendar

#ifdef USE_AKONADI
        KCalCore::FileStorage::Ptr mCalendarStorage; // null pointer for Akonadi
#else
        KCal::Calendar*       mCalendar;           // AlarmResources or CalendarLocal, null for Akonadi
#endif
        ResourceMap           mResourceMap;
        KAEventMap            mEventMap;           // lookup of all events by UID
        EarliestMap           mEarliestAlarm;      // alarm with earliest trigger time, by resource
        QList<QString>        mPendingAlarms;      // IDs of alarms which are currently being processed after triggering
        KUrl                  mUrl;                // URL of current calendar file
        KUrl                  mICalUrl;            // URL of iCalendar file
#ifndef USE_AKONADI
        typedef QMap<AlarmResource*, ProgressDialog*> ProgressDlgMap;
        typedef QMap<AlarmResource*, QWidget*> ResourceWidgetMap;
        ProgressDlgMap        mProgressDlgs;       // download progress dialogues
        ResourceWidgetMap     mProgressParents;    // parent widgets for download progress dialogues
#endif
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
