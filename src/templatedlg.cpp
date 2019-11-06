/*
 *  templatedlg.cpp  -  dialog to create, edit and delete alarm templates
 *  Program:  kalarm
 *  Copyright © 2004-2019 David Jarvie <djarvie@kde.org>
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

#include "templatedlg.h"

#include "editdlg.h"
#include "alarmcalendar.h"
#include "functions.h"
#include "messagebox.h"
#include "newalarmaction.h"
#include "shellprocess.h"
#include "templatelistview.h"
#include "undo.h"
#include "resources/resources.h"
#include "kalarm_debug.h"

#include <KLocalizedString>
#include <kguiitem.h>
#include <kstandardaction.h>
#include <kactioncollection.h>
#include <KSeparator>

#include <QAction>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <QResizeEvent>

using namespace KCal;

static const char TMPL_DIALOG_NAME[] = "TemplateDialog";


TemplateDlg* TemplateDlg::mInstance = nullptr;


TemplateDlg::TemplateDlg(QWidget* parent)
    : QDialog(parent)
{
    setModal(false);
    setWindowTitle(i18nc("@title:window", "Alarm Templates"));

    QBoxLayout* topLayout = new QVBoxLayout(this);

    QBoxLayout* hlayout = new QHBoxLayout(this);
    topLayout->addLayout(hlayout);

    QBoxLayout* layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    hlayout->addLayout(layout);
    mListFilterModel = new TemplateListModel(this);
    if (!ShellProcess::authorised())
        mListFilterModel->setAlarmActionFilter(static_cast<KAEvent::Actions>(KAEvent::ACT_ALL & ~KAEvent::ACT_COMMAND));
    mListView = new TemplateListView(this);
    mListView->setModel(mListFilterModel);
    mListView->sortByColumn(TemplateListModel::TemplateNameColumn, Qt::AscendingOrder);
    mListView->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    mListView->setWhatsThis(i18nc("@info:whatsthis", "The list of alarm templates"));
    mListView->setItemDelegate(new TemplateListDelegate(mListView));
    connect(mListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TemplateDlg::slotSelectionChanged);
    layout->addWidget(mListView);

    layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    hlayout->addLayout(layout);
    QPushButton* button = new QPushButton(i18nc("@action:button", "New"));
    mNewAction = new NewAlarmAction(true, i18nc("@action", "New"), this);
    button->setMenu(mNewAction->menu());
    connect(mNewAction, &NewAlarmAction::selected, this, &TemplateDlg::slotNew);
    button->setWhatsThis(i18nc("@info:whatsthis", "Create a new alarm template"));
    layout->addWidget(button);

    mEditButton = new QPushButton(i18nc("@action:button", "Edit..."));
    connect(mEditButton, &QPushButton::clicked, this, &TemplateDlg::slotEdit);
    mEditButton->setWhatsThis(i18nc("@info:whatsthis", "Edit the currently highlighted alarm template"));
    layout->addWidget(mEditButton);

    mCopyButton = new QPushButton(i18nc("@action:button", "Copy"));
    connect(mCopyButton, &QPushButton::clicked, this, &TemplateDlg::slotCopy);
    mCopyButton->setWhatsThis(i18nc("@info:whatsthis", "Create a new alarm template based on a copy of the currently highlighted template"));
    layout->addWidget(mCopyButton);

    mDeleteButton = new QPushButton(i18nc("@action:button", "Delete"));
    connect(mDeleteButton, &QPushButton::clicked, this, &TemplateDlg::slotDelete);
    mDeleteButton->setWhatsThis(i18nc("@info:whatsthis", "Delete the currently highlighted alarm template"));
    layout->addWidget(mDeleteButton);

    layout->addStretch();

    topLayout->addWidget(new KSeparator(Qt::Horizontal, this));

    QDialogButtonBox* buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::close);
    topLayout->addWidget(buttonBox);

    KActionCollection* actions = new KActionCollection(this);
    QAction* act = KStandardAction::selectAll(mListView, SLOT(selectAll()), actions);
    topLevelWidget()->addAction(act);
    act = KStandardAction::deselect(mListView, SLOT(clearSelection()), actions);
    topLevelWidget()->addAction(act);
    slotSelectionChanged();          // enable/disable buttons as appropriate

    QSize s;
    if (KAlarm::readConfigWindowSize(TMPL_DIALOG_NAME, s))
        resize(s);
}

/******************************************************************************
* Destructor.
*/
TemplateDlg::~TemplateDlg()
{
    mInstance = nullptr;
}

/******************************************************************************
* Create an instance, if none already exists.
*/
TemplateDlg* TemplateDlg::create(QWidget* parent)
{
    if (mInstance)
        return nullptr;
    mInstance = new TemplateDlg(parent);
    return mInstance;
}

/******************************************************************************
* Called when the New Template button is clicked to create a new template.
*/
void TemplateDlg::slotNew(EditAlarmDlg::Type type)
{
    KAlarm::editNewTemplate(type, mListView);
}

/******************************************************************************
* Called when the Copy button is clicked to edit a copy of an existing alarm,
* to add to the list.
*/
void TemplateDlg::slotCopy()
{
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editNewTemplate(&event, mListView);
}

/******************************************************************************
* Called when the Modify button is clicked to edit the currently highlighted
* alarm in the list.
*/
void TemplateDlg::slotEdit()
{
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editTemplate(&event, mListView);
}

/******************************************************************************
* Called when the Delete button is clicked to delete the currently highlighted
* alarms in the list.
*/
void TemplateDlg::slotDelete()
{
    QVector<KAEvent> events = mListView->selectedEvents();
    int n = events.count();
    if (KAMessageBox::warningContinueCancel(this, i18ncp("@info", "Do you really want to delete the selected alarm template?",
                                                         "Do you really want to delete the %1 selected alarm templates?", n),
                                            i18ncp("@title:window", "Delete Alarm Template", "Delete Alarm Templates", n),
                                            KGuiItem(i18nc("@action:button", "&Delete"), QStringLiteral("edit-delete")))
            != KMessageBox::Continue)
        return;

    KAEvent::List delEvents;
    Undo::EventList undos;
    for (int i = 0;  i < n;  ++i)
    {
        KAEvent* event = &events[i];
        delEvents.append(event);
        undos.append(*event, Resources::resourceForEvent(event->id()));
    }
    KAlarm::deleteTemplates(delEvents, this);
    Undo::saveDeletes(undos);
}

/******************************************************************************
* Called when the group of items selected changes.
* Enable/disable the buttons depending on whether/how many templates are
* currently highlighted.
*/
void TemplateDlg::slotSelectionChanged()
{
    AlarmCalendar* resources = AlarmCalendar::resources();
    QVector<KAEvent> events = mListView->selectedEvents();
    int count = events.count();
    bool readOnly = false;
    for (int i = 0;  i < count;  ++i)
    {
        const KAEvent* event = &events[i];
        if (resources->eventReadOnly(event->id()))
        {
            readOnly = true;
            break;
        }
    }
    mEditButton->setEnabled(count == 1);
    mCopyButton->setEnabled(count == 1);
    mDeleteButton->setEnabled(count && !readOnly);
}

/******************************************************************************
* Called when the dialog's size has changed.
* Records the new size in the config file.
*/
void TemplateDlg::resizeEvent(QResizeEvent* re)
{
    if (isVisible())
        KAlarm::writeConfigWindowSize(TMPL_DIALOG_NAME, re->size());
    QDialog::resizeEvent(re);
}

// vim: et sw=4:
