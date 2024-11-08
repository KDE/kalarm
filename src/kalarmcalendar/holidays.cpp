/*
 *  holidays.cpp  -  holiday checker
 *  This file is part of kalarmcalendar library, which provides access to KAlarm
 *  calendar data.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "holidays.h"

#include <KHolidays/HolidayRegion>

namespace KAlarmCal
{

Holidays::Holidays(const KHolidays::HolidayRegion& holidayRegion)
{
    initialise(holidayRegion.regionCode());
}

Holidays::Holidays(const QString& regionCode)
{
    initialise(regionCode);
}

/******************************************************************************
* Set a new holiday region.
*/
void Holidays::setRegion(const KHolidays::HolidayRegion& holidayRegion)
{
    if (holidayRegion.regionCode() == mRegion->regionCode())
        return;
    mTypes.clear();
    mNames.clear();
    initialise(holidayRegion.regionCode());
}

/******************************************************************************
* Set a new holiday region.
*/
void Holidays::setRegion(const QString& regionCode)
{
    if (regionCode == mRegion->regionCode())
        return;
    mTypes.clear();
    mNames.clear();
    initialise(regionCode);
}

/******************************************************************************
* Set a new holiday region.
*/
void Holidays::initialise(const QString& regionCode)
{
    mRegion.reset(new KHolidays::HolidayRegion(regionCode));
    mCacheStartDate = QDate::currentDate().addDays(-1);   // in case KAlarm time zone is different
    mNoCacheDate    = mCacheStartDate;

    if (mRegion->isValid())
    {
        // Initially cache holiday data up to a year from today
        const int COUNT = 366;
        extendCache(mCacheStartDate.addDays(COUNT - 1));
    }
}

/******************************************************************************
* Return the holiday region code.
*/
QString Holidays::regionCode() const
{
    return mRegion ? mRegion->regionCode() : QString();
}

/******************************************************************************
* Return whether the holiday data is valid.
*/
bool Holidays::isValid() const
{
    return mRegion && mRegion->isValid();
}

/******************************************************************************
* Determine whether a date is a non-working holiday.
*/
bool Holidays::isHoliday(const QDate& date) const
{
    return holidayType(date) == NonWorking;
}

/******************************************************************************
* Determine the holiday type for a date.
*/
Holidays::Type Holidays::holidayType(const QDate& date) const
{
    if (date < QDate::currentDate().addDays(-1))
        return None;
    const int offset = mCacheStartDate.daysTo(date);
    if (date < mNoCacheDate)
        return mTypes[offset*2] ? NonWorking : mTypes[offset*2 + 1] ? Working : None;
    // The date is past the end of the cache. Fill the cache.
    extendCache(QDate(date.year(), 12, 31));
    if (date < mNoCacheDate)                            //cppcheck-suppress[knownConditionTrueFalse] extendCache() modifies mNoCacheDate
        return mTypes[offset*2] ? NonWorking : mTypes[offset*2 + 1] ? Working : None;

    // The date is past the maximum cache limit.
    const KHolidays::Holiday::List hols = mRegion->rawHolidaysWithAstroSeasons(date);
    for (const KHolidays::Holiday& h : hols)
        if (h.dayType() == KHolidays::Holiday::NonWorkday)
            return NonWorking;
    return hols.isEmpty() ? None : Working;
}

/******************************************************************************
* Return the name of a holiday.
*/
QStringList Holidays::holidayNames(const QDate& date) const
{
    if (date < QDate::currentDate().addDays(-1))
        return {};
    if (date < mNoCacheDate)
        return mNames[mCacheStartDate.daysTo(date)];
    // The date is past the end of the cache. Fill the cache.
    extendCache(QDate(date.year(), 12, 31));
    if (date < mNoCacheDate)                            //cppcheck-suppress[knownConditionTrueFalse] extendCache() modifies mNoCacheDate
        return mNames[mCacheStartDate.daysTo(date)];

    // The date is past the maximum cache limit.
    const KHolidays::Holiday::List hols = mRegion->rawHolidaysWithAstroSeasons(date);
    QStringList names;
    for (const KHolidays::Holiday& h : hols)
        names.append(h.name());
    return names;
}

/******************************************************************************
* Set the maximum cache size.
*/
void Holidays::setCacheYears(int years)
{
    mCacheYears = years;
}

/******************************************************************************
* Cache holiday data up to an end date.
* This will not be done past mCacheYears from now.
*/
void Holidays::extendCache(const QDate& end) const
{
    const QDate limit = QDate(QDate::currentDate().year() + mCacheYears, 12, 31);
    const QDate endDate = (end > limit) ? limit : end;
    if (endDate < mNoCacheDate)
        return;    // already cached

    const int count = mCacheStartDate.daysTo(endDate) + 1;
    const KHolidays::Holiday::List hols = mRegion->rawHolidaysWithAstroSeasons(mNoCacheDate, endDate);
    mTypes.resize(count * 2);   // this sets all new bits to 0
    mNames.resize(count);
    // Note that more than one holiday can fall on a given day.
    for (const KHolidays::Holiday& h : hols)
    {
        const QString name = h.name();
        const int workday = (h.dayType() == KHolidays::Holiday::NonWorkday) ? 0 : 1;
        const int offset2 = mCacheStartDate.daysTo(h.observedEndDate());
        for (int offset = mCacheStartDate.daysTo(h.observedStartDate());  offset <= offset2;  ++offset)
        {
            mTypes[offset*2 + workday] = 1;
            mNames[offset].append(name);
        }
    }
    mNoCacheDate = endDate.addDays(1);
}

} // namespace KAlarmCal

// vim: et sw=4:
