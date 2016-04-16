/*
 *  templatelistview.cpp  -  widget showing list of alarm templates
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

#include "kalarm.h"
#include "templatelistview.h"

#include "functions.h"
#define TEMPLATE_LIST_MODEL TemplateListModel

#include <KLocalizedString>

#include <QHeaderView>
#include <QApplication>


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
    header()->setResizeMode(TEMPLATE_LIST_MODEL::TypeColumn, QHeaderView::Fixed);
    const int margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin);
    header()->resizeSection(TEMPLATE_LIST_MODEL::TypeColumn, ItemListModel::iconWidth() + 2*margin + 2);
}

void TemplateListDelegate::edit(KAEvent* event, EventListView* view)
{
    KAlarm::editTemplate(event, static_cast<TemplateListView*>(view));
}

// vim: et sw=4:
