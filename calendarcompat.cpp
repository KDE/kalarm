/*
 *  calendarcompat.cpp -  compatibility for old calendar file formats
 *  Program:  kalarm
 *  Copyright © 2001-2009 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"   //krazy:exclude=includes (kalarm.h must be first)
#include "calendarcompat.h"

#include "alarmevent.h"
#include "alarmresource.h"
#include "functions.h"
#include "preferences.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <kapplication.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include <kcal/calendarlocal.h>


using namespace KCal;

static const QByteArray VERSION_PROPERTY("VERSION");     // X-KDE-KALARM-VERSION VCALENDAR property


/******************************************************************************
* Write the X-KDE-KALARM-VERSION custom property into the calendar.
*/
void CalendarCompat::setID(KCal::CalendarLocal& calendar)
{
	calendar.setCustomProperty(KCalendar::APPNAME, VERSION_PROPERTY, KAlarm::currentCalendarVersionString());
}

/******************************************************************************
* Find the version of KAlarm which wrote the calendar file, and do any
* necessary conversions to the current format. If it is a resource calendar,
* the user is prompted whether to save the conversions. For a local calendar
* file, any conversions will only be saved if changes are made later.
* Reply = true if the calendar file is now in the current format.
*/
KCalendar::Status CalendarCompat::fix(KCal::CalendarLocal& calendar, const QString& localFile, AlarmResource* resource, AlarmResource::FixFunc conv)
{
	bool version057_UTC = false;
	QString subVersion, versionString;
	int version = readKAlarmVersion(calendar, localFile, subVersion, versionString);
	if (!version)
		return KCalendar::Current;     // calendar is in current KAlarm format
	if (version < 0  ||  version > KAlarm::Version())
		return KCalendar::Incompatible;    // calendar was created by another program, or an unknown version of KAlarm

	// Calendar was created by an earlier version of KAlarm.
	// Convert it to the current format, and prompt the user whether to update the calendar file.
	if (version == KAlarm::Version(0,5,7)  &&  !localFile.isEmpty())
	{
		// KAlarm version 0.5.7 - check whether times are stored in UTC, in which
		// case it is the KDE 3.0.0 version, which needs adjustment of summer times.
		version057_UTC = isUTC(localFile);
		kDebug() << "KAlarm version 0.5.7 (" << (version057_UTC ?"" :"non-") << "UTC)";
	}
	else
		kDebug() << "KAlarm version" << version;

	// Convert events to current KAlarm format for if the calendar is saved
	KAEvent::convertKCalEvents(calendar, version, version057_UTC);
	if (!resource)
		return KCalendar::Current;    // update non-shared calendars regardless
	if (resource->ResourceCached::readOnly()  ||  conv == AlarmResource::NO_CONVERT)
		return KCalendar::Convertible;
	// Update the calendar file now if the user wants it to be read-write
	if (conv == AlarmResource::PROMPT  ||  conv == AlarmResource::PROMPT_PART)
	{
		QString msg = (conv == AlarmResource::PROMPT)
		            ? i18nc("@info", "Resource <resource>%1</resource> is in an old format (<application>KAlarm</application> version %2), and will be read-only unless "
		                   "you choose to update it to the current format.", resource->resourceName(), versionString)
		            : i18nc("@info", "Some or all of the alarms in resource <resource>%1</resource> are in an old <application>KAlarm</application> format, and will be read-only unless "
		                   "you choose to update them to the current format.", resource->resourceName());
		if (KMessageBox::warningYesNo(0,
		      i18nc("@info", "<para>%1</para><para>"
		           "<warning>Do not update the resource if it is shared with other users who run an older version "
		           "of <application>KAlarm</application>. If you do so, they may be unable to use it any more.</warning></para>"
		           "<para>Do you wish to update the resource?</para>", msg))
		    != KMessageBox::Yes)
			return KCalendar::Convertible;
	}
	calendar.setCustomProperty(KCalendar::APPNAME, VERSION_PROPERTY, QLatin1String(KALARM_VERSION));
	return KCalendar::Converted;
}

/******************************************************************************
* Return the KAlarm version which wrote the calendar which has been loaded.
* The format is, for example, 000507 for 0.5.7.
* Reply = 0 if the calendar was created by the current version of KAlarm
*       = -1 if it was created by KAlarm pre-0.3.5, or another program
*       = version number if created by another KAlarm version.
*/
int CalendarCompat::readKAlarmVersion(KCal::CalendarLocal& calendar, const QString& localFile, QString& subVersion, QString& versionString)
{
	subVersion.clear();
	versionString = calendar.customProperty(KCalendar::APPNAME, VERSION_PROPERTY);
	if (versionString.isEmpty())
	{
		// Pre-KAlarm 1.4 defined the KAlarm version number in the PRODID field.
		// If another application has written to the file, this may not be present.
		const QString prodid = calendar.productId();
		if (prodid.isEmpty())
		{
			// Check whether the calendar file is empty, in which case
			// it can be written to freely.
			QFileInfo fi(localFile);
			if (!fi.size())
				return 0;
		}

		// Find the KAlarm identifier
		QString progname = QLatin1String(" KAlarm ");
		int i = prodid.indexOf(progname, 0, Qt::CaseInsensitive);
		if (i < 0)
		{
			// Older versions used KAlarm's translated name in the product ID, which
			// could have created problems using a calendar in different locales.
			progname = QString(" ") + KGlobal::mainComponent().aboutData()->programName() + ' ';
			i = prodid.indexOf(progname, 0, Qt::CaseInsensitive);
			if (i < 0)
				return -1;    // calendar wasn't created by KAlarm
		}

		// Extract the KAlarm version string
		versionString = prodid.mid(i + progname.length()).trimmed();
		i = versionString.indexOf('/');
		int j = versionString.indexOf(' ');
		if (j >= 0  &&  j < i)
			i = j;
		if (i <= 0)
			return -1;    // missing version string
		versionString = versionString.left(i);   // 'versionString' now contains the KAlarm version string
	}
	if (versionString == KAlarm::currentCalendarVersionString())
		return 0;      // the calendar is in the current KAlarm format
	int ver = KAlarm::getVersionNumber(versionString, &subVersion);
	if (ver >= KAlarm::currentCalendarVersion()  &&  ver <= KAlarm::Version())
		return 0;      // the calendar is in the current KAlarm format
	return KAlarm::getVersionNumber(versionString, &subVersion);
}

/******************************************************************************
* Check whether the calendar file has its times stored as UTC times,
* indicating that it was written by the KDE 3.0.0 version of KAlarm 0.5.7.
* Reply = true if times are stored in UTC
*       = false if the calendar is a vCalendar, times are not UTC, or any error occurred.
*/
bool CalendarCompat::isUTC(const QString& localFile)
{
	// Read the calendar file into a string
	QFile file(localFile);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	QTextStream ts(&file);
	ts.setCodec("ISO 8859-1");
	QByteArray text = ts.readAll().toLocal8Bit();
	file.close();

	// Extract the CREATED property for the first VEVENT from the calendar
	const QByteArray BEGIN_VCALENDAR("BEGIN:VCALENDAR");
	const QByteArray BEGIN_VEVENT("BEGIN:VEVENT");
	const QByteArray CREATED("CREATED:");
	QList<QByteArray> lines = text.split('\n');
	for (int i = 0, end = lines.count();  i < end;  ++i)
	{
		if (lines[i].startsWith(BEGIN_VCALENDAR))
		{
			while (++i < end)
			{
				if (lines[i].startsWith(BEGIN_VEVENT))
				{
					while (++i < end)
					{
						if (lines[i].startsWith(CREATED))
							return lines[i].endsWith('Z');
					}
				}
			}
			break;
		}
	}
	return false;
}

