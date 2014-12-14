/*
 *  repetition.cpp  -  represents a sub-repetition: interval and count
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  Copyright © 2009-2012 by David Jarvie <djarvie@kde.org>
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

#include "repetition.h"
#include <kdatetime.h>

using namespace KCalCore;
namespace KAlarmCal
{

class Repetition::Private
{
public:
    Private() : mInterval(0), mCount(0) {}
    Private(const Duration &interval, int count)
        : mInterval(interval), mCount(count)
    {
        if ((!count && interval) || (count && !interval)) {
            mCount = 0;
            mInterval = 0;
        }
    }

    Duration mInterval;   // sub-repetition interval
    int      mCount;      // sub-repetition count (excluding the first time)
};

Repetition::Repetition()
    : d(new Private)
{
}

Repetition::Repetition(const Duration &interval, int count)
    : d(new Private(interval, count))
{
}

Repetition::Repetition(const Repetition &other)
    : d(new Private(*other.d))
{
}

Repetition::~Repetition()
{
    delete d;
}

Repetition &Repetition::operator=(const Repetition &other)
{
    if (&other != this) {
        *d = *other.d;
    }
    return *this;
}

void Repetition::set(const Duration &interval, int count)
{
    if (!count || !interval) {
        d->mCount = 0;
        d->mInterval = 0;
    } else {
        d->mCount = count;
        d->mInterval = interval;
    }
}

void Repetition::set(const Duration &interval)
{
    if (d->mCount) {
        d->mInterval = interval;
        if (!interval) {
            d->mCount = 0;
        }
    }
}

Repetition::operator bool() const
{
    return d->mCount;
}

bool Repetition::operator==(const Repetition &r) const
{
    return d->mInterval == r.d->mInterval && d->mCount == r.d->mCount;
}

int Repetition::count() const
{
    return d->mCount;
}

Duration Repetition::interval() const
{
    return d->mInterval;
}

Duration Repetition::duration() const
{
    return d->mInterval * d->mCount;
}

Duration Repetition::duration(int count) const
{
    return d->mInterval * count;
}

bool Repetition::isDaily() const
{
    return d->mInterval.isDaily();
}

int Repetition::intervalDays() const
{
    return d->mInterval.asDays();
}

int Repetition::intervalMinutes() const
{
    return d->mInterval.asSeconds() / 60;
}

int Repetition::intervalSeconds() const
{
    return d->mInterval.asSeconds();
}

int Repetition::nextRepeatCount(const KDateTime &from, const KDateTime &preDateTime) const
{
    return d->mInterval.isDaily()
           ? from.daysTo(preDateTime) / d->mInterval.asDays() + 1
           : static_cast<int>(from.secsTo_long(preDateTime) / d->mInterval.asSeconds()) + 1;
}

int Repetition::previousRepeatCount(const KDateTime &from, const KDateTime &afterDateTime) const
{
    return d->mInterval.isDaily()
           ? from.daysTo(afterDateTime.addSecs(-1)) / d->mInterval.asDays()
           : static_cast<int>((from.secsTo_long(afterDateTime) - 1) / d->mInterval.asSeconds());
}

} // namespace KAlarmCal

