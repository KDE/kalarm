/*
 *  preferences.h  -  program preference settings
 *  Program:  kalarm
 *  Copyright © 2001-2010 by David Jarvie <djarvie@kde.org>
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

#include "kalarmconfig.h"

#include <QObject>
#include <QDateTime>

class KTimeZone;
namespace KHolidays { class HolidayRegion; }


// Settings configured in the Preferences dialog
class Preferences : public PreferencesBase
{
        Q_OBJECT
    public:
        enum MailFrom   { MAIL_FROM_KMAIL, MAIL_FROM_SYS_SETTINGS, MAIL_FROM_ADDR };

        static Preferences*     self();
        static void             connect(const char* signal, const QObject* receiver, const char* member);
        static bool             autoStartChangedByUser()         { return mAutoStartChangedByUser; }
        static void             setAutoStartChangedByUser(bool c){ mAutoStartChangedByUser = c; }

        // Access to settings
        static QString          previousVersion()                { return mPreviousVersion; }
        static Backend          previousBackend()                { return mPreviousBackend; }
        static void             setAskAutoStart(bool yes);
        static KTimeZone        timeZone(bool reload = false);
        static void             setTimeZone(const KTimeZone&);
        static const KHolidays::HolidayRegion& holidays();
        static void             setHolidayRegion(const QString& regionCode);
        static QTime            startOfDay()                     { return self()->mBase_StartOfDay.time(); }
        static void             setStartOfDay(const QTime&);
        static QTime            workDayStart()                   { return self()->mBase_WorkDayStart.time(); }
        static QTime            workDayEnd()                     { return self()->mBase_WorkDayEnd.time(); }
        static QBitArray        workDays();
        static void             setWorkDayStart(const QTime& t)  { self()->setBase_WorkDayStart(QDateTime(QDate(1900,1,1), t)); }
        static void             setWorkDayEnd(const QTime& t)    { self()->setBase_WorkDayEnd(QDateTime(QDate(1900,1,1), t)); }
        static void             setWorkDays(const QBitArray&);
        static bool             quitWarn()                       { return mUsingDefaults ? self()->base_QuitWarn() : notifying(QLatin1String(QUIT_WARN)); }
        static void             setQuitWarn(bool yes)            { setNotify(QLatin1String(QUIT_WARN), yes); }
        static bool             confirmAlarmDeletion()           { return mUsingDefaults ? self()->base_ConfirmAlarmDeletion() : notifying(QLatin1String(CONFIRM_ALARM_DELETION)); }
        static void             setConfirmAlarmDeletion(bool yes){ setNotify(QLatin1String(CONFIRM_ALARM_DELETION), yes); }
        static bool             emailCopyToKMail()               { return self()->mBase_EmailCopyToKMail  &&  self()->mEmailClient == sendmail; }
        static void             setEmailCopyToKMail(bool yes)    { self()->setBase_EmailCopyToKMail(yes); }
        static bool             emailQueuedNotify()              { return mUsingDefaults ? self()->base_EmailQueuedNotify() : notifying(QLatin1String(EMAIL_QUEUED_NOTIFY)); }
        static void             setEmailQueuedNotify(bool yes)   { setNotify(QLatin1String(EMAIL_QUEUED_NOTIFY), yes); }
        static MailFrom         emailFrom();
        static QString          emailAddress();
        static void             setEmailAddress(MailFrom, const QString& address);
        static MailFrom         emailBccFrom();
        static QString          emailBccAddress();
        static void             setEmailBccAddress(bool useSystemSettings, const QString& address);
        static bool             emailBccUseSystemSettings();
        static QString          cmdXTermCommand();
        static void             setCmdXTermCommand(const QString& cmd);
        static float            defaultSoundVolume()             { int vol = self()->mBase_DefaultSoundVolume; return (vol < 0) ? -1 : static_cast<float>(vol) / 100; }
        static void             setDefaultSoundVolume(float v)   { self()->setBase_DefaultSoundVolume(v < 0 ? -1 : static_cast<int>(v * 100)); }

        // Config file entry names for notification messages
        static const char*      QUIT_WARN;
        static const char*      ASK_AUTO_START;
        static const char*      CONFIRM_ALARM_DELETION;
        static const char*      EMAIL_QUEUED_NOTIFY;

        virtual bool useDefaults(bool def)   { mUsingDefaults = def;  return PreferencesBase::useDefaults(def); }

    signals:
        void  timeZoneChanged(const KTimeZone& newTz);
        void  holidaysChanged(const KHolidays::HolidayRegion& newHolidays);
        void  startOfDayChanged(const QTime& newStartOfDay);
        void  workTimeChanged(const QTime& startTime, const QTime& endTime, const QBitArray& workDays);

    private slots:
        void  timeZoneChange(const QString&);
        void  holidaysChange(const QString& regionCode);
        void  startDayChange(const QDateTime&);
        void  workTimeChange(const QDateTime&, const QDateTime&, int days);

    private:
        Preferences();         // only one instance allowed
        static int              startOfDayCheck(const QTime&);
        static void             setNotify(const QString& messageID, bool notify);
        static bool             notifying(const QString& messageID);

        static Preferences*     mInstance;
        static bool             mUsingDefaults;
        static KTimeZone        mSystemTimeZone;
        static KHolidays::HolidayRegion* mHolidays;
        static QString          mPreviousVersion;  // last KAlarm version which wrote the config file
        static Backend          mPreviousBackend;  // backend used by last used version of KAlarm

        // All the following members are accessed by the Preferences dialog classes
        static int              mMessageButtonDelay;  // 0 = scatter; -1 = no delay, no scatter; >0 = delay, no scatter

        // Change tracking
        static bool             mAutoStartChangedByUser; // AutoStart has been changed by the user
};

#endif // PREFERENCES_H

// vim: et sw=4:
