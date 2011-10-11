/*
 *  karecurrence.h  -  recurrence with special yearly February 29th handling
 *  Program:  kalarm
 *  Copyright © 2005-2011 by David Jarvie <djarvie@kde.org>
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

#ifndef KARECURRENCE_H
#define KARECURRENCE_H

#include "kalarm_cal_export.h"

#ifdef USE_AKONADI
#include <kcalcore/recurrence.h>
#include <kcalcore/duration.h>
#else
#include <kcal/recurrence.h>
#include <kcal/duration.h>
#endif


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
#ifdef USE_AKONADI
class KALARM_CAL_EXPORT KARecurrence : public KCalCore::Recurrence
#else
class KALARM_CAL_EXPORT KARecurrence : public KCal::Recurrence
#endif
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
#ifdef USE_AKONADI
        KARecurrence(const KCalCore::Recurrence& r);
#else
        KARecurrence(const KCal::Recurrence& r);
#endif
        KARecurrence(const KARecurrence& r);
        ~KARecurrence();
        KARecurrence& operator=(const KARecurrence& r);

        /** Initialise the recurrence from an iCalendar RRULE string. */
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
#ifdef USE_AKONADI
        bool init(KCalCore::RecurrenceRule::PeriodType t, int freq, int count, const KDateTime& start, const KDateTime& end);
#else
        bool init(KCal::RecurrenceRule::PeriodType t, int freq, int count, const KDateTime& start, const KDateTime& end);
#endif

        /** Set up a KARecurrence from recurrence parameters, specifying how
         *  annual 29th February recurrences in non-leap years should be handled.
         *  Only a restricted subset of recurrence types is allowed: minutely, daily,
         *  weekly, monthly, yearly or none.
         *  @return true if successful.
         */
#ifdef USE_AKONADI
        bool init(KCalCore::RecurrenceRule::PeriodType t, int freq, int count, const KDateTime& start, const KDateTime& end, Feb29Type f29);
#else
        bool init(KCal::RecurrenceRule::PeriodType t, int freq, int count, const KDateTime& start, const KDateTime& end, Feb29Type f29);
#endif

        /** Initialise a KCalCore::Recurrence to be the same as this instance.
         *  Additional recurrence rules are created as necessary if it recurs on Feb 29th.
         */
#ifdef USE_AKONADI
        void writeRecurrence(KCalCore::Recurrence&) const;
#else
        void writeRecurrence(KCal::Recurrence&) const;
#endif

        /** Convert the recurrence to KARecurrence types.
         *  Must be called after presetting with a KCalCore::Recurrence.
         *  - Convert hourly recurrences to minutely.
         *  - Remove all but the first day in yearly date recurrences.
         *  - Check for yearly recurrences falling on February 29th and adjust them as
         *    necessary. A 29th of the month rule can be combined with either a 60th day
         *    of the year rule or a last day of February rule.
         */
        void fix();

        /** Set the recurrence start date/time, and optionally set it to all-day.
         *  @param dt        start date/time.
         *  @param dateOnly  if true, sets the recurrence to all-day.
         */
        void        setStartDateTime(const KDateTime& dt, bool dateOnly);

        /** Return the date/time of the last recurrence. */
        KDateTime   endDateTime() const;

        /** Return the date of the last recurrence. */
        QDate       endDate() const;

        /** Get the next time the recurrence occurs, strictly after a specified time. */
        KDateTime   getNextDateTime(const KDateTime& preDateTime) const;

        /** Get the previous time the recurrence occurred, strictly before a specified time. */
        KDateTime   getPreviousDateTime(const KDateTime& afterDateTime) const;

        /** Return whether the event will recur on the specified date.
         *  The start date only returns true if it matches the recurrence rules. */
        bool        recursOn(const QDate&, const KDateTime::Spec&) const;

        /** Return the longest interval between recurrences.
         *  @return  0 if it never recurs.
         */
#ifdef USE_AKONADI
        KCalCore::Duration longestInterval() const;
#else
        KCal::Duration longestInterval() const;
#endif

        /** Return the interval between recurrences, if the interval between
         *  successive occurrences does not vary.
         *  @return  0 if recurrence does not occur at fixed intervals.
         */
#ifdef USE_AKONADI
        KCalCore::Duration regularInterval() const;
#else
        KCal::Duration regularInterval() const;
#endif

        /** Return the recurrence's period type. */
        Type type() const;

        /** Return the type of a recurrence rule. */
#ifdef USE_AKONADI
        static Type type(const KCalCore::RecurrenceRule*);
#else
        static Type type(const KCal::RecurrenceRule*);
#endif

        /** Check if the recurrence rule is a daily rule with or without BYDAYS specified. */
#ifdef USE_AKONADI
        static bool dailyType(const KCalCore::RecurrenceRule*);
#else
        static bool dailyType(const KCal::RecurrenceRule*);
#endif

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
        /** Override Recurrence::setAllDay() to try to prevent public use:
         *  KARecurrence::setStartDateTime() handles all-day setting. */
        void setAllDay(bool);

        //@cond PRIVATE
        class Private;
        Private* const d;
        //@endcond
};

#endif // KARECURRENCE_H

// vim: et sw=4:
