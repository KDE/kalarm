/*
 *  datetime.h  -  date/time representation with optional date-only value
 *  Program:  kalarm
 *  (C) 2003 by David Jarvie  software@astrojar.org.uk
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


class DateTime
{
	public:
		DateTime()                     : mDateOnly(false) { }
		DateTime(const QDate& d)       : mDateTime(d), mDateOnly(true) { }
		DateTime(const QDate& d, const QTime& t)
		                               : mDateTime(d, t), mDateOnly(false) { }
		DateTime(const QDateTime& dt, bool dateOnly = false)
		                               : mDateTime(dt), mDateOnly(dateOnly)
		                               { if (dateOnly) mDateTime.setTime(QTime()); }
		DateTime& operator=(const DateTime& dt)
		                               { mDateTime = dt.mDateTime;  mDateOnly = dt.mDateOnly;  return *this; }
		DateTime& operator=(const QDateTime& dt)
		                               { mDateTime = dt;  mDateOnly = false;  return *this; }
		DateTime& operator=(const QDate& d)
		                               { mDateTime.setDate(d);  mDateOnly = true;  return *this; }
		bool      isNull() const       { return mDateTime.date().isNull()  &&  (mDateOnly || mDateTime.time().isNull()); }
		bool      isValid() const      { return mDateTime.date().isValid()  &&  (mDateOnly || mDateTime.time().isValid()); }
		bool      isDateOnly() const   { return mDateOnly; }
		void      setDateOnly(bool d)  { mDateOnly = d;  if (d) mDateTime.setTime(QTime()); }
		QDate     date() const         { return mDateTime.date(); }
		QTime     time() const;
		QDateTime dateTime() const;
		void      set(const QDateTime& dt, bool dateOnly = false)
				{
					mDateTime = dt;
					mDateOnly = dateOnly;
					if (dateOnly)
						mDateTime.setTime(QTime());
				}
		void      set(const QDate& d, const QTime& t)
		                               { mDateTime.setDate(d);  mDateTime.setTime(t);  mDateOnly = false; }
		void      setTime(const QTime& t)  { mDateTime.setTime(t);  mDateOnly = false; }
		void      setTime_t(uint secs)     { mDateTime.setTime_t(secs);  mDateOnly = false; }
		DateTime  addSecs(int n) const
				{
					if (mDateOnly)
						return DateTime(mDateTime.date().addDays(n / (3600*24)), true);
					else
						return DateTime(mDateTime.addSecs(n), false);
				}
		DateTime  addMins(int n) const
				{
					if (mDateOnly)
						return DateTime(mDateTime.addDays(n / (60*24)), true);
					else
						return DateTime(mDateTime.addSecs(n * 60), false);
				}
		DateTime  addDays(int n) const    { return DateTime(mDateTime.addDays(n), mDateOnly); }
		DateTime  addMonths(int n) const  { return DateTime(mDateTime.addMonths(n), mDateOnly); }
		DateTime  addYears(int n) const   { return DateTime(mDateTime.addYears(n), mDateOnly); }
		int       daysTo(const DateTime& dt) const
		                                  { return (mDateOnly || dt.mDateOnly) ? mDateTime.date().daysTo(dt.date()) : mDateTime.daysTo(dt.mDateTime); }
		int       minsTo(const DateTime& dt) const
		                                  { return (mDateOnly || dt.mDateOnly) ? mDateTime.date().daysTo(dt.date()) * 24*60 : mDateTime.secsTo(dt.mDateTime) / 60; }
		int       secsTo(const DateTime& dt) const
		                                  { return (mDateOnly || dt.mDateOnly) ? mDateTime.date().daysTo(dt.date()) * 24*3600 : mDateTime.secsTo(dt.mDateTime); }
		QString   toString(Qt::DateFormat f = Qt::TextDate) const
				{
					if (mDateOnly)
						return mDateTime.date().toString(f);
					else
						return mDateTime.toString(f);
				}
		QString   toString(const QString& format) const
				{
					if (mDateOnly)
						return mDateTime.date().toString(format);
					else
						return mDateTime.toString(format);
				}
		QString   formatLocale(bool shortFormat = true) const;

		friend bool operator==(const DateTime& dt1, const DateTime& dt2);
		friend bool operator<(const DateTime& dt1, const DateTime& dt2);

	private:
		QDateTime  mDateTime;
		bool       mDateOnly;
};

bool operator==(const DateTime& dt1, const DateTime& dt2);
bool operator<(const DateTime& dt1, const DateTime& dt2);
inline bool operator!=(const DateTime& dt1, const DateTime& dt2)   { return !operator==(dt1, dt2); }
inline bool operator>(const DateTime& dt1, const DateTime& dt2)    { return operator<(dt2, dt1); }
inline bool operator>=(const DateTime& dt1, const DateTime& dt2)   { return !operator<(dt1, dt2); }
inline bool operator<=(const DateTime& dt1, const DateTime& dt2)   { return !operator<(dt2, dt1); }

#endif // DATETIME_H
