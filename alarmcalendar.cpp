/*
 *  alarmcalendar.cpp  -  KAlarm calendar file access
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
 */

#include "kalarm.h"

#include <kmessagebox.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <kio/netaccess.h>
#include <kfileitem.h>
#include <ktempfile.h>
#include <dcopclient.h>
#include <kdebug.h>

#include <libkcal/vcaldrag.h>
#include <libkcal/vcalformat.h>
#include <libkcal/icalformat.h>

#include "kalarmapp.h"
#include "alarmcalendar.h"

const QString DEFAULT_CALENDAR_FILE(QString::fromLatin1("calendar.ics"));


/******************************************************************************
* Read the calendar file URL from the config file (or use the default).
* If there is an error, the program exits.
*/
void AlarmCalendar::getURL() const
{
	if (!url.isValid())
	{
		KConfig* config = kapp->config();
		config->setGroup(QString::fromLatin1("General"));
		*const_cast<KURL*>(&url) = config->readEntry(QString::fromLatin1("Calendar"),
		                                             locateLocal("appdata", DEFAULT_CALENDAR_FILE));
		if (!url.isValid())
		{
			kdDebug(5950) << "AlarmCalendar::getURL(): invalid name: " << url.prettyURL() << endl;
			KMessageBox::error(0L, i18n("Invalid calendar file name: %1").arg(url.prettyURL()),
			                   kapp->aboutData()->programName());
			kapp->exit(1);
		}
	}
}

/******************************************************************************
* Open the calendar file and load it into memory.
*/
bool AlarmCalendar::open()
{
	getURL();
	calendar = new CalendarLocal;
	calendar->showDialogs(FALSE);

	// Find out whether the calendar is ICal or VCal format
	QString ext = url.filename().right(4);
	vCal = (ext == QString::fromLatin1(".vcs"));

	if (!KIO::NetAccess::exists(url))
	{
		if (!create())      // create the calendar file
			return false;
	}

	// Load the calendar file
	switch (load())
	{
		case 1:
			break;
		case 0:
			if (!create()  ||  load() <= 0)
				return false;
		case -1:
/*	if (!KIO::NetAccess::exists(url))
	{
		if (!create()  ||  load() <= 0)
			return false;
	}*/
			return false;
	}
	return true;
}

/******************************************************************************
* Create a new calendar file.
*/
bool AlarmCalendar::create()
{
	// Create the calendar file
	KTempFile* tmpFile = 0L;
	QString filename;
	if (url.isLocalFile())
		filename = url.path();
	else
	{
		tmpFile = new KTempFile;
		filename = tmpFile->name();
	}
	if (!save(filename))
	{
		delete tmpFile;
		return false;
	}
	delete tmpFile;
	return true;
}

/******************************************************************************
* Load the calendar file into memory.
* Reply = 1 if success, -2 if failure, 0 if zero-length file exists.
*/
int AlarmCalendar::load()
{
	getURL();
	kdDebug(5950) << "AlarmCalendar::load(): " << url.prettyURL() << endl;
	QString tmpFile;
	if (!KIO::NetAccess::download(url, tmpFile))
	{
		kdError(5950) << "AlarmCalendar::load(): Load failure" << endl;
		KMessageBox::error(0L, i18n("Cannot open calendar:\n%1").arg(url.prettyURL()), kapp->aboutData()->programName());
		return -1;
	}
	kdDebug(5950) << "AlarmCalendar::load(): --- Downloaded to " << tmpFile << endl;
	if (!calendar->load(tmpFile))
	{
		// Check if the file is zero length
		KIO::NetAccess::removeTempFile(tmpFile);
		KIO::UDSEntry uds;
		KIO::NetAccess::stat(url, uds);
		KFileItem fi(uds, url);
		if (!fi.size())
			return 0;     // file is zero length
		kdDebug(5950) << "AlarmCalendar::load(): Error loading calendar file '" << tmpFile << "'" << endl;
		KMessageBox::error(0L, i18n("Error loading calendar:\n%1\n\nPlease fix or delete the file.").arg(url.prettyURL()),
		                   kapp->aboutData()->programName());
		return -1;
	}
	if (!localFile.isEmpty())
		KIO::NetAccess::removeTempFile(localFile);
	localFile = tmpFile;
	return 1;
}

/******************************************************************************
* Save the calendar from memory to file.
*/
bool AlarmCalendar::save(const QString& filename)
{
	kdDebug(5950) << "AlarmCalendar::save(): " << filename << endl;
	CalFormat* format = (vCal ? static_cast<CalFormat*>(new VCalFormat(calendar)) : static_cast<CalFormat*>(new ICalFormat(calendar)));
	bool success = calendar->save(filename, format);
	delete format;
	if (!success)
	{
		kdDebug(5950) << "AlarmCalendar::save(): calendar save failed." << endl;
		return false;
	}

	getURL();
	if (!url.isLocalFile())
	{
		if (!KIO::NetAccess::upload(filename, url))
		{
			KMessageBox::error(0L, i18n("Cannot upload calendar to\n'%1'").arg(url.prettyURL()), kapp->aboutData()->programName());
			return false;
		}
	}

	// Tell the alarm daemon to reload the calendar
	QByteArray data;
	QDataStream arg(data, IO_WriteOnly);
	arg << QCString(kapp->aboutData()->appName()) << url.url();
	if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "reloadMsgCal(QCString,QString)", data))
		kdDebug(5950) << "AlarmCalendar::save(): addCal dcop send failed" << endl;
	return true;
}

/******************************************************************************
* Delete any temporary file at program exit.
*/
void AlarmCalendar::close()
{
	if (!localFile.isEmpty())
		KIO::NetAccess::removeTempFile(localFile);
}

/******************************************************************************
* Add the specified event to the calendar.
*/
void AlarmCalendar::addEvent(const KAlarmEvent& event)
{
	Event* kcalEvent = new Event;
	event.updateEvent(*kcalEvent);
	calendar->addEvent(kcalEvent);
	const_cast<KAlarmEvent&>(event).setEventID(kcalEvent->uid());
}

/******************************************************************************
* Update the specified event in the calendar with its new contents.
* The event retains the same ID.
*/
void AlarmCalendar::updateEvent(const KAlarmEvent& event)
{
	Event* kcalEvent = getEvent(event.id());
	if (kcalEvent)
		event.updateEvent(*kcalEvent);
}

/******************************************************************************
* Delete the specified event from the calendar.
*/
void AlarmCalendar::deleteEvent(const QString& eventID)
{
	Event* kcalEvent = getEvent(eventID);
	if (kcalEvent)
		calendar->deleteEvent(kcalEvent);
}
