/*
 *  datetime.cpp  -  date/time with start-of-day time for date-only values 
 *  Program:  kalarm
 *  Copyright © 2003,2005-2007,2009 by David Jarvie <djarvie@kde.org>
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
#include "datetime.h"

#include <kglobal.h>
#include <klocale.h>


QTime DateTime::mStartOfDay;

QTime DateTime::effectiveTime() const
{
	return mDateTime.isDateOnly() ? mStartOfDay : mDateTime.time();
}

QDateTime DateTime::effectiveDateTime() const
{
	if (mDateTime.isDateOnly())
	{
		QDateTime dt = mDateTime.dateTime();    // preserve Qt::UTC or Qt::LocalTime
		dt.setTime(mStartOfDay);
		return dt;
	}
	return mDateTime.dateTime();
}

KDateTime DateTime::effectiveKDateTime() const
{
	if (mDateTime.isDateOnly())
	{
		KDateTime dt = mDateTime;
		dt.setTime(mStartOfDay);
		return dt;
	}
	return mDateTime;
}

KDateTime DateTime::calendarKDateTime() const
{
	if (mDateTime.isDateOnly())
	{
		KDateTime dt = mDateTime;
		dt.setTime(QTime(0, 0));
		return dt;
	}
	return mDateTime;
}

QString DateTime::formatLocale(bool shortFormat) const
{
	return KGlobal::locale()->formatDateTime(mDateTime, (shortFormat ? KLocale::ShortDate : KLocale::LongDate));
}
