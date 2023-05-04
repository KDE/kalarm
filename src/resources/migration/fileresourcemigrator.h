/*
 *  fileresourcemigrator.h  -  migrates or creates KAlarm file system resources
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "kalarmcalendar/kacalendar.h"

class AkonadiPlugin;

/**
 * Class to migrate Akonadi alarm calendars from previous versions of KAlarm,
 * and to create default calendar resources if none exist.
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
    void akonadiMigrationComplete(bool migrated);
    void checkIfComplete();

private:
    explicit FileResourceMigrator(QObject* parent = nullptr);

    void createDefaultResources();
    void migrateFileResource(const QString& resourceName,
                             const QUrl& location, KAlarmCal::CalEvent::Types alarmTypes,
                             const QString& displayName, const QColor& backgroundColour,
                             KAlarmCal::CalEvent::Types enabledTypes,
                             KAlarmCal::CalEvent::Types standardTypes, bool readOnly);
    void migrateDirResource(const QString& resourceName,
                            const QString& path, KAlarmCal::CalEvent::Types alarmTypes,
                            const QString& displayName, const QColor& backgroundColour,
                            KAlarmCal::CalEvent::Types enabledTypes,
                            KAlarmCal::CalEvent::Types standardTypes, bool readOnly);
    void createCalendar(KAlarmCal::CalEvent::Type alarmType, const QString& file, const QString& name);

    static FileResourceMigrator* mInstance;
    AkonadiPlugin* mAkonadiPlugin {nullptr};
    KAlarmCal::CalEvent::Types mExistingAlarmTypes {KAlarmCal::CalEvent::EMPTY};  // alarm types provided by existing non-Akonadi resources
    static bool     mCompleted;                 // execute() has completed
};

// vim: et sw=4:
