/*
 *  karecurrence.h  -  recurrence with special yearly February 29th handling
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  Copyright © 2005-2012 by David Jarvie <djarvie@kde.org>
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

#ifndef KALARM_KARECURRENCE_H
#define KALARM_KARECURRENCE_H

#include "kalarmcal_export.h"

#include <kcalcore/recurrencerule.h>
#include <kcalcore/duration.h>
namespace KCalCore { class Recurrence; }

#include <QtCore/QBitArray>

namespace KAlarmCal
{

/**
 * @short Represents recurrences for KAlarm.
 *
 * This class represents the restricted range of recurrence types which are
 * handled by KAlarm, and translates between these and the libkcalcore
 * Recurrence class. In particular, it handles yearly recurrences on 29th
 * February in non-leap years specially:
 *
 * KARecurrence allows annual 29th February recurrences to fall on 28th
 * February or 1st March, or not at all, in non-leap years. It allows such
 * 29th February recurrences to be combined with the 29th of other months in
 * a simple way, represented simply as the 29th of multiple months including
 * February. For storage in the libkcalcore calendar, the 29th day of the month
 * recurrence for other months is combined with a last-day-of-February or a
 * 60th-day-of-the-year recurrence rule, thereby conforming to RFC2445.
 *
 * @author David Jarvie <djarvie@kde.org>
 */
class KALARMCAL_EXPORT KARecurrence
{
    public:
        /** The recurrence's period type.
         *  This is a subset of the possible KCalCore recurrence types.
         */
        enum Type
        {
            NO_RECUR,      //!< does not recur
            MINUTELY,      //!< at an hours/minutes interval
            DAILY,         //!< daily
            WEEKLY,        //!< weekly, on specified weekdays
            MONTHLY_POS,   //!< monthly, on specified weekdays in a specified week of the month
            MONTHLY_DAY,   //!< monthly, on a specified day of the month
            ANNUAL_DATE,   //!< yearly, on a specified date in each of the specified months
            ANNUAL_POS     //!< yearly, on specified weekdays in the specified weeks of the specified months
        };
        /** When annual February 29th recurrences should occur in non-leap years. */
        enum Feb29Type
        {
            Feb29_Feb28,   //!< occurs on 28 February in non-leap years
            Feb29_Mar1,    //!< occurs on 1 March in non-leap years
            Feb29_None     //!< does not occur in non-leap years
        };

        KARecurrence();
        KARecurrence(const KCalCore::Recurrence& r);
        KARecurrence(const KARecurrence& r);
        ~KARecurrence();
        /**
         * Assignment operator.
         * @param r the recurrence which will be assigned to this.
         */
        KARecurrence& operator=(const KARecurrence& r);

        /**
         * Comparison operator for equality.
         * @param r instance to compare with
         * @return true if recurrences are the same, false otherwise
         */
        bool operator==(const KARecurrence& r) const;

        /**
         * Comparison operator for inequality.
         * @param r instance to compare with
         * @return true if recurrences are the different, false if the same
         */
        bool operator!=(const KARecurrence& r) const  { return !operator==(r); }

        /** Initialise the recurrence from an iCalendar RRULE string.
         *  @return true if successful, false if an error occurred.
         */
        bool set(const QString& icalRRULE);

        /** Set up a KARecurrence from recurrence parameters, using the start date to
         *  determine the recurrence day/month as appropriate.
         *  Annual 29th February recurrences in non-leap years will be handled
         *  according to the default set by setDefaultFeb29Type().
         *  Only a restricted subset of recurrence types is allowed: minutely, daily,
         *  weekly, monthly, yearly or none.
         *  @return true if successful.
         */
        bool set(Type t, int freq, int count, const KDateTime& start, const KDateTime& end);

        /** Set up a KARecurrence from recurrence parameters, using the start date to
         *  determine the recurrence day/month as appropriate, and specifying how
         *  annual 29th February recurrences in non-leap years should be handled.
         *  @return true if successful.
         */
        bool set(Type t, int freq, int count, const KDateTime& start, const KDateTime& end, Feb29Type f29);

        /** Set up a KARecurrence from recurrence parameters.
         *  Annual 29th February recurrences in non-leap years will be handled
         *  according to the default set by setDefaultFeb29Type().
         *  Only a restricted subset of recurrence types is allowed: minutely, daily,
         *  weekly, monthly, yearly or none.
         *  @return true if successful.
         */
        bool init(KCalCore::RecurrenceRule::PeriodType t, int freq, int count, const KDateTime& start, const KDateTime& end);

        /** Set up a KARecurrence from recurrence parameters, specifying how
         *  annual 29th February recurrences in non-leap years should be handled.
         *  Only a restricted subset of recurrence types is allowed: minutely, daily,
         *  weekly, monthly, yearly or none.
         *  @return true if successful.
         */
        bool init(KCalCore::RecurrenceRule::PeriodType t, int freq, int count, const KDateTime& start, const KDateTime& end, Feb29Type f29);

        /** Removes all recurrence and exception rules and dates. */
        void clear();

        /** Initialise a KCalCore::Recurrence to be the same as this instance.
         *  Additional recurrence rules are created as necessary if it recurs on Feb 29th.
         */
        void writeRecurrence(KCalCore::Recurrence&) const;

        /** Convert the recurrence to KARecurrence types.
         *  Must be called after presetting with a KCalCore::Recurrence.
         *  - Convert hourly recurrences to minutely.
         *  - Remove all but the first day in yearly date recurrences.
         *  - Check for yearly recurrences falling on February 29th and adjust them as
         *    necessary. A 29th of the month rule can be combined with either a 60th day
         *    of the year rule or a last day of February rule.
         */
        void fix();

        /** Return the start date/time of the recurrence (Time for all-day recurrences will be 0:00).
         * @return the current start/time of the recurrence.
         */
        KDateTime startDateTime() const;
        /** Return the start date/time of the recurrence */
        QDate startDate() const;

        /** Set the recurrence start date/time, and optionally set it to all-day.
         *  @param dt        start date/time.
         *  @param dateOnly  if true, sets the recurrence to all-day.
         */
        void        setStartDateTime(const KDateTime& dt, bool dateOnly);

        /** Return the date/time of the last recurrence. */
        KDateTime   endDateTime() const;

        /** Return the date of the last recurrence. */
        QDate       endDate() const;

    /** Sets the date of the last recurrence. The end time is set to the recurrence start time.
     * @param endDate the ending date after which to stop recurring. If the
     *   recurrence is not all-day, the end time will be 23:59.
     */
    void setEndDate(const QDate& endDate);

    /** Sets the date and time of the last recurrence.
     * @param endDateTime the ending date/time after which to stop recurring.
     */
    void setEndDateTime(const KDateTime& endDateTime);

    /** Set whether the recurrence has no time, just a date.
     * All-day means -- according to rfc2445 -- that the event has no time
     * associated.
     * N.B. This property is derived by default from whether setStartDateTime() is
     * called with a date-only or date/time parameter.
     * @return whether the recurrence has a time (false) or it is just a date (true).
     */
    bool allDay() const;

    /** Set if recurrence is read-only or can be changed. */
    void setRecurReadOnly(bool readOnly);

    /** Returns true if the recurrence is read-only, or false if it can be changed. */
    bool recurReadOnly() const;

    /** Returns whether the event recurs at all. */
    bool recurs() const;

    /** Returns week day mask (bit 0 = Monday). */
    QBitArray days() const; // Emulate the old behavior

    /** Returns list of day positions in months. */
    QList<KCalCore::RecurrenceRule::WDayPos> monthPositions() const;

    /** Returns list of day numbers of a  month. */
    // Emulate old behavior
    QList<int> monthDays() const;

    /** Returns the day numbers within a yearly recurrence.
     * @return the days of the year for the event. E.g. if the list contains
     *         60, this means the recurrence happens on day 60 of the year, i.e.
     *         on Feb 29 in leap years and March 1 in non-leap years.
     */
    QList<int> yearDays() const;

    /** Returns the dates within a yearly recurrence.
     * @return the days of the month for the event. E.g. if the list contains
     *         13, this means the recurrence happens on the 13th of the month.
     *         The months for the recurrence can be obtained through
     *         yearlyMonths(). If this list is empty, the month of the start
     *         date is used.
     */
    QList<int> yearDates() const;

    /** Returns the months within a yearly recurrence.
     * @return the months for the event. E.g. if the list contains
     *         11, this means the recurrence happens in November.
     *         The days for the recurrence can be obtained either through
     *         yearDates() if they are given as dates within the month or
     *         through yearlyPositions() if they are given as positions within the
     *         month. If none is specified, the date of the start date is used.
     */
    QList<int> yearMonths() const;

    /** Returns the positions within a yearly recurrence.
     * @return the positions for the event, either within a month (if months
     *         are set through addYearlyMonth()) or within the year.
     *         E.g. if the list contains {Pos=3, Day=5}, this means the third
     *         friday. If a month is set this position is understoodas third
     *         Friday in the given months, otherwise as third Friday of the
     *         year.
     */
    QList<KCalCore::RecurrenceRule::WDayPos> yearPositions() const;

    /** Adds days to the weekly day recurrence list.
     * @param days a 7 bit array indicating which days on which to recur (bit 0 = Monday).
     */
    void addWeeklyDays(const QBitArray& days);

    /** Adds day number of year within a yearly recurrence.
     *  By default infinite recurrence is used. To set an end date use the
     *  method setEndDate and to set the number of occurrences use setDuration.
     * @param day the day of the year for the event. E.g. if day is 60, this
     *            means Feb 29 in leap years and March 1 in non-leap years.
     */
    void addYearlyDay(int day);

    /** Adds date within a yearly recurrence. The month(s) for the recurrence
     *  can be specified with addYearlyMonth(), otherwise the month of the
     *  start date is used.
     *
     *   By default infinite recurrence is used. To set an end date use the
     *   method setEndDate and to set the number of occurrences use setDuration.
     * @param date the day of the month for the event
     */
    void addYearlyDate(int date);

    /** Adds month in yearly recurrence. You can specify specific day numbers
     *  within the months (by calling addYearlyDate()) or specific day positions
     *  within the month (by calling addYearlyPos).
     * @param month the month in which the event will recur.
     */
    void addYearlyMonth(short month);

    /** Adds position within month/year within a yearly recurrence. If months
     *  are specified (via addYearlyMonth()), the parameters are understood as
     *  position within these months, otherwise within the year.
     *
     *  By default infinite recurrence is used.
     *   To set an end date use the method setEndDate and to set the number
     *   of occurrences use setDuration.
     * @param pos the position in the month/year for the recurrence, with valid
     * values being 1 to 53 and -1 to -53 (53 weeks max in a year).
     * @param days the days for the position to recur on (bit 0 = Monday).
     * Example: pos = 2, and bits 0 and 2 are set in days
     *   If months are specified (via addYearlyMonth), e.g. March, the rule is
     *   to repeat every year on the 2nd Monday and Wednesday of March.
     *   If no months are specified, the fule is to repeat every year on the
     *   2nd Monday and Wednesday of the year.
     */
    void addYearlyPos(short pos, const QBitArray& days);

    /** Adds a position (e.g. first monday) to the monthly recurrence rule.
     * @param pos the position in the month for the recurrence, with valid
     * values being 1-5 (5 weeks max in a month).
     * @param days the days for the position to recur on (bit 0 = Monday).
     * Example: pos = 2, and bits 0 and 2 are set in days:
     * the rule is to repeat every 2nd Monday and Wednesday in the month.
     */
    void addMonthlyPos(short pos, const QBitArray& days);
    void addMonthlyPos(short pos, ushort day);

    /** Adds a date (e.g. the 15th of each month) to the monthly day
     *  recurrence list.
     * @param day the date in the month to recur.
     */
    void addMonthlyDate(short day);

        /** Get the next time the recurrence occurs, strictly after a specified time. */
        KDateTime   getNextDateTime(const KDateTime& preDateTime) const;

        /** Get the previous time the recurrence occurred, strictly before a specified time. */
        KDateTime   getPreviousDateTime(const KDateTime& afterDateTime) const;

        /** Return whether the event will recur on the specified date.
         *  The start date only returns true if it matches the recurrence rules. */
        bool        recursOn(const QDate&, const KDateTime::Spec&) const;

    /**
     * Returns true if the date/time specified is one at which the event will
     * recur. Times are rounded down to the nearest minute to determine the result.
     *
     * @param dt is the date/time to check.
     */
    bool recursAt(const KDateTime& dt) const;

    /** Returns a list of the times on the specified date at which the
     * recurrence will occur. The returned times should be interpreted in the
     * context of @p timeSpec.
     * @param date the date for which to find the recurrence times
     * @param timeSpec time specification for @p date
     */
    KCalCore::TimeList recurTimesOn(const QDate& date, const KDateTime::Spec& timeSpec) const;

    /** Returns a list of all the times at which the recurrence will occur
     * between two specified times.
     *
     * There is a (large) maximum limit to the number of times returned. If due to
     * this limit the list is incomplete, this is indicated by the last entry being
     * set to an invalid KDateTime value. If you need further values, call the
     * method again with a start time set to just after the last valid time returned.
     *
     * @param start inclusive start of interval
     * @param end inclusive end of interval
     * @return list of date/time values
     */
    KCalCore::DateTimeList timesInInterval(const KDateTime& start, const KDateTime& end) const;

    /** Returns frequency of recurrence, in terms of the recurrence time period type. */
    int frequency() const;

    /** Sets the frequency of recurrence, in terms of the recurrence time period type. */
    void setFrequency(int freq);

    /**
     * Returns -1 if the event recurs infinitely, 0 if the end date is set,
     * otherwise the total number of recurrences, including the initial occurrence.
     */
    int duration() const;

    /** Sets the total number of times the event is to occur, including both the
     * first and last.
     */
    void setDuration(int duration);

    /** Returns the number of recurrences up to and including the date/time specified.
     *  @warning This function can be very time consuming - use it sparingly!
     */
    int durationTo(const KDateTime& dt) const;

    /** Returns the number of recurrences up to and including the date specified.
     *  @warning This function can be very time consuming - use it sparingly!
     */
    int durationTo(const QDate& date) const;

        /** Return the longest interval between recurrences.
         *  @return  0 if it never recurs.
         */
        KCalCore::Duration longestInterval() const;

        /** Return the interval between recurrences, if the interval between
         *  successive occurrences does not vary.
         *  @return  0 if recurrence does not occur at fixed intervals.
         */
        KCalCore::Duration regularInterval() const;
    KCalCore::DateTimeList exDateTimes() const;
    KCalCore::DateList exDates() const;
    void setExDateTimes(const KCalCore::DateTimeList& exdates);
    void setExDates(const KCalCore::DateList& exdates);
    void addExDateTime(const KDateTime& exdate);
    void addExDate(const QDate& exdate);

    /**
     * Shift the times of the recurrence so that they appear at the same clock
     * time as before but in a new time zone. The shift is done from a viewing
     * time zone rather than from the actual recurrence time zone.
     *
     * For example, shifting a recurrence whose start time is 09:00 America/New York,
     * using an old viewing time zone (@p oldSpec) of Europe/London, to a new time
     * zone (@p newSpec) of Europe/Paris, will result in the time being shifted
     * from 14:00 (which is the London time of the recurrence start) to 14:00 Paris
     * time.
     *
     * @param oldSpec the time specification which provides the clock times
     * @param newSpec the new time specification
     */
    void shiftTimes(const KDateTime::Spec& oldSpec, const KDateTime::Spec& newSpec);

    KCalCore::RecurrenceRule* defaultRRuleConst() const;
        /** Return the recurrence's period type. */
        Type type() const;

        /** Return the type of a recurrence rule. */
        static Type type(const KCalCore::RecurrenceRule*);

        /** Check if the recurrence rule is a daily rule with or without BYDAYS specified. */
        static bool dailyType(const KCalCore::RecurrenceRule*);

        /** Return when 29th February annual recurrences should occur in non-leap years. */
        Feb29Type feb29Type() const;

        /** Return the default way that 29th February annual recurrences should occur
         *  in non-leap years.
         *  @see setDefaultFeb29Type().
         */
        static Feb29Type defaultFeb29Type();

        /** Set the default way that 29th February annual recurrences should occur
         *  in non-leap years.
         *  @see defaultFeb29Type().
         */
        static void setDefaultFeb29Type(Feb29Type t);

    private:
        //@cond PRIVATE
        class Private;
        Private* const d;
        //@endcond
};

} // namespace KAlarmCal

#endif // KALARM_KARECURRENCE_H

// vim: et sw=4:
