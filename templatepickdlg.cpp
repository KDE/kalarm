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
#include "templatepickdlg.h"

#include "functions.h"
#include "shellprocess.h"
#include "templatelistview.h"

#include <KLocalizedString>

#include <QVBoxLayout>
#include <QResizeEvent>
#include <qdebug.h>
#include <KConfigGroup>
#include <QDialogButtonBox>
#include <QPushButton>

static const char TMPL_PICK_DIALOG_NAME[] = "TemplatePickDialog";


TemplatePickDlg::TemplatePickDlg(KAEvent::Actions type, QWidget* parent)
    : QDialog(parent)
{
    QWidget* topWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    mainLayout->addWidget(topWidget);
    setWindowTitle(i18nc("@title:window", "Choose Alarm Template"));
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    mOkButton = buttonBox->button(QDialogButtonBox::Ok);
    mOkButton->setDefault(true);
    mOkButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    mainLayout->addWidget(buttonBox);
    mOkButton->setDefault(true);
    QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
    topLayout->setMargin(0);
    //QT5 topLayout->setSpacing(spacingHint());

    // Display the list of templates, but exclude command alarms if in kiosk mode.
    KAEvent::Actions shown = KAEvent::ACT_ALL;
    if (!ShellProcess::authorised())
    {
        type = static_cast<KAEvent::Actions>(type & ~KAEvent::ACT_COMMAND);
        shown = static_cast<KAEvent::Actions>(shown & ~KAEvent::ACT_COMMAND);
    }
    mListFilterModel = new TemplateListModel(this);
    mListFilterModel->setAlarmActionsEnabled(type);
    mListFilterModel->setAlarmActionFilter(shown);
    mListView = new TemplateListView(topWidget);
    mainLayout->addWidget(mListView);
    mListView->setModel(mListFilterModel);
    mListView->sortByColumn(TemplateListModel::TemplateNameColumn, Qt::AscendingOrder);
    mListView->setSelectionMode(QAbstractItemView::SingleSelection);
    mListView->setWhatsThis(i18nc("@info:whatsthis", "Select a template to base the new alarm on."));
    connect(mListView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), SLOT(slotSelectionChanged()));
    // Require a real double click (even if KDE is in single-click mode) to accept the selection
    connect(mListView, SIGNAL(doubleClicked(QModelIndex)), SLOT(accept()));
    topLayout->addWidget(mListView);

    slotSelectionChanged();        // enable or disable the OK button

    QSize s;
    if (KAlarm::readConfigWindowSize(TMPL_PICK_DIALOG_NAME, s))
        resize(s);
}

/******************************************************************************
* Return the currently selected alarm template, or 0 if none.
*/
KAEvent TemplatePickDlg::selectedTemplate() const
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
    mOkButton->setEnabled(enable);
}

/******************************************************************************
* Called when the dialog's size has changed.
* Records the new size in the config file.
*/
void TemplatePickDlg::resizeEvent(QResizeEvent* re)
{
    if (isVisible())
        KAlarm::writeConfigWindowSize(TMPL_PICK_DIALOG_NAME, re->size());
    QDialog::resizeEvent(re);
}

#include "moc_templatepickdlg.cpp"
// vim: et sw=4:
