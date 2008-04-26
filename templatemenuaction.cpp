/*
 *  templatemenuaction.cpp  -  menu action to select a template
 *  Program:  kalarm
 *  Copyright © 2005,2006,2008 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"


#include <kmenu.h>
#include <kdebug.h>
#include <kactionmenu.h>
#include <kactioncollection.h>
#include "alarmcalendar.h"
#include "alarmevent.h"
#include "functions.h"
#include "templatemenuaction.moc"


TemplateMenuAction::TemplateMenuAction(const KIcon& icon, const QString& label, KActionCollection* actions, const QString& name)
	: KActionMenu(icon, label, actions)
{
	setDelayed(false);
	connect(menu(), SIGNAL(aboutToShow()), SLOT(slotInitMenu()));
	connect(menu(), SIGNAL(triggered(QAction*)), SLOT(slotSelected(QAction*)));
	actions->addAction(name, this);
}

/******************************************************************************
* Called when the New From Template action is clicked.
* Creates a popup menu listing all alarm templates, in sorted name order.
*/
void TemplateMenuAction::slotInitMenu()
{
	KMenu* m = menu();
	m->clear();
	mOriginalTexts.clear();

	// Compile a sorted list of template names
	int i, end;
	QStringList sorted;
	KAEvent::List templates = KAlarm::templateList();
	for (i = 0, end = templates.count();  i < end;  ++i)
	{
		QString name = templates[i]->templateName();
		int j = 0;
		for (int jend = sorted.count();
		     j < jend  &&  QString::localeAwareCompare(name, sorted[j]) > 0;
		     ++j);
		sorted.insert(j, name);
	}

	for (i = 0, end = sorted.count();  i < end;  ++i)
	{
		QAction* act = m->addAction(sorted[i]);
		mOriginalTexts[act] = sorted[i];   // keep original text, since action text has shortcuts added
	}
}

/******************************************************************************
*  Called when a template is selected from the New From Template popup menu.
*  Executes a New Alarm dialog, preset from the selected template.
*/
void TemplateMenuAction::slotSelected(QAction* action)
{
	QMap<QAction*, QString>::const_iterator it = mOriginalTexts.find(action);
	if (it == mOriginalTexts.end()  ||  it.value().isEmpty())
		return;
	KAEvent* templ = AlarmCalendar::resources()->templateEvent(it.value());
	emit selected(templ);
}
