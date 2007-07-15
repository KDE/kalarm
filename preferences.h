/*
 *  preferences.h  -  program preference settings
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <software@astrojar.org.uk>
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

#include <QObject>
#include <QColor>
#include <QFont>
#include <QDateTime>
class KTimeZone;

#include "colourlist.h"
#include "kalarmconfig.h"


// Settings configured in the Preferences dialog
class Preferences : public PreferencesBase
{
		Q_OBJECT
	public:
		enum MailFrom   { MAIL_FROM_KMAIL, MAIL_FROM_CONTROL_CENTRE, MAIL_FROM_ADDR };

		static Preferences*     self();
		static void             connect(const char* signal, const QObject* receiver, const char* member);

		// Access to settings
		static KTimeZone        timeZone(bool reload = false);
		static void             setTimeZone(const KTimeZone&);
		static ColourList       messageColours();
		static void             setMessageColours(const ColourList&);
		static QColor           defaultFgColour()                { return Qt::black; }
		static QTime            startOfDay()                     { return self()->mBase_StartOfDay.time(); }
		static void             setStartOfDay(const QTime&);
		static void             updateStartOfDayCheck(const QTime&);
		static bool             hasStartOfDayChanged()           { return mStartOfDayChanged; }
		static QTime            workDayStart()                   { return self()->mBase_WorkDayStart.time(); }
		static QTime            workDayEnd()                     { return self()->mBase_WorkDayEnd.time(); }
		static QBitArray        workDays();
		static void             setWorkDayStart(const QTime& t)  { self()->setBase_WorkDayStart(QDateTime(QDate(1900,1,1), t)); }
		static void             setWorkDayEnd(const QTime& t)    { self()->setBase_WorkDayEnd(QDateTime(QDate(1900,1,1), t)); }
		static void             setWorkDays(const QBitArray&);
		static bool             quitWarn()                       { return notifying(QUIT_WARN); }
		static void             setQuitWarn(bool yes)            { setNotify(QUIT_WARN, yes); }
		static bool             confirmAlarmDeletion()           { return notifying(CONFIRM_ALARM_DELETION); }
		static void             setConfirmAlarmDeletion(bool yes){ setNotify(CONFIRM_ALARM_DELETION, yes); }
		static bool             showAlarmTime()                  { return !self()->showTimeToAlarm() || self()->base_ShowAlarmTime(); }
		static void             setShowAlarmTime(bool yes)       { self()->setBase_ShowAlarmTime(yes); }
		static bool             emailCopyToKMail()               { return self()->mBase_EmailCopyToKMail  &&  self()->mEmailClient == sendmail; }
		static void             setEmailCopyToKMail(bool yes)    { self()->setBase_EmailCopyToKMail(yes); }
		static bool             emailQueuedNotify()              { return notifying(EMAIL_QUEUED_NOTIFY); }
		static void             setEmailQueuedNotify(bool yes)   { setNotify(EMAIL_QUEUED_NOTIFY, yes); }
		static MailFrom         emailFrom();
		static QString          emailAddress();
		static void             setEmailAddress(MailFrom, const QString& address);
		static MailFrom         emailBccFrom();
		static QString          emailBccAddress();
		static void             setEmailBccAddress(bool useControlCentre, const QString& address);
		static bool             emailBccUseControlCentre();
		static QString          cmdXTermCommand();
		static void             setCmdXTermCommand(const QString& cmd);
		static float            defaultSoundVolume()             { int vol = self()->mBase_DefaultSoundVolume; return (vol < 0) ? -1 : static_cast<float>(vol) / 100; }
		static void             setDefaultSoundVolume(float v)   { self()->setBase_DefaultSoundVolume(v < 0 ? -1 : static_cast<int>(v * 100)); }

		// Config file entry names for notification messages
		static const char*      QUIT_WARN;
		static const char*      CONFIRM_ALARM_DELETION;
		static const char*      EMAIL_QUEUED_NOTIFY;


	signals:
		void  startOfDayChanged(const QTime& newStartOfDay, const QTime& oldStartOfDay);
		void  workTimeChanged(const QTime& startTime, const QTime& endTime, const QBitArray& workDays);

	private slots:
		void  startDayChange(const QDateTime&);
		void  workTimeChange(const QDateTime&, const QDateTime&, int days);

	private:
		Preferences();         // only one instance allowed
		static int              startOfDayCheck(const QTime&);
		static void             setNotify(const QString& messageID, bool notify);
		static bool             notifying(const QString& messageID);

		static Preferences*     mInstance;
		static KTimeZone        mSystemTimeZone;

		// All the following members are accessed by the Preferences dialog classes
		ColourList          mMessageColours;
		static int              mMessageButtonDelay;  // 0 = scatter; -1 = no delay, no scatter; >0 = delay, no scatter
		static QTime            mOldStartOfDay;       // previous start-of-day time
		static bool             mStartOfDayChanged;   // start-of-day check value doesn't tally with new StartOfDay
};

#endif // PREFERENCES_H
