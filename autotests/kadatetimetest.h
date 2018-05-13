/*
   This file is part of kalarmcal library, which provides access to KAlarm
   calendar data.

   Copyright (c) 2005,2011,2018 David Jarvie <djarvie@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KADATETIMETEST_H
#define KADATETIMETEST_H

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

#endif
