/*
 *  alarmcalendar.cpp  -  KAlarm calendar file access
 *  Program:  kalarm
 *  (C) 2001 - 2003 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"
#include <unistd.h>
#include <time.h>

#include <qfile.h>
#include <qtextstream.h>

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
#include <libical/ical.h>
}

#include <libkcal/vcaldrag.h>
#include <libkcal/vcalformat.h>
#include <libkcal/icalformat.h>

#include "kalarmapp.h"
#include "alarmcalendar.h"

using namespace KCal;


AlarmCalendar::AlarmCalendar(const QString& path, KAlarmEvent::Status type, const QString& icalPath,
                             const QString& configKey)
	: mCalendar(0),
	  mConfigKey(icalPath.isNull() ? QString::null : configKey),
	  mType(type),
	  mKAlarmVersion(-1),
	  mKAlarmVersion057_UTC(false)
{
	mUrl.setPath(path);       // N.B. constructor mUrl(path) doesn't work with UNIX paths
	mICalUrl.setPath(icalPath.isNull() ? path : icalPath);
	mVCal = (icalPath.isNull() || path != icalPath);    // is the calendar in ICal or VCal format?
}

/******************************************************************************
* Open the calendar file and load it into memory.
*/
bool AlarmCalendar::open()
{
	if (mCalendar)
		return true;
	if (!mUrl.isValid())
		return false;
	kdDebug(5950) << "AlarmCalendar::open(" << mUrl.prettyURL() << ")\n";
	mCalendar = new CalendarLocal();
	mCalendar->setLocalTime();    // write out using local time (i.e. no time zone)

	if (!KIO::NetAccess::exists(mUrl))
	{
		if (!create())      // create the calendar file
		{
			delete mCalendar;
			mCalendar = 0;
			return false;
		}
	}

	// Load the calendar file
	switch (load())
	{
		case 1:         // success
			break;
		case 0:         // zero-length file
			if (!create()  ||  load() <= 0)
			{
				delete mCalendar;
				mCalendar = 0;
				return false;
			}
		case -1:        // failure
			delete mCalendar;
			mCalendar = 0;
			return false;
	}
	return true;
}

/******************************************************************************
* Private method to create a new calendar file.
* It is always created in iCalendar format.
*/
bool AlarmCalendar::create()
{
	// Create the calendar file
	KTempFile* tmpFile = 0;
	QString filename;
	if (mICalUrl.isLocalFile())
		filename = mICalUrl.path();
	else
	{
		tmpFile = new KTempFile;
		filename = tmpFile->name();
	}
	if (!saveCal(filename))
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
	if (!mCalendar)
		return -2;

	kdDebug(5950) << "AlarmCalendar::load(): " << mUrl.prettyURL() << endl;
	QString tmpFile;
	if (!KIO::NetAccess::download(mUrl, tmpFile))
	{
		kdError(5950) << "AlarmCalendar::load(): Load failure" << endl;
		KMessageBox::error(0, i18n("Cannot open calendar:\n%1").arg(mUrl.prettyURL()), kapp->aboutData()->programName());
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
		kdError(5950) << "AlarmCalendar::load(): Error loading calendar file '" << tmpFile << "'" << endl;
		KMessageBox::error(0, i18n("Error loading calendar:\n%1\n\nPlease fix or delete the file.").arg(mUrl.prettyURL()),
		                   kapp->aboutData()->programName());
		// load() could have partially populated the calendar, so clear it out
		mCalendar->close();
		delete mCalendar;
		mCalendar = 0;
		return -1;
	}
	if (!mLocalFile.isEmpty())
		KIO::NetAccess::removeTempFile(mLocalFile);   // removes it only if it IS a temporary file
	mLocalFile = tmpFile;

	// Find the version of KAlarm which wrote the calendar file, and do
	// any necessary conversions to the current format
	getKAlarmVersion();
	if (mKAlarmVersion == KAlarmVersion(0,5,7))
	{
		// KAlarm version 0.5.7 - check whether times are stored in UTC, in which
		// case it is the KDE 3.0.0 version, which needs adjustment of summer times.
		mKAlarmVersion057_UTC = isUTC();
		kdDebug(5950) << "AlarmCalendar::load(): KAlarm version 0.5.7 (" << (mKAlarmVersion057_UTC ? "" : "non-") << "UTC)\n";
	}
	else
		kdDebug(5950) << "AlarmCalendar::load(): KAlarm version " << mKAlarmVersion << endl;
	KAlarmEvent::convertKCalEvents(*this);   // convert events to current KAlarm format for when calendar is saved
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
	return open();
}

/******************************************************************************
* Save the calendar from memory to file.
*/
bool AlarmCalendar::saveCal(const QString& filename)
{
	if (!mCalendar)
		return false;

	kdDebug(5950) << "AlarmCalendar::saveCal(" << filename << ")\n";
	QString saveFilename = filename.isNull() ? mLocalFile : filename;
	if (mVCal  &&  filename.isNull()  &&  mUrl.isLocalFile())
		saveFilename = mICalUrl.path();
	bool success = mCalendar->save(saveFilename, new ICalFormat);
	if (!success)
	{
		kdError(5950) << "AlarmCalendar::saveCal(" << saveFilename << "): failed.\n";
		return false;
	}

	if (!mICalUrl.isLocalFile())
	{
		if (!KIO::NetAccess::upload(saveFilename, mICalUrl))
		{
			kdError(5950) << "AlarmCalendar::saveCal(" << saveFilename << "): upload failed.\n";
			KMessageBox::error(0, i18n("Cannot upload calendar to\n'%1'").arg(mICalUrl.prettyURL()), kapp->aboutData()->programName());
			return false;
		}
	}

	if (mVCal)
	{
		// The file was in vCalendar format, but has now been saved in iCalendar format.
		// Save the change in the config file.
		if (!mConfigKey.isNull())
		{
			KConfig* config = kapp->config();
			config->setGroup(QString::fromLatin1("General"));
			config->writePathEntry(mConfigKey, mICalUrl.path());
			config->sync();
		}
		mUrl  = mICalUrl;
		mVCal = false;
	}

	if (mType == KAlarmEvent::ACTIVE)
	{
		// Tell the alarm daemon to reload the calendar
		QByteArray data;
		QDataStream arg(data, IO_WriteOnly);
		arg << QCString(kapp->aboutData()->appName()) << mUrl.url();
		if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "reloadMsgCal(QCString,QString)", data))
			kdError(5950) << "AlarmCalendar::saveCal(): reloadMsgCal dcop send failed" << endl;
	}
	return true;
}

/******************************************************************************
* Delete any temporary file at program exit.
*/
void AlarmCalendar::close()
{
	if (!mLocalFile.isEmpty())
		KIO::NetAccess::removeTempFile(mLocalFile);   // removes it only if it IS a temporary file
	if (mCalendar)
	{
		mCalendar->close();
		delete mCalendar;
		mCalendar = 0;
	}
}

/******************************************************************************
* If it is VCal format, convert the calendar URL to ICal and save the new URL
* in the config file.
*/
void AlarmCalendar::convertToICal()
{
	if (mVCal)
	{
		if (!mConfigKey.isNull())
		{
			KConfig* config = kapp->config();
			config->setGroup(QString::fromLatin1("General"));
			config->writePathEntry(mConfigKey, mICalUrl.path());
			config->sync();
		}
		mUrl  = mICalUrl;
		mVCal = false;
	}
}

/******************************************************************************
* Purge all events from the calendar whose end time is longer ago than 'daysToKeep'.
* All events are deleted if 'daysToKeep' is zero.
*/
void AlarmCalendar::purge(int daysToKeep, bool saveIfPurged)
{
	if (daysToKeep >= 0)
	{
		bool purged = false;
		QDate cutoff = QDate::currentDate().addDays(-daysToKeep);
		Event::List events = mCalendar->events();
                for (Event::List::ConstIterator it = events.begin();  it != events.end();  ++it)
		{
			Event* kcalEvent = *it;
                        if (!daysToKeep  ||  kcalEvent->created().date() < cutoff)
			{
				mCalendar->deleteEvent(kcalEvent);
				purged = true;
			}
		}
		if (purged  &&  saveIfPurged)
			saveCal();
	}
}

/******************************************************************************
* Add the specified event to the calendar.
* If it is the active calendar and 'useEventID' is false, a new event ID is
* created. In all other cases, the event ID is taken from 'event'.
* Reply = the KCal::Event as written to the calendar.
*/
Event* AlarmCalendar::addEvent(const KAlarmEvent& event, bool useEventID)
{
	if (!mCalendar)
		return 0;
	if (useEventID  &&  event.id().isEmpty())
		useEventID = false;
	Event* kcalEvent = new Event;
	switch (mType)
	{
		case KAlarmEvent::ACTIVE:
			if (!useEventID)
				const_cast<KAlarmEvent&>(event).setEventID(kcalEvent->uid());
			break;
		case KAlarmEvent::EXPIRED:
		case KAlarmEvent::DISPLAYING:
			useEventID = true;
			break;
	}
	if (useEventID)
		kcalEvent->setUid(KAlarmEvent::uid(event.id(), mType));
	event.updateKCalEvent(*kcalEvent, false, mType == KAlarmEvent::EXPIRED);
	mCalendar->addEvent(kcalEvent);
	event.clearUpdated();
	return kcalEvent;
}

/******************************************************************************
* Update the specified event in the calendar with its new contents.
* The event retains the same ID.
*/
void AlarmCalendar::updateEvent(const KAlarmEvent& evnt)
{
	if (mCalendar)
	{
		Event* kcalEvent = event(evnt.id());
		if (kcalEvent)
		{
			evnt.updateKCalEvent(*kcalEvent);
			evnt.clearUpdated();
		}
	}
}

/******************************************************************************
* Delete the specified event from the calendar, if it exists.
* The calendar is then optionally saved.
*/
void AlarmCalendar::deleteEvent(const QString& eventID, bool saveit)
{
	if (mCalendar)
	{
		Event* kcalEvent = event(eventID);
		if (kcalEvent)
		{
			mCalendar->deleteEvent(kcalEvent);
			if (saveit)
				saveCal();
		}
	}
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
