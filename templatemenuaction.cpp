/*
 *  templatemenuaction.cpp  -  menu action to select a template
 *  Program:  kalarm
 *  Copyright (C) 2005 by David Jarvie <software@astrojar.org.uk>
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

#include <kactionclasses.h>
#include <kmenu.h>
#include <kdebug.h>

#include "alarmcalendar.h"
#include "alarmevent.h"
#include "functions.h"
#include "templatemenuaction.moc"
//Added by qt3to4:
#include <Q3ValueList>


TemplateMenuAction::TemplateMenuAction(const QString& label, const QString& icon, QObject* receiver,
                                       const char* slot, KActionCollection* actions, const char* name)
	: KActionMenu(label, icon, actions, name)
{
	setDelayed(false);
	connect(popupMenu(), SIGNAL(aboutToShow()), SLOT(slotInitMenu()));
	connect(popupMenu(), SIGNAL(activated(int)), SLOT(slotSelected(int)));
	connect(this, SIGNAL(selected(const KAEvent&)), receiver, slot);
}

/******************************************************************************
*  Called when the New From Template action is clicked.
*  Creates a popup menu listing all alarm templates.
*/
void TemplateMenuAction::slotInitMenu()
{
	KMenu* menu = popupMenu();
	menu->clear();
	mOriginalTexts.clear();
	Q3ValueList<KAEvent> templates = KAlarm::templateList();
	for (Q3ValueList<KAEvent>::Iterator it = templates.begin();  it != templates.end();  ++it)
	{
		QString name = (*it).templateName();
		menu->insertItem(name);
		mOriginalTexts += name;
	}
}

/******************************************************************************
*  Called when a template is selected from the New From Template popup menu.
*  Executes a New Alarm dialog, preset from the selected template.
*/
void TemplateMenuAction::slotSelected(int id)
{
	KMenu* menu = popupMenu();
	QString item = mOriginalTexts[menu->indexOf(id)];
	if (!item.isEmpty())
	{
		AlarmCalendar* cal = AlarmCalendar::templateCalendarOpen();
		if (cal)
		{
			KAEvent templ = KAEvent::findTemplateName(*cal, item);
			emit selected(templ);
		}
	}
}
