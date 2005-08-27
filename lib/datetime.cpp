/*
 *  datetime.cpp  -  date/time representation with optional date-only value
 *  Program:  kalarm
 *  Copyright (C) 2003, 2005 by David Jarvie <software@astrojar.org.uk>
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
#include "kalarm.h"

#include <kglobal.h>
#include <klocale.h>

#include "datetime.h"

QTime DateTime::mStartOfDay;

QTime DateTime::time() const
{
	return mDateOnly ? mStartOfDay : mDateTime.time();
}

QDateTime DateTime::dateTime() const
{
	return mDateOnly ? QDateTime(mDateTime.date(), mStartOfDay) : mDateTime;
}

QString DateTime::formatLocale(bool shortFormat) const
{
	if (mDateOnly)
		return KGlobal::locale()->formatDate(mDateTime.date(), shortFormat);
	else if (mTimeValid)
		return KGlobal::locale()->formatDateTime(mDateTime, shortFormat);
	else
		return QString::null;
}

bool operator==(const DateTime& dt1, const DateTime& dt2)
{
	if (dt1.mDateTime.date() != dt2.mDateTime.date())
		return false;
	if (dt1.mDateOnly && dt2.mDateOnly)
		return true;
	if (!dt1.mDateOnly && !dt2.mDateOnly)
	{
		bool valid1 = dt1.mTimeValid && dt1.mDateTime.time().isValid();
		bool valid2 = dt2.mTimeValid && dt2.mDateTime.time().isValid();
		if (!valid1  &&  !valid2)
			return true;
		if (!valid1  ||  !valid2)
			return false;
		return dt1.mDateTime.time() == dt2.mDateTime.time();
	}
	return (dt1.mDateOnly ? dt2.mDateTime.time() : dt1.mDateTime.time()) == DateTime::startOfDay();
}

bool operator<(const DateTime& dt1, const DateTime& dt2)
{
	if (dt1.mDateTime.date() != dt2.mDateTime.date())
		return dt1.mDateTime.date() < dt2.mDateTime.date();
	if (dt1.mDateOnly && dt2.mDateOnly)
		return false;
	if (!dt1.mDateOnly && !dt2.mDateOnly)
		return dt1.mDateTime.time() < dt2.mDateTime.time();
	QTime t = DateTime::startOfDay();
	if (dt1.mDateOnly)
		return t < dt2.mDateTime.time();
	return dt1.mDateTime.time() < t;
}
