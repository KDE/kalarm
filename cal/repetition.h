/*
 *  repetition.h  -  represents a sub-repetition: interval and count
 *  Program:  kalarm
 *  Copyright © 2009,2011 by David Jarvie <djarvie@kde.org>
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

#ifndef KALARM_REPETITION_H
#define KALARM_REPETITION_H

#include "kalarm_cal_export.h"
#ifdef USE_AKONADI
#include <kcalcore/duration.h>
#else
#include <kcal/duration.h>
#endif

class KDateTime;

namespace KAlarm
{

/**
 * @short Represents a sub-repetition, defined by interval and repeat count.
 *
 *  The Repetition class represents a sub-repetition, storing its interval
 *  and repeat count. The repeat count is the number of repetitions after
 *  the first occurrence.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class KALARM_CAL_EXPORT Repetition
{
    public:
        /** Default constructor.
         *  Initialises to no repetition.
         */
        Repetition();

#ifdef USE_AKONADI
        /** Constructor.
         *  Initialises with the specified @p interval and @p count.
         */
        Repetition(const KCalCore::Duration& interval, int count);
#else
        /** Constructor.
         *  Initialises with the specified @p interval and @p count.
         */
        Repetition(const KCal::Duration& interval, int count);
#endif

        Repetition(const Repetition& other);

        ~Repetition();

        Repetition& operator=(const Repetition& other);

#ifdef USE_AKONADI
        /** Initialises the instance with the specified @p interval and @p count. */
        void set(const KCalCore::Duration& interval, int count);
#else
        /** Initialises the instance with the specified @p interval and @p count. */
        void set(const KCal::Duration& interval, int count);
#endif

#ifdef USE_AKONADI
        /** Sets the @p interval. The repetition count is unchanged unless
         *  The repetition count is set to zero if @p interval is zero; otherwise
         *  the repetition count is unchanged.
         */
        void set(const KCalCore::Duration& interval);
#else
        /** Sets the @p interval. The repetition count is unchanged unless
         *  The repetition count is set to zero if @p interval is zero; otherwise
         *  the repetition count is unchanged.
         */
        void set(const KCal::Duration& interval);
#endif

        /** Returns whether a repetition is defined.
         *  @return true if a repetition is defined, false if not.
         */
        operator bool() const;

        /** Returns whether no repetition is defined.
         *  @return false if a repetition is defined, true if not.
         */
        bool operator!() const                      { return !operator bool(); }

        bool operator==(const Repetition& r) const;
        bool operator!=(const Repetition& r) const  { return !operator==(r); }

        /** Return the number of repetitions. */
        int count() const;

#ifdef USE_AKONADI
        /** Return the interval between repetitions. */
        const KCalCore::Duration& interval() const;

        /** Return the overall duration of the repetition. */
        KCalCore::Duration duration() const;

        /** Return the overall duration of a specified number of repetitions.
         *  @param count the number of repetitions to find the duration of.
         */
        KCalCore::Duration duration(int count) const;
#else
        /** Return the interval between repetitions. */
        const KCal::Duration& interval() const;

        /** Return the overall duration of the repetition. */
        KCal::Duration duration() const;

        /** Return the overall duration of a specified number of repetitions.
         *  @param count the number of repetitions to find the duration of.
         */
        KCal::Duration duration(int count) const;
#endif

        /** Check whether the repetition interval is in terms of days (as opposed to minutes). */
        bool isDaily() const;

        /** Return the repetition interval in terms of days.
         *  If necessary, the interval is rounded down to a whole number of days.
         */
        int intervalDays() const;

        /** Return the repetition interval in terms of minutes.
         *  If necessary, the interval is rounded down to a whole number of minutes.
         */
        int intervalMinutes() const;

        /** Return the repetition interval in terms of seconds. */
        int intervalSeconds() const;

        /** Find the repetition count for the next repetition after a specified time.
         *  @param from         repetition start time, which should not be a date-only value
         *  @param preDateTime  time after which the desired repetition occurs
         */
        int nextRepeatCount(const KDateTime& from, const KDateTime& preDateTime) const;

        /** Find the repetition count for the last repetition before a specified time.
         *  @param from           repetition start time, which should not be a date-only value
         *  @param afterDateTime  time after which the desired repetition occurs
         */
        int previousRepeatCount(const KDateTime& from, const KDateTime& afterDateTime) const;

    private:
        //@cond PRIVATE
        class Private;
        Private* const d;
        //@endcond
};

} // namespace KAlarm

#endif // KALARM_REPETITION_H

// vim: et sw=4:
