/*
 *  alarmcalendar.cpp  -  KAlarm calendar file access
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <software@astrojar.org.uk>
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

#include <QFile>
#include <QTextStream>
#include <QRegExp>

#include <klocale.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kstaticdeleter.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <kio/netaccess.h>
#include <kfileitem.h>
#include <ktemporaryfile.h>
#include <kfiledialog.h>
#include <kdebug.h>

#include <kcal/calendarlocal.h>
#include <kcal/vcaldrag.h>
#include <kcal/vcalformat.h>
#include <kcal/icalformat.h>
#include <kglobal.h>

#include "alarmresources.h"
#include "calendarcompat.h"
#include "daemon.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "preferences.h"
#include "alarmcalendar.moc"

using namespace KCal;

QString AlarmCalendar::icalProductId()
{
	return QString::fromLatin1("-//K Desktop Environment//NONSGML " KALARM_NAME " %1//EN").arg(KAlarm::currentCalendarVersionString());
}

static const QString displayCalendarName = QLatin1String("displaying.ics");
static KStaticDeleter<AlarmCalendar> resourceCalendarDeleter;   // ensure that the calendar destructor is called
static KStaticDeleter<AlarmCalendar> displayCalendarDeleter;    // ensure that the calendar destructor is called

AlarmCalendar* AlarmCalendar::mResourcesCalendar = 0;
AlarmCalendar* AlarmCalendar::mDisplayCalendar = 0;


/******************************************************************************
* Initialise the alarm calendars, and ensure that their file names are different.
* There are 2 calendars:
*  1) A resources calendar containing the active alarms, archived alarms and
*     alarm templates;
*  2) A user-specific one which contains details of alarms which are currently
*     being displayed to that user and which have not yet been acknowledged;
* Reply = true if success, false if calendar name error.
*/
bool AlarmCalendar::initialiseCalendars()
{
	QString displayCal = KStandardDirs::locateLocal("appdata", displayCalendarName);
	AlarmResources::setDebugArea(5951);
	AlarmResources::setReservedFile(displayCal);
	AlarmResources* resources = AlarmResources::create(Preferences::timeZone(true), false);
	if (!resources)
	{
		if (!AlarmResources::creationError().isEmpty())
			KAlarmApp::displayFatalError(AlarmResources::creationError());
		return false;
	}
	resources->setAskDestinationPolicy(Preferences::askResource());
	resources->showProgress(true);
	resourceCalendarDeleter.setObject(mResourcesCalendar, new AlarmCalendar());
	displayCalendarDeleter.setObject(mDisplayCalendar, new AlarmCalendar(displayCal, KCalEvent::DISPLAYING));
	CalFormat::setApplication(QLatin1String(KALARM_NAME), icalProductId());
	return true;
}

/******************************************************************************
* Terminate access to all calendars.
*/
void AlarmCalendar::terminateCalendars()
{
	resourceCalendarDeleter.destructObject();
	displayCalendarDeleter.destructObject();
}

/******************************************************************************
* Return the display calendar, opening it first if necessary.
*/
AlarmCalendar* AlarmCalendar::displayCalendarOpen()
{
	if (mDisplayCalendar->open())
		return mDisplayCalendar;
	kError(5950) <<"AlarmCalendar::displayCalendarOpen(): open error";
	return 0;
}

/******************************************************************************
* Find and return the event with the specified ID.
* The calendar searched is determined by the calendar identifier in the ID.
*/
const Event* AlarmCalendar::getEvent(const QString& uniqueID)
{
	if (uniqueID.isEmpty())
		return 0;
	const Event* event = mResourcesCalendar->event(uniqueID);
	if (!event)
		event = mDisplayCalendar->event(uniqueID);
	return event;
}


/******************************************************************************
* Constructor for the resources calendar.
*/
AlarmCalendar::AlarmCalendar()
	: mCalendar(0),
	  mCalType(RESOURCES),
	  mEventType(KCalEvent::EMPTY),
	  mOpen(false),
	  mUpdateCount(0),
	  mUpdateSave(false)
{
	AlarmResources* resources = AlarmResources::instance();
	resources->inhibitDefaultReload(true, true);    // inhibit downloads of active alarm resources
	resources->setCalIDFunction(&CalendarCompat::setID);
	resources->setFixFunction(&CalendarCompat::fix);
	connect(resources, SIGNAL(cacheDownloaded(AlarmResource*)), SLOT(slotCacheDownloaded(AlarmResource*)));
	connect(resources, SIGNAL(resourceLoaded(AlarmResource*, bool)), SLOT(slotResourceLoaded(AlarmResource*, bool)));
}

/******************************************************************************
* Constructor for a calendar file.
*/
AlarmCalendar::AlarmCalendar(const QString& path, KCalEvent::Status type)
	: mCalendar(0),
	  mEventType(type),
	  mOpen(false),
	  mUpdateCount(0),
	  mUpdateSave(false)
{
	switch (type)
	{
		case KCalEvent::ACTIVE:
		case KCalEvent::ARCHIVED:
		case KCalEvent::TEMPLATE:
		case KCalEvent::DISPLAYING:
			break;
		default:
			Q_ASSERT(false);   // invalid event type for a calendar
			break;
	}
	mUrl.setPath(path);       // N.B. constructor mUrl(path) doesn't work with UNIX paths
	QString icalPath = path;
	icalPath.replace(QLatin1String("\\.vcs$"), QLatin1String(".ics"));
	mICalUrl.setPath(icalPath);
	mCalType = (path == icalPath) ? LOCAL_ICAL : LOCAL_VCAL;    // is the calendar in ICal or VCal format?
}

AlarmCalendar::~AlarmCalendar()
{
	close();
}

/******************************************************************************
* Open the calendar if not already open, and load it into memory.
*/
bool AlarmCalendar::open()
{
	if (mOpen)
		return true;
	if (mCalType == RESOURCES)
	{
		kDebug(5950) <<"AlarmCalendar::open(RESOURCES)";
		mCalendar = AlarmResources::instance();
		load();
	}
	else
	{
		if (!mUrl.isValid())
			return false;

		kDebug(5950) <<"AlarmCalendar::open(" << mUrl.prettyUrl() <<")";
		if (!mCalendar)
			mCalendar = new CalendarLocal(Preferences::timeZone(true));

		// Check for file's existence, assuming that it does exist when uncertain,
		// to avoid overwriting it.
		if (!KIO::NetAccess::exists(mUrl, KIO::NetAccess::SourceSide, MainWindow::mainMainWindow())
		||  load() == 0)
		{
			// The calendar file doesn't yet exist, or it's zero length, so create a new one
			bool created = false;
			if (mICalUrl.isLocalFile())
				created = saveCal(mICalUrl.path());
			else
			{
				KTemporaryFile tmpFile;
				tmpFile.setAutoRemove(false);
				tmpFile.open();
				created = saveCal(tmpFile.fileName());
			}
			if (created)
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
* Load the calendar into memory.
* Reply = 1 if success
*       = 0 if zero-length file exists.
*       = -1 if failure to load calendar file
*       = -2 if instance uninitialised.
*/
int AlarmCalendar::load()
{
	if (mCalType == RESOURCES)
	{
		kDebug(5950) <<"AlarmCalendar::load(RESOURCES)";
		static_cast<AlarmResources*>(mCalendar)->load();
	}
	else
	{
		if (!mCalendar)
			return -2;
		CalendarLocal* calendar = static_cast<CalendarLocal*>(mCalendar);

		kDebug(5950) <<"AlarmCalendar::load(" << mUrl.prettyUrl() <<")";
		QString tmpFile;
		if (!KIO::NetAccess::download(mUrl, tmpFile, MainWindow::mainMainWindow()))
		{
			kError(5950) <<"AlarmCalendar::load(): Download failure";
			KMessageBox::error(0, i18nc("@info", "Cannot download calendar: <filename>%1</filename>", mUrl.prettyUrl()));
			return -1;
		}
		kDebug(5950) <<"AlarmCalendar::load(): --- Downloaded to" << tmpFile;
		calendar->setTimeSpec(Preferences::timeZone(true));
		if (!calendar->load(tmpFile))
		{
			// Check if the file is zero length
			KIO::NetAccess::removeTempFile(tmpFile);
			KIO::UDSEntry uds;
			KIO::NetAccess::stat(mUrl, uds, MainWindow::mainMainWindow());
			KFileItem fi(uds, mUrl);
			if (!fi.size())
				return 0;     // file is zero length
			kError(5950) <<"AlarmCalendar::load(): Error loading calendar file '" << tmpFile <<"'";
			KMessageBox::error(0, i18nc("@info", "<para>Error loading calendar:</para><para><filename>%1</filename></para><para>Please fix or delete the file.</para>", mUrl.prettyUrl()));
			// load() could have partially populated the calendar, so clear it out
			calendar->close();
			delete mCalendar;
			mCalendar = 0;
			return -1;
		}
		if (!mLocalFile.isEmpty())
			KIO::NetAccess::removeTempFile(mLocalFile);   // removes it only if it IS a temporary file
		mLocalFile = tmpFile;
		CalendarCompat::fix(*calendar, mLocalFile);   // convert events to current KAlarm format for when calendar is saved
	}
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
	if (mCalType == RESOURCES)
	{
		kDebug(5950) <<"AlarmCalendar::reload(RESOURCES)";
		return mCalendar->reload();
	}
	else
	{
		kDebug(5950) <<"AlarmCalendar::reload():" << mUrl.prettyUrl();
		close();
		return open();
	}
}

/******************************************************************************
* Save the calendar from memory to file.
* If a filename is specified, create a new calendar file.
*/
bool AlarmCalendar::saveCal(const QString& newFile)
{
	if (!mCalendar)
		return false;
	if (mCalType == RESOURCES)
	{
		kDebug(5950) <<"AlarmCalendar::saveCal(RESOURCES)";
		mCalendar->save();    // this emits signals resourceSaved(ResourceCalendar*)
	}
	else
	{
		if (!mOpen && newFile.isNull())
			return false;

		kDebug(5950) <<"AlarmCalendar::saveCal(\"" << newFile <<"\"," << mEventType <<")";
		QString saveFilename = newFile.isNull() ? mLocalFile : newFile;
		if (mCalType == LOCAL_VCAL  &&  newFile.isNull()  &&  mUrl.isLocalFile())
			saveFilename = mICalUrl.path();
		if (!static_cast<CalendarLocal*>(mCalendar)->save(saveFilename, new ICalFormat))
		{
			kError(5950) <<"AlarmCalendar::saveCal(" << saveFilename <<"): failed.";
			KMessageBox::error(0, i18nc("@info", "Failed to save calendar to <filename>%1</filename>", mICalUrl.prettyUrl()));
			return false;
		}

		if (!mICalUrl.isLocalFile())
		{
			if (!KIO::NetAccess::upload(saveFilename, mICalUrl, MainWindow::mainMainWindow()))
			{
				kError(5950) <<"AlarmCalendar::saveCal(" << saveFilename <<"): upload failed.";
				KMessageBox::error(0, i18nc("@info", "Cannot upload calendar to <filename>%1</filename>", mICalUrl.prettyUrl()));
				return false;
			}
		}

		if (mCalType == LOCAL_VCAL)
		{
			// The file was in vCalendar format, but has now been saved in iCalendar format.
			mUrl  = mICalUrl;
			mCalType = LOCAL_ICAL;
		}
		emit calendarSaved(this);
	}

	mUpdateSave = false;
	return true;
}

/******************************************************************************
* Delete any temporary file at program exit.
*/
void AlarmCalendar::close()
{
	if (mCalType != RESOURCES)
	{
		if (!mLocalFile.isEmpty())
		{
			KIO::NetAccess::removeTempFile(mLocalFile);   // removes it only if it IS a temporary file
			mLocalFile = "";
		}
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
* Load a resource and if it is local, tell the daemon to reload it.
* If the resource is cached, the cache is refreshed and the DCOP signal
* downloaded() will tell the daemon to reload it from cache, thus ensuring that
* it is downloaded only once, by KAlarm.
*/
void AlarmCalendar::loadAndDaemonReload(AlarmResource* resource, QWidget*)
{
	if (!resource->cached()  &&  !mDaemonReloads.contains(resource))
		mDaemonReloads.append(resource);
	if (!AlarmResources::instance()->load(resource, ResourceCached::SyncCache))
		slotResourceLoaded(resource, false);
}

/******************************************************************************
* Called when a remote resource cache has completed loading.
* Tell the daemon to reload the resource.
*/
void AlarmCalendar::slotCacheDownloaded(AlarmResource* resource)
{
	slotResourceLoaded(resource, false);   // false ensures that the daemon is told
}

/******************************************************************************
* Called when a resource has completed loading.
* Tell the daemon to reload the resource either if it is in the daemon-reload
* list, or if loading failed and it is now inactive.
*/
void AlarmCalendar::slotResourceLoaded(AlarmResource* resource, bool success)
{
	bool tellDaemon = !success;    // on failure, tell daemon that resource is now inactive
	int i = mDaemonReloads.indexOf(resource);
	if (i >= 0)
	{
		mDaemonReloads.removeAt(i);
		tellDaemon = true;
	}
	if (tellDaemon)
		Daemon::reloadResource(resource->identifier());
}

/******************************************************************************
* Reload a resource from its cache file, without refreshing the cache first.
*/
void AlarmCalendar::reloadFromCache(const QString& resourceID)
{
	kDebug(5950) <<"AlarmCalendar::reloadFromCache(" << resourceID <<")";
	if (mCalendar  &&  mCalType == RESOURCES)
	{
		AlarmResource* resource = static_cast<AlarmResources*>(mCalendar)->resourceWithId(resourceID);
		if (resource)
			resource->load(ResourceCached::NoSyncCache);    // reload from cache
	}
}

/******************************************************************************
* Called when the alarm daemon registration status changes.
* If the daemon is running, leave downloading of remote active alarm resources
* to it. If the daemon is not running, ensure that KAlarm does downloads.
*/
void AlarmCalendar::slotDaemonRegistered(bool newStatus)
{
	AlarmResources* resources = AlarmResources::instance();
	resources->inhibitDefaultReload(true, newStatus);
	if (!newStatus)
	{
		kDebug(5950) <<"AlarmCalendar::slotDaemonRegistered(false): reload resources";
		resources->loadIfNotReloaded();    // reload any resources which need to be downloaded
	}
}

/******************************************************************************
* Import alarms from an external calendar and merge them into KAlarm's calendar.
* The alarms are given new unique event IDs.
* Parameters: parent = parent widget for error message boxes
* Reply = true if all alarms in the calendar were successfully imported
*       = false if any alarms failed to be imported.
*/
bool AlarmCalendar::importAlarms(QWidget* parent, AlarmResource* resource)
{
	KUrl url = KFileDialog::getOpenUrl(KUrl("filedialog:///importalarms"),
	                                   QString::fromLatin1("*.vcs *.ics|%1").arg(i18nc("@info/plain", "Calendar Files")), parent);
	if (url.isEmpty())
	{
		kError(5950) <<"AlarmCalendar::importAlarms(): Empty URL";
		return false;
	}
	if (!url.isValid())
	{
		kDebug(5950) <<"AlarmCalendar::importAlarms(): Invalid URL";
		return false;
	}
	kDebug(5950) <<"AlarmCalendar::importAlarms(" << url.prettyUrl() <<")";

	bool success = true;
	QString filename;
	bool local = url.isLocalFile();
	if (local)
	{
		filename = url.path();
		if (!KStandardDirs::exists(filename))
		{
			kDebug(5950) <<"AlarmCalendar::importAlarms(): File '" << url.prettyUrl() <<"' not found";
			KMessageBox::error(parent, i18nc("@info", "Could not load calendar <filename>%1</filename>.", url.prettyUrl()));
			return false;
		}
	}
	else
	{
		if (!KIO::NetAccess::download(url, filename, MainWindow::mainMainWindow()))
		{
			kError(5950) <<"AlarmCalendar::importAlarms(): Download failure";
			KMessageBox::error(parent, i18nc("@info", "Cannot download calendar: <filename>%1</filename>", url.prettyUrl()));
			return false;
		}
		kDebug(5950) <<"--- Downloaded to" << filename;
	}

	// Read the calendar and add its alarms to the current calendars
	CalendarLocal cal(Preferences::timeZone(true));
	success = cal.load(filename);
	if (!success)
	{
		kDebug(5950) <<"AlarmCalendar::importAlarms(): error loading calendar '" << filename <<"'";
		KMessageBox::error(parent, i18nc("@info", "Could not load calendar <filename>%1</filename>.", url.prettyUrl()));
	}
	else
	{
		KCalendar::Status caltype = CalendarCompat::fix(cal, filename);
		KCalEvent::Status wantedType = resource ? resource->kcalEventType() : KCalEvent::EMPTY;
		bool saveRes = false;
		AlarmResources* resources = AlarmResources::instance();
		AlarmResource* activeRes   = 0;
		AlarmResource* archivedRes = 0;
		AlarmResource* templateRes = 0;
		Event::List events = cal.rawEvents();
		for (int i = 0, end = events.count();  i < end;  ++i)
		{
			const Event* event = events[i];
			if (event->alarms().isEmpty())
				continue;    // ignore events without alarms
			KCalEvent::Status type = KCalEvent::status(event);
			if (type == KCalEvent::TEMPLATE)
			{
				// If we know the event was not created by KAlarm, don't treat it as a template
				if (caltype == KCalendar::Incompatible)
					type = KCalEvent::ACTIVE;
			}
			AlarmResource** res;
			if (resource)
			{
				if (type != wantedType)
					continue;
				res = &resource;
			}
			else
			{
				switch (type)
				{
					case KCalEvent::ACTIVE:    res = &activeRes;  break;
					case KCalEvent::ARCHIVED:  res = &archivedRes;  break;
					case KCalEvent::TEMPLATE:  res = &templateRes;  break;
					default:  continue;
				}
				if (!*res)
					*res = resources->destination(type);
			}

			Event* newev = new Event(*event);

			// If there is a display alarm without display text, use the event
			// summary text instead.
			if (type == KCalEvent::ACTIVE  &&  !newev->summary().isEmpty())
			{
				const Alarm::List& alarms = newev->alarms();
				for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
				{
					Alarm* alarm = alarms[ai];
					if (alarm->type() == Alarm::Display  &&  alarm->text().isEmpty())
						alarm->setText(newev->summary());
				}
				newev->setSummary(QString());   // KAlarm only uses summary for template names
			}

			// Give the event a new ID and add it to the resources
			newev->setUid(KCalEvent::uid(CalFormat::createUniqueId(), type));
			if (resources->addEvent(newev, *res))
				saveRes = true;
			else
				success = false;
		}

		// Save the resources if they have been modified
		if (saveRes)
			resources->save();
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

/******************************************************************************
* This method must only be called from the main KAlarm queue processing loop,
* to prevent asynchronous calendar operations interfering with one another.
*
* Purge a list of archived events from the calendar.
*/
void AlarmCalendar::purgeEvents(Event::List events)
{
	for (int i = 0, end = events.count();  i < end;  ++i)
		mCalendar->deleteEvent(events[i]);
	saveCal();
}

/******************************************************************************
* Add the specified event to the calendar.
* If it is an active event and 'useEventID' is false, a new event ID is
* created. In all other cases, the event ID is taken from 'event' (if non-null).
* 'event' is updated with the actual event ID.
* The event is added to 'resource' if specified; otherwise the default resource
* is used or the user is prompted, depending on policy. If 'noPrompt' is true,
* the user will not be prompted so that if no default resource is defined, the
* function will fail.
* Reply = the KCal::Event as written to the calendar
*       = 0 if an error occurred, in which case 'event' is unchanged.
*/
Event* AlarmCalendar::addEvent(KAEvent& event, QWidget* promptParent, bool useEventID, AlarmResource* resource, bool noPrompt)
{
	if (!mOpen)
		return 0;
	// Check that the event type is valid for the calendar
	KCalEvent::Status type = event.category();
	if (type != mEventType)
	{
		switch (type)
		{
			case KCalEvent::ACTIVE:
			case KCalEvent::ARCHIVED:
			case KCalEvent::TEMPLATE:
				if (mEventType == KCalEvent::EMPTY)
					break;
				// fall through to default
			default:
				return 0;
		}
	}

	KAEvent oldEvent = event;    // so that we can reinstate it if there's an error
	QString id = event.id();
	Event* kcalEvent = new Event;
	if (type == KCalEvent::ACTIVE)
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
		id = KCalEvent::uid(id, type);
		event.setEventID(id);
		kcalEvent->setUid(id);
	}
	event.updateKCalEvent(kcalEvent, false, (type == KCalEvent::ARCHIVED), true);
	if (mCalType == RESOURCES)
	{
		bool ok;
		if (resource)
			ok = AlarmResources::instance()->addEvent(kcalEvent, resource);
		else
			ok = AlarmResources::instance()->addEvent(kcalEvent, type, promptParent, noPrompt);
		if (!ok)
		{
			event = oldEvent;
			return 0;    // kcalEvent has been deleted by AlarmResources::addEvent()
		}
	}
	else
	{
		if (!mCalendar->addEvent(kcalEvent))
		{
			event = oldEvent;
			delete kcalEvent;
			return 0;
		}
	}
	event.clearUpdated();
	return kcalEvent;
}

/******************************************************************************
* Modify the specified event in the calendar with its new contents.
* The new event must have a different event ID from the old one.
* It is assumed to be of the same event type as the old one (active, etc.)
* Reply = the new KCal::Event as written to the calendar
*       = 0 if an error occurred.
*/
Event* AlarmCalendar::modifyEvent(const QString& oldEventId, KAEvent& newEvent)

{
	QString newId = newEvent.id();
	bool noNewId = newId.isEmpty();
	if (!noNewId  &&  oldEventId == newId)
	{
		kError(5950) <<"AlarmCalendar::modifyEvent(): same IDs";
		return 0;
	}
	if (!mOpen)
		return 0;
	Event* kcalEvent = 0;
	if (mCalType == RESOURCES)
	{
		// Create a new KCal::Event, keeping any custom properties from the old event.
		// Ensure it has a new ID.
		kcalEvent = createKCalEvent(newEvent, oldEventId, (mEventType == KCalEvent::ARCHIVED), true);
		if (noNewId)
			kcalEvent->setUid(CalFormat::createUniqueId());
		AlarmResources* resources = AlarmResources::instance();
		if (!resources->addEvent(kcalEvent, resources->resourceForIncidence(oldEventId)))
			return 0;    // kcalEvent has been deleted by AlarmResources::addEvent()
		if (noNewId)
			newEvent.setEventID(kcalEvent->uid());
	}
	else
	{
		kcalEvent = addEvent(newEvent, 0, true);
		if (!kcalEvent)
			return 0;
	}
	deleteEvent(oldEventId);
	return kcalEvent;
}

/******************************************************************************
* Update the specified event in the calendar with its new contents.
* The event retains the same ID.
* Reply = the KCal::Event as written to the calendar
*       = 0 if an error occurred.
*/
Event* AlarmCalendar::updateEvent(const KAEvent& evnt)
{
	bool active = (evnt.category() == KCalEvent::ACTIVE);
	if (mOpen)
	{
		Event* kcalEvent = event(evnt.id());
		if (kcalEvent)
		{
			evnt.updateKCalEvent(kcalEvent);
			evnt.clearUpdated();
			if (active)
				Daemon::savingEvent(evnt.id());
			return kcalEvent;
		}
	}
	if (active)
		Daemon::eventHandled(evnt.id());
	return 0;
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
			bool active = (KCalEvent::status(kcalEvent) == KCalEvent::ACTIVE);
			mCalendar->deleteEvent(kcalEvent);
			if (active)
				Daemon::savingEvent(eventID);
			if (saveit)
				return save();
			return true;
		}
	}
	// Event not found. Tell daemon just in case it was an active event which was triggered.
	Daemon::eventHandled(eventID);
	return false;
}

/******************************************************************************
* Return a new KCal::Event representing the specified KAEvent.
* If the event exists in the calendar, custom properties are copied from there.
* The caller takes ownership of the returned KCal::Event. Note that the ID of
* the returned KCal::Event may be the same as an existing calendar event, so
* be careful not to end up duplicating IDs.
* If 'original' is true, the event start date/time is adjusted to its original
* value instead of its next occurrence, and the expired main alarm is
* reinstated.
*/
Event* AlarmCalendar::createKCalEvent(const KAEvent& ev, const QString& baseID, bool original, bool cancelCancelledDefer) const
{
	if (mCalType != RESOURCES)
		kFatal(5950) <<"AlarmCalendar::createKCalEvent(KAEvent): invalid for display calendar";
	// If the event exists in the calendar, we want to keep any custom
	// properties. So copy the calendar KCal::Event to base the new one on.
	QString id = baseID.isEmpty() ? ev.id() : baseID;
	Event* calEvent = id.isEmpty() ? 0 : AlarmResources::instance()->event(id);
	Event* newEvent = calEvent ? new Event(*calEvent) : new Event;
	ev.updateKCalEvent(newEvent, false, original, cancelCancelledDefer);
	newEvent->setUid(ev.id());
	return newEvent;
}

/******************************************************************************
* Return the event with the specified ID.
*/
Event* AlarmCalendar::event(const QString& uniqueID)
{
	return mCalendar ? mCalendar->event(uniqueID) : 0;
}

/******************************************************************************
 * Find the alarm template with the specified name.
 * Reply = invalid event if not found.
 */
KAEvent AlarmCalendar::templateEvent(const QString& templateName)
{
	KAEvent event;
	Event::List eventlist = events(KCalEvent::TEMPLATE);
	for (int i = 0, end = eventlist.count();  i < end;  ++i)
	{
		Event* ev = eventlist[i];
		if (ev->summary() == templateName)
		{
			event.set(ev);
			if (!event.isTemplate())
				return KAEvent();    // this shouldn't ever happen
			break;
		}
	}
	return event;
}

/******************************************************************************
* Return all events in the calendar which contain alarms.
* Optionally the event type can be filtered, using an OR of event types.
*/
Event::List AlarmCalendar::events(KCalEvent::Status type)
{
	if (!mCalendar)
		return Event::List();
	Event::List list = mCalendar->rawEvents();
	for (int i = 0;  i < list.count();  )
	{
		Event* event = list[i];
		if (event->alarms().isEmpty()
		||  type != KCalEvent::EMPTY  &&  !(type & KCalEvent::status(event)))
			list.removeAt(i);
		else
			++i;
	}
	return list;
}

/******************************************************************************
* Return all events which have alarms falling within the specified time range.
* 'type' is the OR'ed desired event types.
*/
Event::List AlarmCalendar::eventsWithAlarms(const KDateTime& from, const KDateTime& to, KCalEvent::Status type)
{
	kDebug(5950) <<"AlarmCalendar::eventsWithAlarms(" << from <<" -" << to <<")";
	Event::List evnts;
	if (!mCalendar)
		return evnts;
	KDateTime dt;
	Event::List allEvents = mCalendar->rawEvents();
	for (int i = 0, end = allEvents.count();  i < end;  ++i)
	{
		Event* e = allEvents[i];
		if (type != KCalEvent::EMPTY  &&  !(KCalEvent::status(e) & type))
			continue;
		bool recurs = e->recurs();
		int  endOffset = 0;
		bool endOffsetValid = false;
		const Alarm::List& alarms = e->alarms();
		for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
		{
			Alarm* alarm = alarms[ai];
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
								endOffset = e->hasDuration() ? e->duration().asSeconds() : e->hasEndDate() ? e->dtStart().secsTo(e->dtEnd()) : 0;
								endOffsetValid = true;
							}
							offset = alarm->endOffset().asSeconds() + endOffset;
						}
						// Adjust the 'from' date/time and find the next recurrence at or after it
						KDateTime pre = from.addSecs(-offset - 1);
						if (e->floats()  &&  pre.time() < Preferences::startOfDay())
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
					kDebug(5950) <<"AlarmCalendar::events() '" << e->summary()
					              << "':" << dt;
					evnts.append(e);
					break;
				}
			}
		}
	}
	return evnts;
}

/******************************************************************************
* Return whether an event is read-only.
*/
bool AlarmCalendar::eventReadOnly(const QString& uniqueID) const
{
	if (!mCalendar  ||  mCalType != RESOURCES)
		return true;
	AlarmResources* resources = AlarmResources::instance();
	const Event* event = resources->event(uniqueID);
	AlarmResource* resource = resources->resource(event);
	if (!resource)
		return true;
	return !resource->writable(event);
}

/******************************************************************************
* Emit a signal to indicate whether the calendar is empty.
*/
void AlarmCalendar::emitEmptyStatus()
{
	emit emptyStatus(isEmpty());
}

/******************************************************************************
* Return whether the calendar contains any events with alarms.
*/
bool AlarmCalendar::isEmpty() const
{
	if (!mCalendar)
		return true;
	Event::List list = mCalendar->rawEvents();
	if (list.isEmpty())
		return true;
	for (int i = 0, end = list.count();  i < end;  ++i)
	{
		if (!list[i]->alarms().isEmpty())
			return false;
	}
	return true;
}
