/*
 *  karecurrence.cpp  -  recurrence with special yearly February 29th handling
 *  Program:  kalarm
 *  Copyright (C) 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kalarm.h"

#include <qbitarray.h>

#include <libkcal/icalformat.h>

#include "datetime.h"
#include "functions.h"
#include "karecurrence.h"

using namespace KCal;


KARecurrence::Feb29Type KARecurrence::mDefaultFeb29 = KARecurrence::FEB29_FEB29;


/******************************************************************************
*  Set up a KARecurrence from recurrence parameters, using the start date to
*  determine the recurrence day/month as appropriate.
*  Only a restricted subset of recurrence types is allowed.
*  Reply = true if successful.
*/
bool KARecurrence::set(Type recurType, int freq, int count, int f29, const DateTime& start, const QDateTime& end)
{
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
*  Initialise a KARecurrence from recurrence parameters.
*  Reply = true if successful.
*/
bool KARecurrence::init(RecurrenceRule::PeriodType recurType, int freq, int count, int f29, const DateTime& start,
                        const QDateTime& end)
{
	Feb29Type feb29Type = (f29 == -1) ? mDefaultFeb29 : static_cast<Feb29Type>(f29);
	mFeb29Type = FEB29_FEB29;
	clear();
	if (count < -1)
		return false;
	bool dateOnly = start.isDateOnly();
	if (!count  &&  (!dateOnly && !end.isValid()
	              || dateOnly && !end.date().isValid()))
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
	QDateTime startdt = start.dateTime();
	if (recurType == RecurrenceRule::rYearly
	&&  feb29Type == FEB29_FEB28  ||  feb29Type == FEB29_MAR1)
	{
		int year = startdt.date().year();
		if (!QDate::leapYear(year)
		&&  startdt.date().dayOfYear() == (feb29Type == FEB29_MAR1 ? 60 : 59))
		{
			// The event start date is February 28th or March 1st, but it
			// is a recurrence on February 29th (recurring on February 28th
			// or March 1st in non-leap years). Adjust the start date to
			// be on February 29th in the last previous leap year.
//#warning Is this necessary now?
			while (!QDate::leapYear(--year)) ;
			startdt.setDate(QDate(year, 2, 29));
		}
		mFeb29Type = feb29Type;
	}
	if (dateOnly)
		setStartDate(startdt.date());
	else
		setStartDateTime(startdt);
	return true;
}

/******************************************************************************
 * Initialise the recurrence from an iCalendar RRULE string.
 */
bool KARecurrence::set(const QString& icalRRULE)
{
	static QString RRULE = QString::fromLatin1("RRULE:");
	clear();
	if (icalRRULE.isEmpty())
		return true;
	KCal::ICalFormat format;
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
	mFeb29Type = FEB29_FEB29;
	int convert = 0;
	int days[2] = { 0, 0 };
	RecurrenceRule* rrules[2];
	RecurrenceRule::List rrulelist = rRules();
	RecurrenceRule::List::ConstIterator rr = rrulelist.begin();
	for (int i = 0;  i < 2  &&  rr != rrulelist.end();  ++i, ++rr)
	{
		RecurrenceRule* rrule = *rr;
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
					++rr;    // remove all rules except the first
				break;
			case rYearlyDay:
			{
				// Ensure that the yearly day number is 60 (i.e. Feb 29th/Mar 1st)
				if (convert)
				{
					// This is the second rule.
					// Ensure that it can be combined with the first one.
					if (days[0] != 29
					||  rrule->frequency() != rrules[0]->frequency())
						break;
				}
				QValueList<int> ds = rrule->byYearDays();
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
				QValueList<int> ds = rrule->byMonthDays();
				if (!ds.isEmpty())
				{
					int day = ds.first();
					if (convert)
					{
						// This is the second rule.
						// Ensure that it can be combined with the first one.
						if (day == days[0]  ||  day == -1 && days[0] == 60
						||  rrule->frequency() != rrules[0]->frequency())
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
						QValueList<int> months = rrule->byMonths();
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
					++rr;
				break;
			}
			default:
				break;
		}
		if (stop)
			break;
	}

	// Remove surplus rules
	for ( ;  rr != rrulelist.end();  ++rr)
		removeRRule(*rr);

	QDate end;
	int count;
	QValueList<int> months;
	if (convert == 2)
	{
		// There are two yearly recurrence rules to combine into a February 29th recurrence.
		// Combine the two recurrence rules into a single rYearlyMonth rule falling on Feb 29th.
		// Find the duration of the two RRULEs combined, using the shorter of the two if they differ.
		count = combineDurations(rrules[0], rrules[1], end);
		if (days[0] != 29)
		{
			rrules[0] = rrules[1];    // the 29th rule
			days[1] = days[0];        // the non-29th day
		}
		months = rrules[0]->byMonths();
		mFeb29Type = (days[1] == 60) ? FEB29_MAR1 : FEB29_FEB28;
	}
	else if (convert == 1  &&  days[0] == 60)
	{
		// There is a single 60th day of the year rule.
		// Convert it to a February 29th recurrence.
		count = duration();
		if (!count)
			end = endDate();
		mFeb29Type = FEB29_MAR1;
	}
	else
		return;

	// Create the new February 29th recurrence
	setNewRecurrenceType(RecurrenceRule::rYearly, frequency());
	RecurrenceRule* rrule = defaultRRule();
	months.append(2);
	rrule->setByMonths(months);
	QValueList<int> ds;
	ds.append(29);
	rrule->setByMonthDays(ds);
	if (count)
		setDuration(count);
	else
		setEndDate(end);
}

/******************************************************************************
* Initialise a KCal::Recurrence to be the same as this instance.
* Additional recurrence rules are created as necessary if it recurs on Feb 29th.
*/
void KARecurrence::writeRecurrence(KCal::Recurrence& recur) const
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
	switch (recurrenceType())
	{
		case rWeekly:
		case rMonthlyPos:
			recur.defaultRRule(true)->setByDays(rrule->byDays());
			break;
		case rMonthlyDay:
			recur.defaultRRule(true)->setByMonthDays(rrule->byMonthDays());
			break;
		case rYearlyPos:
			recur.defaultRRule(true)->setByMonths(rrule->byMonths());
			recur.defaultRRule(true)->setByDays(rrule->byDays());
			break;
		case rYearlyMonth:
		{
			QValueList<int> months = rrule->byMonths();
			QValueList<int> days   = monthDays();
			if (mFeb29Type != FEB29_FEB29  &&  !days.isEmpty()
			&&  days.first() == 29  &&  months.remove(2))
			{
				// It recurs on the 29th February.
				// Create an additional 60th day of the year, or
				// last day of February, rule.
				RecurrenceRule* rrule2 = new RecurrenceRule();
				rrule2->setRecurrenceType(RecurrenceRule::rYearly);
				rrule2->setFrequency(freq);
				if (count == -1)
					rrule2->setDuration(count);
				else
					rrule2->setEndDt(endDateTime());
				if (mFeb29Type == FEB29_MAR1)
				{
					QValueList<int> ds;
					ds.append(60);
					rrule2->setByYearDays(ds);
				}
				else
				{
					QValueList<int> ds;
					ds.append(-1);
					rrule2->setByMonthDays(ds);
					QValueList<int> ms;
					ms.append(2);
					rrule2->setByMonths(ms);
				}

				if (months.isEmpty())
				{
					// Only February recurs. Replace the RRULE.
					recur.unsetRecurs();
					recur.addRRule(rrule2);
					return;
				}
				else
				{
					// Months other than February also recur on the 29th.
					// Remove February from the list.
					recur.addRRule(rrule2);
				}
			}
			recur.defaultRRule(true)->setByMonths(months);
			if (!days.isEmpty())
				recur.defaultRRule()->setByMonthDays(days);
			break;
		}
		default:
			return;
	}
	if (count)
		recur.setDuration(count);
	else
		recur.setEndDateTime(endDateTime());
}

/******************************************************************************
* Find the duration of two RRULEs combined.
* Use the shorter of the two if they differ.
*/
int KARecurrence::combineDurations(const RecurrenceRule* rrule1, const RecurrenceRule* rrule2, QDate& end) const
{
//#warning Check for correct frequency and duration when combining rules
	int count1 = rrule1->duration();
	int count2 = rrule2->duration();
	if (count1 == -1  &&  count2 == -1)
		return -1;
	// The duration counts will be different even for recurrences of the same length,
	// because the first recurrence only actually occurs every 4 years.
	// So we need to compare the end dates.
	end = rrule1->endDt().date();
	QDate end2 = rrule2->endDt().date();
	if (!end.isValid()  ||  !end2.isValid())
		return count1;
	if (end2 > end)
		return 0;
	if (end2 < end)
		end = end2;
	return count2;
}
#if 0
	if (!count)
		end = rrule1->endDt();
	int count2 = rrule2->duration();
	if (count2 != count)
	{
		if (count2 > 0)
		{
			if (count < 0  ||  count2 < count)
				count = count2;
			else if (!count)
			{
				QDateTime dt = rrule2->endDt();
				if (end.isValid()  &&  dt.isValid()  &&  dt < end)
					end = dt;
			}
		}
		else if (count2 == 0)
		{
			QDateTime dt = rrule2->endDt();
			if (count < 0)
			{
				end = dt;
				count = 0;
			}
			else
			{
				end = rrule1->endDt();
				if (end.isValid()  &&  dt.isValid()  &&  dt < end)
				{
					end = dt;
					count = 0;
				}
			}
		}
	}
	else if (!count  &&  end.isValid())
	{
		QDateTime dt = rrule2->endDt();
		if (dt.isValid()  &&  dt < end)
			end = dt;
	}
	return count;
#endif

/******************************************************************************
 * Return the longest interval (in minutes) between recurrences.
 */
int KARecurrence::longestInterval() const
{
	int freq = frequency();
	switch (recurrenceType())
	{
		case rMinutely:
			return freq;
		case rDaily:
			return freq * 1440;
		case rWeekly:
		{
			// Find which days of the week it recurs on, and if on more than
			// one, reduce the maximum interval accordingly.
			QBitArray ds = days();
			int first = -1;
			int last  = -1;
			int maxgap = 1;
			for (int i = 0;  i < 7;  ++i)
			{
				if (ds.testBit(KAlarm::localeDayInWeek_to_weekDay(i) - 1))
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
				return (freq*7 - span) * 1440;
			if (7 - span > maxgap)
				return (7 - span) * 1440;
			return maxgap * 1440;
		}
		case rMonthlyDay:
		case rMonthlyPos:
			return freq * 1440 * 31;
		case rYearlyMonth:
		case rYearlyPos:
		{
			// Find which months of the year it recurs on, and if on more than
			// one, reduce the maximum interval accordingly.
			const QValueList<int> months = yearMonths();  // month list is sorted
			if (months.isEmpty())
				break;    // no months recur
			if (months.count() > 1)
			{
				int first = -1;
				int last  = -1;
				int maxgap = 0;
				for (QValueListConstIterator<int> it = months.begin();  it != months.end();  ++it)
				{
					if (first < 0)
						first = *it;
					else
					{
						int span = QDate(2001, last, 1).daysTo(QDate(2001, *it, 1));
						if (span > maxgap)
							maxgap = span;
					}
					last = *it;
				}
				int span = QDate(2001, first, 1).daysTo(QDate(2001, last, 1));
				if (freq > 1)
					return (freq*365 - span) * 1440;
				if (365 - span > maxgap)
					return (365 - span) * 1440;
				return maxgap * 1440;
			}
			// fall through to rYearlyDay
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
	switch (recurrenceType())
	{
		case rMinutely:     return MINUTELY;
		case rDaily:        return DAILY;
		case rWeekly:       return WEEKLY;
		case rMonthlyDay:   return MONTHLY_DAY;
		case rMonthlyPos:   return MONTHLY_POS;
		case rYearlyMonth:  return ANNUAL_DATE;
		case rYearlyPos:    return ANNUAL_POS;
		default:            return NO_RECUR;
	}
}
