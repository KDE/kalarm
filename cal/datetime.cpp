/*
 *  datetime.cpp  -  date/time with start-of-day time for date-only values 
 *  Program:  kalarm
 *  Copyright © 2003,2005-2007,2009,2010 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */
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

bool operator<(const DateTime& dt1, const DateTime& dt2)
{
    if (dt1.isDateOnly()  &&  !dt2.isDateOnly())
    {
        KDateTime dt = dt1.mDateTime.addDays(1);
        dt.setTime(DateTime::mStartOfDay);
        return dt <= dt2.mDateTime;
    }
    if (!dt1.isDateOnly()  &&  dt2.isDateOnly())
    {
        KDateTime dt = dt2.mDateTime;
        dt.setTime(DateTime::mStartOfDay);
        return dt1.mDateTime < dt;
    }
    return dt1.mDateTime < dt2.mDateTime;
}

// vim: et sw=4:
