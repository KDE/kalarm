/*
 *  templatelistfiltermodel.cpp  -  proxy model class for lists of alarm templates
 *  Program:  kalarm
 *  Copyright © 2007 by David Jarvie <software@astrojar.org.uk>
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

#include "kalarm.h"

#include <kdebug.h>

#include "alarmevent.h"
#include "resources/kcalendar.h"
#include "templatelistfiltermodel.h"

// TemplateListFilterModel provides sorting and filtering for the alarm list model.


void TemplateListFilterModel::setTypeFilter(bool excludeCommandAlarms)
{
	if (excludeCommandAlarms != mCmdFilter)
	{
		mCmdFilter = excludeCommandAlarms;
		filterChanged();
	}
}

bool TemplateListFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex&) const
{
	QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0);
	if (sourceModel()->data(sourceIndex, EventListModel::StatusRole).toInt() != KCalEvent::TEMPLATE)
		return false;
	if (!mCmdFilter)
		return true;
	return sourceModel()->data(sourceIndex, EventListModel::ValueRole).toInt() != KAEvent::COMMAND;
}

bool TemplateListFilterModel::filterAcceptsColumn(int sourceCol, const QModelIndex&) const
{
	return sourceCol == EventListModel::TemplateNameColumn
	   ||  sourceCol == EventListModel::TypeColumn;
}

QModelIndex TemplateListFilterModel::mapFromSource(const QModelIndex& sourceIndex) const
{
	int proxyColumn;
	switch (sourceIndex.column())
	{
		case EventListModel::TypeColumn:
			proxyColumn = TypeColumn;
			break;
		case EventListModel::TemplateNameColumn:
			proxyColumn = TemplateNameColumn;
			break;
		default:
			return QModelIndex();
	}
	QModelIndex ix = EventListFilterModel::mapFromSource(sourceIndex);
	return index(ix.row(), proxyColumn, ix.parent());
}

QModelIndex TemplateListFilterModel::mapToSource(const QModelIndex& proxyIndex) const
{
	int sourceColumn;
	switch (proxyIndex.column())
	{
		case TypeColumn:
			sourceColumn = EventListModel::TypeColumn;
			break;
		case TemplateNameColumn:
			sourceColumn = EventListModel::TemplateNameColumn;
			break;
		default:
			return QModelIndex();
	}
	return EventListFilterModel::mapToSource(proxyIndex);
}
