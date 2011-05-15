/*
 *  calendarmigrator.h  -  migrates or creates KAlarm Akonadi resources
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

#ifndef CALENDARMIGRATOR_H
#define CALENDARMIGRATOR_H

#include "kacalendar.h"

#include <akonadi/agentinstance.h>
#include <akonadi/collection.h>

#include <QColor>

class KConfigGroup;
class KJob;
namespace KRES { class Resource; }

class CalendarCreator;

class CalendarMigrator : public QObject
{
        Q_OBJECT
    public:
        ~CalendarMigrator();
        static void execute();

    private slots:
        void calendarCreated(CalendarCreator*);

    private:
        CalendarMigrator(QObject* parent = 0);
        void migrateOrCreate();

        QList<CalendarCreator*> mCalendarsPending;   // pending calendar migration or creation jobs
};

#endif // CALENDARMIGRATOR_H

// vim: et sw=4:
