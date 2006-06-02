/*
 *  resourceremote.cpp  -  KAlarm remote alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2006 by David Jarvie <software@astrojar.org.uk>
 *  Based on resourceremote.cpp in kresources,
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

#include <klocale.h>
#include <kio/job.h>
#include <kstandarddirs.h>
#include <kio/jobclasses.h>

//#include <libkdepim/progressmanager.h>

#include "resourceremote.moc"

using namespace KCal;


KAResourceRemote::KAResourceRemote(const KConfig* config)
	: AlarmResource(config),
	  mDownloadJob(0),
	  mUploadJob(0),
	  mShowProgress(true),
	  mUseCacheFile(true)
{
	if (config)
	{
		mDownloadUrl = KUrl(config->readEntry("DownloadUrl"));
		mUploadUrl = KUrl(config->readEntry("UploadUrl"));
		ResourceCached::readConfig(config );
	}
	init();
}

KAResourceRemote::KAResourceRemote(Type type, const KUrl& downloadUrl, const KUrl& uploadUrl)
	: AlarmResource(type),
	  mDownloadUrl(downloadUrl),
	  mUploadUrl(uploadUrl.isEmpty() ? mDownloadUrl : uploadUrl),
	  mDownloadJob(0),
	  mUploadJob(0),
	  mShowProgress(false),
	  mUseCacheFile(false)
{
	init();
}

void KAResourceRemote::init()
{
	setType("remote");   // set resource type
	lock(QString(""));   // don't use QString::null !
}

KAResourceRemote::~KAResourceRemote()
{
	if (isOpen())
		close();
	cancelDownload();
	if (mUploadJob)
		mUploadJob->kill();
}

void KAResourceRemote::writeConfig(KConfig* config)
{
	config->writeEntry("DownloadUrl", mDownloadUrl.url());
	config->writeEntry("UploadUrl", mUploadUrl.url());
	AlarmResource::writeConfig(config);
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
		kWarning(KARES_DEBUG) << "KAResourceRemote::doLoad(): download still in progress" << endl;
		return false;
	}
	mLoaded = false;
	mCalendar.close();
	clearChanges();
	if (!isActive())
		return false;
	mLoading = true;

	if (mUseCacheFile  ||  !syncCache)
	{
		disableChangeNotification();
		syncCache = !loadFromCache();    // if cache file doesn't exist yet, we need to download
		mUseCacheFile = false;
		enableChangeNotification();
	}
	emit resourceChanged(this);

	if (syncCache)
	{
		kDebug(KARES_DEBUG) << "KAResourceRemote::doLoad(" << mDownloadUrl.prettyUrl() << "): downloading..." << endl;
		mDownloadJob = KIO::file_copy(mDownloadUrl, KUrl(cacheFile()), -1, true,
					      false, (mShowProgress && hasGui()));
		connect(mDownloadJob, SIGNAL(result(KIO::Job*)), SLOT(slotLoadJobResult(KIO::Job*)));
#if 0
		if (mShowProgress  &&  hasGui())
		{
			connect(mDownloadJob, SIGNAL(percent(KIO::Job*, unsigned long)),
					      SLOT(slotPercent(KIO::Job*, unsigned long)));
			emit downloading(this, 0);
		}
#endif
	}
	else
	{
		kDebug(KARES_DEBUG) << "KAResourceRemote::doLoad(" << mDownloadUrl.prettyUrl() << "): from cache" << endl;
		slotLoadJobResult(0);
	}
	return true;
}

void KAResourceRemote::slotPercent(KIO::Job*, unsigned long percent)
{
#if 0
	emit downloading(this, percent);
#endif
}

void KAResourceRemote::slotLoadJobResult(KIO::Job* job)
{
	bool err = false;
	if (job)
	{
		mCalendar.close();
		clearChanges();
		if (job->error())
		{
			if (hasGui())
				job->showErrorDialog(0);
			else
				kError(KARES_DEBUG) << "Resource " << identifier() << " download error: " << job->errorString() << endl;
			setEnabled(false);
			err = true;
		}
		else
		{
			kDebug(KARES_DEBUG) << "KAResourceRemote::slotLoadJobResult(" << mDownloadUrl.prettyUrl() << "): success" << endl;
			setReloaded(true);    // the resource has now been downloaded at least once
			emit cacheDownloaded(this);
			disableChangeNotification();
			loadFromCache();
			enableChangeNotification();
			emit resourceChanged(this);
		}

		mDownloadJob = 0;
#if 0
		emit downloading(this, (unsigned long)-1);
#endif
	}

	if (!err)
	{
		checkCompatibility(cacheFile());
		mLoaded = true;
	}
	mLoading = false;
	emit loaded(this);
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
		emit loaded(this);
	}
}

bool KAResourceRemote::doSave(bool syncCache)
{
	if (saveInhibited())
		return true;
	kDebug(KARES_DEBUG) << "KAResourceRemote::doSave(" << mUploadUrl.prettyUrl() << ")" << endl;
	if (readOnly()  ||  !hasChanges())
		return true;
	if (mDownloadJob)
	{
		kWarning(KARES_DEBUG) << "KAResourceRemote::doSave(): download still in progress" << endl;
		return false;
	}
	if (mUploadJob)
	{
		kWarning(KARES_DEBUG) << "KAResourceRemote::doSave(): upload still in progress" << endl;
		return false;
	}

	mChangedIncidences = allChanges();
	saveToCache();
	if (syncCache)
	{
		mUploadJob = KIO::file_copy(KUrl(cacheFile()), mUploadUrl, -1, true, false, hasGui());
		connect(mUploadJob, SIGNAL(result(KIO::Job*)), SLOT(slotSaveJobResult(KIO::Job*)));
	}
	return true;
}

void KAResourceRemote::slotSaveJobResult(KIO::Job* job)
{
	if (job->error())
	{
		if (hasGui())
			job->showErrorDialog(0);
		else
			kError(KARES_DEBUG) << "Resource " << identifier() << " upload error: " << job->errorString() << endl;
	}
	else
	{
		kDebug(KARES_DEBUG) << "KAResourceRemote::slotSaveJobResult(" << mUploadUrl.prettyUrl() << "): success" << endl;
		clearChanges();
	}

	mUploadJob = 0;
	emit resourceSaved(this);
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
	kDebug(KARES_DEBUG) << "KAResourceRemote::setUrls(" << downloadUrl.prettyUrl() << ", " << uploadUrl.prettyUrl() << ")\n";
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

QString KAResourceRemote::displayLocation(bool prefix) const
{
	QString loc = mDownloadUrl.prettyUrl();
	return prefix ? i18n("URL: %1", loc) : loc;
}
