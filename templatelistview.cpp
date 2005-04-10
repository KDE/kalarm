/*
 *  templatelistview.cpp  -  widget showing list of alarm templates
 *  Program:  kalarm
 *  (C) 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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

#include <klocale.h>
#include <kdebug.h>

#include "alarmcalendar.h"
#include "functions.h"
#include "templatelistview.moc"


/*=============================================================================
=  Class: TemplateListView
=  Displays the list of outstanding alarms.
=============================================================================*/
QValueList<EventListViewBase*>  TemplateListView::mInstanceList;


TemplateListView::TemplateListView(bool includeCmdAlarms, const QString& whatsThisText, QWidget* parent, const char* name)
	: EventListViewBase(parent, name),
	  mWhatsThisText(whatsThisText),
	  mIconColumn(0),
	  mNameColumn(1),
	  mExcludeCmdAlarms(!includeCmdAlarms)
{
	addColumn(QString::null);          // icon column
	addLastColumn(i18n("Name"));
	setSorting(mNameColumn);           // sort initially by name
	setColumnAlignment(mIconColumn, Qt::AlignHCenter);
	setColumnWidthMode(mIconColumn, QListView::Maximum);

	mInstanceList.append(this);
}

TemplateListView::~TemplateListView()
{
	mInstanceList.remove(this);
}

/******************************************************************************
*  Add all the templates to the list.
*/
void TemplateListView::populate()
{
	QPtrList<KAEvent> templates = KAlarm::templateList();
	for (KAEvent* e = templates.first();  e;  e = templates.next())
		addEntry(*e);
}

/******************************************************************************
*  Create a new list item for addEntry().
*/
EventListViewItemBase* TemplateListView::createItem(const KAEvent& event)
{
	return new TemplateListViewItem(this, event);
}

/******************************************************************************
*  Returns the QWhatsThis text for a specified column.
*/
QString TemplateListView::whatsThisText(int column) const
{
	if (column == mIconColumn)
		return i18n("Alarm type");
	if (column == mNameColumn)
		return i18n("Name of the alarm template");
	return mWhatsThisText;
}


/*=============================================================================
=  Class: TemplateListViewItem
=  Contains the details of one alarm for display in the TemplateListView.
=============================================================================*/

TemplateListViewItem::TemplateListViewItem(TemplateListView* parent, const KAEvent& event)
	: EventListViewItemBase(parent, event)
{
	setLastColumnText();     // set the template name column text

	int index;
	switch (event.action())
	{
		case KAAlarm::FILE:     index = 2;  break;
		case KAAlarm::COMMAND:  index = 3;  break;
		case KAAlarm::EMAIL:    index = 4;  break;
		case KAAlarm::MESSAGE:
		default:                index = 1;  break;
	}
	mIconOrder.sprintf("%01u", index);
	setPixmap(templateListView()->iconColumn(), *eventIcon());
}

/******************************************************************************
*  Return the alarm summary text.
*/
QString TemplateListViewItem::lastColumnText() const
{
	return event().templateName();
}

/******************************************************************************
*  Return the column sort order for one item in the list.
*/
QString TemplateListViewItem::key(int column, bool) const
{
	TemplateListView* listView = templateListView();
	if (column == listView->iconColumn())
		return mIconOrder;
	return text(column).lower();
}

