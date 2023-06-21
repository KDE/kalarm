/*
 *  templatelistview.cpp  -  widget showing list of alarm templates
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "templatelistview.h"

#include "functions.h"
#include "resources/eventmodel.h"

#include <KLocalizedString>

#include <QHeaderView>
#include <QApplication>


TemplateListView::TemplateListView(QWidget* parent)
    : EventListView(parent)
{
    setEditOnSingleClick(false);
    setWhatsThis(i18nc("@info:whatsthis", "The list of alarm templates"));
}

/******************************************************************************
* Initialize column settings and sizing.
*/
void TemplateListView::initSections()
{
    header()->setSectionsMovable(false);
    header()->setStretchLastSection(true);
    header()->setSectionResizeMode(TemplateListModel::TypeColumn, QHeaderView::Fixed);
    const int minWidth = listViewOptions().fontMetrics.lineSpacing() * 3 / 4;
    header()->setMinimumSectionSize(minWidth);
    const int margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin);
    header()->resizeSection(TemplateListModel::TypeColumn, EventListModel::iconWidth() + 2*margin + 2);
    sortByColumn(TemplateListModel::TemplateNameColumn, Qt::AscendingOrder);
}

void TemplateListDelegate::edit(KAEvent& event, EventListView* view)
{
    KAlarm::editTemplate(event, static_cast<TemplateListView*>(view));
}

// vim: et sw=4:

#include "moc_templatelistview.cpp"
