/*
 *  resourceremote.h  -  KAlarm remote alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2006-2009 by David Jarvie <djarvie@kde.org>
 *  Based on resourceremote.h in kresources,
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

#ifndef RESOURCEREMOTE_H
#define RESOURCEREMOTE_H

/* @file resourceremote.h - KAlarm remote alarm calendar resource */

#include <kurl.h>

#include <kcal/incidence.h>

#include "alarmresource.h"

class KJob;

namespace KIO {
  class FileCopyJob;
  class Job;
}
namespace KPIM { class ProgressItem; }
namespace KCal { class CalendarLocal; }


/** A KAlarm calendar resource stored as a remote file. */
class KALARM_EXPORT KAResourceRemote : public AlarmResource
{
		Q_OBJECT
	public:
		KAResourceRemote();
		/** Create resource from configuration information stored in a KConfig object. */
		explicit KAResourceRemote(const KConfigGroup&);
		/** Create remote resource.
		 *  @param downloadUrl URL used to download iCalendar file
		 *  @param uploadUrl   URL used to upload iCalendar file. */
		KAResourceRemote(Type, const KUrl& downloadUrl, const KUrl& uploadUrl = KUrl());
		virtual ~KAResourceRemote();
		bool         setUrls(const KUrl& downloadUrl, const KUrl& uploadUrl);
		KUrl         downloadUrl() const                { return mDownloadUrl; }
		KUrl         uploadUrl() const                  { return mUploadUrl; }
		virtual QString     displayType() const;
		virtual QString     displayLocation() const;
		virtual QStringList location() const;
		virtual bool        setLocation(const QString& downloadUrl, const QString& uploadUrl);
		virtual bool readOnly() const;
		virtual void showProgress(bool show)            { mShowProgress = show; }
		virtual void writeConfig(KConfigGroup&);
		virtual void startReconfig();
		virtual void applyReconfig();
		virtual bool isSaving()                         { return mUploadJob; }
		virtual bool cached() const                     { return true; }

		// Override abstract virtual functions
		virtual KCal::Todo::List rawTodos(KCal::TodoSortField = KCal::TodoSortUnsorted, KCal::SortDirection = KCal::SortDirectionAscending)  { return KCal::Todo::List(); }
		virtual KCal::Journal::List rawJournals(KCal::JournalSortField = KCal::JournalSortUnsorted, KCal::SortDirection = KCal::SortDirectionAscending)  { return KCal::Journal::List(); }

	public slots:
		virtual void cancelDownload(bool disable = true);

	protected:
		virtual bool doLoad(bool syncCache);
		virtual bool doSave(bool syncCache);
		virtual bool doSave(bool syncCache, KCal::Incidence* i)  { return AlarmResource::doSave(syncCache, i); }
		virtual void doClose();
		virtual void enableResource(bool enable);

	private slots:
		void slotLoadJobResult(KJob*);
		void slotSaveJobResult(KJob*);
		void slotPercent(KJob*, unsigned long percent);

	private:
		void init();
		// Inherited virtual methods which are not used by derived classes
		using ResourceCalendar::doLoad;
		virtual bool doSave() { return false; }
		virtual bool doSave(KCal::Incidence*) { return false; }

		KUrl                  mDownloadUrl;
		KUrl                  mUploadUrl;
		KIO::FileCopyJob*     mDownloadJob;
		KIO::FileCopyJob*     mUploadJob;
		KCal::Incidence::List mChangedIncidences;
		KUrl                  mNewDownloadUrl;      // new download URL to be applied by applyReconfig()
		KUrl                  mNewUploadUrl;        // new upload URL to be applied by applyReconfig()
		bool                  mShowProgress;        // emit download progress signals
		bool                  mUseCacheFile;        // true to initially use cache until file can be downloaded
		bool                  mRemoteReadOnly;      // the remote file is read-only
};

#endif
