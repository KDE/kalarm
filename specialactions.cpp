/*
 *  specialactions.cpp  -  widget to specify special alarm actions
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

#include <QLabel>
#include <QVBoxLayout>
#include <QResizeEvent>

#include <klineedit.h>
#include <kapplication.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kdebug.h>

#include "functions.h"
#include "shellprocess.h"
#include "specialactions.moc"


/*=============================================================================
= Class SpecialActionsButton
= Button to display the Special Alarm Actions dialogue.
=============================================================================*/

SpecialActionsButton::SpecialActionsButton(const QString& caption, QWidget* parent)
	: QPushButton(caption, parent),
	  mReadOnly(false)
{
	connect(this, SIGNAL(clicked()), SLOT(slotButtonPressed()));
	setWhatsThis(i18n("Specify actions to execute before and after the alarm is displayed."));
}

/******************************************************************************
*  Set the pre- and post-alarm actions.
*  The button's pressed state is set to reflect whether any actions are set.
*/
void SpecialActionsButton::setActions(const QString& pre, const QString& post)
{
	mPreAction  = pre;
	mPostAction = post;
	setDown(!mPreAction.isEmpty() || !mPostAction.isEmpty());
}

/******************************************************************************
*  Called when the OK button is clicked.
*  Display a font and colour selection dialog and get the selections.
*/
void SpecialActionsButton::slotButtonPressed()
{
	SpecialActionsDlg dlg(mPreAction, mPostAction, i18n("Special Alarm Actions"), this);
	dlg.setReadOnly(mReadOnly);
	if (dlg.exec() == QDialog::Accepted)
	{
		setActions(dlg.preAction(), dlg.postAction());
		emit selected();
	}
}


/*=============================================================================
= Class SpecialActionsDlg
= Pre- and post-alarm actions dialogue.
=============================================================================*/

static const char SPEC_ACT_DIALOG_NAME[] = "SpecialActionsDialog";


SpecialActionsDlg::SpecialActionsDlg(const QString& preAction, const QString& postAction,
                                     const QString& caption, QWidget* parent)
	: KDialog(parent )
{
	setCaption(caption);
	setButtons(Ok|Cancel);
	setDefaultButton(Ok);
	connect(this, SIGNAL(okClicked()), SLOT(slotOk()));

	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* layout = new QVBoxLayout(page);
	layout->setMargin(0);
	layout->setSpacing(spacingHint());

	mActions = new SpecialActions(page);
	mActions->setActions(preAction, postAction);
	layout->addWidget(mActions);
	layout->addSpacing(spacingHint());

	QSize s;
	if (KAlarm::readConfigWindowSize(SPEC_ACT_DIALOG_NAME, s))
		resize(s);
}

/******************************************************************************
*  Called when the OK button is clicked.
*/
void SpecialActionsDlg::slotOk()
{
	if (mActions->isReadOnly())
		reject();
	accept();
}

/******************************************************************************
*  Called when the dialog's size has changed.
*  Records the new size in the config file.
*/
void SpecialActionsDlg::resizeEvent(QResizeEvent* re)
{
	if (isVisible())
		KAlarm::writeConfigWindowSize(SPEC_ACT_DIALOG_NAME, re->size());
	KDialog::resizeEvent(re);
}


/*=============================================================================
= Class SpecialActions
= Pre- and post-alarm actions widget.
=============================================================================*/

SpecialActions::SpecialActions(QWidget* parent)
	: QWidget(parent),
	  mReadOnly(false)
{
	QVBoxLayout* topLayout = new QVBoxLayout(this);
	topLayout->setMargin(0);
	topLayout->setSpacing(KDialog::spacingHint());

	// Pre-alarm action
	QLabel* label = new QLabel(i18n("Pre-alarm action:"), this);
	label->setFixedSize(label->sizeHint());
	topLayout->addWidget(label, 0, Qt::AlignLeft);

	mPreAction = new KLineEdit(this);
	label->setBuddy(mPreAction);
	mPreAction->setWhatsThis(i18n("Enter a shell command to execute before the alarm is displayed.\n"
	                              "Note that it is executed only when the alarm proper is displayed, not when a reminder or deferred alarm is displayed.\n"
	                              "N.B. KAlarm will wait for the command to complete before displaying the alarm."));
	topLayout->addWidget(mPreAction);
	topLayout->addSpacing(KDialog::spacingHint());

	// Post-alarm action
	label = new QLabel(i18n("Post-alarm action:"), this);
	label->setFixedSize(label->sizeHint());
	topLayout->addWidget(label, 0, Qt::AlignLeft);

	mPostAction = new KLineEdit(this);
	label->setBuddy(mPostAction);
	mPostAction->setWhatsThis(i18n("Enter a shell command to execute after the alarm window is closed.\n"
	                               "Note that it is not executed after closing a reminder window. If you defer "
	                               "the alarm, it is not executed until the alarm is finally acknowledged or closed."));
	topLayout->addWidget(mPostAction);
}

void SpecialActions::setActions(const QString& pre, const QString& post)
{
	mPreAction->setText(pre);
	mPostAction->setText(post);
}

QString SpecialActions::preAction() const
{
	return mPreAction->text();
}

QString SpecialActions::postAction() const
{
	return mPostAction->text();
}

void SpecialActions::setReadOnly(bool ro)
{
	mReadOnly = ro;
	mPreAction->setReadOnly(mReadOnly);
	mPostAction->setReadOnly(mReadOnly);
}
