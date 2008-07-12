/*
 *  alarmlistview.cpp  -  widget showing list of alarms
 *  Program:  kalarm
 *  Copyright © 2007,2008 by David Jarvie <djarvie@kde.org>
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

#include <kconfiggroup.h>

#include "eventlistmodel.h"
#include "alarmlistfiltermodel.h"
#include "alarmlistview.moc"


AlarmListView::AlarmListView(const QByteArray& configGroup, QWidget* parent)
	: EventListView(parent),
	  mConfigGroup(configGroup)
{
	setEditOnSingleClick(true);
	connect(header(), SIGNAL(sectionMoved(int, int, int)), SLOT(sectionMoved()));
}

void AlarmListView::setModel(QAbstractItemModel* model)
{
	EventListView::setModel(model);
	KConfigGroup config(KGlobal::config(), mConfigGroup.constData());
	QByteArray settings = config.readEntry("ListHead", QByteArray());
	if (!settings.isEmpty())
		header()->restoreState(settings);
	header()->setMovable(true);
	header()->setStretchLastSection(false);
	header()->setResizeMode(EventListModel::TimeColumn, QHeaderView::ResizeToContents);
	header()->setResizeMode(EventListModel::TimeToColumn, QHeaderView::ResizeToContents);
	header()->setResizeMode(EventListModel::RepeatColumn, QHeaderView::ResizeToContents);
	header()->setResizeMode(EventListModel::ColourColumn, QHeaderView::Fixed);
	header()->setResizeMode(EventListModel::TypeColumn, QHeaderView::Fixed);
	header()->setResizeMode(EventListModel::TextColumn, QHeaderView::Stretch);
	header()->setStretchLastSection(true);   // necessary to ensure ResizeToContents columns do resize to contents!
	const int margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin);
	header()->resizeSection(EventListModel::ColourColumn, viewOptions().fontMetrics.lineSpacing() * 3 / 4);
	header()->resizeSection(EventListModel::TypeColumn, EventListModel::iconWidth() + 2*margin + 2);
}

/******************************************************************************
* Called when the column order is changed.
* Save the new order for restoration on program restart.
*/
void AlarmListView::sectionMoved()
{
	KConfigGroup config(KGlobal::config(), mConfigGroup.constData());
	config.writeEntry("ListHead", header()->saveState());
	config.sync();
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

/*
void AlarmListView::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
	for (int col = topLeft.column();  col < bottomRight.column();  ++col)
	{
		if (col != header()->resizeMode(col) == QHeaderView::ResizeToContents)
			resizeColumnToContents(col);
	}
}
*/
