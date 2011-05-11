/*
 *  alarmmigrator.h  -  migrates KAlarm resources to Akonadi
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
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

#include "kacalendar.h"

#include <akonadi/agentinstance.h>
#include <akonadi/collection.h>

class KConfigGroup;
class KJob;
namespace KRES { class Resource; }
class CalendarMigrator;

class AlarmMigrator : public QObject
{
        Q_OBJECT
    public:
        AlarmMigrator() : mCalendarsPending(0), mExitCode(0) {}
        void migrate();

    private slots:
        void calendarDone(CalendarMigrator*);

    private:
        int mCalendarsPending;
        int mExitCode;
};

// Migrates a single alarm calendar
class CalendarMigrator : public QObject
{
        Q_OBJECT
    public:
        CalendarMigrator(KRES::Resource*, const KConfigGroup*);
        ~CalendarMigrator();
        QString resourceName() const;
        QString path() const           { return mPath; }
        QString errorMessage() const   { return mErrorMessage; }

    public slots:
        void agentCreated(KJob*);

    signals:
        void finished(CalendarMigrator*);

    private slots:
        void collectionsReceived(const Akonadi::Collection::List&);
        void collectionFetchResult(KJob*);
        void modifyCollectionJobDone(KJob*);

    private:
        void finish(bool cleanup);
        bool migrateLocalFile();
        bool migrateLocalDirectory();
        bool migrateRemoteFile();
        template <class Interface> Interface* migrateBasic();

        enum ResourceType { LocalFile, LocalDir, RemoteFile };

        KRES::Resource*        mResource;
        const KConfigGroup*    mConfig;
        Akonadi::AgentInstance mAgent;
        KAlarm::CalEvent::Type mAlarmType;
        ResourceType           mResourceType;
        QString                mPath;
        QString                mErrorMessage;
        bool                   mFinished;
};

// vim: et sw=4:
