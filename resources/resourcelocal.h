/*
 *  resourcelocal.h  -  KAlarm local calendar resource
 *  Program:  kalarm
 *  Copyright © 2006-2011 by David Jarvie <djarvie@kde.org>
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

#include "alarmresource.h"

#include <kurl.h>
#include <kdirwatch.h>

#include <QDateTime>


/** A KAlarm calendar resource stored as a local file. */
class KALARM_RESOURCES_EXPORT KAResourceLocal : public AlarmResource
{
        Q_OBJECT
    public:
        KAResourceLocal();
        /** Create resource from configuration information stored in a KConfig object. */
        explicit KAResourceLocal(const KConfigGroup&);
        /** Create resource for file named @a fileName. */
        KAResourceLocal(CalEvent::Type, const QString& fileName);
        virtual ~KAResourceLocal();

        QString      fileName() const;
        bool         setFileName(const KUrl&);
        virtual QString     displayType() const;
        virtual QString     displayLocation() const;
        virtual QStringList location() const   { return QStringList(fileName()); }
        virtual bool        setLocation(const QString& fileName, const QString& = QString());
        virtual bool readOnly() const;
        virtual void writeConfig(KConfigGroup&);
        virtual void startReconfig();
        virtual void applyReconfig();

        // Override unused virtual functions
        virtual KCal::Todo::List rawTodos(KCal::TodoSortField = KCal::TodoSortUnsorted, KCal::SortDirection = KCal::SortDirectionAscending)  { return KCal::Todo::List(); }
        virtual KCal::Journal::List rawJournals(KCal::JournalSortField = KCal::JournalSortUnsorted, KCal::SortDirection = KCal::SortDirectionAscending)  { return KCal::Journal::List(); }

    protected:
        virtual bool doLoad(bool syncCache);
        virtual bool doSave(bool syncCache);
        virtual bool doSave(bool syncCache, KCal::Incidence* i)  { return AlarmResource::doSave(syncCache, i); }
        QDateTime    readLastModified();
        virtual void enableResource(bool enable);

    protected slots:
        void  reload();

    private:
        void  init();
        bool  loadFile();
        // Inherited virtual methods which should not be used by derived classes.
        using ResourceCalendar::doLoad;
        using ResourceCalendar::doSave;

        KUrl        mURL;
        KUrl        mNewURL;    // new file name to be applied by applyReconfig()
        KDirWatch   mDirWatch;
        QDateTime   mLastModified;
        bool        mFileReadOnly;  // calendar file is a read-only file
};

#endif

// vim: et sw=4:
