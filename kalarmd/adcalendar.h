/*
 *  adcalendar.h  -  calendar file access
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  (C) 2001, 2004 by David Jarvie <software@astrojar.org.uk>
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

#ifndef ADCALENDAR_H
#define ADCALENDAR_H

#include <libkcal/calendarlocal.h>
namespace KIO { class Job; }
class ADCalendar;


// Alarm Daemon calendar access
class ADCalendar : public KCal::CalendarLocal
{
		Q_OBJECT
	public:
		typedef QValueList<ADCalendar*>::ConstIterator ConstIterator;

		~ADCalendar();

		const QString&  urlString() const       { return mUrlString; }
		const QCString& appName() const         { return mAppName; }

		void            setEnabled(bool enabled) { mEnabled = enabled; }
		bool            enabled() const         { return mEnabled && !unregistered(); }
		bool            available() const       { return loaded() && !unregistered(); }

		// Client has registered since calendar was constructed, but
		// has not since added the calendar. Monitoring is disabled.
		void            setUnregistered(bool u) { mUnregistered = u; }
		bool            unregistered() const    { return mUnregistered; }
  
		bool            eventHandled(const KCal::Event*, const QValueList<QDateTime>&);
		void            setEventHandled(const KCal::Event*, const QValueList<QDateTime>&);
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
		ADCalendar(const QString& url, const QCString& appname);

	private slots:
		void            slotDownloadJobResult(KIO::Job*);

	private:
		struct EventItem
		{
			EventItem() : eventSequence(0) { }
			EventItem(const QString& url, int seqno, const QValueList<QDateTime>& alarmtimes)
			        : calendarURL(url), eventSequence(seqno), alarmTimes(alarmtimes) {}

			QString   calendarURL;
			int       eventSequence;
			QValueList<QDateTime> alarmTimes;
		};

		typedef QMap<QString, EventItem>  EventsMap;   // event ID, calendar URL/event sequence num
		static EventsMap  mEventsHandled; // IDs of displayed KALARM type events
		static QValueList<ADCalendar*> mCalendars;    // list of all constructed calendars

		ADCalendar(const ADCalendar&);             // prohibit copying
		ADCalendar& operator=(const ADCalendar&);  // prohibit copying

		void            loadLocalFile(const QString& filename);

		QString           mUrlString;     // calendar file URL
		QCString          mAppName;       // name of application owning this calendar
		QString           mTempFileName;  // temporary file used if currently downloading, else null
		bool              mLoaded;        // true if calendar file is currently loaded
		bool              mLoadedConnected; // true if the loaded() signal has been connected to AlarmDaemon
		bool              mUnregistered;  // client has registered, but has not since added the calendar
		bool              mEnabled;       // events are currently manually enabled
};

#endif // ADCALENDAR_H
