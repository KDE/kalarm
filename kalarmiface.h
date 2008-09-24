/*
 *  kalarmiface.h  -  D-Bus interface to KAlarm
 *  Program:  kalarm
 *  Copyright © 2004-2008 by David Jarvie <djarvie@kde.org>
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

#ifndef KALARMIFACE_H
#define KALARMIFACE_H

/** @file kalarmiface.h - D-Bus interface to KAlarm */

/** KAlarmIface provides a D-Bus interface for other applications to request
 *  KAlarm actions.
 */

class KAlarmIface
{
    public:
	/** Bit values for the @p flags parameter of "scheduleXxxx()" D-Bus calls.
	 *  The bit values may be OR'ed together.
	 *  @li REPEAT_AT_LOGIN - repeat the alarm at every login.
	 *  @li BEEP            - sound an audible beep when the alarm is displayed.
	 *  @li SPEAK           - speak the alarm message when it is displayed.
	 *  @li REPEAT_SOUND    - repeat the sound file while the alarm is displayed.
	 *  @li CONFIRM_ACK     - closing the alarm message window requires a confirmation prompt.
	 *  @li AUTO_CLOSE      - auto-close the alarm window after the late-cancel period.
	 *  @li DISPLAY_COMMAND - display command output in the alarm window.
	 *  @li SCRIPT          - the command to execute is a script, not a shell command line.
	 *  @li EXEC_IN_XTERM   - execute the command alarm in a terminal window.
	 *  @li EMAIL_BCC       - send a blind copy the email to the user.
	 *  @li SHOW_IN_KORG    - show the alarm as an event in KOrganizer
	 *  @li WORK_TIME_ONLY  - do not trigger the alarm outside working hours
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
		SPEAK           = 0x200,   // speak the alarm message when it is displayed
		SHOW_IN_KORG    = 0x400,   // show the alarm as an event in KOrganizer
		DISPLAY_COMMAND = 0x800,   // display command output in alarm window
		WORK_TIME_ONLY  = 0x3000   // trigger only during working hours
	};
	/** Values for the @p repeatType parameter of "scheduleXxxx()" D-Bus calls.
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
	/** Values for the @p type parameter of "editNew()" D-Bus call.
	 *  @li DISPLAY - create a display alarm.
	 *  @li COMMAND - create a command alarm.
	 *  @li EMAIL   - create an email alarm.
	 */
	enum AlarmType
	{
		DISPLAY = 1,    // display alarm
		COMMAND = 2,    // command alarm
		EMAIL   = 3     // email alarm
	};
};

#endif // KALARMIFACE_H
