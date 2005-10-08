/*
 *  dcophandler.cpp  -  handler for DCOP calls by other applications
 *  Program:  kalarm
 *  Copyright (c) 2002 - 2005 by David Jarvie <software@astrojar.org.uk>
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

#include <stdlib.h>

#include <kdebug.h>

#include <libkpimidentities/identitymanager.h>
#include <libkpimidentities/identity.h>

#include "daemon.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "karecurrence.h"
#include "preferences.h"
#include "dcophandler.moc"

static const char*  DCOP_OBJECT_NAME = "request";   // DCOP name of KAlarm's request interface


/*=============================================================================
= DcopHandler
= This class's function is to handle DCOP requests by other applications.
=============================================================================*/
DcopHandler::DcopHandler()
	: DCOPObject(DCOP_OBJECT_NAME),
	  QWidget()
{
	kdDebug(5950) << "DcopHandler::DcopHandler()\n";
}


bool DcopHandler::cancelEvent(const QString& url,const QString& eventId)
{
	return theApp()->deleteEvent(url, eventId);
}

bool DcopHandler::triggerEvent(const QString& url,const QString& eventId)
{
	return theApp()->triggerEvent(url, eventId);
}

bool DcopHandler::scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& font,
                                  const KURL& audioFile, int reminderMins, const QString& recurrence,
                                  int repeatInterval, int repeatCount)
{
	DateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurrence))
		return false;
	return scheduleMessage(message, start, lateCancel, flags, bgColor, fgColor, font, audioFile, reminderMins, recur, repeatInterval, repeatCount);
}

bool DcopHandler::scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& font,
                                  const KURL& audioFile, int reminderMins,
                                  int recurType, int recurInterval, int recurCount)
{
	DateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
		return false;
	return scheduleMessage(message, start, lateCancel, flags, bgColor, fgColor, font, audioFile, reminderMins, recur);
}

bool DcopHandler::scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& font,
                                  const KURL& audioFile, int reminderMins,
                                  int recurType, int recurInterval, const QString& endDateTime)
{
	DateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
		return false;
	return scheduleMessage(message, start, lateCancel, flags, bgColor, fgColor, font, audioFile, reminderMins, recur);
}

bool DcopHandler::scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const KURL& audioFile, int reminderMins, const QString& recurrence,
                               int repeatInterval, int repeatCount)
{
	DateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurrence))
		return false;
	return scheduleFile(file, start, lateCancel, flags, bgColor, audioFile, reminderMins, recur, repeatInterval, repeatCount);
}

bool DcopHandler::scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const KURL& audioFile, int reminderMins, int recurType, int recurInterval, int recurCount)
{
	DateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
		return false;
	return scheduleFile(file, start, lateCancel, flags, bgColor, audioFile, reminderMins, recur);
}

bool DcopHandler::scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const KURL& audioFile, int reminderMins, int recurType, int recurInterval, const QString& endDateTime)
{
	DateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
		return false;
	return scheduleFile(file, start, lateCancel, flags, bgColor, audioFile, reminderMins, recur);
}

bool DcopHandler::scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& recurrence, int repeatInterval, int repeatCount)
{
	DateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurrence))
		return false;
	return scheduleCommand(commandLine, start, lateCancel, flags, recur, repeatInterval, repeatCount);
}

bool DcopHandler::scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  int recurType, int recurInterval, int recurCount)
{
	DateTime start = convertStartDateTime(startDateTime);
	if (!start.isValid())
		return false;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
		return false;
	return scheduleCommand(commandLine, start, lateCancel, flags, recur);
}

bool DcopHandler::scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  int recurType, int recurInterval, const QString& endDateTime)
{
	DateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
		return false;
	return scheduleCommand(commandLine, start, lateCancel, flags, recur);
}

bool DcopHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                const QString& recurrence, int repeatInterval, int repeatCount)
{
	DateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurrence))
		return false;
	return scheduleEmail(fromID, addresses, subject, message, attachments, start, lateCancel, flags, recur, repeatInterval, repeatCount);
}

bool DcopHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                int recurType, int recurInterval, int recurCount)
{
	DateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
		return false;
	return scheduleEmail(fromID, addresses, subject, message, attachments, start, lateCancel, flags, recur);
}

bool DcopHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                int recurType, int recurInterval, const QString& endDateTime)
{
	DateTime start;
	KARecurrence recur;
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
		return false;
	return scheduleEmail(fromID, addresses, subject, message, attachments, start, lateCancel, flags, recur);
}


/******************************************************************************
* Schedule a message alarm, after converting the parameters from strings.
*/
bool DcopHandler::scheduleMessage(const QString& message, const DateTime& start, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& fontStr,
                                  const KURL& audioFile, int reminderMins, const KARecurrence& recurrence,
                                  int repeatInterval, int repeatCount)
{
	unsigned kaEventFlags = convertStartFlags(start, flags);
	QColor bg = convertBgColour(bgColor);
	if (!bg.isValid())
		return false;
	QColor fg;
	if (fgColor.isEmpty())
		fg = Preferences::defaultFgColour();
	else
	{
		fg.setNamedColor(fgColor);
		if (!fg.isValid())
		{
			kdError(5950) << "DCOP call: invalid foreground color: " << fgColor << endl;
			return false;
		}
	}
	QFont font;
	if (fontStr.isEmpty())
		kaEventFlags |= KAEvent::DEFAULT_FONT;
	else
	{
		if (!font.fromString(fontStr))    // N.B. this doesn't do good validation
		{
			kdError(5950) << "DCOP call: invalid font: " << fontStr << endl;
			return false;
		}
	}
	return theApp()->scheduleEvent(KAEvent::MESSAGE, message, start.dateTime(), lateCancel, kaEventFlags, bg, fg, font,
	                               audioFile.url(), -1, reminderMins, recurrence, repeatInterval, repeatCount);
}

/******************************************************************************
* Schedule a file alarm, after converting the parameters from strings.
*/
bool DcopHandler::scheduleFile(const KURL& file,
                               const DateTime& start, int lateCancel, unsigned flags, const QString& bgColor,
                               const KURL& audioFile, int reminderMins, const KARecurrence& recurrence,
                               int repeatInterval, int repeatCount)
{
	unsigned kaEventFlags = convertStartFlags(start, flags);
	QColor bg = convertBgColour(bgColor);
	if (!bg.isValid())
		return false;
	return theApp()->scheduleEvent(KAEvent::FILE, file.url(), start.dateTime(), lateCancel, kaEventFlags, bg, Qt::black, QFont(),
	                               audioFile.url(), -1, reminderMins, recurrence, repeatInterval, repeatCount);
}

/******************************************************************************
* Schedule a command alarm, after converting the parameters from strings.
*/
bool DcopHandler::scheduleCommand(const QString& commandLine,
                                  const DateTime& start, int lateCancel, unsigned flags,
                                  const KARecurrence& recurrence, int repeatInterval, int repeatCount)
{
	unsigned kaEventFlags = convertStartFlags(start, flags);
	return theApp()->scheduleEvent(KAEvent::COMMAND, commandLine, start.dateTime(), lateCancel, kaEventFlags, Qt::black, Qt::black, QFont(),
	                               QString::null, -1, 0, recurrence, repeatInterval, repeatCount);
}

/******************************************************************************
* Schedule an email alarm, after validating the addresses and attachments.
*/
bool DcopHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject,
                                const QString& message, const QString& attachments,
                                const DateTime& start, int lateCancel, unsigned flags,
                                const KARecurrence& recurrence, int repeatInterval, int repeatCount)
{
	unsigned kaEventFlags = convertStartFlags(start, flags);
	if (!fromID.isEmpty())
	{
		if (KAMail::identityManager()->identityForName(fromID).isNull())
		{
			kdError(5950) << "DCOP call scheduleEmail(): unknown sender ID: " << fromID << endl;
			return false;
		}
	}
	EmailAddressList addrs;
	QString bad = KAMail::convertAddresses(addresses, addrs);
	if (!bad.isEmpty())
	{
		kdError(5950) << "DCOP call scheduleEmail(): invalid email addresses: " << bad << endl;
		return false;
	}
	if (addrs.isEmpty())
	{
		kdError(5950) << "DCOP call scheduleEmail(): no email address\n";
		return false;
	}
	QStringList atts;
	bad = KAMail::convertAttachments(attachments, atts);
	if (!bad.isEmpty())
	{
		kdError(5950) << "DCOP call scheduleEmail(): invalid email attachment: " << bad << endl;
		return false;
	}
	return theApp()->scheduleEvent(KAEvent::EMAIL, message, start.dateTime(), lateCancel, kaEventFlags, Qt::black, Qt::black, QFont(),
	                               QString::null, -1, 0, recurrence, repeatInterval, repeatCount, fromID, addrs, subject, atts);
}


/******************************************************************************
* Convert the start date/time string to a DateTime. The date/time string is in
* the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
*/
DateTime DcopHandler::convertStartDateTime(const QString& startDateTime)
{
	DateTime start;
	if (startDateTime.length() > 10)
	{
		// Both a date and a time are specified
		start = QDateTime::fromString(startDateTime, Qt::ISODate);
	}
	else
	{
		// Check whether a time is specified
		QString t;
		if (startDateTime[0] == 'T')
			t = startDateTime.mid(1);     // it's a time: remove the leading 'T'
		else if (!startDateTime[2].isDigit())
			t = startDateTime;            // it's a time with no leading 'T'

		if (t.isEmpty())
		{
			// It's a date
			start = QDate::fromString(startDateTime, Qt::ISODate);
		}
		else
		{
			// It's a time, so use today as the date
			start.set(QDate::currentDate(), QTime::fromString(t, Qt::ISODate));
		}
	}
	if (!start.isValid())
		kdError(5950) << "DCOP call: invalid start date/time: " << startDateTime << endl;
	return start;
}

/******************************************************************************
* Convert the flag bits to KAEvent flag bits.
*/
unsigned DcopHandler::convertStartFlags(const DateTime& start, unsigned flags)
{
	unsigned kaEventFlags = 0;
	if (flags & REPEAT_AT_LOGIN) kaEventFlags |= KAEvent::REPEAT_AT_LOGIN;
	if (flags & BEEP)            kaEventFlags |= KAEvent::BEEP;
	if (flags & SPEAK)           kaEventFlags |= KAEvent::SPEAK;
	if (flags & CONFIRM_ACK)     kaEventFlags |= KAEvent::CONFIRM_ACK;
	if (flags & REPEAT_SOUND)    kaEventFlags |= KAEvent::REPEAT_SOUND;
	if (flags & AUTO_CLOSE)      kaEventFlags |= KAEvent::AUTO_CLOSE;
	if (flags & EMAIL_BCC)       kaEventFlags |= KAEvent::EMAIL_BCC;
	if (flags & SCRIPT)          kaEventFlags |= KAEvent::SCRIPT;
	if (flags & EXEC_IN_XTERM)   kaEventFlags |= KAEvent::EXEC_IN_XTERM;
	if (flags & SHOW_IN_KORG)    kaEventFlags |= KAEvent::COPY_KORGANIZER;
	if (flags & DISABLED)        kaEventFlags |= KAEvent::DISABLED;
	if (start.isDateOnly())      kaEventFlags |= KAEvent::ANY_TIME;
	return kaEventFlags;
}

/******************************************************************************
* Convert the background colour string to a QColor.
*/
QColor DcopHandler::convertBgColour(const QString& bgColor)
{
	if (bgColor.isEmpty())
		return Preferences::defaultBgColour();
	QColor bg(bgColor);
	if (!bg.isValid())
			kdError(5950) << "DCOP call: invalid background color: " << bgColor << endl;
	return bg;
}

bool DcopHandler::convertRecurrence(DateTime& start, KARecurrence& recurrence, 
                                    const QString& startDateTime, const QString& icalRecurrence)
{
	start = convertStartDateTime(startDateTime);
	if (!start.isValid())
		return false;
	return recurrence.set(icalRecurrence);
}

bool DcopHandler::convertRecurrence(DateTime& start, KARecurrence& recurrence, const QString& startDateTime,
                                    int recurType, int recurInterval, int recurCount)
{
	start = convertStartDateTime(startDateTime);
	if (!start.isValid())
		return false;
	return convertRecurrence(recurrence, start, recurType, recurInterval, recurCount, QDateTime());
}

bool DcopHandler::convertRecurrence(DateTime& start, KARecurrence& recurrence, const QString& startDateTime,
                                    int recurType, int recurInterval, const QString& endDateTime)
{
	start = convertStartDateTime(startDateTime);
	if (!start.isValid())
		return false;
	QDateTime end;
	if (endDateTime.find('T') < 0)
	{
		if (!start.isDateOnly())
		{
			kdError(5950) << "DCOP call: alarm is date-only, but recurrence end is date/time" << endl;
			return false;
		}
		end.setDate(QDate::fromString(endDateTime, Qt::ISODate));
	}
	else
	{
		if (start.isDateOnly())
		{
			kdError(5950) << "DCOP call: alarm is timed, but recurrence end is date-only" << endl;
			return false;
		}
		end = QDateTime::fromString(endDateTime, Qt::ISODate);
	}
	if (!end.isValid())
	{
		kdError(5950) << "DCOP call: invalid recurrence end date/time: " << endDateTime << endl;
		return false;
	}
	return convertRecurrence(recurrence, start, recurType, recurInterval, 0, end);
}

bool DcopHandler::convertRecurrence(KARecurrence& recurrence, const DateTime& start, int recurType,
                                    int recurInterval, int recurCount, const QDateTime& end)
{
	KARecurrence::Type type;
	switch (recurType)
	{
		case MINUTELY:  type = KARecurrence::MINUTELY;  break;
		case DAILY:     type = KARecurrence::DAILY;  break;
		case WEEKLY:    type = KARecurrence::WEEKLY;  break;
		case MONTHLY:   type = KARecurrence::MONTHLY_DAY;  break;
		case YEARLY:    type = KARecurrence::ANNUAL_DATE;  break;
			break;
		default:
			kdError(5950) << "DCOP call: invalid repeat type: " << recurType << endl;
			return false;
	}
	recurrence.set(type, recurInterval, recurCount, start, end);
	return true;
}
