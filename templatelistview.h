/*
 *  templatelistview.h  -  widget showing list of alarm templates
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

#ifndef TEMPLATELISTVIEW_H
#define TEMPLATELISTVIEW_H

#include "kalarm.h"

#include <qmap.h>
#include <klistview.h>

#include "eventlistviewbase.h"

class TemplateListView;


class TemplateListViewItem : public EventListViewItemBase
{
	public:
		TemplateListViewItem(TemplateListView* parent, const KAEvent&);
		TemplateListView*      templateListView() const  { return (TemplateListView*)listView(); }
		// Overridden base class methods
		TemplateListViewItem*  nextSibling() const       { return (TemplateListViewItem*)QListViewItem::nextSibling(); }
		virtual QString        key(int column, bool ascending) const;
	protected:
		virtual QString        lastColumnText() const;
	private:
		QString                mIconOrder;        // controls ordering of icon column
};


class TemplateListView : public EventListViewBase
{
		Q_OBJECT
	public:
		explicit TemplateListView(bool includeCmdAlarms, const QString& whatsThisText, QWidget* parent = 0, const char* name = 0);
		~TemplateListView();
		int                    iconColumn() const     { return mIconColumn; }
		int                    nameColumn() const     { return mNameColumn; }
		// Overridden base class methods
		static void            addEvent(const KAEvent& e, EventListViewBase* v)
		                             { EventListViewBase::addEvent(e, mInstanceList, v); }
		static void            modifyEvent(const KAEvent& e, EventListViewBase* v)
		                             { EventListViewBase::modifyEvent(e.id(), e, mInstanceList, v); }
		static void            modifyEvent(const QString& oldEventID, const KAEvent& newEvent, EventListViewBase* v)
		                             { EventListViewBase::modifyEvent(oldEventID, newEvent, mInstanceList, v); }
		static void            deleteEvent(const QString& eventID)
		                             { EventListViewBase::deleteEvent(eventID, mInstanceList); }
		TemplateListViewItem*  getEntry(const QString& eventID)  { return (TemplateListViewItem*)EventListViewBase::getEntry(eventID); }
		TemplateListViewItem*  selectedItem() const   { return (TemplateListViewItem*)EventListViewBase::selectedItem(); }
		TemplateListViewItem*  currentItem() const    { return (TemplateListViewItem*)EventListViewBase::currentItem(); }
		TemplateListViewItem*  firstChild() const     { return (TemplateListViewItem*)EventListViewBase::firstChild(); }
		virtual void           setSelected(QListViewItem* item, bool selected)         { EventListViewBase::setSelected(item, selected); }
		virtual void           setSelected(TemplateListViewItem* item, bool selected)  { EventListViewBase::setSelected(item, selected); }
		virtual QValueList<EventListViewBase*> instances()   { return mInstanceList; }

	protected:
		virtual void           populate();
		EventListViewItemBase* createItem(const KAEvent&);
		virtual QString        whatsThisText(int column) const;

	private:
		static QValueList<EventListViewBase*> mInstanceList;
		QString                mWhatsThisText;    // default QWhatsThis text
		int                    mIconColumn;       // index to icon column
		int                    mNameColumn;       // index to template name column
		bool                   mExcludeCmdAlarms; // omit command alarms from the list
};

#endif // TEMPLATELISTVIEW_H

