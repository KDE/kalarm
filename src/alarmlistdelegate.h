/*
 *  alarmlistdelegate.h  -  handles editing and display of alarm list
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2007-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
    void edit(KAEvent&, EventListView*) override;
};

#endif

// vim: et sw=4:
