/*
 *  templatemenuaction.h  -  menu action to select a template
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005, 2006, 2008, 2011 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TEMPLATEMENUACTION_H
#define TEMPLATEMENUACTION_H

#include <KActionMenu>
#include <QIcon>
#include <QMap>
class QAction;
namespace KAlarmCal { class KAEvent; }

class TemplateMenuAction : public KActionMenu
{
        Q_OBJECT
    public:
        TemplateMenuAction(const QIcon& icon, const QString& label, QObject* parent);
        virtual ~TemplateMenuAction() {}
    Q_SIGNALS:
        void   selected(const KAlarmCal::KAEvent*);

    private Q_SLOTS:
        void   slotInitMenu();
        void   slotSelected(QAction*);

    private:
        QMap<QAction*, QString> mOriginalTexts;   // menu item texts without added ampersands
};

#endif // TEMPLATEMENUACTION_H

// vim: et sw=4:
