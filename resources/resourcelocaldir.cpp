/*
 *  resourcelocaldir.cpp  -  KAlarm local directory calendar resource
 *  Program:  kalarm
 *  Copyright © 2006-2009 by David Jarvie <djarvie@kde.org>
 *  Based on resourcelocaldir.cpp in libkcal (updated to rev 779953, 938673),
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
#include <kconfiggroup.h>
#include <kcal/calendarlocal.h>
#include <kcal/event.h>

#include "resourcelocaldir.moc"

using namespace KCal;

static QDateTime readLastModified(const QString& filePath);


KAResourceLocalDir::KAResourceLocalDir()
	: AlarmResource(),
	  mDirReadOnly(false)
{
	init();
}

KAResourceLocalDir::KAResourceLocalDir(const KConfigGroup& group)
	: AlarmResource(group),
	  mDirReadOnly(false)
{
	mURL = KUrl(group.readPathEntry("CalendarURL", QString()));
	init();
}

KAResourceLocalDir::KAResourceLocalDir(Type type, const QString& dirName)
	: AlarmResource(type),
	  mURL(KUrl::fromPath(dirName)),
	  mDirReadOnly(false)
{
	init();
}

void KAResourceLocalDir::init()
{
	setType("dir");   // set resource type

	//setSavePolicy(SaveDelayed);  // unnecessary for KAlarm, and would override base class setting

	connect(&mDirWatch, SIGNAL(dirty(const QString&)), SLOT(slotUpdated(const QString&)));
	connect(&mDirWatch, SIGNAL(created(const QString&)), SLOT(slotUpdated(const QString&)));
	connect(&mDirWatch, SIGNAL(deleted(const QString&)), SLOT(slotUpdated(const QString&)));
	mDirWatch.addDir(mURL.path(), KDirWatch::WatchFiles);

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

void KAResourceLocalDir::writeConfig(KConfigGroup& group)
{
	group.writePathEntry("CalendarURL", mURL.prettyUrl());
	AlarmResource::writeConfig(group);
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

bool KAResourceLocalDir::readOnly() const
{
	return mDirReadOnly || AlarmResource::readOnly();
}

void KAResourceLocalDir::setReadOnly(bool ro)
{
	// Re-evaluate the directory's read-only status (since KDirWatch
	// doesn't pick up permissions changes on the directory itself).
	QFileInfo dirInfo(mURL.path());
	mDirReadOnly = !dirInfo.isWritable();
	AlarmResource::setReadOnly(ro);
}

void KAResourceLocalDir::enableResource(bool enable)
{
	kDebug(KARES_DEBUG) << enable << ":" << mURL.path();
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

bool KAResourceLocalDir::doOpen()
{
	QFileInfo dirInfo(mURL.path());
	return dirInfo.isDir()  &&  dirInfo.isReadable();
}

void KAResourceLocalDir::slotUpdated(const QString& filepath)
{
#ifdef __GNUC__
#warning Only reload the individual file which has changed
#endif
	doLoad(false);
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
	kDebug(KARES_DEBUG) << mURL.path() << (syncCache ?": load all" :": load changes only");
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
		emit invalidate(this);
		calendar()->close();
		clearChanges();
	}
	else
	{
emit invalidate(this);  // necessary until load changes only is fixed
		oldLastModified = mLastModified;
		oldCompatibilityMap = mCompatibilityMap;
		changes = changedIncidences();
	}
	mLastModified.clear();
	mCompatibilityMap.clear();
	QString dirName = mURL.path();
	bool success = false;
	bool foundFile = false;
	if (KStandardDirs::exists(dirName)  ||  KStandardDirs::exists(dirName + '/'))
	{
		kDebug(KARES_DEBUG) << "Opening" << dirName;
		FixFunc prompt = PROMPT_PART;
		QFileInfo dirInfo(dirName);
		if (!(dirInfo.isDir()  &&  dirInfo.isReadable()))
			return false;
		mDirReadOnly = !dirInfo.isWritable();
		QDir dir(dirName, QString(), QDir::Unsorted, QDir::Files | QDir::Readable);
		QStringList entries = dir.entryList(QDir::Files | QDir::Readable);
		QStringList writable = dir.entryList(QDir::Files | QDir::Writable);
		for (int i = 0, end = entries.count();  i < end;  ++i)
		{
			// Check the next file in the directory
			QString id = entries[i];
			if (id.endsWith('~'))   // backup file, ignore it
				continue;
			QString fileName = dirName + '/' + id;
			foundFile = true;

			if (!syncCache)
			{
				// Only load new or changed events
#ifdef __GNUC__
#warning All events are actually loaded, not just new or changed
#endif
				clearChange(id);
				Event* ev = calendar()->event(id);
				if (ev  &&  changes.indexOf(ev) < 0)
				{
kDebug(KARES_DEBUG) << "Loading" << id;
					ModifiedMap::ConstIterator mit = oldLastModified.constFind(id);
					if (mit != oldLastModified.constEnd()  &&  mit.value() == readLastModified(fileName))
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
					calendar()->deleteEvent(ev);
			}
			// Load the file and check whether it's the current KAlarm format.
			// If not, only prompt the user once whether to convert it.
			if (loadFile(fileName, id, !writable.contains(id), prompt))
				success = true;
		}
		if (!foundFile)
			success = true;    // don't return error if there are no files
	}
	else if (syncCache)
	{
		kDebug(KARES_DEBUG) << "Creating" << dirName;

		// Create the directory. Use 0775 to allow group-writable if the umask
		// allows it (permissions will be 0775 & ~umask). This is desired e.g. for
		// group-shared directories!
		success = KStandardDirs::makeDir(dirName, 0775);
		mDirReadOnly = false;
	}

	if (!syncCache)
	{
		if (mLastModified.isEmpty())
		{
			emit invalidate(this);
			calendar()->close();
		}
		else
		{
			// Delete any events in the calendar for which files were not found
			Event::List oldEvents = calendar()->rawEvents();
			for (int i = 0, end = oldEvents.count();  i < end;  ++i)
			{
				if (!mCompatibilityMap.contains(oldEvents[i]))
					calendar()->deleteEvent(oldEvents[i]);
			}
		}
	}
	mLoading = false;
	enableChangeNotification();
	updateCustomEvents();
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
bool KAResourceLocalDir::loadFile(const QString& fileName, const QString& id, bool readOnly, FixFunc& prompt)
{
	bool success = false;
	CalendarLocal calendar(this->calendar()->timeSpec());
	if (!calendar.load(fileName))
	{
		// Loading this file failed, but just assume that it's not a calendar file
		kDebug(KARES_DEBUG) << fileName << "failed";
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
		kDebug(KARES_DEBUG) << fileName << ": compatibility=" << compat;
		Event::List rawEvents = calendar.rawEvents();
		for (int i = 0, end = rawEvents.count();  i < end;  ++i)
		{
			Event* ev = rawEvents[i];
			if (ev->uid() != id)
			{
				kError(KARES_DEBUG) << "Wrong event ID (" << ev->uid();
				continue;    // ignore any event with the wrong ID - it shouldn't be there!
			}
			Alarm::List alarms = ev->alarms();
			if (!alarms.isEmpty())
			{
				Event* event = ev->clone();
				if (readOnly)
					event->setReadOnly(true);
				this->calendar()->addEvent(event);
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
	kDebug(KARES_DEBUG) << mURL.path();
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
	if (mDeletedIncidences.contains(incidence))
	{
		mDeletedIncidences.removeAll(incidence);
		return true;
	}

	QString id = incidence->uid();
	QString fileName = mURL.path() + '/' + id;
	kDebug(KARES_DEBUG) << fileName;

	CalendarLocal cal(calendar()->timeSpec());
	cal.setCustomProperties(calendar()->customProperties());   // copy all VCALENDAR custom properties to each file
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
	kDebug(KARES_DEBUG);
	if (!deleteIncidenceFile(event))
		return false;
	// Remove event from added/changed lists, to avoid it being recreated in doSave()
	clearChange(event);
	disableChangeNotification();    // don't record this deletion as pending
	bool success = calendar()->deleteEvent(event);
	if (success)
		mDeletedIncidences.append(event);
	enableChangeNotification();
	return success;
}

bool KAResourceLocalDir::deleteIncidenceFile(Incidence* incidence)
{
	QFile file(mURL.path() + '/' + incidence->uid());
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
	kDebug(KARES_DEBUG) << newURL.path();
	if (isOpen())
		close();
	bool active = isActive();
	if (active)
		enableResource(false);
	mDirWatch.removeDir(mURL.path());
	mURL = newURL;
	mDirWatch.addDir(mURL.path(), KDirWatch::WatchFiles);
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

QString KAResourceLocalDir::displayLocation() const
{
	return mURL.path();
}

QString KAResourceLocalDir::displayType() const
{
	return i18nc("@info/plain Directory in filesystem", "Directory");
}

QDateTime readLastModified(const QString& filePath)
{
	QFileInfo fi(filePath);
	return fi.lastModified();
}
