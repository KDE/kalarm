/*
 *  resourcelocal.cpp  -  KAlarm local calendar resource
 *  Program:  kalarm
 *  Copyright © 2006-2008 by David Jarvie <djarvie@kde.org>
 *  Based on resourcelocal.cpp in libkcal (updated to rev 765072),
 *  Copyright (c) 1998 Preston Brown <pbrown@kde.org>
 *  Copyright (c) 2001,2003 Cornelius Schumacher <schumacher@kde.org>
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

#include <QFileInfo>

#include <kstandarddirs.h>
#include <klocale.h>
#include <kconfiggroup.h>
#include <kdebug.h>
#include <kcal/calendarlocal.h>

#include "resourcelocal.moc"

using namespace KCal;


KAResourceLocal::KAResourceLocal()
	: AlarmResource(),
	  mFileReadOnly(false)
{
	init();
}

KAResourceLocal::KAResourceLocal(const KConfigGroup& group)
	: AlarmResource(group),
	  mFileReadOnly(false)
{
	mURL = KUrl(group.readPathEntry("CalendarURL", QString()));
	init();
}

KAResourceLocal::KAResourceLocal(Type type, const QString& fileName)
	: AlarmResource(type),
	  mURL(KUrl::fromPath(fileName)),
	  mFileReadOnly(false)
{
	init();
}

void KAResourceLocal::init()
{
	setType("file");   // set resource type

	//setSavePolicy(SaveDelayed);  // unnecessary for KAlarm, and would override base class setting

	connect(&mDirWatch, SIGNAL(dirty(const QString&)), SLOT(reload()));
	connect(&mDirWatch, SIGNAL(created(const QString&)), SLOT(reload()));
	connect(&mDirWatch, SIGNAL(deleted(const QString&)), SLOT(reload()));

	mDirWatch.addFile(mURL.toLocalFile());
	enableResource(isActive());
}

KAResourceLocal::~KAResourceLocal()
{
	mDirWatch.stopScan();
	if (isOpen())
		close();
}

void KAResourceLocal::writeConfig(KConfigGroup& group)
{
	group.writePathEntry("CalendarURL", mURL.prettyUrl());
	AlarmResource::writeConfig(group);
}

void KAResourceLocal::startReconfig()
{
	mNewURL = mURL;
	AlarmResource::startReconfig();
}

void KAResourceLocal::applyReconfig()
{
	if (mReconfiguring)
	{
		AlarmResource::applyReconfig();
		if (setFileName(mNewURL))
			mReconfiguring = 3;    // indicate that location has changed
		AlarmResource::applyReconfig();
	}
}

bool KAResourceLocal::readOnly() const
{
	return mFileReadOnly || AlarmResource::readOnly();
}

void KAResourceLocal::enableResource(bool enable)
{
	kDebug(KARES_DEBUG) << enable << ":" << mURL.toLocalFile();
	if (enable)
	{
		lock(mURL.toLocalFile());
		mDirWatch.startScan();
	}
	else
	{
		lock(QString());
		mDirWatch.stopScan();
	}
}

bool KAResourceLocal::doLoad(bool)
{
	if (!KStandardDirs::exists(mURL.toLocalFile()))
	{
		kDebug(KARES_DEBUG) << "File doesn't exist yet.";
		mLoaded = false;
		emit invalidate(this);
		calendar()->close();
		clearChanges();
		updateCustomEvents(false);   // calendar is now empty
		if (!isActive())
			return false;
		mLoading = true;
		if (!doSave(true))   // save the empty calendar, to create the calendar file
		{
			mLoading = false;
			return false;
		}
		mFileReadOnly = false;
		setCompatibility(KCalendar::Current);
		mLoading = false;
		mLoaded = true;
		setReloaded(true);   // the resource has now been loaded at least once
		emit loaded(this);
		return true;
	}
	return loadFile();
}

void KAResourceLocal::reload()
{
	kDebug(KARES_DEBUG) << mURL.toLocalFile();
	if (!isOpen())
		return;
	if (mLastModified == readLastModified())
	{
		kDebug(KARES_DEBUG) << "File not modified since last read.";
		QFileInfo fi(mURL.toLocalFile());
		mFileReadOnly = !fi.isWritable();
		return;
	}
	loadFile();
	emit resourceChanged(this);
}

bool KAResourceLocal::loadFile()
{
	kDebug(KARES_DEBUG) << mURL.toLocalFile();
	mLoaded = false;
	emit invalidate(this);
	calendar()->close();
	clearChanges();
	if (!isActive())
	{
		updateCustomEvents(false);   // calendar is now empty
		return false;
	}
	mLoading = true;
	disableChangeNotification();
	bool success = calendar()->load(mURL.toLocalFile());
	enableChangeNotification();
	if (!success)
	{
		mLoading = false;
		updateCustomEvents();
		return false;
	}
	mLastModified = readLastModified();
	QFileInfo fi(mURL.toLocalFile());
	mFileReadOnly = !fi.isWritable();
	checkCompatibility(fileName());
	mLoading = false;
	updateCustomEvents();
	mLoaded = true;
	setReloaded(true);   // the resource has now been loaded at least once
	emit loaded(this);
	return true;
}

bool KAResourceLocal::doSave(bool)
{
	kDebug(KARES_DEBUG) << mURL.toLocalFile();
	if (mCalIDFunction)
		(*mCalIDFunction)(*calendar());    // write the application ID into the calendar
	bool success = calendar()->save(mURL.toLocalFile());
	clearChanges();
	mLastModified = readLastModified();
	emit resourceSaved(this);
	return success;
}

QDateTime KAResourceLocal::readLastModified()
{
	QFileInfo fi(mURL.toLocalFile());
	return fi.lastModified();
}

QString KAResourceLocal::fileName() const
{
	return mURL.toLocalFile();
}

bool KAResourceLocal::setFileName(const KUrl& newURL)
{
	if (mReconfiguring == 1)
	{
		mNewURL = newURL;
		return true;
	}
	if ( !newURL.isLocalFile() || newURL.toLocalFile() == mURL.toLocalFile() )
		return false;
	kDebug(KARES_DEBUG) << newURL.toLocalFile();
	if (isOpen())
		close();
	bool active = isActive();
	if (active)
		enableResource(false);
	mDirWatch.removeFile(mURL.toLocalFile());
	mURL = newURL;
	mDirWatch.addFile(mURL.toLocalFile());
	if (active)
		enableResource(true);
	// Trigger loading the new resource, and ensure that the new configuration is saved
	emit locationChanged(this);
	return true;
}

bool KAResourceLocal::setLocation(const QString& fileName, const QString&)
{
	KUrl newURL = KUrl::fromPath(fileName);
	return setFileName(newURL);
}

QString KAResourceLocal::displayLocation() const
{
	return mURL.toLocalFile();
}

QString KAResourceLocal::displayType() const
{
	return i18nc("@info/plain", "File");
}
