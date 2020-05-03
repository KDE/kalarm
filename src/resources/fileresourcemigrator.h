/*
 *  fileresourcemigrator.h  -  migrates or creates KAlarm non-Akonadi resources
 *  Program:  kalarm
 *  Copyright © 2020 David Jarvie <djarvie@kde.org>
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

#ifndef FILERESOURCEMIGRATOR_H
#define FILERESOURCEMIGRATOR_H

#include <KAlarmCal/KACalendar>

#include <AkonadiCore/ServerManager>

class KJob;
namespace Akonadi { class CollectionFetchJob; }

class Resource;

/**
 * Class to migrate Akonadi or KResources alarm calendars from previous
 * versions of KAlarm, and to create default calendar resources if none exist.
 */
class FileResourceMigrator : public QObject
{
    Q_OBJECT
public:
    ~FileResourceMigrator();

    /** Return the unique instance, creating it if necessary.
     *  Note that the instance will be destroyed once migration has completed.
     *  @return  Unique instance, or null if migration is not required or has
     *           already been done.
     */
    static FileResourceMigrator* instance();

    /** Initiate resource migration and default resource creation.
     *  When execution is complete, the unique instance will be destroyed.
     *  Connect to the QObject::destroyed() signal to determine when
     *  execution has completed.
     */
    void execute();

    static bool completed()    { return mCompleted; }

private Q_SLOTS:
    void collectionFetchResult(KJob*);
    void checkIfComplete();

private:
    explicit FileResourceMigrator(QObject* parent = nullptr);
    void migrateAkonadiResources(Akonadi::ServerManager::State);
    void migrateKResources();
    void createDefaultResources();
    void createCalendar(KAlarmCal::CalEvent::Type alarmType, const QString& file, const QString& name);

    static FileResourceMigrator* mInstance;
    QList<Akonadi::CollectionFetchJob*> mFetchesPending;  // pending collection fetch jobs for existing resources
    KAlarmCal::CalEvent::Types mExistingAlarmTypes {KAlarmCal::CalEvent::EMPTY};  // alarm types provided by existing non-Akonadi resources
    bool            mMigratingAkonadi {false};  // attempting to migrate Akonadi resources
    bool            mMigrateKResources {true};  // need to migrate KResource resources
    static bool     mCompleted;                 // execute() has completed
};

#endif // FILERESOURCEMIGRATOR_H

// vim: et sw=4:
