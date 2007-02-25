/*
 *  karecurrence.h  -  recurrence with special yearly February 29th handling
 *  Program:  kalarm
 *  Copyright © 2005-2007 by David Jarvie <software@astrojar.org.uk>
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

#include <kcal/recurrence.h>

#include "preferences.h"
class DateTime;


class KARecurrence : public KCal::Recurrence
{
	public:
		/** The recurrence's period type.
		 *  This is a subset of the possible KCal recurrence types.
		 */
		enum Type {
			NO_RECUR,      // does not recur
			MINUTELY,      // at an hours/minutes interval
			DAILY,         // daily
			WEEKLY,        // weekly, on specified weekdays
			MONTHLY_POS,   // monthly, on specified weekdays in a specified week of the month
			MONTHLY_DAY,   // monthly, on a specified day of the month
			ANNUAL_DATE,   // yearly, on a specified date in each of the specified months
			ANNUAL_POS     // yearly, on specified weekdays in the specified weeks of the specified months
		};

		KARecurrence() : KCal::Recurrence(), mFeb29Type(Preferences::Feb29_None), mCachedType(-1) { }
		KARecurrence(const KCal::Recurrence& r) : KCal::Recurrence(r) { fix(); }
		KARecurrence(const KARecurrence& r) : KCal::Recurrence(r), mFeb29Type(r.mFeb29Type), mCachedType(r.mCachedType) { }
		bool        set(const QString& icalRRULE);
		bool        set(Type t, int freq, int count, const KDateTime& start, const KDateTime& end)
		                        { return set(t, freq, count, -1, start, end); }
		bool        set(Type t, int freq, int count, const KDateTime& start, const KDateTime& end, Preferences::Feb29Type f29)
		                        { return set(t, freq, count, f29, start, end); }
		bool        init(KCal::RecurrenceRule::PeriodType t, int freq, int count, const KDateTime& start, const KDateTime& end)
		                        { return init(t, freq, count, -1, start, end); }
		bool        init(KCal::RecurrenceRule::PeriodType t, int freq, int count, const KDateTime& start, const KDateTime& end, Preferences::Feb29Type f29)
		                        { return init(t, freq, count, f29, start, end); }
		void        fix();
		void        writeRecurrence(KCal::Recurrence&) const;
		KDateTime   endDateTime() const;
		QDate       endDate() const;
		bool        recursOn(const QDate&, const KDateTime::Spec&) const;
		KDateTime   getNextDateTime(const KDateTime& preDateTime) const;
		KDateTime   getPreviousDateTime(const KDateTime& afterDateTime) const;
		int         longestInterval() const;
		Type        type() const;
		static Type type(const KCal::RecurrenceRule*);
		static bool dailyType(const KCal::RecurrenceRule*);
		Preferences::Feb29Type feb29Type() const                   { return mFeb29Type; }
		static Preferences::Feb29Type defaultFeb29Type()           { return mDefaultFeb29; }
		static void setDefaultFeb29Type(Preferences::Feb29Type t)  { mDefaultFeb29 = t; }

	private:
		bool        set(Type, int freq, int count, int feb29Type, const KDateTime& start, const KDateTime& end);
		bool        init(KCal::RecurrenceRule::PeriodType, int freq, int count, int feb29Type, const KDateTime& start, const KDateTime& end);
		int         combineDurations(const KCal::RecurrenceRule*, const KCal::RecurrenceRule*, QDate& end) const;
		int         longestWeeklyInterval(const QBitArray& days, int frequency);

		static Preferences::Feb29Type mDefaultFeb29;
		Preferences::Feb29Type   mFeb29Type;       // yearly recurrence on Feb 29th (leap years) / Mar 1st (non-leap years)
		mutable int              mCachedType;
};

#endif // KARECURRENCE_H
