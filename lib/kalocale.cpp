/*
 *  kalocale.cpp  -  miscellaneous locale functions
 *  Program:  kalarm
 *  Copyright © 2003-2014 by David Jarvie <djarvie@kde.org>
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

#include <kglobal.h>
#include <KLocalizedString>
#include "kalarm_debug.h"
#include <QLocale>

namespace KAlarm
{

/******************************************************************************
* Return the first day of the week for the user's locale.
* Reply = 1 (Mon) .. 7 (Sun).
*/
int localeFirstDayOfWeek()
{
    static int firstDay = 0;
    if (!firstDay)
    {
        Q_ASSERT(Qt::Monday == 1 && Qt::Sunday == 7);  // all weekday numbering assumes this
        firstDay = QLocale().firstDayOfWeek();
    }
    return firstDay;
}

/******************************************************************************
* Return the week day name (Monday = 1).
*/
QString weekDayName(int day, const KLocale* locale)
{
#if 0 //QT5
    switch (day)
    {
        case Qt::Monday:    return ki18nc("@option Name of the weekday", "Monday").toString(locale);
        case Qt::Tuesday:   return ki18nc("@option Name of the weekday", "Tuesday").toString(locale);
        case Qt::Wednesday: return ki18nc("@option Name of the weekday", "Wednesday").toString(locale);
        case Qt::Thursday:  return ki18nc("@option Name of the weekday", "Thursday").toString(locale);
        case Qt::Friday:    return ki18nc("@option Name of the weekday", "Friday").toString(locale);
        case Qt::Saturday:  return ki18nc("@option Name of the weekday", "Saturday").toString(locale);
        case Qt::Sunday:    return ki18nc("@option Name of the weekday", "Sunday").toString(locale);
    }
#endif
    return QString();
}

/******************************************************************************
* Return the default work days in the week, as a bit mask.
* They are determined by the start and end work days in system settings.
*/
uint defaultWorkDays()
{
    KLocale* locale = KLocale::global();
    int end = locale->workingWeekEndDay() - 1;
    uint days = 0;
    for (int day = locale->workingWeekStartDay() - 1;  ; )
    {
        days |= 1 << day;
        if (day == end)
            break;
        ++day;
        if (day >= 7)
            day = 0;
    }
    return days;
}

} // namespace KAlarm

// vim: et sw=4:
