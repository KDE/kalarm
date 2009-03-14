/*
 *  resourcelocaldir.h  -  KAlarm local directory alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2006-2009 by David Jarvie <djarvie@kde.org>
 *  Based on resourcelocaldir.h in libkcal,
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

#ifndef RESOURCELOCALDIR_H
#define RESOURCELOCALDIR_H

/* @file resourcelocaldir.h - KAlarm local directory alarm calendar resource */

#include "alarmresource.h"

#include <kurl.h>
#include <kdirwatch.h>
#include <QMap>
#include <QList>

namespace KCal {
    class Incidence;
}


/** A KAlarm calendar resource stored in a directory as one file per alarm. */
class KALARM_EXPORT KAResourceLocalDir : public AlarmResource
{
		Q_OBJECT
	public:
		KAResourceLocalDir();
		/** Create resource from configuration information stored in a KConfig object. */
		explicit KAResourceLocalDir(const KConfigGroup&);
		/** Create resource for directory named @p dirName. */
		KAResourceLocalDir(Type, const QString& dirName);
		virtual ~KAResourceLocalDir();

		const KUrl&  url() const  { return mURL; }
		QString      dirName() const;
		bool         setDirName(const KUrl&);
		virtual QString     displayType() const;
		virtual QString     displayLocation() const;
		virtual QStringList location() const   { return QStringList(dirName()); }
		virtual bool        setLocation(const QString& dirName, const QString& = QString());
		virtual bool readOnly() const;
		virtual void setReadOnly(bool);
		virtual bool addEvent(KCal::Event*);
		virtual bool deleteEvent(KCal::Event*);
		virtual void writeConfig(KConfigGroup&);
		virtual void startReconfig();
		virtual void applyReconfig();

		// Override unused virtual functions
		virtual KCal::Todo::List rawTodos(KCal::TodoSortField = KCal::TodoSortUnsorted, KCal::SortDirection = KCal::SortDirectionAscending)  { return KCal::Todo::List(); }
		virtual KCal::Journal::List rawJournals(KCal::JournalSortField = KCal::JournalSortUnsorted, KCal::SortDirection = KCal::SortDirectionAscending)  { return KCal::Journal::List(); }

	protected:
		virtual bool doOpen();
		/** Load the resource. If 'syncCache' is true, all files in the directory
		 *  are reloaded. If 'syncCache' is false, only changed files are reloaded. */
		virtual bool doLoad(bool syncCache);

		virtual bool doSave(bool syncCache);
		bool         doSave(bool syncCache, KCal::Incidence*);
		virtual void enableResource(bool enable);

	private slots:
		void         slotUpdated(const QString& filepath);

	private:
		void         init();
		bool         loadFile(const QString& fileName, const QString& id, bool readOnly, FixFunc& prompt);
		bool         deleteIncidenceFile(KCal::Incidence *incidence);
		// Inherited virtual methods which are not used by derived classes
		using ResourceCalendar::doLoad;
		virtual bool doSave() { return false; }
		virtual bool doSave(KCal::Incidence*) { return false; }

		KUrl        mURL;
		KUrl        mNewURL;    // new directory to be applied by applyReconfig()
		KDirWatch   mDirWatch;
		typedef QMap<QString, QDateTime> ModifiedMap;
		ModifiedMap mLastModified;
		QList<KCal::Incidence*> mDeletedIncidences;
		bool        mDirReadOnly;  // directory is read-only
};

#endif
