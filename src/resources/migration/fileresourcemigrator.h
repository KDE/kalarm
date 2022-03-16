/*
 *  fileresourcemigrator.h  -  migrates or creates KAlarm non-Akonadi resources
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "kalarmcalendar/kacalendar.h"

#include <Akonadi/ServerManager>
#include <Akonadi/Collection>

class KJob;
namespace Akonadi { class CollectionFetchJob; }
using namespace KAlarmCal;

class Resource;

/**
 * Class to migrate Akonadi or KResources alarm calendars from previous
 * versions of KAlarm, and to create default calendar resources if none exist.
 */
class FileResourceMigrator : public QObject
{
    Q_OBJECT
public:
    ~FileResourceMigrator() override;

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

    void migrateAkonadiResources();
    void doMigrateAkonadiResources();
    void migrateAkonadiCollection(const Akonadi::Collection&, bool dirType);
    void migrateKResources();
    void createDefaultResources();
    void createCalendar(KAlarmCal::CalEvent::Type alarmType, const QString& file, const QString& name);

    static FileResourceMigrator* mInstance;
    class AkonadiMigration;
    AkonadiMigration* mAkonadiMigration {nullptr};
    struct AkResourceData
    {
        QString             resourceId;  // Akonadi resource identifier
        Akonadi::Collection collection;  // Akonadi collection
        bool                dirType;     // it's a directory resource
        AkResourceData() = default;
        AkResourceData(const QString& r, const Akonadi::Collection& c, bool dir)
            : resourceId(r), collection(c), dirType(dir) {}
    };
    QHash<QString, AkResourceData> mCollectionPaths;    // path, (Akonadi resource identifier, collection) pairs
    QHash<Akonadi::CollectionFetchJob*, bool> mFetchesPending;  // pending collection fetch jobs for existing resources, and whether directory resource
    KAlarmCal::CalEvent::Types mExistingAlarmTypes {KAlarmCal::CalEvent::EMPTY};  // alarm types provided by existing non-Akonadi resources
    bool            mMigrateKResources {true};  // need to migrate KResource resources
    bool            mAkonadiStart {false};      // Akonadi was started by the migrator
    static bool     mCompleted;                 // execute() has completed
};

// vim: et sw=4:
