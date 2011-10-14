/*
 *  kacalendar.h  -  KAlarm kcal library calendar and event categorisation
 *  Program:  kalarm
 *  Copyright © 2005-2011 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#ifndef KALARM_KACALENDAR_H
#define KALARM_KACALENDAR_H

#include "kalarm_cal_export.h"
#ifndef USE_KRESOURCES
#include "kcalcore_constptr.h"
#include <kcalcore/filestorage.h>
#include <kcalcore/calendar.h>
#include <kcalcore/event.h>
#include <akonadi/collection.h>
#endif
#include <QByteArray>
#include <QStringList>

#ifndef USE_KRESOURCES
namespace KCalCore {
  class Alarm;
}
#else
namespace KCal {
  class Event;
  class Alarm;
  class CalendarLocal;
}
#endif

namespace KAlarm
{

#ifndef USE_KRESOURCES
extern const QLatin1String KALARM_CAL_EXPORT MIME_BASE;      //!< The base mime type for KAlarm alarms
extern const QLatin1String KALARM_CAL_EXPORT MIME_ACTIVE;    //!< The mime type for KAlarm active alarms
extern const QLatin1String KALARM_CAL_EXPORT MIME_ARCHIVED;  //!< The mime type for KAlarm archived alarms
extern const QLatin1String KALARM_CAL_EXPORT MIME_TEMPLATE;  //!< The mime type for KAlarm alarm templates
#endif

/**
 * @short Class representing type attributes of a KAlarm event.
 *
 * CalEvent provides methods to manipulate a KAEvent UID according to its category
 * (active, archived or template). It also provides methods to access KAEvent
 * mime types.
 *
 * @author David Jarvie <djarvie@kde.org>
 */
class KALARM_CAL_EXPORT CalEvent
{
    public:
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
        /** All main event categories (ACTIVE, ARCHIVE and TEMPLATE only). */
        static const Types ALL;

        static QString uid(const QString& id, Type);
#ifndef USE_KRESOURCES
        static Type    status(const KCalCore::ConstEventPtr&, QString* param = 0);
        static void    setStatus(const KCalCore::Event::Ptr&, Type, const QString& param = QString());

        /** Return the alarm Type for a mime type string. */
        static Type    type(const QString& mimeType);
        /** Return the alarm Types for a list of mime type strings. */
        static Types   types(const QStringList& mimeTypes);
        /** Return the mime type string corresponding to an alarm Type. */
        static QString mimeType(Type);
        /** Return the mime type strings corresponding to alarm Types. */
        static QStringList mimeTypes(Types);
#else
        static Type    status(const KCal::Event*, QString* param = 0);
        static void    setStatus(KCal::Event*, Type, const QString& param = QString());
#endif
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CalEvent::Types)


/**
 * @short Class representing attributes of a KAlarm calendar.
 *
 * KACalendar provides methods to check and convert the KAlarm calendar format
 * version, and to get and set the iCalendar product ID (which contains the
 * identity of the application which wrote the calendar).
 *
 * @author David Jarvie <djarvie@kde.org>
 */
class KALARM_CAL_EXPORT KACalendar
{
    public:
#ifndef USE_KRESOURCES
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
#else
        /** Compatibility of resource calendar format. */
        enum Compat
        {
            Current,       //!< in current KAlarm format
            Converted,     //!< in current KAlarm format, but not yet saved
            Convertible,   //!< in an older KAlarm format
            Incompatible,  //!< not written by KAlarm, or in a newer KAlarm version
            ByEvent        //!< individual events have their own compatibility status
        };
#endif

        /** Special calendar storage format version codes.
         *  Positive version values are actual KAlarm format version numbers.
         */
        enum
        {
            CurrentFormat      = 0,    //!< current KAlarm format
#ifndef USE_KRESOURCES
            MixedFormat        = -2,   //!< calendar may contain more than one version
#endif
            IncompatibleFormat = -1    //!< not written by KAlarm, or a newer KAlarm version
        };

        /** Check the version of KAlarm which wrote a calendar file, and convert
         *  it in memory to the current KAlarm format if possible. The storage
         *  file is not updated. The compatibility of the calendar format is
         *  indicated by the return value.
         *
         *  @param calendar       calendar stored in @p localFile
         *  @param localFile      full path of the calendar's file storage
         *  @param versionString  receives calendar's KAlarm version as a string
         *  @return CurrentFormat if the calendar is in the current KAlarm format;
         *          IncompatibleFormat calendar is not a KAlarm format or is an
                                       unknown KAlarm format;
         *          >0 the older KAlarm version which wrote the calendar
         */
#ifndef USE_KRESOURCES
        static int updateVersion(const KCalCore::FileStorage::Ptr&, QString& versionString);
#else
        static int updateVersion(KCal::CalendarLocal& calendar, const QString& localFile, QString& versionString);
#endif

        /** Return a prompt string to ask the user whether to convert the calendar to the
         *  current format.
         *  @param whole if true, the whole calendar needs to be converted; else only some
         *               alarms may need to be converted.
         */
        static QString conversionPrompt(const QString& calendarName, const QString& calendarVersion, bool whole);

#ifndef USE_KRESOURCES
        /** Set the KAlarm version custom property for a calendar. */
        static void setKAlarmVersion(const KCalCore::Calendar::Ptr&);
#else
        static void setKAlarmVersion(KCal::CalendarLocal&);
#endif

        /** Set the program name and version for use in calendars. */
        static void setProductId(const QByteArray& progName, const QByteArray& progVersion);

        /** Return the product ID string for use in calendars.
         *  setProductId() must have been called previously.
         */
        static QByteArray  icalProductId();

        static const QByteArray APPNAME;           //!< The application name ("KALARM") used in calendar properties
};

} // namespace KAlarm

#endif // KALARM_KACALENDAR_H

// vim: et sw=4:
