/*
 *  eventlistview.cpp  -  base class for widget showing list of alarms
 *  Program:  kalarm
 *  Copyright © 2007-2009 by David Jarvie <djarvie@kde.org>
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

#include <QHeaderView>
#include <QMouseEvent>
#include <QToolTip>
#include <QApplication>

#include <kdebug.h>

#include "eventlistmodel.h"
#include "find.h"
#include "templatelistfiltermodel.h"
#include "eventlistview.moc"


EventListView::EventListView(QWidget* parent)
	: QTreeView(parent),
	  mFind(0)
{
	setRootIsDecorated(false);    // don't show expander icons for child-less items
	setSortingEnabled(true);
	setAllColumnsShowFocus(true);
	setSelectionMode(ExtendedSelection);
	setSelectionBehavior(SelectRows);
	setTextElideMode(Qt::ElideRight);
	// Set default WhatsThis text to be displayed when no actual item is clicked on
	setWhatsThis(i18nc("@info:whatsthis", "List of scheduled alarms"));
}

/******************************************************************************
* Return the event referred to by an index.
*/
KAEvent* EventListView::event(const QModelIndex& index) const
{
	return eventFilterModel()->event(index);
}

KAEvent* EventListView::event(int row) const
{
	return eventFilterModel()->event(row);
}

/******************************************************************************
* Select one event and make it the current item.
*/
void EventListView::select(const QString& eventId)
{
	select(eventModel()->eventIndex(eventId));
}

void EventListView::select(const QModelIndex& index)
{
	selectionModel()->select(index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
}

/******************************************************************************
* Return the single selected item.
* Reply = invalid if no items are selected, or if multiple items are selected.
*/
QModelIndex EventListView::selectedIndex() const
{
	QModelIndexList list = selectionModel()->selectedRows();
	if (list.count() != 1)
		return QModelIndex();
	return list[0];
}

/******************************************************************************
* Return the single selected event.
* Reply = null if no items are selected, or if multiple items are selected.
*/
KAEvent* EventListView::selectedEvent() const
{
	QModelIndexList list = selectionModel()->selectedRows();
	if (list.count() != 1)
		return 0;
kDebug(0)<<"SelectedEvent() count="<<list.count();
	const QAbstractProxyModel* proxy = static_cast<const QAbstractProxyModel*>(list[0].model());
	QModelIndex source = proxy->mapToSource(list[0]);
	return static_cast<KAEvent*>(source.internalPointer());
}

/******************************************************************************
* Return the selected events.
*/
KAEvent::List EventListView::selectedEvents() const
{
	KAEvent::List elist;
	QModelIndexList ilist = selectionModel()->selectedRows();
	int count = ilist.count();
	if (count)
	{
		const QAbstractProxyModel* proxy = static_cast<const QAbstractProxyModel*>(ilist[0].model());
		for (int i = 0;  i < count;  ++i)
		{
			QModelIndex source = proxy->mapToSource(ilist[i]);
			elist += static_cast<KAEvent*>(source.internalPointer());
		}
	}
	return elist;
}

/******************************************************************************
*  Called when the Find action is selected.
*  Display the non-modal Find dialog.
*/
void EventListView::slotFind()
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
void EventListView::findNext(bool forward)
{
	if (mFind)
		mFind->findNext(forward);
}

/******************************************************************************
 * * Called when a ToolTip or WhatsThis event occurs.
 * */
bool EventListView::viewportEvent(QEvent* e)
{
	if (e->type() == QEvent::ToolTip  &&  isActiveWindow())
	{
		QHelpEvent* he = static_cast<QHelpEvent*>(e);
		QModelIndex index = indexAt(he->pos());
		QVariant value = model()->data(index, Qt::ToolTipRole);
		if (qVariantCanConvert<QString>(value))
		{
			QString toolTip = value.toString();
			int i = toolTip.indexOf('\n');
			if (i < 0)
			{
				EventListFilterModel* m = qobject_cast<EventListFilterModel*>(model());
				if (!m  ||  m->event(index)->commandError() == KAEvent::CMD_NO_ERROR)
				{
					// Single line tooltip. Only display it if the text column
					// is truncated in the view display.
					value = model()->data(index, Qt::FontRole);
					QFontMetrics fm(qvariant_cast<QFont>(value).resolve(viewOptions().font));
					int textWidth = fm.boundingRect(toolTip).width() + 1;
					const int margin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
					QRect rect = visualRect(index);
					int left = columnViewportPosition(index.column()) + margin;
					int right = left + textWidth;
					if (left >= 0  &&  right <= width() - 2*frameWidth())
						toolTip.clear();    // prevent any tooltip showing
				}
			}
			QToolTip::showText(he->globalPos(), toolTip, this);
			return true;
		}
	}
	return QTreeView::viewportEvent(e);
}

/******************************************************************************
* Called when a context menu event is requested by mouse or key.
*/
void EventListView::contextMenuEvent(QContextMenuEvent* e)
{
	emit contextMenuRequested(e->globalPos());
}


bool EventListDelegate::editorEvent(QEvent* e, QAbstractItemModel* model, const QStyleOptionViewItem&, const QModelIndex& index)
{
	// Don't invoke the editor unless it's either a double click or,
	// if KDE is in single click mode and it's a left button release
	// with no other buttons pressed and no keyboard modifiers.
	switch (e->type())
	{
		case QEvent::MouseButtonPress:
		case QEvent::MouseMove:
			return false;
		case QEvent::MouseButtonDblClick:
			break;
		case QEvent::MouseButtonRelease:
		{
			if (!static_cast<EventListView*>(parent())->editOnSingleClick()
			||  !KGlobalSettings::singleClick())
				return false;
			QMouseEvent* me = static_cast<QMouseEvent*>(e);
			if (me->button() != Qt::LeftButton  ||  me->buttons()
			||  me->modifiers() != Qt::NoModifier)
				return false;
			break;
		}
		default:
			break;
	}
	if (index.isValid())
	{
		kDebug();
		QModelIndex source = static_cast<QAbstractProxyModel*>(model)->mapToSource(index);
		KAEvent* event = static_cast<KAEvent*>(source.internalPointer());
		edit(event, static_cast<EventListView*>(parent()));
		return true;
	}
	return false;   // indicate that the event has not been handled
}	
