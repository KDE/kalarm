/*
 *  datetime.cpp  -  date/time with start-of-day time for date-only values
 *  This file is part of kalarmcalendar library, which provides access to KAlarm
 *  calendar data.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2003-2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "datetime.h"

#include <QTimeZone>
#include <QLocale>

namespace KAlarmCal
{

class Q_DECL_HIDDEN DateTime::Private
{
public:
    Private() = default;
    Private(const QDate& d, const KADateTime::Spec& spec) : mDateTime(d, spec) {}
    Private(const QDate& d, const QTime& t, const KADateTime::Spec& spec)
        : mDateTime(d, t, spec) {}
    Private(const QDateTime& dt, const KADateTime::Spec& spec)
        : mDateTime(dt, spec) {}
    Private(const QDateTime& dt) : mDateTime(dt) {}
    Private(const KADateTime& dt) : mDateTime(dt) {}

    static QTime mStartOfDay;
    KADateTime mDateTime;
};

QTime DateTime::Private::mStartOfDay;

DateTime::DateTime()
    : d(new Private)
{
}

DateTime::DateTime(const QDate& d, const KADateTime::Spec& spec)
    : d(new Private(d, spec))
{
}

DateTime::DateTime(const QDate& d, const QTime& t, const KADateTime::Spec& spec)
    : d(new Private(d, t, spec))
{
}

DateTime::DateTime(const QDateTime& dt, const KADateTime::Spec& spec)
    : d(new Private(dt, spec))
{
}

DateTime::DateTime(const QDateTime& dt)
    : d(new Private(dt))
{
}

DateTime::DateTime(const KADateTime& dt)
    : d(new Private(dt))
{
}

DateTime::DateTime(const DateTime& dt)
    : d(new Private(*dt.d))
{
}

DateTime::~DateTime()
{
    delete d;
}

DateTime& DateTime::operator=(const DateTime& dt)
{
    if (&dt != this)
        *d = *dt.d;
    return *this;
}

DateTime& DateTime::operator=(const KADateTime& dt)
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

void DateTime::setDate(const QDate& date)
{
    d->mDateTime.setDate(date);
}

QDateTime DateTime::qDateTime() const
{
    return d->mDateTime.qDateTime();
}

KADateTime DateTime::kDateTime() const
{
    return d->mDateTime;
}

QTime DateTime::effectiveTime() const
{
    return d->mDateTime.isDateOnly() ? d->mStartOfDay : d->mDateTime.time();
}

void DateTime::setTime(const QTime& t)
{
    d->mDateTime.setTime(t);
}

QDateTime DateTime::effectiveDateTime() const
{
    if (d->mDateTime.isDateOnly())
    {
        QDateTime dt = d->mDateTime.qDateTime();    // preserve Qt::UTC or Qt::LocalTime
        dt.setTime(d->mStartOfDay);
        return dt;
    }
    return d->mDateTime.qDateTime();
}

KADateTime DateTime::effectiveKDateTime() const
{
    if (d->mDateTime.isDateOnly())
    {
        KADateTime dt = d->mDateTime;
        dt.setTime(d->mStartOfDay);
        return dt;
    }
    return d->mDateTime;
}

QDateTime DateTime::calendarDateTime() const
{
    if (d->mDateTime.isDateOnly())
    {
        QDateTime dt = d->mDateTime.qDateTime();
        dt.setTime(QTime(0, 0));
        return dt;
    }
    return d->mDateTime.qDateTime();
}

KADateTime DateTime::calendarKDateTime() const
{
    if (d->mDateTime.isDateOnly())
    {
        KADateTime dt = d->mDateTime;
        dt.setTime(QTime(0, 0));
        return dt;
    }
    return d->mDateTime;
}

QTimeZone DateTime::qTimeZone() const
{
    return d->mDateTime.qTimeZone();
}

QTimeZone DateTime::namedTimeZone() const
{
    return d->mDateTime.namedTimeZone();
}

KADateTime::Spec DateTime::timeSpec() const
{
    return d->mDateTime.timeSpec();
}

void DateTime::setTimeSpec(const KADateTime::Spec& spec)
{
    d->mDateTime.setTimeSpec(spec);
}

KADateTime::SpecType DateTime::timeType() const
{
    return d->mDateTime.timeType();
}

bool DateTime::isLocalZone() const
{
    return d->mDateTime.isLocalZone();
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

DateTime DateTime::toZone(const QTimeZone& zone) const
{
    return DateTime(d->mDateTime.toZone(zone));
}

DateTime DateTime::toTimeSpec(const KADateTime::Spec& spec) const
{
    return DateTime(d->mDateTime.toTimeSpec(spec));
}

qint64 DateTime::toSecsSinceEpoch() const
{
    return d->mDateTime.toSecsSinceEpoch();
}

void DateTime::setSecsSinceEpoch(qint64 secs)
{
    d->mDateTime.setSecsSinceEpoch(secs);
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

int DateTime::daysTo(const DateTime& dt) const
{
    return d->mDateTime.daysTo(dt.d->mDateTime);
}

int DateTime::minsTo(const DateTime& dt) const
{
    return d->mDateTime.secsTo(dt.d->mDateTime) / 60;
}

int DateTime::secsTo(const DateTime& dt) const
{
    return d->mDateTime.secsTo(dt.d->mDateTime);
}

qint64 DateTime::secsTo_long(const DateTime& dt) const
{
    return d->mDateTime.secsTo(dt.d->mDateTime);
}

QString DateTime::toString(KADateTime::TimeFormat f) const
{
    return d->mDateTime.toString(f);
}

QString DateTime::toString(const QString& format) const
{
    return d->mDateTime.toString(format);
}

QString DateTime::formatLocale(bool shortFormat) const
{
    QLocale::FormatType format = shortFormat ? QLocale::ShortFormat : QLocale::LongFormat;
    return d->mDateTime.isDateOnly() ? QLocale().toString(d->mDateTime.date(), format)
                                     : QLocale().toString(d->mDateTime.qDateTime(), format);
}

void DateTime::setStartOfDay(const QTime& sod)
{
    Private::mStartOfDay = sod;
}

KADateTime::Comparison DateTime::compare(const DateTime& other) const
{
    return d->mDateTime.compare(other.d->mDateTime);
}

QTime DateTime::startOfDay()
{
    return Private::mStartOfDay;
}

bool operator==(const DateTime& dt1, const DateTime& dt2)
{
    return dt1.d->mDateTime == dt2.d->mDateTime;
}

bool operator==(const KADateTime& dt1, const DateTime& dt2)
{
    return dt1 == dt2.d->mDateTime;
}

bool operator<(const DateTime& dt1, const DateTime& dt2)
{
    if (dt1.d->mDateTime.isDateOnly()  &&  !dt2.d->mDateTime.isDateOnly())
    {
        KADateTime dt = dt1.d->mDateTime.addDays(1);
        dt.setTime(DateTime::Private::mStartOfDay);
        return dt <= dt2.d->mDateTime;
    }
    if (!dt1.d->mDateTime.isDateOnly()  &&  dt2.d->mDateTime.isDateOnly())
    {
        KADateTime dt = dt2.d->mDateTime;
        dt.setTime(DateTime::Private::mStartOfDay);
        return dt1.d->mDateTime < dt;
    }
    return dt1.d->mDateTime < dt2.d->mDateTime;
}

} // namespace KAlarmCal

// vim: et sw=4:
