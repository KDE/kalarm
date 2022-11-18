/*
   This file is part of kalarmcal library, which provides access to KAlarm
   calendar data.

   SPDX-FileCopyrightText: 2018-2022 David Jarvie <djarvie@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QObject>

class KAEventTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void constructors();
    void flags();
    void fromKCalEvent();
    void toKCalEvent();
    void setNextOccurrence();
};

