/*
 *  alarmcalendar.h  -  KAlarm calendar file access
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef ALARMCALENDAR_H
#define ALARMCALENDAR_H

#include <qobject.h>
#include <kurl.h>
#include <libkcal/calendarlocal.h>
#include "alarmevent.h"


/** Provides read and write access to calendar files.
 *  Either vCalendar or iCalendar files may be read, but the calendar is saved
 *  only in iCalendar format to avoid information loss.
 */
class AlarmCalendar : public QObject
{
		Q_OBJECT
	public:
		AlarmCalendar(const QString& file, KAlarmEvent::Status, const QString& icalFile = QString::null,
		              const QString& configKey = QString::null);
		virtual ~AlarmCalendar();
		bool                  valid() const                       { return mUrl.isValid(); }
		KAlarmEvent::Status   type() const                        { return mType; }
		bool                  open();
		int                   load();
		int                   reload();
		bool                  save()                              { return saveCal(); }
		void                  close();
		void                  purge(int daysToKeep, bool saveIfPurged);
		KCal::Event*          event(const QString& uniqueID)      { return mCalendar ? mCalendar->event(uniqueID) : 0; }
		KCal::Event::List     events()                            { return mCalendar->events(); }
		KCal::Event::List     events(const QDate& d, bool sorted = false) { return mCalendar->events(d, sorted); }
		KCal::Event::List     eventsWithAlarms(const QDateTime& from, const QDateTime& to) const;
		KCal::Event*          addEvent(const KAlarmEvent&, bool useEventID = false);
		void                  updateEvent(const KAlarmEvent&);
		void                  deleteEvent(const QString& eventID, bool save = false);
		bool                  isOpen() const                      { return mOpen; }
		QString               path() const                        { return mUrl.prettyURL(); }
		QString               urlString() const                   { return mUrl.url(); }
		int                   KAlarmVersion() const               { return mKAlarmVersion; }
		bool                  KAlarmVersion057_UTC() const        { return mKAlarmVersion057_UTC; }
		static int            KAlarmVersion(int major, int minor, int rev)  { return major*10000 + minor*100 + rev; }

	signals:
		void                  calendarSaved(AlarmCalendar*);

	private:
		bool                  create();
		bool                  saveCal(const QString& tempFile = QString::null);
		void                  convertToICal();
		void                  getKAlarmVersion() const;
		bool                  isUTC() const;

		KCal::CalendarLocal* mCalendar;
		KURL                 mUrl;              // URL of current calendar file
		KURL                 mICalUrl;          // URL of iCalendar file
		QString              mLocalFile;        // local name of calendar file
		QString              mConfigKey;        // config file key for this calendar's URL
		KAlarmEvent::Status  mType;             // what type of events the calendar file is for
		mutable int          mKAlarmVersion;    // version of KAlarm which created the loaded calendar file
		mutable bool         mKAlarmVersion057_UTC;  // calendar file was created by KDE 3.0.0 KAlarm 0.5.7
		bool                 mVCal;             // true if calendar file is in VCal format
		bool                 mOpen;             // true if the calendar file is open
};

#endif // ALARMCALENDAR_H
