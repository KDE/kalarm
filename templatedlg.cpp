/*
 *  templatedlg.cpp  -  dialogue to create, edit and delete alarm templates
 *  Program:  kalarm
 *  Copyright © 2004-2007 by David Jarvie <software@astrojar.org.uk>
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

#include <QPushButton>
#include <QList>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <QResizeEvent>

#include <klocale.h>
#include <kguiitem.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <kstandardaction.h>
#include <kactioncollection.h>

#include "editdlg.h"
#include "alarmcalendar.h"
#include "alarmresources.h"
#include "functions.h"
#include "eventlistmodel.h"
#include "templatelistfiltermodel.h"
#include "templatelistview.h"
#include "undo.h"
#include "templatedlg.moc"

using namespace KCal;

static const char TMPL_DIALOG_NAME[] = "TemplateDialog";


TemplateDlg* TemplateDlg::mInstance = 0;


TemplateDlg::TemplateDlg(QWidget* parent)
	: KDialog(parent)
{
	QWidget* topWidget = new QWidget(this);
        setMainWidget(topWidget);
        setButtons(Close);
        setDefaultButton(Ok);
        setModal(false);
        setCaption(i18n("Alarm Templates"));
        showButtonSeparator(true);
	QBoxLayout* topLayout = new QHBoxLayout(topWidget);
	topLayout->setMargin(0);
	topLayout->setSpacing(spacingHint());

	QBoxLayout* layout = new QVBoxLayout();
	layout->setMargin(0);
	topLayout->addLayout(layout);
	mListFilterModel = new TemplateListFilterModel(EventListModel::templates());
	mListFilterModel->setTypeFilter(false);
	mListView = new TemplateListView(topWidget);
	mListView->setModel(mListFilterModel);
	mListView->sortByColumn(TemplateListFilterModel::TemplateNameColumn, Qt::AscendingOrder);
	mListView->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	mListView->setWhatsThis(i18n("The list of alarm templates"));
	connect(mListView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&,const QItemSelection&)), SLOT(slotSelectionChanged()));
	layout->addWidget(mListView);

	layout = new QVBoxLayout();
	layout->setMargin(0);
	topLayout->addLayout(layout);
	QPushButton* button = new QPushButton(i18n("&New..."), topWidget);
	button->setFixedSize(button->sizeHint());
	connect(button, SIGNAL(clicked()), SLOT(slotNew()));
	button->setWhatsThis(i18n("Create a new alarm template"));
	layout->addWidget(button);

	mEditButton = new QPushButton(i18n("&Edit..."), topWidget);
	mEditButton->setFixedSize(mEditButton->sizeHint());
	connect(mEditButton, SIGNAL(clicked()), SLOT(slotEdit()));
	mEditButton->setWhatsThis(i18n("Edit the currently highlighted alarm template"));
	layout->addWidget(mEditButton);

	mCopyButton = new QPushButton(i18n("Co&py"), topWidget);
	mCopyButton->setFixedSize(mCopyButton->sizeHint());
	connect(mCopyButton, SIGNAL(clicked()), SLOT(slotCopy()));
	mCopyButton->setWhatsThis(i18n("Create a new alarm template based on a copy of the currently highlighted template"));
	layout->addWidget(mCopyButton);

	mDeleteButton = new QPushButton(i18n("&Delete"), topWidget);
	mDeleteButton->setFixedSize(mDeleteButton->sizeHint());
	connect(mDeleteButton, SIGNAL(clicked()), SLOT(slotDelete()));
	mDeleteButton->setWhatsThis(i18n("Delete the currently highlighted alarm template"));
	layout->addWidget(mDeleteButton);

	KActionCollection* actions = new KActionCollection(this);
	//actions->setDefaultShortcutContext(Qt::WindowShortcut);
	actions->setAssociatedWidget(topLevelWidget());
	KStandardAction::selectAll(mListView, SLOT(selectAll()), actions);
	KStandardAction::deselect(mListView, SLOT(clearSelection()), actions);

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
*  Called when the New Template button is clicked to create a new template
*  based on the currently selected alarm.
*/
void TemplateDlg::slotNew()
{
	KAlarm::editNewTemplate(mListView);
}

/******************************************************************************
*  Called when the Copy button is clicked to edit a copy of an existing alarm,
*  to add to the list.
*/
void TemplateDlg::slotCopy()
{
	Event* kcalEvent = mListView->selectedEvent();
	if (kcalEvent)
	{
		KAEvent event(kcalEvent);
		KAlarm::editNewTemplate(mListView, &event);
	}
}

/******************************************************************************
*  Called when the Modify button is clicked to edit the currently highlighted
*  alarm in the list.
*/
void TemplateDlg::slotEdit()
{
	Event* kcalEvent = mListView->selectedEvent();
	if (kcalEvent)
	{
		KAEvent event(kcalEvent);
		KAlarm::editTemplate(event, mListView);
	}
}

/******************************************************************************
*  Called when the Delete button is clicked to delete the currently highlighted
*  alarms in the list.
*/
void TemplateDlg::slotDelete()
{
	Event::List events = mListView->selectedEvents();
	int n = events.count();
	if (KMessageBox::warningContinueCancel(this, i18np("Do you really want to delete the selected alarm template?",
	                                                  "Do you really want to delete the %1 selected alarm templates?", n),
	                                       i18np("Delete Alarm Template", "Delete Alarm Templates", n),
	                                       KGuiItem(i18n("&Delete"), "edit-delete"))
		    != KMessageBox::Continue)
		return;

	QStringList eventIDs;
	Undo::EventList undos;
	AlarmResources* resources = AlarmResources::instance();
	for (int i = 0;  i < n;  ++i)
	{
		const KAEvent event(events[i]);
		eventIDs.append(event.id());
		undos.append(event, resources->resourceForIncidence(event.id()));
	}
	KAlarm::deleteTemplates(eventIDs, this);
	Undo::saveDeletes(undos);
}

/******************************************************************************
* Called when the group of items selected changes.
* Enable/disable the buttons depending on whether/how many templates are
* currently highlighted.
*/
void TemplateDlg::slotSelectionChanged()
{
	int count = mListView->selectionModel()->selectedRows().count();
	mEditButton->setEnabled(count == 1);
	mCopyButton->setEnabled(count == 1);
	mDeleteButton->setEnabled(count);
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
