/*
 *  kacalendar.h  -  KAlarm kcal library calendar and event categorisation
 *  This file is part of kalarmcalendar library, which provides access to KAlarm
 *  calendar data.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "kalarmcal_export.h"

#include <KCalendarCore/FileStorage>
#include <KCalendarCore/Calendar>
#include <KCalendarCore/Event>

#include <QByteArray>
#include <QStringList>
#include <QDebug>

namespace KCalendarCore
{
class Alarm;
}

namespace KAlarmCal
{

extern const QLatin1StringView KALARMCAL_EXPORT MIME_BASE;      //!< The base mime type for KAlarm alarms
extern const QLatin1StringView KALARMCAL_EXPORT MIME_ACTIVE;    //!< The mime type for KAlarm active alarms
extern const QLatin1StringView KALARMCAL_EXPORT MIME_ARCHIVED;  //!< The mime type for KAlarm archived alarms
extern const QLatin1StringView KALARMCAL_EXPORT MIME_TEMPLATE;  //!< The mime type for KAlarm alarm templates

/** Declaration type for a calendar resource ID. */
using ResourceId = qint64;

/**
 * @short Class representing attributes of a KAlarm calendar.
 *
 * KACalendar provides methods to check and convert the KAlarm calendar format
 * version, and to get and set the iCalendar product ID (which contains the
 * identity of the application which wrote the calendar).
 *
 * @author David Jarvie <djarvie@kde.org>
 */
namespace KACalendar
{
/** Compatibility of resource backend calendar format. */
enum Compatibility
{
    Unknown      = 0,               //!< format not determined
    Current      = 0x02,            //!< in current KAlarm format
    Converted    = Current | 0x01,  //!< in current KAlarm format, but not yet saved
    Convertible  = 0x04,            //!< in an older KAlarm format
    Incompatible = 0x08             //!< not written by KAlarm, or in a newer KAlarm version
};
Q_DECLARE_FLAGS(Compat, Compatibility)

/** Special calendar storage format version codes.
 *  Positive version values are actual KAlarm format version numbers.
 */
enum
{
    CurrentFormat      = 0,    //!< current KAlarm format
    MixedFormat        = -2,   //!< calendar may contain more than one version
    IncompatibleFormat = -1    //!< not written by KAlarm, or a newer KAlarm version
};

/** Check the version of KAlarm which wrote a calendar file, and convert
 *  it in memory to the current KAlarm format if possible. The storage
 *  file is not updated. The compatibility of the calendar format is
 *  indicated by the return value.
 *
 *  @param fileStorage    calendar stored in local file
 *  @param versionString  receives calendar's KAlarm version as a string
 *  @return CurrentFormat if the calendar is in the current KAlarm format;
 *          IncompatibleFormat calendar is not a KAlarm format or is an
                               unknown KAlarm format;
 *          >0 the older KAlarm version which wrote the calendar
 */
KALARMCAL_EXPORT int updateVersion(const KCalendarCore::FileStorage::Ptr&, QString& versionString);

/** Set the KAlarm version custom property for a calendar. */
KALARMCAL_EXPORT void setKAlarmVersion(const KCalendarCore::Calendar::Ptr&);

/** Set the program name and version for use in calendars. */
KALARMCAL_EXPORT void setProductId(const QByteArray& progName, const QByteArray& progVersion);

/** Return the product ID string for use in calendars.
 *  setProductId() must have been called previously.
 */
KALARMCAL_EXPORT QByteArray  icalProductId();

extern const QByteArray APPNAME;    //!< The application name ("KALARM") used in calendar properties
} // namespace KACalendar

//

/**
 * @short Class representing type attributes of a KAlarm event.
 *
 * CalEvent provides methods to manipulate a KAEvent UID according to its category
 * (active, archived or template). It also provides methods to access KAEvent
 * mime types.
 *
 * @author David Jarvie <djarvie@kde.org>
 */
namespace CalEvent
{
/** The category of an event, indicated by the middle part of its UID. */
enum Type
{
    EMPTY      = 0,       //!< the event has no alarms
    ACTIVE     = 0x01,    //!< the event is currently active
    ARCHIVED   = 0x02,    //!< the event is archived
    TEMPLATE   = 0x04,    //!< the event is an alarm template
    DISPLAYING = 0x08     //!< the event is currently being displayed
};
Q_DECLARE_FLAGS(Types, Type)

KALARMCAL_EXPORT QString uid(const QString& id, Type);
KALARMCAL_EXPORT Type    status(const KCalendarCore::Event::Ptr&, QString* param = nullptr);
KALARMCAL_EXPORT void    setStatus(const KCalendarCore::Event::Ptr&, Type, const QString& param = QString());

/** Return the alarm Type for a mime type string. */
KALARMCAL_EXPORT Type    type(const QString& mimeType);
/** Return the alarm Types for a list of mime type strings. */
KALARMCAL_EXPORT Types   types(const QStringList& mimeTypes);
/** Return the mime type string corresponding to an alarm Type. */
KALARMCAL_EXPORT QString mimeType(Type);
/** Return the mime type strings corresponding to alarm Types. */
KALARMCAL_EXPORT QStringList mimeTypes(Types);
} // namespace CalEvent

Q_DECLARE_OPERATORS_FOR_FLAGS(CalEvent::Types)

} // namespace KAlarmCal

KALARMCAL_EXPORT QDebug operator<<(QDebug, KAlarmCal::CalEvent::Type);

// vim: et sw=4:
