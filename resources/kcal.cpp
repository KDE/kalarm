/*
 *  kcal.cpp  -  libkcal calendar and event functions
 *  Program:  kalarm
 *  Copyright © 2006 by David Jarvie <software@astrojar.org.uk>
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

#include "kalarm.h"

#include <QMap>
#include <kdebug.h>

#include <libkcal/event.h>
#include <libkcal/alarm.h>

#include "kcal.h"

QByteArray KCalendar::APPNAME = "KALARM";

using namespace KCal;

// Event custom properties.
// Note that all custom property names are prefixed with X-KDE-KALARM- in the calendar file.
static const QByteArray STATUS_PROPERTY("TYPE");    // X-KDE-KALARM-TYPE property
static const QString ACTIVE_STATUS              = QString::fromLatin1("ACTIVE");
static const QString TEMPLATE_STATUS            = QString::fromLatin1("TEMPLATE");
static const QString ARCHIVED_STATUS            = QString::fromLatin1("ARCHIVED");
static const QString DISPLAYING_STATUS          = QString::fromLatin1("DISPLAYING");
typedef QMap<QString, KCalEvent::Status> PropertyMap;
static PropertyMap properties;

// Event ID identifiers
static const QString ARCHIVED_UID   = QString::fromLatin1("-exp-");
static const QString DISPLAYING_UID = QString::fromLatin1("-disp-");

// Old KAlarm format identifiers
static const QString TEMPLATE_UID   = QString::fromLatin1("-tmpl-");

const QString SC = QString::fromLatin1(";");


/******************************************************************************
* Convert a unique ID to indicate that the event is in a specified calendar file.
*/
QString KCalEvent::uid(const QString& id, Status status)
{
	QString result = id;
	Status oldStatus;
	int i, len;
	if ((i = result.indexOf(ARCHIVED_UID)) > 0)
	{
		oldStatus = ARCHIVED;
		len = ARCHIVED_UID.length();
	}
	else if ((i = result.indexOf(DISPLAYING_UID)) > 0)
	{
		oldStatus = DISPLAYING;
		len = DISPLAYING_UID.length();
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
			case ARCHIVED:    part = ARCHIVED_UID;  break;
			case DISPLAYING:  part = DISPLAYING_UID;  break;
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
		properties[ACTIVE_STATUS]     = ACTIVE;
		properties[TEMPLATE_STATUS]   = TEMPLATE;
		properties[ARCHIVED_STATUS]   = ARCHIVED;
		properties[DISPLAYING_STATUS] = DISPLAYING;
	}

	if (param)
		*param = QString();
	if (!event)
		return EMPTY;
	Alarm::List alarms = event->alarms();
	if (alarms.isEmpty())
		return EMPTY;

	QString property = event->customProperty(KCalendar::APPNAME, STATUS_PROPERTY);
	if (!property.isEmpty())
	{
		// There's a X-KDE-KALARM-TYPE property.
		// It consists of the event type, plus an optional parameter.
		PropertyMap::ConstIterator it = properties.find(property);
		if (it != properties.end())
			return it.value();
		int i = property.indexOf(SC);
		if (i < 0)
			return EMPTY;
		it = properties.find(property.left(i));
		if (it == properties.end())
			return EMPTY;
		if (param)
			*param = property.mid(i + 1);
		return it.value();
	}

	// The event either wasn't written by KAlarm, or was written by a pre-2.0 version.
	// Check first for an old KAlarm format, which indicated the event type in its UID.
	QString uid = event->uid();
	if (uid.indexOf(ARCHIVED_UID) > 0)
		return ARCHIVED;
	if (uid.indexOf(TEMPLATE_UID) > 0)
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
		case ACTIVE:      text = ACTIVE_STATUS;  break;
		case TEMPLATE:    text = TEMPLATE_STATUS;  break;
		case ARCHIVED:    text = ARCHIVED_STATUS;  break;
		case DISPLAYING:  text = DISPLAYING_STATUS;  break;
		default:
			event->removeCustomProperty(KCalendar::APPNAME, STATUS_PROPERTY);
			return;
	}
	if (!param.isEmpty())
		text += SC + param;
	event->setCustomProperty(KCalendar::APPNAME, STATUS_PROPERTY, text);
}
