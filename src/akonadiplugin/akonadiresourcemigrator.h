/*
 *  akonadiresourcemigrator.h  -  migrates KAlarm Akonadi resources
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

/**
 * Class to migrate Akonadi or KResources alarm calendars from previous
 * versions of KAlarm, and to create default calendar resources if none exist.
 */
class AkonadiResourceMigrator : public QObject
{
    Q_OBJECT
public:
    ~AkonadiResourceMigrator() override;

    /** Return the unique instance, creating it if necessary.
     *  Note that the instance will be destroyed once migration has completed.
     *  @return  Unique instance, or null if migration is not required or has
     *           already been done.
     */
    static AkonadiResourceMigrator* instance();

    /** Initiate Akonadi resource migration. */
    void initiateMigration();

    /** Delete a named Akonadi resource.
     *  This should be called after the resource has been migrated.
     */
    void deleteAkonadiResource(const QString& resourceName);

Q_SIGNALS:
    /** Emitted when Akonadi resource migration has completed.
     *  @param migrated  true if Akonadi migration was performed.
     */
    void migrationComplete(bool migrated);

    /** Emitted when a file resource needs to be migrated. */
    void fileResource(const QString& resourceName, const QUrl& location, KAlarmCal::CalEvent::Types alarmTypes,
                      const QString& displayName, const QColor& backgroundColour,
                      KAlarmCal::CalEvent::Types enabledTypes, KAlarmCal::CalEvent::Types standardTypes,
                      bool readOnly);

    /** Emitted when a directory resource needs to be migrated. */
    void dirResource(const QString& resourceName, const QString& path, KAlarmCal::CalEvent::Types alarmTypes,
                      const QString& displayName, const QColor& backgroundColour,
                      KAlarmCal::CalEvent::Types enabledTypes, KAlarmCal::CalEvent::Types standardTypes,
                      bool readOnly);

private Q_SLOTS:
    void checkServer(Akonadi::ServerManager::State);
    void collectionFetchResult(KJob*);

private:
    explicit AkonadiResourceMigrator(QObject* parent = nullptr);

    void migrateResources();
    void doMigrateResources();
    void migrateCollection(const Akonadi::Collection&, bool dirType);
    void terminate(bool migrated);

    static AkonadiResourceMigrator* mInstance;
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
    bool            mAkonadiStarted {false};    // Akonadi was started by the migrator
    static bool     mCompleted;                 // migration has completed
};

// vim: et sw=4:
