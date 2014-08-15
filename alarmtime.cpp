/*
 *  alarmtime.cpp  -  conversion functions for alarm times
 *  Program:  kalarm
 *  Copyright © 2007-2012 by David Jarvie <djarvie@kde.org>
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

#include <ksystemtimezone.h>
#include <kglobal.h>
#include <KLocalizedString>
#include <qapplication.h>
#include <qdebug.h>

using namespace KAlarmCal;

int AlarmTime::mTimeHourPos = -2;

/******************************************************************************
* Return the alarm time text in the form "date time".
*/
QString AlarmTime::alarmTimeText(const DateTime& dateTime)
{
    if (!dateTime.isValid())
        return i18nc("@info/plain Alarm never occurs", "Never");
    KLocale* locale = KLocale::global();
    KDateTime kdt = dateTime.effectiveKDateTime().toTimeSpec(Preferences::timeZone());
    QString dateTimeText = locale->formatDate(kdt.date(), KLocale::ShortDate);
    if (!dateTime.isDateOnly()
    ||  (!dateTime.isClockTime()  &&  kdt.utcOffset() != dateTime.utcOffset()))
    {
        // Display the time of day if it's a date/time value, or if it's
        // a date-only value but it's in a different time zone
        dateTimeText += QLatin1Char(' ');
        QString time = locale->formatTime(kdt.time());
        if (mTimeHourPos == -2)
        {
            // Initialise the position of the hour within the time string, if leading
            // zeroes are omitted, so that displayed times can be aligned with each other.
            mTimeHourPos = -1;     // default = alignment isn't possible/sensible
            if (QApplication::isLeftToRight())    // don't try to align right-to-left languages
            {
                QString fmt = locale->timeFormat();
                int i = fmt.indexOf(QRegExp(QLatin1String("%[kl]")));   // check if leading zeroes are omitted
                if (i >= 0  &&  i == fmt.indexOf(QLatin1Char('%')))   // and whether the hour is first
                    mTimeHourPos = i;             // yes, so need to align
            }
        }
        if (mTimeHourPos >= 0  &&  (int)time.length() > mTimeHourPos + 1
        &&  time[mTimeHourPos].isDigit()  &&  !time[mTimeHourPos + 1].isDigit())
            dateTimeText += QLatin1Char('~');     // improve alignment of times with no leading zeroes
        dateTimeText += time;
    }
    return dateTimeText + QLatin1Char(' ');
}

/******************************************************************************
* Return the time-to-alarm text.
*/
QString AlarmTime::timeToAlarmText(const DateTime& dateTime)
{
    if (!dateTime.isValid())
        return i18nc("@info/plain Alarm never occurs", "Never");
    KDateTime now = KDateTime::currentUtcDateTime();
    if (dateTime.isDateOnly())
    {
        int days = now.date().daysTo(dateTime.date());
        // xgettext: no-c-format
        return i18nc("@info/plain n days", "%1d", days);
    }
    int mins = (now.secsTo(dateTime.effectiveKDateTime()) + 59) / 60;
    if (mins < 0)
        return QString();
    char minutes[3] = "00";
    minutes[0] = (mins%60) / 10 + '0';
    minutes[1] = (mins%60) % 10 + '0';
    if (mins < 24*60)
        return i18nc("@info/plain hours:minutes", "%1:%2", mins/60, QLatin1String(minutes));
    int days = mins / (24*60);
    mins = mins % (24*60);
    return i18nc("@info/plain days hours:minutes", "%1d %2:%3", days, mins/60, QLatin1String(minutes));
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
bool AlarmTime::convertTimeString(const QByteArray& timeString, KDateTime& dateTime, const KDateTime& defaultDt, bool allowTZ)
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
    if ((s = strchr(timeStr, ':')) == 0)
        noTime = true;
    else
    {
        noTime = false;
        *s++ = 0;
        dt[4] = strtoul(s, &end, 10);
        if (end == s  ||  *end  ||  dt[4] >= 60)
            return false;
        // Get the hour value
        if ((s = strrchr(timeStr, '-')) == 0)
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
        if ((s = strrchr(timeStr, '-')) == 0)
            s = timeStr;
        else
            *s++ = 0;
        dt[2] = strtoul(s, &end, 10);
        if (end == s  ||  *end  ||  dt[2] == 0  ||  dt[2] > 31)
            return false;
        if (s != timeStr)
        {
            // Get the month value
            if ((s = strrchr(timeStr, '-')) == 0)
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
            KDateTime now = KDateTime::currentDateTime(dateTime.timeSpec());
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
* The time zone specifier is a system time zone name, e.g. "Europe/London",
* "UTC" or "Clock". If no time zone is specified, it defaults to the local time
* zone.
* If 'defaultDt' is valid, it supplies the time spec and default date.
*/
KDateTime AlarmTime::applyTimeZone(const QString& tzstring, const QDate& date, const QTime& time,
                                   bool haveTime, const KDateTime& defaultDt)
{
    bool error = false;
    KDateTime::Spec spec = KDateTime::LocalZone;
    QString zone = tzstring.trimmed();
    if (!zone.isEmpty())
    {
        if (zone == QLatin1String("Clock"))
            spec = KDateTime::ClockTime;
        else if (zone == QLatin1String("UTC"))
            spec = KDateTime::UTC;
        else
        {
            KTimeZone tz = KSystemTimeZones::zone(zone);
            error = !tz.isValid();
            if (!error)
                spec = tz;
        }
    }
    else if (defaultDt.isValid())
        spec = defaultDt.timeSpec();

    KDateTime result;
    if (!error)
    {
        if (!date.isValid())
        {
            // It's a time without a date
            if (defaultDt.isValid())
                   result = KDateTime(defaultDt.date(), time, spec);
            else if (spec == KDateTime::LocalZone  ||  spec == KDateTime::ClockTime)
                result = KDateTime(KDateTime::currentLocalDate(), time, spec);
        }
        else if (haveTime)
        {
            // It's a date and time
            result = KDateTime(date, time, spec);
        }
        else
        {
            // It's a date without a time
            result = KDateTime(date, spec);
        }
    }
    return result;
}

// vim: et sw=4:
