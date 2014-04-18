/*
 *  calendarmigrator.h  -  migrates or creates KAlarm Akonadi resources
 *  Program:  kalarm
 *  Copyright © 2011-2014 by David Jarvie <djarvie@kde.org>
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

#ifndef CALENDARMIGRATOR_H
#define CALENDARMIGRATOR_H

#include <kalarmcal/kacalendar.h>

#include <akonadi/agentinstance.h>
#include <akonadi/collection.h>

#include <QColor>

class KConfigGroup;
class KJob;
namespace KRES { class Resource; }
namespace Akonadi { class CollectionFetchJob; }

class CalendarCreator;
class CalendarUpdater;

using namespace KAlarmCal;

/**
 * Class to migrate KResources alarm calendars from pre-Akonadi versions of
 * KAlarm, and to create default calendar resources if none exist.
 */
class CalendarMigrator : public QObject
{
        Q_OBJECT
    public:
        ~CalendarMigrator();
        static CalendarMigrator* instance();
        static void execute();
        static void updateToCurrentFormat(const Akonadi::Collection&, bool ignoreKeepFormat, QWidget* parent);
        static bool completed()    { return mCompleted; }
        template <class Interface> static Interface* getAgentInterface(const Akonadi::AgentInstance&, QString& errorMessage, QObject* parent);

    signals:
        /** Signal emitted when a resource is about to be created, and when creation has
         *  completed (successfully or not).
         *  @param id    collection ID if @p finished is true, else invalid
         */
        void creating(const QString& path, Akonadi::Collection::Id id, bool finished);

    private slots:
        void collectionFetchResult(KJob*);
        void creatingCalendar(const QString& path);
        void calendarCreated(CalendarCreator*);

    private:
        CalendarMigrator(QObject* parent = 0);
        void migrateOrCreate();
        void createDefaultResources();
        template <class Interface> static bool updateStorageFormat(const Akonadi::AgentInstance&, QString& errorMessage, QObject* parent);

        static CalendarMigrator* mInstance;
        QList<CalendarCreator*> mCalendarsPending;   // pending calendar migration or creation jobs
        QList<Akonadi::CollectionFetchJob*> mFetchesPending;  // pending collection fetch jobs for existing resources
        CalEvent::Types mExistingAlarmTypes;   // alarm types provided by existing Akonadi resources
        static bool     mCompleted;            // execute() has completed

        friend class CalendarUpdater;
};

#endif // CALENDARMIGRATOR_H

// vim: et sw=4:
