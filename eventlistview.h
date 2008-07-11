/*
 *  eventlistview.h  -  base class for widget showing list of alarms
 *  Program:  kalarm
 *  Copyright © 2007,2008 by David Jarvie <djarvie@kde.org>
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

#ifndef EVENTLISTVIEW_H
#define EVENTLISTVIEW_H

#include "kalarm.h"

#include <QTreeView>
#include <QItemDelegate>

#include <kcal/event.h>

#include "eventlistmodel.h"

class Find;


class EventListView : public QTreeView
{
		Q_OBJECT
	public:
		explicit EventListView(QWidget* parent = 0);
		EventListFilterModel* eventFilterModel() const   { return static_cast<EventListFilterModel*>(model()); }
		EventListModel*   eventModel() const   { return static_cast<EventListModel*>(static_cast<QAbstractProxyModel*>(model())->sourceModel()); }
		KAEvent*          event(int row) const;
		KAEvent*          event(const QModelIndex&) const;
		void              select(const QString& eventId);
		void              select(const QModelIndex&);
		QModelIndex       selectedIndex() const;
		KAEvent*          selectedEvent() const;
		KAEvent::List     selectedEvents() const;

	public slots:
		virtual void      slotFind();
		virtual void      slotFindNext()       { findNext(true); }
		virtual void      slotFindPrev()       { findNext(false); }

	signals:
		void              contextMenuRequested(const QPoint& globalPos);
		void              findActive(bool);

	protected:
		virtual bool      viewportEvent(QEvent*);
		virtual void      contextMenuEvent(QContextMenuEvent*);

	private:
		void              findNext(bool forward);

		Find*             mFind;

		using QObject::event;   // prevent "hidden" warning
};

class EventListDelegate : public QItemDelegate
{
		Q_OBJECT
	public:
		explicit EventListDelegate(EventListView* parent = 0) : QItemDelegate(parent) {}
		virtual QWidget* createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const  { return 0; }
		virtual bool editorEvent(QEvent*, QAbstractItemModel*, const QStyleOptionViewItem&, const QModelIndex&);
		virtual void edit(KAEvent*, EventListView*) = 0;
};

#endif // EVENTLISTVIEW_H

