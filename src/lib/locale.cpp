/*
 *  locale.cpp  -  miscellaneous locale functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2003-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "locale.h"

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
        for (Qt::DayOfWeek weekDay : std::as_const(weekDays))
            workDays |= 1 << (weekDay - 1);

        std::sort(weekDays.begin(), weekDays.end());
        int day = 0;
        for (Qt::DayOfWeek weekDay : std::as_const(weekDays))
            if (++day < weekDay)
            {
                lastWorkDay = (day == 1) ? weekDays.at(weekDays.size() - 1) : day - 1;
                firstWorkDay = weekDay;
                break;
            }
    }
    return firstDay;
}

}

namespace Locale
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

} // namespace Locale

// vim: et sw=4:
