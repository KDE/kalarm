/*
 *  messagebox.cpp  -  tweaked KMessageBox class
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "kalarm.h"
#include <kconfig.h>
#include "messagebox.h"


KConfig* MessageBox::mConfig = 0;
QMap<QString, KMessageBox::ButtonCode> MessageBox::mContinueDefaults;


/******************************************************************************
* Set the default button for continue/cancel message boxes with the specified
* 'dontAskAgainName'.
*/
void MessageBox::setContinueDefault(const QString& dontAskAgainName, ButtonCode defaultButton)
{
	mContinueDefaults[dontAskAgainName] = (defaultButton == Cancel ? Cancel : Continue);
}

/******************************************************************************
* Get the default button for continue/cancel message boxes with the specified
* 'dontAskAgainName'.
*/
KMessageBox::ButtonCode MessageBox::getContinueDefault(const QString& dontAskAgainName)
{
	ButtonCode defaultButton = Continue;
	if (!dontAskAgainName.isEmpty())
	{
		QMap<QString, ButtonCode>::ConstIterator it = mContinueDefaults.find(dontAskAgainName);
		if (it != mContinueDefaults.end())
			defaultButton = it.data();
	}
	return defaultButton;
}

/******************************************************************************
* Continue/cancel message box.
* If 'dontAskAgainName' is specified:
*   1) The message box will only be suppressed if the user chose Continue last time.
*   2) The default button is that last set with either setContinueDefault() or
*      warningContinueCancel() for that 'dontAskAgainName' value. If neither method
*      has set a default button, Continue is the default.
*/
int MessageBox::warningContinueCancel(QWidget* parent, const QString& text, const QString& caption,
		                              const KGuiItem& buttonContinue, const QString& dontAskAgainName,
		                              int options)
{
	ButtonCode defaultButton = getContinueDefault(dontAskAgainName);
	return warningContinueCancel(parent, defaultButton, text, caption, buttonContinue, dontAskAgainName, options);
}

/******************************************************************************
* Continue/cancel message box with the option as to which button is the default.
* If 'dontAskAgainName' is specified, the message box will only be suppressed
* if the user chose Continue last time.
*/
int MessageBox::warningContinueCancel(QWidget* parent, ButtonCode defaultButton, const QString& text,
		                              const QString& caption, const KGuiItem& buttonContinue,
		                              const QString& dontAskAgainName, int options)
{
	setContinueDefault(dontAskAgainName, defaultButton);
	if (defaultButton != Cancel)
		return KMessageBox::warningContinueCancel(parent, text, caption, buttonContinue, dontAskAgainName, options);

	// Cancel is the default button, so we have to use KMessageBox::warningYesNo()
	if (!dontAskAgainName.isEmpty())
	{
		ButtonCode b;
		if (!shouldBeShownYesNo(dontAskAgainName, b)
		&&  b != KMessageBox::Yes)
		{
			// Notification has been suppressed, but No (alias Cancel) is the default,
			// so unsuppress notification.
			saveDontShowAgain(dontAskAgainName, true, false);
		}
	}
	return warningYesNo(parent, text, caption, buttonContinue, KStdGuiItem::cancel(), dontAskAgainName, options);
}

/******************************************************************************
* If there is no current setting for whether a non-yes/no message box should be
* shown, set it to 'defaultShow'.
* If a continue/cancel message box has Cancel as the default button, either
* setContinueDefault() or warningContinueCancel() must have been called
* previously to set this for this 'dontShowAgainName' value.
* Reply = true if 'defaultShow' was written.
*/
bool MessageBox::setDefaultShouldBeShownContinue(const QString& dontShowAgainName, bool defaultShow)
{
    if (dontShowAgainName.isEmpty())
		return false;
	// First check whether there is an existing setting
	KConfig* config = mConfig ? mConfig : KGlobal::config();
	config->setGroup(QString::fromLatin1("Notification Messages"));
	if (config->hasKey(dontShowAgainName))
		return false;

	// There is no current setting, so write one
	saveDontShowAgainContinue(dontShowAgainName, !defaultShow);
	return true;
}

/******************************************************************************
* Return whether a non-yes/no message box should be shown.
* If the message box has Cancel as the default button, either setContinueDefault()
* or warningContinueCancel() must have been called previously to set this for this
* 'dontShowAgainName' value.
*/
bool MessageBox::shouldBeShownContinue(const QString& dontShowAgainName)
{
	if (getContinueDefault(dontShowAgainName) != Cancel)
		return KMessageBox::shouldBeShownContinue(dontShowAgainName);
	// Cancel is the default button, so we have to use a yes/no message box
	ButtonCode b;
	return shouldBeShownYesNo(dontShowAgainName, b);
}


/******************************************************************************
* Save whether the yes/no message box should not be shown again.
* If 'dontShow' is true, the message box will be suppressed and it will return
* 'result'.
*/
void MessageBox::saveDontShowAgainYesNo(const QString& dontShowAgainName, bool dontShow, ButtonCode result)
{
	saveDontShowAgain(dontShowAgainName, true, dontShow, (result == Yes ? "yes" : "no"));
}

/******************************************************************************
* Save whether a non-yes/no message box should not be shown again.
* If 'dontShow' is true, the message box will be suppressed and it will return
* Continue.
* If the message box has Cancel as the default button, either setContinueDefault()
* or warningContinueCancel() must have been called previously to set this for this
* 'dontShowAgainName' value.
*/
void MessageBox::saveDontShowAgainContinue(const QString& dontShowAgainName, bool dontShow)
{
	if (getContinueDefault(dontShowAgainName) == Cancel)
		saveDontShowAgainYesNo(dontShowAgainName, dontShow, Yes);
	else
		saveDontShowAgain(dontShowAgainName, false, dontShow);
}

/******************************************************************************
* Save whether the message box should not be shown again.
*/
void MessageBox::saveDontShowAgain(const QString& dontShowAgainName, bool yesno, bool dontShow, const char* yesnoResult)
{
	if (dontShowAgainName.isEmpty())
		return;
	KConfig* config = mConfig ? mConfig : KGlobal::config();
	config->setGroup(QString::fromLatin1("Notification Messages"));
	bool global = (dontShowAgainName[0] == ':');
	if (yesno)
		config->writeEntry(dontShowAgainName, QString::fromLatin1(dontShow ? yesnoResult : ""), true, global);
	else
		config->writeEntry(dontShowAgainName, !dontShow, true, global);
	config->sync();
}
