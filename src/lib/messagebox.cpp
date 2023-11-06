/*
 *  messagebox.cpp  -  enhanced KMessageBox class
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004, 2005, 2007, 2008, 2011, 2014 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "messagebox.h"

#include <KConfigGroup>
#include <KSharedConfig>


QMap<QString, KMessageBox::ButtonCode> KAMessageBox::mContinueDefaults;

const KMessageBox::Options KAMessageBox::NoAppModal = KMessageBox::Options(KMessageBox::Notify | KMessageBox::WindowModal);

/******************************************************************************
* Set the default button for continue/cancel message boxes with the specified
* 'dontAskAgainName'.
*/
void KAMessageBox::setContinueDefault(const QString& dontAskAgainName, KMessageBox::ButtonCode defaultButton)
{
    mContinueDefaults[dontAskAgainName] = (defaultButton == KMessageBox::Cancel ? KMessageBox::Cancel : KMessageBox::Continue);
}

/******************************************************************************
* Get the default button for continue/cancel message boxes with the specified
* 'dontAskAgainName'.
*/
KMessageBox::ButtonCode KAMessageBox::getContinueDefault(const QString& dontAskAgainName)
{
    KMessageBox::ButtonCode defaultButton = KMessageBox::Continue;
    if (!dontAskAgainName.isEmpty())
    {
        QMap<QString, KMessageBox::ButtonCode>::ConstIterator it = mContinueDefaults.constFind(dontAskAgainName);
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
    KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("Notification Messages"));
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
    if (getContinueDefault(dontShowAgainName) != KMessageBox::Cancel)
        return KMessageBox::shouldBeShownContinue(dontShowAgainName);
    // Cancel is the default button, so we have to use a yes/no message box
    KMessageBox::ButtonCode b;
    return shouldBeShownTwoActions(dontShowAgainName, b);
}


/******************************************************************************
* Save whether the yes/no message box should not be shown again.
* If 'dontShow' is true, the message box will be suppressed and it will return
* 'result'.
*/
void KAMessageBox::saveDontShowAgainYesNo(const QString& dontShowAgainName, bool dontShow, KMessageBox::ButtonCode result)
{
    saveDontShowAgain(dontShowAgainName, true, dontShow, (result == KMessageBox::ButtonCode::PrimaryAction ? "yes" : "no"));
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
    if (getContinueDefault(dontShowAgainName) == KMessageBox::Cancel)
        saveDontShowAgainYesNo(dontShowAgainName, dontShow, KMessageBox::ButtonCode::PrimaryAction);
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
    KConfigGroup config(KSharedConfig::openConfig(), QStringLiteral("Notification Messages"));
    KConfig::WriteConfigFlags flags = (dontShowAgainName[0] == QLatin1Char(':')) ? KConfig::Global | KConfig::Persistent : KConfig::Persistent;
    if (yesno)
        config.writeEntry(dontShowAgainName, QString::fromLatin1(dontShow ? yesnoResult : ""), flags);
    else
        config.writeEntry(dontShowAgainName, !dontShow, flags);
    config.sync();
}

// vim: et sw=4:
