/*
 *  eventlistviewbase.cpp  -  base classes for widget showing list of events
 *  Program:  kalarm
 *  Copyright (c) 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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

//Added by qt3to4:
#include <q3header.h>
#include <QWhatsThis>
#include <QPixmap>
#include <QShowEvent>
#include <QResizeEvent>

#include <kiconloader.h>
#include <kdebug.h>

#include "find.h"
#include "eventlistviewbase.moc"


/*=============================================================================
=  Class: EventListViewBase
=  Base class for displaying a list of events.
=============================================================================*/

EventListViewBase::EventListViewBase(QWidget* parent)
	: KListView(parent),
	  mFind(0),
	  mLastColumn(-1),
	  mLastColumnHeaderWidth(0)
{
	setAllColumnsShowFocus(true);
	setShowSortIndicator(true);
}

void EventListViewBase::addLastColumn(const QString& title)
{
	addColumn(title);
	mLastColumn = columns() - 1;
	mLastColumnHeaderWidth = columnWidth(mLastColumn);
	setColumnWidthMode(mLastColumn, Q3ListView::Maximum);
}

/******************************************************************************
*  Refresh the list by clearing it and redisplaying all the current alarms.
*/
void EventListViewBase::refresh()
{
	QString currentID;
	if (currentItem())
		currentID = currentItem()->event().id();    // save current item for restoration afterwards
	clear();
	populate();
	resizeLastColumn();
	EventListViewItemBase* current = getEntry(currentID);
	if (current)
	{
		setCurrentItem(current);
		ensureItemVisible(current);
	}
}

/******************************************************************************
*  Get the item for a given event ID.
*/
EventListViewItemBase* EventListViewBase::getEntry(const QString& eventID) const
{
	if (!eventID.isEmpty())
	{
		for (EventListViewItemBase* item = firstChild();  item;  item = item->nextSibling())
			if (item->event().id() == eventID)
				return item;
	}
	return 0;
}

/******************************************************************************
*  Add an event to every list instance.
*  If 'selectionView' is non-null, the selection highlight is moved to the new
*  event in that listView instance.
*/
void EventListViewBase::addEvent(const KAEvent& event, const InstanceList& instanceList, EventListViewBase* selectionView)
{
	for (int i = 0, end = instanceList.count();  i < end;  ++i)
		instanceList[i]->addEntry(event, true, (instanceList[i] == selectionView));
}

/******************************************************************************
*  Modify an event in every list instance.
*  If 'selectionView' is non-null, the selection highlight is moved to the
*  modified event in that listView instance.
*/
void EventListViewBase::modifyEvent(const QString& oldEventID, const KAEvent& newEvent,
                                    const InstanceList& instanceList, EventListViewBase* selectionView)
{
	for (int i = 0, end = instanceList.count();  i < end;  ++i)
	{
		EventListViewBase* v = instanceList[i];
		EventListViewItemBase* item = v->getEntry(oldEventID);
		if (item)
			v->deleteEntry(item, false);
		v->addEntry(newEvent, true, (v == selectionView));
	}
}

/******************************************************************************
*  Delete an event from every displayed list.
*/
void EventListViewBase::deleteEvent(const QString& eventID, const InstanceList& instanceList)
{
	for (int i = 0, end = instanceList.count();  i < end;  ++i)
	{
		EventListViewBase* v = instanceList[i];
		EventListViewItemBase* item = v->getEntry(eventID);
		if (item)
			v->deleteEntry(item, true);
		else
			v->refresh();
	}
}

/******************************************************************************
*  Add a new item to the list.
*  If 'reselect' is true, select/highlight the new item.
*/
EventListViewItemBase* EventListViewBase::addEntry(const KAEvent& event, bool setSize, bool reselect)
{
	if (!shouldShowEvent(event))
		return 0;
	return addEntry(createItem(event), setSize, reselect);
}

EventListViewItemBase* EventListViewBase::addEntry(EventListViewItemBase* item, bool setSize, bool reselect)
{
	if (setSize)
		resizeLastColumn();
	if (reselect)
	{
		clearSelection();
		setSelected(item, true);
	}
	return item;
}

/******************************************************************************
*  Update a specified item in the list.
*  If 'reselect' is true, select the updated item.
*/
EventListViewItemBase* EventListViewBase::updateEntry(EventListViewItemBase* item, const KAEvent& newEvent, bool setSize, bool reselect)
{
	deleteEntry(item);
	return addEntry(newEvent, setSize, reselect);
}

/******************************************************************************
*  Delete a specified item from the list.
*/
void EventListViewBase::deleteEntry(EventListViewItemBase* item, bool setSize)
{
	if (item)
	{
		delete item;
		if (setSize)
			resizeLastColumn();
		emit itemDeleted();
	}
}

/******************************************************************************
*  Called when the Find action is selected.
*  Display the non-modal Find dialog.
*/
void EventListViewBase::slotFind()
{
	if (!mFind)
	{
		mFind = new Find(this);
		connect(mFind, SIGNAL(active(bool)), SIGNAL(findActive(bool)));
	}
	mFind->display();
}

/******************************************************************************
*  Called when the Find Next or Find Prev action is selected.
*/
void EventListViewBase::findNext(bool forward)
{
	if (mFind)
		mFind->findNext(forward);
}

/******************************************************************************
*  Check whether there are any selected items.
*/
bool EventListViewBase::anySelected() const
{
	for (Q3ListViewItem* item = KListView::firstChild();  item;  item = item->nextSibling())
		if (isSelected(item))
			return true;
	return false;
}

/******************************************************************************
*  Get the single selected event.
*  Reply = the event
*        = 0 if no event is selected or multiple events are selected.
*/
const KAEvent* EventListViewBase::selectedEvent() const
{
	EventListViewItemBase* sel = selectedItem();
	return sel ? &sel->event() : 0;
}

/******************************************************************************
*  Fetch the single selected item.
*  This method works in both Single and Multi selection mode, unlike
*  QListView::selectedItem().
*  Reply = null if no items are selected, or if multiple items are selected.
*/
EventListViewItemBase* EventListViewBase::selectedItem() const
{
	if (selectionMode() == Q3ListView::Single)
		return (EventListViewItemBase*)KListView::selectedItem();

	Q3ListViewItem* item = 0;
	for (Q3ListViewItem* it = firstChild();  it;  it = it->nextSibling())
	{
		if (isSelected(it))
		{
			if (item)
				return 0;
			item = it;
		}
	}
	return (EventListViewItemBase*)item;
}

/******************************************************************************
*  Fetch all selected items.
*/
QList<EventListViewItemBase*> EventListViewBase::selectedItems() const
{
	QList<EventListViewItemBase*> items;
	for (Q3ListViewItem* item = firstChild();  item;  item = item->nextSibling())
	{
		if (isSelected(item))
			items.append((EventListViewItemBase*)item);
	}
	return items;
}

/******************************************************************************
*  Return how many items are selected.
*/
int EventListViewBase::selectedCount() const
{
	int count = 0;
	for (Q3ListViewItem* item = firstChild();  item;  item = item->nextSibling())
	{
		if (isSelected(item))
			++count;
	}
	return count;
}

/******************************************************************************
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void EventListViewBase::resizeLastColumn()
{
	int lastColumnWidth = mLastColumnHeaderWidth;
	for (EventListViewItemBase* item = firstChild();  item;  item = item->nextSibling())
	{
		int mw = item->lastColumnWidth();
		if (mw > lastColumnWidth)
			lastColumnWidth = mw;
	}
	int x = header()->sectionPos(mLastColumn);
	int width = visibleWidth() - x;
	if (width < lastColumnWidth)
		width = lastColumnWidth;
	setColumnWidth(mLastColumn, width);
	if (contentsWidth() > x + width)
		resizeContents(x + width, contentsHeight());
}

/******************************************************************************
*  Called when the widget's size has changed (before it is painted).
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void EventListViewBase::resizeEvent(QResizeEvent* re)
{
	KListView::resizeEvent(re);
	resizeLastColumn();
}

/******************************************************************************
*  Called when the widget is first displayed.
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void EventListViewBase::showEvent(QShowEvent* se)
{
	KListView::showEvent(se);
	resizeLastColumn();
}

/******************************************************************************
*  Called when any event occurs.
*  Displays the WhatsThis text for the chosen column.
*/
bool EventListViewBase::event(QEvent *e)
{
	if (e->type() == QEvent::WhatsThis)
	{
		QHelpEvent* he = (QHelpEvent*)e;
		QPoint pt = he->pos();
		int column = -1;
		QPoint viewportPt = viewport()->mapFrom(this, pt);
		QRect frame = header()->frameGeometry();
		if (frame.contains(pt)
		||  itemAt(QPoint(itemMargin(), viewportPt.y())) && frame.contains(QPoint(pt.x(), frame.y())))
			column = header()->sectionAt(pt.x());
		QWhatsThis::showText(pt, whatsThisText(column));
		return true;
	}
	return KListView::event(e);
}

/******************************************************************************
*  Find the height of one list item.
*/
int EventListViewBase::itemHeight()
{
	EventListViewItemBase* item = firstChild();
	if (!item)
	{
		// The list is empty, so create a temporary item to find its height
		Q3ListViewItem* item = new Q3ListViewItem(this, QString::null);
		int height = item->height();
		delete item;
		return height;
	}
	else
		return item->height();
}


/*=============================================================================
=  Class: EventListViewItemBase
=  Base class containing the details of one event for display in an
*  EventListViewBase.
=============================================================================*/
QPixmap* EventListViewItemBase::mTextIcon;
QPixmap* EventListViewItemBase::mFileIcon;
QPixmap* EventListViewItemBase::mCommandIcon;
QPixmap* EventListViewItemBase::mEmailIcon;
int      EventListViewItemBase::mIconWidth = 0;


EventListViewItemBase::EventListViewItemBase(EventListViewBase* parent, const KAEvent& event)
	: Q3ListViewItem(parent),
	  mEvent(event)
{
	iconWidth();    // load the icons
}

/******************************************************************************
*  Set the text for the last column, and find its width.
*/
void EventListViewItemBase::setLastColumnText()
{
	EventListViewBase* parent = (EventListViewBase*)listView();
	setText(parent->lastColumn(), lastColumnText());
	mLastColumnWidth = width(parent->fontMetrics(), parent, parent->lastColumn());
}

/******************************************************************************
*  Return the width of the widest alarm type icon.
*/
int EventListViewItemBase::iconWidth()
{
	if (!mIconWidth)
	{
		mTextIcon    = new QPixmap(SmallIcon("message"));
		mFileIcon    = new QPixmap(SmallIcon("file"));
		mCommandIcon = new QPixmap(SmallIcon("exec"));
		mEmailIcon   = new QPixmap(SmallIcon("mail_generic"));
		if (mTextIcon)
			mIconWidth = mTextIcon->width();
		if (mFileIcon  &&  mFileIcon->width() > mIconWidth)
			mIconWidth = mFileIcon->width();
		if (mCommandIcon  &&  mCommandIcon->width() > mIconWidth)
			mIconWidth = mCommandIcon->width();
		if (mEmailIcon  &&  mEmailIcon->width() > mIconWidth)
			mIconWidth = mEmailIcon->width();
	}
	return mIconWidth;
}

/******************************************************************************
*  Return the icon associated with the event's action.
*/
QPixmap* EventListViewItemBase::eventIcon() const
{
	switch (mEvent.action())
	{
		case KAAlarm::FILE:     return mFileIcon;
		case KAAlarm::COMMAND:  return mCommandIcon;
		case KAAlarm::EMAIL:    return mEmailIcon;
		case KAAlarm::MESSAGE:
		default:                return mTextIcon;
	}
}

