/*
 *  karecurrence.cpp  -  recurrence with special yearly February 29th handling
 *  Program:  kalarm
 *  Copyright © 2005-2010 by David Jarvie <djarvie@kde.org>
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

#include "karecurrence.h"

#include "datetime.h"

#ifdef USE_AKONADI
#include <kcalcore/icalformat.h>
#else
#include <kcal/icalformat.h>
#endif
#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>

#include <QBitArray>


#ifdef USE_AKONADI
using namespace KCalCore;
#else
using namespace KCal;
#endif

/*=============================================================================
= Class KARecurrence
= The purpose of this class is to represent the restricted range of recurrence
= types which are handled by KAlarm, and to translate between these and the
= libkcal Recurrence class. In particular, it handles yearly recurrences on
= 29th February specially:
=
= KARecurrence allows annual 29th February recurrences to fall on 28th
= February or 1st March, or not at all, in non-leap years. It allows such
= 29th February recurrences to be combined with the 29th of other months in
= a simple way, represented simply as the 29th of multiple months including
= February. For storage in the libkcal calendar, the 29th day of the month
= recurrence for other months is combined with a last-day-of-February or a
= 60th-day-of-the-year recurrence rule, thereby conforming to RFC2445.
=============================================================================*/


KARecurrence::Feb29Type KARecurrence::mDefaultFeb29 = KARecurrence::Feb29_None;


/******************************************************************************
* Set up a KARecurrence from recurrence parameters, using the start date to
* determine the recurrence day/month as appropriate.
* Only a restricted subset of recurrence types is allowed.
* Reply = true if successful.
*/
bool KARecurrence::set(Type recurType, int freq, int count, int f29, const KDateTime& start, const KDateTime& end)
{
    mCachedType = -1;
    RecurrenceRule::PeriodType rrtype;
    switch (recurType)
    {
        case MINUTELY:    rrtype = RecurrenceRule::rMinutely;  break;
        case DAILY:       rrtype = RecurrenceRule::rDaily;  break;
        case WEEKLY:      rrtype = RecurrenceRule::rWeekly;  break;
        case MONTHLY_DAY: rrtype = RecurrenceRule::rMonthly;  break;
        case ANNUAL_DATE: rrtype = RecurrenceRule::rYearly;  break;
        case NO_RECUR:    rrtype = RecurrenceRule::rNone;  break;
        default:
            return false;
    }
    if (!init(rrtype, freq, count, f29, start, end))
        return false;
    switch (recurType)
    {
        case WEEKLY:
        {
            QBitArray days(7);
            days.setBit(start.date().dayOfWeek() - 1);
            addWeeklyDays(days);
            break;
        }
        case MONTHLY_DAY:
            addMonthlyDate(start.date().day());
            break;
        case ANNUAL_DATE:
            addYearlyDate(start.date().day());
            addYearlyMonth(start.date().month());
            break;
        default:
            break;
    }
    return true;
}

/******************************************************************************
* Initialise a KARecurrence from recurrence parameters.
* Reply = true if successful.
*/
bool KARecurrence::init(RecurrenceRule::PeriodType recurType, int freq, int count, int f29, const KDateTime& start,
                        const KDateTime& end)
{
    mCachedType = -1;
    Feb29Type feb29Type = (f29 == -1) ? mDefaultFeb29 : static_cast<Feb29Type>(f29);
    mFeb29Type = Feb29_None;
    clear();
    if (count < -1)
        return false;
    bool dateOnly = start.isDateOnly();
    if (!count  &&  ((!dateOnly && !end.isValid())
                  || (dateOnly && !end.date().isValid())))
        return false;
    switch (recurType)
    {
        case RecurrenceRule::rMinutely:
        case RecurrenceRule::rDaily:
        case RecurrenceRule::rWeekly:
        case RecurrenceRule::rMonthly:
        case RecurrenceRule::rYearly:
            break;
        case rNone:
            return true;
        default:
            return false;
    }
    setNewRecurrenceType(recurType, freq);
    if (count)
        setDuration(count);
    else if (dateOnly)
        setEndDate(end.date());
    else
        setEndDateTime(end);
    KDateTime startdt = start;
    if (recurType == RecurrenceRule::rYearly
    &&  (feb29Type == Feb29_Feb28  ||  feb29Type == Feb29_Mar1))
    {
        int year = startdt.date().year();
        if (!QDate::isLeapYear(year)
        &&  startdt.date().dayOfYear() == (feb29Type == Feb29_Mar1 ? 60 : 59))
        {
            /* The event start date is February 28th or March 1st, but it
             * is a recurrence on February 29th (recurring on February 28th
             * or March 1st in non-leap years). Adjust the start date to
             * be on February 29th in the last previous leap year.
             * This is necessary because KARecurrence represents all types
             * of 29th February recurrences by a simple 29th February.
             */
            while (!QDate::isLeapYear(--year)) ;
            startdt.setDate(QDate(year, 2, 29));
        }
        mFeb29Type = feb29Type;
    }
    Recurrence::setStartDateTime(startdt);   // sets recurrence all-day if date-only
    return true;
}

/******************************************************************************
* Initialise the recurrence from an iCalendar RRULE string.
*/
bool KARecurrence::set(const QString& icalRRULE)
{
    static QString RRULE = QLatin1String("RRULE:");
    mCachedType = -1;
    clear();
    if (icalRRULE.isEmpty())
        return true;
    ICalFormat format;
    if (!format.fromString(defaultRRule(true),
                           (icalRRULE.startsWith(RRULE) ? icalRRULE.mid(RRULE.length()) : icalRRULE)))
        return false;
    fix();
    return true;
}

/******************************************************************************
* Must be called after presetting with a KCal::Recurrence, to convert the
* recurrence to KARecurrence types:
* - Convert hourly recurrences to minutely.
* - Remove all but the first day in yearly date recurrences.
* - Check for yearly recurrences falling on February 29th and adjust them as
*   necessary. A 29th of the month rule can be combined with either a 60th day
*   of the year rule or a last day of February rule.
*/
void KARecurrence::fix()
{
    mCachedType = -1;
    mFeb29Type = Feb29_None;
    int convert = 0;
    int days[2] = { 0, 0 };
    RecurrenceRule* rrules[2];
    RecurrenceRule::List rrulelist = rRules();
    int rri = 0;
    int rrend = rrulelist.count();
    for (int i = 0;  i < 2  &&  rri < rrend;  ++i, ++rri)
    {
        RecurrenceRule* rrule = rrulelist[rri];
        rrules[i] = rrule;
        bool stop = true;
        int rtype = recurrenceType(rrule);
        switch (rtype)
        {
            case rHourly:
                // Convert an hourly recurrence to a minutely one
                rrule->setRecurrenceType(RecurrenceRule::rMinutely);
                rrule->setFrequency(rrule->frequency() * 60);
                // fall through to rMinutely
            case rMinutely:
            case rDaily:
            case rWeekly:
            case rMonthlyDay:
            case rMonthlyPos:
            case rYearlyPos:
                if (!convert)
                    ++rri;    // remove all rules except the first
                break;
            case rOther:
                if (dailyType(rrule))
                {                        // it's a daily rule with BYDAYS
                    if (!convert)
                        ++rri;    // remove all rules except the first
                }
                break;
            case rYearlyDay:
            {
                // Ensure that the yearly day number is 60 (i.e. Feb 29th/Mar 1st)
                if (convert)
                {
                    // This is the second rule.
                    // Ensure that it can be combined with the first one.
                    if (days[0] != 29
                    ||  rrule->frequency() != rrules[0]->frequency()
                    ||  rrule->startDt()   != rrules[0]->startDt())
                        break;
                }
                QList<int> ds = rrule->byYearDays();
                if (!ds.isEmpty()  &&  ds.first() == 60)
                {
                    ++convert;    // this rule needs to be converted
                    days[i] = 60;
                    stop = false;
                    break;
                }
                break;     // not day 60, so remove this rule
            }
            case rYearlyMonth:
            {
                QList<int> ds = rrule->byMonthDays();
                if (!ds.isEmpty())
                {
                    int day = ds.first();
                    if (convert)
                    {
                        // This is the second rule.
                        // Ensure that it can be combined with the first one.
                        if (day == days[0]  ||  (day == -1 && days[0] == 60)
                        ||  rrule->frequency() != rrules[0]->frequency()
                        ||  rrule->startDt()   != rrules[0]->startDt())
                            break;
                    }
                    if (ds.count() > 1)
                    {
                        ds.clear();   // remove all but the first day
                        ds.append(day);
                        rrule->setByMonthDays(ds);
                    }
                    if (day == -1)
                    {
                        // Last day of the month - only combine if it's February
                        QList<int> months = rrule->byMonths();
                        if (months.count() != 1  ||  months.first() != 2)
                            day = 0;
                    }
                    if (day == 29  ||  day == -1)
                    {
                        ++convert;    // this rule may need to be converted
                        days[i] = day;
                        stop = false;
                        break;
                    }
                }
                if (!convert)
                    ++rri;
                break;
            }
            default:
                break;
        }
        if (stop)
            break;
    }

    // Remove surplus rules
    for ( ;  rri < rrend;  ++rri)
        deleteRRule(rrulelist[rri]);

    QDate end;
    int count;
    QList<int> months;
    if (convert == 2)
    {
        // There are two yearly recurrence rules to combine into a February 29th recurrence.
        // Combine the two recurrence rules into a single rYearlyMonth rule falling on Feb 29th.
        // Find the duration of the two RRULEs combined, using the shorter of the two if they differ.
        if (days[0] != 29)
        {
            // Swap the two rules so that the 29th rule is the first
            RecurrenceRule* rr = rrules[0];
            rrules[0] = rrules[1];    // the 29th rule
            rrules[1] = rr;
            int d = days[0];
            days[0] = days[1];
            days[1] = d;        // the non-29th day
        }
        // If February is included in the 29th rule, remove it to avoid duplication
        months = rrules[0]->byMonths();
        if (months.removeAll(2))
            rrules[0]->setByMonths(months);

        count = combineDurations(rrules[0], rrules[1], end);
        mFeb29Type = (days[1] == 60) ? Feb29_Mar1 : Feb29_Feb28;
    }
    else if (convert == 1  &&  days[0] == 60)
    {
        // There is a single 60th day of the year rule.
        // Convert it to a February 29th recurrence.
        count = duration();
        if (!count)
            end = endDate();
        mFeb29Type = Feb29_Mar1;
    }
    else
        return;

    // Create the new February 29th recurrence
    setNewRecurrenceType(RecurrenceRule::rYearly, frequency());
    RecurrenceRule* rrule = defaultRRule();
    months.append(2);
    rrule->setByMonths(months);
    QList<int> ds;
    ds.append(29);
    rrule->setByMonthDays(ds);
    if (count)
        setDuration(count);
    else
        setEndDate(end);
}

/******************************************************************************
* Get the next time the recurrence occurs, strictly after a specified time.
*/
KDateTime KARecurrence::getNextDateTime(const KDateTime& preDateTime) const
{
    switch (type())
    {
        case ANNUAL_DATE:
        case ANNUAL_POS:
        {
            Recurrence recur;
            writeRecurrence(recur);
            return recur.getNextDateTime(preDateTime);
        }
        default:
            return Recurrence::getNextDateTime(preDateTime);
    }
}

/******************************************************************************
* Get the previous time the recurrence occurred, strictly before a specified time.
*/
KDateTime KARecurrence::getPreviousDateTime(const KDateTime& afterDateTime) const
{
    switch (type())
    {
        case ANNUAL_DATE:
        case ANNUAL_POS:
        {
            Recurrence recur;
            writeRecurrence(recur);
            return recur.getPreviousDateTime(afterDateTime);
        }
        default:
            return Recurrence::getPreviousDateTime(afterDateTime);
    }
}

/******************************************************************************
* Initialise a KCal::Recurrence to be the same as this instance.
* Additional recurrence rules are created as necessary if it recurs on Feb 29th.
*/
void KARecurrence::writeRecurrence(Recurrence& recur) const
{
    recur.clear();
    recur.setStartDateTime(startDateTime());
    recur.setExDates(exDates());
    recur.setExDateTimes(exDateTimes());
    const RecurrenceRule* rrule = defaultRRuleConst();
    if (!rrule)
        return;
    int freq  = frequency();
    int count = duration();
    static_cast<KARecurrence*>(&recur)->setNewRecurrenceType(rrule->recurrenceType(), freq);
    if (count)
        recur.setDuration(count);
    else
        recur.setEndDateTime(endDateTime());
    switch (type())
    {
        case DAILY:
            if (rrule->byDays().isEmpty())
                break;
            // fall through to rWeekly
        case WEEKLY:
        case MONTHLY_POS:
            recur.defaultRRule(true)->setByDays(rrule->byDays());
            break;
        case MONTHLY_DAY:
            recur.defaultRRule(true)->setByMonthDays(rrule->byMonthDays());
            break;
        case ANNUAL_POS:
            recur.defaultRRule(true)->setByMonths(rrule->byMonths());
            recur.defaultRRule()->setByDays(rrule->byDays());
            break;
        case ANNUAL_DATE:
        {
            QList<int> months = rrule->byMonths();
            QList<int> days   = monthDays();
            bool special = (mFeb29Type != Feb29_None  &&  !days.isEmpty()
                            &&  days.first() == 29  &&  months.removeAll(2));
            RecurrenceRule* rrule1 = recur.defaultRRule();
            rrule1->setByMonths(months);
            rrule1->setByMonthDays(days);
            if (!special)
                break;

            // It recurs on the 29th February.
            // Create an additional 60th day of the year, or last day of February, rule.
            RecurrenceRule* rrule2 = new RecurrenceRule();
            rrule2->setRecurrenceType(RecurrenceRule::rYearly);
            rrule2->setFrequency(freq);
            rrule2->setStartDt(startDateTime());
            rrule2->setAllDay(allDay());
            if (!count)
                rrule2->setEndDt(endDateTime());
            if (mFeb29Type == Feb29_Mar1)
            {
                QList<int> ds;
                ds.append(60);
                rrule2->setByYearDays(ds);
            }
            else
            {
                QList<int> ds;
                ds.append(-1);
                rrule2->setByMonthDays(ds);
                QList<int> ms;
                ms.append(2);
                rrule2->setByMonths(ms);
            }

            if (months.isEmpty())
            {
                // Only February recurs.
                // Replace the RRULE and keep the recurrence count the same.
                if (count)
                    rrule2->setDuration(count);
                recur.unsetRecurs();
            }
            else
            {
                // Months other than February also recur on the 29th.
                // Remove February from the list and add a separate RRULE for February.
                if (count)
                {
                    rrule1->setDuration(-1);
                    rrule2->setDuration(-1);
                    if (count > 0)
                    {
                        /* Adjust counts in the two rules to keep the correct occurrence total.
                         * Note that durationTo() always includes the start date. Since for an
                         * individual RRULE the start date may not actually be included, we need
                         * to decrement the count if the start date doesn't actually recur in
                         * this RRULE.
                         * Note that if the count is small, one of the rules may not recur at
                         * all. In that case, retain it so that the February 29th characteristic
                         * is not lost should the user later change the recurrence count.
                         */
                        KDateTime end = endDateTime();
                        int count1 = rrule1->durationTo(end)
                                     - (rrule1->recursOn(startDate(), startDateTime().timeSpec()) ? 0 : 1);
                        if (count1 > 0)
                            rrule1->setDuration(count1);
                        else
                            rrule1->setEndDt(startDateTime());
                        int count2 = rrule2->durationTo(end)
                                     - (rrule2->recursOn(startDate(), startDateTime().timeSpec()) ? 0 : 1);
                        if (count2 > 0)
                            rrule2->setDuration(count2);
                        else
                            rrule2->setEndDt(startDateTime());
                    }
                }
            }
            recur.addRRule(rrule2);
            break;
        }
        default:
            break;
    }
}

/******************************************************************************
* Return the date/time of the last recurrence.
*/
KDateTime KARecurrence::endDateTime() const
{
    if (mFeb29Type == Feb29_None  ||  duration() <= 1)
    {
        /* Either it doesn't have any special February 29th treatment,
         * it's infinite (count = -1), the end date is specified
         * (count = 0), or it ends on the start date (count = 1).
         * So just use the normal KCal end date calculation.
         */
        return Recurrence::endDateTime();
    }

    /* Create a temporary recurrence rule to find the end date.
     * In a standard KCal recurrence, the 29th February only occurs once every
     * 4 years. So shift the temporary recurrence date to the 28th to ensure
     * that it occurs every year, thus giving the correct occurrence count.
     */
    RecurrenceRule* rrule = new RecurrenceRule();
    rrule->setRecurrenceType(RecurrenceRule::rYearly);
    KDateTime dt = startDateTime();
    QDate d = dt.date();
    switch (d.day())
    {
        case 29:
            // The start date is definitely a recurrence date, so shift
            // start date to the temporary recurrence date of the 28th
            d.setYMD(d.year(), d.month(), 28);
            break;
        case 28:
            if (d.month() != 2  ||  mFeb29Type != Feb29_Feb28  ||  QDate::isLeapYear(d.year()))
            {
                // Start date is not a recurrence date, so shift it to 27th
                d.setYMD(d.year(), d.month(), 27);
            }
            break;
        case 1:
            if (d.month() == 3  &&  mFeb29Type == Feb29_Mar1  &&  !QDate::isLeapYear(d.year()))
            {
                // Start date is a March 1st recurrence date, so shift
                // start date to the temporary recurrence date of the 28th
                d.setYMD(d.year(), 2, 28);
            }
            break;
        default:
            break;
    }
    dt.setDate(d);
    rrule->setStartDt(dt);
    rrule->setAllDay(allDay());
    rrule->setFrequency(frequency());
    rrule->setDuration(duration());
    QList<int> ds;
    ds.append(28);
    rrule->setByMonthDays(ds);
    rrule->setByMonths(defaultRRuleConst()->byMonths());
    dt = rrule->endDt();
    delete rrule;

    // We've found the end date for a recurrence on the 28th. Unless that date
    // is a real February 28th recurrence, adjust to the actual recurrence date.
    if (mFeb29Type == Feb29_Feb28  &&  dt.date().month() == 2  &&  !QDate::isLeapYear(dt.date().year()))
        return dt;
    return dt.addDays(1);
}

/******************************************************************************
* Return the date/time of the last recurrence.
*/
QDate KARecurrence::endDate() const
{
    KDateTime end = endDateTime();
    return end.isValid() ? end.date() : QDate();
}

/******************************************************************************
* Return whether the event will recur on the specified date.
* The start date only returns true if it matches the recurrence rules.
*/
bool KARecurrence::recursOn(const QDate& dt, const KDateTime::Spec& timeSpec) const
{
    if (!Recurrence::recursOn(dt, timeSpec))
        return false;
    if (dt != startDate())
        return true;
    // We know now that it isn't in EXDATES or EXRULES,
    // so we just need to check if it's in RDATES or RRULES
    if (rDates().contains(dt))
        return true;
    RecurrenceRule::List rulelist = rRules();
    for (int rri = 0, rrend = rulelist.count();  rri < rrend;  ++rri)
        if (rulelist[rri]->recursOn(dt, timeSpec))
            return true;
    DateTimeList dtlist = rDateTimes();
    for (int dti = 0, dtend = dtlist.count();  dti < dtend;  ++dti)
        if (dtlist[dti].date() == dt)
            return true;
    return false;
}

/******************************************************************************
* Find the duration of two RRULEs combined.
* Use the shorter of the two if they differ.
*/
int KARecurrence::combineDurations(const RecurrenceRule* rrule1, const RecurrenceRule* rrule2, QDate& end) const
{
    int count1 = rrule1->duration();
    int count2 = rrule2->duration();
    if (count1 == -1  &&  count2 == -1)
        return -1;

    // One of the RRULEs may not recur at all if the recurrence count is small.
    // In this case, its end date will have been set to the start date.
    if (count1  &&  !count2  &&  rrule2->endDt().date() == startDateTime().date())
        return count1;
    if (count2  &&  !count1  &&  rrule1->endDt().date() == startDateTime().date())
        return count2;

    /* The duration counts will be different even for RRULEs of the same length,
     * because the first RRULE only actually occurs every 4 years. So we need to
     * compare the end dates.
     */
    if (!count1  ||  !count2)
        count1 = count2 = 0;
    // Get the two rules sorted by end date.
    KDateTime end1 = rrule1->endDt();
    KDateTime end2 = rrule2->endDt();
    if (end1.date() == end2.date())
    {
        end = end1.date();
        return count1 + count2;
    }
    const RecurrenceRule* rr1;    // earlier end date
    const RecurrenceRule* rr2;    // later end date
    if (end2.isValid()
    &&  (!end1.isValid()  ||  end1.date() > end2.date()))
    {
        // Swap the two rules to make rr1 have the earlier end date
        rr1 = rrule2;
        rr2 = rrule1;
        KDateTime e = end1;
        end1 = end2;
        end2 = e;
    }
    else
    {
        rr1 = rrule1;
        rr2 = rrule2;
    }

    // Get the date of the next occurrence after the end of the earlier ending rule
    RecurrenceRule rr(*rr1);
    rr.setDuration(-1);
    KDateTime next1(rr.getNextDate(end1));
    next1.setDateOnly(true);
    if (!next1.isValid())
        end = end1.date();
    else
    {
        if (end2.isValid()  &&  next1 > end2)
        {
            // The next occurrence after the end of the earlier ending rule
            // is later than the end of the later ending rule. So simply use
            // the end date of the later rule.
            end = end2.date();
            return count1 + count2;
        }
        QDate prev2 = rr2->getPreviousDate(next1).date();
        end = (prev2 > end1.date()) ? prev2 : end1.date();
    }
    if (count2)
        count2 = rr2->durationTo(end);
    return count1 + count2;
}

/******************************************************************************
* Return the longest interval between recurrences.
* Reply = 0 if it never recurs.
*/
Duration KARecurrence::longestInterval() const
{
    int freq = frequency();
    switch (type())
    {
        case MINUTELY:
            return Duration(freq * 60, Duration::Seconds);

        case DAILY:
        {
            QList<RecurrenceRule::WDayPos> days = defaultRRuleConst()->byDays();
            if (days.isEmpty())
                return Duration(freq, Duration::Days);

            // After applying the frequency, the specified days of the week
            // further restrict when the recurrence occurs.
            // So the maximum interval may be greater than the frequency.
            bool ds[7] = { false, false, false, false, false, false, false };
            for (int i = 0, end = days.count();  i < end;  ++i)
                if (days[i].pos() == 0)
                    ds[days[i].day() - 1] = true;
            if (freq % 7)
            {
                // It will recur on every day of the week in some week or other
                // (except for those days which are excluded).
                int first = -1;
                int last  = -1;
                int maxgap = 1;
                for (int i = 0;  i < freq*7;  i += freq)
                {
                    if (ds[i % 7])
                    {
                        if (first < 0)
                            first = i;
                        else if (i - last > maxgap)
                            maxgap = i - last;
                        last = i;
                    }
                }
                int wrap = freq*7 - last + first;
                if (wrap > maxgap)
                    maxgap = wrap;
                return Duration(maxgap, Duration::Days);
            }
            else
            {
                // It will recur on the same day of the week every time.
                // Ensure that the day is a day which is not excluded.
                if (ds[startDate().dayOfWeek() - 1])
                       return Duration(freq, Duration::Days);
                break;
            }
        }
        case WEEKLY:
        {
            // Find which days of the week it recurs on, and if on more than
            // one, reduce the maximum interval accordingly.
            QBitArray ds = days();
            int first = -1;
            int last  = -1;
            int maxgap = 1;
            // Use the user's definition of the week, starting at the
            // day of the week specified by the user's locale.
            int weekStart = KGlobal::locale()->weekStartDay() - 1;  // zero-based
            for (int i = 0;  i < 7;  ++i)
            {
                // Get the standard KDE day-of-week number (zero-based)
                // for the day-of-week number in the user's locale.
                if (ds.testBit((i + weekStart) % 7))
                {
                    if (first < 0)
                        first = i;
                    else if (i - last > maxgap)
                        maxgap = i - last;
                    last = i;
                }
            }
            if (first < 0)
                break;    // no days recur
            int span = last - first;
            if (freq > 1)
                return Duration(freq*7 - span, Duration::Days);
            if (7 - span > maxgap)
                return Duration(7 - span, Duration::Days);
            return Duration(maxgap, Duration::Days);
        }
        case MONTHLY_DAY:
        case MONTHLY_POS:
            return Duration(freq * 31, Duration::Days);

        case ANNUAL_DATE:
        case ANNUAL_POS:
        {
            // Find which months of the year it recurs on, and if on more than
            // one, reduce the maximum interval accordingly.
            const QList<int> months = yearMonths();  // month list is sorted
            if (months.isEmpty())
                break;    // no months recur
            if (months.count() == 1)
                return Duration(freq * 365, Duration::Days);
            int first = -1;
            int last  = -1;
            int maxgap = 0;
            for (int i = 0, end = months.count();  i < end;  ++i)
            {
                if (first < 0)
                    first = months[i];
                else
                {
                    int span = QDate(2001, last, 1).daysTo(QDate(2001, months[i], 1));
                    if (span > maxgap)
                        maxgap = span;
                }
                last = months[i];
            }
            int span = QDate(2001, first, 1).daysTo(QDate(2001, last, 1));
            if (freq > 1)
                return Duration(freq*365 - span, Duration::Days);
            if (365 - span > maxgap)
                return Duration(365 - span, Duration::Days);
            return Duration(maxgap, Duration::Days);
        }
        default:
            break;
    }
    return 0;
}

/******************************************************************************
* Return the interval between recurrences, if the interval between  successive
* occurrences does not vary.
* Reply = 0 if recurrence does not occur at fixed intervals.
*/
Duration KARecurrence::regularInterval() const
{
    int freq = frequency();
    switch (type())
    {
        case MINUTELY:
            return Duration(freq * 60, Duration::Seconds);
        case DAILY:
        {
            QList<RecurrenceRule::WDayPos> days = defaultRRuleConst()->byDays();
            if (days.isEmpty())
                return Duration(freq, Duration::Days);
            // After applying the frequency, the specified days of the week
            // further restrict when the recurrence occurs.
            // Find which days occur, and count the number of days which occur.
            bool ds[7] = { false, false, false, false, false, false, false };
            for (int i = 0, end = days.count();  i < end;  ++i)
                if (days[i].pos() == 0)
                    ds[days[i].day() - 1] = true;
            if (!(freq % 7))
            {
                // It will recur on the same day of the week every time.
                // Check whether that day is in the list of included days.
                if (ds[startDate().dayOfWeek() - 1])
                       return Duration(freq, Duration::Days);
                break;
            }
            int n = 0;   // number of days which occur
            for (int i = 0;  i < 7;  ++i)
                if (ds[i])
                    ++n;
            if (n == 7)
                return Duration(freq, Duration::Days);   // every day is included
            if (n == 1)
                return Duration(freq * 7, Duration::Days);   // only one day of the week is included
            break;
        }
        case WEEKLY:
        {
            QList<RecurrenceRule::WDayPos> days = defaultRRuleConst()->byDays();
            if (days.isEmpty())
                return Duration(freq * 7, Duration::Days);
            // The specified days of the week occur every week in which the
            // recurrence occurs.
            // Find which days occur, and count the number of days which occur.
            bool ds[7] = { false, false, false, false, false, false, false };
            for (int i = 0, end = days.count();  i < end;  ++i)
                if (days[i].pos() == 0)
                    ds[days[i].day() - 1] = true;
            int n = 0;   // number of days which occur
            for (int i = 0;  i < 7;  ++i)
                if (ds[i])
                    ++n;
            if (n == 7)
            {
                if (freq == 1)
                    return Duration(freq, Duration::Days);  // every day is included
                break;
            }
            if (n == 1)
                return Duration(freq * 7, Duration::Days);   // only one day of the week is included
            break;
        }
        default:
            break;
    }
    return 0;
}

/******************************************************************************
* Return the recurrence's period type.
*/
KARecurrence::Type KARecurrence::type() const
{
    if (mCachedType == -1)
        mCachedType = type(defaultRRuleConst());
    return static_cast<Type>(mCachedType);
}

/******************************************************************************
* Return the recurrence rule type.
*/
KARecurrence::Type KARecurrence::type(const RecurrenceRule* rrule)
{
    switch (recurrenceType(rrule))
    {
        case rMinutely:     return MINUTELY;
        case rDaily:        return DAILY;
        case rWeekly:       return WEEKLY;
        case rMonthlyDay:   return MONTHLY_DAY;
        case rMonthlyPos:   return MONTHLY_POS;
        case rYearlyMonth:  return ANNUAL_DATE;
        case rYearlyPos:    return ANNUAL_POS;
        default:
            if (dailyType(rrule))
                return DAILY;
            return NO_RECUR;
    }
}

/******************************************************************************
* Check if the rule is a daily rule with or without BYDAYS specified.
*/
bool KARecurrence::dailyType(const RecurrenceRule* rrule)
{
    if (rrule->recurrenceType() != RecurrenceRule::rDaily
    ||  !rrule->bySeconds().isEmpty()
    ||  !rrule->byMinutes().isEmpty()
    ||  !rrule->byHours().isEmpty()
    ||  !rrule->byWeekNumbers().isEmpty()
    ||  !rrule->byMonthDays().isEmpty()
    ||  !rrule->byMonths().isEmpty()
    ||  !rrule->bySetPos().isEmpty()
    ||  !rrule->byYearDays().isEmpty())
        return false;
    QList<RecurrenceRule::WDayPos> days = rrule->byDays();
    if (days.isEmpty())
        return true;
    // Check that all the positions are zero (i.e. every time)
    bool found = false;
    for (int i = 0, end = days.count();  i < end;  ++i)
    {
        if (days[i].pos() != 0)
            return false;
        found = true;
    }
    return found;
}

// vim: et sw=4:
