/*
 *  resourceremote.cpp  -  KAlarm remote alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2006-2009 by David Jarvie <djarvie@kde.org>
 *  Based on resourceremote.cpp in kresources (updated to rev 721447),
 *  Copyright (c) 2003,2004 Cornelius Schumacher <schumacher@kde.org>
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

#include <kcal/calendarlocal.h>
#include <klocale.h>
#include <kio/job.h>
#include <kio/jobuidelegate.h>
#include <kstandarddirs.h>
#include <kio/jobclasses.h>

//#include <libkdepim/progressmanager.h>

#include "resourceremote.moc"

using namespace KCal;


KAResourceRemote::KAResourceRemote()
	: AlarmResource(),
	  mDownloadJob(0),
	  mUploadJob(0),
	  mShowProgress(true),
	  mUseCacheFile(true),
	  mRemoteReadOnly(false)
{
	init();
}

KAResourceRemote::KAResourceRemote(const KConfigGroup& group)
	: AlarmResource(group),
	  mDownloadJob(0),
	  mUploadJob(0),
	  mShowProgress(true),
	  mUseCacheFile(true),
	  mRemoteReadOnly(false)
{
	mDownloadUrl = KUrl(group.readEntry("DownloadUrl"));
	mUploadUrl = KUrl(group.readEntry("UploadUrl"));
	ResourceCached::readConfig(group );
	init();
}

KAResourceRemote::KAResourceRemote(Type type, const KUrl& downloadUrl, const KUrl& uploadUrl)
	: AlarmResource(type),
	  mDownloadUrl(downloadUrl),
	  mUploadUrl(uploadUrl.isEmpty() ? mDownloadUrl : uploadUrl),
	  mDownloadJob(0),
	  mUploadJob(0),
	  mShowProgress(false),
	  mUseCacheFile(false),
	  mRemoteReadOnly(false)
{
	init();
}

void KAResourceRemote::init()
{
	setType("remote");   // set resource type
	lock(cacheFile());
}

KAResourceRemote::~KAResourceRemote()
{
	if (isOpen())
		close();
}

void KAResourceRemote::doClose()
{
	cancelDownload();
	if (mUploadJob)
	{
		mUploadJob->kill();
		mUploadJob = 0;
	}
	AlarmResource::doClose();
}

void KAResourceRemote::writeConfig(KConfigGroup& group)
{
	group.writeEntry("DownloadUrl", mDownloadUrl.url());
	group.writeEntry("UploadUrl", mUploadUrl.url());
	AlarmResource::writeConfig(group);
}

void KAResourceRemote::startReconfig()
{
	mNewDownloadUrl = mDownloadUrl;
	mNewUploadUrl   = mUploadUrl;
	AlarmResource::startReconfig();
}

void KAResourceRemote::applyReconfig()
{
	if (mReconfiguring)
	{
		AlarmResource::applyReconfig();
		if (setUrls(mNewDownloadUrl, mNewUploadUrl))
			mReconfiguring = 3;    // indicate that location has changed
		AlarmResource::applyReconfig();
	}
}

bool KAResourceRemote::readOnly() const
{
	return mRemoteReadOnly || AlarmResource::readOnly();
}

void KAResourceRemote::enableResource(bool enable)
{
	if (!enable)
		cancelDownload(false);
}

bool KAResourceRemote::doLoad(bool syncCache)
{
	if (mUploadJob)
		syncCache = false;   // still uploading, so the cache is up-to-date
	if (mDownloadJob)
	{
		kWarning(KARES_DEBUG) << "Download still in progress";
		return true;
	}
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

	if (mUseCacheFile  ||  !syncCache)
	{
		disableChangeNotification();
		syncCache = !loadFromCache();    // if cache file doesn't exist yet, we need to download
		mUseCacheFile = false;
		enableChangeNotification();
	}
	emit resourceChanged(this);

	if (!syncCache)
	{
		kDebug(KARES_DEBUG) << mDownloadUrl.prettyUrl() << ": from cache";
		slotLoadJobResult(0);
	}
	else if (!lock()->lock())
	{
		kDebug(KARES_DEBUG) << mDownloadUrl.prettyUrl() << ": cache file is locked - something else must be loading the file";
		updateCustomEvents();
	}
	else
	{
		kDebug(KARES_DEBUG) << mDownloadUrl.prettyUrl() << ": downloading...";
		mDownloadJob = KIO::file_copy(mDownloadUrl, KUrl(cacheFile()), -1, KIO::Overwrite |
			((mShowProgress && hasGui()) ? KIO::DefaultFlags : KIO::HideProgressInfo));
		connect(mDownloadJob, SIGNAL(result(KJob*)), SLOT(slotLoadJobResult(KJob*)));
#if 0
		if (mShowProgress  &&  hasGui())
		{
			connect(mDownloadJob, SIGNAL(percent(KJob*, unsigned long)),
					      SLOT(slotPercent(KJob*, unsigned long)));
			emit downloading(this, 0);
		}
#endif
	}
	return true;
}

void KAResourceRemote::slotPercent(KJob*, unsigned long percent)
{
#if 0
	emit downloading(this, percent);
#endif
}

void KAResourceRemote::slotLoadJobResult(KJob* job)
{
	bool err = false;
	if (job)
	{
		emit invalidate(this);
		calendar()->close();
		clearChanges();
		if (job->error())
		{
			if (hasGui())
			{
				KIO::FileCopyJob* j = qobject_cast<KIO::FileCopyJob*>(job);
				if (j)
					j->ui()->showErrorMessage();
			}
			kError(KARES_DEBUG) << "Resource" << identifier() << " download error:" << job->errorString();
			setEnabled(false);
			err = true;
		}
		else
		{
			kDebug(KARES_DEBUG) << mDownloadUrl.prettyUrl() << ": success";
			setReloaded(true);    // the resource has now been downloaded at least once
			emit cacheDownloaded(this);
			disableChangeNotification();
			loadFromCache();
			enableChangeNotification();
		}

#if 0
		emit downloading(this, (unsigned long)-1);
#endif
	}
	mDownloadJob = 0;

	if (!err)
	{
		checkCompatibility(cacheFile());
		mLoaded = true;
	}
	mLoading = false;
	lock()->unlock();
	updateCustomEvents();
	emit loaded(this);
	if (job  &&  !err)
		emit resourceChanged(this);
}

void KAResourceRemote::cancelDownload(bool disable)
{
	if (mDownloadJob)
	{
		mDownloadJob->kill();
		mDownloadJob = 0;
		if (disable)
			setEnabled(false);
#if 0
		emit downloading(this, (unsigned long)-1);
#endif
		mLoading = false;
		lock()->unlock();
		updateCustomEvents();
		emit loaded(this);
	}
}

bool KAResourceRemote::doSave(bool syncCache)
{
	kDebug(KARES_DEBUG) << mUploadUrl.prettyUrl();
	if (readOnly()  ||  !hasChanges())
		return true;
	if (mDownloadJob)
	{
		kWarning(KARES_DEBUG) << "Download still in progress";
		return false;
	}
	if (mUploadJob)
	{
		kWarning(KARES_DEBUG) << "Upload still in progress";
		return false;
	}

	mChangedIncidences = allChanges();
	if (mCalIDFunction)
		(*mCalIDFunction)(*calendar());    // write the application ID into the calendar
	saveToCache();
	if (syncCache)
	{
		mUploadJob = KIO::file_copy(KUrl(cacheFile()), mUploadUrl, -1, KIO::Overwrite | (hasGui()?KIO::DefaultFlags:KIO::HideProgressInfo));
		connect(mUploadJob, SIGNAL(result(KJob*)), SLOT(slotSaveJobResult(KJob*)));
	}
	return true;
}

void KAResourceRemote::slotSaveJobResult(KJob* job)
{
	if (job->error())
	{
		if (hasGui())
		{
			KIO::FileCopyJob* j = qobject_cast<KIO::FileCopyJob*>(job);
			if (j)
				j->ui()->showErrorMessage();
		}
		kError(KARES_DEBUG) << "Resource" << identifier() << " upload error:" << job->errorString();
	}
	else
	{
		kDebug(KARES_DEBUG) << mUploadUrl.prettyUrl() << ": success";
		clearChanges();
	}

	mUploadJob = 0;
	emit resourceSaved(this);
	if (closeAfterSave())
		close();
}

bool KAResourceRemote::setUrls(const KUrl& downloadUrl, const KUrl& uploadUrl)
{
	if (mReconfiguring == 1)
	{
		mNewDownloadUrl = downloadUrl;
		mNewUploadUrl   = uploadUrl;
		return true;
	}
	if (downloadUrl.equals(mDownloadUrl)
	&&  uploadUrl.equals(mUploadUrl))
		return false;
	kDebug(KARES_DEBUG) << downloadUrl.prettyUrl() << "," << uploadUrl.prettyUrl();
	if (isOpen())
		close();
	bool active = isActive();
	if (active)
		enableResource(false);
	mDownloadUrl = downloadUrl;
	mUploadUrl   = uploadUrl;
	if (active)
		enableResource(true);
	// Trigger loading the new resource, and ensure that the new configuration is saved
	emit locationChanged(this);
	return true;
}

bool KAResourceRemote::setLocation(const QString& downloadUrl, const QString& uploadUrl)
{
	return setUrls(KUrl(downloadUrl), KUrl(uploadUrl));
}

QStringList KAResourceRemote::location() const
{
	return QStringList(downloadUrl().url()) << uploadUrl().url();
}

QString KAResourceRemote::displayLocation() const
{
	return mDownloadUrl.prettyUrl();
}

QString KAResourceRemote::displayType() const
{
	return i18nc("@info/plain", "URL");
}
