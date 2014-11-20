/*
 *  datetime.cpp  -  date/time with start-of-day time for date-only values
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  Copyright © 2003,2005-2007,2009-2011 by David Jarvie <djarvie@kde.org>
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

#include <klocale.h>
#include <klocalizedstring.h>
#include <ktimezone.h>

#include <QDateTime>

namespace KAlarmCal
{

class DateTime::Private
{
public:
    Private() {}
    Private(const QDate &d, const KDateTime::Spec &spec) : mDateTime(d, spec) {}
    Private(const QDate &d, const QTime &t, const KDateTime::Spec &spec) : mDateTime(d, t, spec) {}
    Private(const QDateTime &dt, const KDateTime::Spec &spec) : mDateTime(dt, spec) {}
    Private(const KDateTime &dt) : mDateTime(dt) {}

    static QTime mStartOfDay;
    KDateTime    mDateTime;
};

QTime DateTime::Private::mStartOfDay;

DateTime::DateTime()
    : d(new Private)
{
}

DateTime::DateTime(const QDate &d, const KDateTime::Spec &spec)
    : d(new Private(d, spec))
{
}

DateTime::DateTime(const QDate &d, const QTime &t, const KDateTime::Spec &spec)
    : d(new Private(d, t, spec))
{
}

DateTime::DateTime(const QDateTime &dt, const KDateTime::Spec &spec)
    : d(new Private(dt, spec))
{
}

DateTime::DateTime(const KDateTime &dt)
    : d(new Private(dt))
{
}

DateTime::DateTime(const DateTime &dt)
    : d(new Private(*dt.d))
{
}

DateTime::~DateTime()
{
    delete d;
}

DateTime &DateTime::operator=(const DateTime &dt)
{
    if (&dt != this) {
        *d = *dt.d;
    }
    return *this;
}

DateTime &DateTime::operator=(const KDateTime &dt)
{
    d->mDateTime = dt;
    return *this;
}

bool DateTime::isNull() const
{
    return d->mDateTime.isNull();
}

bool DateTime::isValid() const
{
    return d->mDateTime.isValid();
}

bool DateTime::isDateOnly() const
{
    return d->mDateTime.isDateOnly();
}

void DateTime::setDateOnly(bool dateOnly)
{
    d->mDateTime.setDateOnly(dateOnly);
}

QDate DateTime::date() const
{
    return d->mDateTime.date();
}

void DateTime::setDate(const QDate &date)
{
    d->mDateTime.setDate(date);
}

QDateTime DateTime::rawDateTime() const
{
    return d->mDateTime.dateTime();
}

KDateTime DateTime::kDateTime() const
{
    return d->mDateTime;
}

QTime DateTime::effectiveTime() const
{
    return d->mDateTime.isDateOnly() ? d->mStartOfDay : d->mDateTime.time();
}

void DateTime::setTime(const QTime &t)
{
    d->mDateTime.setTime(t);
}

QDateTime DateTime::effectiveDateTime() const
{
    if (d->mDateTime.isDateOnly()) {
        QDateTime dt = d->mDateTime.dateTime();    // preserve Qt::UTC or Qt::LocalTime
        dt.setTime(d->mStartOfDay);
        return dt;
    }
    return d->mDateTime.dateTime();
}

void DateTime::setDateTime(const QDateTime &dt)
{
    d->mDateTime.setDateTime(dt);
}

KDateTime DateTime::effectiveKDateTime() const
{
    if (d->mDateTime.isDateOnly()) {
        KDateTime dt = d->mDateTime;
        dt.setTime(d->mStartOfDay);
        return dt;
    }
    return d->mDateTime;
}

KDateTime DateTime::calendarKDateTime() const
{
    if (d->mDateTime.isDateOnly()) {
        KDateTime dt = d->mDateTime;
        dt.setTime(QTime(0, 0));
        return dt;
    }
    return d->mDateTime;
}

KTimeZone DateTime::timeZone() const
{
    return d->mDateTime.timeZone();
}

KDateTime::Spec DateTime::timeSpec() const
{
    return d->mDateTime.timeSpec();
}

void DateTime::setTimeSpec(const KDateTime::Spec &spec)
{
    d->mDateTime.setTimeSpec(spec);
}

KDateTime::SpecType DateTime::timeType() const
{
    return d->mDateTime.timeType();
}

bool DateTime::isLocalZone() const
{
    return d->mDateTime.isLocalZone();
}

bool DateTime::isClockTime() const
{
    return d->mDateTime.isClockTime();
}

bool DateTime::isUtc() const
{
    return d->mDateTime.isUtc();
}

bool DateTime::isOffsetFromUtc() const
{
    return d->mDateTime.isOffsetFromUtc();
}

int DateTime::utcOffset() const
{
    return d->mDateTime.utcOffset();
}

bool DateTime::isSecondOccurrence() const
{
    return d->mDateTime.isSecondOccurrence();
}

void DateTime::setSecondOccurrence(bool second)
{
    d->mDateTime.setSecondOccurrence(second);
}

DateTime DateTime::toUtc() const
{
    return DateTime(d->mDateTime.toUtc());
}

DateTime DateTime::toOffsetFromUtc() const
{
    return DateTime(d->mDateTime.toOffsetFromUtc());
}

DateTime DateTime::toOffsetFromUtc(int utcOffset) const
{
    return DateTime(d->mDateTime.toOffsetFromUtc(utcOffset));
}

DateTime DateTime::toLocalZone() const
{
    return DateTime(d->mDateTime.toLocalZone());
}

DateTime DateTime::toClockTime() const
{
    return DateTime(d->mDateTime.toClockTime());
}

DateTime DateTime::toZone(const KTimeZone &zone) const
{
    return DateTime(d->mDateTime.toZone(zone));
}

DateTime DateTime::toTimeSpec(const KDateTime::Spec &spec) const
{
    return DateTime(d->mDateTime.toTimeSpec(spec));
}

uint DateTime::toTime_t() const
{
    return d->mDateTime.toTime_t();
}

void DateTime::setTime_t(uint secs)
{
    d->mDateTime.setTime_t(secs);
}

DateTime DateTime::addSecs(qint64 n) const
{
    return DateTime(d->mDateTime.addSecs(n));
}

DateTime DateTime::addMins(qint64 n) const
{
    return DateTime(d->mDateTime.addSecs(n * 60));
}

DateTime DateTime::addDays(int n) const
{
    return DateTime(d->mDateTime.addDays(n));
}

DateTime DateTime::addMonths(int n) const
{
    return DateTime(d->mDateTime.addMonths(n));
}

DateTime DateTime::addYears(int n) const
{
    return DateTime(d->mDateTime.addYears(n));
}

int DateTime::daysTo(const DateTime &dt) const
{
    return d->mDateTime.daysTo(dt.d->mDateTime);
}

int DateTime::minsTo(const DateTime &dt) const
{
    return d->mDateTime.secsTo(dt.d->mDateTime) / 60;
}

int DateTime::secsTo(const DateTime &dt) const
{
    return d->mDateTime.secsTo(dt.d->mDateTime);
}

qint64 DateTime::secsTo_long(const DateTime &dt) const
{
    return d->mDateTime.secsTo_long(dt.d->mDateTime);
}

QString DateTime::toString(Qt::DateFormat f) const
{
    if (d->mDateTime.isDateOnly()) {
        return d->mDateTime.date().toString(f);
    } else {
        return d->mDateTime.dateTime().toString(f);
    }
}

QString DateTime::toString(const QString &format) const
{
    if (d->mDateTime.isDateOnly()) {
        return d->mDateTime.date().toString(format);
    } else {
        return d->mDateTime.dateTime().toString(format);
    }
}

QString DateTime::formatLocale(bool shortFormat) const
{
    return KLocale::global()->formatDateTime(d->mDateTime, (shortFormat ? KLocale::ShortDate : KLocale::LongDate));
}

void DateTime::setStartOfDay(const QTime &sod)
{
    Private::mStartOfDay = sod;
}

KDateTime::Comparison DateTime::compare(const DateTime &other) const
{
    return d->mDateTime.compare(other.d->mDateTime);
}

QTime DateTime::startOfDay()
{
    return Private::mStartOfDay;
}

bool operator==(const DateTime &dt1, const DateTime &dt2)
{
    return dt1.d->mDateTime == dt2.d->mDateTime;
}

bool operator==(const KDateTime &dt1, const DateTime &dt2)
{
    return dt1 == dt2.d->mDateTime;
}

bool operator<(const DateTime &dt1, const DateTime &dt2)
{
    if (dt1.d->mDateTime.isDateOnly()  &&  !dt2.d->mDateTime.isDateOnly()) {
        KDateTime dt = dt1.d->mDateTime.addDays(1);
        dt.setTime(DateTime::Private::mStartOfDay);
        return dt <= dt2.d->mDateTime;
    }
    if (!dt1.d->mDateTime.isDateOnly()  &&  dt2.d->mDateTime.isDateOnly()) {
        KDateTime dt = dt2.d->mDateTime;
        dt.setTime(DateTime::Private::mStartOfDay);
        return dt1.d->mDateTime < dt;
    }
    return dt1.d->mDateTime < dt2.d->mDateTime;
}

} // namespace KAlarmCal
