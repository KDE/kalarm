/*
 *  newalarmaction.h  -  menu action to select a new alarm type
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "editdlg.h"

#include <KActionMenu>
#include <QMap>
#include <QAction>

namespace KAlarmCal { class KAEvent; }
class KActionCollection;
class TemplateMenuAction;

class NewAlarmAction : public KActionMenu
{
    Q_OBJECT
public:
    /** Create a New Alarm action which provides a sub-menu containing each alarm type.
     *  @param templates   The sub-menu should provide actions to create an alarm
     *                     template of each type, not an alarm of each type.
     *  @param label       The action's label.
     *  @param parent      Parent widget.
     *  @param collection  Action collection. If supplied, each action must be
     *                     given a name within the collection using setActionNames().
     */
    NewAlarmAction(bool templates, const QString& label, QObject* parent, KActionCollection* collection = nullptr);

    ~NewAlarmAction() override {}

    /** If a KActionCollection was supplied to the constructor, add the actions
     *  to the collection, and configure to allow the user to set a global shortcut
     *  for the non-alarm template actions.
     *  @param displayName   Name of 'New display alarm' action within the KActionCollection.
     *  @param commandName   Name of 'New command alarm' action within the KActionCollection.
     *  @param emailName     Name of 'New email alarm' action within the KActionCollection.
     *  @param audioName     Name of 'New audio alarm' action within the KActionCollection.
     *  @param templateName  Name of 'New alarm from template' action within the KActionCollection.
     */
    void setActionNames(const QString& displayName, const QString& commandName,
                        const QString& emailName, const QString& audioName,
                        const QString& templateName = QString());

    /** Return the 'New display alarm' action. */
    QAction* displayAlarmAction() const  { return mDisplayAction; }

    /** Return the 'New command alarm' action. */
    QAction* commandAlarmAction() const  { return mCommandAction; }

    /** Return the 'New email alarm' action. */
    QAction* emailAlarmAction() const  { return mEmailAction; }

    /** Return the 'New audio alarm' action. */
    QAction* audioAlarmAction() const  { return mAudioAction; }

    /** Return the 'New alarm from template' action. */
    TemplateMenuAction* fromTemplateAlarmAction() const  { return mTemplateAction; }

Q_SIGNALS:
    /** Signal emitted when a new alarm action is selected from the sub-menu. */
    void selected(EditAlarmDlg::Type);

    /** Signal emitted when a new alarm from template action is selected from
     *  the sub-menu. */
    void selectedTemplate(const KAlarmCal::KAEvent&);

private Q_SLOTS:
    void   slotSelected(QAction*);
    void   slotInitMenu();
    void   slotCalendarStatusChanged();

private:
    QAction*            mDisplayAction;
    QAction*            mCommandAction;
    QAction*            mEmailAction;
    QAction*            mAudioAction;
    TemplateMenuAction* mTemplateAction {nullptr};  // New From Template action, for non-template menu only
    KActionCollection*  mActionCollection;
    QMap<QAction*, EditAlarmDlg::Type> mTypes;
};


// vim: et sw=4:
