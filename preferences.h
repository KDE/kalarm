/*
 *  preferences.h  -  program preference settings
 *  Program:  kalarm
 *  (C) 2001 - 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
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
#include "recurrenceedit.h"
#include "reminder.h"


// Settings configured in the Preferences dialog
class Preferences : public QObject
{
		Q_OBJECT
	public:
		enum MailClient { SENDMAIL, KMAIL };
		enum Feb29Type  { FEB29_MAR1, FEB29_FEB28, FEB29_NONE };

		static Preferences* instance();

		const ColourList& messageColours() const        { return mMessageColours; }
		QColor         defaultBgColour() const          { return mDefaultBgColour; }
		QColor         defaultFgColour() const          { return default_defaultFgColour; }
		const QFont&   messageFont() const              { return mMessageFont; }
		const QTime&   startOfDay() const               { return mStartOfDay; }
		bool           hasStartOfDayChanged() const     { return mStartOfDayChanged; }
		bool           runInSystemTray() const          { return mRunInSystemTray; }
		bool           disableAlarmsIfStopped() const   { return mDisableAlarmsIfStopped; }
		bool           quitWarn() const;
		void           setQuitWarn(bool);
		bool           autostartTrayIcon() const        { return mAutostartTrayIcon; }
		bool           confirmAlarmDeletion() const;
		void           setConfirmAlarmDeletion(bool);
		Feb29Type      feb29RecurType() const           { return mFeb29RecurType; }
		bool           modalMessages() const            { return mModalMessages; }
		bool           showExpiredAlarms() const        { return mShowExpiredAlarms; }
		bool           showAlarmTime() const            { return mShowAlarmTime; }
		bool           showTimeToAlarm() const          { return mShowTimeToAlarm; }
		int            tooltipAlarmCount() const        { return mTooltipAlarmCount; }
		bool           showTooltipAlarmTime() const     { return mShowTooltipAlarmTime; }
		bool           showTooltipTimeToAlarm() const   { return mShowTooltipTimeToAlarm; }
		const QString& tooltipTimeToPrefix() const      { return mTooltipTimeToPrefix; }
		int            daemonTrayCheckInterval() const  { return mDaemonTrayCheckInterval; }
		MailClient     emailClient() const              { return mEmailClient; }
		bool           emailQueuedNotify() const        { return mEmailQueuedNotify; }
		bool           emailUseControlCentre() const    { return mEmailUseControlCentre; }
		bool           emailBccUseControlCentre() const { return mEmailBccUseControlCentre; }
		QString        emailAddress() const;
		QString        emailBccAddress() const;
		QColor         expiredColour() const            { return mExpiredColour; }
		int            expiredKeepDays() const          { return mExpiredKeepDays; }
		bool           defaultSound() const             { return mDefaultSound; }
		const QString& defaultSoundFile() const         { return mDefaultSoundFile; }
		float          defaultSoundVolume() const       { return mDefaultSoundVolume; }
		bool           defaultSoundRepeat() const       { return mDefaultSoundRepeat; }
		bool           defaultBeep() const              { return mDefaultBeep; }
		bool           defaultLateCancel() const        { return mDefaultLateCancel; }
		bool           defaultConfirmAck() const        { return mDefaultConfirmAck; }
		bool           defaultEmailBcc() const          { return mDefaultEmailBcc; }
		RecurrenceEdit::RepeatType
		               defaultRecurPeriod() const       { return mDefaultRecurPeriod; }
		Reminder::Units defaultReminderUnits() const    { return mDefaultReminderUnits; }
		const QString& defaultPreAction() const         { return mDefaultPreAction; }
		const QString& defaultPostAction() const        { return mDefaultPostAction; }

		void           save(bool syncToDisc = true);
		void           syncToDisc();
		void           updateStartOfDayCheck();

		static void    setNotify(const QString& messageID, bool yesNoMessage, bool notify);
		static bool    notifying(const QString& messageID, bool yesNoMessage);

		// Config file entry names for notification messages
		static const QString     QUIT_WARN;
		static const QString     CONFIRM_ALARM_DELETION;

		// Default values for settings
		static const ColourList  default_messageColours;
		static const QColor      default_defaultBgColour;
		static const QColor      default_defaultFgColour;
		static QFont             default_messageFont;
		static const QTime       default_startOfDay;
		static const bool        default_runInSystemTray;
		static const bool        default_disableAlarmsIfStopped;
		static const bool        default_quitWarn;
		static const bool        default_autostartTrayIcon;
		static const bool        default_confirmAlarmDeletion;
		static const Feb29Type   default_feb29RecurType;
		static const bool        default_modalMessages;
		static const bool        default_showExpiredAlarms;
		static const bool        default_showAlarmTime;
		static const bool        default_showTimeToAlarm;
		static const int         default_tooltipAlarmCount;
		static const bool        default_showTooltipAlarmTime;
		static const bool        default_showTooltipTimeToAlarm;
		static const QString     default_tooltipTimeToPrefix;
		static const int         default_daemonTrayCheckInterval;
		static const MailClient  default_emailClient;
		static const bool        default_emailQueuedNotify;
		static const bool        default_emailUseControlCentre;
		static const bool        default_emailBccUseControlCentre;
		static const QString     default_emailAddress;
		static const QString     default_emailBccAddress;
		static const QColor      default_expiredColour;
		static const int         default_expiredKeepDays;
		static const QString     default_defaultSoundFile;
		static const float       default_defaultSoundVolume;
		static const bool        default_defaultSound;
		static const bool        default_defaultSoundRepeat;
		static const bool        default_defaultBeep;
		static const bool        default_defaultLateCancel;
		static const bool        default_defaultConfirmAck;
		static const bool        default_defaultEmailBcc;
		static const RecurrenceEdit::RepeatType
		                         default_defaultRecurPeriod;
		static const Reminder::Units
		                         default_defaultReminderUnits;
		static const QString     default_defaultPreAction;
		static const QString     default_defaultPostAction;

	signals:
		void preferencesChanged();
		void startOfDayChanged(const QTime& oldStartOfDay);

	private:
		Preferences();     // only one instance allowed

		static Preferences* mInstance;
		int                 startOfDayCheck() const;
		QString             mEmailAddress;
		QString             mEmailBccAddress;

		// All the following members are accessed by the Preferences dialog classes
		friend class MiscPrefTab;
		friend class EditPrefTab;
		friend class ViewPrefTab;
		friend class MessagePrefTab;
		friend class EmailPrefTab;
		void                setEmailAddress(bool useControlCentre, const QString& address);
		void                setEmailBccAddress(bool useControlCentre, const QString& address);
		ColourList          mMessageColours;
		QColor              mDefaultBgColour;
		QFont               mMessageFont;
		QTime               mStartOfDay;
		bool                mRunInSystemTray;
		bool                mDisableAlarmsIfStopped;
		bool                mAutostartTrayIcon;
		Feb29Type           mFeb29RecurType;
		bool                mModalMessages;
		bool                mShowExpiredAlarms;
		bool                mShowAlarmTime;
		bool                mShowTimeToAlarm;
		int                 mTooltipAlarmCount;
		bool                mShowTooltipAlarmTime;
		bool                mShowTooltipTimeToAlarm;
		QString             mTooltipTimeToPrefix;
		int                 mDaemonTrayCheckInterval;
		MailClient          mEmailClient;
		bool                mEmailQueuedNotify;
		bool                mEmailUseControlCentre;
		bool                mEmailBccUseControlCentre;
		QColor              mExpiredColour;
		int                 mExpiredKeepDays;     // 0 = don't keep, -1 = keep indefinitely
		// Default settings for Edit Alarm dialog
		QString             mDefaultSoundFile;
		float               mDefaultSoundVolume;
		bool                mDefaultSound;
		bool                mDefaultSoundRepeat;
		bool                mDefaultBeep;
		bool                mDefaultLateCancel;
		bool                mDefaultConfirmAck;
		bool                mDefaultEmailBcc;
		RecurrenceEdit::RepeatType  mDefaultRecurPeriod;
		Reminder::Units     mDefaultReminderUnits;
		QString             mDefaultPreAction;
		QString             mDefaultPostAction;
		// Change tracking
		QTime               mOldStartOfDay;       // previous start-of-day time
		bool                mStartOfDayChanged;   // start-of-day check value doesn't tally with mStartOfDay
};

#endif // PREFERENCES_H
