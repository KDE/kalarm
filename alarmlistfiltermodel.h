/*
 *  alarmlistfiltermodel.h  -  proxy model class for lists of alarms
 *  Program:  kalarm
 *  Copyright © 2007-2011 by David Jarvie <djarvie@kde.org>
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

#ifndef ALARMLISTFILTERMODEL_H
#define ALARMLISTFILTERMODEL_H

#include "kalarm.h"

#include "eventlistmodel.h"

#include <KAlarmCal/kacalendar.h>


class AlarmListFilterModel : public EventListFilterModel
{
        Q_OBJECT
    public:
        enum { ColumnCount = 6 };

        explicit AlarmListFilterModel(EventListModel* baseModel, QObject* parent = 0);
        void                 setStatusFilter(CalEvent::Types);
        CalEvent::Types      statusFilter() const   { return mStatusFilter; }
        virtual QModelIndex  mapFromSource(const QModelIndex& sourceIndex) const;

    protected:
        virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;
        virtual bool filterAcceptsColumn(int sourceCol, const QModelIndex& sourceParent) const;

    private:
        CalEvent::Types mStatusFilter;
};

#endif // ALARMLISTFILTERMODEL_H

// vim: et sw=4:
