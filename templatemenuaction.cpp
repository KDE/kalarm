/*
 *  templatemenuaction.cpp  -  menu action to select a template
 *  Program:  kalarm
 *  (C) 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "kalarm.h"

#include <kactionclasses.h>
#include <kpopupmenu.h>
#include <kdebug.h>

#include "alarmcalendar.h"
#include "alarmevent.h"
#include "functions.h"
#include "templatemenuaction.moc"


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
	KPopupMenu* menu = popupMenu();
	menu->clear();
	QPtrList<KAEvent> templates = KAlarm::templateList();
	for (QPtrList<KAEvent>::ConstIterator it = templates.begin();  it != templates.end();  ++it)
		menu->insertItem((*it)->templateName());
}

/******************************************************************************
*  Called when a template is selected from the New From Template popup menu.
*  Executes a New Alarm dialog, preset from the selected template.
*/
void TemplateMenuAction::slotSelected(int id)
{
	KPopupMenu* menu = popupMenu();
	QString item = menu->text(id);
	if (!item.isEmpty())
	{
		AlarmCalendar* cal = AlarmCalendar::templateCalendarOpen();
		if (cal)
		{
			KAEvent templ = KAEvent::findTemplateName(*cal, menu->text(id));
			emit selected(templ);
		}
	}
}
