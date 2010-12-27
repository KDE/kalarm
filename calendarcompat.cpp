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

#ifdef USE_AKONADI
#include "akonadimodel.h"
#include "collectionmodel.h"
#else
#include "alarmresource.h"
#endif
#include "functions.h"
#include "kaevent.h"
#include "preferences.h"

#ifndef USE_AKONADI
#include <kcal/calendarlocal.h>
using namespace KCal;
#endif

#include <kapplication.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

static const QByteArray VERSION_PROPERTY("VERSION");     // X-KDE-KALARM-VERSION VCALENDAR property


/******************************************************************************
* Find the version of KAlarm which wrote the calendar file, and do any
* necessary conversions to the current format. If it is a resource calendar,
* the user is prompted whether to save the conversions. For a local calendar
* file, any conversions will only be saved if changes are made later.
* If the calendar only contains the wrong alarm types, 'wrongType' is set true.
* Reply = true if the calendar file is now in the current format.
*/
#ifdef USE_AKONADI
KAlarm::Calendar::Compat CalendarCompat::fix(const KCalCore::FileStorage::Ptr& fileStorage,
                                      const Akonadi::Collection& collection, FixFunc conv, bool* wrongType)
#else
KAlarm::Calendar::Compat CalendarCompat::fix(KCal::CalendarLocal& calendar, const QString& localFile, AlarmResource* resource,
                                      AlarmResource::FixFunc conv, bool* wrongType)
#endif
{
    if (wrongType)
        *wrongType = false;
    QString versionString;
#ifdef USE_AKONADI
    int version = KAlarm::Calendar::checkCompatibility(fileStorage, versionString);
#else
    int version = KAlarm::Calendar::checkCompatibility(calendar, localFile, versionString);
#endif
    if (version < 0)
        return KAlarm::Calendar::Incompatible;    // calendar was created by another program, or an unknown version of KAlarm
#ifdef USE_AKONADI
    if (!collection.isValid())
#else
    if (!resource)
#endif
        return KAlarm::Calendar::Current;    // update non-shared calendars regardless

    // Check whether the alarm types in the calendar correspond with the resource's alarm type
#ifdef USE_AKONADI
    KCalCore::Calendar::Ptr calendar = fileStorage->calendar();
    if (wrongType)
        *wrongType = !AkonadiModel::checkAlarmTypes(collection, calendar);
#else
    if (wrongType)
        *wrongType = !resource->checkAlarmTypes(calendar);
#endif

    if (!version)
        return KAlarm::Calendar::Current;     // calendar is in current KAlarm format
#ifdef USE_AKONADI
    if (!CollectionControlModel::isWritable(collection)  ||  conv == NO_CONVERT)
#else
    if (resource->ResourceCached::readOnly()  ||  conv == AlarmResource::NO_CONVERT)
#endif
        return KAlarm::Calendar::Convertible;
    // Update the calendar file now if the user wants it to be read-write
#ifdef USE_AKONADI
    if (conv == PROMPT  ||  conv == PROMPT_PART)
#else
    if (conv == AlarmResource::PROMPT  ||  conv == AlarmResource::PROMPT_PART)
#endif
    {
#ifdef USE_AKONADI
        QString msg = (conv == PROMPT)
#else
        QString msg = (conv == AlarmResource::PROMPT)
#endif
                    ? i18nc("@info", "Calendar <resource>%1</resource> is in an old format (<application>KAlarm</application> version %2), and will be read-only unless "
                           "you choose to update it to the current format.",
#ifdef USE_AKONADI
                           collection.name(), versionString)
#else
                           resource->resourceName(), versionString)
#endif
                    : i18nc("@info", "Some or all of the alarms in calendar <resource>%1</resource> are in an old <application>KAlarm</application> format, and will be read-only unless "
                           "you choose to update them to the current format.",
#ifdef USE_AKONADI
                           collection.name());
#else
                           resource->resourceName());
#endif
        if (KMessageBox::warningYesNo(0,
              i18nc("@info", "<para>%1</para><para>"
                   "<warning>Do not update the calendar if it is shared with other users who run an older version "
                   "of <application>KAlarm</application>. If you do so, they may be unable to use it any more.</warning></para>"
                   "<para>Do you wish to update the calendar?</para>", msg))
            != KMessageBox::Yes)
            return KAlarm::Calendar::Convertible;
    }
    KAlarm::Calendar::setKAlarmVersion(calendar);
    return KAlarm::Calendar::Converted;
}

// vim: et sw=4:
