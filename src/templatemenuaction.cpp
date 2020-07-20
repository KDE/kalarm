/*
 *  templatemenuaction.cpp  -  menu action to select a template
 *  Program:  kalarm
 *  Copyright © 2005-2020 David Jarvie <djarvie@kde.org>
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

#include "templatemenuaction.h"

#include "functions.h"
#include "resourcescalendar.h"
#include "kalarm_debug.h"

#include <KAlarmCal/KAEvent>

#include <KActionMenu>
#include <QMenu>


TemplateMenuAction::TemplateMenuAction(const QIcon& icon, const QString& label, QObject* parent)
    : KActionMenu(icon, label, parent)
{
    setDelayed(false);
    connect(menu(), &QMenu::aboutToShow, this, &TemplateMenuAction::slotInitMenu);
    connect(menu(), &QMenu::triggered, this, &TemplateMenuAction::slotSelected);
}

/******************************************************************************
* Called when the New From Template action is clicked.
* Creates a popup menu listing all alarm templates, in sorted name order.
*/
void TemplateMenuAction::slotInitMenu()
{
    QMenu* m = menu();
    m->clear();
    mOriginalTexts.clear();

    // Compile a sorted list of template names
    QStringList sorted;
    const QVector<KAEvent> templates = KAlarm::templateList();
    for (const KAEvent& templ : templates)
    {
        const QString name = templ.templateName();
        int j = 0;
        for (int jend = sorted.count();
             j < jend  &&  QString::localeAwareCompare(name, sorted[j]) > 0;
             ++j) ;
        sorted.insert(j, name);
    }

    for (const QString& name : sorted)
    {
        QAction* act = m->addAction(name);
        mOriginalTexts[act] = name;   // keep original text, since action text has shortcuts added
    }
}

/******************************************************************************
* Called when a template is selected from the New From Template popup menu.
* Executes a New Alarm dialog, preset from the selected template.
*/
void TemplateMenuAction::slotSelected(QAction* action)
{
    QMap<QAction*, QString>::ConstIterator it = mOriginalTexts.constFind(action);
    if (it == mOriginalTexts.constEnd()  ||  it.value().isEmpty())
        return;
    const KAEvent templ = ResourcesCalendar::templateEvent(it.value());
    Q_EMIT selected(templ.isValid() ? &templ : nullptr);
}

// vim: et sw=4:
