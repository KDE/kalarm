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
#include <unistd.h>
#include <time.h>

#include <qfile.h>

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

extern "C" {
#include <ical.h>
}

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
	if (!mUrl.isValid())
	{
		KConfig* config = kapp->config();
		config->setGroup(QString::fromLatin1("General"));
		*const_cast<KURL*>(&mUrl) = config->readEntry(QString::fromLatin1("Calendar"),
		                                             locateLocal("appdata", DEFAULT_CALENDAR_FILE));
		if (!mUrl.isValid())
		{
			kdDebug(5950) << "AlarmCalendar::getURL(): invalid name: " << mUrl.prettyURL() << endl;
			KMessageBox::error(0L, i18n("Invalid calendar file name: %1").arg(mUrl.prettyURL()),
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
	mCalendar = new CalendarLocal();
	mCalendar->setLocalTime();    // write out using local time (i.e. no time zone)

	// Find out whether the calendar is ICal or VCal format
	QString ext = mUrl.filename().right(4);
	mVCal = (ext == QString::fromLatin1(".vcs"));

	if (!KIO::NetAccess::exists(mUrl))
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
/*	if (!KIO::NetAccess::exists(mUrl))
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
	if (mUrl.isLocalFile())
		filename = mUrl.path();
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
	kdDebug(5950) << "AlarmCalendar::load(): " << mUrl.prettyURL() << endl;
	QString tmpFile;
	if (!KIO::NetAccess::download(mUrl, tmpFile))
	{
		kdError(5950) << "AlarmCalendar::load(): Load failure" << endl;
		KMessageBox::error(0L, i18n("Cannot open calendar:\n%1").arg(mUrl.prettyURL()), kapp->aboutData()->programName());
		return -1;
	}
	kdDebug(5950) << "AlarmCalendar::load(): --- Downloaded to " << tmpFile << endl;
	mKAlarmVersion        = -1;
	mKAlarmVersion057_UTC = false;
	mCalendar->setTimeZoneId(QString::null);   // default to the local time zone for reading
	bool loaded = mCalendar->load(tmpFile);
	mCalendar->setLocalTime();                 // write using local time (i.e. no time zone)
	if (!loaded)
	{
		// Check if the file is zero length
		KIO::NetAccess::removeTempFile(tmpFile);
		KIO::UDSEntry uds;
		KIO::NetAccess::stat(mUrl, uds);
		KFileItem fi(uds, mUrl);
		if (!fi.size())
			return 0;     // file is zero length
		kdDebug(5950) << "AlarmCalendar::load(): Error loading calendar file '" << tmpFile << "'" << endl;
		KMessageBox::error(0L, i18n("Error loading calendar:\n%1\n\nPlease fix or delete the file.").arg(mUrl.prettyURL()),
		                   kapp->aboutData()->programName());
		return -1;
	}
	if (!mLocalFile.isEmpty())
		KIO::NetAccess::removeTempFile(mLocalFile);
	mLocalFile = tmpFile;

	// Find the version of KAlarm which wrote the calendar file, and do
	// any necessary conversions to the current format
	getKAlarmVersion();
	if (mKAlarmVersion == KAlarmVersion(0,5,7))
	{
		// KAlarm version 0.5.7 - check whether times are stored in UTC, in which
		// case it is the KDE 3.0.0 version which needs adjustment of summer times.
		mKAlarmVersion057_UTC = isUTC();
		kdDebug(5950) << "AlarmCalendar::load(): KAlarm version 0.5.7 (" << (mKAlarmVersion057_UTC ? "" : "non-") << "UTC)\n";
	}
	else
		kdDebug(5950) << "AlarmCalendar::load(): KAlarm version " << mKAlarmVersion << endl;
	KAlarmEvent::convertKCalEvents();    // convert events to current KAlarm format for when calendar is saved
	return 1;
}

/******************************************************************************
* Reload the calendar file into memory.
* Reply = 1 if success, -2 if failure, 0 if zero-length file exists.
*/
int AlarmCalendar::reload()
{
	if (!mCalendar)
		return -2;
	kdDebug(5950) << "AlarmCalendar::reload(): " << mUrl.prettyURL() << endl;
	close();
	return load();
}

/******************************************************************************
* Save the calendar from memory to file.
*/
bool AlarmCalendar::save(const QString& filename)
{
	kdDebug(5950) << "AlarmCalendar::save(): " << filename << endl;
	CalFormat* format;
	if(mVCal)
		format = new VCalFormat;
	else
		format = new ICalFormat;
	bool success = mCalendar->save(filename, format);
	if (!success)
	{
		kdDebug(5950) << "AlarmCalendar::save(): calendar save failed." << endl;
		return false;
	}

	getURL();
	if (!mUrl.isLocalFile())
	{
		if (!KIO::NetAccess::upload(filename, mUrl))
		{
			KMessageBox::error(0L, i18n("Cannot upload calendar to\n'%1'").arg(mUrl.prettyURL()), kapp->aboutData()->programName());
			return false;
		}
	}

	// Tell the alarm daemon to reload the calendar
	QByteArray data;
	QDataStream arg(data, IO_WriteOnly);
	arg << QCString(kapp->aboutData()->appName()) << mUrl.url();
	if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "reloadMsgCal(QCString,QString)", data))
		kdDebug(5950) << "AlarmCalendar::save(): addCal dcop send failed" << endl;
	return true;
}

/******************************************************************************
* Delete any temporary file at program exit.
*/
void AlarmCalendar::close()
{
	if (!mLocalFile.isEmpty())
		KIO::NetAccess::removeTempFile(mLocalFile);
	if (mCalendar)
		mCalendar->close();
}

/******************************************************************************
* Add the specified event to the calendar.
*/
void AlarmCalendar::addEvent(const KAlarmEvent& event)
{
	Event* kcalEvent = new Event;
	event.updateEvent(*kcalEvent);
	mCalendar->addEvent(kcalEvent);
	const_cast<KAlarmEvent&>(event).setEventID(kcalEvent->uid());
}

/******************************************************************************
* Update the specified event in the calendar with its new contents.
* The event retains the same ID.
*/
void AlarmCalendar::updateEvent(const KAlarmEvent& evnt)
{
	Event* kcalEvent = event(evnt.id());
	if (kcalEvent)
		evnt.updateEvent(*kcalEvent);
}

/******************************************************************************
* Delete the specified event from the calendar.
*/
void AlarmCalendar::deleteEvent(const QString& eventID)
{
	Event* kcalEvent = event(eventID);
	if (kcalEvent)
		mCalendar->deleteEvent(kcalEvent);
}

/******************************************************************************
* Return the KAlarm version which wrote the calendar which has been loaded.
* The format is, for example, 000507 for 0.5.7, or 0 if unknown.
*/
void AlarmCalendar::getKAlarmVersion() const
{
	// N.B. Remember to change  KAlarmVersion(int major, int minor, int rev)
	//      if the representation returned by this method changes.
	mKAlarmVersion = 0;   // set default to KAlarm pre-0.3.5, or another program
	if (mCalendar)
	{
		const QString& prodid = mCalendar->loadedProductId();
		QString progname = QString(" ") + theApp()->aboutData()->programName() + " ";
		int i = prodid.find(progname, 0, false);
		if (i >= 0)
		{
			QString ver = prodid.mid(i + progname.length()).stripWhiteSpace();
			i = ver.find('/');
			int j = ver.find(' ');
			if (j >= 0  &&  j < i)
				i = j;
			if (i > 0)
			{
				ver = ver.left(i);
				// ver now contains the KAlarm version string
				if ((i = ver.find('.')) > 0)
				{
					bool ok;
					int version = ver.left(i).toInt(&ok) * 10000;   // major version
					if (ok)
					{
						ver = ver.mid(i + 1);
						if ((i = ver.find('.')) > 0)
						{
							int v = ver.left(i).toInt(&ok);   // minor version
							if (ok)
							{
								version += (v < 9 ? v : 9) * 100;
								ver = ver.mid(i + 1);
								if (ver.at(0).isDigit())
								{
									// Allow other characters to follow last digit
									v = ver.toInt();   // issue number
									mKAlarmVersion = version + (v < 9 ? v : 9);
								}
							}
						}
						else
						{
							// There is no issue number
							if (ver.at(0).isDigit())
							{
								// Allow other characters to follow last digit
								int v = ver.toInt();   // minor number
								mKAlarmVersion = version + (v < 9 ? v : 9) * 100;
							}
						}
					}
				}
			}
		}
	}
}

/******************************************************************************
 * Check whether the calendar file has its times stored as UTC times,
 * indicating that it was written by the KDE 3.0.0 version of KAlarm 0.5.7.
 * Reply = true if times are stored in UTC
 *       = false if the calendar is a vCalendar, times are not UTC, or any error occurred.
 */
bool AlarmCalendar::isUTC() const
{
	// Read the calendar file into a QString
	QFile file(mLocalFile);
	if (!file.open(IO_ReadOnly))
		return false;
	QTextStream ts(&file);
	ts.setEncoding(QTextStream::UnicodeUTF8);
	QString text = ts.read();
	file.close();

	// Extract the CREATED property for the first VEVENT from the calendar
	bool result = false;
	icalcomponent* calendar = icalcomponent_new_from_string(text.local8Bit().data());
	if (calendar)
	{
		if (icalcomponent_isa(calendar) == ICAL_VCALENDAR_COMPONENT)
		{
			icalcomponent* c = icalcomponent_get_first_component(calendar, ICAL_VEVENT_COMPONENT);
			if (c)
			{
				icalproperty* p = icalcomponent_get_first_property(c, ICAL_CREATED_PROPERTY);
				if (p)
				{
					struct icaltimetype datetime = icalproperty_get_created(p);
					if (datetime.is_utc)
						result = true;
				}
			}
		}
		icalcomponent_free(calendar);
	}
	return result;
}
