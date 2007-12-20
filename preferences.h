/*
 *  preferences.h  -  program preference settings
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <djarvie@kde.org>
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

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "kalarm.h"

#include <qobject.h>
#include <qcolor.h>
#include <qfont.h>
#include <qdatetime.h>
#include <qvaluelist.h>
class QWidget;

#include "colourlist.h"
#include "editdlg.h"
#include "karecurrence.h"
#include "recurrenceedit.h"
#include "soundpicker.h"
#include "timeperiod.h"


// Settings configured in the Preferences dialog
class Preferences : public QObject
{
		Q_OBJECT
	public:
		enum MailClient { SENDMAIL, KMAIL };
		enum MailFrom   { MAIL_FROM_KMAIL, MAIL_FROM_CONTROL_CENTRE, MAIL_FROM_ADDR };
		enum CmdLogType { DISCARD_OUTPUT, LOG_TO_FILE, EXEC_IN_TERMINAL };

		static void              initialise();
		static void              save(bool syncToDisc = true);
		static void              syncToDisc();
		static void              updateStartOfDayCheck();
		static void              connect(const char* signal, const QObject* receiver, const char* member);

		// Access to settings
		static const ColourList& messageColours()                 { return mMessageColours; }
		static QColor            defaultBgColour()                { return mDefaultBgColour; }
		static QColor            defaultFgColour()                { return default_defaultFgColour; }
		static const QFont&      messageFont()                    { return mMessageFont; }
		static const QTime&      startOfDay()                     { return mStartOfDay; }
		static bool              hasStartOfDayChanged()           { return mStartOfDayChanged; }
		static bool              runInSystemTray()                { return mRunInSystemTray; }
		static bool              disableAlarmsIfStopped()         { return mDisableAlarmsIfStopped; }
		static bool              quitWarn()                       { return notifying(QUIT_WARN); }
		static void              setQuitWarn(bool yes)            { setNotify(QUIT_WARN, yes); }
		static bool              autostartTrayIcon()              { return mAutostartTrayIcon; }
		static bool              confirmAlarmDeletion()           { return notifying(CONFIRM_ALARM_DELETION); }
		static void              setConfirmAlarmDeletion(bool yes){ setNotify(CONFIRM_ALARM_DELETION, yes); }
		static bool              modalMessages()                  { return mModalMessages; }
		static int               messageButtonDelay()             { return mMessageButtonDelay; }
		static int               tooltipAlarmCount()              { return mTooltipAlarmCount; }
		static bool              showTooltipAlarmTime()           { return mShowTooltipAlarmTime; }
		static bool              showTooltipTimeToAlarm()         { return mShowTooltipTimeToAlarm; }
		static const QString&    tooltipTimeToPrefix()            { return mTooltipTimeToPrefix; }
		static int               daemonTrayCheckInterval()        { return mDaemonTrayCheckInterval; }
		static MailClient        emailClient()                    { return mEmailClient; }
		static bool              emailCopyToKMail()               { return mEmailCopyToKMail  &&  mEmailClient == SENDMAIL; }
		static bool              emailQueuedNotify()              { return notifying(EMAIL_QUEUED_NOTIFY); }
		static void              setEmailQueuedNotify(bool yes)   { setNotify(EMAIL_QUEUED_NOTIFY, yes); }
		static MailFrom          emailFrom()                      { return mEmailFrom; }
		static bool              emailBccUseControlCentre()       { return mEmailBccFrom == MAIL_FROM_CONTROL_CENTRE; }
		static QString           emailAddress();
		static QString           emailBccAddress();
		static QString           cmdXTermCommand()                { return mCmdXTermCommand; }
		static QColor            disabledColour()                 { return mDisabledColour; }
		static QColor            expiredColour()                  { return mExpiredColour; }
		static int               expiredKeepDays()                { return mExpiredKeepDays; }
		static SoundPicker::Type defaultSoundType()               { return mDefaultSoundType; }
		static const QString&    defaultSoundFile()               { return mDefaultSoundFile; }
		static float             defaultSoundVolume()             { return mDefaultSoundVolume; }
		static bool              defaultSoundRepeat()             { return mDefaultSoundRepeat; }
		static int               defaultLateCancel()              { return mDefaultLateCancel; }
		static bool              defaultAutoClose()               { return mDefaultAutoClose; }
		static bool              defaultConfirmAck()              { return mDefaultConfirmAck; }
		static bool              defaultCopyToKOrganizer()        { return mDefaultCopyToKOrganizer; }
		static bool              defaultCmdScript()               { return mDefaultCmdScript; }
		static EditAlarmDlg::CmdLogType  
		                         defaultCmdLogType()              { return mDefaultCmdLogType; }
		static QString           defaultCmdLogFile()              { return mDefaultCmdLogFile; }
		static bool              defaultEmailBcc()                { return mDefaultEmailBcc; }
		static RecurrenceEdit::RepeatType
		                         defaultRecurPeriod()             { return mDefaultRecurPeriod; }
		static KARecurrence::Feb29Type
		                         defaultFeb29Type()               { return mDefaultFeb29Type; }
		static TimePeriod::Units defaultReminderUnits()           { return mDefaultReminderUnits; }
		static const QString&    defaultPreAction()               { return mDefaultPreAction; }
		static const QString&    defaultPostAction()              { return mDefaultPostAction; }

		// Config file entry names for notification messages
		static const QString     QUIT_WARN;
		static const QString     CONFIRM_ALARM_DELETION;
		static const QString     EMAIL_QUEUED_NOTIFY;

		// Default values for settings
		static const ColourList                 default_messageColours;
		static const QColor                     default_defaultBgColour;
		static const QColor                     default_defaultFgColour;
		static const QFont&                     default_messageFont()  { return mDefault_messageFont; };
		static const QTime                      default_startOfDay;
		static const bool                       default_runInSystemTray;
		static const bool                       default_disableAlarmsIfStopped;
		static const bool                       default_quitWarn;
		static const bool                       default_autostartTrayIcon;
		static const bool                       default_confirmAlarmDeletion;
		static const bool                       default_modalMessages;
		static const int                        default_messageButtonDelay;
		static const int                        default_tooltipAlarmCount;
		static const bool                       default_showTooltipAlarmTime;
		static const bool                       default_showTooltipTimeToAlarm;
		static const QString                    default_tooltipTimeToPrefix;
		static const int                        default_daemonTrayCheckInterval;
		static const MailClient                 default_emailClient;
		static const bool                       default_emailCopyToKMail;
		static MailFrom                         default_emailFrom();
		static const bool                       default_emailQueuedNotify;
		static const MailFrom                   default_emailBccFrom;
		static const QString                    default_emailAddress;
		static const QString                    default_emailBccAddress;
		static const QColor                     default_disabledColour;
		static const QColor                     default_expiredColour;
		static const int                        default_expiredKeepDays;
		static const QString                    default_defaultSoundFile;
		static const float                      default_defaultSoundVolume;
		static const int                        default_defaultLateCancel;
		static const bool                       default_defaultAutoClose;
		static const bool                       default_defaultCopyToKOrganizer;
		static const SoundPicker::Type          default_defaultSoundType;
		static const bool                       default_defaultSoundRepeat;
		static const bool                       default_defaultConfirmAck;
		static const bool                       default_defaultCmdScript;
		static const EditAlarmDlg::CmdLogType   default_defaultCmdLogType;
		static const bool                       default_defaultEmailBcc;
		static const RecurrenceEdit::RepeatType default_defaultRecurPeriod;
		static const KARecurrence::Feb29Type    default_defaultFeb29Type;
		static const TimePeriod::Units          default_defaultReminderUnits;
		static const QString                    default_defaultPreAction;
		static const QString                    default_defaultPostAction;

	signals:
		void  preferencesChanged();
		void  startOfDayChanged(const QTime& oldStartOfDay);

	private:
		Preferences()  { }     // only one instance allowed
		void  emitPreferencesChanged();
		void  emitStartOfDayChanged();

		static void                read();
		static void                convertOldPrefs();
		static int                 startOfDayCheck();
		static QString	           emailFrom(MailFrom, bool useAddress, bool bcc);
		static MailFrom            emailFrom(const QString&);
		static void                setNotify(const QString& messageID, bool notify);
		static bool                notifying(const QString& messageID);

		static Preferences*        mInstance;
		static QFont               mDefault_messageFont;
		static QString             mEmailAddress;
		static QString             mEmailBccAddress;

		// All the following members are accessed by the Preferences dialog classes
		friend class MiscPrefTab;
		friend class EditPrefTab;
		friend class ViewPrefTab;
		friend class FontColourPrefTab;
		friend class EmailPrefTab;
		static void                setEmailAddress(MailFrom, const QString& address);
		static void                setEmailBccAddress(bool useControlCentre, const QString& address);
		static ColourList          mMessageColours;
		static QColor              mDefaultBgColour;
		static QFont               mMessageFont;
		static QTime               mStartOfDay;
		static bool                mRunInSystemTray;
		static bool                mDisableAlarmsIfStopped;
		static bool                mAutostartTrayIcon;
		static bool                mModalMessages;
		static int                 mMessageButtonDelay;  // 0 = scatter; -1 = no delay, no scatter; >0 = delay, no scatter
		static int                 mTooltipAlarmCount;
		static bool                mShowTooltipAlarmTime;
		static bool                mShowTooltipTimeToAlarm;
		static QString             mTooltipTimeToPrefix;
		static int                 mDaemonTrayCheckInterval;
		static MailClient          mEmailClient;
		static MailFrom            mEmailFrom;
		static MailFrom            mEmailBccFrom;
		static bool                mEmailCopyToKMail;
		static QString             mCmdXTermCommand;
		static QColor              mDisabledColour;
		static QColor              mExpiredColour;
		static int                 mExpiredKeepDays;     // 0 = don't keep, -1 = keep indefinitely
		// Default settings for Edit Alarm dialog
		static QString             mDefaultSoundFile;
		static float               mDefaultSoundVolume;
		static int                 mDefaultLateCancel;
		static bool                mDefaultAutoClose;
		static bool                mDefaultCopyToKOrganizer;
		static SoundPicker::Type   mDefaultSoundType;
		static bool                mDefaultSoundRepeat;
		static bool                mDefaultConfirmAck;
		static bool                mDefaultEmailBcc;
		static bool                mDefaultCmdScript;
		static EditAlarmDlg::CmdLogType   mDefaultCmdLogType;
		static QString                    mDefaultCmdLogFile;
		static RecurrenceEdit::RepeatType mDefaultRecurPeriod;
		static KARecurrence::Feb29Type    mDefaultFeb29Type;
		static TimePeriod::Units   mDefaultReminderUnits;
		static QString             mDefaultPreAction;
		static QString             mDefaultPostAction;
		// Change tracking
		static QTime               mOldStartOfDay;       // previous start-of-day time
		static bool                mStartOfDayChanged;   // start-of-day check value doesn't tally with mStartOfDay
};

#endif // PREFERENCES_H
