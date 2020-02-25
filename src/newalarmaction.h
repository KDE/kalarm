/*
 *  newalarmaction.h  -  menu action to select a new alarm type
 *  Program:  kalarm
 *  Copyright © 2007-2009,2011,2018 David Jarvie <djarvie@kde.org>
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
