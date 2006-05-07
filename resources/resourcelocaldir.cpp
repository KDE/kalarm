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
#include <klocale.h>
#include <kstandarddirs.h>

#include <libkcal/calendarlocal.h>
#include <libkcal/event.h>

#include "resourcelocaldir.moc"

using namespace KCal;


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

	connect(&mDirWatch, SIGNAL(dirty(const QString&)), SLOT(reload(const QString&)));
	connect(&mDirWatch, SIGNAL(created(const QString&)), SLOT(reload(const QString&)));
	connect(&mDirWatch, SIGNAL(deleted(const QString&)), SLOT(reload(const QString&)));

	mDirWatch.addDir(mURL.path(), true);
	enableResource(isActive());
}

KAResourceLocalDir::~KAResourceLocalDir()
{
	mDirWatch.stopScan();
	if (isOpen())
		close();
}

void KAResourceLocalDir::writeConfig(KConfig* config)
{
	config->writePathEntry("CalendarURL", mURL.prettyURL());
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
	kDebug(5950) << "KAResourceLocalDir::enableResource(" << enable << "): " << mURL.path() << endl;
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
* Load all the files in the local directory.
* Add their events to our calendar, omitting any which contain no alarms.
* Reply = true if any file in the directory was loaded successfully.
*/
bool KAResourceLocalDir::doLoad()
{
	kDebug(5950) << "KAResourceLocalDir::doLoad(" << mURL.path() << ")" << endl;
	mLoading = true;
	mLoaded = false;
	mCalendar.close();
	if (!isActive())
		return false;
	mCompatibilityMap.clear();
	QString dirName = mURL.path();
	bool success = false;
	bool emptyDir = true;
	setCompatibility(KCalendar::ByEvent);
	if (!(KStandardDirs::exists(dirName) || KStandardDirs::exists(dirName + "/")))
	{
		kDebug(5950) << "KAResourceLocalDir::doLoad(): creating '" << dirName << "'" << endl;

		// Create the directory. Use 0775 to allow group-writable if the umask
		// allows it (permissions will be 0775 & ~umask). This is desired e.g. for
		// group-shared directories!
		success = KStandardDirs::makeDir(dirName, 0775);
	}
	else
	{
		kDebug(5950) << "KAResourceLocalDir::doLoad(): opening '" << dirName << "'" << endl;
		FixFunc prompt = PROMPT_PART;
		QDir dir(dirName);
		QStringList entries = dir.entryList(QDir::Files | QDir::Readable);
		for (QStringList::ConstIterator it = entries.begin();  it != entries.end();  ++it)
		{
			if ((*it).endsWith("~"))   // backup file, ignore it
				continue;

			// Load the next file in the directory, and check whether it's
			// the current KAlarm format. If not, only prompt the user once
			// whether to convert it.
			emptyDir = false;
			QString fileName = dirName + "/" + *it;
			CalendarLocal calendar(mCalendar.timeZoneId());
			if (!calendar.load(fileName))
			{
				// Loading this file failed, but just assume that it's not a calendar file
				kDebug(5950) << "KAResourceLocalDir::doLoad(): '" << fileName << "' failed" << endl;
			}
			else
			{
kDebug(5950) << "%%%doLoad(): '" << fileName << "' checking compat..." << endl;
				KCalendar::Status compat = checkCompatibility(calendar, fileName, prompt);
kDebug(5950) << "%%%doLoad(): '" << fileName << "' compat="<<compat << endl;
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
				kDebug(5950) << "KAResourceLocalDir::doLoad(): '" << fileName << "': compatibility=" << compat << endl;
				Event::List events = calendar.rawEvents();
				for (Event::List::Iterator it = events.begin();  it != events.end();  ++it)
				{
					Event* event = *it;
					Alarm::List alarms = event->alarms();
					if (!alarms.isEmpty())
					{
						Event* ev = event->clone();
						mCalendar.addEvent(ev);
						mCompatibilityMap[ev] = compat;
					}
				}
				success = true;     // at least one file has been opened successfully
			}
		}
	}
	mLoading = false;
	if (!success)
		return false;
	mLoaded = true;
	emit loaded(this);
	return true;
}

void KAResourceLocalDir::reload(const QString& file)
{
	kDebug(5950) << "KAResourceLocalDir::reload(" << file << ")" << endl;
	refresh();
}

bool KAResourceLocalDir::doSave()
{
	bool success = true;
	Incidence::List list = addedIncidences();
	for (Incidence::List::iterator it = list.begin();  it != list.end();  ++it)
		if (!doSave(*it))
			success = false;
	list = changedIncidences();
	for (Incidence::List::iterator it = list.begin();  it != list.end();  ++it)
		if (!doSave(*it))
			success = false;
	emit resourceSaved(this);
	return success;
}

bool KAResourceLocalDir::doSave(Incidence* incidence)
{

	QString fileName = mURL.path() + "/" + incidence->uid();
	kDebug(5950) << "KAResourceLocalDir::doSave(): '" << fileName << "'" << endl;

	CalendarLocal cal(mCalendar.timeZoneId());
	cal.setCustomProperties(mCalendar.customProperties());   // copy all VCALENDAR custom properties to each file
	bool success = cal.addIncidence(incidence->clone());
	if (success)
	{
		mDirWatch.stopScan();  // prohibit the dirty() signal and a following reload()
		success = cal.save(fileName);
		mDirWatch.startScan();
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
	kDebug(5950) << "KAResourceLocalDir::deleteEvent" << endl;
	if (!deleteIncidenceFile(event))
		return false;
	return mCalendar.deleteEvent(event);
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

bool KAResourceLocalDir::setDirName(const QString& dirName)
{
	KUrl newURL(dirName);
	return setDirName(newURL);
}

bool KAResourceLocalDir::setDirName(const KUrl& newURL)
{
	if (mReconfiguring == 1)
	{
		mNewURL = newURL;
		return true;
	}
	if (newURL.path() == mURL.path())
		return false;
	kDebug(5950) << "KAResourceLocalDir::setDirName(" << newURL.path() << ")\n";
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

QString KAResourceLocalDir::location(bool prefix) const
{
	QString loc = mURL.path();
	return prefix ? i18n("Directory: %1", loc) : loc;
}
