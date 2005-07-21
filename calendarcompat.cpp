/*
 *  calendarcompat.cpp -  compatibility for old calendar file formats
 *  Program:  kalarm
 *  Copyright (C) 2001 - 2005 by David Jarvie <software@astrojar.org.uk>
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

#include <qfile.h>
#include <qtextstream.h>

#include <kapplication.h>
#include <kaboutdata.h>
#include <kdebug.h>

extern "C" {
#include <libical/ical.h>
}
#include <libkcal/calendar.h>

#include "alarmevent.h"
#include "preferences.h"
#include "calendarcompat.h"

using namespace KCal;


/******************************************************************************
* Find the version of KAlarm which wrote the calendar file, and do any
* necessary conversions to the current format. The calendar is not saved - any
* conversions will only be saved if changes are made later.
*/
void CalendarCompat::fix(KCal::Calendar& calendar, const QString& localFile)
{
	bool version057_UTC = false;
	QString subVersion;
	int version = readKAlarmVersion(calendar, subVersion);
	if (!version)
	{
		// The calendar was created either by the current version of KAlarm,
		// or another program, so don't do any conversions
		return;
	}
	if (version == Version(0,5,7)  &&  !localFile.isEmpty())
	{
		// KAlarm version 0.5.7 - check whether times are stored in UTC, in which
		// case it is the KDE 3.0.0 version, which needs adjustment of summer times.
		version057_UTC = isUTC(localFile);
		kdDebug(5950) << "CalendarCompat::fix(): KAlarm version 0.5.7 (" << (version057_UTC ? "" : "non-") << "UTC)\n";
	}
	else
		kdDebug(5950) << "CalendarCompat::fix(): KAlarm version " << version << endl;

	// Convert events to current KAlarm format for when calendar is saved
	KAEvent::convertKCalEvents(calendar, version, version057_UTC);
}

/******************************************************************************
* Return the KAlarm version which wrote the calendar which has been loaded.
* The format is, for example, 000507 for 0.5.7.
* Reply = 0 if the calendar was created by the current version of KAlarm,
*           KAlarm pre-0.3.5, or another program.
*/
int CalendarCompat::readKAlarmVersion(KCal::Calendar& calendar, QString& subVersion)
{
	// N.B. Remember to change  Version(int major, int minor, int rev)
	//      if the representation returned by this method changes.
	subVersion = QString::null;
	const QString& prodid = calendar.productId();

	// Find the KAlarm identifier
	QString progname = QString::fromLatin1(" KAlarm ");
	int i = prodid.find(progname, 0, false);
	if (i < 0)
	{
		// Older versions used KAlarm's translated name in the product ID, which
		// could have created problems using a calendar in different locales.
		progname = QString(" ") + kapp->aboutData()->programName() + " ";
		i = prodid.find(progname, 0, false);
		if (i < 0)
			return 0;    // calendar wasn't created by KAlarm
	}

	// Extract the KAlarm version string
	QString ver = prodid.mid(i + progname.length()).stripWhiteSpace();
	i = ver.find('/');
	int j = ver.find(' ');
	if (j >= 0  &&  j < i)
		i = j;
	if (i <= 0)
		return 0;
	ver = ver.left(i);     // ver now contains the KAlarm version string
	if (ver == KALARM_VERSION)
		return 0;      // the calendar was created by the current KAlarm version

	// Convert the version string to a numeric version
	int version = 0;
	if ((i = ver.find('.')) <= 0)
		return 0;      // missing major or minor version
	bool ok;
	version = ver.left(i).toInt(&ok) * 10000;   // major version
	if (!ok)
		return 0;      // invalid major version
	ver = ver.mid(i + 1);
	if ((i = ver.find('.')) > 0)
	{
		int v = ver.left(i).toInt(&ok);   // minor version
		if (ok)
		{
			version += (v < 99 ? v : 99) * 100;
			ver = ver.mid(i + 1);
			if (ver.at(0).isDigit())
			{
				// Allow other characters to follow last digit
				v = ver.toInt();   // issue number
				version += (v < 99 ? v : 99);
				for (i = 1;  const_cast<const QString&>(ver).at(i).isDigit();  ++i) ;
				subVersion = ver.mid(i);
			}
		}
	}
	else
	{
		// There is no issue number
		if (ver.at(0).isDigit())
		{
			// Allow other characters to follow last digit
			int v = ver.toInt();   // minor number
			version += (v < 99 ? v : 99) * 100;
		}
	}
	return version;
}

/******************************************************************************
 * Check whether the calendar file has its times stored as UTC times,
 * indicating that it was written by the KDE 3.0.0 version of KAlarm 0.5.7.
 * Reply = true if times are stored in UTC
 *       = false if the calendar is a vCalendar, times are not UTC, or any error occurred.
 */
bool CalendarCompat::isUTC(const QString& localFile)
{
	// Read the calendar file into a QString
	QFile file(localFile);
	if (!file.open(IO_ReadOnly))
		return false;
	QTextStream ts(&file);
	ts.setEncoding(QTextStream::UnicodeUTF8);
	QString text = ts.read();
	file.close();

	// Extract the CREATED property for the first VEVENT from the calendar
	bool result = false;
	icalcomponent* calendar = icalcomponent_new_from_string(text.local8Bit().data());
	if (calendar)
	{
		if (icalcomponent_isa(calendar) == ICAL_VCALENDAR_COMPONENT)
		{
			icalcomponent* c = icalcomponent_get_first_component(calendar, ICAL_VEVENT_COMPONENT);
			if (c)
			{
				icalproperty* p = icalcomponent_get_first_property(c, ICAL_CREATED_PROPERTY);
				if (p)
				{
					struct icaltimetype datetime = icalproperty_get_created(p);
					if (datetime.is_utc)
						result = true;
				}
			}
		}
		icalcomponent_free(calendar);
	}
	return result;
}
