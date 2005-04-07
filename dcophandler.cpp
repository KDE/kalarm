/*
 *  dcophandler.cpp  -  handler for DCOP calls by other applications
 *  Program:  kalarm
 *  (C) 2002 - 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "kalarm.h"

#include <stdlib.h>

#include <kdebug.h>

#include <libkpimidentities/identitymanager.h>
#include <libkpimidentities/identity.h>
#include <libkcal/icalformat.h>

#include "kalarmapp.h"
#include "preferences.h"
#include "kamail.h"
#include "daemon.h"
#include "dcophandler.moc"

static const char*  DCOP_OBJECT_NAME = "request";   // DCOP name of KAlarm's request interface
#ifdef OLD_DCOP
static const char*  DCOP_OLD_OBJECT_NAME = "display";
#endif


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
	KCal::Recurrence recur(0);
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
	KCal::Recurrence recur(0);
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
	KCal::Recurrence recur(0);
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
		return false;
	return scheduleMessage(message, start, lateCancel, flags, bgColor, fgColor, font, audioFile, reminderMins, recur);
}

bool DcopHandler::scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const KURL& audioFile, int reminderMins, const QString& recurrence,
                               int repeatInterval, int repeatCount)
{
	DateTime start;
	KCal::Recurrence recur(0);
	if (!convertRecurrence(start, recur, startDateTime, recurrence))
		return false;
	return scheduleFile(file, start, lateCancel, flags, bgColor, audioFile, reminderMins, recur, repeatInterval, repeatCount);
}

bool DcopHandler::scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const KURL& audioFile, int reminderMins, int recurType, int recurInterval, int recurCount)
{
	DateTime start;
	KCal::Recurrence recur(0);
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
		return false;
	return scheduleFile(file, start, lateCancel, flags, bgColor, audioFile, reminderMins, recur);
}

bool DcopHandler::scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const KURL& audioFile, int reminderMins, int recurType, int recurInterval, const QString& endDateTime)
{
	DateTime start;
	KCal::Recurrence recur(0);
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
		return false;
	return scheduleFile(file, start, lateCancel, flags, bgColor, audioFile, reminderMins, recur);
}

bool DcopHandler::scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& recurrence, int repeatInterval, int repeatCount)
{
	DateTime start;
	KCal::Recurrence recur(0);
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
	KCal::Recurrence recur(0);
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
		return false;
	return scheduleCommand(commandLine, start, lateCancel, flags, recur);
}

bool DcopHandler::scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  int recurType, int recurInterval, const QString& endDateTime)
{
	DateTime start;
	KCal::Recurrence recur(0);
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
		return false;
	return scheduleCommand(commandLine, start, lateCancel, flags, recur);
}

bool DcopHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                const QString& recurrence, int repeatInterval, int repeatCount)
{
	DateTime start;
	KCal::Recurrence recur(0);
	if (!convertRecurrence(start, recur, startDateTime, recurrence))
		return false;
	return scheduleEmail(fromID, addresses, subject, message, attachments, start, lateCancel, flags, recur, repeatInterval, repeatCount);
}

bool DcopHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                int recurType, int recurInterval, int recurCount)
{
	DateTime start;
	KCal::Recurrence recur(0);
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
		return false;
	return scheduleEmail(fromID, addresses, subject, message, attachments, start, lateCancel, flags, recur);
}

bool DcopHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                int recurType, int recurInterval, const QString& endDateTime)
{
	DateTime start;
	KCal::Recurrence recur(0);
	if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
		return false;
	return scheduleEmail(fromID, addresses, subject, message, attachments, start, lateCancel, flags, recur);
}


/******************************************************************************
* Schedule a message alarm, after converting the parameters from strings.
*/
bool DcopHandler::scheduleMessage(const QString& message, const DateTime& start, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& fontStr,
                                  const KURL& audioFile, int reminderMins, const KCal::Recurrence& recurrence,
                                  int repeatInterval, int repeatCount)
{
	unsigned kaEventFlags = convertStartFlags(start, flags);
	QColor bg = convertBgColour(bgColor);
	if (!bg.isValid())
		return false;
	QColor fg;
	if (fgColor.isEmpty())
		fg = Preferences::instance()->defaultFgColour();
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
                               const KURL& audioFile, int reminderMins, const KCal::Recurrence& recurrence,
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
                                  const KCal::Recurrence& recurrence, int repeatInterval, int repeatCount)
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
                                const KCal::Recurrence& recurrence, int repeatInterval, int repeatCount)
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
	if (flags & CONFIRM_ACK)     kaEventFlags |= KAEvent::CONFIRM_ACK;
	if (flags & REPEAT_SOUND)    kaEventFlags |= KAEvent::REPEAT_SOUND;
	if (flags & AUTO_CLOSE)      kaEventFlags |= KAEvent::AUTO_CLOSE;
	if (flags & EMAIL_BCC)       kaEventFlags |= KAEvent::EMAIL_BCC;
	if (flags & SCRIPT)          kaEventFlags |= KAEvent::SCRIPT;
	if (flags & EXEC_IN_XTERM)   kaEventFlags |= KAEvent::EXEC_IN_XTERM;
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
		return Preferences::instance()->defaultBgColour();
	QColor bg(bgColor);
	if (!bg.isValid())
			kdError(5950) << "DCOP call: invalid background color: " << bgColor << endl;
	return bg;
}

bool DcopHandler::convertRecurrence(DateTime& start, KCal::Recurrence& recurrence, 
                                    const QString& startDateTime, const QString& icalRecurrence)
{
	start = convertStartDateTime(startDateTime);
	if (!start.isValid())
		return false;
	if (icalRecurrence.isEmpty())
		return true;
	QString icalr = icalRecurrence;
	static QString rrule = QString::fromLatin1("RRULE:");
	if (icalr.startsWith(rrule))
		icalr = icalr.mid(rrule.length());
	KCal::ICalFormat format;
	return format.fromString(&recurrence, icalr);
}

bool DcopHandler::convertRecurrence(DateTime& start, KCal::Recurrence& recurrence, const QString& startDateTime,
                                    int recurType, int recurInterval, int recurCount)
{
	start = convertStartDateTime(startDateTime);
	if (!start.isValid())
		return false;
	return convertRecurrence(recurrence, start, recurType, recurInterval, recurCount, QDateTime());
}

bool DcopHandler::convertRecurrence(DateTime& start, KCal::Recurrence& recurrence, const QString& startDateTime,
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

bool DcopHandler::convertRecurrence(KCal::Recurrence& recurrence, const DateTime& start, int recurType,
                                    int recurInterval, int recurCount, const QDateTime& end)
{
	KAEvent::RecurType type;
	switch (recurType)
	{
		case MINUTELY:  type = KAEvent::MINUTELY;  break;
		case DAILY:     type = KAEvent::DAILY;  break;
		case WEEKLY:    type = KAEvent::WEEKLY;  break;
		case MONTHLY:   type = KAEvent::MONTHLY_DAY;  break;
		case YEARLY:    type = KAEvent::ANNUAL_DATE;  break;
			break;
		default:
			kdError(5950) << "DCOP call: invalid repeat type: " << recurType << endl;
			return false;
	}
	KAEvent::setRecurrence(recurrence, type, recurInterval, recurCount, start, end);
	return true;
}


#ifdef OLD_DCOP
/*=============================================================================
= DcopHandlerOld
= This class's function is simply to act as a receiver for DCOP requests.
=============================================================================*/
DcopHandlerOld::DcopHandlerOld()
	: QWidget(),
	  DCOPObject(DCOP_OLD_OBJECT_NAME)
{
	kdDebug(5950) << "DcopHandlerOld::DcopHandlerOld()\n";
}

/******************************************************************************
* Process a DCOP request.
*/
bool DcopHandlerOld::process(const QCString& func, const QByteArray& data, QCString& replyType, QByteArray&)
{
	kdDebug(5950) << "DcopHandlerOld::process(): " << func << endl;
	enum
	{
		ERR            = 0,
		OPERATION      = 0x0007,    // mask for main operation
		  HANDLE       = 0x0001,
		  CANCEL       = 0x0002,
		  TRIGGER      = 0x0003,
		  SCHEDULE     = 0x0004,
		ALARM_TYPE     = 0x00F0,    // mask for SCHEDULE alarm type
		  MESSAGE      = 0x0010,
		  FILE         = 0x0020,
		  COMMAND      = 0x0030,
		  EMAIL        = 0x0040,
		SCH_FLAGS      = 0x0F00,    // mask for SCHEDULE flags
		  REP_COUNT    = 0x0100,
		  REP_END      = 0x0200,
		  FONT         = 0x0400,
		PRE_096        = 0x1000,           // old-style pre-0.9.6 deprecated method
		PRE_091        = 0x2000 | PRE_096  // old-style pre-0.9.1 deprecated method
	};
	replyType = "void";
	int function;
	if (func == "handleEvent(const QString&,const QString&)"
	||       func == "handleEvent(QString,QString)")
		function = HANDLE;
	else if (func == "cancelEvent(const QString&,const QString&)"
	||       func == "cancelEvent(QString,QString)")
		function = CANCEL;
	else if (func == "triggerEvent(const QString&,const QString&)"
	||       func == "triggerEvent(QString,QString)")
		function = TRIGGER;

	//                scheduleMessage(message, dateTime, colour, colourfg, flags, audioURL, reminder, recurrence)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,const QColor&,const QColor&,Q_UINT32,const QString&,Q_INT32,const QString&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,QColor,Q_UINT32,QString,Q_UINT32,QString)")
		function = SCHEDULE | MESSAGE;
	//                scheduleMessage(message, dateTime, colour, colourfg, font, flags, audioURL, reminder, recurrence)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,const QColor&,const QColor&,const QFont&,Q_UINT32,const QString&,Q_INT32,const QString&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,QColor,QFont,Q_UINT32,QString,Q_UINT32,QString)")
		function = SCHEDULE | MESSAGE | FONT;
	//                scheduleFile(URL, dateTime, colour, flags, audioURL, reminder, recurrence)
	else if (func == "scheduleFile(const QString&,const QDateTime&,const QColor&,Q_UINT32,const QString&,Q_INT32,Q_INT32,const QString&)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString,Q_UINT32,QString)")
		function = SCHEDULE | FILE;
	//                scheduleCommand(commandLine, dateTime, flags, recurrence)
	else if (func == "scheduleCommand(const QString&,const QDateTime&,Q_UINT32,const QString&)"
	||       func == "scheduleCommand(QString,QDateTime,Q_UINT32,QString)")
		function = SCHEDULE | COMMAND;
	//                scheduleEmail(addresses, subject, message, attachments, dateTime, flags, recurrence)
	else if (func == "scheduleEmail(const QString&,const QString&,const QString&,const QString&,const QDateTime&,Q_UINT32,const QString&)"
	||       func == "scheduleEmail(QString,QString,QString,QString,QDateTime,Q_UINT32,QString)")
		function = SCHEDULE | EMAIL;

	//                scheduleMessage(message, dateTime, colour, colourfg, flags, audioURL, reminder, recurType, interval, recurCount)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,const QColor&,const QColor&,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | MESSAGE | REP_COUNT;
	//                scheduleFile(URL, dateTime, colour, flags, audioURL, reminder, recurType, interval, recurCount)
	else if (func == "scheduleFile(const QString&,const QDateTime&,const QColor&,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | FILE | REP_COUNT;
	//                scheduleCommand(commandLine, dateTime, flags, recurType, interval, recurCount)
	else if (func == "scheduleCommand(const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleCommand(QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | COMMAND | REP_COUNT;
	//                scheduleEmail(addresses, subject, message, attachments, dateTime, flags, recurType, interval, recurCount)
	else if (func == "scheduleEmail(const QString&,const QString&,const QString&,const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleEmail(QString,QString,QString,QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | EMAIL | REP_COUNT;

	//                scheduleMessage(message, dateTime, colour, colourfg, flags, audioURL, reminder, recurType, interval, endTime)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,const QColor&,const QColor&,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | MESSAGE | REP_END;
	//                scheduleFile(URL, dateTime, colour, flags, audioURL, reminder, recurType, interval, endTime)
	else if (func == "scheduleFile(const QString&,const QDateTime&,const QColor&,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | FILE | REP_END;
	//                scheduleCommand(commandLine, dateTime, flags, recurType, interval, endTime)
	else if (func == "scheduleCommand(const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleCommand(QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | COMMAND | REP_END;
	//                scheduleEmail(addresses, subject, message, attachments, dateTime, flags, recurType, interval, endTime)
	else if (func == "scheduleEmail(const QString&,const QString&,const QString&,const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleEmail(QString,QString,QString,QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | EMAIL | REP_END;

	// Deprecated methods: backwards compatibility with KAlarm pre-0.9.6
	//                scheduleMessage(message, dateTime, colour, flags, audioURL, reminder, recurrence)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,const QColor&,Q_UINT32,const QString&,Q_INT32,const QString&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString,Q_UINT32,QString)")
		function = SCHEDULE | MESSAGE | PRE_096;
	//                scheduleMessage(message, dateTime, colour, font, flags, audioURL, reminder, recurrence)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,const QColor&,const QFont&,Q_UINT32,const QString&,Q_INT32,const QString&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,QFont,Q_UINT32,QString,Q_UINT32,QString)")
		function = SCHEDULE | MESSAGE | FONT | PRE_096;
	//                scheduleMessage(message, dateTime, colour, flags, audioURL, reminder, recurType, interval, recurCount)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,const QColor&,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | MESSAGE | REP_COUNT | PRE_096;
	//                scheduleMessage(message, dateTime, colour, flags, audioURL, reminder, recurType, interval, endTime)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,const QColor&,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | MESSAGE | REP_END | PRE_096;

	// Deprecated methods: backwards compatibility with KAlarm pre-0.9.1
	//                scheduleMessage(message, dateTime, colour, flags, audioURL)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,const QColor&,Q_UINT32,const QString&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString)")
		function = SCHEDULE | MESSAGE | PRE_091;
	//                scheduleFile(URL, dateTime, colour, flags, audioURL)
	else if (func == "scheduleFile(const QString&,const QDateTime&,const QColor&,Q_UINT32,const QString&)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString)")
		function = SCHEDULE | FILE | PRE_091;
	//                scheduleMessage(message, dateTime, colour, flags, audioURL, recurType, interval, recurCount)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,const QColor&,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | MESSAGE | REP_COUNT | PRE_091;
	//                scheduleFile(URL, dateTime, colour, flags, audioURL, recurType, interval, recurCount)
	else if (func == "scheduleFile(const QString&,const QDateTime&,const QColor&,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | FILE | REP_COUNT | PRE_091;
	//                scheduleMessage(message, dateTime, colour, flags, audioURL, recurType, interval, endTime)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,const QColor&,Q_UINT32,const QString&,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | MESSAGE | REP_END | PRE_091;
	//                scheduleFile(URL, dateTime, colour, flags, audioURL, recurType, interval, endTime)
	else if (func == "scheduleFile(const QString&,const QDateTime&,const QColor&,Q_UINT32,const QString&,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | FILE | REP_END | PRE_091;

	// Obsolete methods: backwards compatibility with KAlarm pre-0.7
	else if (func == "scheduleMessage(const QString&,const QDateTime&,const QColor&,Q_UINT32,Q_INT32,Q_INT32)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,Q_INT32,Q_INT32)"
	||       func == "scheduleFile(const QString&,const QDateTime&,const QColor&,Q_UINT32,Q_INT32,Q_INT32)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,Q_INT32,Q_INT32)"
	||       func == "scheduleCommand(const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32)"
	||       func == "scheduleCommand(QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32)"
	// Obsolete methods: backwards compatibility with KAlarm pre-0.6
	||       func == "cancelMessage(const QString&,const QString&)"
	||       func == "cancelMessage(QString,QString)"
	||       func == "displayMessage(const QString&,const QString&)"
	||       func == "displayMessage(QString,QString)")
	{
		kdError(5950) << "DcopHandlerOld::process(): obsolete DCOP function call: '" << func << "'" << endl;
		return false;
	}
	else
	{
		kdError(5950) << "DcopHandlerOld::process(): unknown DCOP function" << endl;
		return false;
	}

	switch (function & OPERATION)
	{
		case HANDLE:        // trigger or cancel event with specified ID from calendar file
		case CANCEL:        // cancel event with specified ID from calendar file
		case TRIGGER:       // trigger event with specified ID in calendar file
		{

			QDataStream arg(data, IO_ReadOnly);
			QString urlString, vuid;
			arg >> urlString >> vuid;
			switch (function)
			{
				case HANDLE:
					return theApp()->handleEvent(urlString, vuid);
				case CANCEL:
					return theApp()->deleteEvent(urlString, vuid);
				case TRIGGER:
					return theApp()->triggerEvent(urlString, vuid);
			}
			break;
		}
		case SCHEDULE:      // schedule a new event
		{
			KAEvent::Action action;
			switch (function & ALARM_TYPE)
			{
				case MESSAGE:  action = KAEvent::MESSAGE;  break;
				case FILE:     action = KAEvent::FILE;     break;
				case COMMAND:  action = KAEvent::COMMAND;  break;
				case EMAIL:    action = KAEvent::EMAIL;    break;
				default:  return false;
			}
			QDataStream arg(data, IO_ReadOnly);
			QString     text, audioFile, mailSubject;
			float       audioVolume = -1;
			EmailAddressList mailAddresses;
			QStringList mailAttachments;
			QDateTime   dateTime, endTime;
			QColor      bgColour;
			QColor      fgColour(Qt::black);
			QFont       font;
			Q_UINT32    flags;
			int         lateCancel = 0;
			KCal::Recurrence  recurrence(0);
			Q_INT32     reminderMinutes = 0;
			if (action == KAEvent::EMAIL)
			{
				QString addresses, attachments;
				arg >> addresses >> mailSubject >> text >> attachments;
				QString bad = KAMail::convertAddresses(addresses, mailAddresses);
				if (!bad.isEmpty())
				{
					kdError(5950) << "DcopHandlerOld::process(): invalid email addresses: " << bad << endl;
					return false;
				}
				if (mailAddresses.isEmpty())
				{
					kdError(5950) << "DcopHandlerOld::process(): no email address\n";
					return false;
				}
				bad = KAMail::convertAttachments(attachments, mailAttachments);
				if (!bad.isEmpty())
				{
					kdError(5950) << "DcopHandlerOld::process(): invalid email attachment: " << bad << endl;
					return false;
				}
			}
			else
				arg >> text;
			arg.readRawBytes((char*)&dateTime, sizeof(dateTime));
			if (action != KAEvent::COMMAND)
				arg.readRawBytes((char*)&bgColour, sizeof(bgColour));
			if (action == KAEvent::MESSAGE  &&  !(function & PRE_096))
				arg.readRawBytes((char*)&fgColour, sizeof(fgColour));
			if (function & FONT)
			{
				arg.readRawBytes((char*)&font, sizeof(font));
				arg >> flags;
			}
			else
			{
				arg >> flags;
				flags |= KAEvent::DEFAULT_FONT;
			}
			if (flags & KAEvent::LATE_CANCEL)
				lateCancel = 1;
			if (action == KAEvent::MESSAGE  ||  action == KAEvent::FILE)
			{
				arg >> audioFile;
				if (!(function & PRE_091))
					arg >> reminderMinutes;
			}
			if (function & (REP_COUNT | REP_END))
			{
				KAEvent::RecurType recurType;
				Q_INT32 recurCount = 0;
				Q_INT32 recurInterval;
				Q_INT32 type;
				arg >> type >> recurInterval;
				recurType = KAEvent::RecurType(type);
				switch (recurType)
				{
					case KAEvent::MINUTELY:
					case KAEvent::DAILY:
					case KAEvent::WEEKLY:
					case KAEvent::MONTHLY_DAY:
					case KAEvent::ANNUAL_DATE:
						break;
					default:
						kdError(5950) << "DcopHandlerOld::process(): invalid simple repetition type: " << type << endl;
						return false;
				}
				if (function & REP_COUNT)
					arg >> recurCount;
				else
					arg.readRawBytes((char*)&endTime, sizeof(endTime));
				KAEvent::setRecurrence(recurrence, recurType, recurInterval, recurCount,
				                       DateTime(dateTime, flags & KAEvent::ANY_TIME), endTime);
			}
			else if (!(function & PRE_091))
			{
				QString rule;
				arg >> rule;
				KCal::ICalFormat format;
				format.fromString(&recurrence, rule);
			}
			return theApp()->scheduleEvent(action, text, dateTime, lateCancel, flags, bgColour, fgColour, font, audioFile,
			                               audioVolume, reminderMinutes, recurrence, 0, 0, QString::null, mailAddresses, mailSubject, mailAttachments);
		}
	}
	return false;
}
#endif // OLD_DCOP
