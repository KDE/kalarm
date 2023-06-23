/*
 *  templatedlg.cpp  -  dialog to create, edit and delete alarm templates
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "templatedlg.h"

#include "functions.h"
#include "newalarmaction.h"
#include "templatelistview.h"
#include "undo.h"
#include "resources/datamodel.h"
#include "resources/eventmodel.h"
#include "resources/resources.h"
#include "lib/config.h"
#include "lib/messagebox.h"
#include "lib/shellprocess.h"

#include <KLocalizedString>
#include <KGuiItem>
#include <KStandardAction>
#include <KActionCollection>
#include <KSeparator>

#include <QAction>
#include <QBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QMenu>

using namespace KCal;

static const char TMPL_DIALOG_NAME[] = "TemplateDialog";


TemplateDlg* TemplateDlg::mInstance = nullptr;


TemplateDlg::TemplateDlg(QWidget* parent)
    : QDialog(parent)
{
    setModal(false);
    setWindowTitle(i18nc("@title:window", "Alarm Templates"));

    QBoxLayout* topLayout = new QVBoxLayout(this);

    QBoxLayout* hlayout = new QHBoxLayout();
    topLayout->addLayout(hlayout);

    QBoxLayout* layout = new QVBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    hlayout->addLayout(layout);
    mListFilterModel = DataModel::createTemplateListModel(this);
    if (!ShellProcess::authorised())
        mListFilterModel->setAlarmActionFilter(static_cast<KAEvent::Actions>(KAEvent::ACT_ALL & ~KAEvent::ACT_COMMAND));
    mListView = new TemplateListView(this);
    mListView->setModel(mListFilterModel);
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
    button->setToolTip(i18nc("@info:tooltip", "Create a new alarm template"));
    button->setWhatsThis(i18nc("@info:whatsthis", "Create a new alarm template"));
    layout->addWidget(button);

    mEditButton = new QPushButton(i18nc("@action:button", "Edit..."));
    connect(mEditButton, &QPushButton::clicked, this, &TemplateDlg::slotEdit);
    mEditButton->setToolTip(i18nc("@info:tooltip", "Edit the selected alarm template"));
    mEditButton->setWhatsThis(i18nc("@info:whatsthis", "Edit the currently highlighted alarm template"));
    layout->addWidget(mEditButton);

    mCopyButton = new QPushButton(i18nc("@action:button", "Copy"));
    connect(mCopyButton, &QPushButton::clicked, this, &TemplateDlg::slotCopy);
    mCopyButton->setToolTip(i18nc("@info:tooltip", "Create a new alarm template based on the selected template"));
    mCopyButton->setWhatsThis(i18nc("@info:whatsthis", "Create a new alarm template based on a copy of the currently highlighted template"));
    layout->addWidget(mCopyButton);

    mDeleteButton = new QPushButton(i18nc("@action:button", "Delete"));
    connect(mDeleteButton, &QPushButton::clicked, this, &TemplateDlg::slotDelete);
    mDeleteButton->setToolTip(i18nc("@info:tooltip", "Delete the selected alarm templates"));
    mDeleteButton->setWhatsThis(i18nc("@info:whatsthis", "Delete the currently highlighted alarm templates"));
    layout->addWidget(mDeleteButton);

    layout->addStretch();

    topLayout->addWidget(new KSeparator(Qt::Horizontal, this));

    auto buttonBox = new QDialogButtonBox(this);
    buttonBox->addButton(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::close);
    topLayout->addWidget(buttonBox);

    KActionCollection* actions = new KActionCollection(this);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    QAction* act = KStandardAction::selectAll(mListView, &QTreeView::selectAll, actions);
    topLevelWidget()->addAction(act);
    act = KStandardAction::deselect(mListView, &QAbstractItemView::clearSelection, actions);
    topLevelWidget()->addAction(act);
    slotSelectionChanged();          // enable/disable buttons as appropriate

    QSize s;
    if (Config::readWindowSize(TMPL_DIALOG_NAME, s))
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
        KAlarm::editNewTemplate(event, mListView);
}

/******************************************************************************
* Called when the Modify button is clicked to edit the currently highlighted
* alarm in the list.
*/
void TemplateDlg::slotEdit()
{
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editTemplate(event, mListView);
}

/******************************************************************************
* Called when the Delete button is clicked to delete the currently highlighted
* alarms in the list.
*/
void TemplateDlg::slotDelete()
{
    QList<KAEvent> events = mListView->selectedEvents();
    int n = events.count();
    if (KAMessageBox::warningContinueCancel(this, i18ncp("@info", "Do you really want to delete the selected alarm template?",
                                                         "Do you really want to delete the %1 selected alarm templates?", n),
                                            i18ncp("@title:window", "Delete Alarm Template", "Delete Alarm Templates", n),
                                            KGuiItem(i18nc("@action:button", "&Delete"), QStringLiteral("edit-delete")))
            != KMessageBox::Continue)
        return;

    KAEvent::List delEvents;
    delEvents.reserve(n);
    Undo::EventList undos;
    undos.reserve(n);
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
    QList<KAEvent> events = mListView->selectedEvents();
    int count = events.count();
    bool readOnly = false;
    for (int i = 0;  i < count;  ++i)
    {
        const KAEvent* event = &events[i];
        if (KAlarm::eventReadOnly(event->id()))
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
        Config::writeWindowSize(TMPL_DIALOG_NAME, re->size());
    QDialog::resizeEvent(re);
}

#include "moc_templatedlg.cpp"

// vim: et sw=4:
