/*
 *  alarmcalendar.h  -  KAlarm calendar file access
 *  Program:  kalarm
 *  Copyright © 2001-2006 by David Jarvie <software@astrojar.org.uk>
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

#include <qobject.h>
#include <kurl.h>
#include <libkcal/calendarlocal.h>
#include "alarmevent.h"

class KConfig;


/** Provides read and write access to calendar files.
 *  Either vCalendar or iCalendar files may be read, but the calendar is saved
 *  only in iCalendar format to avoid information loss.
 */
class AlarmCalendar : public QObject
{
		Q_OBJECT
	public:
		virtual ~AlarmCalendar();
		bool                  valid() const                       { return mUrl.isValid(); }
		KAEvent::Status       type() const                        { return mType; }
		bool                  open();
		int                   load();
		bool                  reload();
		bool                  save();
		void                  close();
		void                  startUpdate();
		bool                  endUpdate();
		KCal::Event*          event(const QString& uniqueID);
		KCal::Event::List     events();
		KCal::Event::List     eventsWithAlarms(const QDateTime& from, const QDateTime& to);
		KCal::Event*          addEvent(KAEvent&, bool useEventID = false);
		void                  updateEvent(const KAEvent&);
		bool                  deleteEvent(const QString& eventID, bool save = false);
		void                  emitEmptyStatus();
		void                  purgeAll()                          { purge(0); }
		void                  setPurgeDays(int days);
		void                  purgeIfQueued();    // must only be called from KAlarmApp::processQueue()
		bool                  isOpen() const                      { return mOpen; }
		QString               path() const                        { return mUrl.prettyURL(); }
		QString               urlString() const                   { return mUrl.url(); }

		static QString        icalProductId();
		static bool           initialiseCalendars();
		static void           terminateCalendars();
		static AlarmCalendar* activeCalendar()        { return mCalendars[ACTIVE]; }
		static AlarmCalendar* expiredCalendar()       { return mCalendars[EXPIRED]; }
		static AlarmCalendar* displayCalendar()       { return mCalendars[DISPLAY]; }
		static AlarmCalendar* templateCalendar()      { return mCalendars[TEMPLATE]; }
		static AlarmCalendar* activeCalendarOpen()    { return calendarOpen(ACTIVE); }
		static AlarmCalendar* expiredCalendarOpen()   { return calendarOpen(EXPIRED); }
		static AlarmCalendar* displayCalendarOpen()   { return calendarOpen(DISPLAY); }
		static AlarmCalendar* templateCalendarOpen()  { return calendarOpen(TEMPLATE); }
		static bool           importAlarms(QWidget*);
		static const KCal::Event* getEvent(const QString& uniqueID);

		enum CalID { ACTIVE, EXPIRED, DISPLAY, TEMPLATE, NCALS };

	signals:
		void                  calendarSaved(AlarmCalendar*);
		void                  purged();
		void                  emptyStatus(bool empty);

	private slots:
		void                  slotPurge();

	private:
		AlarmCalendar(const QString& file, CalID, const QString& icalFile = QString::null,
		              const QString& configKey = QString::null);
		bool                  create();
		bool                  saveCal(const QString& newFile = QString::null);
		void                  purge(int daysToKeep);
		void                  startPurgeTimer();
		static AlarmCalendar* createCalendar(CalID, KConfig*, QString& writePath, const QString& configKey = QString::null);
		static AlarmCalendar* calendarOpen(CalID);

		static AlarmCalendar* mCalendars[NCALS];   // the calendars

		KCal::CalendarLocal*  mCalendar;
		KURL                  mUrl;                // URL of current calendar file
		KURL                  mICalUrl;            // URL of iCalendar file
		QString               mLocalFile;          // calendar file, or local copy if it's a remote file
		QString               mConfigKey;          // config file key for this calendar's URL
		KAEvent::Status       mType;               // what type of events the calendar file is for
		int                   mPurgeDays;          // how long to keep alarms, 0 = don't keep, -1 = keep indefinitely
		bool                  mOpen;               // true if the calendar file is open
		int                   mPurgeDaysQueued;    // >= 0 to purge the calendar when called from KAlarmApp::processLoop()
		int                   mUpdateCount;        // nesting level of group of calendar update calls
		bool                  mUpdateSave;         // save() was called while mUpdateCount > 0
		bool                  mVCal;               // true if calendar file is in VCal format
};

#endif // ALARMCALENDAR_H
