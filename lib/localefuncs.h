/*
 *  localefuncs.h  -  miscellaneous locale functions
 *  Program:  kalarm
 *  Copyright © 2004-2009 by David Jarvie <djarvie@kde.org>
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

#ifndef LOCALE_H
#define LOCALE_H

#include <QString>
class KLocale;

namespace KAlarm
{

/** Return the first day of the week for the user's locale.
*   Reply = 1 (Mon) .. 7 (Sun).
*/
int        localeFirstDayOfWeek();

/** Return the week day name (Monday = 1). */
QString    weekDayName(int day, const KLocale*);

/* Given a standard KDE day number, return the day number in the week for the user's locale.
 * Standard day number = 1 (Mon) .. 7 (Sun)
 * Locale day number in week = 0 .. 6
 */
inline int weekDay_to_localeDayInWeek(int weekDay)  { return (weekDay + 7 - localeFirstDayOfWeek()) % 7; }

/* Given a day number in the week for the user's locale, return the standard KDE day number.
 * 'index' = 0 .. 6
 * Standard day number = 1 (Mon) .. 7 (Sun)
 */
inline int localeDayInWeek_to_weekDay(int index)  { return (index + localeFirstDayOfWeek() - 1) % 7 + 1; }

} // namespace KAlarm

#endif // LOCALE_H
