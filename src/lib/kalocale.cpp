/*
 *  kalocale.cpp  -  miscellaneous locale functions
 *  Program:  kalarm
 *  Copyright © 2003-2018 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"   //krazy:exclude=includes (kalarm.h must be first)
#include "kalocale.h"

#include "kalarm_debug.h"
#include <QLocale>

namespace
{
int firstDay     = 0;
int firstWorkDay = 0;
int lastWorkDay  = 0;
uint workDays    = 0;

/******************************************************************************
* Return the first day of the week for the user's locale.
* Reply = 1 (Mon) .. 7 (Sun).
*/
int localeFirstDayOfWeek()
{
    static QLocale locale;
    if (!firstDay || locale != QLocale())
    {
        Q_ASSERT(Qt::Monday == 1 && Qt::Sunday == 7);  // all weekday numbering assumes this
        locale = QLocale();
        firstDay = locale.firstDayOfWeek();
        QList<Qt::DayOfWeek> weekDays = locale.weekdays();
        for (int i = 0; i < weekDays.size(); ++i)
            workDays |= 1 << (weekDays.at(i) - 1);

        qSort(weekDays);
        int day = 0;
        for (int i = 0; i < weekDays.size(); ++i)
            if (++day < weekDays.at(i))
            {
                lastWorkDay = (day == 1) ? weekDays.at(weekDays.size() - 1) : day - 1;
                firstWorkDay = weekDays.at(i);
                break;
            }
    }
    return firstDay;
}

}

namespace KAlarm
{

/*****************************************************************************/
int weekDay_to_localeDayInWeek(int weekDay)
{
    return (weekDay + 7 - localeFirstDayOfWeek()) % 7;
}

/*****************************************************************************/
int localeDayInWeek_to_weekDay(int index)
{
    return (index + localeFirstDayOfWeek() - 1) % 7 + 1;
}

/******************************************************************************
* Return the default work days in the week, as a bit mask.
* Reply = bit 1 set for Monday ... bit 0x40 for Sunday.
*/
uint defaultWorkDays()
{
    localeFirstDayOfWeek();
    return workDays;
}

} // namespace KAlarm

// vim: et sw=4:
