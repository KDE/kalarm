/*
 *  alarmcalendar.h  -  KAlarm calendar file access
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#ifndef ALARMCALENDAR_H
#define ALARMCALENDAR_H

#include <kurl.h>
#include <libkcal/calendarlocal.h>
#include "msgevent.h"
using namespace KCal;


class AlarmCalendar
{
	public:
		AlarmCalendar(const QString& file, KAlarmEvent::Status);
		bool            valid() const                       { return mUrl.isValid(); }
		bool            open();
		int             load();
		int             reload();
		bool            save()                              { return save(mLocalFile); }
		void            close();
		void            purge(int daysToKeep, bool saveIfPurged);
		Event*          event(const QString& uniqueID)      { return mCalendar ? mCalendar->event(uniqueID) : 0; }
		QPtrList<Event> events()                            { return mCalendar->events(); }
		QString         addEvent(const KAlarmEvent&);
		void            updateEvent(const KAlarmEvent&);
		void            deleteEvent(const QString& eventID);
		bool            isOpen() const                      { return !!mCalendar; }
		QString         path() const                        { return mUrl.prettyURL(); }
		QString         urlString() const                   { return mUrl.url(); }
		int             KAlarmVersion() const               { return mKAlarmVersion; }
		bool            KAlarmVersion057_UTC() const        { return mKAlarmVersion057_UTC; }
		static int      KAlarmVersion(int major, int minor, int rev)  { return major*10000 + minor*100 + rev; }
	private:
		bool            create();
		bool            save(const QString& tempFile);
		void            getKAlarmVersion() const;
		bool            isUTC() const;

		CalendarLocal*  mCalendar;
		KURL            mUrl;            // URL of calendar file
		QString         mLocalFile;      // local name of calendar file
		KAlarmEvent::Status mType;       // what type of events the calendar file is for
		mutable int     mKAlarmVersion;  // version of KAlarm which created the loaded calendar file
		mutable bool    mKAlarmVersion057_UTC;  // calendar file was created by KDE 3.0.0 KAlarm 0.5.7
		bool            mVCal;           // true if calendar file is in VCal format
};

#endif // ALARMCALENDAR_H
