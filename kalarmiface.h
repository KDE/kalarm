/*
 *  kalarmiface.h  -  DCOP interface for KAlarm
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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

#ifndef KALARMIFACE_H
#define KALARMIFACE_H

// No forward declarations - dcopidl2cpp won't work
#include <dcopobject.h>
#include <kurl.h>
#include <qstringlist.h>
class QString;

class KAlarmIface : virtual public DCOPObject
{
	K_DCOP
    public:
	// Bit values for the 'flags' parameter of "scheduleXxxx()" DCOP calls.
	// The bit values may be OR'ed together.
	enum
	{
		REPEAT_AT_LOGIN = 0x01,    // repeat alarm at every login
		BEEP            = 0x02,    // sound audible beep when alarm is displayed
		CONFIRM_ACK     = 0x04,    // closing the alarm message window requires confirmation prompt
		REPEAT_SOUND    = 0x08,    // repeat sound file while alarm is displayed
		AUTO_CLOSE      = 0x10,    // auto-close alarm window after late-cancel period
		EMAIL_BCC       = 0x20,    // blind copy the email to the user
		DISABLED        = 0x40     // alarm is currently disabled
	};
	// Values for the 'repeatType' parameter of "scheduleXxxx()" DCOP calls
	enum
	{
		MINUTELY = 1,    // the repeat interval is measured in minutes
		DAILY    = 2,    // the repeat interval is measured in days
		WEEKLY   = 3,    // the repeat interval is measured in weeks
		MONTHLY  = 4,    // the repeat interval is measured in months
		YEARLY   = 5     // the repeat interval is measured in years
	};
    k_dcop:
	virtual bool cancelEvent(const QString& url,const QString& eventId) = 0;
	virtual bool triggerEvent(const QString& url,const QString& eventId) = 0;

	virtual bool scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
	                             const QString& bgColor, const QString& fgColor, const QString& font,
	                             const KURL& audioFile, int reminderMins, const QString& recurrence,
	                             int repeatInterval, int repeatCount) = 0;
	virtual bool scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
	                             const QString& bgColor, const QString& fgColor, const QString& font,
	                             const KURL& audioFile, int reminderMins,
	                             int repeatType, int repeatInterval, int repeatCount) = 0;
	virtual bool scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
	                             const QString& bgColor, const QString& fgColor, const QString& font,
	                             const KURL& audioFile, int reminderMins,
	                             int repeatType, int repeatInterval, const QString& endDateTime) = 0;
	virtual bool scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
	                          const KURL& audioFile, int reminderMins, const QString& recurrence,
	                          int repeatInterval, int repeatCount) = 0;
	virtual bool scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
	                          const KURL& audioFile, int reminderMins, int repeatType, int repeatInterval, int repeatCount) = 0;
	virtual bool scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
	                          const KURL& audioFile, int reminderMins,
	                          int repeatType, int repeatInterval, const QString& endDateTime) = 0;
	virtual bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
	                             const QString& recurrence, int repeatInterval, int repeatCount) = 0;
	virtual bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
	                             int repeatType, int repeatInterval, int repeatCount) = 0;
	virtual bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
	                             int repeatType, int repeatInterval, const QString& endDateTime) = 0;
	/**
	  Schedule an email alarm.
	  @param fromID The KMail identity to use as the sender of the email, or QString::null to use KAlarm's default sender ID.
	  @param addresses Comma-separated list of addresses to send the email to.
	  @param subject Subject line of the email.
	  @param message Email message's body text.
	  @param attachments Comma- or semicolon-separated list of files to send as attachments to the email.
	  @param startDateTime Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	  @param lateCancel Late-cancellation period in minutes, or 0 for no cancellation.
	  @param flags OR of flag bits defined in enum above.
	  @param recurrence Recurrence specification using iCalendar syntax (defined in RFC2445).
	  @param repeatInterval Simple repetition repeat interval in minutes, or 0 for no simple repetition.
	  @param repeatCount Simple repetition repeat count (after the first occurrence), or 0 for no simple repetition.
	  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
	                           const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
	                           const QString& recurrence, int repeatInterval, int repeatCount) = 0;
	virtual bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
	                           const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
	                           int repeatType, int repeatInterval, int repeatCount) = 0;
	virtual bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
	                           const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
	                           int repeatType, int repeatInterval, const QString& endDateTime) = 0;
};

#endif // KALARMIFACE_H
