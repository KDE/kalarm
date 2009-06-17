/*
 *  specialactions.cpp  -  widget to specify special alarm actions
 *  Program:  kalarm
 *  Copyright © 2004-2009 by David Jarvie <djarvie@kde.org>
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
#include "specialactions.moc"

#include "autoqpointer.h"
#include "checkbox.h"
#include "functions.h"
#include "shellprocess.h"

#include <klineedit.h>
#include <khbox.h>
#include <kapplication.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kdebug.h>

#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QResizeEvent>


/*=============================================================================
= Class SpecialActionsButton
= Button to display the Special Alarm Actions dialog.
=============================================================================*/

SpecialActionsButton::SpecialActionsButton(bool enableCancelOnError, QWidget* parent)
	: QPushButton(i18nc("@action:button", "Special Actions..."), parent),
	  mEnableCancel(enableCancelOnError),
	  mReadOnly(false)
{
	setCheckable(true);
	setChecked(false);
	connect(this, SIGNAL(clicked()), SLOT(slotButtonPressed()));
	setWhatsThis(i18nc("@info:whatsthis", "Specify actions to execute before and after the alarm is displayed."));
}

/******************************************************************************
*  Set the pre- and post-alarm actions.
*  The button's pressed state is set to reflect whether any actions are set.
*/
void SpecialActionsButton::setActions(const QString& pre, const QString& post, bool cancelOnError)
{
	mPreAction     = pre;
	mPostAction    = post;
	mCancelOnError = cancelOnError;
	setChecked(!mPreAction.isEmpty() || !mPostAction.isEmpty());
}

/******************************************************************************
*  Called when the OK button is clicked.
*  Display a font and colour selection dialog and get the selections.
*/
void SpecialActionsButton::slotButtonPressed()
{
	// Use AutoQPointer to guard against crash on application exit while
	// the dialogue is still open. It prevents double deletion (both on
	// deletion of SpecialActionsButton, and on return from this function).
	AutoQPointer<SpecialActionsDlg> dlg = new SpecialActionsDlg(mPreAction, mPostAction, mCancelOnError, mEnableCancel, this);
	dlg->setReadOnly(mReadOnly);
	if (dlg->exec() == QDialog::Accepted)
	{
		mPreAction     = dlg->preAction();
		mPostAction    = dlg->postAction();
		mCancelOnError = dlg->cancelOnError();
		emit selected();
	}
        if (dlg)
	        setChecked(!mPreAction.isEmpty() || !mPostAction.isEmpty());
}


/*=============================================================================
= Class SpecialActionsDlg
= Pre- and post-alarm actions dialog.
=============================================================================*/

static const char SPEC_ACT_DIALOG_NAME[] = "SpecialActionsDialog";


SpecialActionsDlg::SpecialActionsDlg(const QString& preAction, const QString& postAction,
                                     bool cancelOnError, bool enableCancelOnError, QWidget* parent)
	: KDialog(parent)
{
#ifdef __GNUC__
#warning Dialogue appears below edit dialogue when Edit button in alarm message window is clicked
#endif
	setCaption(i18nc("@title:window", "Special Alarm Actions"));
	setButtons(Ok|Cancel);
	setDefaultButton(Ok);
	connect(this, SIGNAL(okClicked()), SLOT(slotOk()));

	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* layout = new QVBoxLayout(page);
	layout->setMargin(0);
	layout->setSpacing(spacingHint());

	mActions = new SpecialActions(enableCancelOnError, page);
	mActions->setActions(preAction, postAction, cancelOnError);
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

SpecialActions::SpecialActions(bool enableCancelOnError, QWidget* parent)
	: QWidget(parent),
	  mEnableCancel(enableCancelOnError),
	  mReadOnly(false)
{
	QVBoxLayout* topLayout = new QVBoxLayout(this);
	topLayout->setMargin(0);
	topLayout->setSpacing(KDialog::spacingHint());

	// Pre-alarm action
	QGroupBox* group = new QGroupBox(i18nc("@title:group", "Pre-Alarm Action"), this);
	topLayout->addWidget(group);
	QVBoxLayout* vlayout = new QVBoxLayout(group);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());

	KHBox* box = new KHBox(group);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	vlayout->addWidget(box);
	QLabel* label = new QLabel(i18nc("@label:textbox", "Command:"), box);
	mPreAction = new KLineEdit(box);
	label->setBuddy(mPreAction);
	connect(mPreAction, SIGNAL(textChanged(const QString&)), SLOT(slotPreActionChanged(const QString&)));
	box->setWhatsThis(i18nc("@info:whatsthis",
	                        "<para>Enter a shell command to execute before the alarm is displayed.</para>"
	                        "<para>Note that it is executed only when the alarm proper is displayed, not when a reminder or deferred alarm is displayed.</para>"
	                        "<para><note>KAlarm will wait for the command to complete before displaying the alarm.</note></para>"));
	box->setStretchFactor(mPreAction, 1);

	// Cancel if error in pre-alarm action
	mCancelOnError = new CheckBox(i18nc("@option:check", "Cancel alarm on error"), group);
	mCancelOnError->setWhatsThis(i18nc("@info:whatsthis", "Cancel the alarm if the pre-alarm command fails, i.e. do not display the alarm or execute any post-alarm action command."));
	vlayout->addWidget(mCancelOnError, 0, Qt::AlignLeft);

	// Post-alarm action
	group = new QGroupBox(i18nc("@title:group", "Post-Alarm Action"), this);
	topLayout->addWidget(group);
	vlayout = new QVBoxLayout(group);
	vlayout->setMargin(KDialog::marginHint());
	vlayout->setSpacing(KDialog::spacingHint());

	box = new KHBox(group);   // this is to control the QWhatsThis text display area
	box->setMargin(0);
	box->setSpacing(KDialog::spacingHint());
	vlayout->addWidget(box);
	label = new QLabel(i18nc("@label:textbox", "Command:"), box);
	mPostAction = new KLineEdit(box);
	label->setBuddy(mPostAction);
	box->setWhatsThis(i18nc("@info:whatsthis",
	                        "<para>Enter a shell command to execute after the alarm window is closed.</para>"
	                        "<para>Note that it is not executed after closing a reminder window. If you defer "
	                        "the alarm, it is not executed until the alarm is finally acknowledged or closed.</para>"));
	box->setStretchFactor(mPostAction, 1);

	mCancelOnError->setEnabled(enableCancelOnError);
}

void SpecialActions::setActions(const QString& pre, const QString& post, bool cancelOnError)
{
	mPreAction->setText(pre);
	mPostAction->setText(post);
	mCancelOnError->setChecked(cancelOnError);
}

QString SpecialActions::preAction() const
{
	return mPreAction->text();
}

QString SpecialActions::postAction() const
{
	return mPostAction->text();
}

bool SpecialActions::cancelOnError() const
{
	return mCancelOnError->isChecked();
}

void SpecialActions::setReadOnly(bool ro)
{
	mReadOnly = ro;
	mPreAction->setReadOnly(mReadOnly);
	mPostAction->setReadOnly(mReadOnly);
	mCancelOnError->setReadOnly(mReadOnly);
}

void SpecialActions::slotPreActionChanged(const QString& text)
{
	if (!mEnableCancel)
		mCancelOnError->setEnabled(!text.isEmpty());
}
