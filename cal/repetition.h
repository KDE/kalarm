/*
 *  repetition.h  -  represents a sub-repetition: interval and count
 *  Program:  kalarm
 *  Copyright © 2009 by David Jarvie <djarvie@kde.org>
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

#ifndef REPETITION_H
#define REPETITION_H

#include "kalarm_cal_export.h"
#include <kcal/duration.h>
#include <kdatetime.h>

/**
 *  The Repetition class represents a sub-repetition, storing its interval
 *  and repeat count. The repeat count is the number of repetitions after
 *  the first occurrence.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class KALARM_CAL_EXPORT Repetition
{
    public:
	Repetition() : mInterval(0), mCount(0) {}
	Repetition(const KCal::Duration& interval, int count)
		: mInterval(interval), mCount(count)
	{
		if ((!count && interval) || (count && !interval))
		{
			mCount = 0;
			mInterval = 0;
		}
	}

	void set(const KCal::Duration& interval, int count)
	{
		if (!count || !interval)
		{
			mCount = 0;
			mInterval = 0;
		}
		else
		{
			mCount = count;
			mInterval = interval;
		}
	}

	void set(const KCal::Duration& interval)
	{
		if (mCount)
		{
			mInterval = interval;
			if (!interval)
				mCount = 0;
		}
	}

	operator bool() const                   { return mCount; }
	bool operator!() const                  { return !mCount; }
	bool operator==(const Repetition& r)    { return mInterval == r.mInterval && mCount == r.mCount; }
	bool operator!=(const Repetition& r)    { return mInterval != r.mInterval || mCount != r.mCount; }

	/** Return the number of repetitions. */
	int count() const     { return mCount; }

	/** Return the interval between repetitions. */
	const KCal::Duration& interval() const  { return mInterval; }

	/** Return the overall duration of the repetition. */
	KCal::Duration duration() const  { return mInterval * mCount; }

	/** Return the overall duration of a specified number of repetitions.
	 *  @param count the number of repetitions to find the duration of.
	 */
	KCal::Duration duration(int count) const  { return mInterval * count; }

	/** Check whether the repetition interval is in terms of days (as opposed to minutes). */
	bool isDaily() const          { return mInterval.isDaily(); }

	/** Return the repetition interval in terms of days.
	 *  If necessary, the interval is rounded down to a whole number of days.
	 */
	int intervalDays() const      { return mInterval.asDays(); }

	/** Return the repetition interval in terms of minutes.
	 *  If necessary, the interval is rounded down to a whole number of minutes.
	 */
	int intervalMinutes() const   { return mInterval.asSeconds() / 60; }

	/** Return the repetition interval in terms of seconds. */
	int intervalSeconds() const   { return mInterval.asSeconds(); }

	/** Find the repetition count for the next repetition after a specified time.
	 *  @param from         repetition start time, which should not be a date-only value
	 *  @param preDateTime  time after which the desired repetition occurs
	 */
	int nextRepeatCount(const KDateTime& from, const KDateTime& preDateTime) const
	{
		return mInterval.isDaily()
		     ? from.daysTo(preDateTime) / mInterval.asDays() + 1
		     : static_cast<int>(from.secsTo_long(preDateTime) / mInterval.asSeconds()) + 1;
	}

	/** Find the repetition count for the last repetition before a specified time.
	 *  @param from           repetition start time, which should not be a date-only value
	 *  @param afterDateTime  time after which the desired repetition occurs
	 */
	int previousRepeatCount(const KDateTime& from, const KDateTime& afterDateTime) const
	{
		return mInterval.isDaily()
		     ? from.daysTo(afterDateTime.addSecs(-1)) / mInterval.asDays()
		     : static_cast<int>((from.secsTo_long(afterDateTime) - 1) / mInterval.asSeconds());
	}

    private:
	KCal::Duration mInterval;   // sub-repetition interval
	int            mCount;      // sub-repetition count (excluding the first time)
};

#endif // REPETITION_H
