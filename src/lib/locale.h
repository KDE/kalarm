/*
 *  locale.h  -  miscellaneous locale functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LIB_LOCALE_H
#define LIB_LOCALE_H

namespace Locale
{

/**
* Given a standard KDE day number, return the day number in the week for the
* user's locale.
* @param weekDay  Standard day number = 1 (Mon) .. 7 (Sun)
* @return  Locale day number in week = 0 .. 6
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
