/*
 *  alarmlistview.h  -  widget showing list of alarms
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
