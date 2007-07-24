/*
 *  resourcelocal.cpp  -  KAlarm local calendar resource
 *  Program:  kalarm
 *  Copyright © 2006 by David Jarvie <software@astrojar.org.uk>
 *  Based on resourcelocal.cpp in libkcal,
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
#include <kdebug.h>
#include <kcal/calendarlocal.h>

#include "resourcelocal.moc"

using namespace KCal;


KAResourceLocal::KAResourceLocal()
	: AlarmResource()
{
	init();
}

KAResourceLocal::KAResourceLocal(const KConfigGroup& group)
	: AlarmResource(group)
{
	mURL = KUrl(group.readPathEntry("CalendarURL"));
	init();
}

KAResourceLocal::KAResourceLocal(Type type, const QString& fileName)
	: AlarmResource(type),
	  mURL(KUrl(fileName))
{
	init();
}

void KAResourceLocal::init()
{
	setType("file");   // set resource type

	connect(&mDirWatch, SIGNAL(dirty(const QString&)), SLOT(reload()));
	connect(&mDirWatch, SIGNAL(created(const QString&)), SLOT(reload()));
	connect(&mDirWatch, SIGNAL(deleted(const QString&)), SLOT(reload()));

	mDirWatch.addFile(mURL.path());
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

void KAResourceLocal::enableResource(bool enable)
{
	kDebug(KARES_DEBUG) << "KAResourceLocal::enableResource(" << enable << "): " << mURL.path() << endl;
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

bool KAResourceLocal::doLoad(bool)
{
	if (!KStandardDirs::exists(mURL.path()))
	{
		kDebug(KARES_DEBUG) << "KAResourceLocal::doLoad(): File doesn't exist yet." << endl;
		mLoaded = false;
		calendar()->close();
		clearChanges();
		if (!isActive())
			return false;
		mLoading = true;
		if (!doSave(true))   // save the empty calendar, to create the calendar file
		{
			mLoading = false;
			return false;
		}
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
	kDebug(KARES_DEBUG) << "KAResourceLocal::reload(" << mURL.path() << ")" << endl;
	if (!isOpen())
		return;
	if (mLastModified == readLastModified())
	{
		kDebug(KARES_DEBUG) << "KAResourceLocal::reload(): file not modified since last read." << endl;
		return;
	}
	loadFile();
	emit resourceChanged(this);
}

bool KAResourceLocal::loadFile()
{
	kDebug(KARES_DEBUG) << "KAResourceLocal::loadFile(" << mURL.path() << ")" << endl;
	mLoaded = false;
	calendar()->close();
	clearChanges();
	if (!isActive())
		return false;
	mLoading = true;
	disableChangeNotification();
	bool success = calendar()->load(mURL.path());
	enableChangeNotification();
	if (!success)
	{
		mLoading = false;
		return false;
	}
	mLastModified = readLastModified();
	checkCompatibility(fileName());
	mLoading = false;
	mLoaded = true;
	setReloaded(true);   // the resource has now been loaded at least once
	emit loaded(this);
	return true;
}

bool KAResourceLocal::doSave(bool)
{
	if (saveInhibited())
		return true;
	kDebug(KARES_DEBUG) << "KAResourceLocal::doSave(" << mURL.path() << ")" <<endl;
	if (mCalIDFunction)
		(*mCalIDFunction)(*calendar());    // write the application ID into the calendar
	bool success = calendar()->save(mURL.path());
	clearChanges();
	mLastModified = readLastModified();
	emit resourceSaved(this);
	return success;
}

QDateTime KAResourceLocal::readLastModified()
{
	QFileInfo fi(mURL.path());
	return fi.lastModified();
}

QString KAResourceLocal::fileName() const
{
	return mURL.path();
}

bool KAResourceLocal::setFileName(const KUrl& newURL)
{
	if (mReconfiguring == 1)
	{
		mNewURL = newURL;
		return true;
	}
	if (newURL.path() == mURL.path()  ||  !newURL.isLocalFile())
		return false;
	kDebug(KARES_DEBUG) << "KAResourceLocal::setFileName(" << newURL.path() << ")\n";
	if (isOpen())
		close();
	bool active = isActive();
	if (active)
		enableResource(false);
	mDirWatch.removeFile(mURL.path());
	mURL = newURL;
	mDirWatch.addFile(mURL.path());
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

QString KAResourceLocal::displayLocation(bool prefix) const
{
	QString loc = mURL.path();
	return prefix ? i18n("File: %1", loc) : loc;
}
