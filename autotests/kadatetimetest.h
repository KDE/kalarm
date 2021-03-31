/*
   This file is part of kalarmcal library, which provides access to KAlarm
   calendar data.

   SPDX-FileCopyrightText: 2005, 2011, 2018 David Jarvie <djarvie@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QObject>

class KADateTimeTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void specConstructors();
    void specSet();
    void constructors();
    void toUtc();
    void toOffsetFromUtc();
    void toLocalZone();
    void toZone();
    void toTimeSpec();
    void set();
    void equal();
    void lessThan();
    void compare();
    void addSubtract();
    void addMSecs();
    void addSubtractDate();
    void dstShifts();
    void strings_iso8601();
    void strings_rfc2822();
    void strings_rfc3339();
    void strings_qttextdate();
    void strings_format();
#ifdef COMPILING_TESTS
    void cache();
#endif
    void stream();
    void misc();
};

