/*
 *  preferences.h  -  program preference settings
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "kalarmconfig.h"
#include "kalarmcalendar/kadatetime.h"

#include <QObject>
#include <QDateTime>
#include <QTimeZone>

using namespace KAlarmCal;

namespace KHolidays { class HolidayRegion; }


// Settings configured in the Preferences dialog
class Preferences : public PreferencesBase
{
    Q_OBJECT
public:
    enum MailFrom   { MAIL_FROM_KMAIL, MAIL_FROM_SYS_SETTINGS, MAIL_FROM_ADDR };

    static Preferences*     self();
    static void             connect(const char* signal, const QObject* receiver, const char* member);
    template<class Signal, class Receiver, class Func,
             class = typename std::enable_if<!std::is_convertible<Signal, const char*>::value>::type,
             class = typename std::enable_if<!std::is_convertible<Func, const char*>::value>::type>
    static void             connect(Signal signal, const Receiver* receiver, Func member)
    {
        QObject::connect(self(), signal, receiver, member);
    }
    static int              autoHideSystemTray();
    static void             setAutoHideSystemTray(int timeout);
    static bool             autoStartChangedByUser()         { return mAutoStartChangedByUser; }
    static void             setAutoStartChangedByUser(bool c){ mAutoStartChangedByUser = c; }

    // Access to settings
    static QString          previousVersion()                { return mPreviousVersion; }
    static Backend          previousBackend()                { return mPreviousBackend; }
    static void             setAskAutoStart(bool yes);
    static bool             noAutoStart()                    { return self()->base_NoAutoStart(); }
    static void             setNoAutoStart(bool yes);
    static bool             modalMessages();
    static void             setModalMessages(bool yes);
    static KADateTime::Spec timeSpec();
    static QTimeZone        timeSpecAsZone();
    static void             setTimeSpec(const KADateTime::Spec&);
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
    static bool             quitWarn()                       { return mUsingDefaults ? self()->base_QuitWarn() : notifying(QUIT_WARN); }
    static void             setQuitWarn(bool yes)            { setNotify(QUIT_WARN, yes); }
    static bool             confirmAlarmDeletion()           { return mUsingDefaults ? self()->base_ConfirmAlarmDeletion() : notifying(CONFIRM_ALARM_DELETION); }
    static void             setConfirmAlarmDeletion(bool yes){ setNotify(CONFIRM_ALARM_DELETION, yes); }
    static bool             emailCopyToKMail()               { return self()->mBase_EmailCopyToKMail  &&  self()->mEmailClient == sendmail; }
    static void             setEmailCopyToKMail(bool yes)    { self()->setBase_EmailCopyToKMail(yes); }
    static bool             emailQueuedNotify()              { return mUsingDefaults ? self()->base_EmailQueuedNotify() : notifying(EMAIL_QUEUED_NOTIFY); }
    static void             setEmailQueuedNotify(bool yes)   { setNotify(EMAIL_QUEUED_NOTIFY, yes); }
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
    static const QLatin1String QUIT_WARN;
    static const QLatin1String ASK_AUTO_START;
    static const QLatin1String CONFIRM_ALARM_DELETION;
    static const QLatin1String EMAIL_QUEUED_NOTIFY;

    bool                    useDefaults(bool def) override   { mUsingDefaults = def;  return PreferencesBase::useDefaults(def); }

Q_SIGNALS:
    void                    timeZoneChanged(const KADateTime::Spec& newTz);
    void                    holidaysChanged(const KHolidays::HolidayRegion& newHolidays);
    void                    startOfDayChanged(const QTime& newStartOfDay);
    void                    workTimeChanged(const QTime& startTime, const QTime& endTime, const QBitArray& workDays);

private Q_SLOTS:
    void                    timeZoneChange(const QString&);
    void                    holidaysChange(const QString& regionCode);
    void                    startDayChange(const QDateTime&);
    void                    workTimeChange(const QDateTime&, const QDateTime&, int days);

private:
    Preferences();         // only one instance allowed
    static void             setNotify(const QString& messageID, bool notify);
    static bool             notifying(const QString& messageID);

    static Preferences*     mInstance;
    static bool             mUsingDefaults;
    static KHolidays::HolidayRegion* mHolidays;
    static QString          mPreviousVersion;  // last KAlarm version which wrote the config file
    static Backend          mPreviousBackend;  // backend used by last used version of KAlarm

    // All the following members are accessed by the Preferences dialog classes
    static int              mMessageButtonDelay;  // 0 = scatter; -1 = no delay, no scatter; >0 = delay, no scatter

    // Change tracking
    static bool             mAutoStartChangedByUser; // AutoStart has been changed by the user
};

// vim: et sw=4:
