/*
 *  kalarmiface.h  -  D-Bus interface to KAlarm
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

/** @file kalarmiface.h - D-Bus interface to KAlarm */

/** KAlarmIface provides a D-Bus interface for other applications to request
 *  KAlarm actions.
 */

class KAlarmIface
{
public:
    /** Bit values for the @p flags parameter of "scheduleXxxx()" D-Bus calls.
     *  The bit values may be OR'ed together.
     */
    enum Flags
    {
        REPEAT_AT_LOGIN = 0x01,    //!< repeat the alarm at every login
        BEEP            = 0x02,    //!< sound an audible beep when the alarm is displayed
        REPEAT_SOUND    = 0x08,    //!< repeat the sound file while the alarm is displayed
        CONFIRM_ACK     = 0x04,    //!< closing the alarm message window requires a confirmation prompt
        AUTO_CLOSE      = 0x10,    //!< auto-close the alarm window after the late-cancel period
        EMAIL_BCC       = 0x20,    //!< send a blind copy of the email to the user
        DISABLED        = 0x40,    //!< set the alarm status to disabled
        SCRIPT          = 0x80,    //!< the command to execute is a script, not a shell command line
        EXEC_IN_XTERM   = 0x100,   //!< execute the command alarm in a terminal window
        SPEAK           = 0x200,   //!< speak the alarm message when it is displayed
        SHOW_IN_KORG    = 0x400,   //!< show the alarm as an event in KOrganizer
        DISPLAY_COMMAND = 0x800,   //!< display command output in the alarm window
        EXCL_HOLIDAYS   = 0x1000,  //!< don't trigger the alarm on holidays
        WORK_TIME_ONLY  = 0x2000,  //!< do not trigger the alarm outside working hours (or on holidays)
        NOTIFY          = 0x4000   //!< display the alarm message as a notification
    };

    /** Values for the @p repeatType parameter of "scheduleXxxx()" D-Bus calls. */
    enum RecurType
    {
        MINUTELY = 1,    //!< the repeat interval is measured in minutes
        DAILY    = 2,    //!< the repeat interval is measured in days
        WEEKLY   = 3,    //!< the repeat interval is measured in weeks
        MONTHLY  = 4,    //!< the repeat interval is measured in months
        YEARLY   = 5     //!< the repeat interval is measured in years
    };

    /** Values for the @p type parameter of "editNew()" D-Bus call. */
    enum AlarmType
    {
        DISPLAY = 1,    //!< create a display alarm
        COMMAND = 2,    //!< create a command alarm
        EMAIL   = 3,    //!< create an email alarm
        AUDIO   = 4     //!< create an audio alarm
    };
};


// vim: et sw=4:
