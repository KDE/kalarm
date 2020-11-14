/*
 *  fileresourcemigrator.h  -  migrates or creates KAlarm non-Akonadi resources
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
    void start();

    static bool completed()    { return mCompleted; }

private Q_SLOTS:
    void checkAkonadiResources(Akonadi::ServerManager::State);
    void akonadiMigrationComplete();
    void collectionFetchResult(KJob*);
    void checkIfComplete();

private:
    explicit FileResourceMigrator(QObject* parent = nullptr);
    static void callMigrateAkonadiResources();
    void migrateAkonadiResources();
    void migrateKResources();
    void createDefaultResources();
    void createCalendar(KAlarmCal::CalEvent::Type alarmType, const QString& file, const QString& name);

    static FileResourceMigrator* mInstance;
    class AkonadiMigration;
    AkonadiMigration* mAkonadiMigration {nullptr};
    QList<Akonadi::CollectionFetchJob*> mFetchesPending;  // pending collection fetch jobs for existing resources
    KAlarmCal::CalEvent::Types mExistingAlarmTypes {KAlarmCal::CalEvent::EMPTY};  // alarm types provided by existing non-Akonadi resources
    bool            mMigrateKResources {true};  // need to migrate KResource resources
    bool            mAkonadiStart {false};      // Akonadi was started by the migrator
    static bool     mCompleted;                 // execute() has completed
};

#endif // FILERESOURCEMIGRATOR_H

// vim: et sw=4:
