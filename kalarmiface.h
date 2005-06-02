/*
 *  kalarmiface.h  -  DCOP interface for KAlarm
 *  Program:  kalarm
 *  (C) 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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
	/** Bit values for the @p flags parameter of "scheduleXxxx()" DCOP calls.
	 *  The bit values may be OR'ed together.
	 *  @li REPEAT_AT_LOGIN - repeat the alarm at every login.
	 *  @li BEEP            - sound an audible beep when the alarm is displayed.
	 *  @li SPEAK           - speak the alarm message when it is displayed.
	 *  @li REPEAT_SOUND    - repeat the sound file while the alarm is displayed.
	 *  @li CONFIRM_ACK     - closing the alarm message window requires a confirmation prompt.
	 *  @li AUTO_CLOSE      - auto-close the alarm window after the late-cancel period.
	 *  @li SCRIPT          - the command to execute is a script, not a shell command line.
	 *  @li EXEC_IN_XTERM   - execute the command alarm in a terminal window.
	 *  @li EMAIL_BCC       - send a blind copy the email to the user.
	 *  @li DISABLED        - set the alarm status to disabled.
	 */
	enum Flags
	{
		REPEAT_AT_LOGIN = 0x01,    // repeat alarm at every login
		BEEP            = 0x02,    // sound audible beep when alarm is displayed
		REPEAT_SOUND    = 0x08,    // repeat sound file while alarm is displayed
		CONFIRM_ACK     = 0x04,    // closing the alarm message window requires confirmation prompt
		AUTO_CLOSE      = 0x10,    // auto-close alarm window after late-cancel period
		EMAIL_BCC       = 0x20,    // blind copy the email to the user
		DISABLED        = 0x40,    // alarm is currently disabled
		SCRIPT          = 0x80,    // command is a script, not a shell command line
		EXEC_IN_XTERM   = 0x100,   // execute command alarm in terminal window
		SPEAK           = 0x200    // speak the alarm message when it is displayed
	};
	/** Values for the @p repeatType parameter of "scheduleXxxx()" DCOP calls.
	 *  @li MINUTELY - the repeat interval is measured in minutes.
	 *  @li DAILY    - the repeat interval is measured in days.
	 *  @li WEEKLY   - the repeat interval is measured in weeks.
	 *  @li MONTHLY  - the repeat interval is measured in months.
	 *  @li YEARLY   - the repeat interval is measured in years.
	 */
	enum RecurType
	{
		MINUTELY = 1,    // the repeat interval is measured in minutes
		DAILY    = 2,    // the repeat interval is measured in days
		WEEKLY   = 3,    // the repeat interval is measured in weeks
		MONTHLY  = 4,    // the repeat interval is measured in months
		YEARLY   = 5     // the repeat interval is measured in years
	};

    k_dcop:
	/** Cancel (delete) an already scheduled alarm.
	 *  @param url     - The URL (not path) of the calendar file containing the event to be cancelled.
	 *                   Used only for integrity checking: the call will fail if it is not KAlarm's
	 *                   current calendar file.
	 *  @param eventId - The unique ID of the event to be cancelled, as stored in the calendar file @p url.
	 */
	virtual bool cancelEvent(const QString& url, const QString& eventId) = 0;

	/** Trigger the immediate display or execution of an alarm, regardless of what time it is scheduled for.
	 *  @param url     - The URL (not path) of the calendar file containing the event to be triggered.
	 *                   Used only for integrity checking: the call will fail if it is not KAlarm's
	 *                   current calendar file.
	 *  @param eventId - The unique ID of the event to be triggered, as stored in the calendar file @p url.
	 */
	virtual bool triggerEvent(const QString& url, const QString& eventId) = 0;

	/** Schedule a message display alarm.
	 *  @param message        The text of the message to display.
	 *  @param startDateTime  Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	 *  @param lateCancel     Late-cancellation period in minutes, or 0 for no cancellation.
	 *  @param flags          OR of flag bits defined in Flags enum.
	 *  @param bgColor        The background colour for the alarm message window, or QString::null for the
	 *                        current default background colour. The string may be in any of the formats
	 *                        accepted by QColor::QColor(const QString&).
	 *  @param fgColor        The foreground colour for the alarm message, or QString::null for the current
	 *                        default foreground colour. The format of the string is the same as for @p bgColor.
	 *  @param font           The font for the alarm message, or QString::null for the default message font
	 *                        current at the time the message is displayed. The string should be in format
	 *                        returned by QFont::toString().
	 *  @param audioFile      The audio file to play when the alarm is displayed, or QString::null for none.
	 *  @param reminderMins   The number of minutes in advance of the main alarm and its recurrences to display
	 *                        a reminder alarm, or 0 for no reminder.
	 *  @param recurrence     Recurrence specification using iCalendar syntax (defined in RFC2445).
	 *  @param repeatInterval Simple repetition repeat interval in minutes, or 0 for no simple repetition.
	 *  @param repeatCount    Simple repetition repeat count (after the first occurrence), or 0 for no simple repetition.
	 *  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
	                             const QString& bgColor, const QString& fgColor, const QString& font,
	                             const KURL& audioFile, int reminderMins, const QString& recurrence,
	                             int repeatInterval, int repeatCount) = 0;
	/** Schedule a message display alarm.
	 *  @param message        The text of the message to display.
	 *  @param startDateTime  Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	 *  @param lateCancel     Late-cancellation period in minutes, or 0 for no cancellation.
	 *  @param flags          OR of flag bits defined in Flags enum.
	 *  @param bgColor        The background colour for the alarm message window, or QString::null for the
	 *                        current default background colour. The string may be in any of the formats
	 *                        accepted by QColor::QColor(const QString&).
	 *  @param fgColor        The foreground colour for the alarm message, or QString::null for the current
	 *                        default foreground colour. The format of the string is the same as for @p bgColor.
	 *  @param font           The font for the alarm message, or QString::null for the default message font
	 *                        current at the time the message is displayed. The string should be in format
	 *                        returned by QFont::toString().
	 *  @param audioFile      The audio file to play when the alarm is displayed, or QString::null for none.
	 *  @param reminderMins   The number of minutes in advance of the main alarm and its recurrences to display
	 *                        a reminder alarm, or 0 for no reminder.
	 *  @param repeatType     The time units to use for recurrence. The actual recurrence interval is equal to 
	 *                        @p repeatType multiplied by @p repeatInterval.
	 *                        The value of @p repeatType must a value defined in the RecurType enum.
	 *  @param repeatInterval Recurrence interval in units defined by @p repeatType, or 0 for no recurrence.
	 *  @param repeatCount    Recurrence count (after the first occurrence), or 0 for no recurrence.
	 *  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
	                             const QString& bgColor, const QString& fgColor, const QString& font,
	                             const KURL& audioFile, int reminderMins,
	                             int repeatType, int repeatInterval, int repeatCount) = 0;
	/** Schedule a message display alarm.
	 *  @param message        The text of the message to display.
	 *  @param startDateTime  Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	 *  @param lateCancel     Late-cancellation period in minutes, or 0 for no cancellation.
	 *  @param flags          OR of flag bits defined in Flags enum.
	 *  @param bgColor        The background colour for the alarm message window, or QString::null for the
	 *                        current default background colour. The string may be in any of the formats
	 *                        accepted by QColor::QColor(const QString&).
	 *  @param fgColor        The foreground colour for the alarm message, or QString::null for the current
	 *                        default foreground colour. The format of the string is the same as for @p bgColor.
	 *  @param font           The font for the alarm message, or QString::null for the default message font
	 *                        current at the time the message is displayed. The string should be in format
	 *                        returned by QFont::toString().
	 *  @param audioFile      The audio file to play when the alarm is displayed, or QString::null for none.
	 *  @param reminderMins   The number of minutes in advance of the main alarm and its recurrences to display
	 *                        a reminder alarm, or 0 for no reminder.
	 *  @param repeatType     The time units to use for recurrence. The actual recurrence interval is equal to 
	 *                        @p repeatType multiplied by @p repeatInterval.
	 *                        The value of @p repeatType must a value defined in the RecurType enum.
	 *  @param repeatInterval Recurrence interval in units defined by @p repeatType, or 0 for no recurrence.
	 *  @param endDateTime    Date/time after which the recurrence will end.
	 *  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
	                             const QString& bgColor, const QString& fgColor, const QString& font,
	                             const KURL& audioFile, int reminderMins,
	                             int repeatType, int repeatInterval, const QString& endDateTime) = 0;
	
	/** Schedule a file display alarm.
	 *  @param file           The text or image file to display.
	 *  @param startDateTime  Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	 *  @param lateCancel     Late-cancellation period in minutes, or 0 for no cancellation.
	 *  @param flags          OR of flag bits defined in Flags enum.
	 *  @param bgColor        The background colour for the alarm message window, or QString::null for the
	 *                        current default background colour. The string may be in any of the formats
	 *                        accepted by QColor::QColor(const QString&).
	 *  @param audioFile      The audio file to play when the alarm is displayed, or QString::null for none.
	 *  @param reminderMins   The number of minutes in advance of the main alarm and its recurrences to display
	 *                        a reminder alarm, or 0 for no reminder.
	 *  @param recurrence     Recurrence specification using iCalendar syntax (defined in RFC2445).
	 *  @param repeatInterval Simple repetition repeat interval in minutes, or 0 for no simple repetition.
	 *  @param repeatCount    Simple repetition repeat count (after the first occurrence), or 0 for no simple repetition.
	 *  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
	                          const KURL& audioFile, int reminderMins, const QString& recurrence,
	                          int repeatInterval, int repeatCount) = 0;
	/** Schedule a file display alarm.
	 *  @param file           The text or image file to display.
	 *  @param startDateTime  Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	 *  @param lateCancel     Late-cancellation period in minutes, or 0 for no cancellation.
	 *  @param flags          OR of flag bits defined in Flags enum.
	 *  @param bgColor        The background colour for the alarm message window, or QString::null for the
	 *                        current default background colour. The string may be in any of the formats
	 *                        accepted by QColor::QColor(const QString&).
	 *  @param audioFile      The audio file to play when the alarm is displayed, or QString::null for none.
	 *  @param repeatType     The time units to use for recurrence. The actual recurrence interval is equal to 
	 *                        @p repeatType multiplied by @p repeatInterval.
	 *                        The value of @p repeatType must a value defined in the RecurType enum.
	 *  @param repeatInterval Recurrence interval in units defined by @p repeatType, or 0 for no recurrence.
	 *  @param repeatCount    Recurrence count (after the first occurrence), or 0 for no recurrence.
	 *  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
	                          const KURL& audioFile, int reminderMins, int repeatType, int repeatInterval, int repeatCount) = 0;
	/** Schedule a file display alarm.
	 *  @param file           The text or image file to display.
	 *  @param startDateTime  Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	 *  @param lateCancel     Late-cancellation period in minutes, or 0 for no cancellation.
	 *  @param flags          OR of flag bits defined in Flags enum.
	 *  @param bgColor        The background colour for the alarm message window, or QString::null for the
	 *                        current default background colour. The string may be in any of the formats
	 *                        accepted by QColor::QColor(const QString&).
	 *  @param audioFile      The audio file to play when the alarm is displayed, or QString::null for none.
	 *  @param repeatType     The time units to use for recurrence. The actual recurrence interval is equal to 
	 *                        @p repeatType multiplied by @p repeatInterval.
	 *                        The value of @p repeatType must a value defined in the RecurType enum.
	 *  @param repeatInterval Recurrence interval in units defined by @p repeatType, or 0 for no recurrence.
	 *  @param endDateTime    Date/time after which the recurrence will end.
	 *  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleFile(const KURL& file, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
	                          const KURL& audioFile, int reminderMins,
	                          int repeatType, int repeatInterval, const QString& endDateTime) = 0;
	
	/** Schedule a command execution alarm.
	 *  @param commandLine    The command line or command script to execute.
	 *  @param startDateTime  Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	 *  @param lateCancel     Late-cancellation period in minutes, or 0 for no cancellation.
	 *  @param flags          OR of flag bits defined in Flags enum.
	 *  @param recurrence     Recurrence specification using iCalendar syntax (defined in RFC2445).
	 *  @param repeatInterval Simple repetition repeat interval in minutes, or 0 for no simple repetition.
	 *  @param repeatCount    Simple repetition repeat count (after the first occurrence), or 0 for no simple repetition.
	 *  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
	                             const QString& recurrence, int repeatInterval, int repeatCount) = 0;
	/** Schedule a command execution alarm.
	 *  @param commandLine    The command line or command script to execute.
	 *  @param startDateTime  Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	 *  @param lateCancel     Late-cancellation period in minutes, or 0 for no cancellation.
	 *  @param flags          OR of flag bits defined in Flags enum.
	 *  @param repeatType     The time units to use for recurrence. The actual recurrence interval is equal to 
	 *                        @p repeatType multiplied by @p repeatInterval.
	 *                        The value of @p repeatType must a value defined in the RecurType enum.
	 *  @param repeatInterval Recurrence interval in units defined by @p repeatType, or 0 for no recurrence.
	 *  @param repeatCount    Recurrence count (after the first occurrence), or 0 for no recurrence.
	 *  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
	                             int repeatType, int repeatInterval, int repeatCount) = 0;
	/** Schedule a command execution alarm.
	 *  @param commandLine    The command line or command script to execute.
	 *  @param startDateTime  Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	 *  @param lateCancel     Late-cancellation period in minutes, or 0 for no cancellation.
	 *  @param flags          OR of flag bits defined in Flags enum.
	 *  @param repeatType     The time units to use for recurrence. The actual recurrence interval is equal to 
	 *                        @p repeatType multiplied by @p repeatInterval.
	 *                        The value of @p repeatType must a value defined in the RecurType enum.
	 *  @param repeatInterval Recurrence interval in units defined by @p repeatType, or 0 for no recurrence.
	 *  @param endDateTime    Date/time after which the recurrence will end.
	 *  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
	                             int repeatType, int repeatInterval, const QString& endDateTime) = 0;

	/** Schedule an email alarm.
	 *  @param fromID         The KMail identity to use as the sender of the email, or QString::null to use KAlarm's default sender ID.
	 *  @param addresses      Comma-separated list of addresses to send the email to.
	 *  @param subject        Subject line of the email.
	 *  @param message        Email message's body text.
	 *  @param attachments    Comma- or semicolon-separated list of paths or URLs of files to send as
	 *                        attachments to the email.
	 *  @param startDateTime  Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	 *  @param lateCancel     Late-cancellation period in minutes, or 0 for no cancellation.
	 *  @param flags          OR of flag bits defined in Flags enum.
	 *  @param recurrence     Recurrence specification using iCalendar syntax (defined in RFC2445).
	 *  @param repeatInterval Simple repetition repeat interval in minutes, or 0 for no simple repetition.
	 *  @param repeatCount    Simple repetition repeat count (after the first occurrence), or 0 for no simple repetition.
	 *  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
	                           const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
	                           const QString& recurrence, int repeatInterval, int repeatCount) = 0;
	/** Schedule an email alarm.
	 *  @param fromID         The KMail identity to use as the sender of the email, or QString::null to use KAlarm's default sender ID.
	 *  @param addresses      Comma-separated list of addresses to send the email to.
	 *  @param subject        Subject line of the email.
	 *  @param message        Email message's body text.
	 *  @param attachments    Comma- or semicolon-separated list of paths or URLs of files to send as
	 *                        attachments to the email.
	 *  @param startDateTime  Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	 *  @param lateCancel     Late-cancellation period in minutes, or 0 for no cancellation.
	 *  @param flags          OR of flag bits defined in the Flags enum.
	 *  @param repeatType     The time units to use for recurrence. The actual recurrence interval is equal to 
	 *                        @p repeatType multiplied by @p repeatInterval.
	 *                        The value of @p repeatType must a value defined in the RecurType enum.
	 *  @param repeatInterval Recurrence interval in units defined by @p repeatType, or 0 for no recurrence.
	 *  @param repeatCount    Recurrence count (after the first occurrence), or 0 for no recurrence.
	 *  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
	                           const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
	                           int repeatType, int repeatInterval, int repeatCount) = 0;
	/** Schedule an email alarm.
	 *  @param fromID         The KMail identity to use as the sender of the email, or QString::null to use KAlarm's default sender ID.
	 *  @param addresses      Comma-separated list of addresses to send the email to.
	 *  @param subject        Subject line of the email.
	 *  @param message        Email message's body text.
	 *  @param attachments    Comma- or semicolon-separated list of paths or URLs of files to send as
	 *                        attachments to the email.
	 *  @param startDateTime  Start date/time, in the format YYYY-MM-DD[THH:MM[:SS]] or [T]HH:MM[:SS]
	 *  @param lateCancel     Late-cancellation period in minutes, or 0 for no cancellation.
	 *  @param flags          OR of flag bits defined in the Flags enum.
	 *  @param repeatType     The time units to use for recurrence. The actual recurrence interval is equal to 
	 *                        @p repeatType multiplied by @p repeatInterval.
	 *                        The value of @p repeatType must a value defined in the RecurType enum.
	 *  @param repeatInterval Recurrence interval in units defined by @p repeatType, or 0 for no recurrence.
	 *  @param endDateTime    Date/time after which the recurrence will end.
	 *  @return true if alarm was scheduled successfully, false if configuration errors were found.
	 */
	virtual bool scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
	                           const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
	                           int repeatType, int repeatInterval, const QString& endDateTime) = 0;
};

#endif // KALARMIFACE_H
