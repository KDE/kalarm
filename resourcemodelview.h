/*
 *  resourcemodelview.h  -  model/view classes for alarm resource lists
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

#ifndef RESOURCEMODELVIEW_H
#define RESOURCEMODELVIEW_H

#include "kalarm.h"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QItemDelegate>
#include <QListView>
#include <QList>
#include <QFont>
#include <QColor>

#include "resources/alarmresource.h"
#include "resources/alarmresources.h"


class ResourceModel : public QAbstractListModel
{
		Q_OBJECT
	public:
		static ResourceModel* instance(QObject* parent = 0);
		virtual int      rowCount(const QModelIndex& parent = QModelIndex()) const;
		virtual QModelIndex index(int row, int column, const QModelIndex& parent) const;
		virtual QVariant data(const QModelIndex&, int role = Qt::DisplayRole) const;
		virtual bool     setData(const QModelIndex&, const QVariant& value, int role = Qt::EditRole);
		virtual Qt::ItemFlags flags(const QModelIndex&) const;
		AlarmResource*   resource(const QModelIndex&) const;
		void             notifyChange(const QModelIndex&);

	private slots:
		void             refresh();
		void             addResource(AlarmResource*);
		void             updateResource(AlarmResource*);
		void             slotStandardChanged(AlarmResource::Type);
		void             slotStatusChanged(AlarmResource*, AlarmResources::Change);

	private:
		explicit ResourceModel(QObject* parent = 0);

		static ResourceModel* mInstance;
		QList<AlarmResource*> mResources;
		QString               mErrorPrompt;
		QFont                 mFont;
};


class ResourceFilterModel : public QSortFilterProxyModel
{
	public:
		ResourceFilterModel(QAbstractItemModel* baseModel, QObject* parent);
		void           setFilter(AlarmResource::Type);
		AlarmResource* resource(int row) const;
		AlarmResource* resource(const QModelIndex&) const;
		void           notifyChange(int row);
		void           notifyChange(const QModelIndex&);

	protected:
		virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;

	private:
		AlarmResource::Type mResourceType;
};


class ResourceView : public QListView
{
	public:
		ResourceView(QWidget* parent = 0)  : QListView(parent) {}
		virtual void   setModel(QAbstractItemModel*);
		AlarmResource* resource(int row) const;
		AlarmResource* resource(const QModelIndex&) const;
		void           notifyChange(int row) const;
		void           notifyChange(const QModelIndex&) const;

	protected:
		virtual void   mouseReleaseEvent(QMouseEvent*);
		virtual bool   viewportEvent(QEvent*);
};


class ResourceDelegate : public QItemDelegate
{
	public:
		ResourceDelegate(ResourceView* parent = 0)  : QItemDelegate(parent) {}
		virtual bool editorEvent(QEvent*, QAbstractItemModel*, const QStyleOptionViewItem&, const QModelIndex&);
};

#endif // RESOURCEMODELVIEW_H

