/*
 *  templatedlg.cpp  -  dialog to create, edit and delete alarm templates
 *  Program:  kalarm
 *  Copyright © 2004-2010 by David Jarvie <djarvie@kde.org>
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
#include "templatedlg.moc"

#include "editdlg.h"
#include "alarmcalendar.h"
#ifndef USE_AKONADI
#include "alarmresources.h"
#include "eventlistmodel.h"
#include "templatelistfiltermodel.h"
#endif
#include "functions.h"
#include "newalarmaction.h"
#include "shellprocess.h"
#include "templatelistview.h"
#include "undo.h"

#include <klocale.h>
#include <kguiitem.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <kstandardaction.h>
#include <kactioncollection.h>
#include <kaction.h>
#include <kmenu.h>

#include <QPushButton>
#include <QList>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <QResizeEvent>

using namespace KCal;

static const char TMPL_DIALOG_NAME[] = "TemplateDialog";


TemplateDlg* TemplateDlg::mInstance = 0;


TemplateDlg::TemplateDlg(QWidget* parent)
    : KDialog(parent)
{
    QWidget* topWidget = new QWidget(this);
    setMainWidget(topWidget);
    setButtons(Close);
    setDefaultButton(Close);
    setModal(false);
    setCaption(i18nc("@title:window", "Alarm Templates"));
    showButtonSeparator(true);
    QBoxLayout* topLayout = new QHBoxLayout(topWidget);
    topLayout->setMargin(0);
    topLayout->setSpacing(spacingHint());

    QBoxLayout* layout = new QVBoxLayout();
    layout->setMargin(0);
    topLayout->addLayout(layout);
#ifdef USE_AKONADI
    mListFilterModel = new TemplateListModel(this);
    if (!ShellProcess::authorised())
        mListFilterModel->setAlarmActionFilter(static_cast<KAEvent::Actions>(KAEvent::ACT_ALL & ~KAEvent::ACT_COMMAND));
#else
    mListFilterModel = new TemplateListFilterModel(EventListModel::templates());
    if (!ShellProcess::authorised())
        mListFilterModel->setTypeFilter(static_cast<KAEvent::Actions>(KAEvent::ACT_ALL & ~KAEvent::ACT_COMMAND));
#endif
    mListView = new TemplateListView(topWidget);
    mListView->setModel(mListFilterModel);
#ifdef USE_AKONADI
    mListView->sortByColumn(TemplateListModel::TemplateNameColumn, Qt::AscendingOrder);
#else
    mListView->sortByColumn(TemplateListFilterModel::TemplateNameColumn, Qt::AscendingOrder);
#endif
    mListView->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    mListView->setWhatsThis(i18nc("@info:whatsthis", "The list of alarm templates"));
    mListView->setItemDelegate(new TemplateListDelegate(mListView));
    connect(mListView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), SLOT(slotSelectionChanged()));
    layout->addWidget(mListView);

    layout = new QVBoxLayout();
    layout->setMargin(0);
    topLayout->addLayout(layout);
    QPushButton* button = new QPushButton(i18nc("@action:button", "New"), topWidget);
    mNewAction = new NewAlarmAction(true, i18nc("@action", "New"), this);
    button->setMenu(mNewAction->menu());
    connect(mNewAction, SIGNAL(selected(EditAlarmDlg::Type)), SLOT(slotNew(EditAlarmDlg::Type)));
    button->setWhatsThis(i18nc("@info:whatsthis", "Create a new alarm template"));
    layout->addWidget(button);

    mEditButton = new QPushButton(i18nc("@action:button", "Edit..."), topWidget);
    connect(mEditButton, SIGNAL(clicked()), SLOT(slotEdit()));
    mEditButton->setWhatsThis(i18nc("@info:whatsthis", "Edit the currently highlighted alarm template"));
    layout->addWidget(mEditButton);

    mCopyButton = new QPushButton(i18nc("@action:button", "Copy"), topWidget);
    connect(mCopyButton, SIGNAL(clicked()), SLOT(slotCopy()));
    mCopyButton->setWhatsThis(i18nc("@info:whatsthis", "Create a new alarm template based on a copy of the currently highlighted template"));
    layout->addWidget(mCopyButton);

    mDeleteButton = new QPushButton(i18nc("@action:button", "Delete"), topWidget);
    connect(mDeleteButton, SIGNAL(clicked()), SLOT(slotDelete()));
    mDeleteButton->setWhatsThis(i18nc("@info:whatsthis", "Delete the currently highlighted alarm template"));
    layout->addWidget(mDeleteButton);

    KActionCollection* actions = new KActionCollection(this);
    KAction* act = KStandardAction::selectAll(mListView, SLOT(selectAll()), actions);
    topLevelWidget()->addAction(act);
    act = KStandardAction::deselect(mListView, SLOT(clearSelection()), actions);
    topLevelWidget()->addAction(act);

    slotSelectionChanged();          // enable/disable buttons as appropriate

    QSize s;
    if (KAlarm::readConfigWindowSize(TMPL_DIALOG_NAME, s))
        resize(s);
}

/******************************************************************************
*  Destructor.
*/
TemplateDlg::~TemplateDlg()
{
    mInstance = 0;
}

/******************************************************************************
*  Create an instance, if none already exists.
*/
TemplateDlg* TemplateDlg::create(QWidget* parent)
{
    if (mInstance)
        return 0;
    mInstance = new TemplateDlg(parent);
    return mInstance;
}

/******************************************************************************
*  Called when the New Template button is clicked to create a new template.
*/
void TemplateDlg::slotNew(EditAlarmDlg::Type type)
{
    KAlarm::editNewTemplate(type, mListView);
}

/******************************************************************************
*  Called when the Copy button is clicked to edit a copy of an existing alarm,
*  to add to the list.
*/
void TemplateDlg::slotCopy()
{
#ifdef USE_AKONADI
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editNewTemplate(&event, mListView);
#else
    KAEvent* event = mListView->selectedEvent();
    if (event)
        KAlarm::editNewTemplate(event, mListView);
#endif
}

/******************************************************************************
*  Called when the Modify button is clicked to edit the currently highlighted
*  alarm in the list.
*/
void TemplateDlg::slotEdit()
{
#ifdef USE_AKONADI
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editTemplate(&event, mListView);
#else
    KAEvent* event = mListView->selectedEvent();
    if (event)
        KAlarm::editTemplate(event, mListView);
#endif
}

/******************************************************************************
*  Called when the Delete button is clicked to delete the currently highlighted
*  alarms in the list.
*/
void TemplateDlg::slotDelete()
{
#ifdef USE_AKONADI
    QList<KAEvent> events = mListView->selectedEvents();
#else
    KAEvent::List events = mListView->selectedEvents();
#endif
    int n = events.count();
    if (KMessageBox::warningContinueCancel(this, i18ncp("@info", "Do you really want to delete the selected alarm template?",
                                                      "Do you really want to delete the %1 selected alarm templates?", n),
                                           i18ncp("@title:window", "Delete Alarm Template", "Delete Alarm Templates", n),
                                           KGuiItem(i18nc("@action:button", "&Delete"), "edit-delete"))
            != KMessageBox::Continue)
        return;

#ifdef USE_AKONADI
    KAEvent::List delEvents;
#else
    QStringList delEvents;
#endif
    Undo::EventList undos;
    AlarmCalendar* resources = AlarmCalendar::resources();
    for (int i = 0;  i < n;  ++i)
    {
#ifdef USE_AKONADI
        KAEvent* event = &events[i];
        delEvents.append(event);
        Akonadi::Collection c = resources->collectionForEvent(event->itemId());
        undos.append(*event, c);
#else
        const KAEvent* event = events[i];
        delEvents.append(event->id());
        undos.append(*event, resources->resourceForEvent(event->id()));
#endif
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
#ifdef USE_AKONADI
    QList<KAEvent> events = mListView->selectedEvents();
#else
    KAEvent::List events = mListView->selectedEvents();
#endif
    int count = events.count();
    bool readOnly = false;
    for (int i = 0;  i < count;  ++i)
    {
#ifdef USE_AKONADI
        const KAEvent* event = &events[i];
        if (resources->eventReadOnly(event->itemId()))
#else
        const KAEvent* event = events[i];
        if (resources->eventReadOnly(event->id()))
#endif
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
*  Called when the dialog's size has changed.
*  Records the new size in the config file.
*/
void TemplateDlg::resizeEvent(QResizeEvent* re)
{
    if (isVisible())
        KAlarm::writeConfigWindowSize(TMPL_DIALOG_NAME, re->size());
    KDialog::resizeEvent(re);
}

// vim: et sw=4:
