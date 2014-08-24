/*
 *  messagebox.cpp  -  enhanced KMessageBox class
 *  Program:  kalarm
 *  Copyright © 2004,2005,2007,2008,2011,2014 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"   //krazy:exclude=includes (kalarm.h must be first)
#include "messagebox.h"

#include <kconfiggroup.h>
#include <ksharedconfig.h>
#include <kglobal.h>


QMap<QString, KMessageBox::ButtonCode> KAMessageBox::mContinueDefaults;

const KAMessageBox::Options KAMessageBox::NoAppModal = KMessageBox::Options(KMessageBox::Notify | KAMessageBox::WindowModal);

/******************************************************************************
* Set the default button for continue/cancel message boxes with the specified
* 'dontAskAgainName'.
*/
void KAMessageBox::setContinueDefault(const QString& dontAskAgainName, ButtonCode defaultButton)
{
    mContinueDefaults[dontAskAgainName] = (defaultButton == Cancel ? Cancel : Continue);
}

/******************************************************************************
* Get the default button for continue/cancel message boxes with the specified
* 'dontAskAgainName'.
*/
KMessageBox::ButtonCode KAMessageBox::getContinueDefault(const QString& dontAskAgainName)
{
    ButtonCode defaultButton = Continue;
    if (!dontAskAgainName.isEmpty())
    {
        QMap<QString, ButtonCode>::ConstIterator it = mContinueDefaults.constFind(dontAskAgainName);
        if (it != mContinueDefaults.constEnd())
            defaultButton = it.value();
    }
    return defaultButton;
}

/******************************************************************************
* If there is no current setting for whether a non-yes/no message box should be
* shown, set it to 'defaultShow'.
* If a continue/cancel message box has Cancel as the default button, either
* setContinueDefault() or warningContinueCancel() must have been called
* previously to set this for this 'dontShowAgainName' value.
* Reply = true if 'defaultShow' was written.
*/
bool KAMessageBox::setDefaultShouldBeShownContinue(const QString& dontShowAgainName, bool defaultShow)
{
    if (dontShowAgainName.isEmpty())
        return false;
    // First check whether there is an existing setting
    KConfigGroup config(KGlobal::config(), "Notification Messages");
    if (config.hasKey(dontShowAgainName))
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
bool KAMessageBox::shouldBeShownContinue(const QString& dontShowAgainName)
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
void KAMessageBox::saveDontShowAgainYesNo(const QString& dontShowAgainName, bool dontShow, ButtonCode result)
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
void KAMessageBox::saveDontShowAgainContinue(const QString& dontShowAgainName, bool dontShow)
{
    if (getContinueDefault(dontShowAgainName) == Cancel)
        saveDontShowAgainYesNo(dontShowAgainName, dontShow, Yes);
    else
        saveDontShowAgain(dontShowAgainName, false, dontShow);
}

/******************************************************************************
* Save whether the message box should not be shown again.
*/
void KAMessageBox::saveDontShowAgain(const QString& dontShowAgainName, bool yesno, bool dontShow, const char* yesnoResult)
{
    if (dontShowAgainName.isEmpty())
        return;
    KConfigGroup config(KGlobal::config(), "Notification Messages");
    KConfig::WriteConfigFlags flags = (dontShowAgainName[0] == QLatin1Char(':')) ? KConfig::Global | KConfig::Persistent : KConfig::Persistent;
    if (yesno)
        config.writeEntry(dontShowAgainName, QString::fromLatin1(dontShow ? yesnoResult : ""), flags);
    else
        config.writeEntry(dontShowAgainName, !dontShow, flags);
    config.sync();
}

// vim: et sw=4:
