/*
 *  datetime.h  -  date/time representation with optional date-only value
 *  Program:  kalarm
 *  (C) 2003, 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef DATETIME_H
#define DATETIME_H

#include <qdatetime.h>


/**
 *  The DateTime class holds a date, with or without a time.
 *  DateTime is very similar to the QDateTime class, with the additional option to
 *  hold a date-only value. This allows a single date-time representation to be used
 *  for both an event having a specific date and time, and an all-day event.
 *
 *  @short QDateTime with date-only option.
 *  @author David Jarvie <software@astrojar.org.uk>
*/
class DateTime
{
	public:
		/** Default constructor.
		 *  Constructs a null date-time.
		 */
		DateTime()                     : mDateOnly(false) { }
		/** Constructor for a date-only value. */
		DateTime(const QDate& d)       : mDateTime(d), mDateOnly(true) { }
		/** Constructor for a date-time value. */
		DateTime(const QDate& d, const QTime& t)
		                               : mDateTime(d, t), mDateOnly(false) { }
		/** Constructor for a date-time or date-only value.
		 *  @param dateOnly True to construct a date-only value; false to construct a date-time value.
		 */
		DateTime(const QDateTime& dt, bool dateOnly = false)
		                               : mDateTime(dt), mDateOnly(dateOnly)
		                               { if (dateOnly) mDateTime.setTime(QTime()); }
		/** Assignment operator.
		 *  Sets the value to a specified date-time or date-only value.
		 */
		DateTime& operator=(const DateTime& dt)
		                               { mDateTime = dt.mDateTime;  mDateOnly = dt.mDateOnly;  return *this; }
		/** Assignment operator.
		 *  Sets the value to a specified date-time.
		 */
		DateTime& operator=(const QDateTime& dt)
		                               { mDateTime = dt;  mDateOnly = false;  return *this; }
		/** Assignment operator.
		 *  Sets the value to a specified date-only value.
		 */
		DateTime& operator=(const QDate& d)
		                               { mDateTime.setDate(d);  mDateOnly = true;  return *this; }
		/** Returns true if the date is null and, if it is a date-time value, the time is also null. */
		bool      isNull() const       { return mDateTime.date().isNull()  &&  (mDateOnly || mDateTime.time().isNull()); }
		/** Returns true if the date is valid and, if it is a date-time value, the time is also valid. */
		bool      isValid() const      { return mDateTime.date().isValid()  &&  (mDateOnly || mDateTime.time().isValid()); }
		/** Returns true if it is date-only value. */
		bool      isDateOnly() const   { return mDateOnly; }
		/** Sets the value to be either date-only or date-time.
		 *  @param d True to set the value to be date-only; false to set it to a date-time value.
		 */
		void      setDateOnly(bool d)  { mDateOnly = d;  if (d) mDateTime.setTime(QTime()); }
		/** Returns the date part of the value. */
		QDate     date() const         { return mDateTime.date(); }
		/** Returns the time part of the value.
		 *  If the value is date-only, the time returned is the start-of-day time set
		 *  in the Preferences dialogue.
		 */
		QTime     time() const;
		/** Returns the date and time of the value.
		 *  If the value is date-only, the time part returned is equal to the start-of-day time set
		 *  in the KAlarm Preferences dialogue.
		 */
		QDateTime dateTime() const;
		/** Sets a date-time or date-only value.
		 *  @param dateOnly True to set a date-only value; false to set a date-time value.
		 */
		void      set(const QDateTime& dt, bool dateOnly = false)
				{
					mDateTime = dt;
					mDateOnly = dateOnly;
					if (dateOnly)
						mDateTime.setTime(QTime());
				}
		/** Sets a date-time value. */
		void      set(const QDate& d, const QTime& t)
		                               { mDateTime.setDate(d);  mDateTime.setTime(t);  mDateOnly = false; }
		/** Sets the time component of the value.
		 *  The value is converted if necessary to be a date-time value.
		 */
		void      setTime(const QTime& t)  { mDateTime.setTime(t);  mDateOnly = false; }
		/** Sets the value to a specified date-time value.
		 *  @param secs The time_t date-time value, expressed as the number of seconds elapsed
		 *              since 1970-01-01 00:00:00 UTC.
		 */
		void      setTime_t(uint secs)     { mDateTime.setTime_t(secs);  mDateOnly = false; }
		/** Returns a DateTime value @p secs seconds later than the value of this object.
		 *  If this object is date-only, @p secs is first rounded down to a whole number of days
		 *  before adding the value.
		 */
		DateTime  addSecs(int n) const
				{
					if (mDateOnly)
						return DateTime(mDateTime.date().addDays(n / (3600*24)), true);
					else
						return DateTime(mDateTime.addSecs(n), false);
				}
		/** Returns a DateTime value @p mins minutes later than the value of this object.
		 *  If this object is date-only, @p mins is first rounded down to a whole number of days
		 *  before adding the value.
		 */
		DateTime  addMins(int n) const
				{
					if (mDateOnly)
						return DateTime(mDateTime.addDays(n / (60*24)), true);
					else
						return DateTime(mDateTime.addSecs(n * 60), false);
				}
		/** Returns a DateTime value @p n days later than the value of this object. */
		DateTime  addDays(int n) const    { return DateTime(mDateTime.addDays(n), mDateOnly); }
		/** Returns a DateTime value @p n months later than the value of this object. */
		DateTime  addMonths(int n) const  { return DateTime(mDateTime.addMonths(n), mDateOnly); }
		/** Returns a DateTime value @p n years later than the value of this object. */
		DateTime  addYears(int n) const   { return DateTime(mDateTime.addYears(n), mDateOnly); }
		/** Returns the number of days from this date or date-time to @p dt. */
		int       daysTo(const DateTime& dt) const
		                                  { return (mDateOnly || dt.mDateOnly) ? mDateTime.date().daysTo(dt.date()) : mDateTime.daysTo(dt.mDateTime); }
		/** Returns the number of minutes from this date or date-time to @p dt.
		 *  If either of the values is date-only, the result is calculated by simply
		 *  taking the difference in dates and ignoring the times.
		 */
		int       minsTo(const DateTime& dt) const
		                                  { return (mDateOnly || dt.mDateOnly) ? mDateTime.date().daysTo(dt.date()) * 24*60 : mDateTime.secsTo(dt.mDateTime) / 60; }
		/** Returns the number of seconds from this date or date-time to @p dt.
		 *  If either of the values is date-only, the result is calculated by simply
		 *  taking the difference in dates and ignoring the times.
		 */
		int       secsTo(const DateTime& dt) const
		                                  { return (mDateOnly || dt.mDateOnly) ? mDateTime.date().daysTo(dt.date()) * 24*3600 : mDateTime.secsTo(dt.mDateTime); }
		/** Returns the value as a string.
		 *  If it is a date-time, both time and date are included in the output.
		 *  If it is date-only, only the date is included in the output.
		 */
		QString   toString(Qt::DateFormat f = Qt::TextDate) const
				{
					if (mDateOnly)
						return mDateTime.date().toString(f);
					else
						return mDateTime.toString(f);
				}
		/** Returns the value as a string.
		 *  If it is a date-time, both time and date are included in the output.
		 *  If it is date-only, only the date is included in the output.
		 */
		QString   toString(const QString& format) const
				{
					if (mDateOnly)
						return mDateTime.date().toString(format);
					else
						return mDateTime.toString(format);
				}
		/** Returns the value as a string, formatted according to the user's locale.
		 *  If it is a date-time, both time and date are included in the output.
		 *  If it is date-only, only the date is included in the output.
		 */
		QString   formatLocale(bool shortFormat = true) const;

		friend bool operator==(const DateTime& dt1, const DateTime& dt2);
		friend bool operator<(const DateTime& dt1, const DateTime& dt2);

	private:
		QDateTime  mDateTime;
		bool       mDateOnly;
};

/** Returns true if the two values are equal. */
bool operator==(const DateTime& dt1, const DateTime& dt2);
/** Returns true if the @p dt1 is earlier than @p dt2.
 *  If the two values have the same date, and one value is date-only while the other is a date-time, the
 *  time used for the date-only value is the start-of-day time set in the KAlarm Preferences dialogue.
 */
bool operator<(const DateTime& dt1, const DateTime& dt2);
/** Returns true if the two values are not equal. */
inline bool operator!=(const DateTime& dt1, const DateTime& dt2)   { return !operator==(dt1, dt2); }
/** Returns true if the @p dt1 is later than @p dt2.
 *  If the two values have the same date, and one value is date-only while the other is a date-time, the
 *  time used for the date-only value is the start-of-day time set in the KAlarm Preferences dialogue.
 */
inline bool operator>(const DateTime& dt1, const DateTime& dt2)    { return operator<(dt2, dt1); }
/** Returns true if the @p dt1 is later than or equal to @p dt2.
 *  If the two values have the same date, and one value is date-only while the other is a date-time, the
 *  time used for the date-only value is the start-of-day time set in the KAlarm Preferences dialogue.
 */
inline bool operator>=(const DateTime& dt1, const DateTime& dt2)   { return !operator<(dt1, dt2); }
/** Returns true if the @p dt1 is earlier than or equal to @p dt2.
 *  If the two values have the same date, and one value is date-only while the other is a date-time, the
 *  time used for the date-only value is the start-of-day time set in the KAlarm Preferences dialogue.
 */
inline bool operator<=(const DateTime& dt1, const DateTime& dt2)   { return !operator<(dt2, dt1); }

#endif // DATETIME_H
