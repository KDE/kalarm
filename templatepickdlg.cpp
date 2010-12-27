/*
 *  templatepickdlg.cpp  -  dialog to choose an alarm template
 *  Program:  kalarm
 *  Copyright © 2004,2006-2010 by David Jarvie <djarvie@kde.org>
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
#include "templatepickdlg.moc"

#ifndef USE_AKONADI
#include "eventlistmodel.h"
#include "templatelistfiltermodel.h"
#endif
#include "functions.h"
#include "shellprocess.h"
#include "templatelistview.h"

#include <klocale.h>
#include <kdebug.h>

#include <QVBoxLayout>
#include <QResizeEvent>

static const char TMPL_PICK_DIALOG_NAME[] = "TemplatePickDialog";


TemplatePickDlg::TemplatePickDlg(KAEvent::Actions type, QWidget* parent)
    : KDialog(parent)
{
    QWidget* topWidget = new QWidget(this);
    setMainWidget(topWidget);
    setCaption(i18nc("@title:window", "Choose Alarm Template"));
    setButtons(Ok|Cancel);
    setDefaultButton(Ok);
    QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
    topLayout->setMargin(0);
    topLayout->setSpacing(spacingHint());

    // Display the list of templates, but exclude command alarms if in kiosk mode.
    KAEvent::Actions shown = KAEvent::ACT_ALL;
    if (!ShellProcess::authorised())
    {
        type = static_cast<KAEvent::Actions>(type & ~KAEvent::ACT_COMMAND);
        shown = static_cast<KAEvent::Actions>(shown & ~KAEvent::ACT_COMMAND);
    }
#ifdef USE_AKONADI
    mListFilterModel = new TemplateListModel(this);
    mListFilterModel->setAlarmActionsEnabled(type);
    mListFilterModel->setAlarmActionFilter(shown);
#else
    mListFilterModel = new TemplateListFilterModel(EventListModel::templates());
    mListFilterModel->setTypesEnabled(type);
    mListFilterModel->setTypeFilter(shown);
#endif
    mListView = new TemplateListView(topWidget);
    mListView->setModel(mListFilterModel);
#ifdef USE_AKONADI
    mListView->sortByColumn(TemplateListModel::TemplateNameColumn, Qt::AscendingOrder);
#else
    mListView->sortByColumn(TemplateListFilterModel::TemplateNameColumn, Qt::AscendingOrder);
#endif
    mListView->setSelectionMode(QAbstractItemView::SingleSelection);
    mListView->setWhatsThis(i18nc("@info:whatsthis", "Select a template to base the new alarm on."));
    connect(mListView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&,const QItemSelection&)), SLOT(slotSelectionChanged()));
    // Require a real double click (even if KDE is in single-click mode) to accept the selection
    connect(mListView, SIGNAL(doubleClicked(const QModelIndex&)), SLOT(accept()));
    topLayout->addWidget(mListView);

    slotSelectionChanged();        // enable or disable the OK button

    QSize s;
    if (KAlarm::readConfigWindowSize(TMPL_PICK_DIALOG_NAME, s))
        resize(s);
}

/******************************************************************************
* Return the currently selected alarm template, or 0 if none.
*/
#ifdef USE_AKONADI
KAEvent TemplatePickDlg::selectedTemplate() const
#else
const KAEvent* TemplatePickDlg::selectedTemplate() const
#endif
{
    return mListView->selectedEvent();
}

/******************************************************************************
* Called when the template selection changes.
* Enable/disable the OK button depending on whether anything is selected.
*/
void TemplatePickDlg::slotSelectionChanged()
{
    bool enable = !mListView->selectionModel()->selectedRows().isEmpty();
    if (enable)
        enable = mListView->model()->flags(mListView->selectedIndex()) & Qt::ItemIsEnabled;
    enableButtonOk(enable);
}

/******************************************************************************
*  Called when the dialog's size has changed.
*  Records the new size in the config file.
*/
void TemplatePickDlg::resizeEvent(QResizeEvent* re)
{
    if (isVisible())
        KAlarm::writeConfigWindowSize(TMPL_PICK_DIALOG_NAME, re->size());
    KDialog::resizeEvent(re);
}

// vim: et sw=4:
