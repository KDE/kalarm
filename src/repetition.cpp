/*
 *  repetition.cpp  -  represents a sub-repetition: interval and count
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  SPDX-FileCopyrightText: 2009-2012, 2018 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "repetition.h"
#include "kadatetime.h"

using namespace KCalendarCore;
namespace KAlarmCal
{

class Q_DECL_HIDDEN Repetition::Private
{
public:
  Private() = default;
  Private(const Duration &interval, int count)
      : mInterval(interval), mCount(count) {
    if ((!count && !interval.isNull()) || (count && interval.isNull())) {
      mCount = 0;
      mInterval = 0;
    }
    }

    Duration mInterval = 0;   // sub-repetition interval
    int      mCount = 0;      // sub-repetition count (excluding the first time)
};

Repetition::Repetition()
    : d(new Private)
{
}

Repetition::Repetition(const KCalendarCore::Duration &interval, int count)
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

void Repetition::set(const KCalendarCore::Duration &interval, int count)
{
    if (!count || interval.isNull()) {
        d->mCount = 0;
        d->mInterval = 0;
    } else {
        d->mCount = count;
        d->mInterval = interval;
    }
}

void Repetition::set(const KCalendarCore::Duration &interval)
{
    if (d->mCount) {
        d->mInterval = interval;
        if (interval.isNull()) {
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

int Repetition::nextRepeatCount(const KADateTime &from, const KADateTime &preDateTime) const
{
    return d->mInterval.isDaily()
           ? from.daysTo(preDateTime) / d->mInterval.asDays() + 1
           : static_cast<int>(from.secsTo(preDateTime) / d->mInterval.asSeconds()) + 1;
}

int Repetition::previousRepeatCount(const KADateTime &from, const KADateTime &afterDateTime) const
{
    return d->mInterval.isDaily()
           ? from.daysTo(afterDateTime.addSecs(-1)) / d->mInterval.asDays()
           : static_cast<int>((from.secsTo(afterDateTime) - 1) / d->mInterval.asSeconds());
}

} // namespace KAlarmCal

// vim: et sw=4:
