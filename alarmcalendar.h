/*
 *  alarmcalendar.h  -  KAlarm calendar file access
 *  Program:  kalarm
 *  Copyright © 2001-2009 by David Jarvie <djarvie@kde.org>
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

#include <QObject>
#include <kurl.h>
#include "alarmevent.h"
#include "alarmresources.h"

namespace KCal {
  class Calendar;
  class CalendarLocal;
}
class AlarmResource;
class ProgressDialog;


/** Provides read and write access to calendar files and resources.
 *  Either vCalendar or iCalendar files may be read, but the calendar is saved
 *  only in iCalendar format to avoid information loss.
 */
class AlarmCalendar : public QObject
{
		Q_OBJECT
	public:
		virtual ~AlarmCalendar();
		bool                  valid() const          { return (mCalType == RESOURCES) || mUrl.isValid(); }
		KCalEvent::Status     type() const           { return (mCalType == RESOURCES) ? KCalEvent::EMPTY : mEventType; }
		bool                  open();
		int                   load();
		void                  loadResource(AlarmResource*, QWidget* parent);
		bool                  reload();
		void                  reloadFromCache(const QString& resourceID);
		bool                  save();
		void                  close();
		void                  startUpdate();
		bool                  endUpdate();
		KAEvent*              earliestAlarm() const;
		void                  setAlarmPending(KAEvent*, bool pending = true);
		KAEvent::List         atLoginAlarms() const;
		KCal::Event*          createKCalEvent(const KAEvent* e, bool original = false) const
		                                             { return createKCalEvent(e, QString(), original); }
		KCal::Event*          createKCalEvent(const KAEvent*, const QString& baseID, bool original = false) const;
		KAEvent*              event(const QString& uniqueID);
		KCal::Event*          kcalEvent(const QString& uniqueID);
		KAEvent*              templateEvent(const QString& templateName);
		KAEvent::List         events(KCalEvent::Status s = KCalEvent::EMPTY)   { return events(0, s); }
		KAEvent::List         events(AlarmResource*, KCalEvent::Status = KCalEvent::EMPTY);
		KAEvent::List         events(const KDateTime& from, const KDateTime& to, KCalEvent::Status);
		KCal::Event::List     kcalEvents(KCalEvent::Status s = KCalEvent::EMPTY)   { return kcalEvents(0, s); }
		KCal::Event::List     kcalEvents(AlarmResource*, KCalEvent::Status = KCalEvent::EMPTY);
		bool                  eventReadOnly(const QString& uniqueID) const;
		AlarmResource*        resourceForEvent(const QString& eventID) const;
		bool                  addEvent(KAEvent*, QWidget* promptParent = 0, bool useEventID = false, AlarmResource* = 0, bool noPrompt = false, bool* cancelled = 0);
		bool                  modifyEvent(const QString& oldEventId, KAEvent* newEvent);
		KAEvent*              updateEvent(const KAEvent&);
		KAEvent*              updateEvent(const KAEvent*);
		bool                  deleteEvent(const QString& eventID, bool save = false);
		void                  emitEmptyStatus();
		void                  purgeEvents(const KAEvent::List&);
		bool                  isOpen() const         { return mOpen; }
		bool                  isEmpty() const;
		QString               path() const           { return (mCalType == RESOURCES) ? QString() : mUrl.prettyUrl(); }
		QString               urlString() const      { return (mCalType == RESOURCES) ? QString() : mUrl.url(); }

		static QString        icalProductId();
		static bool           initialiseCalendars();
		static void           terminateCalendars();
		static AlarmCalendar* resources()            { return mResourcesCalendar; }
		static AlarmCalendar* displayCalendar()      { return mDisplayCalendar; }
		static AlarmCalendar* displayCalendarOpen();
		static bool           importAlarms(QWidget*, AlarmResource* = 0);
		static bool           exportAlarms(const KAEvent::List&, QWidget* parent);
		static KAEvent*       getEvent(const QString& uniqueID);
		static const KCal::Event* getKCalEvent(const QString& uniqueID);

	signals:
		void                  earliestAlarmChanged();
		void                  calendarSaved(AlarmCalendar*);
		void                  emptyStatus(bool empty);

	private slots:
		void                  slotCacheDownloaded(AlarmResource*);
		void                  slotResourceLoaded(AlarmResource*, bool success);
		void                  slotResourceChange(AlarmResource*, AlarmResources::Change);

	private:
		enum CalType { RESOURCES, LOCAL_ICAL, LOCAL_VCAL };
		typedef QMap<AlarmResource*, KAEvent::List> ResourceMap;  // resource = null for display calendar
		typedef QMap<QString, KAEvent*> KAEventMap;  // indexed by event UID
		typedef QMap<AlarmResource*, KAEvent*> EarliestMap;

		AlarmCalendar();
		AlarmCalendar(const QString& file, KCalEvent::Status);
		bool                  saveCal(const QString& newFile = QString());
		bool                  addEvent(AlarmResource*, KAEvent*);
		KAEvent*              addEvent(AlarmResource*, const KCal::Event*);
		void                  addNewEvent(AlarmResource*, KAEvent*);
		KCalEvent::Status     deleteEventInternal(const QString& eventID);
		void                  updateKAEvents(AlarmResource*, KCal::CalendarLocal*);
		static void           updateResourceKAEvents(AlarmResource*, KCal::CalendarLocal*);
		void                  removeKAEvents(AlarmResource*, bool closing = false);
		void                  findEarliestAlarm(AlarmResource*);

		static AlarmCalendar* mResourcesCalendar;  // the calendar resources
		static AlarmCalendar* mDisplayCalendar;    // the display calendar

		KCal::Calendar*       mCalendar;           // AlarmResources or CalendarLocal
		ResourceMap           mResourceMap;
		KAEventMap            mEventMap;           // lookup of all events by UID
		EarliestMap           mEarliestAlarm;      // alarm with earliest trigger time, by resource
		QList<QString>        mPendingAlarms;      // IDs of alarms which are currently being processed after triggering
		KUrl                  mUrl;                // URL of current calendar file
		KUrl                  mICalUrl;            // URL of iCalendar file
		typedef QMap<AlarmResource*, ProgressDialog*> ProgressDlgMap;
		typedef QMap<AlarmResource*, QWidget*> ResourceWidgetMap;
		ProgressDlgMap        mProgressDlgs;       // download progress dialogues
		ResourceWidgetMap     mProgressParents;    // parent widgets for download progress dialogues
		QString               mLocalFile;          // calendar file, or local copy if it's a remote file
		CalType               mCalType;            // what type of calendar mCalendar is (resources/ical/vcal)
		KCalEvent::Status     mEventType;          // what type of events the calendar file is for
		bool                  mOpen;               // true if the calendar file is open
		int                   mUpdateCount;        // nesting level of group of calendar update calls
		bool                  mUpdateSave;         // save() was called while mUpdateCount > 0

		using QObject::event;   // prevent "hidden" warning
};

#endif // ALARMCALENDAR_H
