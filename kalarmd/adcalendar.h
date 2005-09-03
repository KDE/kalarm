/*
 *  adcalendar.h  -  calendar file access
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  Copyright (C) 2001, 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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

#ifndef ADCALENDAR_H
#define ADCALENDAR_H

#include <libkcal/calendarlocal.h>
//Added by qt3to4:
#include <Q3ValueList>
#include <Q3CString>
namespace KIO { class Job; }
class ADCalendar;


// Alarm Daemon calendar access
class ADCalendar : public KCal::CalendarLocal
{
		Q_OBJECT
	public:
		typedef Q3ValueList<ADCalendar*>::ConstIterator ConstIterator;

		~ADCalendar();

		const QString&  urlString() const       { return mUrlString; }
		const Q3CString& appName() const         { return mAppName; }

		void            setEnabled(bool enabled) { mEnabled = enabled; }
		bool            enabled() const         { return mEnabled && !unregistered(); }
		bool            available() const       { return loaded() && !unregistered(); }

		// Client has registered since calendar was constructed, but
		// has not since added the calendar. Monitoring is disabled.
		void            setUnregistered(bool u) { mUnregistered = u; }
		bool            unregistered() const    { return mUnregistered; }
  
		bool            eventHandled(const KCal::Event*, const Q3ValueList<QDateTime>&);
		void            setEventHandled(const KCal::Event*, const Q3ValueList<QDateTime>&);
		void            clearEventsHandled(bool nonexistentOnly = false);

		bool            loadFile(bool reset);
		bool            setLoadedConnected();     // check status of mLoadedConnected and set it to true
		bool            downloading() const     { return !mTempFileName.isNull(); }
		bool            loaded() const          { return mLoaded; }


		static ConstIterator begin()            { return mCalendars.begin(); }
		static ConstIterator end()              { return mCalendars.end(); }
		static ADCalendar*   getCalendar(const QString& calendarURL);

	signals:
		void            loaded(ADCalendar*, bool success);

	protected:
		// Only ClientInfo can construct ADCalendar objects
		friend class ClientInfo;
		ADCalendar(const QString& url, const Q3CString& appname);

	private slots:
		void            slotDownloadJobResult(KIO::Job*);

	private:
		struct EventKey
		{
			EventKey() : calendarIndex(-1) { }
			EventKey(const QString& id, int cal) : eventID(id), calendarIndex(cal) { }
			bool    operator<(const EventKey& k) const
			            { return (calendarIndex == k.calendarIndex)
			                   ? (eventID < k.eventID) : (calendarIndex < k.calendarIndex);
			            }
			QString eventID;
			int     calendarIndex;
		};
		struct EventItem
		{
			EventItem() : eventSequence(0) { }
			EventItem(int seqno, const Q3ValueList<QDateTime>& alarmtimes)
			        : eventSequence(seqno), alarmTimes(alarmtimes) {}
			int                   eventSequence;
			Q3ValueList<QDateTime> alarmTimes;
		};

		typedef QMap<EventKey, EventItem>  EventsMap;   // calendar/event ID, event sequence num
		static EventsMap               mEventsHandled;  // IDs of already triggered events
		static QStringList             mCalendarUrls;   // URLs of all calendars ever opened
		static Q3ValueList<ADCalendar*> mCalendars;      // list of all constructed calendars

		ADCalendar(const ADCalendar&);             // prohibit copying
		ADCalendar& operator=(const ADCalendar&);  // prohibit copying

		void            loadLocalFile(const QString& filename);

		QString           mUrlString;       // calendar file URL
		Q3CString          mAppName;         // name of application owning this calendar
		QString           mTempFileName;    // temporary file used if currently downloading, else null
		int               mUrlIndex;        // unique index to URL in mCalendarUrls
		bool              mLoaded;          // true if calendar file is currently loaded
		bool              mLoadedConnected; // true if the loaded() signal has been connected to AlarmDaemon
		bool              mUnregistered;    // client has registered, but has not since added the calendar
		bool              mEnabled;         // events are currently manually enabled
};

#endif // ADCALENDAR_H
