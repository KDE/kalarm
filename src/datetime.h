/*
 *  datetime.h  -  date/time with start-of-day time for date-only values
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  Copyright © 2003,2005-2007,2009,2011 by David Jarvie <djarvie@kde.org>
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
#ifndef KALARM_DATETIME_H
#define KALARM_DATETIME_H

#include "kalarmcal_export.h"

#include <kdatetime.h>

namespace KAlarmCal
{

/**
 *  @short As KDateTime, but with a configurable start-of-day time for date-only values.
 *
 *  The DateTime class holds a date, with or without a time.
 *
 *  DateTime is very similar to the KDateTime class. The time assumed for date-only values
 *  is the start-of-day time set by setStartOfDay().
 *
 *  @author David Jarvie <djarvie@kde.org>
*/
class KALARMCAL_EXPORT DateTime
{
public:
    /** Default constructor.
     *  Constructs an invalid date-time.
     */
    DateTime();
    /** Constructor for a date-only value. */
    DateTime(const QDate &d, const KDateTime::Spec &spec);
    /** Constructor for a date-time value. */
    DateTime(const QDate &d, const QTime &t, const KDateTime::Spec &spec);
    /** Constructor for a date-time value. */
    DateTime(const QDateTime &dt, const KDateTime::Spec &spec);
    /** Constructor for a date-time value. */
    DateTime(const KDateTime &dt);
    /** Copy constructor. */
    DateTime(const DateTime &dt);

    ~DateTime();
    /** Assignment operator. */
    DateTime &operator=(const DateTime &dt);
    /** Assignment operator.
     *  Sets the value to a specified date-time.
     */
    DateTime &operator=(const KDateTime &dt);

    /** Returns true if the date is null and, if it is a date-time value, the time is also null. */
    bool isNull() const;
    /** Returns true if the date is valid and, if it is a date-time value, the time is also valid. */
    bool isValid() const;
    /** Returns true if it is date-only value. */
    bool isDateOnly() const;
    /** Sets the value to be either date-only or date-time.
     *  @param d True to set the value to be date-only; false to set it to a date-time value.
     */
    void setDateOnly(bool d);

    /** Returns the date part of the value. */
    QDate date() const;
    /** Sets the date component of the value. */
    void setDate(const QDate &d);

    /** Returns the date and time of the value.
     *  If the value is date-only, the time part returned is 00:00:00. */
    QDateTime rawDateTime() const;
    /** Returns the date and time of the value as a KDateTime. */
    KDateTime kDateTime() const;
    /** Returns the time part of the value.
     *  If the value is date-only, the time returned is the start-of-day time set by setStartOfDay().
     */
    QTime effectiveTime() const;
    /** Sets the time component of the value.
     *  The value is converted if necessary to be a date-time value. */
    void setTime(const QTime &t);
    /** Returns the date and time of the value.
     *  If the value is date-only, the time part returned is equal to the start-of-day time set
     *  by setStartOfDay().
     */
    QDateTime effectiveDateTime() const;
    /** Sets the date/time component of the value. */
    void setDateTime(const QDateTime &dt);

    /** Returns the date and time of the value.
     *  If the value is date-only, the time part returned is equal to the start-of-day time set
     *  by setStartOfDay().
     */
    KDateTime effectiveKDateTime() const;
    /** Returns the date and time of the value as written in the calendar.
     *  If the value is date-only, the time part returned is 00:00.
     */
    KDateTime calendarKDateTime() const;

    /** Returns the time zone of the value. */
    KTimeZone timeZone() const;
    /** Returns the time specification of the value. */
    KDateTime::Spec timeSpec() const;
    /** Changes the time specification of the value. */
    void setTimeSpec(const KDateTime::Spec &spec);
    /** Returns the time specification type of the date/time, i.e. whether it is
     * UTC, has a time zone, etc. */
    KDateTime::SpecType timeType() const;
    /** Returns whether the time zone for the date/time is the current local system time zone. */
    bool isLocalZone() const;
    /** Returns whether the date/time is a local clock time. */
    bool isClockTime() const;
    /** Returns whether the date/time is a UTC time. */
    bool isUtc() const;
    /** Returns whether the date/time is a local time at a fixed offset from UTC. */
    bool isOffsetFromUtc() const;
    /** Returns the UTC offset associated with the date/time. */
    int utcOffset() const;

    /** Returns whether the date/time is the second occurrence of this time. */
    bool isSecondOccurrence() const;
    /** Sets whether this is the second occurrence of this date/time. */
    void setSecondOccurrence(bool second);

    /** Returns the time converted to UTC. */
    DateTime toUtc() const;
    /** Returns the time expressed as an offset from UTC, using the UTC offset
     * associated with this instance's date/time. */
    DateTime toOffsetFromUtc() const;
    /** Returns the time expressed as a specified offset from UTC. */
    DateTime toOffsetFromUtc(int utcOffset) const;
    /** Returns the time converted to the current local system time zone. */
    DateTime toLocalZone() const;
    /** Returns the time converted to the local clock time. */
    DateTime toClockTime() const;
    /** Returns the time converted to a specified time zone. */
    DateTime toZone(const KTimeZone &zone) const;
    /** Returns the time converted to a new time specification. */
    DateTime toTimeSpec(const KDateTime::Spec &spec) const;

    /** Converts the time to a UTC time, measured in seconds since 00:00:00 UTC
     * 1st January 1970 (as returned by time(2)). */
    uint toTime_t() const;
    /** Sets the value to a specified date-time value.
     *  @param secs The time_t date-time value, expressed as the number of seconds elapsed
     *              since 1970-01-01 00:00:00 UTC.
     */
    void setTime_t(uint secs);

    /** Returns a DateTime value @p secs seconds later than the value of this object. */
    DateTime addSecs(qint64 n) const;
    /** Returns a DateTime value @p mins minutes later than the value of this object. */
    DateTime addMins(qint64 n) const;
    /** Returns a DateTime value @p n days later than the value of this object. */
    DateTime addDays(int n) const;
    /** Returns a DateTime value @p n months later than the value of this object. */
    DateTime addMonths(int n) const;
    /** Returns a DateTime value @p n years later than the value of this object. */
    DateTime addYears(int n) const;

    /** Returns the number of days from this date or date-time to @p dt. */
    int daysTo(const DateTime &dt) const;
    /** Returns the number of minutes from this date or date-time to @p dt. */
    int minsTo(const DateTime &dt) const;
    /** Returns the number of seconds from this date or date-time to @p dt.
     *
     *  @warning The return value can overflow if the two values are far enough
     *           apart. Use sectTo_long() to avoid this.
     */
    int secsTo(const DateTime &dt) const;
    /** Returns the number of seconds as a qint64 from this date or date-time to @p dt. */
    qint64 secsTo_long(const DateTime &dt) const;

    /** Returns the value as a string.
     *  If it is a date-time, both time and date are included in the output.
     *  If it is date-only, only the date is included in the output.
     */
    QString toString(Qt::DateFormat f = Qt::TextDate) const;
    /** Returns the value as a string.
     *  If it is a date-time, both time and date are included in the output.
     *  If it is date-only, only the date is included in the output.
     */
    QString toString(const QString &format) const;
    /** Returns the value as a string, formatted according to the user's locale.
     *  If it is a date-time, both time and date are included in the output.
     *  If it is date-only, only the date is included in the output.
     */
    QString formatLocale(bool shortFormat = true) const;

    /** Sets the start-of-day time.
     *  The default value is midnight (0000 hrs).
     */
    static void setStartOfDay(const QTime &sod);

    /** Returns the start-of-day time. */
    static QTime startOfDay();

    /** Compare this value with another. */
    KDateTime::Comparison compare(const DateTime &other) const;

    KALARMCAL_EXPORT friend bool operator==(const KAlarmCal::DateTime &dt1, const KAlarmCal::DateTime &dt2);
    KALARMCAL_EXPORT friend bool operator==(const KDateTime &dt1, const KAlarmCal::DateTime &dt2);
    KALARMCAL_EXPORT friend bool operator<(const KAlarmCal::DateTime &dt1, const KAlarmCal::DateTime &dt2);
    friend bool operator<(const KDateTime &dt1, const KAlarmCal::DateTime &dt2);

private:
    //@cond PRIVATE
    class Private;
    Private *const d;
    //@endcond
};

/** Returns true if the two values are equal. */
KALARMCAL_EXPORT bool operator==(const DateTime &dt1, const DateTime &dt2);
KALARMCAL_EXPORT bool operator==(const KDateTime &dt1, const DateTime &dt2);

/** Returns true if the two values are not equal. */
inline bool operator!=(const DateTime &dt1, const DateTime &dt2)
{
    return !operator==(dt1, dt2);
}
inline bool operator!=(const KDateTime &dt1, const DateTime &dt2)
{
    return !operator==(dt1, dt2);
}

/** Returns true if the @p dt1 is earlier than @p dt2.
 *  If the two values have the same date, and one value is date-only while the other is a date-time, the
 *  time used for the date-only value is the start-of-day time set in the KAlarm Preferences dialog.
 */
KALARMCAL_EXPORT bool operator<(const DateTime &dt1, const DateTime &dt2);
inline bool operator<(const KDateTime &dt1, const DateTime &dt2)
{
    return operator<(DateTime(dt1), dt2);
}

/** Returns true if the @p dt1 is later than @p dt2.
 *  If the two values have the same date, and one value is date-only while the other is a date-time, the
 *  time used for the date-only value is the start-of-day time set in the KAlarm Preferences dialog.
 */
inline bool operator>(const DateTime &dt1, const DateTime &dt2)
{
    return operator<(dt2, dt1);
}
inline bool operator>(const KDateTime &dt1, const DateTime &dt2)
{
    return operator<(dt2, DateTime(dt1));
}

/** Returns true if the @p dt1 is later than or equal to @p dt2.
 *  If the two values have the same date, and one value is date-only while the other is a date-time, the
 *  time used for the date-only value is the start-of-day time set in the KAlarm Preferences dialog.
 */
inline bool operator>=(const DateTime &dt1, const DateTime &dt2)
{
    return !operator<(dt1, dt2);
}
inline bool operator>=(const KDateTime &dt1, const DateTime &dt2)
{
    return !operator<(DateTime(dt1), dt2);
}

/** Returns true if the @p dt1 is earlier than or equal to @p dt2.
 *  If the two values have the same date, and one value is date-only while the other is a date-time, the
 *  time used for the date-only value is the start-of-day time set in the KAlarm Preferences dialog.
 */
inline bool operator<=(const DateTime &dt1, const DateTime &dt2)
{
    return !operator<(dt2, dt1);
}
inline bool operator<=(const KDateTime &dt1, const DateTime &dt2)
{
    return !operator<(dt2, DateTime(dt1));
}

} // namespace KAlarmCal

#endif // KALARM_DATETIME_H

