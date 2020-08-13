/*
 *  akonadiresourcemigrator.h  -  migrates or creates KAlarm Akonadi resources
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef AKONADIRESOURCEMIGRATOR_H
#define AKONADIRESOURCEMIGRATOR_H

#include <KAlarmCal/KACalendar>

#include <AkonadiCore/Collection>

class KJob;
namespace Akonadi {
class CollectionFetchJob;
}

class CalendarCreator;
class AkonadiCalendarUpdater;
class Resource;

using namespace KAlarmCal;

/**
 * Class to migrate KResources alarm calendars from pre-Akonadi versions of
 * KAlarm, and to create default calendar resources if none exist.
 */
class AkonadiResourceMigrator : public QObject
{
    Q_OBJECT
public:
    ~AkonadiResourceMigrator();
    static AkonadiResourceMigrator* instance();
    static void reset();
    static void execute();
    static bool completed()    { return mCompleted; }

Q_SIGNALS:
    /** Signal emitted when a resource is about to be created, and when creation has
     *  completed (successfully or not).
     *  @param path     path of the resource
     *  @param id       collection ID if @p finished is @c true, else invalid
     *  @param finished @c true if finished, @c false otherwise
     */
    void creating(const QString& path, Akonadi::Collection::Id id, bool finished);

private Q_SLOTS:
    void collectionFetchResult(KJob*);
    void creatingCalendar(const QString& path);
    void calendarCreated(CalendarCreator*);

private:
    AkonadiResourceMigrator(QObject* parent = nullptr);
    void migrateOrCreate();
    void createDefaultResources();

    static AkonadiResourceMigrator* mInstance;
    QList<CalendarCreator*> mCalendarsPending;  // pending calendar migration or creation jobs
    QList<Akonadi::CollectionFetchJob*> mFetchesPending;  // pending collection fetch jobs for existing resources
    CalEvent::Types mExistingAlarmTypes;   // alarm types provided by existing Akonadi resources
    static bool     mCompleted;            // execute() has completed

    friend class AkonadiCalendarUpdater;
};

#endif // AKONADIRESOURCEMIGRATOR_H

// vim: et sw=4:
