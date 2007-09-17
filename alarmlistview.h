/*
 *  alarmlistview.h  -  widget showing list of alarms
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

#ifndef ALARMLISTVIEW_H
#define ALARMLISTVIEW_H

#include "kalarm.h"

#include <QByteArray>
#include <kcal/event.h>

#include "eventlistview.h"


class AlarmListView : public EventListView
{
		Q_OBJECT
	public:
		explicit AlarmListView(const QByteArray& configGroup, QWidget* parent = 0);
		virtual void      setModel(QAbstractItemModel*);
		void              selectTimeColumns(bool time, bool timeTo);

//	protected slots:
//		virtual void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
	private slots:
		void              sectionMoved();

	private:
		QByteArray        mConfigGroup;
};

#endif // ALARMLISTVIEW_H

