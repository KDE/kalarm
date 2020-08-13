/*
 *  newalarmaction.h  -  menu action to select a new alarm type
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2009, 2011, 2018 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NEWALARMACTION_H
#define NEWALARMACTION_H

#include "editdlg.h"
#include <KActionMenu>
#include <QMap>
#include <QAction>
class KActionCollection;
class TemplateMenuAction;

class NewAlarmAction : public KActionMenu
{
        Q_OBJECT
    public:
        NewAlarmAction(bool templates, const QString& label, QObject* parent, KActionCollection* collection = nullptr);
        virtual ~NewAlarmAction() {}
        QAction* displayAlarmAction(const QString& name);
        QAction* commandAlarmAction(const QString& name);
        QAction* emailAlarmAction(const QString& name);
        QAction* audioAlarmAction(const QString& name);
        TemplateMenuAction* fromTemplateAlarmAction(const QString& name);

    Q_SIGNALS:
        void   selected(EditAlarmDlg::Type);

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

#endif // NEWALARMACTION_H

// vim: et sw=4:
