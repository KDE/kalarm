/*
 *  holidays.h  -  holiday checker
 *  This file is part of kalarmcalendar library, which provides access to KAlarm
 *  calendar data.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2023-2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "kalarmcal_export.h"

#include <QSharedPointer>
#include <QDate>
#include <QBitArray>

namespace KHolidays
{
class HolidayRegion;
}

namespace KAlarmCal
{

/**
 * Class providing FUTURE holiday data for a holiday region.
 * Data is cached to avoid unnecessary repeated evaluations of holiday data by
 * KHolidays functions.
 *
 * NOTE: Dates before the current date are NOT handled, since KAlarm does not
 *       use such dates.
 */
class KALARMCAL_EXPORT Holidays
{
public:
    /** Holiday type */
    enum Type
    {
        None,        //!< Not a holiday
        Working,     //!< A holiday, but a working day
        NonWorking   //!< A non-working holiday
    };

    /** Constructor.
     *  @param region  the holiday region data.
     */
    explicit Holidays(const KHolidays::HolidayRegion& region);

    /** Constructor.
     *  @param regionCode  the code for the holiday region.
     */
    explicit Holidays(const QString& regionCode = {});

    /** Set a new holiday region.
     *  @param region  the holiday region data.
     */
    void setRegion(const KHolidays::HolidayRegion& region);

    /** Set a new holiday region.
     *  @param regionCode  the code for the holiday region.
     */
    void setRegion(const QString& regionCode);

    /** Return the holiday region code. */
    QString regionCode() const;

    /** Return whether the holiday data is valid. */
    bool isValid() const;

    /** Determine whether a date is a non-working holiday.
     *  @param date  date to check; must be today or later.
     *  @return whether a holiday, or false if date is earlier than today.
     */
    bool isHoliday(const QDate& date) const;

    /** Determine the holiday type for a date.
     *  @param date  date to check; must be today or later.
     *  @return holiday type, or None if date is earlier than today.
     */
    Type holidayType(const QDate& date) const;

    /** Return the name(s) of a holiday.
     *  @param date  date to check; must be today or later.
     *  @return holiday name(s), or empty if not a holiday or date is earlier than today.
     */
    QStringList holidayNames(const QDate& date) const;

    /** Set the maximum cache size. The preset maximum size is 10 years. Only call
     *  this if the maximum size needs to be changed.
     */
    void setCacheYears(int years);

private:
    void initialise();
    void extendCache(const QDate& end) const;

    QSharedPointer<const KHolidays::HolidayRegion> mRegion;

    QDate         mCacheStartDate;   // start date for cached data arrays
    mutable QDate mNoCacheDate;      // first date past end of cached data arrays
    int           mCacheYears{10};   // maximum cache size in years

    // Arrays containing holiday data, indexed by day number offset from start of year.
    mutable QBitArray mTypes;        // pairs of bits: (is a non-working holiday, is a working holiday)
    mutable QList<QStringList> mNames; // holiday names, or empty if not a holiday
};

} // namespace KAlarmCal

// vim: et sw=4:
