/*
 *  resourcelocal.h  -  KAlarm local calendar resource
 *  Program:  kalarm
 *  Copyright (c) 2006 by David Jarvie <software@astrojar.org.uk>
 *  Based on resourcelocal.h in libkcal,
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

#ifndef KARESOURCELOCAL_H
#define KARESOURCELOCAL_H

/* @file resourcelocal.h - KAlarm local calendar resource */

#include <QDateTime>

#include <kurl.h>
#include <kdirwatch.h>

#include "alarmresource.h"


/** A KAlarm calendar resource stored as a local file. */
class KAResourceLocal : public AlarmResource
{
		Q_OBJECT
	public:
		/** Create resource from configuration information stored in a KConfig object. */
		KAResourceLocal(const KConfig*);
		/** Create resource for file named @a fileName. */
		KAResourceLocal(Type, const QString& fileName);
		virtual ~KAResourceLocal();

		QString      fileName() const;
		bool         setFileName(const QString& fileName);
		virtual QString location(bool prefix = false) const;
		virtual void writeConfig(KConfig*);
		virtual void startReconfig();
		virtual void applyReconfig();

		// Override unused virtual functions
		virtual KCal::Todo::List rawTodos(KCal::TodoSortField = KCal::TodoSortUnsorted, KCal::SortDirection = KCal::SortDirectionAscending)  { return KCal::Todo::List(); }
		virtual KCal::Journal::List rawJournals(KCal::JournalSortField = KCal::JournalSortUnsorted, KCal::SortDirection = KCal::SortDirectionAscending)  { return KCal::Journal::List(); }

	protected:
		/**
		  Called by reload() to reload the resource, if it is already open.
		  @return true if successful, else false. If true is returned,
			  reload() will emit a resourceChanged() signal.
		*/
		virtual bool doLoad();
		virtual bool doSave();
		QDateTime    readLastModified();
		virtual void enableResource(bool enable);

	protected slots:
		void  reload();

	private:
		void  init();
		bool  loadFile();
		bool  setFileName(const KUrl&);

		KUrl        mURL;
		KUrl        mNewURL;    // new file name to be applied by applyReconfig()
		KDirWatch   mDirWatch;
		QDateTime   mLastModified;
};

#endif
