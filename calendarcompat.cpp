/*
 *  calendarcompat.cpp -  compatibility for old calendar file formats
 *  Program:  kalarm
 *  Copyright © 2001-2010 by David Jarvie <djarvie@kde.org>
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

#include "alarmresource.h"
#include "functions.h"
#include "kaevent.h"
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
	calendar.setCustomProperty(KCalendar::APPNAME, VERSION_PROPERTY, QString::fromLatin1(KAEvent::currentCalendarVersionString()));
}

/******************************************************************************
* Find the version of KAlarm which wrote the calendar file, and do any
* necessary conversions to the current format. If it is a resource calendar,
* the user is prompted whether to save the conversions. For a local calendar
* file, any conversions will only be saved if changes are made later.
* If the calendar only contains the wrong alarm types, 'wrongType' is set true.
* Reply = true if the calendar file is now in the current format.
*/
KCalendar::Status CalendarCompat::fix(KCal::CalendarLocal& calendar, const QString& localFile, AlarmResource* resource,
                                      AlarmResource::FixFunc conv, bool* wrongType)
{
	if (wrongType)
		*wrongType = false;
	QString versionString;
	int version = KCalendar::checkCompatibility(calendar, localFile, versionString);
	if (version < 0)
		return KCalendar::Incompatible;    // calendar was created by another program, or an unknown version of KAlarm
	if (!resource)
		return KCalendar::Current;    // update non-shared calendars regardless

	// Check whether the alarm types in the calendar correspond with the resource's alarm type
	if (wrongType)
		*wrongType = !resource->checkAlarmTypes(calendar);

	if (!version)
		return KCalendar::Current;     // calendar is in current KAlarm format
	if (resource->ResourceCached::readOnly()  ||  conv == AlarmResource::NO_CONVERT)
		return KCalendar::Convertible;
	// Update the calendar file now if the user wants it to be read-write
	if (conv == AlarmResource::PROMPT  ||  conv == AlarmResource::PROMPT_PART)
	{
		QString msg = (conv == AlarmResource::PROMPT)
		            ? i18nc("@info", "Calendar <resource>%1</resource> is in an old format (<application>KAlarm</application> version %2), and will be read-only unless "
		                   "you choose to update it to the current format.", resource->resourceName(), versionString)
		            : i18nc("@info", "Some or all of the alarms in calendar <resource>%1</resource> are in an old <application>KAlarm</application> format, and will be read-only unless "
		                   "you choose to update them to the current format.", resource->resourceName());
		if (KMessageBox::warningYesNo(0,
		      i18nc("@info", "<para>%1</para><para>"
		           "<warning>Do not update the calendar if it is shared with other users who run an older version "
		           "of <application>KAlarm</application>. If you do so, they may be unable to use it any more.</warning></para>"
		           "<para>Do you wish to update the calendar?</para>", msg))
		    != KMessageBox::Yes)
			return KCalendar::Convertible;
	}
	calendar.setCustomProperty(KCalendar::APPNAME, VERSION_PROPERTY, QLatin1String(KALARM_VERSION));
	return KCalendar::Converted;
}
