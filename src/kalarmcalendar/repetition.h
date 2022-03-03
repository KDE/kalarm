/*
 *  repetition.h  -  represents a sub-repetition: interval and count
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  SPDX-FileCopyrightText: 2009-2012, 2018 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "kalarmcal_export.h"
#include <KCalendarCore/Duration>

namespace KAlarmCal
{

class KADateTime;

/**
 * @short Represents a sub-repetition, defined by interval and repeat count.
 *
 *  The Repetition class represents a sub-repetition, storing its interval
 *  and repeat count. The repeat count is the number of repetitions after
 *  the first occurrence.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class KALARMCAL_EXPORT Repetition
{
public:
    /** Default constructor.
     *  Initialises to no repetition.
     */
    Repetition();

    /** Constructor.
     *  Initialises with the specified @p interval and @p count.
     */
    Repetition(const KCalendarCore::Duration &interval, int count);

    Repetition(const Repetition &other);

    ~Repetition();

    Repetition &operator=(const Repetition &other);

    /** Initialises the instance with the specified @p interval and @p count. */
    void set(const KCalendarCore::Duration &interval, int count);
    /** Sets the @p interval. The repetition count is unchanged unless
     *  The repetition count is set to zero if @p interval is zero; otherwise
     *  the repetition count is unchanged.
     */
    void set(const KCalendarCore::Duration &interval);

    /** Returns whether a repetition is defined.
     *  @return true if a repetition is defined, false if not.
     */
    operator bool() const;

    /** Returns whether no repetition is defined.
     *  @return false if a repetition is defined, true if not.
     */
    bool operator!() const
    {
        return !operator bool();
    }

    bool operator==(const Repetition &r) const;
    bool operator!=(const Repetition &r) const
    {
        return !operator==(r);
    }

    /** Return the number of repetitions. */
    int count() const;

    /** Return the interval between repetitions. */
    KCalendarCore::Duration interval() const;

    /** Return the overall duration of the repetition. */
    KCalendarCore::Duration duration() const;

    /** Return the overall duration of a specified number of repetitions.
     *  @param count the number of repetitions to find the duration of.
     */
    KCalendarCore::Duration duration(int count) const;

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
    int nextRepeatCount(const KADateTime &from, const KADateTime &preDateTime) const;

    /** Find the repetition count for the last repetition before a specified time.
     *  @param from           repetition start time, which should not be a date-only value
     *  @param afterDateTime  time after which the desired repetition occurs
     */
    int previousRepeatCount(const KADateTime &from, const KADateTime &afterDateTime) const;

private:
    //@cond PRIVATE
    class Private;
    Private *const d;
    //@endcond
};

} // namespace KAlarmCal


// vim: et sw=4:
