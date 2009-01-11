/*
 *  alarmcalendar.cpp  -  KAlarm calendar file access
 *  Program:  kalarm
 *  Copyright © 2001-2006,2009 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"
#include <unistd.h>
#include <time.h>

#include <qfile.h>
#include <qtextstream.h>
#include <qregexp.h>
#include <qtimer.h>

#include <klocale.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kstaticdeleter.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <kio/netaccess.h>
#include <kfileitem.h>
#include <ktempfile.h>
#include <kfiledialog.h>
#include <dcopclient.h>
#include <kdebug.h>

extern "C" {
#include <libical/ical.h>
}

#include <libkcal/vcaldrag.h>
#include <libkcal/vcalformat.h>
#include <libkcal/icalformat.h>

#include "calendarcompat.h"
#include "daemon.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "preferences.h"
#include "startdaytimer.h"
#include "alarmcalendar.moc"

using namespace KCal;

QString AlarmCalendar::icalProductId()
{
	return QString::fromLatin1("-//K Desktop Environment//NONSGML " KALARM_NAME " %1//EN").arg(KAlarm::currentCalendarVersionString());
}

static const KAEvent::Status eventTypes[AlarmCalendar::NCALS] = {
	KAEvent::ACTIVE, KAEvent::EXPIRED, KAEvent::DISPLAYING, KAEvent::TEMPLATE
};
static const QString calendarNames[AlarmCalendar::NCALS] = {
	QString::fromLatin1("calendar.ics"),
	QString::fromLatin1("expired.ics"),
	QString::fromLatin1("displaying.ics"),
	QString::fromLatin1("template.ics")
};
static KStaticDeleter<AlarmCalendar> calendarDeleter[AlarmCalendar::NCALS];    // ensure that the calendar destructors are called

AlarmCalendar* AlarmCalendar::mCalendars[NCALS] = { 0, 0, 0, 0 };


/******************************************************************************
* Initialise the alarm calendars, and ensure that their file names are different.
* There are 4 calendars:
*  1) A user-independent one containing the active alarms;
*  2) A historical one containing expired alarms;
*  3) A user-specific one which contains details of alarms which are currently
*     being displayed to that user and which have not yet been acknowledged;
*  4) One containing alarm templates.
* Reply = true if success, false if calendar name error.
*/
bool AlarmCalendar::initialiseCalendars()
{
	KConfig* config = kapp->config();
	config->setGroup(QString::fromLatin1("General"));
	QString activeKey   = QString::fromLatin1("Calendar");
	QString expiredKey  = QString::fromLatin1("ExpiredCalendar");
	QString templateKey = QString::fromLatin1("TemplateCalendar");
	QString displayCal, activeCal, expiredCal, templateCal;
	calendarDeleter[ACTIVE].setObject(mCalendars[ACTIVE], createCalendar(ACTIVE, config, activeCal, activeKey));
	calendarDeleter[EXPIRED].setObject(mCalendars[EXPIRED], createCalendar(EXPIRED, config, expiredCal, expiredKey));
	calendarDeleter[DISPLAY].setObject(mCalendars[DISPLAY], createCalendar(DISPLAY, config, displayCal));
	calendarDeleter[TEMPLATE].setObject(mCalendars[TEMPLATE], createCalendar(TEMPLATE, config, templateCal, templateKey));

	QString errorKey1, errorKey2;
	if (activeCal == displayCal)
		errorKey1 = activeKey;
	else if (expiredCal == displayCal)
		errorKey1 = expiredKey;
	else if (templateCal == displayCal)
		errorKey1 = templateKey;
	if (!errorKey1.isNull())
	{
		kdError(5950) << "AlarmCalendar::initialiseCalendars(): '" << errorKey1 << "' calendar name = display calendar name\n";
		QString file = config->readPathEntry(errorKey1);
		KAlarmApp::displayFatalError(i18n("%1: file name not permitted: %2").arg(errorKey1).arg(file));
		return false;
	}
	if (activeCal == expiredCal)
	{
		errorKey1 = activeKey;
		errorKey2 = expiredKey;
	}
	else if (activeCal == templateCal)
	{
		errorKey1 = activeKey;
		errorKey2 = templateKey;
	}
	else if (expiredCal == templateCal)
	{
		errorKey1 = expiredKey;
		errorKey2 = templateKey;
	}
	if (!errorKey1.isNull())
	{
		kdError(5950) << "AlarmCalendar::initialiseCalendars(): calendar names clash: " << errorKey1 << ", " << errorKey2 << endl;
		KAlarmApp::displayFatalError(i18n("%1, %2: file names must be different").arg(errorKey1).arg(errorKey2));
		return false;
	}
	if (!mCalendars[ACTIVE]->valid())
	{
		QString path = mCalendars[ACTIVE]->path();
		kdError(5950) << "AlarmCalendar::initialiseCalendars(): invalid name: " << path << endl;
		KAlarmApp::displayFatalError(i18n("Invalid calendar file name: %1").arg(path));
		return false;
	}
	return true;
}

/******************************************************************************
* Create an alarm calendar instance.
* If 'configKey' is non-null, the calendar will be converted to ICal format.
*/
AlarmCalendar* AlarmCalendar::createCalendar(CalID type, KConfig* config, QString& writePath, const QString& configKey)
{
	static QRegExp vcsRegExp(QString::fromLatin1("\\.vcs$"));
	static QString ical = QString::fromLatin1(".ics");

	if (configKey.isNull())
	{
		writePath = locateLocal("appdata", calendarNames[type]);
		return new AlarmCalendar(writePath, type);
	}
	else
	{
		QString readPath = config->readPathEntry(configKey, locateLocal("appdata", calendarNames[type]));
		writePath = readPath;
		writePath.replace(vcsRegExp, ical);
		return new AlarmCalendar(readPath, type, writePath, configKey);
	}
}

/******************************************************************************
* Terminate access to all calendars.
*/
void AlarmCalendar::terminateCalendars()
{
	for (int i = 0;  i < NCALS;  ++i)
	{
		calendarDeleter[i].destructObject();
		mCalendars[i] = 0;
	}
}

/******************************************************************************
* Return a calendar, opening it first if not already open.
* Reply = calendar instance
*       = 0 if calendar could not be opened.
*/
AlarmCalendar* AlarmCalendar::calendarOpen(CalID id)
{
	AlarmCalendar* cal = mCalendars[id];
	if (!cal->mPurgeDays)
		return 0;     // all events are automatically purged from the calendar
	if (cal->open())
		return cal;
	kdError(5950) << "AlarmCalendar::calendarOpen(" << calendarNames[id] << "): open error\n";
	return 0;
}

/******************************************************************************
* Find and return the event with the specified ID.
* The calendar searched is determined by the calendar identifier in the ID.
*/
const KCal::Event* AlarmCalendar::getEvent(const QString& uniqueID)
{
	if (uniqueID.isEmpty())
		return 0;
	CalID calID;
	switch (KAEvent::uidStatus(uniqueID))
	{
		case KAEvent::ACTIVE:      calID = ACTIVE;  break;
		case KAEvent::TEMPLATE:    calID = TEMPLATE;  break;
		case KAEvent::EXPIRED:     calID = EXPIRED;  break;
		case KAEvent::DISPLAYING:  calID = DISPLAY;  break;
		default:
			return 0;
	}
	AlarmCalendar* cal = calendarOpen(calID);
	if (!cal)
		return 0;
	return cal->event(uniqueID);
}


/******************************************************************************
* Constructor.
* If 'icalPath' is non-null, the file will be always be saved in ICal format.
* If 'configKey' is also non-null, that config file entry will be updated when
* the file is saved in ICal format.
*/
AlarmCalendar::AlarmCalendar(const QString& path, CalID type, const QString& icalPath,
                             const QString& configKey)
	: mCalendar(0),
	  mConfigKey(icalPath.isNull() ? QString::null : configKey),
	  mType(eventTypes[type]),
	  mPurgeDays(-1),      // default to not purging
	  mOpen(false),
	  mPurgeDaysQueued(-1),
	  mUpdateCount(0),
	  mUpdateSave(false)
{
	mUrl.setPath(path);       // N.B. constructor mUrl(path) doesn't work with UNIX paths
	mICalUrl.setPath(icalPath.isNull() ? path : icalPath);
	mVCal = (icalPath.isNull() || path != icalPath);    // is the calendar in ICal or VCal format?
}

AlarmCalendar::~AlarmCalendar()
{
	close();
}

/******************************************************************************
* Open the calendar file if not already open, and load it into memory.
*/
bool AlarmCalendar::open()
{
	if (mOpen)
		return true;
	if (!mUrl.isValid())
		return false;

	kdDebug(5950) << "AlarmCalendar::open(" << mUrl.prettyURL() << ")\n";
	if (!mCalendar)
		mCalendar = new CalendarLocal(QString::fromLatin1("UTC"));
	mCalendar->setLocalTime();    // write out using local time (i.e. no time zone)

	// Check for file's existence, assuming that it does exist when uncertain,
	// to avoid overwriting it.
	if (!KIO::NetAccess::exists(mUrl, true, MainWindow::mainMainWindow()))
	{
		// The calendar file doesn't yet exist, so create it
		if (create())
			load();
	}
	else
	{
		// Load the existing calendar file
		if (load() == 0)
		{
			if (create())       // zero-length file - create a new one
				load();
		}
	}
	if (!mOpen)
	{
		delete mCalendar;
		mCalendar = 0;
	}
	return mOpen;
}

/******************************************************************************
* Private method to create a new calendar file.
* It is always created in iCalendar format.
*/
bool AlarmCalendar::create()
{
	if (mICalUrl.isLocalFile())
		return saveCal(mICalUrl.path());
	else
	{
		KTempFile tmpFile;
		return saveCal(tmpFile.name());
	}
}

/******************************************************************************
* Load the calendar file into memory.
* Reply = 1 if success
*       = 0 if zero-length file exists.
*       = -1 if failure to load calendar file
*       = -2 if instance uninitialised.
*/
int AlarmCalendar::load()
{
	if (!mCalendar)
		return -2;

	kdDebug(5950) << "AlarmCalendar::load(): " << mUrl.prettyURL() << endl;
	QString tmpFile;
	if (!KIO::NetAccess::download(mUrl, tmpFile, MainWindow::mainMainWindow()))
	{
		kdError(5950) << "AlarmCalendar::load(): Load failure" << endl;
		KMessageBox::error(0, i18n("Cannot open calendar:\n%1").arg(mUrl.prettyURL()));
		return -1;
	}
	kdDebug(5950) << "AlarmCalendar::load(): --- Downloaded to " << tmpFile << endl;
	mCalendar->setTimeZoneId(QString::null);   // default to the local time zone for reading
	bool loaded = mCalendar->load(tmpFile);
	mCalendar->setLocalTime();                 // write using local time (i.e. no time zone)
	if (!loaded)
	{
		// Check if the file is zero length
		KIO::NetAccess::removeTempFile(tmpFile);
		KIO::UDSEntry uds;
		KIO::NetAccess::stat(mUrl, uds, MainWindow::mainMainWindow());
		KFileItem fi(uds, mUrl);
		if (!fi.size())
			return 0;     // file is zero length
		kdError(5950) << "AlarmCalendar::load(): Error loading calendar file '" << tmpFile << "'" << endl;
		KMessageBox::error(0, i18n("Error loading calendar:\n%1\n\nPlease fix or delete the file.").arg(mUrl.prettyURL()));
		// load() could have partially populated the calendar, so clear it out
		mCalendar->close();
		delete mCalendar;
		mCalendar = 0;
		return -1;
	}
	if (!mLocalFile.isEmpty())
		KIO::NetAccess::removeTempFile(mLocalFile);   // removes it only if it IS a temporary file
	mLocalFile = tmpFile;

	CalendarCompat::fix(*mCalendar, mLocalFile);   // convert events to current KAlarm format for when calendar is saved
	mOpen = true;
	return 1;
}

/******************************************************************************
* Reload the calendar file into memory.
*/
bool AlarmCalendar::reload()
{
	if (!mCalendar)
		return false;
	kdDebug(5950) << "AlarmCalendar::reload(): " << mUrl.prettyURL() << endl;
	close();
	bool result = open();
	return result;
}

/******************************************************************************
* Save the calendar from memory to file.
* If a filename is specified, create a new calendar file.
*/
bool AlarmCalendar::saveCal(const QString& newFile)
{
	if (!mCalendar  ||  !mOpen && newFile.isNull())
		return false;

	kdDebug(5950) << "AlarmCalendar::saveCal(\"" << newFile << "\", " << mType << ")\n";
	QString saveFilename = newFile.isNull() ? mLocalFile : newFile;
	if (mVCal  &&  newFile.isNull()  &&  mUrl.isLocalFile())
		saveFilename = mICalUrl.path();
	if (!mCalendar->save(saveFilename, new ICalFormat))
	{
		kdError(5950) << "AlarmCalendar::saveCal(" << saveFilename << "): failed.\n";
		KMessageBox::error(0, i18n("Failed to save calendar to\n'%1'").arg(mICalUrl.prettyURL()));
		return false;
	}

	if (!mICalUrl.isLocalFile())
	{
		if (!KIO::NetAccess::upload(saveFilename, mICalUrl, MainWindow::mainMainWindow()))
		{
			kdError(5950) << "AlarmCalendar::saveCal(" << saveFilename << "): upload failed.\n";
			KMessageBox::error(0, i18n("Cannot upload calendar to\n'%1'").arg(mICalUrl.prettyURL()));
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

	mUpdateSave = false;
	emit calendarSaved(this);
	return true;
}

/******************************************************************************
* Delete any temporary file at program exit.
*/
void AlarmCalendar::close()
{
	if (!mLocalFile.isEmpty())
	{
		KIO::NetAccess::removeTempFile(mLocalFile);   // removes it only if it IS a temporary file
		mLocalFile = "";
	}
	if (mCalendar)
	{
		mCalendar->close();
		delete mCalendar;
		mCalendar = 0;
	}
	mOpen = false;
}

/******************************************************************************
* Import alarms from an external calendar and merge them into KAlarm's calendar.
* The alarms are given new unique event IDs.
* Parameters: parent = parent widget for error message boxes
* Reply = true if all alarms in the calendar were successfully imported
*       = false if any alarms failed to be imported.
*/
bool AlarmCalendar::importAlarms(QWidget* parent)
{
	KURL url = KFileDialog::getOpenURL(QString::fromLatin1(":importalarms"),
	                                   QString::fromLatin1("*.vcs *.ics|%1").arg(i18n("Calendar Files")), parent);
	if (url.isEmpty())
	{
		kdError(5950) << "AlarmCalendar::importAlarms(): Empty URL" << endl;
		return false;
	}
	if (!url.isValid())
	{
		kdDebug(5950) << "AlarmCalendar::importAlarms(): Invalid URL" << endl;
		return false;
	}
	kdDebug(5950) << "AlarmCalendar::importAlarms(" << url.prettyURL() << ")" << endl;

	bool success = true;
	QString filename;
	bool local = url.isLocalFile();
	if (local)
	{
		filename = url.path();
		if (!KStandardDirs::exists(filename))
		{
			kdDebug(5950) << "AlarmCalendar::importAlarms(): File '" << url.prettyURL() << "' not found" << endl;
			KMessageBox::error(parent, i18n("Could not load calendar '%1'.").arg(url.prettyURL()));
			return false;
		}
	}
	else
	{
		if (!KIO::NetAccess::download(url, filename, MainWindow::mainMainWindow()))
		{
			kdError(5950) << "AlarmCalendar::importAlarms(): Download failure" << endl;
			KMessageBox::error(parent, i18n("Cannot download calendar:\n%1").arg(url.prettyURL()));
			return false;
		}
		kdDebug(5950) << "--- Downloaded to " << filename << endl;
	}

	// Read the calendar and add its alarms to the current calendars
	CalendarLocal cal(QString::fromLatin1("UTC"));
	cal.setLocalTime();    // write out using local time (i.e. no time zone)
	success = cal.load(filename);
	if (!success)
	{
		kdDebug(5950) << "AlarmCalendar::importAlarms(): error loading calendar '" << filename << "'" << endl;
		KMessageBox::error(parent, i18n("Could not load calendar '%1'.").arg(url.prettyURL()));
	}
	else
	{
		CalendarCompat::fix(cal, filename);
		bool saveActive   = false;
		bool saveExpired  = false;
		bool saveTemplate = false;
		AlarmCalendar* active  = activeCalendar();
		AlarmCalendar* expired = expiredCalendar();
		AlarmCalendar* templat = 0;
		AlarmCalendar* acal;
		Event::List events = cal.rawEvents();
		for (Event::List::ConstIterator it = events.begin();  it != events.end();  ++it)
		{
			const Event* event = *it;
			if (event->alarms().isEmpty()  ||  !KAEvent(*event).valid())
				continue;    // ignore events without alarms, or usable alarms
			KAEvent::Status type = KAEvent::uidStatus(event->uid());
			switch (type)
			{
				case KAEvent::ACTIVE:
					acal = active;
					saveActive = true;
					break;
				case KAEvent::EXPIRED:
					acal = expired;
					saveExpired = true;
					break;
				case KAEvent::TEMPLATE:
					if (!templat)
						templat = templateCalendarOpen();
					acal = templat;
					saveTemplate = true;
					break;
				default:
					continue;
			}
			if (!acal)
				continue;

			Event* newev = new Event(*event);

			// If there is a display alarm without display text, use the event
			// summary text instead.
			if (type == KAEvent::ACTIVE  &&  !newev->summary().isEmpty())
			{
				const Alarm::List& alarms = newev->alarms();
				for (Alarm::List::ConstIterator ait = alarms.begin();  ait != alarms.end();  ++ait)
				{
					Alarm* alarm = *ait;
					if (alarm->type() == Alarm::Display  &&  alarm->text().isEmpty())
						alarm->setText(newev->summary());
				}
				newev->setSummary(QString::null);   // KAlarm only uses summary for template names
			}
			// Give the event a new ID and add it to the calendar
			newev->setUid(KAEvent::uid(CalFormat::createUniqueId(), type));
			if (!acal->mCalendar->addEvent(newev))
				success = false;
		}

		// Save any calendars which have been modified
		if (saveActive)
			active->saveCal();
		if (saveExpired)
			expired->saveCal();
		if (saveTemplate)
			templat->saveCal();
	}
	if (!local)
		KIO::NetAccess::removeTempFile(filename);
	return success;
}

/******************************************************************************
* Flag the start of a group of calendar update calls.
* The purpose is to avoid multiple calendar saves during a group of operations.
*/
void AlarmCalendar::startUpdate()
{
	++mUpdateCount;
}

/******************************************************************************
* Flag the end of a group of calendar update calls.
* The calendar is saved if appropriate.
*/
bool AlarmCalendar::endUpdate()
{
	if (mUpdateCount > 0)
		--mUpdateCount;
	if (!mUpdateCount)
	{
		if (mUpdateSave)
			return saveCal();
	}
	return true;
}

/******************************************************************************
* Save the calendar, or flag it for saving if in a group of calendar update calls.
*/
bool AlarmCalendar::save()
{
	if (mUpdateCount)
	{
		mUpdateSave = true;
		return true;
	}
	else
		return saveCal();
}

#if 0
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
#endif

/******************************************************************************
* Set the number of days to keep alarms.
* Alarms which are older are purged immediately, and at the start of each day.
*/
void AlarmCalendar::setPurgeDays(int days)
{
	if (days != mPurgeDays)
	{
		int oldDays = mPurgeDays;
		mPurgeDays = days;
		if (mPurgeDays <= 0)
			StartOfDayTimer::disconnect(this);
		if (oldDays < 0  ||  days >= 0 && days < oldDays)
		{
			// Alarms are now being kept for less long, so purge them
			if (open())
				slotPurge();
		}
		else if (mPurgeDays > 0)
			startPurgeTimer();
	}
}

/******************************************************************************
* Called at the start of each day by the purge timer.
* Purge all events from the calendar whose end time is longer ago than 'mPurgeDays'.
*/
void AlarmCalendar::slotPurge()
{
	purge(mPurgeDays);
	startPurgeTimer();
}

/******************************************************************************
* Purge all events from the calendar whose end time is longer ago than
* 'daysToKeep'. All events are deleted if 'daysToKeep' is zero.
*/
void AlarmCalendar::purge(int daysToKeep)
{
	if (mPurgeDaysQueued < 0  ||  daysToKeep < mPurgeDaysQueued)
		mPurgeDaysQueued = daysToKeep;

	// Do the purge once any other current operations are completed
	theApp()->processQueue();
}

/******************************************************************************
* This method must only be called from the main KAlarm queue processing loop,
* to prevent asynchronous calendar operations interfering with one another.
*
* Purge all events from the calendar whose end time is longer ago than 'daysToKeep'.
* All events are deleted if 'daysToKeep' is zero.
* The calendar must already be open.
*/
void AlarmCalendar::purgeIfQueued()
{
	if (mPurgeDaysQueued >= 0)
	{
		if (open())
		{
			kdDebug(5950) << "AlarmCalendar::purgeIfQueued(" << mPurgeDaysQueued << ")\n";
			bool changed = false;
			QDate cutoff = QDate::currentDate().addDays(-mPurgeDaysQueued);
			Event::List events = mCalendar->rawEvents();
			for (Event::List::ConstIterator it = events.begin();  it != events.end();  ++it)
			{
				Event* kcalEvent = *it;
				if (!mPurgeDaysQueued  ||  kcalEvent->created().date() < cutoff)
				{
					mCalendar->deleteEvent(kcalEvent);
					changed = true;
				}
			}
			if (changed)
			{
				saveCal();
				emit purged();
			}
			mPurgeDaysQueued = -1;
		}
	}
}


/******************************************************************************
* Start the purge timer to expire at the start of the next day (using the user-
* defined start-of-day time).
*/
void AlarmCalendar::startPurgeTimer()
{
	if (mPurgeDays > 0)
		StartOfDayTimer::connect(this, SLOT(slotPurge()));
}

/******************************************************************************
* Add the specified event to the calendar.
* If it is the active calendar and 'useEventID' is false, a new event ID is
* created. In all other cases, the event ID is taken from 'event'.
* 'event' is updated with the actual event ID.
* Reply = the KCal::Event as written to the calendar.
*/
Event* AlarmCalendar::addEvent(KAEvent& event, bool useEventID)
{
	if (!mOpen)
		return 0;
	QString id = event.id();
	Event* kcalEvent = new Event;
	if (mType == KAEvent::ACTIVE)
	{
		if (id.isEmpty())
			useEventID = false;
		if (!useEventID)
			event.setEventID(kcalEvent->uid());
	}
	else
	{
		if (id.isEmpty())
			id = kcalEvent->uid();
		useEventID = true;
	}
	if (useEventID)
	{
		id = KAEvent::uid(id, mType);
		event.setEventID(id);
		kcalEvent->setUid(id);
	}
	event.updateKCalEvent(*kcalEvent, false, (mType == KAEvent::EXPIRED), true);
	mCalendar->addEvent(kcalEvent);
	event.clearUpdated();
	return kcalEvent;
}

/******************************************************************************
* Update the specified event in the calendar with its new contents.
* The event retains the same ID.
*/
void AlarmCalendar::updateEvent(const KAEvent& evnt)
{
	if (mOpen)
	{
		Event* kcalEvent = event(evnt.id());
		if (kcalEvent)
		{
			evnt.updateKCalEvent(*kcalEvent);
			evnt.clearUpdated();
			if (mType == KAEvent::ACTIVE)
				Daemon::savingEvent(evnt.id());
			return;
		}
	}
	if (mType == KAEvent::ACTIVE)
		Daemon::eventHandled(evnt.id(), false);
}

/******************************************************************************
* Delete the specified event from the calendar, if it exists.
* The calendar is then optionally saved.
*/
bool AlarmCalendar::deleteEvent(const QString& eventID, bool saveit)
{
	if (mOpen)
	{
		Event* kcalEvent = event(eventID);
		if (kcalEvent)
		{
			mCalendar->deleteEvent(kcalEvent);
			if (mType == KAEvent::ACTIVE)
				Daemon::savingEvent(eventID);
			if (saveit)
				return save();
			return true;
		}
	}
	if (mType == KAEvent::ACTIVE)
		Daemon::eventHandled(eventID, false);
	return false;
}

/******************************************************************************
* Emit a signal to indicate whether the calendar is empty.
*/
void AlarmCalendar::emitEmptyStatus()
{
	emit emptyStatus(events().isEmpty());
}

/******************************************************************************
* Return the event with the specified ID.
*/
KCal::Event* AlarmCalendar::event(const QString& uniqueID)
{
	return mCalendar ?  mCalendar->event(uniqueID) : 0;
}

/******************************************************************************
* Return all events in the calendar which contain usable alarms.
*/
KCal::Event::List AlarmCalendar::events()
{
	if (!mCalendar)
		return KCal::Event::List();
	KCal::Event::List list = mCalendar->rawEvents();
	KCal::Event::List::Iterator it = list.begin();
	while (it != list.end())
	{
		KCal::Event* event = *it;
		if (event->alarms().isEmpty()  ||  !KAEvent(*event).valid())
			it = list.remove(it);
		else
			++it;
	}
	return list;
}

/******************************************************************************
* Return all events which have alarms falling within the specified time range.
*/
Event::List AlarmCalendar::eventsWithAlarms(const QDateTime& from, const QDateTime& to)
{
	kdDebug(5950) << "AlarmCalendar::eventsWithAlarms(" << from.toString() << " - " << to.toString() << ")\n";
	Event::List evnts;
	QDateTime dt;
	Event::List allEvents = events();   // ignore events without usable alarms
	for (Event::List::ConstIterator it = allEvents.begin();  it != allEvents.end();  ++it)
	{
		Event* e = *it;
		bool recurs = e->doesRecur();
		int  endOffset = 0;
		bool endOffsetValid = false;
		const Alarm::List& alarms = e->alarms();
		for (Alarm::List::ConstIterator ait = alarms.begin();  ait != alarms.end();  ++ait)
		{
			Alarm* alarm = *ait;
			if (alarm->enabled())
			{
				if (recurs)
				{
					if (alarm->hasTime())
						dt = alarm->time();
					else
					{
						// The alarm time is defined by an offset from the event start or end time.
						// Find the offset from the event start time, which is also used as the
						// offset from the recurrence time.
						int offset = 0;
						if (alarm->hasStartOffset())
							offset = alarm->startOffset().asSeconds();
						else if (alarm->hasEndOffset())
						{
							if (!endOffsetValid)
							{
								endOffset = e->hasDuration() ? e->duration() : e->hasEndDate() ? e->dtStart().secsTo(e->dtEnd()) : 0;
								endOffsetValid = true;
							}
							offset = alarm->endOffset().asSeconds() + endOffset;
						}
						// Adjust the 'from' date/time and find the next recurrence at or after it
						QDateTime pre = from.addSecs(-offset - 1);
						if (e->doesFloat()  &&  pre.time() < Preferences::startOfDay())
							pre = pre.addDays(-1);    // today's recurrence (if today recurs) is still to come
						dt = e->recurrence()->getNextDateTime(pre);
						if (!dt.isValid())
							continue;
						dt = dt.addSecs(offset);
					}
				}
				else
					dt = alarm->time();
				if (dt >= from  &&  dt <= to)
				{
					kdDebug(5950) << "AlarmCalendar::events() '" << e->summary()
					              << "': " << dt.toString() << endl;
					evnts.append(e);
					break;
				}
			}
		}
	}
	return evnts;
}
