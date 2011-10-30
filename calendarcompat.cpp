/*
 *  calendarcompat.cpp -  compatibility for old calendar file formats
 *  Program:  kalarm
 *  Copyright © 2001-2011 by David Jarvie <djarvie@kde.org>
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

#include "calendarcompat.h"
#include "functions.h"
#include "mainwindow.h"
#include "messagebox.h"

#include <kcal/calendarlocal.h>

#include <kdebug.h>

namespace CalendarCompat
{

/******************************************************************************
* Find the version of KAlarm which wrote the calendar file, and do any
* necessary conversions to the current format. If it is a resource calendar,
* the user is prompted whether to save the conversions. For a local calendar
* file, any conversions will only be saved if changes are made later.
* If the calendar only contains the wrong alarm types, 'wrongType' is set true.
* Reply = true if the calendar file is now in the current format.
*/
KACalendar::Compat fix(KCal::CalendarLocal& calendar, const QString& localFile, AlarmResource* resource,
                             AlarmResource::FixFunc conv, bool* wrongType)
{
    if (wrongType)
        *wrongType = false;
    QString versionString;
    int version = KACalendar::updateVersion(calendar, localFile, versionString);
    if (version == KACalendar::IncompatibleFormat)
        return KACalendar::Incompatible;    // calendar was created by another program, or an unknown version of KAlarm
    if (!resource)
        return KACalendar::Current;    // update non-shared calendars regardless

    // Check whether the alarm types in the calendar correspond with the resource's alarm type
    if (wrongType)
        *wrongType = !resource->checkAlarmTypes(calendar);

    if (version == KACalendar::CurrentFormat)
        return KACalendar::Current;     // calendar is in current KAlarm format
    if (resource->ResourceCached::readOnly()  ||  conv == AlarmResource::NO_CONVERT)
        return KACalendar::Convertible;
    // Update the calendar file now if the user wants it to be read-write
    if (conv == AlarmResource::PROMPT  ||  conv == AlarmResource::PROMPT_PART)
    {
        QString msg = KAlarm::conversionPrompt(resource->resourceName(), versionString, (conv == AlarmResource::PROMPT));
        if (KAMessageBox::warningYesNo(MainWindow::mainMainWindow(), msg) != KMessageBox::Yes)
            return KACalendar::Convertible;
    }
    KACalendar::setKAlarmVersion(calendar);
    return KACalendar::Converted;
}

} // namespace CalendarCompat

// vim: et sw=4:
