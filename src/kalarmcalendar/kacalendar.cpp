/*
 *  kacalendar.cpp  -  KAlarm kcal library calendar and event functions
 *  This file is part of kalarmcalendar library, which provides access to KAlarm
 *  calendar data.
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kacalendar.h"

#include "kaevent.h"
#include "version.h"
#include "kalarmcal_debug.h"

#include <KCalendarCore/Alarm>
#include <KCalendarCore/CalFormat>

#include <KLocalizedString>

#include <QFile>
#include <QFileInfo>
#include <QMap>
using namespace Qt::Literals::StringLiterals;

using namespace KCalendarCore;

namespace KAlarmCal
{

const QLatin1StringView MIME_BASE("application/x-vnd.kde.alarm");
const QLatin1StringView MIME_ACTIVE("application/x-vnd.kde.alarm.active");
const QLatin1StringView MIME_ARCHIVED("application/x-vnd.kde.alarm.archived");
const QLatin1StringView MIME_TEMPLATE("application/x-vnd.kde.alarm.template");

static const QByteArray VERSION_PROPERTY("VERSION");     // X-KDE-KALARM-VERSION VCALENDAR property

class Private
{
public:
    static int readKAlarmVersion(const FileStorage::Ptr&, QString& subVersion, QString& versionString);

    static QByteArray mIcalProductId;
};

QByteArray Private::mIcalProductId;

//=============================================================================

namespace KACalendar
{

const QByteArray APPNAME("KALARM");

void setProductId(const QByteArray& progName, const QByteArray& progVersion)
{
    Private::mIcalProductId = QByteArray("-//K Desktop Environment//NONSGML " + progName + " " + progVersion + "//EN");
    KCalendarCore::CalFormat::setApplication(QString::fromLatin1(progName), QString::fromLatin1(Private::mIcalProductId));
}

QByteArray icalProductId()
{
    return Private::mIcalProductId.isEmpty() ? QByteArray("-//K Desktop Environment//NONSGML  //EN") : Private::mIcalProductId;
}

/******************************************************************************
* Set the X-KDE-KALARM-VERSION property in a calendar.
*/
void setKAlarmVersion(const Calendar::Ptr& calendar)
{
    calendar->setCustomProperty(APPNAME, VERSION_PROPERTY, QString::fromLatin1(KAEvent::currentCalendarVersionString()));
}

/******************************************************************************
* Check the version of KAlarm which wrote a calendar file, and convert it in
* memory to the current KAlarm format if possible. The storage file is not
* updated. The compatibility of the calendar format is indicated by the return
* value.
*/
int updateVersion(const FileStorage::Ptr& fileStorage, QString& versionString)
{
    QString subVersion;
    const int version = Private::readKAlarmVersion(fileStorage, subVersion, versionString);
    if (version == CurrentFormat)
        return CurrentFormat;    // calendar is in the current KAlarm format
    if (version == IncompatibleFormat  ||  version > KAEvent::currentCalendarVersion())
        return IncompatibleFormat;    // calendar was created by another program, or an unknown version of KAlarm

    // Calendar was created by an earlier version of KAlarm.
    // Convert events to current KAlarm format for when/if the calendar is saved.
    qCDebug(KALARMCAL_LOG) << "KAlarm version" << version;
    KAEvent::convertKCalEvents(fileStorage->calendar(), version);
    // Set the new calendar version.
    setKAlarmVersion(fileStorage->calendar());
    return version;
}

} // namespace KACalendar

/******************************************************************************
* Return the KAlarm version which wrote the calendar which has been loaded.
* The format is, for example, 000507 for 0.5.7.
* Reply = CurrentFormat if the calendar was created by the current version of KAlarm
*       = IncompatibleFormat if it was created by KAlarm pre-0.3.5, or another program
*       = version number if created by another KAlarm version.
*/
int Private::readKAlarmVersion(const FileStorage::Ptr& fileStorage, QString& subVersion, QString& versionString)
{
    subVersion.clear();
    Calendar::Ptr calendar = fileStorage->calendar();
    versionString = calendar->customProperty(KACalendar::APPNAME, VERSION_PROPERTY);
    qCDebug(KALARMCAL_LOG) << "File=" << fileStorage->fileName() << ", version=" << versionString;

    if (versionString.isEmpty())
    {
        // Pre-KAlarm 1.4 defined the KAlarm version number in the PRODID field.
        // If another application has written to the file, this may not be present.
        const QString prodid = calendar->productId();
        if (prodid.isEmpty())
        {
            // Check whether the calendar file is empty, in which case
            // it can be written to freely.
            const QFileInfo fi(fileStorage->fileName());
            if (!fi.size())
                return KACalendar::CurrentFormat;
        }

        // Find the KAlarm identifier
        QString progname = QStringLiteral(" KAlarm ");
        int i = prodid.indexOf(progname, 0, Qt::CaseInsensitive);
        if (i < 0)
        {
            // Older versions used KAlarm's translated name in the product ID, which
            // could have created problems using a calendar in different locales.
            progname = " "_L1 + i18n("KAlarm") + ' '_L1;
            i = prodid.indexOf(progname, 0, Qt::CaseInsensitive);
            if (i < 0)
                return KACalendar::IncompatibleFormat;    // calendar wasn't created by KAlarm
        }

        // Extract the KAlarm version string
        versionString = prodid.mid(i + progname.length()).trimmed();
        i = versionString.indexOf('/'_L1);
        const int j = versionString.indexOf(' '_L1);
        if (j >= 0  &&  j < i)
            i = j;
        if (i <= 0)
            return KACalendar::IncompatibleFormat;    // missing version string
        versionString.truncate(i);   // 'versionString' now contains the KAlarm version string
    }
    if (versionString == QLatin1StringView(KAEvent::currentCalendarVersionString()))
        return KACalendar::CurrentFormat;    // the calendar is in the current KAlarm format
    const int ver = KAlarmCal::getVersionNumber(versionString, &subVersion);
    if (ver == KAEvent::currentCalendarVersion())
        return KACalendar::CurrentFormat;    // the calendar is in the current KAlarm format
    return KAlarmCal::getVersionNumber(versionString, &subVersion);
}

//=============================================================================

namespace CalEvent
{

// Struct to contain static strings, to allow use of Q_GLOBAL_STATIC
// to delete them on program termination.
struct StaticStrings
{
    StaticStrings()
        : STATUS_PROPERTY("TYPE")
        , ACTIVE_STATUS(QStringLiteral("ACTIVE"))
        , TEMPLATE_STATUS(QStringLiteral("TEMPLATE"))
        , ARCHIVED_STATUS(QStringLiteral("ARCHIVED"))
        , DISPLAYING_STATUS(QStringLiteral("DISPLAYING"))
        , ARCHIVED_UID(QStringLiteral("exp-"))
        , DISPLAYING_UID(QStringLiteral("disp-"))
        , OLD_ARCHIVED_UID(QStringLiteral("-exp-"))
        , OLD_TEMPLATE_UID(QStringLiteral("-tmpl-"))
    {}
    // Event custom properties.
    // Note that all custom property names are prefixed with X-KDE-KALARM- in the calendar file.
    const QByteArray STATUS_PROPERTY;    // X-KDE-KALARM-TYPE property
    const QString ACTIVE_STATUS;
    const QString TEMPLATE_STATUS;
    const QString ARCHIVED_STATUS;
    const QString DISPLAYING_STATUS;

    // Event ID identifiers
    const QString ARCHIVED_UID;
    const QString DISPLAYING_UID;

    // Old KAlarm format identifiers
    const QString OLD_ARCHIVED_UID;
    const QString OLD_TEMPLATE_UID;
};
Q_GLOBAL_STATIC(StaticStrings, staticStrings)

/******************************************************************************
* Convert a unique ID to indicate that the event is in a specified calendar file.
* This is done by prefixing archived or displaying alarms with "exp-" or "disp-",
* while active alarms have no prefix.
* Note that previously, "-exp-" was inserted in the middle of the UID.
*/
QString uid(const QString& id, Type status)
{
    QString result = id;
    Type oldType;
    int i;
    int len;
    if (result.startsWith(staticStrings->ARCHIVED_UID))
    {
        oldType = ARCHIVED;
        len = staticStrings->ARCHIVED_UID.length();
    }
    else if (result.startsWith(staticStrings->DISPLAYING_UID))
    {
        oldType = DISPLAYING;
        len = staticStrings->DISPLAYING_UID.length();
    }
    else
    {
        if ((i = result.indexOf(staticStrings->OLD_ARCHIVED_UID)) > 0)
            result.remove(i, staticStrings->OLD_ARCHIVED_UID.length());
        oldType = ACTIVE;
        len = 0;
    }
    if (status != oldType)
    {
        QString part;
        switch (status)
        {
            case ARCHIVED:    part = staticStrings->ARCHIVED_UID;  break;
            case DISPLAYING:  part = staticStrings->DISPLAYING_UID;  break;
            case ACTIVE:
                break;
            case TEMPLATE:
            case EMPTY:
            default:
                return result;
        }
        result.replace(0, len, part);
    }
    return result;
}

/******************************************************************************
* Check an event to determine its type - active, archived, template or empty.
* The default type is active if it contains alarms and there is nothing to
* indicate otherwise.
* Note that the mere fact that all an event's alarms have passed does not make
* an event archived, since it may be that they have not yet been able to be
* triggered. They will be archived once KAlarm tries to handle them.
* Do not call this function for the displaying alarm calendar.
*/
Type status(const Event::Ptr& event, QString* param)
{
    // Set up a static quick lookup for type strings
    using PropertyMap = QMap<QString, Type>;
    static PropertyMap properties;
    if (properties.isEmpty())
    {
        properties[staticStrings->ACTIVE_STATUS]     = ACTIVE;
        properties[staticStrings->TEMPLATE_STATUS]   = TEMPLATE;
        properties[staticStrings->ARCHIVED_STATUS]   = ARCHIVED;
        properties[staticStrings->DISPLAYING_STATUS] = DISPLAYING;
    }

    if (param)
        param->clear();
    if (!event)
        return EMPTY;
    const Alarm::List alarms = event->alarms();
    if (alarms.isEmpty())
        return EMPTY;

    const QString property = event->customProperty(KACalendar::APPNAME, staticStrings->STATUS_PROPERTY);
    if (!property.isEmpty())
    {
        // There's a X-KDE-KALARM-TYPE property.
        // It consists of the event type, plus an optional parameter.
        PropertyMap::ConstIterator it = properties.constFind(property);
        if (it != properties.constEnd())
            return it.value();
        const int i = property.indexOf(';'_L1);
        if (i < 0)
            return EMPTY;
        it = properties.constFind(property.left(i));
        if (it == properties.constEnd())
            return EMPTY;
        if (param)
            *param = property.mid(i + 1);
        return it.value();
    }

    // The event either wasn't written by KAlarm, or was written by a pre-2.0 version.
    // Check first for an old KAlarm format, which indicated the event type in the
    // middle of its UID.
    const QString euid = event->uid();
    if (euid.indexOf(staticStrings->OLD_ARCHIVED_UID) > 0)
        return ARCHIVED;
    if (euid.indexOf(staticStrings->OLD_TEMPLATE_UID) > 0)
        return TEMPLATE;

    // Otherwise, assume it's an active alarm
    return ACTIVE;
}

/******************************************************************************
* Set the event's type - active, archived, template, etc.
* If a parameter is supplied, it will be appended as a second parameter to the
* custom property.
*/
void setStatus(const Event::Ptr& event, Type status, const QString& param)
{
    if (!event)
        return;
    QString text;
    switch (status)
    {
        case ACTIVE:      text = staticStrings->ACTIVE_STATUS;  break;
        case TEMPLATE:    text = staticStrings->TEMPLATE_STATUS;  break;
        case ARCHIVED:    text = staticStrings->ARCHIVED_STATUS;  break;
        case DISPLAYING:  text = staticStrings->DISPLAYING_STATUS;  break;
        default:
            event->removeCustomProperty(KACalendar::APPNAME, staticStrings->STATUS_PROPERTY);
            return;
    }
    if (!param.isEmpty())
        text += ';'_L1 + param;
    event->setCustomProperty(KACalendar::APPNAME, staticStrings->STATUS_PROPERTY, text);
}

Type type(const QString& mimeType)
{
    if (mimeType == MIME_ACTIVE)
        return ACTIVE;
    if (mimeType == MIME_ARCHIVED)
        return ARCHIVED;
    if (mimeType == MIME_TEMPLATE)
        return TEMPLATE;
    return EMPTY;
}

Types types(const QStringList& mimeTypes)
{
    Types types = {};
    for (const QString& mtype : mimeTypes)
    {
        if (mtype == MIME_ACTIVE)
            types |= ACTIVE;
        else if (mtype == MIME_ARCHIVED)
            types |= ARCHIVED;
        else if (mtype == MIME_TEMPLATE)
            types |= TEMPLATE;
    }
    return types;
}

QString mimeType(Type mtype)
{
    switch (mtype)
    {
        case ACTIVE:    return MIME_ACTIVE;
        case ARCHIVED:  return MIME_ARCHIVED;
        case TEMPLATE:  return MIME_TEMPLATE;
        default:
            return {};
    }
}

QStringList mimeTypes(Types types)
{
    QStringList mimes;
    for (int i = 1;  types;  i <<= 1)
    {
        if (types & i)
        {
            mimes += mimeType(Type(i));
            types &= ~i;
        }
    }
    return mimes;
}

} // namespace CalEvent

} // namespace KAlarmCal

QDebug operator<<(QDebug debug, KAlarmCal::CalEvent::Type type)
{
    const char* str;
    switch (type)
    {
        case KAlarmCal::CalEvent::ACTIVE:    str = "Active alarms";    break;
        case KAlarmCal::CalEvent::ARCHIVED:  str = "Archived alarms";  break;
        case KAlarmCal::CalEvent::TEMPLATE:  str = "Alarm templates";  break;
        default:        return debug;
    }
    debug << str;
    return debug;
}

// vim: et sw=4:
