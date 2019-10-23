/*
 *  alarmlistdelegate.h  -  handles editing and display of alarm list
 *  Program:  kalarm
 *  Copyright © 2007,2008 David Jarvie <djarvie@kde.org>
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

#include "alarmlistview.h"


class AlarmListDelegate : public EventListDelegate
{
        Q_OBJECT
    public:
        explicit AlarmListDelegate(AlarmListView* parent = nullptr)
                   : EventListDelegate(parent) {}
        void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
        QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;
        void edit(KAEvent*, EventListView*) override;
};

#endif

// vim: et sw=4:
