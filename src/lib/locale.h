/*
 *  locale.h  -  miscellaneous locale functions
 *  Program:  kalarm
 *  Copyright © 2004-2019 David Jarvie <djarvie@kde.org>
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

#ifndef LIB_LOCALE_H
#define LIB_LOCALE_H

namespace Locale
{

/**
* Given a standard KDE day number, return the day number in the week for the
* user's locale.
* @param weekDay  Locale day number in week = 0 .. 6
* @return  Standard day number = 1 (Mon) .. 7 (Sun)
*/
int weekDay_to_localeDayInWeek(int weekDay);

/**
* Given a day number in the week for the user's locale, return the standard KDE
* day number.
* @param index  0 .. 6
* @return  Standard day number = 1 (Mon) .. 7 (Sun)
*/
int localeDayInWeek_to_weekDay(int index);

/**
* Return the default work days in the week, as a bit mask.
* @return  Bit 1 set for Monday ... bit 0x40 for Sunday.
*/
unsigned defaultWorkDays();

} // namespace Locale

#endif // LIB_LOCALE_H

// vim: et sw=4:
