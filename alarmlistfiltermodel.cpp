/*
 *  alarmlistfiltermodel.cpp  -  proxy model class for lists of alarms
 *  Program:  kalarm
 *  Copyright © 2007,2008,2010 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"   //krazy:exclude=includes (kalarm.h must be first)
#include "alarmlistfiltermodel.h"

#include "eventlistmodel.h"

#include <kdebug.h>


// AlarmListFilterModel provides sorting and filtering for the alarm list model.


AlarmListFilterModel::AlarmListFilterModel(EventListModel* baseModel, QObject* parent)
	: EventListFilterModel(baseModel, parent),
	  mStatusFilter(KCalEvent::EMPTY)
{}

void AlarmListFilterModel::setStatusFilter(KCalEvent::Statuses type)
{
	if (type != mStatusFilter)
	{
		mStatusFilter = type;
		invalidateFilter();
	}
}

bool AlarmListFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex&) const
{
	return sourceModel()->data(sourceModel()->index(sourceRow, 0), EventListModel::StatusRole).toInt() & mStatusFilter;
}

bool AlarmListFilterModel::filterAcceptsColumn(int sourceCol, const QModelIndex&) const
{
	return (sourceCol != EventListModel::TemplateNameColumn);
}

QModelIndex AlarmListFilterModel::mapFromSource(const QModelIndex& sourceIndex) const
{
	if (sourceIndex.column() == EventListModel::TemplateNameColumn)
		return QModelIndex();
	return EventListFilterModel::mapFromSource(sourceIndex);
}
