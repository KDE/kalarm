/*
 *  alarmlistview.h  -  widget showing list of alarms
 *  Program:  kalarm
 *  Copyright © 2007-2020 David Jarvie <djarvie@kde.org>
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

#include "eventlistview.h"

#include <QByteArray>


class AlarmListView : public EventListView
{
    Q_OBJECT
public:
    explicit AlarmListView(const QByteArray& configGroup, QWidget* parent = nullptr);
    QList<bool> columnsVisible() const;
    void        setColumnsVisible(const QList<bool>& show);

Q_SIGNALS:
    void        columnsVisibleChanged();

protected Q_SLOTS:
    void        initSections() override;

private Q_SLOTS:
    void        saveColumnsState();
    void        headerContextMenuRequested(const QPoint&);

private:
    void        showHideColumn(QMenu&, QAction*);
    void        enableTimeColumns(QMenu*);

    QByteArray  mConfigGroup;
};

#endif // ALARMLISTVIEW_H

// vim: et sw=4:
