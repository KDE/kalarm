/*
 *  kcalendar.cpp  -  kcal library calendar and event functions
 *  Program:  kalarm
 *  Copyright © 2006,2007,2009 by David Jarvie <djarvie@kde.org>
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
#include "kcalendar.h"

#include <kglobal.h>
#include <kdebug.h>

#include <kcal/event.h>
#include <kcal/alarm.h>

#include <QMap>


QByteArray KCalendar::APPNAME = "KALARM";

using namespace KCal;

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

typedef QMap<QString, KCalEvent::Status> PropertyMap;
static PropertyMap properties;


/******************************************************************************
* Convert a unique ID to indicate that the event is in a specified calendar file.
*/
QString KCalEvent::uid(const QString& id, Status status)
{
	QString result = id;
	Status oldStatus;
	int i, len;
	if ((i = result.indexOf(staticStrings->ARCHIVED_UID)) > 0)
	{
		oldStatus = ARCHIVED;
		len = staticStrings->ARCHIVED_UID.length();
	}
	else if ((i = result.indexOf(staticStrings->DISPLAYING_UID)) > 0)
	{
		oldStatus = DISPLAYING;
		len = staticStrings->DISPLAYING_UID.length();
	}
	else
	{
		oldStatus = ACTIVE;
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
	if (status != oldStatus  &&  i > 0)
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
KCalEvent::Status KCalEvent::status(const KCal::Event* event, QString* param)
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

	const QString property = event->customProperty(KCalendar::APPNAME, staticStrings->STATUS_PROPERTY);
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
void KCalEvent::setStatus(KCal::Event* event, KCalEvent::Status status, const QString& param)
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
			event->removeCustomProperty(KCalendar::APPNAME, staticStrings->STATUS_PROPERTY);
			return;
	}
	if (!param.isEmpty())
		text += ';' + param;
	event->setCustomProperty(KCalendar::APPNAME, staticStrings->STATUS_PROPERTY, text);
}
