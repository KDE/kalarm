/*
 *  newalarmaction.cpp  -  menu action to select a new alarm type
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "newalarmaction.h"

#include "templatemenuaction.h"
#include "resources/datamodel.h"
#include "resources/resources.h"
#include "resources/eventmodel.h"
#include "lib/shellprocess.h"

#include <KActionCollection>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <KStandardShortcut>

#include <QMenu>

using namespace KAlarmCal;

#define DISP_ICON     QStringLiteral("window-new")
#define CMD_ICON      QStringLiteral("new-command-alarm")
#define MAIL_ICON     QStringLiteral("mail-message-new")
#define AUDIO_ICON    QStringLiteral("new-audio-alarm")
#define TEMPLATE_ICON QStringLiteral("document-new-from-template")
#define DISP_KEY      QKeySequence(Qt::CTRL | Qt::Key_D)
#define CMD_KEY       QKeySequence(Qt::CTRL | Qt::Key_C)
#define MAIL_KEY      QKeySequence(Qt::CTRL | Qt::Key_L)
#define AUDIO_KEY     QKeySequence(Qt::CTRL | Qt::Key_U)


/******************************************************************************
* Create New Alarm actions as a menu containing each alarm type, and add to
* the KActionCollection.
*/
NewAlarmAction::NewAlarmAction(bool templates, const QString& label, QObject* parent, KActionCollection* collection)
    : KActionMenu(QIcon::fromTheme(QStringLiteral("document-new")), label, parent)
    , mActionCollection(collection)
{
    mDisplayAction = new QAction(QIcon::fromTheme(DISP_ICON), (templates ? i18nc("@item:inmenu", "Display Alarm Template") : i18nc("@action", "New Display Alarm")), parent);
    menu()->addAction(mDisplayAction);
    mTypes[mDisplayAction] = EditAlarmDlg::DISPLAY;
    mCommandAction = new QAction(QIcon::fromTheme(CMD_ICON), (templates ? i18nc("@item:inmenu", "Command Alarm Template") : i18nc("@action", "New Command Alarm")), parent);
    menu()->addAction(mCommandAction);
    mTypes[mCommandAction] = EditAlarmDlg::COMMAND;
    mEmailAction = new QAction(QIcon::fromTheme(MAIL_ICON), (templates ? i18nc("@item:inmenu", "Email Alarm Template") : i18nc("@action", "New Email Alarm")), parent);
    menu()->addAction(mEmailAction);
    mTypes[mEmailAction] = EditAlarmDlg::EMAIL;
    mAudioAction = new QAction(QIcon::fromTheme(AUDIO_ICON), (templates ? i18nc("@item:inmenu", "Audio Alarm Template") : i18nc("@action", "New Audio Alarm")), parent);
    menu()->addAction(mAudioAction);
    mTypes[mAudioAction] = EditAlarmDlg::AUDIO;
    if (!templates)
    {
        // Sub-menu is to create new alarms (not new alarm templates).
        if (!mActionCollection)
        {
            mDisplayAction->setShortcut(DISP_KEY);
            mCommandAction->setShortcut(CMD_KEY);
            mEmailAction->setShortcut(MAIL_KEY);
            mAudioAction->setShortcut(AUDIO_KEY);
        }

        // Include New From Template only in non-template menu
        mTemplateAction = new TemplateMenuAction(QIcon::fromTheme(TEMPLATE_ICON), i18nc("@action", "New Alarm From Template"), parent);
        menu()->addAction(mTemplateAction);
        connect(mTemplateAction, &TemplateMenuAction::selected, this, &NewAlarmAction::selectedTemplate);
        connect(Resources::instance(), &Resources::settingsChanged, this, &NewAlarmAction::slotCalendarStatusChanged);
        connect(DataModel::allTemplateListModel(), &EventListModel::haveEventsStatus, this, &NewAlarmAction::slotCalendarStatusChanged);
        slotCalendarStatusChanged();   // initialise action states
    }
    setPopupMode(QToolButton::InstantPopup);
    connect(menu(), &QMenu::aboutToShow, this, &NewAlarmAction::slotInitMenu);
    connect(menu(), &QMenu::triggered, this, &NewAlarmAction::slotSelected);
}

/******************************************************************************
*/
void NewAlarmAction::setActionNames(const QString& displayName, const QString& commandName,
                                    const QString& emailName, const QString& audioName,
                                    const QString& templateName)
{
    if (mActionCollection)
    {
        mActionCollection->addAction(displayName, mDisplayAction);
        mActionCollection->setDefaultShortcut(mDisplayAction, DISP_KEY);
        KGlobalAccel::setGlobalShortcut(mDisplayAction, QList<QKeySequence>());  // allow user to set a global shortcut

        mActionCollection->addAction(commandName, mCommandAction);
        mActionCollection->setDefaultShortcut(mCommandAction, CMD_KEY);
        KGlobalAccel::setGlobalShortcut(mCommandAction, QList<QKeySequence>());  // allow user to set a global shortcut

        mActionCollection->addAction(emailName, mEmailAction);
        mActionCollection->setDefaultShortcut(mEmailAction, MAIL_KEY);
        KGlobalAccel::setGlobalShortcut(mEmailAction, QList<QKeySequence>());  // allow user to set a global shortcut

        mActionCollection->addAction(audioName, mAudioAction);
        mActionCollection->setDefaultShortcut(mAudioAction, AUDIO_KEY);
        KGlobalAccel::setGlobalShortcut(mAudioAction, QList<QKeySequence>());  // allow user to set a global shortcut

        if (mTemplateAction)
        {
            Q_ASSERT(!templateName.isEmpty());
            mActionCollection->addAction(templateName, mTemplateAction);
        }
    }
}

/******************************************************************************
* Called when the action is clicked.
*/
void NewAlarmAction::slotInitMenu()
{
    // Don't allow shell commands in kiosk mode
    mCommandAction->setEnabled(ShellProcess::authorised());
}

/******************************************************************************
* Called when an alarm type is selected from the New popup menu.
*/
void NewAlarmAction::slotSelected(QAction* action)
{
    QMap<QAction*, EditAlarmDlg::Type>::ConstIterator it = mTypes.constFind(action);
    if (it != mTypes.constEnd())
        Q_EMIT selected(it.value());
}

/******************************************************************************
* Called when the status of a calendar has changed.
* Enable or disable the New From Template action appropriately.
*/
void NewAlarmAction::slotCalendarStatusChanged()
{
    // Find whether there are any writable active alarm calendars
    bool active = !Resources::enabledResources(CalEvent::ACTIVE, true).isEmpty();
    bool haveEvents = DataModel::allTemplateListModel()->haveEvents();
    mTemplateAction->setEnabled(active && haveEvents);
    setEnabled(active);
}

// vim: et sw=4:
