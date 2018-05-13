/*
 *  alarmtime.h  -  conversion functions for alarm times
 *  Program:  kalarm
 *  Copyright © 2012,2018 by David Jarvie <djarvie@kde.org>
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

#include <kalarmcal/kadatetime.h>

namespace KAlarmCal { class DateTime; }

using namespace KAlarmCal;

class AlarmTime
{
  public:
    static QString alarmTimeText(const DateTime& dateTime);
    static QString timeToAlarmText(const DateTime& dateTime);
    static bool convertTimeString(const QByteArray& timeString, KADateTime& dateTime,
                                  const KADateTime& defaultDt = KADateTime(), bool allowTZ = true);
    static KADateTime applyTimeZone(const QString& tzstring, const QDate& date, const QTime& time,
                                   bool haveTime, const KADateTime& defaultDt = KADateTime());

  private:
    static int mTimeHourPos;
};

// vim: et sw=4:
