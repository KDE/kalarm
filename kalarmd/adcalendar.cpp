/*
 *  adcalendar.cpp  -  calendar file access
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  Copyright (c) 2001, 2004-2006 by David Jarvie <software@astrojar.org.uk>
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

#include <unistd.h>
#include <assert.h>

#include <qfile.h>

#include <ktempfile.h>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kdebug.h>

#include "adcalendar.moc"

QList<ADCalendar*> ADCalendar::mCalendars;
ADCalendar::EventsMap   ADCalendar::mEventsHandled;
ADCalendar::EventsMap   ADCalendar::mEventsPending;
QStringList             ADCalendar::mCalendarUrls;    // never delete or reorder anything in this list!


ADCalendar::ADCalendar(const QString& url, const QByteArray& appname)
        : KCal::CalendarLocal(QLatin1String("UTC")),
	  mUrlString(url),
	  mAppName(appname),
	  mLoaded(false),
	  mLoadedConnected(false),
	  mUnregistered(false),
	  mEnabled(true)
{
	ADCalendar* cal = calendar(url);
	if (cal)
	{
		kError(5900) << "ADCalendar::ADCalendar(" << url << "): calendar already exists" << endl;
		assert(0);
	}
	mUrlIndex = mCalendarUrls.indexOf(url);    // get unique index for this URL
	if (mUrlIndex < 0)
	{
		mUrlIndex = static_cast<int>(mCalendarUrls.count());
		mCalendarUrls.append(url);
	}
	loadFile(false);
	mCalendars.append(this);
}

ADCalendar::~ADCalendar()
{
	clearEventsHandled();
	mCalendars.remove(this);
}

/******************************************************************************
* Load the calendar file.
*/
bool ADCalendar::loadFile(bool reset)
{
	if (reset)
		clearEventsHandled();
	if (!mTempFileName.isNull())
	{
		// Don't try to load the file if already downloading it
		kError(5900) << "ADCalendar::loadFile(): already downloading another file\n";
		return false;
	}
	mLoaded = false;
	KUrl url(mUrlString);
	if (url.isLocalFile())
	{
		// It's a local file
		loadLocalFile(url.path());
		emit loaded(this, mLoaded);
	}
	else
	{
		// It's a remote file. Download to a temporary file before loading it
		KTempFile tempFile;
		mTempFileName = tempFile.name();
		KUrl dest;
		dest.setPath(mTempFileName);
		KIO::FileCopyJob* job = KIO::file_copy(url, dest, -1, true);
		connect(job, SIGNAL(result(KIO::Job*)), SLOT(slotDownloadJobResult(KIO::Job*)));
	}
	return true;
}

void ADCalendar::slotDownloadJobResult(KIO::Job *job)
{
	if (job->error())
	{
		KUrl url(mUrlString);
		kDebug(5900) << "Error downloading calendar from " << url.prettyURL() << endl;
		job->showErrorDialog(0);
	}
	else
	{
		kDebug(5900) << "--- Downloaded to " << mTempFileName << endl;
		loadLocalFile(mTempFileName);
	}
	unlink(QFile::encodeName(mTempFileName));
	mTempFileName.clear();
	emit loaded(this, mLoaded);
}

void ADCalendar::loadLocalFile(const QString& filename)
{
	mLoaded = load(filename);
	if (!mLoaded)
		kDebug(5900) << "ADCalendar::loadLocalFile(): Error loading calendar file '" << filename << "'\n";
	else
		clearEventsHandled(true);   // remove all events which no longer exist from handled list
}

bool ADCalendar::setLoadedConnected()
{
	if (mLoadedConnected)
		return true;
	mLoadedConnected = true;
	return false;
}

/******************************************************************************
* Check whether all the alarms for the event with the given ID have already
* been handled.
*/
bool ADCalendar::eventHandled(const KCal::Event* event, const QList<QDateTime>& alarmtimes)
{
	EventsMap::ConstIterator it = mEventsHandled.find(EventKey(event->uid(), mUrlIndex));
	if (it == mEventsHandled.end())
		return false;

	int oldCount = it.value().alarmTimes.count();
	int count = alarmtimes.count();
	for (int i = 0;  i < count;  ++i)
	{
		if (alarmtimes[i].isValid()
		&&  (i >= oldCount                             // is it an additional alarm?
		     || !it.value().alarmTimes[i].isValid()     // or has it just become due?
		     || it.value().alarmTimes[i].isValid()      // or has it changed?
		        && alarmtimes[i] != it.value().alarmTimes[i]))
			return false;     // this alarm has changed
	}
	return true;
}

/******************************************************************************
* Remember that the event with the given ID has been handled.
* It must already be in the pending list.
*/
void ADCalendar::setEventHandled(const QString& eventID)
{
	kDebug(5900) << "ADCalendar::setEventHandled(" << eventID << ")\n";
	EventKey key(eventID, mUrlIndex);

	// Remove it from the pending list, and add it to the handled list
	EventsMap::Iterator it = mEventsPending.find(key);
	if (it != mEventsPending.end())
	{
		setEventInMap(mEventsHandled, key, it.value().alarmTimes, it.value().eventSequence);
		mEventsPending.remove(it);
	}
}

/******************************************************************************
* Remember that the specified alarms for the event with the given ID have been
* notified to KAlarm, but no reply has come back yet.
*/
void ADCalendar::setEventPending(const KCal::Event* event, const QList<QDateTime>& alarmtimes)
{
	if (event)
	{
		kDebug(5900) << "ADCalendar::setEventPending(" << event->uid() << ")\n";
		EventKey key(event->uid(), mUrlIndex);
		setEventInMap(mEventsPending, key, alarmtimes, event->revision());
	}
}

/******************************************************************************
* Add a specified entry to the events pending or handled list.
*/
void ADCalendar::setEventInMap(EventsMap& map, const EventKey& key, const QList<QDateTime>& alarmtimes, int sequence)
{
	EventsMap::Iterator it = map.find(key);
	if (it != map.end())
	{
		// Update the existing entry for the event
		it.value().alarmTimes = alarmtimes;
		it.value().eventSequence = sequence;
	}
	else
		map.insert(key, EventItem(sequence, alarmtimes));
}

/******************************************************************************
* Clear all memory of events handled for the calendar.
*/
void ADCalendar::clearEventsHandled(bool nonexistentOnly)
{
	clearEventMap(mEventsPending, nonexistentOnly);
	clearEventMap(mEventsHandled, nonexistentOnly);
}

/******************************************************************************
* Clear the events pending or handled list of all events handled for the calendar.
*/
void ADCalendar::clearEventMap(EventsMap& map, bool nonexistentOnly)
{
	for (EventsMap::Iterator it = map.begin();  it != map.end();  )
	{
		if (it.key().calendarIndex == mUrlIndex
		&&  (!nonexistentOnly  ||  !event(it.key().eventID)))
		{
			EventsMap::Iterator i = it;
			++it;                      // prevent iterator becoming invalid with remove()
			map.remove(i);
		}
		else
			++it;
	}
}

/******************************************************************************
* Look up the calendar with the specified full calendar URL.
*/
ADCalendar* ADCalendar::calendar(const QString& calendarURL)
{
	if (!calendarURL.isEmpty())
	{
		for (int i = 0, end = mCalendars.count();  i < end;  ++i)
		{
			if (mCalendars[i]->urlString() == calendarURL)
				return mCalendars[i];
		}
	}
	return 0;
}
