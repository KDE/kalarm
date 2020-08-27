/*
 *  templatepickdlg.cpp  -  dialog to choose an alarm template
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "templatepickdlg.h"

#include "functions.h"
#include "templatelistview.h"
#include "resources/datamodel.h"
#include "resources/eventmodel.h"
#include "lib/config.h"
#include "lib/shellprocess.h"
#include "kalarm_debug.h"

#include <KLocalizedString>

#include <QVBoxLayout>
#include <QResizeEvent>
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
    mainLayout->addWidget(buttonBox);
    mOkButton = buttonBox->button(QDialogButtonBox::Ok);
    mOkButton->setDefault(true);
    mOkButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &TemplatePickDlg::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &TemplatePickDlg::reject);
    QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);

    // Display the list of templates, but exclude command alarms if in kiosk mode.
    KAEvent::Actions shown = KAEvent::ACT_ALL;
    if (!ShellProcess::authorised())
    {
        type = static_cast<KAEvent::Actions>(type & ~KAEvent::ACT_COMMAND);
        shown = static_cast<KAEvent::Actions>(shown & ~KAEvent::ACT_COMMAND);
    }
    mListFilterModel = DataModel::createTemplateListModel(this);
    mListFilterModel->setAlarmActionsEnabled(type);
    mListFilterModel->setAlarmActionFilter(shown);
    mListView = new TemplateListView(topWidget);
    mainLayout->addWidget(mListView);
    mListView->setModel(mListFilterModel);
    mListView->sortByColumn(TemplateListModel::TemplateNameColumn, Qt::AscendingOrder);
    mListView->setSelectionMode(QAbstractItemView::SingleSelection);
    mListView->setWhatsThis(i18nc("@info:whatsthis", "Select a template to base the new alarm on."));
    connect(mListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TemplatePickDlg::slotSelectionChanged);
    // Require a real double click (even if KDE is in single-click mode) to accept the selection
    connect(mListView, &TemplateListView::doubleClicked, this, &TemplatePickDlg::slotDoubleClick);
    topLayout->addWidget(mListView);

    slotSelectionChanged();        // enable or disable the OK button

    QSize s;
    if (Config::readWindowSize(TMPL_PICK_DIALOG_NAME, s))
        resize(s);
}

/******************************************************************************
* Return the currently selected alarm template, or invalid if none.
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
* Called when the user double clicks to accept a selection.
* Ignore if the double click is on a disabled item.
*/
void TemplatePickDlg::slotDoubleClick(const QModelIndex& ix)
{
    if ((mListFilterModel->flags(ix) & (Qt::ItemIsEnabled | Qt::ItemIsSelectable)) == (Qt::ItemIsEnabled | Qt::ItemIsSelectable))
        Q_EMIT accept();
}

/******************************************************************************
* Called when the dialog's size has changed.
* Records the new size in the config file.
*/
void TemplatePickDlg::resizeEvent(QResizeEvent* re)
{
    if (isVisible())
        Config::writeWindowSize(TMPL_PICK_DIALOG_NAME, re->size());
    QDialog::resizeEvent(re);
}

// vim: et sw=4:
