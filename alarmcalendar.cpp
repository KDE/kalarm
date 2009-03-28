/*
 *  alarmcalendar.cpp  -  KAlarm calendar file access
 *  Program:  kalarm
 *  Copyright © 2001-2009 by David Jarvie <djarvie@kde.org>
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
#include "alarmcalendar.moc"

#include "alarmresources.h"
#include "calendarcompat.h"
#include "eventlistmodel.h"
#include "filedialog.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "preferences.h"

#include <kcal/calendarlocal.h>
#include <kcal/vcaldrag.h>
#include <kcal/vcalformat.h>
#include <kcal/icalformat.h>

#include <kglobal.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <kio/netaccess.h>
#include <kfileitem.h>
#include <ktemporaryfile.h>
#include <kfiledialog.h>
#include <kdebug.h>

#include <QFile>
#include <QTextStream>
#include <QRegExp>

#include <unistd.h>
#include <time.h>

using namespace KCal;

QString AlarmCalendar::icalProductId()
{
	return QString::fromLatin1("-//K Desktop Environment//NONSGML " KALARM_NAME " " KALARM_VERSION "//EN");
}

static const QString displayCalendarName = QLatin1String("displaying.ics");

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
		KAlarmApp::displayFatalError(AlarmResources::creationError());
		return false;
	}
	resources->setAskDestinationPolicy(Preferences::askResource());
	resources->showProgress(true);
	mResourcesCalendar = new AlarmCalendar();
	mDisplayCalendar = new AlarmCalendar(displayCal, KCalEvent::DISPLAYING);
	CalFormat::setApplication(QLatin1String(KALARM_NAME), icalProductId());
	return true;
}

/******************************************************************************
* Terminate access to all calendars.
*/
void AlarmCalendar::terminateCalendars()
{
	delete mResourcesCalendar;
	mResourcesCalendar = 0;
	delete mDisplayCalendar;
	mDisplayCalendar = 0;
}

/******************************************************************************
* Return the display calendar, opening it first if necessary.
*/
AlarmCalendar* AlarmCalendar::displayCalendarOpen()
{
	if (mDisplayCalendar->open())
		return mDisplayCalendar;
	kError() << "Open error";
	return 0;
}

/******************************************************************************
* Find and return the event with the specified ID.
* The calendar searched is determined by the calendar identifier in the ID.
*/
KAEvent* AlarmCalendar::getEvent(const QString& uniqueID)
{
	if (uniqueID.isEmpty())
		return 0;
	KAEvent* event = mResourcesCalendar->event(uniqueID);
	if (!event)
		event = mDisplayCalendar->event(uniqueID);
	return event;
}

/******************************************************************************
* Find and return the event with the specified ID.
* The calendar searched is determined by the calendar identifier in the ID.
*/
const KCal::Event* AlarmCalendar::getKCalEvent(const QString& uniqueID)
{
	if (uniqueID.isEmpty())
		return 0;
	const KCal::Event* event = mResourcesCalendar->kcalEvent(uniqueID);
	if (!event)
		event = mDisplayCalendar->kcalEvent(uniqueID);
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
	resources->setCalIDFunction(&CalendarCompat::setID);
	resources->setFixFunction(&CalendarCompat::fix);
	resources->setCustomEventFunction(&updateResourceKAEvents);
	connect(resources, SIGNAL(resourceStatusChanged(AlarmResource*, AlarmResources::Change)), SLOT(slotResourceChange(AlarmResource*, AlarmResources::Change)));
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
		kDebug() << "RESOURCES";
		mCalendar = AlarmResources::instance();
		load();
	}
	else
	{
		if (!mUrl.isValid())
			return false;

		kDebug() << mUrl.prettyUrl();
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
		kDebug() << "RESOURCES";
		static_cast<AlarmResources*>(mCalendar)->load();
	}
	else
	{
		if (!mCalendar)
			return -2;
		CalendarLocal* calendar = static_cast<CalendarLocal*>(mCalendar);

		kDebug() << mUrl.prettyUrl();
		QString tmpFile;
		if (!KIO::NetAccess::download(mUrl, tmpFile, MainWindow::mainMainWindow()))
		{
			kError() << "Download failure";
			KMessageBox::error(0, i18nc("@info", "Cannot download calendar: <filename>%1</filename>", mUrl.prettyUrl()));
			return -1;
		}
		kDebug() << "--- Downloaded to" << tmpFile;
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
			kError() << "Error loading calendar file '" << tmpFile <<"'";
			KMessageBox::error(0, i18nc("@info", "<para>Error loading calendar:</para><para><filename>%1</filename></para><para>Please fix or delete the file.</para>", mUrl.prettyUrl()));
			// load() could have partially populated the calendar, so clear it out
			calendar->close();
			delete mCalendar;
			mCalendar = 0;
			mOpen = false;
			return -1;
		}
		if (!mLocalFile.isEmpty())
			KIO::NetAccess::removeTempFile(mLocalFile);   // removes it only if it IS a temporary file
		mLocalFile = tmpFile;
		CalendarCompat::fix(*calendar, mLocalFile);   // convert events to current KAlarm format for when calendar is saved
		updateKAEvents(0, calendar);
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
		kDebug() << "RESOURCES";
		return mCalendar->reload();
	}
	else
	{
		kDebug() << mUrl.prettyUrl();
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
		kDebug() << "RESOURCES";
		mCalendar->save();    // this emits signals resourceSaved(ResourceCalendar*)
	}
	else
	{
		if (!mOpen && newFile.isNull())
			return false;

		kDebug() << "\"" << newFile << "\"," << mEventType;
		QString saveFilename = newFile.isNull() ? mLocalFile : newFile;
		if (mCalType == LOCAL_VCAL  &&  newFile.isNull()  &&  mUrl.isLocalFile())
			saveFilename = mICalUrl.path();
		if (!static_cast<CalendarLocal*>(mCalendar)->save(saveFilename, new ICalFormat))
		{
			kError() << "Saving" << saveFilename << "failed.";
			KMessageBox::error(0, i18nc("@info", "Failed to save calendar to <filename>%1</filename>", mICalUrl.prettyUrl()));
			return false;
		}

		if (!mICalUrl.isLocalFile())
		{
			if (!KIO::NetAccess::upload(saveFilename, mICalUrl, MainWindow::mainMainWindow()))
			{
				kError() << saveFilename << "upload failed.";
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
	while (!mResourceMap.isEmpty())
		removeKAEvents(mResourceMap.begin().key(), true);
	// Flag as closed now to prevent removeKAEvents() doing silly things
	// when it's called again
	mOpen = false;
	if (mCalendar)
	{
		mCalendar->close();
		delete mCalendar;
		mCalendar = 0;
	}
}

/******************************************************************************
* Load a single resource. If the resource is cached, the cache is refreshed.
*/
void AlarmCalendar::loadResource(AlarmResource* resource, QWidget*)
{
	if (!AlarmResources::instance()->load(resource, ResourceCached::SyncCache))
		slotResourceLoaded(resource, false);
}

/******************************************************************************
* Called when a remote resource cache has completed loading.
*/
void AlarmCalendar::slotCacheDownloaded(AlarmResource* resource)
{
	slotResourceLoaded(resource, false);
}

/******************************************************************************
* Create a KAEvent instance corresponding to each KCal::Event in a resource.
* Called after the resource has completed loading.
* The event list is simply cleared if 'cal' is null.
*/
void AlarmCalendar::updateResourceKAEvents(AlarmResource* resource, KCal::CalendarLocal* cal)
{
	mResourcesCalendar->updateKAEvents(resource, cal);
}
void AlarmCalendar::updateKAEvents(AlarmResource* resource, KCal::CalendarLocal* cal)
{
	kDebug() << "AlarmCalendar::updateKAEvents(" << (resource ? resource->resourceName() : "0") << ")";
	KAEvent::List& events = mResourceMap[resource];
	int i, end;
	for (i = 0, end = events.count();  i < end;  ++i)
	{
		KAEvent* event = events[i];
		mEventMap.remove(event->id());
		delete event;
	}
	events.clear();
	if (!cal)
		return;

	KConfigGroup config(KGlobal::config(), KAEvent::commandErrorConfigGroup());
	Event::List kcalevents = cal->rawEvents();
	for (i = 0, end = kcalevents.count();  i < end;  ++i)
	{
		const Event* kcalevent = kcalevents[i];
		if (kcalevent->alarms().isEmpty())
			continue;    // ignore events without alarms

		KAEvent* event = new KAEvent(kcalevent);
		if (!event->valid())
		{
			kWarning() << "Ignoring unusable event" << kcalevent->uid();
			delete event;
			continue;    // ignore events without usable alarms
		}
		event->setResource(resource);
		events += event;
		mEventMap[kcalevent->uid()] = event;

		// Set any command execution error flags for the alarm.
		// These are stored in the KAlarm config file, not the alarm
		// calendar, since they are specific to the user's local system.
		QString cmdErr = config.readEntry(event->id());
		if (!cmdErr.isEmpty())
			event->setCommandError(cmdErr);
	}

	// Now scan the list of alarms to find the earliest one to trigger
	findEarliestAlarm(resource);
}

/******************************************************************************
* Delete a resource and all its KAEvent instances from the lists.
* Called after the resource is deleted or disabled, or the calendar is closed.
*/
void AlarmCalendar::removeKAEvents(AlarmResource* resource, bool closing)
{
	ResourceMap::Iterator rit = mResourceMap.find(resource);
	if (rit != mResourceMap.end())
	{
		KAEvent::List& events = rit.value();
		for (int i = 0, end = events.count();  i < end;  ++i)
		{
			KAEvent* event = events[i];
			mEventMap.remove(event->id());
			delete event;
		}
		mResourceMap.erase(rit);
	}
	mEarliestAlarm.remove(resource);
	// Emit signal only if we're not in the process of closing the calendar
	if (!closing  &&  mOpen)
		emit earliestAlarmChanged();
}

void AlarmCalendar::slotResourceChange(AlarmResource* resource, AlarmResources::Change change)
{
	switch (change)
	{
		case AlarmResources::Enabled:
			if (resource->isActive())
				return;
			kDebug() << "Enabled (inactive)";
			break;
		case AlarmResources::Invalidated:
			kDebug() << "Invalidated";
			break;
		case AlarmResources::Deleted:
			kDebug() << "Deleted";
			break;
		default:
			return;
	}
	// Ensure the data model is notified before deleting the KAEvent instances
	EventListModel::resourceStatusChanged(resource, change);
	removeKAEvents(resource);
}

/******************************************************************************
* Called when a resource has completed loading.
*/
void AlarmCalendar::slotResourceLoaded(AlarmResource* resource, bool success)
{
}

/******************************************************************************
* Reload a resource from its cache file, without refreshing the cache first.
*/
void AlarmCalendar::reloadFromCache(const QString& resourceID)
{
	kDebug() << resourceID;
	if (mCalendar  &&  mCalType == RESOURCES)
	{
		AlarmResource* resource = static_cast<AlarmResources*>(mCalendar)->resourceWithId(resourceID);
		if (resource)
			resource->load(ResourceCached::NoSyncCache);    // reload from cache
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
		kError() << "Empty URL";
		return false;
	}
	if (!url.isValid())
	{
		kDebug() << "Invalid URL";
		return false;
	}
	kDebug() << url.prettyUrl();

	bool success = true;
	QString filename;
	bool local = url.isLocalFile();
	if (local)
	{
		filename = url.path();
		if (!KStandardDirs::exists(filename))
		{
			kDebug() << "File '" << url.prettyUrl() <<"' not found";
			KMessageBox::error(parent, i18nc("@info", "Could not load calendar <filename>%1</filename>.", url.prettyUrl()));
			return false;
		}
	}
	else
	{
		if (!KIO::NetAccess::download(url, filename, MainWindow::mainMainWindow()))
		{
			kError() << "Download failure";
			KMessageBox::error(parent, i18nc("@info", "Cannot download calendar: <filename>%1</filename>", url.prettyUrl()));
			return false;
		}
		kDebug() << "--- Downloaded to" << filename;
	}

	// Read the calendar and add its alarms to the current calendars
	CalendarLocal cal(Preferences::timeZone(true));
	success = cal.load(filename);
	if (!success)
	{
		kDebug() << "Error loading calendar '" << filename <<"'";
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
		KAEvent::List newEvents;
		Event::List events = cal.rawEvents();
		for (int i = 0, end = events.count();  i < end;  ++i)
		{
			const Event* event = events[i];
			if (event->alarms().isEmpty()  ||  !KAEvent(event).valid())
				continue;    // ignore events without alarms, or usable alarms
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
			{
				saveRes = true;
				KAEvent* ev = mResourcesCalendar->addEvent(*res, newev);
				if (type != KCalEvent::TEMPLATE)
					newEvents += ev;
			}
			else
				success = false;
		}

		// Save the resources if they have been modified
		if (saveRes)
		{
			resources->save();
			EventListModel::alarms()->addEvents(newEvents);
		}
	}
	if (!local)
		KIO::NetAccess::removeTempFile(filename);
	return success;
}

/******************************************************************************
* Export all selected alarms to an external calendar.
* The alarms are given new unique event IDs.
* Parameters: parent = parent widget for error message boxes
* Reply = true if all alarms in the calendar were successfully imported
*       = false if any alarms failed to be imported.
*/
bool AlarmCalendar::exportAlarms(const KAEvent::List& events, QWidget* parent)
{
	bool append;
	QString file = FileDialog::getSaveFileName(KUrl("kfiledialog:///exportalarms"),
	                                           QString::fromLatin1("*.ics|%1").arg(i18nc("@info/plain", "Calendar Files")),
	                                           parent, i18nc("@title:window", "Choose Export Calendar"),
	                                           &append);
	if (file.isEmpty())
	    return false;
	KUrl url;
	url.setPath(file);
	if (!url.isValid())
	{
		kDebug() << "Invalid URL";
		return false;
	}
	kDebug() << url.prettyUrl();

	CalendarLocal calendar(Preferences::timeZone(true));
	if (append  &&  !calendar.load(file))
	{
		KIO::UDSEntry uds;
		KIO::NetAccess::stat(url, uds, parent);
		KFileItem fi(uds, url);
		if (fi.size())
		{
			kError() << "Error loading calendar file" << file << "for append";
			KMessageBox::error(0, i18nc("@info", "Error loading calendar to append to:<nl/><filename>%1</filename>", url.prettyUrl()));
			return false;
		}
	}
	CalendarCompat::setID(calendar);

	// Add the alarms to the calendar
	bool ok = true;
	bool some = false;
	for (int i = 0, end = events.count();  i < end;  ++i)
	{
		KAEvent* event = events[i];
		Event* kcalEvent = new Event;
		KCalEvent::Status type = event->category();
		QString id = KCalEvent::uid(kcalEvent->uid(), type);
		kcalEvent->setUid(id);
		event->updateKCalEvent(kcalEvent, false, (type == KCalEvent::ARCHIVED));
		if (calendar.addEvent(kcalEvent))
			some = true;
		else
			ok = false;
	}

	// Save the calendar to file
	bool success = true;
	KTemporaryFile* tempFile = 0;
	bool local = url.isLocalFile();
	if (!local)
	{
		tempFile = new KTemporaryFile;
		file = tempFile->fileName();
	}
	if (!calendar.save(file, new ICalFormat))
	{
		kError() << file << ": failed";
		KMessageBox::error(0, i18nc("@info", "Failed to save new calendar to:<nl/><filename>%1</filename>", url.prettyUrl()));
		success = false;
	}
	else if (!local  &&  !KIO::NetAccess::upload(file, url, parent))
	{
		kError() << file << ": upload failed";
		KMessageBox::error(0, i18nc("@info", "Cannot upload new calendar to:<nl/><filename>%1</filename>", url.prettyUrl()));
		success = false;
	}
	calendar.close();
	delete tempFile;
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
void AlarmCalendar::purgeEvents(const KAEvent::List& events)
{
	for (int i = 0, end = events.count();  i < end;  ++i)
		deleteEventInternal(events[i]->id());
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
* Reply = true if 'event' was written to the calendar, in which case ownership
*              of 'event' is taken by the calendar. 'event' is updated.
*       = false if an error occurred, in which case 'event' is unchanged.
*/
bool AlarmCalendar::addEvent(KAEvent* event, QWidget* promptParent, bool useEventID, AlarmResource* resource, bool noPrompt, bool* cancelled)
{
	if (cancelled)
		*cancelled = false;
	if (!mOpen)
		return false;
	// Check that the event type is valid for the calendar
	KCalEvent::Status type = event->category();
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
				return false;
		}
	}

	KAEvent oldEvent(*event);    // so that we can reinstate it if there's an error
	QString id = event->id();
	Event* kcalEvent = new Event;
	if (type == KCalEvent::ACTIVE)
	{
		if (id.isEmpty())
			useEventID = false;
		if (!useEventID)
			event->setEventID(kcalEvent->uid());
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
		event->setEventID(id);
		kcalEvent->setUid(id);
	}
	event->updateKCalEvent(kcalEvent, false, (type == KCalEvent::ARCHIVED));
	bool ok = false;
	bool remove = false;
	if (mCalType == RESOURCES)
	{
		if (!resource)
			resource = AlarmResources::instance()->destination(type, promptParent, noPrompt, cancelled);
		if (resource  &&  addEvent(resource, event))
		{
			ok = AlarmResources::instance()->addEvent(kcalEvent, resource);
			kcalEvent = 0;  // if there was an error, kcalEvent is deleted by AlarmResources::addEvent()
			remove = !ok;
		}
	}
	else
	{
		resource = 0;
		if (addEvent(0, event))
		{
			ok = mCalendar->addEvent(kcalEvent);
			remove = !ok;
		}
	}
	if (!ok)
	{
		if (remove)
		{
			// Adding to mCalendar failed, so undo AlarmCalendar::addEvent()
			mEventMap.remove(event->id());
			mResourceMap[resource].removeAll(event);
		}
		*event = oldEvent;
		delete kcalEvent;
		return false;
	}
	event->clearUpdated();
	return true;
}

/******************************************************************************
* Internal method to add an event to the calendar.
* The calendar takes ownership of 'event'.
* Reply = true if success
*       = false if error because the event ID already exists.
*/
bool AlarmCalendar::addEvent(AlarmResource* resource, KAEvent* event)
{
	if (mEventMap.contains(event->id()))
		return false;
	addNewEvent(resource, event);
	return true;
}

/******************************************************************************
* Internal method to add an event to the calendar.
* Reply = event as stored in calendar
*       = 0 if error because the event ID already exists.
*/
KAEvent* AlarmCalendar::addEvent(AlarmResource* resource, const Event* kcalEvent)
{
	if (mEventMap.contains(kcalEvent->uid()))
		return 0;
	// Create a new event
	KAEvent* ev = new KAEvent(kcalEvent);
	addNewEvent(resource, ev);
	return ev;
}

/******************************************************************************
* Internal method to add an already checked event to the calendar.
*/
void AlarmCalendar::addNewEvent(AlarmResource* resource, KAEvent* event)
{
	mResourceMap[resource] += event;
	mEventMap[event->id()] = event;
	if (resource  &&  resource->alarmType() == AlarmResource::ACTIVE
	&&  event->category() == KCalEvent::ACTIVE)
	{
		// Update the earliest alarm to trigger
		KDateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
		if (dt.isValid())
		{
			EarliestMap::ConstIterator eit = mEarliestAlarm.constFind(resource);
			KAEvent* earliest = (eit != mEarliestAlarm.constEnd()) ? eit.value() : 0;
			if (!earliest
			||  dt < earliest->nextTrigger(KAEvent::ALL_TRIGGER))
			{
				mEarliestAlarm[resource] = event;
				emit earliestAlarmChanged();
			}
		}
	}
}

/******************************************************************************
* Modify the specified event in the calendar with its new contents.
* The new event must have a different event ID from the old one.
* It is assumed to be of the same event type as the old one (active, etc.)
* Reply = true if 'newEvent' was written to the calendar, in which case ownership
*              of 'newEvent' is taken by the calendar. 'newEvent' is updated.
*       = false if an error occurred, in which case 'newEvent' is unchanged.
*/
bool AlarmCalendar::modifyEvent(const QString& oldEventId, KAEvent* newEvent)

{
	QString newId = newEvent->id();
	bool noNewId = newId.isEmpty();
	if (!noNewId  &&  oldEventId == newId)
	{
		kError() << "Same IDs";
		return false;
	}
	if (!mOpen)
		return false;
	if (mCalType == RESOURCES)
	{
		// Create a new KCal::Event, keeping any custom properties from the old event.
		// Ensure it has a new ID.
		Event* kcalEvent = createKCalEvent(newEvent, oldEventId, (mEventType == KCalEvent::ARCHIVED));
		if (noNewId)
			kcalEvent->setUid(CalFormat::createUniqueId());
		AlarmResources* resources = AlarmResources::instance();
		AlarmResource* resource = resources->resourceForIncidence(oldEventId);
		if (!resources->addEvent(kcalEvent, resource))
			return false;    // kcalEvent has been deleted by AlarmResources::addEvent()
		if (noNewId)
			newEvent->setEventID(kcalEvent->uid());
		addEvent(resource, newEvent);
	}
	else
	{
		if (!addEvent(newEvent, 0, true))
			return false;
	}
	deleteEvent(oldEventId);
	return true;
}

/******************************************************************************
* Update the specified event in the calendar with its new contents.
* The event retains the same ID.
* Reply = event which has been updated
*       = 0 if error.
*/
KAEvent* AlarmCalendar::updateEvent(const KAEvent& evnt)
{
	return updateEvent(&evnt);
}
KAEvent* AlarmCalendar::updateEvent(const KAEvent* evnt)
{
	if (mOpen)
	{
		QString id = evnt->id();
		KAEvent* kaevnt = event(id);
		Event* kcalEvent = mCalendar ? mCalendar->event(id) : 0;
		if (kaevnt  &&  kcalEvent)
		{
			evnt->updateKCalEvent(kcalEvent);
			evnt->clearUpdated();
			if (kaevnt != evnt)
				*kaevnt = *evnt;   // update the event instance in our lists, keeping the same pointer
			findEarliestAlarm(AlarmResources::instance()->resource(kcalEvent));
			return kaevnt;
		}
	}
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
		KCalEvent::Status status = deleteEventInternal(eventID);
		if (status != KCalEvent::EMPTY)
		{
			if (saveit)
				return save();
			return true;
		}
	}
	return false;
}

/******************************************************************************
* Internal method to delete the specified event from the calendar and lists.
* Reply = event status, if it was found in the CalendarLocal
*       = KCalEvent::EMPTY otherwise.
*/
KCalEvent::Status AlarmCalendar::deleteEventInternal(const QString& eventID)
{
	// Make a copy of the ID QString since the supplied reference might be
	// destructed when the event is deleted.
	const QString id = eventID;

	AlarmResource* resource = 0;
	Event* kcalEvent = mCalendar->event(eventID);
	KAEventMap::Iterator it = mEventMap.find(eventID);
	if (it != mEventMap.end())
	{
		KAEvent* ev = it.value();
		mEventMap.erase(it);
		resource = AlarmResources::instance()->resource(kcalEvent);
		mResourceMap[resource].removeAll(ev);
		bool recalc = (mEarliestAlarm[resource] == ev);
		delete ev;
		if (recalc)
			findEarliestAlarm(resource);
	}
	else
	{
		for (EarliestMap::Iterator eit = mEarliestAlarm.begin();  eit != mEarliestAlarm.end();  ++eit)
		{
			KAEvent* event = eit.value();
			if (event  &&  event->id() == eventID)
			{
				findEarliestAlarm(eit.key());
				break;
			}
		}
	}
	KCalEvent::Status status = KCalEvent::EMPTY;
	if (kcalEvent)
	{
		status = KCalEvent::status(kcalEvent);
		mCalendar->deleteEvent(kcalEvent);
	}

	// Delete any command execution error flags for the alarm.
	KConfigGroup config(KGlobal::config(), KAEvent::commandErrorConfigGroup());
	if (config.hasKey(id))
	{
	        config.deleteEntry(id);
		config.sync();
	}
	return status;
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
Event* AlarmCalendar::createKCalEvent(const KAEvent* ev, const QString& baseID, bool original) const
{
	if (mCalType != RESOURCES)
		kFatal() << "AlarmCalendar::createKCalEvent(KAEvent): invalid for display calendar";
	// If the event exists in the calendar, we want to keep any custom
	// properties. So copy the calendar KCal::Event to base the new one on.
	QString id = baseID.isEmpty() ? ev->id() : baseID;
	Event* calEvent = id.isEmpty() ? 0 : AlarmResources::instance()->event(id);
	Event* newEvent = calEvent ? new Event(*calEvent) : new Event;
	ev->updateKCalEvent(newEvent, false, original);
	newEvent->setUid(ev->id());
	return newEvent;
}

/******************************************************************************
* Return the event with the specified ID.
*/
KAEvent* AlarmCalendar::event(const QString& uniqueID)
{
	if (!mCalendar)
		return 0;
	KAEventMap::ConstIterator it = mEventMap.constFind(uniqueID);
	if (it == mEventMap.constEnd())
		return 0;
	return it.value();
}

/******************************************************************************
* Return the event with the specified ID.
*/
Event* AlarmCalendar::kcalEvent(const QString& uniqueID)
{
	return mCalendar ? mCalendar->event(uniqueID) : 0;
}

/******************************************************************************
* Find the alarm template with the specified name.
* Reply = 0 if not found.
*/
KAEvent* AlarmCalendar::templateEvent(const QString& templateName)
{
	if (templateName.isEmpty())
		return 0;
	KAEvent::List eventlist = events(KCalEvent::TEMPLATE);
	for (int i = 0, end = eventlist.count();  i < end;  ++i)
	{
		if (eventlist[i]->templateName() == templateName)
			return eventlist[i];
	}
	return 0;
}

/******************************************************************************
* Return all events in the calendar which contain alarms.
* Optionally the event type can be filtered, using an OR of event types.
*/
KAEvent::List AlarmCalendar::events(AlarmResource* resource, KCalEvent::Status type)
{
	KAEvent::List list;
	if (!mCalendar  ||  (resource && mCalType != RESOURCES))
		return list;
	if (resource)
	{
		ResourceMap::ConstIterator rit = mResourceMap.constFind(resource);
		if (rit == mResourceMap.constEnd())
			return list;
		const KAEvent::List events = rit.value();
		if (type == KCalEvent::EMPTY)
			return events;
		for (int i = 0, end = events.count();  i < end;  ++i)
			if (type & events[i]->category())
				list += events[i];
	}
	else
	{
		for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
		{
			const KAEvent::List events = rit.value();
			if (type == KCalEvent::EMPTY)
				list += events;
			else
			{
				for (int i = 0, end = events.count();  i < end;  ++i)
					if (type & events[i]->category())
						list += events[i];
			}
		}
	}
	return list;
}

/******************************************************************************
* Return all events in the calendar which contain usable alarms.
* Optionally the event type can be filtered, using an OR of event types.
*/
Event::List AlarmCalendar::kcalEvents(AlarmResource* resource, KCalEvent::Status type)
{
	Event::List list;
	if (!mCalendar  ||  (resource && mCalType != RESOURCES))
		return list;
	list = resource ? AlarmResources::instance()->rawEvents(resource) : mCalendar->rawEvents();
	for (int i = 0;  i < list.count();  )
	{
		Event* event = list[i];
		if (event->alarms().isEmpty()
		||  (type != KCalEvent::EMPTY  &&  !(type & KCalEvent::status(event)))
		||  !KAEvent(event).valid())
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
KAEvent::List AlarmCalendar::events(const KDateTime& from, const KDateTime& to, KCalEvent::Status type)
{
	kDebug() << from << "-" << to;
	KAEvent::List evnts;
	if (!mCalendar)
		return evnts;
	KDateTime dt;
	AlarmResources* resources = AlarmResources::instance();
	KAEvent::List allEvents = events(type);
	for (int i = 0, end = allEvents.count();  i < end;  ++i)
	{
		KAEvent* event = allEvents[i];
		Event* e = resources->event(event->id());
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
						if (e->allDay()  &&  pre.time() < Preferences::startOfDay())
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
					kDebug() << "'" << e->summary() << "':" << dt;
					evnts.append(event);
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
* Return the resource containing a specified event.
*/
AlarmResource* AlarmCalendar::resourceForEvent(const QString& eventID) const
{
	if (!mCalendar  ||  mCalType != RESOURCES)
		return 0;
	return AlarmResources::instance()->resourceForIncidence(eventID);
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

/******************************************************************************
* Return a list of all active at-login alarms.
*/
KAEvent::List AlarmCalendar::atLoginAlarms() const
{
	KAEvent::List atlogins;
	if (!mCalendar  ||  mCalType != RESOURCES)
		return atlogins;
	for (ResourceMap::ConstIterator rit = mResourceMap.constBegin();  rit != mResourceMap.constEnd();  ++rit)
	{
		AlarmResource* resource = rit.key();
		if (!resource  ||  resource->alarmType() != AlarmResource::ACTIVE)
			continue;
		const KAEvent::List& events = rit.value();
		for (int i = 0, end = events.count();  i < end;  ++i)
		{
			KAEvent* event = events[i];
			if (event->category() == KCalEvent::ACTIVE  &&  event->repeatAtLogin())
				atlogins += event;
		}
	}
	return atlogins;
}

/******************************************************************************
* Find and note the active alarm with the earliest trigger time for a resource.
*/
void AlarmCalendar::findEarliestAlarm(AlarmResource* resource)
{
	if (!mCalendar  ||  mCalType != RESOURCES
	||  !resource  ||  resource->alarmType() != AlarmResource::ACTIVE)
		return;
	ResourceMap::ConstIterator rit = mResourceMap.constFind(resource);
	if (rit == mResourceMap.constEnd())
		return;
	const KAEvent::List& events = rit.value();
	KAEvent* earliest = 0;
	KDateTime earliestTime;
	for (int i = 0, end = events.count();  i < end;  ++i)
	{
		KAEvent* event = events[i];
		if (event->category() != KCalEvent::ACTIVE
		||  mPendingAlarms.contains(event->id()))
			continue;
		KDateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
		if (dt.isValid()  &&  (!earliest || dt < earliestTime))
		{
			earliestTime = dt;
			earliest = event;
		}
	}
	mEarliestAlarm[resource] = earliest;
	emit earliestAlarmChanged();
}

/******************************************************************************
* Return the active alarm with the earliest trigger time.
* Reply = 0 if none.
*/
KAEvent* AlarmCalendar::earliestAlarm() const
{
	KAEvent* earliest = 0;
	KDateTime earliestTime;
	for (EarliestMap::ConstIterator eit = mEarliestAlarm.constBegin();  eit != mEarliestAlarm.constEnd();  ++eit)
	{
		KAEvent* event = eit.value();
		if (!event)
			continue;
		KDateTime dt = event->nextTrigger(KAEvent::ALL_TRIGGER).effectiveKDateTime();
		if (dt.isValid()  &&  (!earliest || dt < earliestTime))
		{
			earliestTime = dt;
			earliest = event;
		}
	}
	return earliest;
}

/******************************************************************************
* Note that an alarm which has triggered is now being processed. While pending,
* it will be ignored for the purposes of finding the earliest trigger time.
*/
void AlarmCalendar::setAlarmPending(KAEvent* event, bool pending)
{
	QString id = event->id();
	bool wasPending = mPendingAlarms.contains(id);
	kDebug() << id << "," << pending << "(was" << wasPending << ")";
	if (pending)
	{
		if (wasPending)
			return;
		mPendingAlarms.append(id);
	}
	else
	{
		if (!wasPending)
			return;
		mPendingAlarms.removeAll(id);
	}
	// Now update the earliest alarm to trigger for its resource
	findEarliestAlarm(AlarmResources::instance()->resourceForIncidence(id));
}
