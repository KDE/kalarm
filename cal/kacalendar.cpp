/*
 *  kacalendar.cpp  -  KAlarm kcal library calendar and event functions
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

#include "kacalendar.h"

#include "kaevent.h"
#include "version.h"

#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>

#include <kcal/event.h>
#include <kcal/alarm.h>
#include <kcal/calendarlocal.h>

#include <QMap>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>


QByteArray KACalendar::APPNAME = "KALARM";
static const QByteArray VERSION_PROPERTY("VERSION");     // X-KDE-KALARM-VERSION VCALENDAR property

using namespace KCal;

static bool isUTC(const QString& localFile);

/*=============================================================================
* Class: KACalendar
*============================================================================*/

QByteArray KACalendar::mIcalProductId;
bool       KACalendar::mHaveKAlarmCatalog = false;

void KACalendar::setProductId(const QByteArray& progName, const QByteArray& progVersion)
{
    mIcalProductId = QByteArray("-//K Desktop Environment//NONSGML " + progName + " " + progVersion + "//EN");
}

QByteArray KACalendar::icalProductId()
{
    return mIcalProductId.isEmpty() ? QByteArray("-//K Desktop Environment//NONSGML  //EN") : mIcalProductId;
}

/******************************************************************************
* Check the version of KAlarm which wrote a calendar file, and convert it in
* memory to the current KAlarm format if possible. The storage file is not
* updated. The compability of the calendar format is indicated by the return
* value.
*/
int KACalendar::checkCompatibility(CalendarLocal& calendar, const QString& localFile, QString& versionString)
{
    bool version057_UTC = false;
    QString subVersion;
    int version = readKAlarmVersion(calendar, localFile, subVersion, versionString);
    if (!version)
        return 0;     // calendar is in the current KAlarm format
    if (version < 0  ||  version > KAEvent::currentCalendarVersion())
        return -1;    // calendar was created by another program, or an unknown version of KAlarm

    // Calendar was created by an earlier version of KAlarm.
    // Convert it to the current format.
    if (version == KAlarm::Version(0,5,7)  &&  !localFile.isEmpty())
    {
        // KAlarm version 0.5.7 - check whether times are stored in UTC, in which
        // case it is the KDE 3.0.0 version, which needs adjustment of summer times.
        version057_UTC = isUTC(localFile);
        kDebug() << "KAlarm version 0.5.7 (" << (version057_UTC ?"" :"non-") << "UTC)";
    }
    else
        kDebug() << "KAlarm version" << version;

    // Convert events to current KAlarm format for when/if the calendar is saved
    KAEvent::convertKCalEvents(calendar, version, version057_UTC);
    return version;
}

/******************************************************************************
* Return the KAlarm version which wrote the calendar which has been loaded.
* The format is, for example, 000507 for 0.5.7.
* Reply = 0 if the calendar was created by the current version of KAlarm
*       = -1 if it was created by KAlarm pre-0.3.5, or another program
*       = version number if created by another KAlarm version.
*/
int KACalendar::readKAlarmVersion(CalendarLocal& calendar, const QString& localFile, QString& subVersion, QString& versionString)
{
    subVersion.clear();
    versionString = calendar.customProperty(APPNAME, VERSION_PROPERTY);
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
            insertKAlarmCatalog();
            progname = QString(" ") + i18n("KAlarm") + ' ';
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
    if (versionString == KAEvent::currentCalendarVersionString())
        return 0;      // the calendar is in the current KAlarm format
    int ver = KAlarm::getVersionNumber(versionString, &subVersion);
    if (ver == KAEvent::currentCalendarVersion())
        return 0;      // the calendar is in the current KAlarm format
    return KAlarm::getVersionNumber(versionString, &subVersion);
}

/******************************************************************************
* Access the KAlarm message translation catalog.
*/
void KACalendar::insertKAlarmCatalog()
{
    if (!mHaveKAlarmCatalog)
    {
        KGlobal::locale()->insertCatalog("kalarm");
        mHaveKAlarmCatalog = true;
    }
}

/******************************************************************************
* Check whether the calendar file has its times stored as UTC times,
* indicating that it was written by the KDE 3.0.0 version of KAlarm 0.5.7.
* Reply = true if times are stored in UTC
*       = false if the calendar is a vCalendar, times are not UTC, or any error occurred.
*/
bool isUTC(const QString& localFile)
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

/*=============================================================================
* Class: KCalEvent
*============================================================================*/

// Struct to contain static strings, to allow use of K_GLOBAL_STATIC
// to delete them on program termination.
struct StaticStrings
{
	StaticStrings()
		: STATUS_PROPERTY("TYPE"),
		  ACTIVE_STATUS(QLatin1String("ACTIVE")),
		  TEMPLATE_STATUS(QLatin1String("TEMPLATE")),
		  ARCHIVED_STATUS(QLatin1String("ARCHIVED")),
		  DISPLAYING_STATUS(QLatin1String("DISPLAYING")),
		  ARCHIVED_UID(QLatin1String("-exp-")),
		  DISPLAYING_UID(QLatin1String("-disp-")),
		  TEMPLATE_UID(QLatin1String("-tmpl-"))
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
	const QString TEMPLATE_UID;
};
K_GLOBAL_STATIC(StaticStrings, staticStrings)

typedef QMap<QString, KACalEvent::Type> PropertyMap;
static PropertyMap properties;


/******************************************************************************
* Convert a unique ID to indicate that the event is in a specified calendar file.
*/
QString KACalEvent::uid(const QString& id, Type status)
{
	QString result = id;
	Type oldType;
	int i, len;
	if ((i = result.indexOf(staticStrings->ARCHIVED_UID)) > 0)
	{
		oldType = ARCHIVED;
		len = staticStrings->ARCHIVED_UID.length();
	}
	else if ((i = result.indexOf(staticStrings->DISPLAYING_UID)) > 0)
	{
		oldType = DISPLAYING;
		len = staticStrings->DISPLAYING_UID.length();
	}
	else
	{
		oldType = ACTIVE;
		i = result.lastIndexOf('-');
		len = 1;
		if (i < 0)
		{
			i = result.length();
			len = 0;
		}
		else
			len = 1;
	}
	if (status != oldType  &&  i > 0)
	{
		QString part;
		switch (status)
		{
			case ARCHIVED:    part = staticStrings->ARCHIVED_UID;  break;
			case DISPLAYING:  part = staticStrings->DISPLAYING_UID;  break;
			case ACTIVE:
			case TEMPLATE:
			case EMPTY:
			default:          part = QLatin1String("-");  break;
		}
		result.replace(i, len, part);
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
KACalEvent::Type KACalEvent::status(const KCal::Event* event, QString* param)
{
	// Set up a static quick lookup for type strings
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
	Alarm::List alarms = event->alarms();
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
		int i = property.indexOf(';');
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
	// Check first for an old KAlarm format, which indicated the event type in its UID.
	QString uid = event->uid();
	if (uid.indexOf(staticStrings->ARCHIVED_UID) > 0)
		return ARCHIVED;
	if (uid.indexOf(staticStrings->TEMPLATE_UID) > 0)
		return TEMPLATE;

	// Otherwise, assume it's an active alarm
	return ACTIVE;
}

/******************************************************************************
* Set the event's type - active, archived, template, etc.
* If a parameter is supplied, it will be appended as a second parameter to the
* custom property.
*/
void KACalEvent::setStatus(KCal::Event* event, KACalEvent::Type status, const QString& param)
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
		text += ';' + param;
	event->setCustomProperty(KACalendar::APPNAME, staticStrings->STATUS_PROPERTY, text);
}

// vim: et sw=4:
