/*
 *  eventlistview.h  -  base class for widget showing list of alarms
 *  Program:  kalarm
 *  Copyright © 2007 by David Jarvie <software@astrojar.org.uk>
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

#include <kcal/event.h>

#include "eventlistmodel.h"


class EventListView : public QTreeView
{
		Q_OBJECT
	public:
		explicit EventListView(QWidget* parent = 0);
		EventListFilterModel* eventFilterModel() const   { return static_cast<EventListFilterModel*>(model()); }
		EventListModel*   eventModel() const   { return static_cast<EventListModel*>(static_cast<QAbstractProxyModel*>(model())->sourceModel()); }
		KCal::Event*      event(int row) const;
		KCal::Event*      event(const QModelIndex&) const;
		void              select(const QString& eventId);
		void              select(const QModelIndex&);
		QModelIndex       selectedIndex() const;
		KCal::Event*      selectedEvent() const;
		KCal::Event::List selectedEvents() const;

	signals:
		void  rightButtonClicked(const QPoint& globalPos);

	protected:
		virtual void mouseReleaseEvent(QMouseEvent*);
};

#endif // EVENTLISTVIEW_H

