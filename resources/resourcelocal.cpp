/*
 *  resourcelocal.cpp  -  KAlarm local calendar resource
 *  Program:  kalarm
 *  Copyright (c) 2006 by David Jarvie <software@astrojar.org.uk>
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

#include "resourcelocal.moc"

using namespace KCal;


KAResourceLocal::KAResourceLocal(const KConfig* config)
	: AlarmResource(config)
{
	if (config)
		mURL = KUrl(config->readPathEntry("CalendarURL"));
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

void KAResourceLocal::writeConfig(KConfig* config)
{
	config->writePathEntry("CalendarURL", mURL.prettyURL());
	AlarmResource::writeConfig(config);
}

void KAResourceLocal::startReconfig()
{
	mNewURL = mURL;
	AlarmResource::startReconfig();
}

void KAResourceLocal::applyReconfig()
{
kDebug(5950)<<"***applyReconfig()"<<endl;
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
	kDebug(5950) << "KAResourceLocal::enableResource(" << enable << "): " << mURL.path() << endl;
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

bool KAResourceLocal::doLoad()
{
	if (!KStandardDirs::exists(mURL.path()))
	{
		kDebug(5950) << "KAResourceLocal::doLoad(): File doesn't exist yet." << endl;
		mLoaded = false;
		mCalendar.close();
		if (!isActive())
			return false;
		mLoading = true;
		if (!doSave())   // save the empty calendar, to create the calendar file
		{
			mLoading = false;
			return false;
		}
		setCompatibility(KCalendar::Current);
		mLoading = false;
		mLoaded = true;
		emit loaded(this);
		return true;
	}
	return loadFile();
}

void KAResourceLocal::reload()
{
	kDebug(5950) << "KAResourceLocal::reload(" << mURL.path() << ")" << endl;
	if (!isOpen())
		return;
	if (mLastModified == readLastModified())
	{
		kDebug(5950) << "KAResourceLocal::reload(): file not modified since last read." << endl;
		return;
	}
	loadFile();
	emit resourceChanged(this);
}

bool KAResourceLocal::loadFile()
{
	kDebug(5950) << "KAResourceLocal::loadFile(" << mURL.path() << ")" << endl;
	mLoaded = false;
	mCalendar.close();
	if (!isActive())
		return false;
	mLoading = true;
	if (!mCalendar.load(mURL.path()))
	{
		mLoading = false;
		return false;
	}
	mLastModified = readLastModified();
	checkCompatibility(fileName());
	mLoading = false;
	mLoaded = true;
	emit loaded(this);
	return true;
}

bool KAResourceLocal::doSave()
{
kDebug(5950)<<"***doSave("<<mURL.path()<<")"<<endl;
	bool success = mCalendar.save(mURL.path());
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

bool KAResourceLocal::setFileName(const QString& fileName)
{
	KUrl newURL(fileName);
	return setFileName(newURL);
}

bool KAResourceLocal::setFileName(const KUrl& newURL)
{
	if (mReconfiguring == 1)
	{
		mNewURL = newURL;
		return true;
	}
	if (newURL.path() == mURL.path())
		return false;
	kDebug(5950) << "KAResourceLocal::setFileName(" << newURL.path() << ")\n";
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

QString KAResourceLocal::location(bool prefix) const
{
	QString loc = mURL.path();
	return prefix ? i18n("File: %1", loc) : loc;
}
