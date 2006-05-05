/*
 *  kcal.cpp  -  libkcal calendar and event functions
 *  Program:  kalarm
 *  Copyright (c) 2006 by David Jarvie <software@astrojar.org.uk>
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

#include <kdebug.h>

#include <libkcal/event.h>
#include <libkcal/alarm.h>

#include "kcal.h"

namespace KCalendar
{
	QByteArray APPNAME = "KALARM";
}


using namespace KCal;

// Event custom properties.
// Note that all custom property names are prefixed with X-KDE-KALARM- in the calendar file.
static const QByteArray STATUS_PROPERTY("TYPE");    // X-KDE-KALARM-TYPE property
static const QString ACTIVE_STATUS              = QString::fromLatin1("ACTIVE");
static const QString TEMPLATE_STATUS            = QString::fromLatin1("TEMPLATE");
static const QString EXPIRED_STATUS             = QString::fromLatin1("EXPIRED");
static const QString DISPLAYING_STATUS          = QString::fromLatin1("DISPLAYING");
static const QString KORGANIZER_STATUS          = QString::fromLatin1("KORG");

// Event ID identifiers
static const QString EXPIRED_UID    = QString::fromLatin1("-exp-");
static const QString DISPLAYING_UID = QString::fromLatin1("-disp-");
static const QString TEMPLATE_UID   = QString::fromLatin1("-tmpl-");
static const QString KORGANIZER_UID = QString::fromLatin1("-korg-");

const QString TEMPL_AFTER_TIME_CATEGORY = QString::fromLatin1("TMPLAFTTIME;");


/******************************************************************************
* Convert a unique ID to indicate that the event is in a specified calendar file.
*/
QString KCalEvent::uid(const QString& id, Status status)
{
	QString result = id;
	Status oldStatus;
	int i, len;
	if ((i = result.indexOf(EXPIRED_UID)) > 0)
	{
		oldStatus = EXPIRED;
		len = EXPIRED_UID.length();
	}
	else if ((i = result.indexOf(DISPLAYING_UID)) > 0)
	{
		oldStatus = DISPLAYING;
		len = DISPLAYING_UID.length();
	}
	else if ((i = result.indexOf(TEMPLATE_UID)) > 0)
	{
		oldStatus = TEMPLATE;
		len = TEMPLATE_UID.length();
	}
	else if ((i = result.indexOf(KORGANIZER_UID)) > 0)
	{
		oldStatus = KORGANIZER;
		len = KORGANIZER_UID.length();
	}
	else
	{
		oldStatus = ACTIVE;
		i = result.lastIndexOf('-');
		len = 1;
	}
	if (status != oldStatus  &&  i > 0)
	{
		QString part;
		switch (status)
		{
			case ACTIVE:      part = QString::fromLatin1("-");  break;
			case EXPIRED:     part = EXPIRED_UID;  break;
			case DISPLAYING:  part = DISPLAYING_UID;  break;
			case TEMPLATE:    part = TEMPLATE_UID;  break;
			case KORGANIZER:  part = KORGANIZER_UID;  break;
			case EMPTY:
			default:          return result;
		}
		result.replace(i, len, part);
	}
	return result;
}

/******************************************************************************
* Check an event to determine its type - active, expired, template or empty.
* The default type is active if it contains alarms and there is nothing to
* indicate otherwise.
* Note that the mere fact that all an event's alarms have passed does not make
* an event expired, since it may be that they have not yet been able to be
* triggered. They will be marked expired once KAlarm tries to handle them.
* Do not call this function for the displaying alarm calendar.
*/
KCalEvent::Status KCalEvent::status(const KCal::Event* event)
{
	if (!event)
		return EMPTY;

	// The order of these checks is important in case the calendar hasn't been
	// created by KAlarm.
	Alarm::List alarms = event->alarms();
	if (alarms.isEmpty())
		return EMPTY;
#ifdef NEW_EVENT_FORMAT
	QString property = event->customProperty(KCalendar::APPNAME, STATUS_PROPERTY);
	if (!property.isEmpty())
	{
		if (property == ACTIVE_STATUS)      return ACTIVE;
		if (property == TEMPLATE_STATUS)    return TEMPLATE;
		if (property == EXPIRED_STATUS)     return EXPIRED;
		if (property == DISPLAYING_STATUS)  return DISPLAYING;
		if (property == KORGANIZER_STATUS)  return KORGANIZER;
	}
#endif
	switch (uidStatus(event->uid()))
	{
		case EXPIRED:   return EXPIRED;
		case TEMPLATE:  return TEMPLATE;
		default:  break;
	}
	if (!event->summary().isEmpty())
		return TEMPLATE;
	const QStringList& cats = event->categories();
	for (int i = 0;  i < cats.count();  ++i)
	{
		if (cats[i].startsWith(TEMPL_AFTER_TIME_CATEGORY))
			return TEMPLATE;
	}
	return ACTIVE;
}

/******************************************************************************
* Get the calendar type for a unique ID.
*/
KCalEvent::Status KCalEvent::uidStatus(const QString& uid)
{
	if (uid.indexOf(EXPIRED_UID) > 0)
		return EXPIRED;
	if (uid.indexOf(DISPLAYING_UID) > 0)
		return DISPLAYING;
	if (uid.indexOf(TEMPLATE_UID) > 0)
		return TEMPLATE;
	if (uid.indexOf(KORGANIZER_UID) > 0)
		return KORGANIZER;
	return ACTIVE;
}

/******************************************************************************
* Set the event's type - active, expired, template, etc.
*/
void KCalEvent::setStatus(const KCal::Event* event, KCalEvent::Status status)
{
	if (!event)
		return;
#ifdef NEW_EVENT_FORMAT
	QString text;
	switch (status)
	{
		case ACTIVE:      text = ACTIVE_STATUS;  break;
		case TEMPLATE:    text = TEMPLATE_STATUS;  break;
		case EXPIRED:     text = EXPIRED_STATUS;  break;
		case DISPLAYING:  text = DISPLAYING_STATUS;  break;
		case KORGANIZER:  text = KORGANIZER_STATUS;  break;
		default:
			event->removeCustomProperty(KCalendar::APPNAME, STATUS_PROPERTY);
			return;
	}
	event->setCustomProperty(KCalendar::APPNAME, STATUS_PROPERTY, text);
#endif
}
