/*
 *  templatemenuaction.cpp  -  menu action to select a template
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "templatemenuaction.h"

#include "functions.h"
#include "resourcescalendar.h"
#include "kalarmcalendar/kaevent.h"

#include <QMenu>


TemplateMenuAction::TemplateMenuAction(const QIcon& icon, const QString& label, QObject* parent)
    : KActionMenu(icon, label, parent)
{
    setPopupMode(QToolButton::InstantPopup);
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
    const QList<KAEvent> templates = KAlarm::templateList();
    for (const KAEvent& templ : templates)
    {
        const QString name = templ.name();
        int j = 0;
        for (int jend = sorted.count();
             j < jend  &&  QString::localeAwareCompare(name, sorted[j]) > 0;
             ++j) ;
        sorted.insert(j, name);
    }

    for (const QString& name : std::as_const(sorted))
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
    KAEvent templ = ResourcesCalendar::templateEvent(it.value());
    templ.setName(QString());   // don't preset the new alarm with the template's name
    Q_EMIT selected(templ);
}

// vim: et sw=4:

#include "moc_templatemenuaction.cpp"
