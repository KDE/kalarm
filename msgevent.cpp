/*
 *  msgevent.cpp  -  the event object for messages
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  djarvie@lineone.net
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include <stdlib.h>
#include <ctype.h>
#include <qcolor.h>
#include <kdebug.h>

#include "msgevent.h"
using namespace KCal;


static const QString TEXT_PREFIX   = QString::fromLatin1("TEXT:");
static const QString FILE_PREFIX   = QString::fromLatin1("FILE:");
static const QString BEEP_CATEGORY = QString::fromLatin1("BEEP");

MessageEvent::MessageEvent(const MessageEvent& event)
	: Event()
{
	setMessage(event.dateTime(), event.flags(), event.colour(), event.alarms().getFirst()->text(), -1);
	setRepetition(event.repeatMinutes(), event.repeatCount());
}

void MessageEvent::set(const QDateTime& dateTime, bool lateCancel)
{
	setDtStart(dateTime);
	setDtEnd(dateTime.addDays(lateCancel ? 0 : 1));
	alarms().getFirst()->setTime(dateTime);
}

void MessageEvent::setMessage(const QDateTime& dateTime, int flags,
                              const QColor& colour, const QString& message, int type)
{
	alarms().getFirst()->setEnabled(true);        // enable the alarm
	set(dateTime, !!(flags & LATE_CANCEL));
	if (type >= 0)
	{
		QString text(type ? FILE_PREFIX : TEXT_PREFIX);
		text += message;
		alarms().getFirst()->setText(text);
	}
	else
		alarms().getFirst()->setText(message);
	QStringList cats;
	cats.append(colour.name());
	if (flags & BEEP)
		cats.append(BEEP_CATEGORY);
	setCategories(cats);
	setRevision(0);
}

void MessageEvent::setRepetition(int minutes, int initialCount, int remainingCount)
{
	if (remainingCount < 0)
		remainingCount = initialCount;
	alarms().getFirst()->setRepeatCount(remainingCount);
	alarms().getFirst()->setSnoozeTime(minutes);
	setRevision(initialCount - remainingCount);
}

void MessageEvent::updateRepetition(const QDateTime& dateTime, int remainingCount)
{
	bool readonly = isReadOnly();
	alarms().getFirst()->setAlarmReadOnly(false);
	alarms().getFirst()->setTime(dateTime);
	int initialCount = initialRepeatCount();
	alarms().getFirst()->setRepeatCount(remainingCount);
	setReadOnly(false);
	setRevision(initialCount - remainingCount);
	setReadOnly(readonly);
	alarms().getFirst()->setAlarmReadOnly(readonly);
}

bool MessageEvent::messageIsFileName() const
{
	return alarms().getFirst()->text().startsWith(FILE_PREFIX);
}

QString MessageEvent::cleanText() const
{
	if (alarms().getFirst()->text().startsWith(FILE_PREFIX)
	||  alarms().getFirst()->text().startsWith(TEXT_PREFIX))
		return alarms().getFirst()->text().mid(5);
	return alarms().getFirst()->text();
}

QString MessageEvent::message() const
{
	if (alarms().getFirst()->text().startsWith(FILE_PREFIX))
		return QString::null;
	if (alarms().getFirst()->text().startsWith(TEXT_PREFIX))
		return alarms().getFirst()->text().mid(5);
	return alarms().getFirst()->text();
}

QString MessageEvent::fileName() const
{
	if (alarms().getFirst()->text().startsWith(FILE_PREFIX))
		return alarms().getFirst()->text().mid(5);
	return QString::null;
}

QColor MessageEvent::colour() const
{
	const QStringList& cats = categories();
	if (cats.count() > 0)
	{
		QColor color(cats[0]);
		if (color.isValid())
			return color;
	}
	return QColor(255, 255, 255);    // missing/invalid colour - return white
}

int MessageEvent::flags() const
{
	int flags = 0;
	const QStringList& cats = categories();
	for (unsigned int i = 1;  i < cats.count();  ++i)
	{
		if ( cats[i] == BEEP_CATEGORY)
			flags |= BEEP;
	}
	if (!isMultiDay())
		flags |= LATE_CANCEL;
	return flags;
}
