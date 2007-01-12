/*
 *  templatepickdlg.cpp  -  dialogue to choose an alarm template
 *  Program:  kalarm
 *  Copyright (c) 2004, 2006 by David Jarvie <software@astrojar.org.uk>
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

#include <QVBoxLayout>
#include <QResizeEvent>

#include <klocale.h>
#include <kdebug.h>

#include "functions.h"
#include "shellprocess.h"
#include "templatelistview.h"
#include "templatepickdlg.moc"

static const char TMPL_PICK_DIALOG_NAME[] = "TemplatePickDialog";


TemplatePickDlg::TemplatePickDlg(QWidget* parent)
	: KDialog(parent)
{
	QWidget* topWidget = new QWidget(this);
	setMainWidget(topWidget);
	setCaption(i18n("Choose Alarm Template"));
	setButtons(Ok|Cancel);
	setDefaultButton(Ok);
	QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
	topLayout->setMargin(0);
	topLayout->setSpacing(spacingHint());

	// Display the list of templates, but exclude command alarms if in kiosk mode.
	bool includeCmdAlarms = ShellProcess::authorised();
	mTemplateList = new TemplateListView(includeCmdAlarms, i18n("Select a template to base the new alarm on."), topWidget);
	mTemplateList->setSelectionMode(Q3ListView::Single);
	mTemplateList->refresh();      // populate the template list
	connect(mTemplateList, SIGNAL(selectionChanged()), SLOT(slotSelectionChanged()));
	connect(mTemplateList, SIGNAL(executed(Q3ListViewItem*)), SLOT(slotOk()));
	topLayout->addWidget(mTemplateList);

	slotSelectionChanged();        // enable or disable the OK button

	QSize s;
	if (KAlarm::readConfigWindowSize(TMPL_PICK_DIALOG_NAME, s))
		resize(s);
	connect(this,SIGNAL(okClicked()),SLOT(slotOk()));
}

/******************************************************************************
* Return the currently selected alarm template, or 0 if none.
*/
const KAEvent* TemplatePickDlg::selectedTemplate() const
{
	return mTemplateList->selectedEvent();
}

/******************************************************************************
* Called when the template selection changes.
* Enable/disable the OK button depending on whether anything is selected.
*/
void TemplatePickDlg::slotSelectionChanged()
{
	enableButtonOk(mTemplateList->selectedItem());
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
