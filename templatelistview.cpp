/*
 *  templatelistview.cpp  -  widget showing list of alarm templates
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

#include "eventlistmodel.h"
#include "functions.h"
#include "templatelistfiltermodel.h"
#include "templatelistview.moc"


TemplateListView::TemplateListView(QWidget* parent)
	: EventListView(parent)
{
	setEditOnSingleClick(false);
	setWhatsThis(i18nc("@info:whatsthis", "The list of alarm templates"));
}

void TemplateListView::setModel(QAbstractItemModel* model)
{
	EventListView::setModel(model);
	header()->setMovable(false);
	header()->setStretchLastSection(true);
	header()->setResizeMode(TemplateListFilterModel::TypeColumn, QHeaderView::Fixed);
	const int margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin);
	header()->resizeSection(TemplateListFilterModel::TypeColumn, EventListModel::iconWidth() + 2*margin + 2);
}

void TemplateListView::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
/*	for (int col = topLeft.column();  col < bottomRight.column();  ++col)
	{
		if (col != header()->resizeMode(col) == QHeaderView::ResizeToContents)
			resizeColumnToContents(col);
	}*/
}


void TemplateListDelegate::edit(KAEvent* event, EventListView* view)
{
	KAlarm::editTemplate(event, static_cast<TemplateListView*>(view));
}
