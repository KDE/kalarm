/*
 *  alarmtime.cpp  -  conversion functions for alarm times
 *  Program:  kalarm
 *  Copyright © 2007-2016,2018 by David Jarvie <djarvie@kde.org>
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

#include "alarmtime.h"
#include "preferences.h"

#include <kalarmcal/datetime.h>

#include <KLocalizedString>
#include <QLocale>
#include <QApplication>
#include "kalarm_debug.h"

using namespace KAlarmCal;

// Whether the date and time contain leading zeroes.
QString AlarmTime::mDateFormat;      // date format for current locale
QString AlarmTime::mTimeFormat;      // time format for current locale
QString AlarmTime::mTimeFullFormat;  // time format with leading zero, if different from mTimeFormat
int     AlarmTime::mHourOffset = 0;  // offset within time string to the hour
bool    AlarmTime::mLeadingZeroesChecked = false;

/******************************************************************************
* Return the alarm time text in the form "date time".
* Parameters:
*   dateTime    = the date/time to format.
*   leadingZero = the character to represent a leading zero, or '\0' for no leading zeroes.
*/
QString AlarmTime::alarmTimeText(const DateTime& dateTime, char leadingZero)
{
    if (!dateTime.isValid())
        return i18nc("@info Alarm never occurs", "Never");
    if (!mLeadingZeroesChecked  &&  QApplication::isLeftToRight())    // don't try to align right-to-left languages
    {
        // Check whether the day number and/or hour have no leading zeroes, if
        // they are at the start of the date/time. If no leading zeroes, they
        // will need to be padded when displayed, so that displayed dates/times
        // can be aligned with each other.
        // Note that if leading zeroes are not included in other components, no
        // alignment will be attempted.
        QLocale locale;
        {
            // Check the date format. 'dd' provides leading zeroes; single 'd'
            // provides no leading zeroes.
            mDateFormat = locale.dateFormat(QLocale::ShortFormat);
        }
        {
            // Check the time format.
            // Remove all but hours, minutes and AM/PM, since alarms are on minute
            // boundaries. Preceding separators are also removed.
            mTimeFormat = locale.timeFormat(QLocale::ShortFormat);
            for (int del = 0, predel = 0, c = 0;  c < mTimeFormat.size();  ++c)
            {
                char ch = mTimeFormat.at(c).toLatin1();
                switch (ch)
                {
                    case 'H':
                    case 'h':
                    case 'm':
                    case 'a':
                    case 'A':
                        if (predel == 1)
                        {
                            mTimeFormat.remove(del, c - del);
                            c = del;
                        }
                        del = c + 1;   // start deleting from the next character
                        if ((ch == 'A'  &&  del < mTimeFormat.size()  &&  mTimeFormat.at(del).toLatin1() == 'P')
                        ||  (ch == 'a'  &&  del < mTimeFormat.size()  &&  mTimeFormat.at(del).toLatin1() == 'p'))
                            ++c, ++del;
                        predel = -1;
                        break;

                    case 's':
                    case 'z':
                    case 't':
                        mTimeFormat.remove(del, c + 1 - del);
                        c = del - 1;
                        if (!predel)
                            predel = 1;
                        break;

                    default:
                        break;
                }
            }

            // 'HH' and 'hh' provide leading zeroes; single 'H' or 'h' provide no
            // leading zeroes.
            int i = mTimeFormat.indexOf(QRegExp(QLatin1String("[hH]")));
            int first = mTimeFormat.indexOf(QRegExp(QLatin1String("[hHmaA]")));
            if (i >= 0  &&  i == first  &&  (i == mTimeFormat.size() - 1  ||  mTimeFormat.at(i) != mTimeFormat.at(i + 1)))
            {
                mTimeFullFormat = mTimeFormat;
                mTimeFullFormat.insert(i, mTimeFormat.at(i));
                // Find index to hour in formatted times
                const QTime t(1,30,30);
                const QString nozero = t.toString(mTimeFormat);
                const QString zero   = t.toString(mTimeFullFormat);
                for (int i = 0; i < nozero.size(); ++i)
                    if (nozero[i] != zero[i])
                    {
                        mHourOffset = i;
                        break;
                    }
            }
        }
    }
    mLeadingZeroesChecked = true;

    const KADateTime kdt = dateTime.effectiveKDateTime().toTimeSpec(Preferences::timeSpec());
    QString dateTimeText = kdt.date().toString(mDateFormat);

    if (!dateTime.isDateOnly()  ||  kdt.utcOffset() != dateTime.utcOffset())
    {
        // Display the time of day if it's a date/time value, or if it's
        // a date-only value but it's in a different time zone
        dateTimeText += QLatin1Char(' ');
        bool useFullFormat = leadingZero && !mTimeFullFormat.isEmpty();
        QString timeText = kdt.time().toString(useFullFormat ? mTimeFullFormat : mTimeFormat);
        if (useFullFormat  &&  leadingZero != '0'  &&  timeText.at(mHourOffset) == QLatin1Char('0'))
            timeText[mHourOffset] = leadingZero;
        dateTimeText += timeText;
    }
    return dateTimeText + QLatin1Char(' ');
}

/******************************************************************************
* Return the time-to-alarm text.
*/
QString AlarmTime::timeToAlarmText(const DateTime& dateTime)
{
    if (!dateTime.isValid())
        return i18nc("@info Alarm never occurs", "Never");
    KADateTime now = KADateTime::currentUtcDateTime();
    if (dateTime.isDateOnly())
    {
        int days = now.date().daysTo(dateTime.date());
        // xgettext: no-c-format
        return i18nc("@info n days", "%1d", days);
    }
    int mins = (now.secsTo(dateTime.effectiveKDateTime()) + 59) / 60;
    if (mins < 0)
        return QString();
    char minutes[3] = "00";
    minutes[0] = (mins%60) / 10 + '0';
    minutes[1] = (mins%60) % 10 + '0';
    if (mins < 24*60)
        return i18nc("@info hours:minutes", "%1:%2", mins/60, QLatin1String(minutes));
    // If we render a day count, then we zero-pad the hours, to make the days line up and be more scanable.
    int hrs = mins / 60;
    char hours[3] = "00";
    hours[0] = (hrs%24) / 10 + '0';
    hours[1] = (hrs%24) % 10 + '0';
    int days = hrs / 24;
    return i18nc("@info days hours:minutes", "%1d %2:%3", days, QLatin1String(hours), QLatin1String(minutes));
}

/******************************************************************************
* Convert a date/time specification string into a local date/time or date value.
* Parameters:
*   timeString  = in the form [[[yyyy-]mm-]dd-]hh:mm [TZ] or yyyy-mm-dd [TZ].
*   dateTime  = receives converted date/time value.
*   defaultDt = default date/time used for missing parts of timeString, or null
*               to use current date/time.
*   allowTZ   = whether to allow a time zone specifier in timeString.
* Reply = true if successful.
*/
bool AlarmTime::convertTimeString(const QByteArray& timeString, KADateTime& dateTime, const KADateTime& defaultDt, bool allowTZ)
{
#define MAX_DT_LEN 19
    int i = timeString.indexOf(' ');
    if (i > MAX_DT_LEN  ||  (i >= 0 && !allowTZ))
        return false;
    QString zone = (i >= 0) ? QString::fromLatin1(timeString.mid(i)) : QString();
    char timeStr[MAX_DT_LEN+1];
    strcpy(timeStr, timeString.left(i >= 0 ? i : MAX_DT_LEN));
    int dt[5] = { -1, -1, -1, -1, -1 };
    char* s;
    char* end;
    bool noTime;
    // Get the minute value
    if ((s = strchr(timeStr, ':')) == nullptr)
        noTime = true;
    else
    {
        noTime = false;
        *s++ = 0;
        dt[4] = strtoul(s, &end, 10);
        if (end == s  ||  *end  ||  dt[4] >= 60)
            return false;
        // Get the hour value
        if ((s = strrchr(timeStr, '-')) == nullptr)
            s = timeStr;
        else
            *s++ = 0;
        dt[3] = strtoul(s, &end, 10);
        if (end == s  ||  *end  ||  dt[3] >= 24)
            return false;
    }
    bool noDate = true;
    if (s != timeStr)
    {
        noDate = false;
        // Get the day value
        if ((s = strrchr(timeStr, '-')) == nullptr)
            s = timeStr;
        else
            *s++ = 0;
        dt[2] = strtoul(s, &end, 10);
        if (end == s  ||  *end  ||  dt[2] == 0  ||  dt[2] > 31)
            return false;
        if (s != timeStr)
        {
            // Get the month value
            if ((s = strrchr(timeStr, '-')) == nullptr)
                s = timeStr;
            else
                *s++ = 0;
            dt[1] = strtoul(s, &end, 10);
            if (end == s  ||  *end  ||  dt[1] == 0  ||  dt[1] > 12)
                return false;
            if (s != timeStr)
            {
                // Get the year value
                dt[0] = strtoul(timeStr, &end, 10);
                if (end == timeStr  ||  *end)
                    return false;
            }
        }
    }

    QDate date;
    if (dt[0] >= 0)
        date = QDate(dt[0], dt[1], dt[2]);
    QTime time(0, 0, 0);
    if (noTime)
    {
        // No time was specified, so the full date must have been specified
        if (dt[0] < 0  ||  !date.isValid())
            return false;
        dateTime = applyTimeZone(zone, date, time, false, defaultDt);
    }
    else
    {
        // Compile the values into a date/time structure
        time.setHMS(dt[3], dt[4], 0);
        if (dt[0] < 0)
        {
            // Some or all of the date was omitted.
            // Use the default date/time if provided.
            if (defaultDt.isValid())
            {
                dt[0] = defaultDt.date().year();
                date.setYMD(dt[0],
                            (dt[1] < 0 ? defaultDt.date().month() : dt[1]),
                            (dt[2] < 0 ? defaultDt.date().day() : dt[2]));
            }
            else
                date.setYMD(2000, 1, 1);  // temporary substitute for date
        }
        dateTime = applyTimeZone(zone, date, time, true, defaultDt);
        if (!dateTime.isValid())
            return false;
        if (dt[0] < 0)
        {
            // Some or all of the date was omitted.
            // Use the current date in the specified time zone as default.
            const KADateTime now = KADateTime::currentDateTime(dateTime.timeSpec());
            date = dateTime.date();
            date.setYMD(now.date().year(),
                        (dt[1] < 0 ? now.date().month() : dt[1]),
                        (dt[2] < 0 ? now.date().day() : dt[2]));
            if (!date.isValid())
                return false;
            if (noDate  &&  time < now.time())
                date = date.addDays(1);
            dateTime.setDate(date);
        }
    }
    return dateTime.isValid();
}

/******************************************************************************
* Convert a time zone specifier string and apply it to a given date and/or time.
* The time zone specifier is a system time zone name, e.g. "Europe/London" or
* "UTC". If no time zone is specified, it defaults to the local time zone.
* If 'defaultDt' is valid, it supplies the time spec and default date.
*/
KADateTime AlarmTime::applyTimeZone(const QString& tzstring, const QDate& date, const QTime& time,
                                   bool haveTime, const KADateTime& defaultDt)
{
    bool error = false;
    KADateTime::Spec spec = KADateTime::LocalZone;
    const QString zone = tzstring.trimmed();
    if (!zone.isEmpty())
    {
        if (zone == QStringLiteral("UTC"))
            spec = KADateTime::UTC;
        else
        {
            const QTimeZone tz(zone.toLatin1());
            error = !tz.isValid();
            if (!error)
                spec = tz;
        }
    }
    else if (defaultDt.isValid())
        spec = defaultDt.timeSpec();

    KADateTime result;
    if (!error)
    {
        if (!date.isValid())
        {
            // It's a time without a date
            if (defaultDt.isValid())
                   result = KADateTime(defaultDt.date(), time, spec);
            else if (spec == KADateTime::LocalZone)
                result = KADateTime(KADateTime::currentLocalDate(), time, spec);
        }
        else if (haveTime)
        {
            // It's a date and time
            result = KADateTime(date, time, spec);
        }
        else
        {
            // It's a date without a time
            result = KADateTime(date, spec);
        }
    }
    return result;
}

// vim: et sw=4:
