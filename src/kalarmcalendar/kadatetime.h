/*
 *  This file is part of kalarmcalendar library, which provides access to KAlarm
 *  calendar data.
 *  This is the Qt5 version of KDE 4 kdelibs/kdecore/date/kdatetime.h.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
*/

/** @file
 * Date/times with associated time zone
 * @author David Jarvie <djarvie@kde.org>.
 */

#pragma once

#include "kalarmcal_export.h"

#include <QDateTime>
#include <QMetaType>
#include <QSharedDataPointer>

class QDataStream;

namespace KAlarmCal
{

class KADateTimePrivate;
class KADateTimeSpecPrivate;

/**
 * @short A class representing a date and time with an associated time zone
 *
 * Topics:
 *  - @ref intro
 *  - @ref manipulation
 *  - @ref compatibility
 *
 * @section intro Introduction
 *
 * The class KADateTime extends the functionality of QDateTime. It combines a
 * date and time with support for an associated time zone or UTC offset. When
 * manipulating KADateTime objects, their time zones or UTC offsets are
 * automatically taken into account. Extensions to QDateTime are:
 * - KADateTime can be set to represent a date-only value with no associated
 *   time.
 * - Local date/times which occur twice due to daylight savings time shifts,
 *   are handled properly, so that the two occurrences can be distinguished.
 * - Extra string formatting functions are provided.
 *
 * The class uses QDateTime internally to represent date/time values. This
 * imposes the following limitations:
 * - QDateTime uses the Gregorian calendar retroactively. If you need the
 *   Julian calendar for historical dates (as commonly used prior to some date
 *   between 1582 and 1923 depending on nation), please use @c KCalendarSystem
 *   and related classes.
 * - QDateTime does not handle daylight savings time changes properly before
 *   1970, even when the system's time zone database contains this information.
 *
 * The time specification types which KADateTime supports are:
 * - the UTC time zone
 * - a local time with a specified offset from UTC
 * - a local time in a specified time zone
 * - a local time using the current system time zone. This follows any changes
 *   to the system time zone.
 *
 * These characteristics are more fully described in the description of the
 * SpecType enumeration. Also see
 * <a href="http://www.w3.org/TR/timezone/">W3C: Working with Time Zones</a>
 * for a good overview of the different ways of representing times.
 *
 * To set the time specification, use one of the setTimeSpec() methods. To get
 * the time specification, call timeSpec(), isUtc(), isLocalZone(), or
 * isOffsetFromUtc(). To determine whether two KADateTime instances have the
 * same time specification, call timeSpec() on each and compare the returned
 * values using KADateTime::Spec::operator==().
 *
 * @section manipulation Date and Time Manipulation
 *
 * A KADateTime object can be created by passing a date and time in its
 * constructor, together with a time specification.
 *
 * If both the date and time are null, isNull() returns true. If the date, time
 * and time specification are all valid, isValid() returns true.
 *
 * A KADateTime object can be converted to a different time specification by
 * using toUtc() or toLocalZone(). It can be converted to a specific time zone
 * by toZone(). To return the time as an elapsed time since 1 January 1970 (as
 * used by time(2)), use toSecsSinceEpoch(). The results of time zone
 * conversions are cached to minimize the need for recalculation. Each
 * KADateTime object caches its UTC equivalent and the last time zone
 * conversion performed.
 *
 * The date and time can be set either in the constructor, or afterwards by
 * calling setDate() or setTime(). To return the date and/or time components of
 * the KADateTime, use date(), time() and qDateTime(). You can determine whether
 * the KADateTime represents a date and time, or a date only, by isDateOnly().
 * You can change between a date and time or a date only value using
 * setDateOnly().
 *
 * You can increment or decrement the date/time using addSecs(), addDays(),
 * addMonths() and addYears(). The interval between two date/time values can
 * be found using secsTo() or daysTo().
 *
 * The comparison operators (operator==(), operator<(), etc.) all take the time
 * zone properly into account; if the two KADateTime objects have different time
 * zones, they are first converted to UTC before the comparison is
 * performed. An alternative to the comparison operators is compare() which will
 * in addition tell you if a KADateTime object overlaps with another when one or
 * both are date-only values.
 *
 * KADateTime values may be converted to and from a string representation using
 * the toString() and fromString() methods. These handle a variety of text
 * formats including ISO 8601 and RFC 2822.
 *
 * KADateTime uses Qt's facilities to implicitly share data. Copying instances
 * is very efficient, and copied instances share cached UTC and time zone
 * conversions even after the copy is performed. A separate copy of the data is
 * created whenever a non-const method is called. If you want to force the
 * creation of a separate copy of the data (e.g. if you want two copies to
 * cache different time zone conversions), call detach().
 *
 * @section compatibility QDateTime Considerations
 *
 * KADateTime's interface is designed to be as compatible as possible with that
 * of QDateTime. Because QDateTime lacks virtual methods, KADateTime is not
 * inherited from QDateTime, but instead is implemented using a private
 * QDateTime object.
 *
 * @section simulation Simulation Facility
 *
 * This class provides a facility to simulate the local system time, which
 * affects all functions using or returning the system time. This facility is
 * provided for testing purposes only, and is only available if the library is
 * compiled with debug enabled. In release mode, simulation is inoperative and
 * the real local system time is used at all times. Use
 * setSimulatedSystemTime() to set or clear the simulated time. To read the
 * real (not simulated) system time, use realCurrentLocalDateTime().
 *
 * @see QTimeZone, QDateTime, QDate, QTime
 * @see <a href="http://www.w3.org/TR/timezone/">W3C: Working with Time Zones</a>
 * @author David Jarvie \<djarvie@kde.org\>.
 */
class KALARMCAL_EXPORT KADateTime
{
public:
    /**
     * The time specification type of a KADateTime instance.
     * This specifies how the date/time component of the KADateTime instance
     * should be interpreted, i.e. what type of time zone (if any) the date/time
     * is expressed in. For the full time specification (including time zone
     * details), see KADateTime::Spec.
     */
    enum SpecType
    {
        Invalid,    /**< an invalid time specification. */
        UTC,        /**< a UTC time. */
        OffsetFromUTC, /**< a local time which has a fixed offset from UTC. */
        TimeZone,   /**< a time in a specified time zone.
		     *   If the time zone happens to be the current system time
		     *   zone (i.e. that returned by QTimeZone::systemTimeZone()),
		     *   that time zone will continue to be used, unlike in the
		     *   case of LocalZone, even if the system changes to use a
		     *   different time zone.
                     */
        LocalZone   /**< a time in the current system time zone.
		     *   Note that if the system is changed to a different time
		     *   zone afterwards, the KADateTime instance will then use
		     *   the new system time zone, as returned currently by
		     *   QTimeZone::systemTimeZone().
                     */
    };

    /**
     * The full time specification of a KADateTime instance.
     * This specifies how the date/time component of the KADateTime instance
     * should be interpreted, i.e. which time zone (if any) the date/time is
     * expressed in.
     */
    class KALARMCAL_EXPORT Spec
    {
    public:
        /**
         * Constructs an invalid time specification.
         */
        Spec();

        /**
         * Constructs a time specification for a given time zone.
         * If @p tz is QTimeZone::utc(), the time specification type is set to @c UTC.
         *
         * @param tz  time zone
         */
        Spec(const QTimeZone& tz);   //cppcheck-suppress noExplicitConstructor; Allow implicit conversion

        /**
         * Constructs a time specification.
         *
         * @param type      time specification type, which should not be @c TimeZone
         * @param utcOffset number of seconds to add to UTC to get the local
         *                  time. Ignored if @p type is not @c OffsetFromUTC.
         */
        Spec(SpecType type, int utcOffset = 0);   //cppcheck-suppress noExplicitConstructor; Allow implicit conversion

        /**
         * Copy constructor.
         */
        Spec(const Spec& spec);

        /**
         * Assignment operator.
         */
        Spec& operator=(const Spec& spec);

        /**
         * Destructor
         */
        ~Spec();

        /**
         * Returns whether the time specification is valid.
         *
         * @return @c true if valid, else @c false
         */
        bool isValid() const;

        /**
         * Returns the new (Qt >= 6.5) QTimeZone equivalent for the time
         * specification type as follows:
         * - @c TimeZone      : the time zone is returned.
         * - @c UTC           : UTC is returned.
         * - @c OffsetFromUTC : the offset from UTC is returned.
         * - @c LocalZone     : a "local time" instance is returned, NOT the
         *                      system time zone.
         *
         * @return time zone as defined above, or invalid in all other cases
         * @see isUtc(), isLocal()
         */
        QTimeZone qTimeZone() const;

        /**
         * Returns the fully fledged time zone (i.e. not a UTC offset) for the
         * time specification type as follows:
         * - @c TimeZone      : the specified time zone is returned.
         * - @c UTC           : the UTC time zone is returned.
         * - @c LocalZone     : the current local time zone is returned.
         * - @c OffsetFromUTC : an invalid time zone is returned.
         *
         * @return time zone as defined above, or invalid in all other cases
         * @see isUtc(), isLocal()
         */
        QTimeZone namedTimeZone() const;

        /**
         * Returns the time specification type, i.e. whether it is
         * UTC, has a time zone, etc.
         *
         * @return specification type
         * @see isLocalZone(), isUtc(), namedTimeZone(), qTimeZone()
         */
        SpecType type() const;

        /**
         * Returns whether the time specification follows the current local
         * system time zone.
         *
         * @return @c true if local system time zone
         * @see isUtc(), isOffsetFromUtc(), namedTimeZone(), qTimeZone()
         */
        bool isLocalZone() const;

        /**
         * Returns whether the time specification is a UTC time.
         * It is considered to be a UTC time if it is either type @c UTC,
         * or is type @c OffsetFromUTC with a zero UTC offset.
         *
         * @return @c true if UTC
         * @see isLocal(), isOffsetFromUtc(), namedTimeZone(), qTimeZone()
         */
        bool isUtc() const;

        /**
         * Returns whether the time specification is a local time at a fixed
         * offset from UTC.
         *
         * @return @c true if local time at fixed offset from UTC
         * @see isLocal(), isUtc(), utcOffset()
         */
        bool isOffsetFromUtc() const;

        /**
         * Returns the UTC offset associated with the time specification. The
         * UTC offset is the number of seconds to add to UTC to get the local time.
         *
         * @return UTC offset in seconds if type is @c OffsetFromUTC, else 0
         * @see isOffsetFromUtc()
         */
        int utcOffset() const;

        /**
         * Initialises the time specification.
         *
         * @param type      the time specification type. Note that @c TimeZone
         *                  is invalid here.
         * @param utcOffset number of seconds to add to UTC to get the local
         *                  time. Ignored if @p spec is not @c OffsetFromUTC.
         * @see type(), setType(const QTimeZone&)
         */
        void setType(SpecType type, int utcOffset = 0);

        /**
         * Sets the time zone for the time specification.
         *
         * Note that setting the time zone to the current local system time zone
	 * has a different effect from setType(LocalZone), since the latter
	 * will follow changes to the system time zone.
         *
         * @param tz new time zone
         * @see namedTimeZone(), qTimeZone(), setType(SpecType)
         */
        void setType(const QTimeZone& tz);

        /**
         * Comparison operator.
         *
         * @return @c true if the two instances are identical, @c false otherwise
         * @see equivalentTo()
         */
        bool operator==(const Spec& other) const;

        bool operator!=(const Spec& other) const
        {
            return !operator==(other);
        }

        /**
         * Checks whether this instance is equivalent to another.
         * The two instances are considered to be equivalent if any of the following
         * conditions apply:
         * - both instances are type @c OffsetFromUTC and their offsets from UTC are equal.
         * - both instances are type @c TimeZone and their time zones are equal.
         * - one instance is type @c TimeZone, with the time zone set to the system time
         *   zone, and the other instance is type @c LocalZone.
         * - both instances are type @c LocalZone.
         * - both instances are UTC. An instance is considered to be UTC if it is
         *   either type @c UTC, or is type @c OffsetFromUTC with a zero UTC offset.
         *
         * @return @c true if the two instances are equivalent, @c false otherwise
         * @see operator==()
         */
        bool equivalentTo(const Spec& other) const;

        /**
         * The UTC time specification.
         * Provided as a shorthand for KADateTime::Spec(KADateTime::UTC).
         */
        static Spec UTC();

        /**
         * Returns a UTC offset time specification.
         * Provided as a shorthand for KADateTime::Spec(KADateTime::OffsetFromUTC, utcOffset).
         *
         * @param utcOffset number of seconds to add to UTC to get the local time
         * @return UTC offset time specification
         */
        static Spec OffsetFromUTC(int utcOffset);

        /**
         * Returns a local time zone time specification.
         * Provided as a shorthand for KADateTime::Spec(KADateTime::LocalZone).
         *
         * @return Local zone time specification
         */
        static Spec LocalZone();

    private:
        KADateTimeSpecPrivate* const d;
    };

    /** Format for strings representing date/time values. */
    enum TimeFormat
    {
        ISODate,    /**< ISO 8601 format, i.e. [±]YYYY-MM-DDThh[:mm[:ss[.sss]]]TZ,
                     *   where TZ is the time zone offset (blank for local
                     *   time, Z for UTC, or ±hhmm for an offset from UTC).
                     *   When parsing a string, the ISO 8601 basic format,
                     *   [±]YYYYMMDDThh[mm[ss[.sss]]]TZ, is also accepted. For
                     *   date-only values, the formats [±]YYYY-MM-DD and
                     *   [±]YYYYMMDD (without time zone specifier) are used. All
                     *   formats may contain a day of the year instead of day
                     *   and month.
                     *   To allow for years past 9999, the year may optionally
                     *   contain more than 4 digits. To avoid ambiguity, this is
                     *   not allowed in the basic format containing a day
                     *   of the year (i.e. when the date part is [±]YYYYDDD).
                     */
        ISODateFull,/**< ISO 8601 format, always including a time zone.
		     *   This is the same as ISODate, except that the current
		     *   system time zone offset is appended for local times
		     *   (type LocalZone).
		     */
        RFCDate,    /**< RFC 2822 format,
                     *   i.e. "[Wdy,] DD Mon YYYY hh:mm[:ss] ±hhmm". This format
                     *   also covers RFCs 822, 850, 1036 and 1123.
                     *   When parsing a string, it also accepts the format
                     *   "Wdy Mon DD HH:MM:SS YYYY" specified by RFCs 850 and
                     *   1036. There is no valid date-only format.
                     */
        RFCDateDay, /**< RFC 2822 format including day of the week,
                     *   i.e. "Wdy, DD Mon YYYY hh:mm:ss ±hhmm"
                     */
        QtTextDate, /**< Same format as Qt::TextDate (i.e. Day Mon DD hh:mm:ss YYYY)
                     *   with, if not local time, the UTC offset appended. The
                     *   time may be omitted to indicate a date-only value.
                     */
        LocalDate,  /**< Same format as Qt::LocalDate (i.e. locale dependent)
                     *   with, if not local time, the UTC offset appended. The
                     *   time may be omitted to indicate a date-only value.
                     */
        RFC3339Date /**< RFC 3339 format,
                     *   i.e. "YYYY-MM-DDThh:mm:ss[.sss](Z|±hh:mm)".
                     *   There is no valid date-only format.
                     */

    };

    /**
     * How this KADateTime compares with another.
     * If any date-only value is involved, comparison of KADateTime values
     * requires them to be considered as representing time periods. A date-only
     * instance represents a time period from 00:00:00 to 23:59:59.999 on a given
     * date, while a date/time instance can be considered to represent a time
     * period whose start and end times are the same. They may therefore be
     * earlier or later, or may overlap or be contained one within the other.
     *
     * Values may be OR'ed with each other in any combination of 'consecutive'
     * intervals to represent different types of relationship.
     *
     * In the descriptions of the values below,
     * - s1 = start time of this instance
     * - e1 = end time of this instance
     * - s2 = start time of other instance
     * - e2 = end time of other instance.
     */
    enum Comparison
    {
        Before  = 0x01, /**< This KADateTime is strictly earlier than the other,
                         *   i.e. e1 < s2.
                         */
        AtStart = 0x02, /**< This KADateTime starts at the same time as the other,
                         *   and ends before the end of the other,
                         *   i.e. s1 = s2, e1 < e2.
                         */
        Inside  = 0x04, /**< This KADateTime starts after the start of the other,
                         *   and ends before the end of the other,
                         *   i.e. s1 > s2, e1 < e2.
                         */
        AtEnd   = 0x08, /**< This KADateTime starts after the start of the other,
                         *   and ends at the same time as the other,
                         *   i.e. s1 > s2, e1 = e2.
                         */
        After   = 0x10, /**< This KADateTime is strictly later than the other,
                         *   i.e. s1 > e2.
                         */
        Equal = AtStart | Inside | AtEnd,
                        /**< Simultaneous, i.e. s1 = s2 && e1 = e2.
                         */
        Outside = Before | AtStart | Inside | AtEnd | After,
                        /**< This KADateTime starts before the start of the other,
                         *   and ends after the end of the other,
                         *   i.e. s1 < s2, e1 > e2.
                         */
        StartsAt = AtStart | Inside | AtEnd | After,
                        /**< This KADateTime starts at the same time as the other,
                         *   and ends after the end of the other,
                         *   i.e. s1 = s2, e1 > e2.
                         */
        EndsAt = Before | AtStart | Inside | AtEnd
                        /**< This KADateTime starts before the start of the other,
                         *   and ends at the same time as the other,
                         *   i.e. s1 < s2, e1 = e2.
                         */
    };

    /**
     * Constructs an invalid date/time.
     */
    KADateTime();

    /**
     * Constructs a date-only value expressed in a given time specification. The
     * time is set to 00:00:00.
     *
     * The instance is initialised according to the time specification type of
     * @p spec as follows:
     * - @c UTC           : date is stored as UTC.
     * - @c OffsetFromUTC : date is a local time at the specified offset
     *                      from UTC.
     * - @c TimeZone      : date is a local time in the specified time zone.
     * - @c LocalZone     : date is a local date in whatever the system time
     *                      zone is currently.
     *
     * @param date date in the time zone indicated by @p spec
     * @param spec time specification
     */
    explicit KADateTime(const QDate& date, const Spec& spec = Spec(LocalZone));

    /**
     * Constructs a date/time expressed as specified by @p spec.
     *
     * @p date and @p time are interpreted and stored according to the value of
     * @p spec as follows:
     * - @c UTC           : @p date and @p time are in UTC.
     * - @c OffsetFromUTC : date/time is a local time at the specified offset
     *                      from UTC.
     * - @c TimeZone      : date/time is a local time in the specified time zone.
     * - @c LocalZone     : @p date and @p time are local times in whatever the
     *                      system time zone is currently.
     *
     * If @p spec is @c TimeZone or @c LocalZone and the resultant date/time
     * occurs twice due to a daylight savings time shift, it is set to the first
     * occurrence (before the time shift).
     *
     * @param date date in the time zone indicated by @p spec
     * @param time time in the time zone indicated by @p spec
     * @param spec time specification
     */
    KADateTime(const QDate& date, const QTime& time, const Spec& spec = Spec(LocalZone));

    /**
     * Constructs a date/time expressed in a given time specification.
     *
     * In detail, @p dt is interpreted and stored according to the time
     * specification type of @p spec as follows:
     * - @c UTC           : @p dt is stored as a UTC value. If @c dt.timeSpec()
     *                      is not @c Qt::UTC, @p dt is first converted to UTC
     *                      before storage.
     * - @c OffsetFromUTC : date/time is stored as a time at the specified offset
     *                      from UTC. If @c dt.timeSpec() is not @c Qt::UTC,
     *                      the time is converted to UTC and then adjusted by
     *                      the UTC offset before storage.
     * - @c TimeZone      : date/time is stored as a local time in the specified
     *                      time zone. If @c dt.timeSpec() is not the same time
     *                      zone, @p dt is converted to the specified time zone
     *                      before storage.
     * - @c LocalZone     : @p dt is stored as a local time in the current system
     *                      time zone. If @c dt.timeSpec() is not @c Qt::LocalTime,
     *                      @p dt is first converted to the system time zone before
     *                      storage.
     *
     * @param dt date and time
     * @param spec time specification
     */
    KADateTime(const QDateTime& dt, const Spec& spec);

    /**
     * Constructs a date/time from a QDateTime.
     * The KADateTime's time specification is determined according to @p dt.timeSpec().
     *
     * @param dt date and time
     */
    explicit KADateTime(const QDateTime& dt);

    KADateTime(const KADateTime& other);
    ~KADateTime();

    KADateTime& operator=(const KADateTime& other);

    /**
     * Returns whether the date/time is null.
     *
     * @return @c true if both date and time are null, else @c false
     * @see isValid(), QDateTime::isNull()
     */
    bool isNull() const;

    /**
     * Returns whether the date/time is valid.
     *
     * @return @c true if both date and time are valid, else @c false
     * @see isNull(), QDateTime::isValid()
     */
    bool isValid() const;

    /**
     * Returns whether the instance represents a date/time or a date-only value.
     *
     * @return @c true if date-only, @c false if date and time
     */
    bool isDateOnly() const;

    /**
     * Returns the date part of the date/time. The value returned should be
     * interpreted in terms of the instance's time zone or UTC offset.
     *
     * @return date value
     * @see time(), qDateTime()
     */
    QDate date() const;

    /**
     * Returns the time part of the date/time. The value returned should be
     * interpreted in terms of the instance's time zone or UTC offset. If
     * the instance is date-only, the time returned is 00:00:00.
     *
     * @return time value
     * @see date(), qDateTime(), isDateOnly()
     */
    QTime time() const;

    /**
     * Converts the instance to a QDateTime value.
     *
     * If the instance is date-only, the time value is set to 00:00:00.
     *
     * @return date/time
     * @see date(), time()
     */
    QDateTime qDateTime() const;

    /**
     * Returns the new (Qt >= 6.5) QTimeZone for the date/time,
     * depending on the timeSpec() as follows:
     * - @c TimeZone      : the time zone is returned.
     * - @c UTC           : UTC is returned.
     * - @c OffsetFromUTC : the offset from UTC is returned.
     * - @c LocalZone     : a "local time" instance is returned, NOT the
     *                      system time zone.
     *
     * @return time zone as defined above, or invalid in all other cases
     * @see isUtc(), isLocal()
     */
    QTimeZone qTimeZone() const;

    /**
     * Returns the fully fledged time zone (i.e. not a UTC offset) for the date/time.
     * If the date/time is specified as a UTC time, the UTC time zone is
     * returned. If it is specified as LocalZone, the system time zone is
     * returned.
     *
     * @return time zone, or invalid if a time at a fixed UTC offset
     * @see isUtc(), isLocal()
     */
    QTimeZone namedTimeZone() const;

    /**
     * Returns the time specification of the date/time, i.e. whether it is
     * UTC, what time zone it is, etc.
     *
     * @return time specification
     * @see isLocalZone(), isUtc(), namedTimeZone(), qTimeZone()
     */
    Spec timeSpec() const;

    /**
     * Returns the time specification type of the date/time, i.e. whether it is
     * UTC, has a time zone, etc.
     *
     * @return specification type
     * @see timeSpec(), isLocalZone(), isUtc(), namedTimeZone(), qTimeZone()
     */
    SpecType timeType() const;

    /**
     * Returns whether the time zone for the date/time follows the current local
     * system time zone.
     *
     * @return @c true if local system time zone
     * @see isUtc(), isOffsetFromUtc(), namedTimeZone(), qTimeZone()
     */
    bool isLocalZone() const;

    /**
     * Returns whether the date/time is a UTC time.
     * It is considered to be a UTC time if it either has a UTC time
     * specification (SpecType == UTC), or has a zero offset from UTC
     * (SpecType == OffsetFromUTC with zero UTC offset).
     *
     * @return @c true if UTC
     * @see isLocal(), isOffsetFromUtc(), namedTimeZone(), qTimeZone()
     */
    bool isUtc() const;

    /**
     * Returns whether the date/time is a local time at a fixed offset from
     * UTC.
     *
     * @return @c true if local time at fixed offset from UTC
     * @see isLocal(), isUtc(), utcOffset()
     */
    bool isOffsetFromUtc() const;

    /**
     * Returns the UTC offset associated with the date/time. The UTC offset is
     * the number of seconds to add to UTC to get the local time.
     *
     * @return UTC offset in seconds
     */
    int utcOffset() const;

    /**
     * Returns whether the date/time is the second occurrence of this time. This
     * is only applicable to a date/time expressed in terms of a time zone (type
     * @c TimeZone or @c LocalZone), around the time of change from daylight
     * savings to standard time.
     *
     * When a shift from daylight savings time to standard time occurs, the local
     * times (typically the previous hour) immediately preceding the shift occur
     * twice. For example, if a time shift of 1 hour happens at 03:00, the clock
     * jumps backwards to 02:00, so the local times between 02:00:00 and 02:59:59
     * occur once before the shift, and again after the shift.
     *
     * For instances which are not of type @c TimeZone, or when the date/time is
     * not near to a time shift, @c false is returned.
     *
     * @return @c true if the time is the second occurrence, @c false otherwise
     * @see setSecondOccurrence()
     */
    bool isSecondOccurrence() const;

    /**
     * Returns whether the date/time falls within daylight savings time in its
     * time zone.
     *
     * @return @c true if daylight savings time applies;
     *         @c false if standard time applies, or if timeSpec() is not
     *                  TimeZone or LocalZone.
     */
    bool isDaylightTime() const;

    /**
     * Returns the time converted to UTC. The converted time has a UTC offset
     * of zero.
     * If the instance is a date-only value, a date-only UTC value is returned,
     * with the date unchanged.
     *
     * @return converted time
     * @see toOffsetFromUtc(), toLocalZone(), toZone(), toTimeSpec(), toSecsSinceEpoch(), QTimeZone::convert()
     */
    KADateTime toUtc() const;

    /**
     * Returns the time expressed as an offset from UTC, using the UTC offset
     * associated with this instance's date/time. The date and time
     * components are unchanged. For example, 14:15 on 12 Jan 2001, US Eastern
     * time zone would return a KADateTime value of 14:15 on 12 Jan 2001 with a
     * UTC offset of -18000 seconds (i.e. -5 hours).
     *
     * If the instance is a date-only value, the offset is set to that at the
     * start of the day.
     *
     * @return converted time
     * @see toUtc(), toOffsetFromUtc(int), toLocalZone(), toZone(), toTimeSpec(), toSecsSinceEpoch(), QTimeZone::convert()
     */
    KADateTime toOffsetFromUtc() const;

    /**
     * Returns the time expressed as a specified offset from UTC.
     *
     * If the instance is a date-only value, a date-only UTC offset time value
     * is returned, with the date unchanged.
     *
     * @param utcOffset number of seconds to add to UTC to get the local time.
     * @return converted time
     * @see toUtc(), toOffsetFromUtc(), toLocalZone(), toZone(), toTimeSpec(), toSecsSinceEpoch(), QTimeZone::convert()
     */
    KADateTime toOffsetFromUtc(int utcOffset) const;

    /**
     * Returns the time converted to the current local system time zone.
     * If the instance is a date-only value, a date-only local time zone value
     * is returned, with the date unchanged.
     *
     * @return converted time
     * @see toUtc(), toOffsetFromUtc(), toZone(), toTimeSpec(), QTimeZone::convert()
     */
    KADateTime toLocalZone() const;

    /**
     * Returns the time converted to a specified time zone.
     * If the instance is a date-only value, a date-only value in @p zone is
     * returned, with the date unchanged.
     *
     * @param zone time zone to convert to
     * @return converted time
     * @see toUtc(), toOffsetFromUtc(), toLocalZone(), toTimeSpec(), QTimeZone::convert()
     */
    KADateTime toZone(const QTimeZone& zone) const;

    /**
     * Returns the time converted to a new time specification.
     * If the instance is a date-only value, a date-only value is returned,
     * with the date unchanged.
     *
     * @param spec new time specification
     * @return converted time
     * @see toLocalZone(), toUtc(), toOffsetFromUtc(), toZone(), QTimeZone::convert()
     */
    KADateTime toTimeSpec(const Spec& spec) const;

    /**
     * Returns the time converted to the time specification of another instance.
     * If this instance is a date-only value, a date-only value is returned,
     * with the date unchanged.
     *
     * @param dt instance providing the new time specification
     * @return converted time
     * @see toLocalZone(), toUtc(), toOffsetFromUtc(), toZone(), QTimeZone::convert()
     */
    KADateTime toTimeSpec(const KADateTime& dt) const;

    /**
     * Converts the time to a UTC time, measured in seconds since 00:00:00 UTC
     * 1st January 1970 (as returned by time(2)).
     *
     * @return converted time, or @c LLONG_MIN if the date is out of range or invalid
     * @see setSecsSinceEpoch()
     */
    qint64 toSecsSinceEpoch() const;

    /**
     * Sets the time to a UTC time, specified as seconds since 00:00:00 UTC
     * 1st January 1970 (as returned by time(2)).
     *
     * @param seconds number of seconds since 00:00:00 UTC 1st January 1970
     * @see toSecsSinceEpoch()
     */
    void setSecsSinceEpoch(qint64 seconds);

    /**
     * Sets the instance either to being a date and time value, or a date-only
     * value. If its status is changed to date-only, its time is set to
     * 00:00:00.
     *
     * If the resultant date/time occurs twice due to a daylight savings time
     * shift, it is set to the first occurrence (before the time shift).
     *
     * @param dateOnly @c true to set to date-only, @c false to set to date
     *                 and time.
     * @see isDateOnly(), setTime()
     */
    void setDateOnly(bool dateOnly);

    /**
     * Sets the date part of the date/time.
     *
     * If the resultant date/time occurs twice due to a daylight savings time
     * shift, it is set to the first occurrence (before the time shift).
     *
     * @param date new date value
     * @see date(), setTime(), setTimeSpec(), setSecsSinceEpoch(), setDateOnly()
     */
    void setDate(const QDate& date);

    /**
     * Sets the time part of the date/time. If the instance was date-only, it
     * is changed to being a date and time value.
     *
     * If the resultant date/time occurs twice due to a daylight savings time
     * shift, it is set to the first occurrence (before the time shift).
     *
     * @param time new time value
     * @see time(), setDate(), setTimeSpec(), setSecsSinceEpoch()
     */
    void setTime(const QTime& time);

    /**
     * Changes the time specification of the instance.
     *
     * Any previous time zone is forgotten. The stored date/time component of
     * the instance is left unchanged (except that its Qt::TimeSpec setting is
     * set to correspond with @p spec). Usually this method will change the
     * absolute time which this instance represents.
     *
     * @param spec new time specification
     * @see timeSpec(), namedTimeZone(), qTimeZone()
     */
    void setTimeSpec(const Spec& spec);

    /**
     * Sets whether the date/time is the second occurrence of this time. This
     * is only applicable to a date/time expressed in terms of a time zone (type
     * @c TimeZone or @c LocalZone), around the time of change from daylight
     * savings to standard time.
     *
     * When a shift from daylight savings time to standard time occurs, the local
     * times (typically the previous hour) immediately preceding the shift occur
     * twice. For example, if a time shift of 1 hour happens at 03:00, the clock
     * jumps backwards to 02:00, so the local times between 02:00:00 and 02:59:59
     * occur once before the shift, and again after the shift.
     *
     * For instances which are not of type @c TimeZone, or when the date/time is
     * not near to a time shift, calling this method has no effect.
     *
     * Note that most other setting methods clear the second occurrence indicator,
     * so if you want to retain its setting, you must call setSecondOccurrence()
     * again after changing the instance's value.
     *
     * @param second @c true to set as the second occurrence, @c false to set as
     *               the first occurrence
     * @see isSecondOccurrence()
     */
    void setSecondOccurrence(bool second);

    /**
     * Returns a date/time @p msecs milliseconds later than the stored date/time.
     *
     * The calculation is done in UTC to ensure that the result takes proper
     * account of clock changes (e.g. daylight savings) in the time zone. The
     * result is expressed using the same time specification as the original
     * instance.
     *
     * If the instance is date-only, @p msecs is rounded down to a whole number
     * of days and that value is added to the date to find the result.
     *
     * @return resultant date/time
     * @see addSecs(), addDays(), addMonths(), addYears(), secsTo()
     */
    KADateTime addMSecs(qint64 msecs) const;

    /**
     * Returns a date/time @p secs seconds later than the stored date/time.
     *
     * The calculation is done in UTC to ensure that the result takes proper
     * account of clock changes (e.g. daylight savings) in the time zone. The
     * result is expressed using the same time specification as the original
     * instance.
     *
     * If the instance is date-only, @p secs is rounded down to a whole number
     * of days and that value is added to the date to find the result.
     *
     * @return resultant date/time
     * @see addMSecs(), addDays(), addMonths(), addYears(), secsTo()
     */
    KADateTime addSecs(qint64 secs) const;

    /**
     * Returns a date/time @p days days later than the stored date/time.
     * The result is expressed using the same time specification as the
     * original instance.
     *
     * @return resultant date/time
     * @see addSecs(), addMonths(), addYears(), daysTo()
     */
    KADateTime addDays(qint64 days) const;

    /**
     * Returns a date/time @p months months later than the stored date/time.
     * The result is expressed using the same time specification as the
     * original instance.
     *
     * @return resultant date/time
     * @see addSecs(), addDays(), addYears(), daysTo()
     */
    KADateTime addMonths(int months) const;

    /**
     * Returns a date/time @p years years later than the stored date/time.
     * The result is expressed using the same time specification as the
     * original instance.
     *
     * @return resultant date/time
     * @see addSecs(), addDays(), addMonths(), daysTo()
     */
    KADateTime addYears(int years) const;

    /**
     * Returns the number of milliseconds from this date/time to the @p other date/time.
     *
     * Before performing the comparison, the two date/times are converted to UTC
     * to ensure that the result is correct if one of the two date/times has
     * daylight saving time (DST) and the other doesn't.
     *
     * If one instance is date-only and the other is date-time, the date-time
     * value is first converted to the same time specification as the date-only
     * value, and the result is the difference in days between the resultant
     * date and the date-only date.
     *
     * If both instances are date-only, the result is the difference in days
     * between the two dates, ignoring time zones.
     *
     * @param other other date/time
     * @return number of milliseconds difference
     * @see addSecs(), daysTo()
     */
    qint64 msecsTo(const KADateTime& other) const;

    /**
     * Returns the number of seconds from this date/time to the @p other date/time.
     *
     * Before performing the comparison, the two date/times are converted to UTC
     * to ensure that the result is correct if one of the two date/times has
     * daylight saving time (DST) and the other doesn't.
     *
     * If one instance is date-only and the other is date-time, the date-time
     * value is first converted to the same time specification as the date-only
     * value, and the result is the difference in days between the resultant
     * date and the date-only date.
     *
     * If both instances are date-only, the result is the difference in days
     * between the two dates, ignoring time zones.
     *
     * @param other other date/time
     * @return number of seconds difference
     * @see addSecs(), daysTo()
     */
    qint64 secsTo(const KADateTime& other) const;

    /**
     * Calculates the number of days from this date/time to the @p other date/time.
     * In calculating the result, @p other is first converted to this instance's
     * time zone. The number of days difference is then calculated ignoring
     * the time parts of the two date/times. For example, if this date/time
     * was 13:00 on 1 January 2000, and @p other was 02:00 on 2 January 2000,
     * the result would be 1.
     *
     * If one instance is date-only and the other is date-time, the date-time
     * value is first converted to the same time specification as the date-only
     * value, and the result is the difference in days between the resultant
     * date and the date-only date.
     *
     * If both instances are date-only, the calculation ignores time zones.
     *
     * @param other other date/time
     * @return number of days difference
     * @see secsTo(), addDays()
     */
    qint64 daysTo(const KADateTime& other) const;

    /**
     * Returns the current date and time, as reported by the system clock,
     * expressed in the local system time zone (type LocalZone).
     *
     * @return current date/time
     * @see currentUtcDateTime(), currentDateTime()
     */
    static KADateTime currentLocalDateTime();

    /**
     * Returns the current date and time, as reported by the system clock,
     * expressed in UTC.
     *
     * @return current date/time
     * @see currentLocalDateTime(), currentDateTime(), currentLocalDate(), currentLocalTime()
     */
    static KADateTime currentUtcDateTime();

    /**
     * Returns the current date and time, as reported by the system clock,
     * expressed in a given time specification.
     *
     * @note To fetch the current date and time expressed in UTC or in the local
     * system time zone, it is more efficient to use currentUtcDateTime() or
     * currentLocalDateTime().
     *
     * @param spec time specification
     * @return current date/time
     * @see currentUtcDateTime(), currentLocalDateTime()
     */
    static KADateTime currentDateTime(const Spec& spec);

    /**
     * Returns the current date in the local time zone, as reported by the
     * system clock.
     *
     * @return current date
     * @see currentLocalDateTime(), currentLocalTime()
     */
    static QDate currentLocalDate();

    /**
     * Returns the current time of day in the local time zone, as reported
     * by the system clock.
     *
     * @return current date
     * @see currentLocalDateTime(), currentLocalDate()
     */
    static QTime currentLocalTime();

    /**
     * Returns the date/time as a string. The @p format parameter determines the
     * format of the result string. The @p format codes used for the date and time
     * components follow those used elsewhere in KDE, and are similar but not
     * identical to those used by strftime(3). Conversion specifiers are
     * introduced by a '\%' character, and are replaced in @p format as follows:
     *
     * \b Date
     *
     * - \%y   2-digit year excluding century (00 - 99). Conversion is undefined
     *         if year < 0.
     * - \%Y   full year number
     * -  %:m  month number, without leading zero (1 - 12)
     * - \%m   month number, 2 digits (01 - 12)
     * - \%b   abbreviated month name in current locale
     * - \%B   full month name in current locale
     * -  %:b  abbreviated month name in English (Jan, Feb, ...)
     * -  %:B  full month name in English
     * - \%e   day of the month (1 - 31)
     * - \%d   day of the month, 2 digits (01 - 31)
     * - \%a   abbreviated weekday name in current locale
     * - \%A   full weekday name in current locale
     * -  %:a  abbreviated weekday name in English (Mon, Tue, ...)
     * -  %:A  full weekday name in English
     *
     * \b Time
     *
     * - \%H   hour in the 24 hour clock, 2 digits (00 - 23)
     * - \%k   hour in the 24 hour clock, without leading zero (0 - 23)
     * - \%I   hour in the 12 hour clock, 2 digits (01 - 12)
     * - \%l   hour in the 12 hour clock, without leading zero (1 - 12)
     * - \%M   minute, 2 digits (00 - 59)
     * - \%S   seconds (00 - 59)
     * -  %:S  seconds preceded with ':', but omitted if seconds value is zero
     * -  %:s  milliseconds, 3 digits (000 - 999)
     * - \%P   "am" or "pm" in the current locale, or if undefined there, in English
     * - \%p   "AM" or "PM" in the current locale, or if undefined there, in English
     * -  %:P  "am" or "pm"
     * -  %:p  "AM" or "PM"
     *
     * \b Time zone
     *
     * -  %:u  UTC offset of the time zone in hours, e.g. -02. If the offset
     *         is not a whole number of hours, the output is the same as for '\%U'.
     * - \%z   UTC offset of the time zone in hours and minutes, e.g. -0200.
     * -  %:z  UTC offset of the time zone in hours and minutes, e.g. +02:00.
     * - \%Z   time zone abbreviation, e.g. UTC, EDT, GMT. This is not guaranteed
     *         to be unique among different time zones. If not applicable (i.e. if
     *         the instance is type OffsetFromUTC), the UTC offset is substituted.
     * -  %:Z  time zone name, e.g. Europe/London. This is system dependent. If
     *         not applicable (i.e. if the instance is type OffsetFromUTC), the
     *         UTC offset is substituted.
     *
     * \b Other
     *
     * -  %%   literal '\%' character
     *
     * If you want to use the current locale's date format, you should call
     * QLocale::toString() to format the date part of the KADateTime.
     *
     * @param format format for the string
     * @return formatted string
     * @see fromString(), QLocale::toString()
     */
    QString toString(const QString& format) const;

    /**
     * Returns the date/time as a string, formatted according to the @p format
     * parameter, with the UTC offset appended.
     *
     * If the instance is date-only, the time will when @p format permits be
     * omitted from the output string. This applies to @p format = QtTextDate
     * or LocalDate. For all other cases, a time of 00:00:00 will be output.
     *
     * For RFC 2822 format, set @p format to RFCDateDay to include the day
     * of the week, or to RFCDate to omit it.
     *
     * @param format format for output string
     * @return formatted string
     * @see fromString(), QDateTime::toString()
     */
    QString toString(TimeFormat format = ISODate) const;

    /**
     * Returns the KADateTime represented by @p string, using the @p format given.
     *
     * This method is the inverse of toString(TimeFormat), except that it can
     * only return a time specification of UTC or OffsetFromUTC. An actual
     * named time zone cannot be returned since an offset from UTC only
     * partially specifies a time zone.
     *
     * The time specification of the result is determined by the UTC offset
     * present in the string:
     * - if the UTC offset is zero the result is type @c UTC.
     * - if the UTC offset is non-zero, the result is type @c OffsetFromUTC.
     * - if there is no UTC offset (when @p format permits this), the result is
     *   by default type @c LocalZone. You can use setFromStringDefault() to
     *   change this default.
     *
     * If no time is found in @p string, a date-only value is returned, except
     * when the specified @p format does not permit the time to be omitted, in
     * which case an error is returned. An error is therefore returned for
     * ISODate when @p string includes a time zone specification, and for
     * RFCDate in all cases.
     *
     * For RFC format strings (not RFC 3339), you should normally set @p format
     * to RFCDate. Only set it to RFCDateDay if you want to return an error
     * when the day of the week is omitted.
     *
     * @param string string to convert
     * @param format format code. LocalDate cannot be used here.
     * @param negZero if non-null, the value is set to true if a UTC offset of
     *                '-0000' is found or, for RFC 2822 format, an unrecognised
     *                or invalid time zone abbreviation is found, else false.
     * @return KADateTime value, or an invalid KADateTime if either parameter is invalid
     * @see setFromStringDefault(), toString(), QString::fromString()
     */
    static KADateTime fromString(const QString& string, TimeFormat format = ISODate, bool* negZero = nullptr);

    /**
     * Returns the KADateTime represented by @p string, using the @p format
     * given, optionally using a time zone collection @p zones as the source of
     * time zone definitions. The @p format codes are basically the same as
     * those for toString(), and are similar but not identical to those used by
     * strftime(3).
     *
     * The @p format string consists of the same codes as that for
     * toString(). However, some codes which are distinct in toString() have
     * the same function as each other here.
     *
     * Numeric values without a stated number of digits permit, but do not
     * require, leading zeroes. The maximum number of digits consumed by a
     * numeric code is the minimum needed to cover the possible range of the
     * number (e.g. for minutes, the range is 0 - 59, so the maximum number of
     * digits consumed is 2). All non-numeric values are case insensitive.
     *
     * \b Date
     *
     * - \%y   year excluding century (0 - 99). Years 0 - 50 return 2000 - 2050,
     *         while years 51 - 99 return 1951 - 1999.
     * - \%Y   full year number (4 digits with optional sign)
     * -  %:Y  full year number (>= 4 digits with optional sign)
     * -  %:m  month number (1 - 12)
     * - \%m   month number, 2 digits (01 - 12)
     * - \%b
     * - \%B   month name in the current locale or, if no match, in English,
     *         abbreviated or in full
     * -  %:b
     * -  %:B  month name in English, abbreviated or in full
     * - \%e   day of the month (1 - 31)
     * - \%d   day of the month, 2 digits (01 - 31)
     * - \%a
     * - \%A   weekday name in the current locale or, if no match, in English,
     *         abbreviated or in full
     * -  %:a
     * -  %:A  weekday name in English, abbreviated or in full
     *
     * \b Time
     *
     * - \%H   hour in the 24 hour clock, 2 digits (00 - 23)
     * - \%k   hour in the 24 hour clock (0 - 23)
     * - \%I   hour in the 12 hour clock, 2 digits (01 - 12)
     * - \%l   hour in the 12 hour clock (1 - 12)
     * - \%M   minute, 2 digits (00 - 59)
     * -  %:M  minute (0 - 59)
     * - \%S   seconds, 2 digits (00 - 59)
     * - \%s   seconds (0 - 59)
     * -  %:S  optional seconds value (0 - 59) preceded with ':'. If no colon is
     *         found in @p string, no input is consumed and the seconds value is
     *         set to zero.
     * -  %:s  fractional seconds value, preceded with a decimal point (either '.'
     *         or the locale's decimal point symbol)
     * - \%P
     * - \%p   "am" or "pm", in the current locale or, if no match, in
     *         English. This format is only useful when used with \%I or \%l.
     * -  %:P
     * -  %:p  "am" or "pm" in English. This format is only useful when used with
     *         \%I or \%l.
     *
     * \b Time zone
     *
     * -  %:u
     * - \%z   UTC offset of the time zone in hours and optionally minutes,
     *         e.g. -02, -0200.
     * -  %:z  UTC offset of the time zone in hours and minutes, colon separated,
     *         e.g. +02:00.
     * - \%Z   time zone abbreviation, consisting of alphanumeric characters,
     *         e.g. UTC, EDT, GMT. Use of time zone abbreviations is strongly
     *         advised against, for reasons given below.
     * -  %:Z  time zone name, e.g. Europe/London. The name may contain any
     *         characters and is delimited by the following character in the
     *         @p format string. It will not work if you follow %:Z with another
     *         escape sequence (except %% or \%t).
     *
     * \b Other
     *
     * - \%t   matches one or more whitespace characters
     * -  %%   literal '\%' character
     *
     * Any other character must have a matching character in @p string, except
     * that a space will match zero or more whitespace characters in the input
     * string.
     *
     * If any time zone information is present in the string, the function
     * attempts to find a matching time zone in the @p zones collection. A time
     * zone name (format code %:Z) will provide an unambiguous look up in
     * @p zones. Any other type of time zone information (an abbreviated time
     * zone code (\%Z) or UTC offset (\%z, %:z, %:u) is searched for in @p zones
     * and if only one time zone is found to match, the result is set to that
     * zone. Otherwise:
     * - If more than one match of a UTC offset is found, the action taken is
     *   determined by @p offsetIfAmbiguous: if @p offsetIfAmbiguous is true,
     *   a local time with an offset from UTC (type @c OffsetFromUTC) will be
     *   returned; if false an invalid KADateTime is returned.
     * - If more than one match of a time zone abbreviation is found, the UTC
     *   offset for each matching time zone is compared and, if the offsets are
     *   the same, a local time with an offset from UTC (type @c OffsetFromUTC)
     *   will be returned provided that @p offsetIfAmbiguous is true. Otherwise
     *   an invalid KADateTime is returned.
     * - If a time zone abbreviation does not match any time zone in @p zones,
     *   or the abbreviation does not apply at the parsed date/time, an
     *   invalid KADateTime is returned.
     *   Warning: To avoid unacceptable processing loads, searches for a time
     *            zone abbreviation are done using QTimeZone::displayName()
     *            instead of QTimeZone::abbreviation(). However, the two
     *            methods can return different results, and if this happens, no
     *            match will be found. For this reason as well as the fact that
     *            time zone abbreviations are not unique, the use of time zone
     *            abbreviations is strongly advised against.
     * - If a time zone name does not match any time zone in @p zones, an
     *   invalid KADateTime is returned.
     * - If the time zone UTC offset does not match any time zone in @p zones,
     *   a local time with an offset from UTC (type @c OffsetFromUTC) is
     *   returned.
     * If @p format contains more than one time zone or UTC offset code, an
     * error is returned.
     *
     * If no time zone information is present in the string, by default a local
     * time (type @c LocalZone) is returned. You can use setFromStringDefault()
     * to change this default.
     *
     * If no time is found in @p string, a date-only value is returned.
     *
     * If any inconsistencies are found, i.e. the same item of information
     * appears more than once but with different values, the weekday name does
     * not tally with the date, an invalid KADateTime is returned.
     *
     * @param string string to convert
     * @param format format string
     * @param zones time zone collection, or null for none
     * @param offsetIfAmbiguous specifies what to do if more than one zone
     *                          matches the UTC offset found in the
     *                          string. Ignored if @p zones is null.
     * @return KADateTime value, or an invalid KADateTime if an error occurs, if
     *         time zone information doesn't match any in @p zones, or if the
     *         time zone information is ambiguous and @p offsetIfAmbiguous is
     *         false
     * @see setFromStringDefault(), toString()
     */
    static KADateTime fromString(const QString& string, const QString& format,
                                const QList<QTimeZone>* zones = nullptr, bool offsetIfAmbiguous = true);

    /**
     * Sets the default time specification for use by fromString() when no time
     * zone or UTC offset is found in the string being parsed, or when "-0000"
     * is found in an RFC 2822 string.
     *
     * By default, fromString() returns a local time (type @c LocalZone) when
     * no definite zone or UTC offset is found. You can use this method to make
     * it return UTC, or whatever you wish.
     *
     * @param spec the new default time specification
     * @see fromString()
     */
    static void setFromStringDefault(const Spec& spec);

    /**
     * Compare this instance with another to determine whether they are
     * simultaneous, earlier or later, and in the case of date-only values,
     * whether they overlap (i.e. partly coincide but are not wholly
     * simultaneous).
     * The comparison takes time zones into account: if the two instances have
     * different time zones, they are first converted to UTC before comparing.
     *
     * If both instances are date/time values, this instance is considered to
     * be either simultaneous, earlier or later, and does not overlap.
     *
     * If one instance is date-only and the other is a date/time, this instance
     * is either strictly earlier, strictly later, or overlaps.
     *
     * If both instance are date-only, they are considered simultaneous if both
     * their start of day and end of day times are simultaneous with each
     * other. (Both start and end of day times need to be considered in case a
     * daylight savings change occurs during that day.) Otherwise, this instance
     * can be strictly earlier, earlier but overlapping, later but overlapping,
     * or strictly later.
     *
     * @return @c true if the two instances represent the same time, @c false otherwise
     * @see operator==(), operator!=(), operator<(), operator<=(), operator>=(), operator>()
     */
    Comparison compare(const KADateTime& other) const;

    /**
     * Check whether this date/time is simultaneous with another.
     * The comparison takes time zones into account: if the two instances have
     * different time zones, they are first converted to UTC before comparing.
     *
     * If one instance is date-only and the other is date/time, they are
     * considered unequal.
     *
     * If both instances are date-only, they are considered simultaneous if both
     * their start of day and end of day times are simultaneous with each
     * other. (Both start and end of day times need to be considered in case a
     * daylight saving change occurs during that day.)
     *
     * @return @c true if the two instances represent the same time, @c false otherwise
     * @see compare()
     */
    bool operator==(const KADateTime& other) const;

    bool operator!=(const KADateTime& other) const
    {
        return !(*this == other);
    }

    /**
     * Check whether this date/time is earlier than another.
     * The comparison takes time zones into account: if the two instances have
     * different time zones, they are first converted to UTC before comparing.
     *
     * If one or both instances are date-only, the comparison returns true if
     * this date/time or day, falls wholly before the other date/time or
     * day. To achieve this, the time used in the comparison is the end of day
     * (if this instance is date-only) or the start of day (if the other
     * instance is date-only).
     *
     * @return @c true if this instance represents an earlier time than @p other,
     *         @c false otherwise
     * @see compare()
     */
    bool operator<(const KADateTime& other) const;

    bool operator<=(const KADateTime& other) const
    {
        return !(other < *this);
    }
    bool operator>(const KADateTime& other) const
    {
        return other < *this;
    }
    bool operator>=(const KADateTime& other) const
    {
        return !(*this < other);
    }

    /**
     * Create a separate copy of this instance's data if it is implicitly shared
     * with another instance.
     *
     * You would normally only call this if you want different copies of the
     * same date/time value to cache conversions to different time zones. Because
     * only the last conversion to another time zone is cached, and the cached
     * value is implicitly shared, judicious use of detach() could improve
     * efficiency when handling several time zones. But take care: if used
     * inappropriately, it will reduce efficiency!
     */
    void detach();

    /**
     * Set an adjustment to be applied when fetching the current system time.
     * This is applied by all KADateTime methods which return the system date
     * and/or time.
     *
     * The supplied date/time is used as the current simulated time and the
     * time adjustment is set to the difference between the real current time
     * and @p newTime. If @p newTime has a time zone, that time zone is set
     * to be the simulated local system time zone.
     *
     * To cancel time simulation, supply an invalid @p newTime parameter.
     *
     * @warning This function is provided only for testing purposes, and should
     *          not be used in released code. If the library is compiled without
     *          debug enabled, setSimulatedSystemTime() has no effect.
     *          To avoid confusion, it is recommended that calls to it should be
     *          conditionally compiled, e.g.:
     *          \code
     *          #ifndef NDEBUG
     *             KADateTime::simulateSystemTime(kdt);
     *          #endif
     *          \endcode
     *
     * @param newTime the current simulated time, or invalid to cancel simulation
     *
     * @see currentDateTime(), currentLocalDateTime(), currentUtcDateTime(),
     *      currentLocalDate(), currentLocalTime()
     */
    static void setSimulatedSystemTime(const KADateTime& newTime);

    /**
     * Return the real (not simulated) system time.
     *
     * @warning This method is provided only for testing purposes, and should
     *          not be used in released code. If the library is compiled without
     *          debug enabled, currentLocalDateTime() and realCurrentLocalDateTime()
     *          both return the real system time.
     *          To avoid confusion, it is recommended that calls to
     *          realCurrentLocalDateTime() should be conditionally compiled, e.g.:
     *          \code
     *          #ifndef NDEBUG
     *             dt = KADateTime::realCurrentLocalDateTime();
     *          #endif
     *          \endcode
     *
     */
    static KADateTime realCurrentLocalDateTime();

private:
    QSharedDataPointer<KADateTimePrivate> d;
};

/** Write @p spec to the datastream @p out, in binary format. */
KALARMCAL_EXPORT QDataStream& operator<<(QDataStream& out, const KADateTime::Spec& spec);
/** Read a KADateTime::Spec object into @p spec from @p in, in binary format. */
KALARMCAL_EXPORT QDataStream& operator>>(QDataStream& in, KADateTime::Spec& spec);

/** Write @p dateTime to the datastream @p out, in binary format. */
KALARMCAL_EXPORT QDataStream& operator<<(QDataStream& out, const KADateTime& dateTime);
/** Read a KADateTime object into @p dateTime from @p in, in binary format. */
KALARMCAL_EXPORT QDataStream& operator>>(QDataStream& in, KADateTime& dateTime);

} // namespace KAlarmCal

Q_DECLARE_METATYPE(KAlarmCal::KADateTime)
Q_DECLARE_METATYPE(KAlarmCal::KADateTime::Spec)

// vim: et sw=4:
