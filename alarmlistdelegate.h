/*
 *  alarmlistdelegate.h  -  handles editing and display of alarm list
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

#ifndef ALARMLISTDELEGATE_H
#define ALARMLISTDELEGATE_H

#include "kalarm.h"

#include <QItemDelegate>

class AlarmListView;


class AlarmListDelegate : public QItemDelegate
{
		Q_OBJECT
	public:
		explicit AlarmListDelegate(AlarmListView* parent = 0);
		virtual QWidget* createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const  { return 0; }
		virtual void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const;
		virtual QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const;
		virtual bool editorEvent(QEvent*, QAbstractItemModel*, const QStyleOptionViewItem&, const QModelIndex&);
};

#endif

