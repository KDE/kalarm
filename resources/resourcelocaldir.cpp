/*
 *  resourcelocaldir.cpp  -  KAlarm local directory calendar resource
 *  Program:  kalarm
 *  Copyright © 2006 by David Jarvie <software@astrojar.org.uk>
 *  Based on resourcelocaldir.cpp in libkcal,
 *  Copyright (c) 2003 Cornelius Schumacher <schumacher@kde.org>
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

#include <QDir>
#include <QFile>
#include <QtAlgorithms>

#include <klocale.h>
#include <kstandarddirs.h>

#include <kcal/calendarlocal.h>
#include <kcal/event.h>

#include "resourcelocaldir.moc"

using namespace KCal;

static QDateTime readLastModified(const QString& filePath);


KAResourceLocalDir::KAResourceLocalDir(const KConfig* config)
	: AlarmResource(config)
{
	if (config)
		mURL = KUrl(config->readPathEntry("CalendarURL"));
	init();
}

KAResourceLocalDir::KAResourceLocalDir(Type type, const QString& dirName)
	: AlarmResource(type),
	  mURL(KUrl(dirName))
{
	init();
}

void KAResourceLocalDir::init()
{
	setType("dir");   // set resource type

	connect(&mDirWatch, SIGNAL(dirty(const QString&)), SLOT(slotReload()));
	connect(&mDirWatch, SIGNAL(created(const QString&)), SLOT(slotReload()));
	connect(&mDirWatch, SIGNAL(deleted(const QString&)), SLOT(slotReload()));

	mDirWatch.addDir(mURL.path(), true);
	enableResource(isActive());

	// Initially load all files in the directory, then just load changes
	setReloadPolicy(ReloadOnStartup);
}

KAResourceLocalDir::~KAResourceLocalDir()
{
	mDirWatch.stopScan();
	if (isOpen())
		close();
}

void KAResourceLocalDir::writeConfig(KConfig* config)
{
	config->writePathEntry("CalendarURL", mURL.prettyUrl());
	AlarmResource::writeConfig(config);
}

void KAResourceLocalDir::startReconfig()
{
	mNewURL = mURL;
	AlarmResource::startReconfig();
}

void KAResourceLocalDir::applyReconfig()
{
	if (mReconfiguring)
	{
		AlarmResource::applyReconfig();
		if (setDirName(mNewURL))
			mReconfiguring = 3;    // indicate that location has changed
		AlarmResource::applyReconfig();
	}
}

void KAResourceLocalDir::enableResource(bool enable)
{
	kDebug(KARES_DEBUG) << "KAResourceLocalDir::enableResource(" << enable << "): " << mURL.path() << endl;
	if (enable)
	{
		lock(mURL.path());
		mDirWatch.startScan();
	}
	else
	{
		lock(QString());
		mDirWatch.stopScan();
	}
}

/******************************************************************************
* Load the files in the local directory, and add their events to our calendar.
* If 'syncCache' is true, all files are loaded; if false, only changed files
* are loaded.
* Events which contain no alarms are ignored..
* Reply = true if any file in the directory was loaded successfully.
*/
bool KAResourceLocalDir::doLoad(bool syncCache)
{
	kDebug(KARES_DEBUG) << "KAResourceLocalDir::doLoad(" << mURL.path() << (syncCache ? "): load all" : "): load changes only") << endl;
	if (!isActive()  ||  !isOpen())
		return false;
	ModifiedMap      oldLastModified;
	CompatibilityMap oldCompatibilityMap;
	Incidence::List  changes;
	mLoading = true;
	mLoaded = false;
	disableChangeNotification();
	setCompatibility(KCalendar::ByEvent);
	if (syncCache)
	{
		mCalendar.close();
		clearChanges();
	}
	else
	{
		oldLastModified = mLastModified;
		oldCompatibilityMap = mCompatibilityMap;
		changes = changedIncidences();
	}
	mLastModified.clear();
	mCompatibilityMap.clear();
	QString dirName = mURL.path();
	bool success = false;
	bool foundFile = false;
	if (KStandardDirs::exists(dirName)  ||  KStandardDirs::exists(dirName + "/"))
	{
		kDebug(KARES_DEBUG) << "KAResourceLocalDir::doLoad(): opening '" << dirName << "'" << endl;
		FixFunc prompt = PROMPT_PART;
		QDir dir(dirName);
		QStringList entries = dir.entryList(QDir::Files | QDir::Readable);
		for (int i = 0, end = entries.count();  i < end;  ++i)
		{
			// Check the next file in the directory
			QString id = entries[i];
			if (id.endsWith("~"))   // backup file, ignore it
				continue;
			QString fileName = dirName + "/" + id;
			foundFile = true;

			if (!syncCache)
			{
				// Only load new or changed events
				Event* ev = mCalendar.event(id);
				if (ev  &&  changes.indexOf(ev) < 0)
				{
					ModifiedMap::ConstIterator mit = oldLastModified.find(id);
					if (mit != oldLastModified.end()  &&  mit.value() == readLastModified(fileName))
					{
						// The file hasn't changed, and its event is unchanged
						// in our calendar, so just transfer the event to the
						// new maps without rereading the file.
						mCompatibilityMap[ev] = oldCompatibilityMap[ev];
						mLastModified[id] = mit.value();
						success = true;
						continue;
					}
				}
				// It's either a new file, or it has changed
				if (ev)
					mCalendar.deleteEvent(ev);
			}
			// Load the file and check whether it's the current KAlarm format.
			// If not, only prompt the user once whether to convert it.
			if (loadFile(fileName, id, prompt))
				success = true;
		}
		if (!foundFile)
			success = true;    // don't return error if there are no files
	}
	else if (syncCache)
	{
		kDebug(KARES_DEBUG) << "KAResourceLocalDir::doLoad(): creating '" << dirName << "'" << endl;

		// Create the directory. Use 0775 to allow group-writable if the umask
		// allows it (permissions will be 0775 & ~umask). This is desired e.g. for
		// group-shared directories!
		success = KStandardDirs::makeDir(dirName, 0775);
	}

	if (!syncCache)
	{
		if (mLastModified.isEmpty())
			mCalendar.close();
		else
		{
			// Delete any events in the calendar for which files were not found
			Event::List oldEvents = mCalendar.rawEvents();
			for (int i = 0, end = oldEvents.count();  i < end;  ++i)
			{
				if (!mCompatibilityMap.contains(oldEvents[i]))
					mCalendar.deleteEvent(oldEvents[i]);
			}
		}
	}
	mLoading = false;
	enableChangeNotification();
	if (success)
	{
		mLoaded = true;
		setReloaded(true);   // the resource has now been loaded at least once
		emit loaded(this);
		if (!syncCache)
			emit resourceChanged(this);
	}
	return success;
}

/******************************************************************************
* Load one file from the local directory, and return its event in 'event'.
* Any event whose ID is not the same as the file name, or any event not
* containing alarms, is ignored.
* Reply = true if the calendar loaded successfully (even if empty).
*/
bool KAResourceLocalDir::loadFile(const QString& fileName, const QString& id, FixFunc& prompt)
{
	bool success = false;
	CalendarLocal calendar(mCalendar.timeSpec());
	if (!calendar.load(fileName))
	{
		// Loading this file failed, but just assume that it's not a calendar file
		kDebug(KARES_DEBUG) << "KAResourceLocalDir::loadFile(): '" << fileName << "' failed" << endl;
	}
	else
	{
		KCalendar::Status compat = checkCompatibility(calendar, fileName, prompt);
		switch (compat)
		{
			case KCalendar::Converted:   // user elected to convert. Don't prompt again.
				prompt = CONVERT;
				compat = KCalendar::Current;
				break;
			case KCalendar::Convertible: // user elected not to convert. Don't prompt again.
				prompt = NO_CONVERT;
				break;
			case KCalendar::Current:
			case KCalendar::Incompatible:
			case KCalendar::ByEvent:
				break;
		}
		kDebug(KARES_DEBUG) << "KAResourceLocalDir::loadFile(): '" << fileName << "': compatibility=" << compat << endl;
		Event::List rawEvents = calendar.rawEvents();
		for (int i = 0, end = rawEvents.count();  i < end;  ++i)
		{
			Event* ev = rawEvents[i];
			if (ev->uid() != id)
			{
				kError(KARES_DEBUG) << "KAResourceLocalDir::loadFile(): wrong event ID (" << ev->uid() << ")" << endl;
				continue;    // ignore any event with the wrong ID - it shouldn't be there!
			}
			Alarm::List alarms = ev->alarms();
			if (!alarms.isEmpty())
			{
				Event* event = ev->clone();
				mCalendar.addEvent(event);
				mCompatibilityMap[event] = compat;
			}
		}
		success = true;     // at least one file has been opened successfully
	}
	mLastModified[id] = readLastModified(fileName);
	return success;
}

bool KAResourceLocalDir::doSave(bool)
{
	if (saveInhibited())
		return true;
	kDebug(KARES_DEBUG) << "KAResourceLocalDir::doSave(" << mURL.path() << ")" <<endl;
	bool success = true;
	Incidence::List list = addedIncidences();
	list += changedIncidences();
	qSort(list);
	Incidence* last = 0;
	for (int i = 0, end = list.count();  i < end;  ++i)
	{
		if (list[i] != last)
		{
			last = list[i];
			if (!doSave(true, last))
				success = false;
		}
	}
	emit resourceSaved(this);
	return success;
}

bool KAResourceLocalDir::doSave(bool, Incidence* incidence)
{
	if (saveInhibited())
		return true;
	QString id = incidence->uid();
	QString fileName = mURL.path() + "/" + id;
	kDebug(KARES_DEBUG) << "KAResourceLocalDir::doSave(): '" << fileName << "'" << endl;

	CalendarLocal cal(mCalendar.timeSpec());
	cal.setCustomProperties(mCalendar.customProperties());   // copy all VCALENDAR custom properties to each file
	if (mCalIDFunction)
		(*mCalIDFunction)(cal);                          // write the application ID into the calendar
	bool success = cal.addIncidence(incidence->clone());
	if (success)
	{
		mDirWatch.stopScan();  // prohibit the dirty() signal and a following reload
		success = cal.save(fileName);
		mDirWatch.startScan();
		clearChange(id);
		mLastModified[id] = readLastModified(fileName);
	}
	return success;
}

bool KAResourceLocalDir::addEvent(Event* event)
{
	if (!AlarmResource::addEvent(event))
		return false;
	mCompatibilityMap[event] = KCalendar::Current;
	return true;
}

bool KAResourceLocalDir::deleteEvent(Event* event)
{
	kDebug(KARES_DEBUG) << "KAResourceLocalDir::deleteEvent" << endl;
	if (!deleteIncidenceFile(event))
		return false;
	// Remove event from added/changed lists, to avoid it being recreated in doSave()
	clearChange(event);
	disableChangeNotification();    // don't record this deletion as pending
	bool success = mCalendar.deleteEvent(event);
	enableChangeNotification();
	return success;
}

bool KAResourceLocalDir::deleteIncidenceFile(Incidence* incidence)
{
	QFile file(mURL.path() + "/" + incidence->uid());
	if (!file.exists())
		return true;
	mDirWatch.stopScan();
	bool removed = file.remove();
	mDirWatch.startScan();
	return removed;
}

QString KAResourceLocalDir::dirName() const
{
	return mURL.path();
}

bool KAResourceLocalDir::setDirName(const KUrl& newURL)
{
	if (mReconfiguring == 1)
	{
		mNewURL = newURL;
		return true;
	}
	if (newURL.path() == mURL.path()  ||  !newURL.isLocalFile())
		return false;
	kDebug(KARES_DEBUG) << "KAResourceLocalDir::setDirName(" << newURL.path() << ")\n";
	if (isOpen())
		close();
	bool active = isActive();
	if (active)
		enableResource(false);
	mDirWatch.removeDir(mURL.path());
	mURL = newURL;
	mDirWatch.addDir(mURL.path(), true);
	if (active)
		enableResource(true);
	// Trigger loading the new resource, and ensure that the new configuration is saved
	emit locationChanged(this);
	return true;
}

bool KAResourceLocalDir::setLocation(const QString& dirName, const QString&)
{
	KUrl newURL = KUrl::fromPath(dirName);
	return setDirName(newURL);
}

QString KAResourceLocalDir::displayLocation(bool prefix) const
{
	QString loc = mURL.path();
	return prefix ? i18n("Directory: %1", loc) : loc;
}

QDateTime readLastModified(const QString& filePath)
{
	QFileInfo fi(filePath);
	return fi.lastModified();
}
