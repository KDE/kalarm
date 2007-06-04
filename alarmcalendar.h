/*
 *  alarmcalendar.h  -  KAlarm calendar file access
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <software@astrojar.org.uk>
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
		void                  loadAndDaemonReload(AlarmResource*, QWidget* parent);
		bool                  reload();
		void                  reloadFromCache(const QString& resourceID);
		bool                  save();
		void                  close();
		void                  startUpdate();
		bool                  endUpdate();
		KCal::Event*          createKCalEvent(const KAEvent& e, bool original = false, bool cancelCancelledDefer = false) const
		                                             { return createKCalEvent(e, QString(), original, cancelCancelledDefer); }
		KCal::Event*          createKCalEvent(const KAEvent&, const QString& baseID, bool original = false, bool cancelCancelledDefer = false) const;
		KCal::Event*          event(const QString& uniqueID);
		KAEvent               templateEvent(const QString& templateName);
		KCal::Event::List     events(KCalEvent::Status = KCalEvent::EMPTY);
		KCal::Event::List     eventsWithAlarms(const KDateTime& from, const KDateTime& to, KCalEvent::Status);
		bool                  eventReadOnly(const QString& uniqueID) const;
		KCal::Event*          addEvent(KAEvent&, QWidget* promptParent = 0, bool useEventID = false, AlarmResource* = 0, bool noPrompt = false);
		KCal::Event*          modifyEvent(const QString& oldEventId, KAEvent& newEvent);
		KCal::Event*          updateEvent(const KAEvent&);
		bool                  deleteEvent(const QString& eventID, bool save = false);
		void                  emitEmptyStatus();
		void                  purgeEvents(KCal::Event::List);
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
		static const KCal::Event* getEvent(const QString& uniqueID);

	public slots:
		void                  slotDaemonRegistered(bool newStatus);

	signals:
		void                  calendarSaved(AlarmCalendar*);
		void                  emptyStatus(bool empty);

	private slots:
		void                  slotCacheDownloaded(AlarmResource*);
		void                  slotResourceLoaded(AlarmResource*, bool success);

	private:
		enum CalType { RESOURCES, LOCAL_ICAL, LOCAL_VCAL };

		AlarmCalendar();
		AlarmCalendar(const QString& file, KCalEvent::Status);
		bool                  saveCal(const QString& newFile = QString());

		static AlarmCalendar* mResourcesCalendar;  // the calendar resources
		static AlarmCalendar* mDisplayCalendar;    // the display calendar

		KCal::Calendar*       mCalendar;           // AlarmResources or CalendarLocal
		KUrl                  mUrl;                // URL of current calendar file
		KUrl                  mICalUrl;            // URL of iCalendar file
		QList<AlarmResource*> mDaemonReloads;      // resources which daemon should reload once KAlarm has loaded them
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
};

#endif // ALARMCALENDAR_H
