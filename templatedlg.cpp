/*
 *  templatedlg.cpp  -  dialogue to create, edit and delete alarm templates
 *  Program:  kalarm
 *  (C) 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "kalarm.h"

#include <qlayout.h>
#include <qpushbutton.h>
#include <qwhatsthis.h>

#include <klocale.h>
#include <kguiitem.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include "editdlg.h"
#include "alarmcalendar.h"
#include "functions.h"
#include "templatelistview.h"
#include "undo.h"
#include "templatedlg.moc"

static const char TMPL_DIALOG_NAME[] = "TemplateDialog";


TemplateDlg* TemplateDlg::mInstance = 0;


TemplateDlg::TemplateDlg(QWidget* parent, const char* name)
	: KDialogBase(KDialogBase::Plain, i18n("Alarm Templates"), Close, Ok, parent, name, false, true)
{
	QWidget* topWidget = plainPage();
	QBoxLayout* topLayout = new QHBoxLayout(topWidget);
	topLayout->setSpacing(spacingHint());

	QBoxLayout* layout = new QVBoxLayout(topLayout);
	mTemplateList = new TemplateListView(true, i18n("The list of alarm templates"), topWidget);
	mTemplateList->setSelectionMode(QListView::Extended);
	mTemplateList->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	connect(mTemplateList, SIGNAL(selectionChanged()), SLOT(slotSelectionChanged()));
	layout->addWidget(mTemplateList);

	layout = new QVBoxLayout(topLayout);
	QPushButton* button = new QPushButton(i18n("&New..."), topWidget);
	button->setFixedSize(button->sizeHint());
	connect(button, SIGNAL(clicked()), SLOT(slotNew()));
	QWhatsThis::add(button, i18n("Create a new alarm template"));
	layout->addWidget(button);

	mEditButton = new QPushButton(i18n("&Edit..."), topWidget);
	mEditButton->setFixedSize(mEditButton->sizeHint());
	connect(mEditButton, SIGNAL(clicked()), SLOT(slotEdit()));
	QWhatsThis::add(mEditButton, i18n("Edit the currently highlighted alarm template"));
	layout->addWidget(mEditButton);

	mCopyButton = new QPushButton(i18n("Co&py"), topWidget);
	mCopyButton->setFixedSize(mCopyButton->sizeHint());
	connect(mCopyButton, SIGNAL(clicked()), SLOT(slotCopy()));
	QWhatsThis::add(mCopyButton,
	      i18n("Create a new alarm template based on a copy of the currently highlighted template"));
	layout->addWidget(mCopyButton);

	mDeleteButton = new QPushButton(i18n("&Delete"), topWidget);
	mDeleteButton->setFixedSize(mDeleteButton->sizeHint());
	connect(mDeleteButton, SIGNAL(clicked()), SLOT(slotDelete()));
	QWhatsThis::add(mDeleteButton, i18n("Delete the currently highlighted alarm template"));
	layout->addWidget(mDeleteButton);

	mTemplateList->refresh();
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
TemplateDlg* TemplateDlg::create(QWidget* parent, const char* name)
{
	if (mInstance)
		return 0;
	mInstance = new TemplateDlg(parent, name);
	return mInstance;
}

/******************************************************************************
*  Called when the New Template button is clicked to create a new template
*  based on the currently selected alarm.
*/
void TemplateDlg::slotNew()
{
	createTemplate(0, this, mTemplateList);
}

/******************************************************************************
*  Called when the Copy button is clicked to edit a copy of an existing alarm,
*  to add to the list.
*/
void TemplateDlg::slotCopy()
{
	TemplateListViewItem* item = mTemplateList->selectedItem();
	if (item)
	{
		KAEvent event = item->event();
		createTemplate(&event, mTemplateList);
	}
}

/******************************************************************************
*  Create a new template.
*  If 'event' is non-zero, base the new template on an existing event or template.
*/
void TemplateDlg::createTemplate(const KAEvent* event, QWidget* parent, TemplateListView* view)
{
	EditAlarmDlg editDlg(true, i18n("New Alarm Template"), parent, "editDlg", event);
	if (editDlg.exec() == QDialog::Accepted)
	{
		KAEvent event;
		editDlg.getEvent(event);

		// Add the template to the displayed lists and to the calendar file
		KAlarm::addTemplate(event, view);
		Undo::saveAdd(event);
	}
}

/******************************************************************************
*  Called when the Modify button is clicked to edit the currently highlighted
*  alarm in the list.
*/
void TemplateDlg::slotEdit()
{
	TemplateListViewItem* item = mTemplateList->selectedItem();
	if (item)
	{
		KAEvent event = item->event();
		EditAlarmDlg* editDlg = new EditAlarmDlg(true, i18n("Edit Alarm Template"), this, "editDlg", &event);
		if (editDlg->exec() == QDialog::Accepted)
		{
			KAEvent newEvent;
			editDlg->getEvent(newEvent);
			QString id = event.id();
			newEvent.setEventID(id);

			// Update the event in the displays and in the calendar file
			KAlarm::updateTemplate(newEvent, mTemplateList);
			Undo::saveEdit(event, newEvent);
		}
	}
}

/******************************************************************************
*  Called when the Delete button is clicked to delete the currently highlighted
*  alarms in the list.
*/
void TemplateDlg::slotDelete()
{
	QValueList<EventListViewItemBase*> items = mTemplateList->selectedItems();
	int n = items.count();
	if (KMessageBox::warningContinueCancel(this, i18n("Do you really want to delete the selected alarm template?",
	                                                  "Do you really want to delete the %n selected alarm templates?", n),
	                                       i18n("Delete Alarm Template", "Delete Alarm Templates", n), KGuiItem(i18n("&Delete"), "editdelete"))
		    != KMessageBox::Continue)
		return;

	QValueList<KAEvent> events;
	AlarmCalendar::templateCalendar()->startUpdate();    // prevent multiple saves of the calendar until we're finished
	for (QValueList<EventListViewItemBase*>::Iterator it = items.begin();  it != items.end();  ++it)
	{
		TemplateListViewItem* item = (TemplateListViewItem*)(*it);
		events.append(item->event());
		KAlarm::deleteTemplate(item->event());
	}
	AlarmCalendar::templateCalendar()->endUpdate();    // save the calendar now
	Undo::saveDeletes(events);
}

/******************************************************************************
* Called when the group of items selected changes.
* Enable/disable the buttons depending on whether/how many templates are
* currently highlighted.
*/
void TemplateDlg::slotSelectionChanged()
{
	int count = mTemplateList->selectedCount();
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
