/*
 *  localefuncs.cpp  -  miscellaneous locale functions
 *  Program:  kalarm
 *  Copyright © 2003-2009 by David Jarvie <djarvie@kde.org>
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
#include "localefuncs.h"

#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>


namespace KAlarm
{

/******************************************************************************
*  Return the first day of the week for the user's locale.
*  Reply = 1 (Mon) .. 7 (Sun).
*/
int localeFirstDayOfWeek()
{
	static int firstDay = 0;
	if (!firstDay)
		firstDay = KGlobal::locale()->weekStartDay();
	return firstDay;
}

/******************************************************************************
* Return the week day name (Monday = 1).
*/
QString weekDayName(int day, const KLocale* locale)
{
	switch (day)
	{
		case 1: return ki18nc("@option Name of the weekday", "Monday").toString(locale);
		case 2: return ki18nc("@option Name of the weekday", "Tuesday").toString(locale);
		case 3: return ki18nc("@option Name of the weekday", "Wednesday").toString(locale);
		case 4: return ki18nc("@option Name of the weekday", "Thursday").toString(locale);
		case 5: return ki18nc("@option Name of the weekday", "Friday").toString(locale);
		case 6: return ki18nc("@option Name of the weekday", "Saturday").toString(locale);
		case 7: return ki18nc("@option Name of the weekday", "Sunday").toString(locale);
	}
	return QString();
}

} // namespace KAlarm
