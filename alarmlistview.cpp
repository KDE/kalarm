/*
 *  alarmlistview.cpp  -  widget showing list of alarms
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

#include <QHeaderView>
#include <QMouseEvent>
#include <QApplication>

#include "eventlistmodel.h"
#include "alarmlistfiltermodel.h"
#include "alarmlistview.moc"


AlarmListView::AlarmListView(QWidget* parent)
	: EventListView(parent)
{
}

void AlarmListView::setModel(QAbstractItemModel* model)
{
	EventListView::setModel(model);
	header()->setMovable(true);
	header()->setStretchLastSection(false);
	header()->setResizeMode(EventListModel::TimeColumn, QHeaderView::ResizeToContents);
	header()->setResizeMode(EventListModel::TimeToColumn, QHeaderView::ResizeToContents);
	header()->setResizeMode(EventListModel::RepeatColumn, QHeaderView::ResizeToContents);
	header()->setResizeMode(EventListModel::ColourColumn, QHeaderView::Fixed);
	header()->setResizeMode(EventListModel::TypeColumn, QHeaderView::Fixed);
	header()->setResizeMode(EventListModel::TextColumn, QHeaderView::Stretch);
	const int margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin);
	header()->resizeSection(EventListModel::ColourColumn, viewOptions().fontMetrics.lineSpacing() * 3 / 4);
	header()->resizeSection(EventListModel::TypeColumn, EventListModel::iconWidth() + 2*margin + 2);
#warning Time column shrinks when Show Archived Alarms is selected
}

void AlarmListView::setColumnOrder(const QList<int>& order)
{
	// Set the column order
	if (order.count() >= AlarmListFilterModel::ColumnCount)
	{
		// The column order is specified
		int posn[AlarmListFilterModel::ColumnCount];
		int i;
		for (i = 0;  i < AlarmListFilterModel::ColumnCount;  ++i)
			posn[i] = -1;
		for (i = 0;  i < AlarmListFilterModel::ColumnCount;  ++i)
		{
			int ord = order[i];
			if (ord < AlarmListFilterModel::ColumnCount  &&  ord >= 0)
				posn[i] = ord;
		}
		bool ok = true;
		for (i = 0;  i < AlarmListFilterModel::ColumnCount;  ++i)
			if (posn[i] < 0)
				ok = false;    // no column is specified at this position
		if (ok  &&  posn[EventListModel::TextColumn] != EventListModel::TextColumn)
		{
			// Shift the message column to be last, since otherwise
			// column widths get screwed up.
			int messageCol = posn[EventListModel::TextColumn];
			for (i = 0;  i < AlarmListFilterModel::ColumnCount;  ++i)
				if (posn[i] > messageCol)
					--posn[i];
			posn[EventListModel::TextColumn] = EventListModel::TextColumn;
		}
		if (ok)
		{
			// Check whether the columns need to be reordered
			for (i = 0;  i < AlarmListFilterModel::ColumnCount;  ++i)
				if (posn[i] != i)
				{
					ok = false;
					break;
				}
			if (!ok)
			{
				// Reorder the columns
				for (i = 0;  i < AlarmListFilterModel::ColumnCount;  ++i)
				{
#warning Reorder the columns
			//		int j = posn.indexOf(i);
			//		moveSection(j, i);
				}
			}
		}
	}
}

/******************************************************************************
* Return the column order.
*/
QList<int> AlarmListView::columnOrder() const
{
	int order[AlarmListFilterModel::ColumnCount];
	QHeaderView* head = header();
	order[EventListModel::TimeColumn]   = head->visualIndex(EventListModel::TimeColumn);
	order[EventListModel::TimeToColumn] = head->visualIndex(EventListModel::TimeToColumn);
	order[EventListModel::RepeatColumn] = head->visualIndex(EventListModel::RepeatColumn);
	order[EventListModel::ColourColumn] = head->visualIndex(EventListModel::ColourColumn);
	order[EventListModel::TypeColumn]   = head->visualIndex(EventListModel::TypeColumn);
	order[EventListModel::TextColumn]   = head->visualIndex(EventListModel::TextColumn);
	QList<int> orderList;
	for (int i = 0;  i < AlarmListFilterModel::ColumnCount;  ++i)
		orderList += order[i];
	return orderList;
}

/******************************************************************************
* Set which time columns are to be displayed.
*/
void AlarmListView::selectTimeColumns(bool time, bool timeTo)
{
	if (!time  &&  !timeTo)
		return;       // always show at least one time column
//	bool changed = false;
	bool hidden = header()->isSectionHidden(EventListModel::TimeColumn);
	if (time  &&  hidden)
	{
		// Unhide the time column
		header()->setSectionHidden(EventListModel::TimeColumn, false);
//		changed = true;
	}
	else if (!time  &&  !hidden)
	{
		// Hide the time column
		header()->setSectionHidden(EventListModel::TimeColumn, true);
//		changed = true;
	}
	hidden = header()->isSectionHidden(EventListModel::TimeToColumn);
	if (timeTo  &&  hidden)
	{
		// Unhide the time-to-alarm column
		header()->setSectionHidden(EventListModel::TimeToColumn, false);
//		changed = true;
	}
	else if (!timeTo  &&  !hidden)
	{
		// Hide the time-to-alarm column
		header()->setSectionHidden(EventListModel::TimeToColumn, true);
//		changed = true;
	}
//	if (changed)
//	{
//		resizeLastColumn();
//		triggerUpdate();   // ensure scroll bar appears if needed
//	}
}

void AlarmListView::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
/*	for (int col = topLeft.column();  col < bottomRight.column();  ++col)
	{
		if (col != header()->resizeMode(col) == QHeaderView::ResizeToContents)
			resizeColumnToContents(col);
	}*/
}
