/*
 *  dcophandler.cpp  -  handler for DCOP calls by other applications
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <stdlib.h>

#include <kdebug.h>

#include <libkcal/icalformat.h>
#include "kalarmapp.h"
#include "preferences.h"
#include "kamail.h"
#include "dcophandler.moc"


/*=============================================================================
= DcopHandler
= This class's function is simply to act as a receiver for DCOP requests.
=============================================================================*/
DcopHandler::DcopHandler(const char* dcopObject)
	: QWidget(),
	  DCOPObject(dcopObject)
{
	kdDebug(5950) << "DcopHandler::DcopHandler()\n";
}

/******************************************************************************
* Process a DCOP request.
*/
bool DcopHandler::process(const QCString& func, const QByteArray& data, QCString& replyType, QByteArray&)
{
	kdDebug(5950) << "DcopHandler::process(): " << func << endl;
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
		PRE_091        = 0x1000,           // old-style pre-0.9.1 deprecated method
		PRE_070        = 0x2000 | PRE_091  // old-style pre-0.7 deprecated method
	};
	int function;
	if      (func == "handleEvent(const QString&,const QString&)"
	||       func == "handleEvent(QString,QString)")
		function = HANDLE;
	else if (func == "cancelEvent(const QString&,const QString&)"
	||       func == "cancelEvent(QString,QString)")
		function = CANCEL;
	else if (func == "triggerEvent(const QString&,const QString&)"
	||       func == "triggerEvent(QString,QString)")
		function = TRIGGER;

	//                scheduleMessage(message, dateTime, colour, flags, audioURL, reminder, recurrence)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,const QString&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString,Q_UINT32,QString)")
		function = SCHEDULE | MESSAGE;
	//                scheduleFile(URL, dateTime, colour, flags, audioURL, reminder, recurrence)
	else if (func == "scheduleFile(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,const QString&)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString,Q_UINT32,QString)")
		function = SCHEDULE | FILE;
	//                scheduleCommand(commandLine, dateTime, flags, recurrence)
	else if (func == "scheduleCommand(const QString&,const QDateTime&,Q_UINT32,Q_INT32,const QString&)"
	||       func == "scheduleCommand(QString,QDateTime,Q_UINT32,QString)")
		function = SCHEDULE | COMMAND;
	//                scheduleEmail(addresses, subject, message, attachments, dateTime, flags, recurrence)
	else if (func == "scheduleEmail(const QString&,const QString&,const QString&,const QString&,const QDateTime&,Q_UINT32,Q_INT32,const QString&)"
	||       func == "scheduleEmail(QString,QString,QString,QString,QDateTime,Q_UINT32,QString)")
		function = SCHEDULE | COMMAND;

	//                scheduleMessage(message, dateTime, colour, flags, audioURL, reminder, repeatType, interval, repeatCount)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | MESSAGE | REP_COUNT;
	//                scheduleFile(URL, dateTime, colour, flags, audioURL, reminder, repeatType, interval, repeatCount)
	else if (func == "scheduleFile(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | FILE | REP_COUNT;
	//                scheduleCommand(commandLine, dateTime, flags, repeatType, interval, repeatCount)
	else if (func == "scheduleCommand(const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleCommand(QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | COMMAND | REP_COUNT;
	//                scheduleEmail(addresses, subject, message, attachments, dateTime, flags, repeatType, interval, repeatCount)
	else if (func == "scheduleEmail(const QString&,const QString&,const QString&,const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleEmail(QString,QString,QString,QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | COMMAND | REP_COUNT;

	//                scheduleMessage(message, dateTime, colour, flags, audioURL, reminder, repeatType, interval, endTime)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | MESSAGE | REP_END;
	//                scheduleFile(URL, dateTime, colour, flags, audioURL, reminder, repeatType, interval, endTime)
	else if (func == "scheduleFile(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | FILE | REP_END;
	//                scheduleCommand(commandLine, dateTime, flags, repeatType, interval, endTime)
	else if (func == "scheduleCommand(const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleCommand(QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | COMMAND | REP_END;
	//                scheduleEmail(addresses, subject, message, attachments, dateTime, flags, repeatType, interval, endTime)
	else if (func == "scheduleEmail(const QString&,const QString&,const QString&,const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleEmail(QString,QString,QString,QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | COMMAND | REP_END;

	// Deprecated methods: backwards compatibility with KAlarm pre-0.9.1
	//                scheduleMessage(message, dateTime, colour, flags, audioURL)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString)")
		function = SCHEDULE | MESSAGE | PRE_091;
	//                scheduleFile(URL, dateTime, colour, flags, audioURL)
	else if (func == "scheduleFile(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString)")
		function = SCHEDULE | FILE | PRE_091;
	//                scheduleMessage(message, dateTime, colour, flags, audioURL, repeatType, interval, repeatCount)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | MESSAGE | REP_COUNT | PRE_091;
	//                scheduleFile(URL, dateTime, colour, flags, audioURL, repeatType, interval, repeatCount)
	else if (func == "scheduleFile(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,Q_INT32)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | FILE | REP_COUNT | PRE_091;
	//                scheduleMessage(message, dateTime, colour, flags, audioURL, repeatType, interval, endTime)
	else if (func == "scheduleMessage(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | MESSAGE | REP_END | PRE_091;
	//                scheduleFile(URL, dateTime, colour, flags, audioURL, repeatType, interval, endTime)
	else if (func == "scheduleFile(const QString&,const QDateTime&,QColor,Q_UINT32,const QString&,Q_INT32,Q_INT32,const QDateTime&)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,QString,Q_INT32,Q_INT32,QDateTime)")
		function = SCHEDULE | FILE | REP_END | PRE_091;

	// Deprecated methods: backwards compatibility with KAlarm pre-0.7
	else if (func == "scheduleMessage(const QString&,const QDateTime&,QColor,Q_UINT32,Q_INT32,Q_INT32)"
	||       func == "scheduleMessage(QString,QDateTime,QColor,Q_UINT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | MESSAGE | REP_COUNT | PRE_070;
	else if (func == "scheduleFile(const QString&,const QDateTime&,QColor,Q_UINT32,Q_INT32,Q_INT32)"
	||       func == "scheduleFile(QString,QDateTime,QColor,Q_UINT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | FILE | REP_COUNT | PRE_070;
	else if (func == "scheduleCommand(const QString&,const QDateTime&,Q_UINT32,Q_INT32,Q_INT32)"
	||       func == "scheduleCommand(QString,QDateTime,Q_UINT32,Q_INT32,Q_INT32)")
		function = SCHEDULE | COMMAND | REP_COUNT | PRE_070;

	// Deprecated methods: backwards compatibility with KAlarm pre-0.6
	else if (func == "cancelMessage(const QString&,const QString&)"
	||       func == "cancelMessage(QString,QString)")
		function = CANCEL;
	else if (func == "displayMessage(const QString&,const QString&)"
	||       func == "displayMessage(QString,QString)")
		function = TRIGGER;
	else
	{
		kdDebug(5950) << "DcopHandler::process(): unknown DCOP function" << endl;
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
			replyType = "void";
			switch (function)
			{
				case HANDLE:
					theApp()->handleEvent(urlString, vuid);
					break;
				case CANCEL:
					theApp()->deleteEvent(urlString, vuid);
					break;
				case TRIGGER:
					theApp()->triggerEvent(urlString, vuid);
					break;
			}
			break;
		}
		case SCHEDULE:      // schedule a new event
		{
			KAlarmEvent::Action action;
			switch (function & ALARM_TYPE)
			{
				case MESSAGE:  action = KAlarmEvent::MESSAGE;  break;
				case FILE:     action = KAlarmEvent::FILE;     break;
				case COMMAND:  action = KAlarmEvent::COMMAND;  break;
				case EMAIL:    action = KAlarmEvent::EMAIL;    break;
				default:  return false;
			}
			QDataStream arg(data, IO_ReadOnly);
			QString     text, audioFile, mailSubject;
			EmailAddressList mailAddresses;
			QStringList mailAttachments;
			QDateTime   dateTime, endTime;
			QColor      bgColour;
			QFont       font = theApp()->preferences()->messageFont();
			Q_UINT32    flags;
			KCal::Recurrence  recurrence(0);
			Q_INT32     reminderMinutes = 0;
			if (action == KAlarmEvent::EMAIL)
			{
				QString addresses, attachments;
				arg >> addresses >> mailSubject >> text >> attachments;
				QString bad = KAMail::convertAddresses(addresses, mailAddresses);
				if (!bad.isEmpty())
				{
					kdDebug(5950) << "DcopHandler::process(): invalid email addresses: " << bad << endl;
					return false;
				}
				if (mailAddresses.isEmpty())
				{
					kdDebug(5950) << "DcopHandler::process(): no email address\n";
					return false;
				}
				bad = KAMail::convertAttachments(attachments, mailAttachments, true);
				if (!bad.isEmpty())
				{
					kdDebug(5950) << "DcopHandler::process(): invalid email attachment: " << bad << endl;
					return false;
				}
			}
			else
				arg >> text;
			arg.readRawBytes((char*)&dateTime, sizeof(dateTime));
			if (action != KAlarmEvent::COMMAND)
				arg.readRawBytes((char*)&bgColour, sizeof(bgColour));
			arg >> flags;
			if (!(function & PRE_070))
				arg >> audioFile;
			if (!(function & PRE_091))
				arg >> reminderMinutes;
			if (function & (REP_COUNT | REP_END))
			{
				KAlarmEvent::RecurType recurType;
				Q_INT32 repeatCount = 0;
				Q_INT32 repeatInterval;
				if (function & PRE_070)
				{
					// Backwards compatibility with KAlarm pre-0.7
					recurType = KAlarmEvent::MINUTELY;
					arg >> repeatCount >> repeatInterval;
					++repeatCount;
				}
				else
				{
					Q_INT32 type;
					arg >> type >> repeatInterval;
					recurType = KAlarmEvent::RecurType(type);
					switch (recurType)
					{
						case KAlarmEvent::MINUTELY:
						case KAlarmEvent::DAILY:
						case KAlarmEvent::WEEKLY:
						case KAlarmEvent::MONTHLY_DAY:
						case KAlarmEvent::ANNUAL_DATE:
							break;
						default:
							kdDebug(5950) << "DcopHandler::process(): invalid simple repetition type: " << type << endl;
							return false;
					}
					if (function & REP_COUNT)
						arg >> repeatCount;
					else
						arg.readRawBytes((char*)&endTime, sizeof(endTime));
				}
				KAlarmEvent::setRecurrence(recurrence, recurType, repeatInterval, repeatCount, endTime);
			}
			else if (!(function & PRE_091))
			{
				QString rule;
				arg >> rule;
				KCal::ICalFormat format;
				format.fromString(&recurrence, rule);
			}
			theApp()->scheduleEvent(text, dateTime, bgColour, font, flags, audioFile, mailAddresses, mailSubject,
			                        mailAttachments, action, recurrence, reminderMinutes);
			break;
		}
	}
	replyType = "void";
	return true;
}
