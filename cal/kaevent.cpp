/*
 *  kaevent.cpp  -  represents calendar events
 *  Program:  kalarm
 *  Copyright © 2001-2011 by David Jarvie <djarvie@kde.org>
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

#include "kaevent.h"

#ifndef USE_AKONADI
#include "alarmresource.h"
#endif
#include "alarmtext.h"
#include "identities.h"
#include "version.h"

#ifdef USE_AKONADI
#include <kcalcore/memorycalendar.h>
#else
#include <kcal/calendarlocal.h>
#endif
#include <kholidays/holidays.h>
using namespace KHolidays;

#include <ksystemtimezone.h>
#include <klocale.h>
#ifndef USE_AKONADI
#include <kconfiggroup.h>
#endif
#include <kdebug.h>

#ifdef USE_AKONADI
using namespace KCalCore;
#else
using namespace KCal;
#endif
using namespace KHolidays;


#ifdef USE_AKONADI
typedef KCalCore::Person  EmailAddress;
class EmailAddressList : public KCalCore::Person::List
#else
typedef KCal::Person  EmailAddress;
class EmailAddressList : public QList<KCal::Person>
#endif
{
    public:
#ifdef USE_AKONADI
        EmailAddressList() : KCalCore::Person::List() { }
        EmailAddressList(const KCalCore::Person::List& list)  { operator=(list); }
        EmailAddressList& operator=(const KCalCore::Person::List&);
#else
        EmailAddressList() : QList<KCal::Person>() { }
        EmailAddressList(const QList<KCal::Person>& list)  { operator=(list); }
        EmailAddressList& operator=(const QList<KCal::Person>&);
#endif
        operator QStringList() const;
        QString     join(const QString& separator) const;
        QStringList pureAddresses() const;
        QString     pureAddresses(const QString& separator) const;
    private:
        QString     address(int index) const;
};


class KAAlarm::Private
{
    public:
        Private();

        Action             mActionType;       // alarm action type
        Type               mType;             // alarm type
        DateTime           mNextMainDateTime; // next time to display the alarm, excluding repetitions
        Repetition         mRepetition;       // sub-repetition count and interval
        int                mNextRepeat;       // repetition count of next due sub-repetition
        bool               mRepeatAtLogin;    // whether to repeat the alarm at every login
        bool               mRecurs;           // there is a recurrence rule for the alarm
        bool               mDeferred;         // whether the alarm is an extra deferred/deferred-reminder alarm
        bool               mTimedDeferral;    // if mDeferred = true: true if the deferral is timed, false if date-only
};


class KAEvent::Private : public QSharedData
{
    public:
        // Read-only internal flags additional to KAEvent::Flags enum values.
        // NOTE: If any values are added to those in KAEvent::Flags, ensure
        //       that these values don't overlap them.
        enum
        {
            REMINDER        = 0x100000,
            DEFERRAL        = 0x200000,
            TIMED_FLAG      = 0x400000,
            DATE_DEFERRAL   = DEFERRAL,
            TIME_DEFERRAL   = DEFERRAL | TIMED_FLAG,
            DISPLAYING_     = 0x800000,
            READ_ONLY_FLAGS = 0xF00000  //!< mask for all read-only internal values
        };
        enum ReminderType   // current active state of reminder
        {
            NO_REMINDER,       // reminder is not due
            ACTIVE_REMINDER,   // reminder is due
            HIDDEN_REMINDER    // reminder-after is disabled due to main alarm being deferred past it
        };
        enum DeferType
        {
            NO_DEFERRAL = 0,   // there is no deferred alarm
            NORMAL_DEFERRAL,   // the main alarm, a recurrence or a repeat is deferred
            REMINDER_DEFERRAL  // a reminder alarm is deferred
        };
        // Alarm types.
        // This uses the same scheme as KAAlarm::Type, with some extra values.
        // Note that the actual enum values need not be the same as KAAlarm::Type.
        enum AlarmType
        {
            INVALID_ALARM       = 0,     // Not an alarm
            MAIN_ALARM          = 1,     // THE real alarm. Must be the first in the enumeration.
            REMINDER_ALARM      = 0x02,  // Reminder in advance of/after the main alarm
            DEFERRED_ALARM      = 0x04,  // Deferred alarm
            DEFERRED_REMINDER_ALARM = REMINDER_ALARM | DEFERRED_ALARM,  // Deferred reminder alarm
            // The following values must be greater than the preceding ones, to
            // ensure that in ordered processing they are processed afterwards.
            AT_LOGIN_ALARM      = 0x10,  // Additional repeat-at-login trigger
            DISPLAYING_ALARM    = 0x20,  // Copy of the alarm currently being displayed
            // The following values are for internal KAEvent use only
            AUDIO_ALARM         = 0x30,  // sound to play when displaying the alarm
            PRE_ACTION_ALARM    = 0x40,  // command to execute before displaying the alarm
            POST_ACTION_ALARM   = 0x50   // command to execute after the alarm window is closed
        };

        struct AlarmData
        {
#ifdef USE_AKONADI
            ConstAlarmPtr               alarm;
#else
            const Alarm*                alarm;
#endif
            QString                     cleanText;       // text or audio file name
            uint                        emailFromId;
            QFont                       font;
            QColor                      bgColour, fgColour;
            float                       soundVolume;
            float                       fadeVolume;
            int                         fadeSeconds;
            int                         nextRepeat;
            bool                        speak;
            KAEvent::Private::AlarmType type;
            KAAlarm::Action             action;
            int                         displayingFlags;
            bool                        defaultFont;
            bool                        isEmailText;
            bool                        commandScript;
            bool                        cancelOnPreActErr;
            bool                        dontShowPreActErr;
            bool                        repeatSound;
            bool                        timedDeferral;
            bool                        hiddenReminder;
        };
        typedef QMap<AlarmType, AlarmData> AlarmMap;

        Private();
        Private(const KDateTime&, const QString& message, const QColor& bg, const QColor& fg,
                const QFont& f, SubAction, int lateCancel, Flags flags, bool changesPending = false);
#ifdef USE_AKONADI
        explicit Private(const KCalCore::ConstEventPtr&);
#else
        explicit Private(const KCal::Event*);
#endif
        Private(const Private&);
        ~Private()         { delete mRecurrence; }
        Private&           operator=(const Private& e)       { if (&e != this) copy(e);  return *this; }
#ifdef USE_AKONADI
        void               set(const KCalCore::ConstEventPtr&);
#else
        void               set(const KCal::Event*);
#endif
        void               set(const KDateTime&, const QString& message, const QColor& bg, const QColor& fg,
                               const QFont&, SubAction, int lateCancel, Flags flags, bool changesPending = false);
        void               setAudioFile(const QString& filename, float volume, float fadeVolume, int fadeSeconds, bool allowEmptyFile);
        OccurType          setNextOccurrence(const KDateTime& preDateTime);
        void               setFirstRecurrence();
        void               setCategory(KAlarm::CalEvent::Type);
        void               setRepeatAtLogin(bool);
        void               setRepeatAtLoginTrue(bool clearReminder);
        void               setReminder(int minutes, bool onceOnly);
        void               activateReminderAfter(const DateTime& mainAlarmTime);
        void               defer(const DateTime&, bool reminder, bool adjustRecurrence = false);
        void               cancelDefer();
#ifdef USE_AKONADI
        bool               setDisplaying(const Private&, KAAlarm::Type, Akonadi::Collection::Id, const KDateTime& dt, bool showEdit, bool showDefer);
        void               reinstateFromDisplaying(const KCalCore::ConstEventPtr&, Akonadi::Collection::Id&, bool& showEdit, bool& showDefer);
#else
        bool               setDisplaying(const Private&, KAAlarm::Type, const QString& resourceID, const KDateTime& dt, bool showEdit, bool showDefer);
        void               reinstateFromDisplaying(const KCal::Event*, QString& resourceID, bool& showEdit, bool& showDefer);
        void               setCommandError(const QString& configString);
        void               setCommandError(CmdErrType, bool writeConfig) const;
#endif
        void               startChanges()                 { ++mChangeCount; }
        void               endChanges();
        void               removeExpiredAlarm(KAAlarm::Type);
        KAAlarm            alarm(KAAlarm::Type) const;
        KAAlarm            firstAlarm() const;
        KAAlarm            nextAlarm(KAAlarm::Type) const;
#ifdef USE_AKONADI
        bool               updateKCalEvent(const KCalCore::Event::Ptr&, UidAction, bool setCustomProperties = true) const;
#else
        bool               updateKCalEvent(KCal::Event*, UidAction) const;
#endif
        DateTime           mainDateTime(bool withRepeats = false) const
                                   { return (withRepeats && mNextRepeat && mRepetition)
                                            ? mRepetition.duration(mNextRepeat).end(mNextMainDateTime.kDateTime()) : mNextMainDateTime; }
        DateTime           mainEndRepeatTime() const
                                   { return mRepetition ? mRepetition.duration().end(mNextMainDateTime.kDateTime()) : mNextMainDateTime; }
        DateTime           deferralLimit(DeferLimitType* = 0) const;
        Flags              flags() const;
        bool               isWorkingTime(const KDateTime&) const;
        bool               setRepetition(const Repetition&);
        bool               occursAfter(const KDateTime& preDateTime, bool includeRepetitions) const;
        OccurType          nextOccurrence(const KDateTime& preDateTime, DateTime& result, OccurOption = IGNORE_REPETITION) const;
        OccurType          previousOccurrence(const KDateTime& afterDateTime, DateTime& result, bool includeRepetitions = false) const;
        void               setRecurrence(const KARecurrence&);
#ifdef USE_AKONADI
        bool               setRecur(KCalCore::RecurrenceRule::PeriodType, int freq, int count, const QDate& end, KARecurrence::Feb29Type = KARecurrence::Feb29_None);
        bool               setRecur(KCalCore::RecurrenceRule::PeriodType, int freq, int count, const KDateTime& end, KARecurrence::Feb29Type = KARecurrence::Feb29_None);
#else
        bool               setRecur(KCal::RecurrenceRule::PeriodType, int freq, int count, const QDate& end, KARecurrence::Feb29Type = KARecurrence::Feb29_None);
        bool               setRecur(KCal::RecurrenceRule::PeriodType, int freq, int count, const KDateTime& end, KARecurrence::Feb29Type = KARecurrence::Feb29_None);
#endif
        KARecurrence::Type checkRecur() const;
        void               clearRecur();
        void               calcTriggerTimes() const;
#ifdef KDE_NO_DEBUG_OUTPUT
        void               dumpDebug() const  { }
#else
        void               dumpDebug() const;
#endif
#ifdef USE_AKONADI
        static bool        convertRepetition(const KCalCore::Event::Ptr&);
        static bool        convertStartOfDay(const KCalCore::Event::Ptr&);
        static DateTime    readDateTime(const KCalCore::ConstEventPtr&, bool dateOnly, DateTime& start);
        static void        readAlarms(const KCalCore::ConstEventPtr&, void* alarmMap, bool cmdDisplay = false);
        static void        readAlarm(const KCalCore::ConstAlarmPtr&, AlarmData&, bool audioMain, bool cmdDisplay = false);
#else
        static bool        convertRepetition(KCal::Event*);
        static bool        convertStartOfDay(KCal::Event*);
        static DateTime    readDateTime(const KCal::Event*, bool dateOnly, DateTime& start);
        static void        readAlarms(const KCal::Event*, void* alarmMap, bool cmdDisplay = false);
        static void        readAlarm(const KCal::Alarm*, AlarmData&, bool audioMain, bool cmdDisplay = false);
#endif

    private:
        void               copy(const Private&);
        bool               mayOccurDailyDuringWork(const KDateTime&) const;
        int                nextWorkRepetition(const KDateTime& pre) const;
        void               calcNextWorkingTime(const DateTime& nextTrigger) const;
        DateTime           nextWorkingTime() const;
        OccurType          nextRecurrence(const KDateTime& preDateTime, DateTime& result) const;
#ifdef USE_AKONADI
        void               setAudioAlarm(const KCalCore::Alarm::Ptr&) const;
        KCalCore::Alarm::Ptr initKCalAlarm(const KCalCore::Event::Ptr&, const DateTime&, const QStringList& types, AlarmType = INVALID_ALARM) const;
        KCalCore::Alarm::Ptr initKCalAlarm(const KCalCore::Event::Ptr&, int startOffsetSecs, const QStringList& types, AlarmType = INVALID_ALARM) const;
#else
        void               setAudioAlarm(KCal::Alarm*) const;
        KCal::Alarm*       initKCalAlarm(KCal::Event*, const DateTime&, const QStringList& types, AlarmType = INVALID_ALARM) const;
        KCal::Alarm*       initKCalAlarm(KCal::Event*, int startOffsetSecs, const QStringList& types, AlarmType = INVALID_ALARM) const;
#endif
        inline void        set_deferral(DeferType);
        inline void        activate_reminder(bool activate);

    public:
#ifndef USE_AKONADI
        static QString     mCmdErrConfigGroup; // config file group for command error recording
#endif
        static QFont       mDefaultFont;       // default alarm message font
        static const KHolidays::HolidayRegion* mHolidays;  // holiday region to use
        static QBitArray   mWorkDays;          // working days of the week
        static QTime       mWorkDayStart;      // start time of the working day
        static QTime       mWorkDayEnd;        // end time of the working day
        static int         mWorkTimeIndex;     // incremented every time working days/times are changed
#ifndef USE_AKONADI
        AlarmResource*     mResource;          // resource which owns the event (for convenience - not used by this class)
#endif
        mutable DateTime   mAllTrigger;        // next trigger time, including reminders, ignoring working hours
        mutable DateTime   mMainTrigger;       // next trigger time, ignoring reminders and working hours
        mutable DateTime   mAllWorkTrigger;    // next trigger time, taking account of reminders and working hours
        mutable DateTime   mMainWorkTrigger;   // next trigger time, ignoring reminders but taking account of working hours
        mutable CmdErrType mCommandError;      // command execution error last time the alarm triggered

        QString            mEventID;           // UID: KCal::Event unique ID
        QString            mTemplateName;      // alarm template's name, or null if normal event
#ifdef USE_AKONADI
        QMap<QByteArray, QString> mCustomProperties;  // KCal::Event's non-KAlarm custom properties
        Akonadi::Item::Id  mItemId;            // Akonadi::Item ID for this event
        Akonadi::Collection::Id mOriginalCollectionId; // saved collection ID (not the collection the event is in)
#else
        QString            mOriginalResourceId;// saved resource ID (not the resource the event is in)
#endif
        QString            mText;              // message text, file URL, command, email body [or audio file for KAAlarm]
        QString            mAudioFile;         // ATTACH: audio file to play
        QString            mPreAction;         // command to execute before alarm is displayed
        QString            mPostAction;        // command to execute after alarm window is closed
        DateTime           mStartDateTime;     // DTSTART and DTEND: start and end time for event
        KDateTime          mCreatedDateTime;   // CREATED: date event was created, or saved in archive calendar
        DateTime           mNextMainDateTime;  // next time to display the alarm, excluding repetitions
        KDateTime          mAtLoginDateTime;   // repeat-at-login end time
        DateTime           mDeferralTime;      // extra time to trigger alarm (if alarm or reminder deferred)
        DateTime           mDisplayingTime;    // date/time shown in the alarm currently being displayed
        int                mDisplayingFlags;   // type of alarm which is currently being displayed
        int                mReminderMinutes;   // how long in advance reminder is to be, or 0 if none (<0 for reminder AFTER the alarm)
        DateTime           mReminderAfterTime; // if mReminderActive true, time to trigger reminder AFTER the main alarm, or invalid if not pending
        ReminderType       mReminderActive;    // whether a reminder is due (before next, or after last, main alarm/recurrence)
        int                mDeferDefaultMinutes; // default number of minutes for deferral dialog, or 0 to select time control
        bool               mDeferDefaultDateOnly;// select date-only by default in deferral dialog
        int                mRevision;          // SEQUENCE: revision number of the original alarm, or 0
        KARecurrence*      mRecurrence;        // RECUR: recurrence specification, or 0 if none
        Repetition         mRepetition;        // sub-repetition count and interval
        int                mNextRepeat;        // repetition count of next due sub-repetition
        int                mAlarmCount;        // number of alarms: count of !mMainExpired, mRepeatAtLogin, mDeferral, mReminderActive, mDisplaying
        DeferType          mDeferral;          // whether the alarm is an extra deferred/deferred-reminder alarm
        unsigned long      mKMailSerialNumber; // if email text, message's KMail serial number
        int                mTemplateAfterTime; // time not specified: use n minutes after default time, or -1 (applies to templates only)
        QColor             mBgColour;          // background colour of alarm message
        QColor             mFgColour;          // foreground colour of alarm message, or invalid for default
        QFont              mFont;              // font of alarm message (ignored if mUseDefaultFont true)
        uint               mEmailFromIdentity; // standard email identity uoid for 'From' field, or empty
        EmailAddressList   mEmailAddresses;    // ATTENDEE: addresses to send email to
        QString            mEmailSubject;      // SUMMARY: subject line of email
        QStringList        mEmailAttachments;  // ATTACH: email attachment file names
        mutable int        mChangeCount;       // >0 = inhibit calling calcTriggerTimes()
        mutable bool       mTriggerChanged;    // true if need to recalculate trigger times
        QString            mLogFile;           // alarm output is to be logged to this URL
        float              mSoundVolume;       // volume for sound file (range 0 - 1), or < 0 for unspecified
        float              mFadeVolume;        // initial volume for sound file (range 0 - 1), or < 0 for no fade
        int                mFadeSeconds;       // fade time for sound file, or 0 if none
        int                mLateCancel;        // how many minutes late will cancel the alarm, or 0 for no cancellation
        mutable const KHolidays::HolidayRegion*
                           mExcludeHolidays;   // non-null to not trigger alarms on holidays (= mHolidays when trigger calculated)
        mutable int        mWorkTimeOnly;      // non-zero to trigger alarm only during working hours (= mWorkTimeIndex when trigger calculated)
        SubAction          mActionSubType;     // sub-action type for the event's main alarm
        KAlarm::CalEvent::Type mCategory;      // event category (active, archived, template, ...)
#ifdef USE_AKONADI
        KAlarm::Calendar::Compat mCompatibility; // event's storage format compatibility
        bool               mReadOnly;          // event is read-only in its original calendar file
#endif
        bool               mCancelOnPreActErr; // cancel alarm if pre-alarm action fails
        bool               mDontShowPreActErr; // don't notify error if pre-alarm action fails
        bool               mConfirmAck;        // alarm acknowledgement requires confirmation by user
        bool               mUseDefaultFont;    // use default message font, not mFont
        bool               mCommandScript;     // the command text is a script, not a shell command line
        bool               mCommandXterm;      // command alarm is to be executed in a terminal window
        bool               mCommandDisplay;    // command output is to be displayed in an alarm window
        bool               mEmailBcc;          // blind copy the email to the user
        bool               mBeep;              // whether to beep when the alarm is displayed
        bool               mRepeatSound;       // whether to repeat the sound file while the alarm is displayed
        bool               mSpeak;             // whether to speak the message when the alarm is displayed
        bool               mCopyToKOrganizer;  // KOrganizer should hold a copy of the event
        bool               mReminderOnceOnly;  // the reminder is output only for the first recurrence
        bool               mAutoClose;         // whether to close the alarm window after the late-cancel period
        bool               mMainExpired;       // main alarm has expired (in which case a deferral alarm will exist)
        bool               mRepeatAtLogin;     // whether to repeat the alarm at every login
        bool               mArchiveRepeatAtLogin; // if now archived, original event was repeat-at-login
        bool               mArchive;           // event has triggered in the past, so archive it when closed
        bool               mDisplaying;        // whether the alarm is currently being displayed (i.e. in displaying calendar)
        bool               mDisplayingDefer;   // show Defer button (applies to displaying calendar only)
        bool               mDisplayingEdit;    // show Edit button (applies to displaying calendar only)
        bool               mEnabled;           // false if event is disabled

    public:
        static const QByteArray FLAGS_PROPERTY;
        static const QString DATE_ONLY_FLAG;
        static const QString EMAIL_BCC_FLAG;
        static const QString CONFIRM_ACK_FLAG;
        static const QString KORGANIZER_FLAG;
        static const QString EXCLUDE_HOLIDAYS_FLAG;
        static const QString WORK_TIME_ONLY_FLAG;
        static const QString REMINDER_ONCE_FLAG;
        static const QString DEFER_FLAG;
        static const QString LATE_CANCEL_FLAG;
        static const QString AUTO_CLOSE_FLAG;
        static const QString TEMPL_AFTER_TIME_FLAG;
        static const QString KMAIL_SERNUM_FLAG;
        static const QString ARCHIVE_FLAG;
        static const QByteArray NEXT_RECUR_PROPERTY;
        static const QByteArray REPEAT_PROPERTY;
        static const QByteArray LOG_PROPERTY;
        static const QString xtermURL;
        static const QString displayURL;
        static const QByteArray TYPE_PROPERTY;
        static const QString FILE_TYPE;
        static const QString AT_LOGIN_TYPE;
        static const QString REMINDER_TYPE;
        static const QString REMINDER_ONCE_TYPE;
        static const QString TIME_DEFERRAL_TYPE;
        static const QString DATE_DEFERRAL_TYPE;
        static const QString DISPLAYING_TYPE;
        static const QString PRE_ACTION_TYPE;
        static const QString POST_ACTION_TYPE;
        static const QString SOUND_REPEAT_TYPE;
        static const QByteArray NEXT_REPEAT_PROPERTY;
        static const QString HIDDEN_REMINDER_FLAG;
        static const QByteArray FONT_COLOUR_PROPERTY;
        static const QByteArray VOLUME_PROPERTY;
        static const QString EMAIL_ID_FLAG;
        static const QString SPEAK_FLAG;
        static const QString CANCEL_ON_ERROR_FLAG;
        static const QString DONT_SHOW_ERROR_FLAG;
        static const QString DISABLED_STATUS;
        static const QString DISP_DEFER;
        static const QString DISP_EDIT;
        static const QString CMD_ERROR_VALUE;
        static const QString CMD_ERROR_PRE_VALUE;
        static const QString CMD_ERROR_POST_VALUE;
        static const QString SC;
};


// KAlarm version which first used the current calendar/event format.
// If this changes, KAEvent::convertKCalEvents() must be changed correspondingly.
// The string version is the KAlarm version string used in the calendar file.
QByteArray KAEvent::currentCalendarVersionString()  { return QByteArray("2.7.0"); }
int        KAEvent::currentCalendarVersion()        { return KAlarm::Version(2,7,0); }

// Custom calendar properties.
// Note that all custom property names are prefixed with X-KDE-KALARM- in the calendar file.

// Event properties
const QByteArray KAEvent::Private::FLAGS_PROPERTY("FLAGS");              // X-KDE-KALARM-FLAGS property
const QString    KAEvent::Private::DATE_ONLY_FLAG        = QLatin1String("DATE");
const QString    KAEvent::Private::EMAIL_BCC_FLAG        = QLatin1String("BCC");
const QString    KAEvent::Private::CONFIRM_ACK_FLAG      = QLatin1String("ACKCONF");
const QString    KAEvent::Private::KORGANIZER_FLAG       = QLatin1String("KORG");
const QString    KAEvent::Private::EXCLUDE_HOLIDAYS_FLAG = QLatin1String("EXHOLIDAYS");
const QString    KAEvent::Private::WORK_TIME_ONLY_FLAG   = QLatin1String("WORKTIME");
const QString    KAEvent::Private::REMINDER_ONCE_FLAG    = QLatin1String("ONCE");
const QString    KAEvent::Private::DEFER_FLAG            = QLatin1String("DEFER");   // default defer interval for this alarm
const QString    KAEvent::Private::LATE_CANCEL_FLAG      = QLatin1String("LATECANCEL");
const QString    KAEvent::Private::AUTO_CLOSE_FLAG       = QLatin1String("LATECLOSE");
const QString    KAEvent::Private::TEMPL_AFTER_TIME_FLAG = QLatin1String("TMPLAFTTIME");
const QString    KAEvent::Private::KMAIL_SERNUM_FLAG     = QLatin1String("KMAIL");
const QString    KAEvent::Private::ARCHIVE_FLAG          = QLatin1String("ARCHIVE");

const QByteArray KAEvent::Private::NEXT_RECUR_PROPERTY("NEXTRECUR");     // X-KDE-KALARM-NEXTRECUR property
const QByteArray KAEvent::Private::REPEAT_PROPERTY("REPEAT");            // X-KDE-KALARM-REPEAT property
const QByteArray KAEvent::Private::LOG_PROPERTY("LOG");                  // X-KDE-KALARM-LOG property
const QString    KAEvent::Private::xtermURL = QLatin1String("xterm:");
const QString    KAEvent::Private::displayURL = QLatin1String("display:");

// - General alarm properties
const QByteArray KAEvent::Private::TYPE_PROPERTY("TYPE");                // X-KDE-KALARM-TYPE property
const QString    KAEvent::Private::FILE_TYPE                  = QLatin1String("FILE");
const QString    KAEvent::Private::AT_LOGIN_TYPE              = QLatin1String("LOGIN");
const QString    KAEvent::Private::REMINDER_TYPE              = QLatin1String("REMINDER");
const QString    KAEvent::Private::TIME_DEFERRAL_TYPE         = QLatin1String("DEFERRAL");
const QString    KAEvent::Private::DATE_DEFERRAL_TYPE         = QLatin1String("DATE_DEFERRAL");
const QString    KAEvent::Private::DISPLAYING_TYPE            = QLatin1String("DISPLAYING");   // used only in displaying calendar
const QString    KAEvent::Private::PRE_ACTION_TYPE            = QLatin1String("PRE");
const QString    KAEvent::Private::POST_ACTION_TYPE           = QLatin1String("POST");
const QString    KAEvent::Private::SOUND_REPEAT_TYPE          = QLatin1String("SOUNDREPEAT");
const QByteArray KAEvent::Private::NEXT_REPEAT_PROPERTY("NEXTREPEAT");   // X-KDE-KALARM-NEXTREPEAT property
const QString    KAEvent::Private::HIDDEN_REMINDER_FLAG = QLatin1String("HIDE");
// - Display alarm properties
const QByteArray KAEvent::Private::FONT_COLOUR_PROPERTY("FONTCOLOR");    // X-KDE-KALARM-FONTCOLOR property
// - Email alarm properties
const QString    KAEvent::Private::EMAIL_ID_FLAG        = QLatin1String("EMAILID");
// - Audio alarm properties
const QByteArray KAEvent::Private::VOLUME_PROPERTY("VOLUME");            // X-KDE-KALARM-VOLUME property
const QString    KAEvent::Private::SPEAK_FLAG           = QLatin1String("SPEAK");
// - Command alarm properties
const QString    KAEvent::Private::CANCEL_ON_ERROR_FLAG = QLatin1String("ERRCANCEL");
const QString    KAEvent::Private::DONT_SHOW_ERROR_FLAG = QLatin1String("ERRNOSHOW");

// Event status strings
const QString    KAEvent::Private::DISABLED_STATUS            = QLatin1String("DISABLED");

// Displaying event ID identifier
const QString    KAEvent::Private::DISP_DEFER = QLatin1String("DEFER");
const QString    KAEvent::Private::DISP_EDIT  = QLatin1String("EDIT");

// Command error strings
#ifndef USE_AKONADI
QString          KAEvent::Private::mCmdErrConfigGroup = QLatin1String("CommandErrors");
#endif
const QString    KAEvent::Private::CMD_ERROR_VALUE      = QLatin1String("MAIN");
const QString    KAEvent::Private::CMD_ERROR_PRE_VALUE  = QLatin1String("PRE");
const QString    KAEvent::Private::CMD_ERROR_POST_VALUE = QLatin1String("POST");

const QString    KAEvent::Private::SC = QLatin1String(";");

QFont                           KAEvent::Private::mDefaultFont;
const KHolidays::HolidayRegion* KAEvent::Private::mHolidays = 0;
QBitArray                       KAEvent::Private::mWorkDays(7);
QTime                           KAEvent::Private::mWorkDayStart(9, 0, 0);
QTime                           KAEvent::Private::mWorkDayEnd(17, 0, 0);
int                             KAEvent::Private::mWorkTimeIndex = 1;


#ifdef USE_AKONADI
static void setProcedureAlarm(const Alarm::Ptr&, const QString& commandLine);
#else
static void setProcedureAlarm(Alarm*, const QString& commandLine);
#endif
static QString reminderToString(int minutes);


/*=============================================================================
= Class KAEvent
= Corresponds to a KCal::Event instance.
=============================================================================*/

inline void KAEvent::Private::set_deferral(DeferType type)
{
    if (type)
    {
        if (mDeferral == NO_DEFERRAL)
            ++mAlarmCount;
    }
    else
    {
        if (mDeferral != NO_DEFERRAL)
            --mAlarmCount;
    }
    mDeferral = type;
}

inline void KAEvent::Private::activate_reminder(bool activate)
{
    if (activate  &&  mReminderActive != ACTIVE_REMINDER  &&  mReminderMinutes)
    {
        if (mReminderActive == NO_REMINDER)
            ++mAlarmCount;
        mReminderActive = ACTIVE_REMINDER;
    }
    else if (!activate  &&  mReminderActive != NO_REMINDER)
    {
        mReminderActive = NO_REMINDER;
        mReminderAfterTime = DateTime();
        --mAlarmCount;
    }
}

KAEvent::KAEvent()
    : d(new Private)
{ }

KAEvent::Private::Private()
    :
#ifndef USE_AKONADI
      mResource(0),
#endif
      mCommandError(CMD_NO_ERROR),
#ifdef USE_AKONADI
      mItemId(-1),
#endif
      mReminderMinutes(0),
      mReminderActive(NO_REMINDER),
      mRevision(0),
      mRecurrence(0),
      mNextRepeat(0),
      mAlarmCount(0),
      mDeferral(NO_DEFERRAL),
      mChangeCount(0),
      mTriggerChanged(false),
      mLateCancel(0),
      mExcludeHolidays(0),
      mWorkTimeOnly(0),
      mCategory(KAlarm::CalEvent::EMPTY),
#ifdef USE_AKONADI
      mCompatibility(KAlarm::Calendar::Current),
      mReadOnly(false),
#endif
      mConfirmAck(false),
      mEmailBcc(false),
      mBeep(false),
      mAutoClose(false),
      mRepeatAtLogin(false),
      mDisplaying(false)
{ }

KAEvent::KAEvent(const KDateTime& dt, const QString& message, const QColor& bg, const QColor& fg, const QFont& f,
                 SubAction action, int lateCancel, Flags flags, bool changesPending)
    : d(new Private(dt, message, bg, fg, f, action, lateCancel, flags, changesPending))
{
}

KAEvent::Private::Private(const KDateTime& dt, const QString& message, const QColor& bg, const QColor& fg, const QFont& f,
                          SubAction action, int lateCancel, Flags flags, bool changesPending)
    : mRecurrence(0)
{
    set(dt, message, bg, fg, f, action, lateCancel, flags, changesPending);
}

#ifdef USE_AKONADI
KAEvent::KAEvent(const ConstEventPtr& e)
#else
KAEvent::KAEvent(const Event* e)
#endif
    : d(new Private(e))
{
}

#ifdef USE_AKONADI
KAEvent::Private::Private(const ConstEventPtr& e)
#else
KAEvent::Private::Private(const Event* e)
#endif
    : mRecurrence(0)
{
    set(e);
}

KAEvent::Private::Private(const KAEvent::Private& e)
    : QSharedData(e),
      mRecurrence(0)
{
    copy(e);
}

KAEvent::KAEvent(const KAEvent& other)
    : d(other.d)
{ }

KAEvent::~KAEvent()
{ }

KAEvent& KAEvent::operator=(const KAEvent& other)
{
    if (&other != this)
        d = other.d;
    return *this;
}

/******************************************************************************
* Copies the data from another instance.
*/
void KAEvent::Private::copy(const KAEvent::Private& event)
{
#ifndef USE_AKONADI
    mResource                = event.mResource;
#endif
    mAllTrigger              = event.mAllTrigger;
    mMainTrigger             = event.mMainTrigger;
    mAllWorkTrigger          = event.mAllWorkTrigger;
    mMainWorkTrigger         = event.mMainWorkTrigger;
    mCommandError            = event.mCommandError;
    mEventID                 = event.mEventID;
    mTemplateName            = event.mTemplateName;
#ifdef USE_AKONADI
    mCustomProperties        = event.mCustomProperties;
    mItemId                  = event.mItemId;
    mOriginalCollectionId    = event.mOriginalCollectionId;
#else
    mOriginalResourceId      = event.mOriginalResourceId;
#endif
    mText                    = event.mText;
    mAudioFile               = event.mAudioFile;
    mPreAction               = event.mPreAction;
    mPostAction              = event.mPostAction;
    mStartDateTime           = event.mStartDateTime;
    mCreatedDateTime         = event.mCreatedDateTime;
    mNextMainDateTime        = event.mNextMainDateTime;
    mAtLoginDateTime         = event.mAtLoginDateTime;
    mDeferralTime            = event.mDeferralTime;
    mDisplayingTime          = event.mDisplayingTime;
    mDisplayingFlags         = event.mDisplayingFlags;
    mReminderMinutes         = event.mReminderMinutes;
    mReminderAfterTime       = event.mReminderAfterTime;
    mReminderActive          = event.mReminderActive;
    mDeferDefaultMinutes     = event.mDeferDefaultMinutes;
    mDeferDefaultDateOnly    = event.mDeferDefaultDateOnly;
    mRevision                = event.mRevision;
    mRepetition              = event.mRepetition;
    mNextRepeat              = event.mNextRepeat;
    mAlarmCount              = event.mAlarmCount;
    mDeferral                = event.mDeferral;
    mKMailSerialNumber       = event.mKMailSerialNumber;
    mTemplateAfterTime       = event.mTemplateAfterTime;
    mBgColour                = event.mBgColour;
    mFgColour                = event.mFgColour;
    mFont                    = event.mFont;
    mEmailFromIdentity       = event.mEmailFromIdentity;
    mEmailAddresses          = event.mEmailAddresses;
    mEmailSubject            = event.mEmailSubject;
    mEmailAttachments        = event.mEmailAttachments;
    mLogFile                 = event.mLogFile;
    mSoundVolume             = event.mSoundVolume;
    mFadeVolume              = event.mFadeVolume;
    mFadeSeconds             = event.mFadeSeconds;
    mLateCancel              = event.mLateCancel;
    mExcludeHolidays         = event.mExcludeHolidays;
    mWorkTimeOnly            = event.mWorkTimeOnly;
    mActionSubType           = event.mActionSubType;
    mCategory                = event.mCategory;
#ifdef USE_AKONADI
    mCompatibility           = event.mCompatibility;
    mReadOnly                = event.mReadOnly;
#endif
    mCancelOnPreActErr       = event.mCancelOnPreActErr;
    mDontShowPreActErr       = event.mDontShowPreActErr;
    mConfirmAck              = event.mConfirmAck;
    mUseDefaultFont          = event.mUseDefaultFont;
    mCommandScript           = event.mCommandScript;
    mCommandXterm            = event.mCommandXterm;
    mCommandDisplay          = event.mCommandDisplay;
    mEmailBcc                = event.mEmailBcc;
    mBeep                    = event.mBeep;
    mRepeatSound             = event.mRepeatSound;
    mSpeak                   = event.mSpeak;
    mCopyToKOrganizer        = event.mCopyToKOrganizer;
    mReminderOnceOnly        = event.mReminderOnceOnly;
    mAutoClose               = event.mAutoClose;
    mMainExpired             = event.mMainExpired;
    mRepeatAtLogin           = event.mRepeatAtLogin;
    mArchiveRepeatAtLogin    = event.mArchiveRepeatAtLogin;
    mArchive                 = event.mArchive;
    mDisplaying              = event.mDisplaying;
    mDisplayingDefer         = event.mDisplayingDefer;
    mDisplayingEdit          = event.mDisplayingEdit;
    mEnabled                 = event.mEnabled;
    mChangeCount             = 0;
    mTriggerChanged          = event.mTriggerChanged;
    delete mRecurrence;
    if (event.mRecurrence)
        mRecurrence = new KARecurrence(*event.mRecurrence);
    else
        mRecurrence = 0;
}

#ifdef USE_AKONADI
void KAEvent::set(const ConstEventPtr& e)
#else
void KAEvent::set(const Event* e)
#endif
{
    d->set(e);
}

/******************************************************************************
* Initialise the KAEvent::Private from a KCal::Event.
*/
#ifdef USE_AKONADI
void KAEvent::Private::set(const ConstEventPtr& event)
#else
void KAEvent::Private::set(const Event* event)
#endif
{
    startChanges();
    // Extract status from the event
    mCommandError           = CMD_NO_ERROR;
#ifndef USE_AKONADI
    mResource               = 0;
#endif
    mEventID                = event->uid();
    mRevision               = event->revision();
    mTemplateName.clear();
    mLogFile.clear();
#ifdef USE_AKONADI
    mItemId                 = -1;
    mOriginalCollectionId   = -1;
#else
    mOriginalResourceId.clear();
#endif
    mTemplateAfterTime      = -1;
    mBeep                   = false;
    mSpeak                  = false;
    mEmailBcc               = false;
    mCommandXterm           = false;
    mCommandDisplay         = false;
    mCopyToKOrganizer       = false;
    mConfirmAck             = false;
    mArchive                = false;
    mReminderOnceOnly       = false;
    mAutoClose              = false;
    mArchiveRepeatAtLogin   = false;
    mDisplayingDefer        = false;
    mDisplayingEdit         = false;
    mDeferDefaultDateOnly   = false;
    mReminderActive         = NO_REMINDER;
    mReminderMinutes        = 0;
    mDeferDefaultMinutes    = 0;
    mLateCancel             = 0;
    mKMailSerialNumber      = 0;
    mExcludeHolidays        = 0;
    mWorkTimeOnly           = 0;
    mChangeCount            = 0;
    mBgColour               = QColor(255, 255, 255);    // missing/invalid colour - return white background
    mFgColour               = QColor(0, 0, 0);          // and black foreground
#ifdef USE_AKONADI
    mCompatibility          = KAlarm::Calendar::Current;
    mReadOnly               = event->isReadOnly();
#endif
    mUseDefaultFont         = true;
    mEnabled                = true;
    clearRecur();
    QString param;
    bool ok;
    mCategory               = KAlarm::CalEvent::status(event, &param);
    if (mCategory == KAlarm::CalEvent::DISPLAYING)
    {
        // It's a displaying calendar event - set values specific to displaying alarms
        const QStringList params = param.split(SC, QString::KeepEmptyParts);
        int n = params.count();
        if (n)
        {
#ifdef USE_AKONADI
            const qlonglong id = params[0].toLongLong(&ok);
            if (ok)
                mOriginalCollectionId = id;
#else
            mOriginalResourceId = params[0];
#endif
            for (int i = 1;  i < n;  ++i)
            {
                if (params[i] == DISP_DEFER)
                    mDisplayingDefer = true;
                if (params[i] == DISP_EDIT)
                    mDisplayingEdit = true;
            }
        }
    }
#ifdef USE_AKONADI
    // Store the non-KAlarm custom properties of the event
    const QByteArray kalarmKey = "X-KDE-" + KAlarm::Calendar::APPNAME + '-';
    mCustomProperties = event->customProperties();
    for (QMap<QByteArray, QString>::Iterator it = mCustomProperties.begin();  it != mCustomProperties.end(); )
    {
        if (it.key().startsWith(kalarmKey))
            it = mCustomProperties.erase(it);
        else
            ++it;
    }
#endif

    bool dateOnly = false;
    QStringList flags = event->customProperty(KAlarm::Calendar::APPNAME, FLAGS_PROPERTY).split(SC, QString::SkipEmptyParts);
    flags << QString() << QString();    // to avoid having to check for end of list
    for (int i = 0, end = flags.count() - 1;  i < end;  ++i)
    {
        if (flags[i] == DATE_ONLY_FLAG)
            dateOnly = true;
        else if (flags[i] == CONFIRM_ACK_FLAG)
            mConfirmAck = true;
        else if (flags[i] == EMAIL_BCC_FLAG)
            mEmailBcc = true;
        else if (flags[i] == KORGANIZER_FLAG)
            mCopyToKOrganizer = true;
        else if (flags[i] == EXCLUDE_HOLIDAYS_FLAG)
            mExcludeHolidays = mHolidays;
        else if (flags[i] == WORK_TIME_ONLY_FLAG)
            mWorkTimeOnly = 1;
        else if (flags[i]== KMAIL_SERNUM_FLAG)
        {
            const unsigned long n = flags[i + 1].toULong(&ok);
            if (!ok)
                continue;
            mKMailSerialNumber = n;
            ++i;
        }
        else if (flags[i] == Private::ARCHIVE_FLAG)
            mArchive = true;
        else if (flags[i] == Private::AT_LOGIN_TYPE)
            mArchiveRepeatAtLogin = true;
        else if (flags[i] == Private::REMINDER_TYPE)
        {
            if (flags[++i] == Private::REMINDER_ONCE_FLAG)
            {
                mReminderOnceOnly = true;
                ++i;
            }
            const int len = flags[i].length() - 1;
            mReminderMinutes = -flags[i].left(len).toInt();    // -> 0 if conversion fails
            switch (flags[i].at(len).toLatin1())
            {
                case 'M':  break;
                case 'H':  mReminderMinutes *= 60;  break;
                case 'D':  mReminderMinutes *= 1440;  break;
                default:   mReminderMinutes = 0;  break;
            }
        }
        else if (flags[i] == DEFER_FLAG)
        {
            QString mins = flags[i + 1];
            if (mins.endsWith('D'))
            {
                mDeferDefaultDateOnly = true;
                mins.truncate(mins.length() - 1);
            }
            const int n = static_cast<int>(mins.toUInt(&ok));
            if (!ok)
                continue;
            mDeferDefaultMinutes = n;
            ++i;
        }
        else if (flags[i] == TEMPL_AFTER_TIME_FLAG)
        {
            const int n = static_cast<int>(flags[i + 1].toUInt(&ok));
            if (!ok)
                continue;
            mTemplateAfterTime = n;
            ++i;
        }
        else if (flags[i] == LATE_CANCEL_FLAG)
        {
            mLateCancel = static_cast<int>(flags[i + 1].toUInt(&ok));
            if (ok)
                ++i;
            if (!ok  ||  !mLateCancel)
                mLateCancel = 1;    // invalid parameter defaults to 1 minute
        }
        else if (flags[i] == AUTO_CLOSE_FLAG)
        {
            mLateCancel = static_cast<int>(flags[i + 1].toUInt(&ok));
            if (ok)
                ++i;
            if (!ok  ||  !mLateCancel)
                mLateCancel = 1;    // invalid parameter defaults to 1 minute
            mAutoClose = true;
        }
    }

    QString prop = event->customProperty(KAlarm::Calendar::APPNAME, LOG_PROPERTY);
    if (!prop.isEmpty())
    {
        if (prop == xtermURL)
            mCommandXterm = true;
        else if (prop == displayURL)
            mCommandDisplay = true;
        else
            mLogFile = prop;
    }
    prop = event->customProperty(KAlarm::Calendar::APPNAME, REPEAT_PROPERTY);
    if (!prop.isEmpty())
    {
        // This property is used when the main alarm has expired
        const QStringList list = prop.split(QLatin1Char(':'));
        if (list.count() >= 2)
        {
            const int interval = static_cast<int>(list[0].toUInt());
            const int count = static_cast<int>(list[1].toUInt());
            if (interval && count)
            {
                if (interval % (24*60))
                    mRepetition.set(Duration(interval * 60, Duration::Seconds), count);
                else
                    mRepetition.set(Duration(interval / (24*60), Duration::Days), count);
            }
        }
    }
    mNextMainDateTime = readDateTime(event, dateOnly, mStartDateTime);
    mCreatedDateTime = event->created();
    if (dateOnly  &&  !mRepetition.isDaily())
        mRepetition.set(Duration(mRepetition.intervalDays(), Duration::Days));
    if (mCategory == KAlarm::CalEvent::TEMPLATE)
        mTemplateName = event->summary();
#ifdef USE_AKONADI
    if (event->customStatus() == DISABLED_STATUS)
#else
    if (event->statusStr() == DISABLED_STATUS)
#endif
        mEnabled = false;

    // Extract status from the event's alarms.
    // First set up defaults.
    mActionSubType     = MESSAGE;
    mMainExpired       = true;
    mRepeatAtLogin     = false;
    mDisplaying        = false;
    mRepeatSound       = false;
    mCommandScript     = false;
    mCancelOnPreActErr = false;
    mDontShowPreActErr = false;
    mDeferral          = NO_DEFERRAL;
    mSoundVolume       = -1;
    mFadeVolume        = -1;
    mFadeSeconds       = 0;
    mEmailFromIdentity = 0;
    mReminderAfterTime = DateTime();
    mText.clear();
    mAudioFile.clear();
    mPreAction.clear();
    mPostAction.clear();
    mEmailSubject.clear();
    mEmailAddresses.clear();
    mEmailAttachments.clear();

    // Extract data from all the event's alarms and index the alarms by sequence number
    AlarmMap alarmMap;
    readAlarms(event, &alarmMap, mCommandDisplay);

    // Incorporate the alarms' details into the overall event
    mAlarmCount = 0;       // initialise as invalid
    DateTime alTime;
    bool set = false;
    bool isEmailText = false;
    bool setDeferralTime = false;
    Duration deferralOffset;
    for (AlarmMap::ConstIterator it = alarmMap.constBegin();  it != alarmMap.constEnd();  ++it)
    {
        const AlarmData& data = it.value();
        const DateTime dateTime = data.alarm->hasStartOffset() ? data.alarm->startOffset().end(mNextMainDateTime.effectiveKDateTime()) : data.alarm->time();
        switch (data.type)
        {
            case MAIN_ALARM:
                mMainExpired = false;
                alTime = dateTime;
                alTime.setDateOnly(mStartDateTime.isDateOnly());
                if (data.alarm->repeatCount()  &&  data.alarm->snoozeTime())
                {
                    mRepetition.set(data.alarm->snoozeTime(), data.alarm->repeatCount());   // values may be adjusted in setRecurrence()
                    mNextRepeat = data.nextRepeat;
                }
                if (data.action != KAAlarm::AUDIO)
                    break;
                // Fall through to AUDIO_ALARM
            case AUDIO_ALARM:
                mAudioFile   = data.cleanText;
                mSpeak       = data.speak  &&  mAudioFile.isEmpty();
                mBeep        = !mSpeak  &&  mAudioFile.isEmpty();
                mSoundVolume = (!mBeep && !mSpeak) ? data.soundVolume : -1;
                mFadeVolume  = (mSoundVolume >= 0  &&  data.fadeSeconds > 0) ? data.fadeVolume : -1;
                mFadeSeconds = (mFadeVolume >= 0) ? data.fadeSeconds : 0;
                mRepeatSound = (!mBeep && !mSpeak)  &&  (data.alarm->repeatCount() < 0);
                break;
            case AT_LOGIN_ALARM:
                mRepeatAtLogin   = true;
                mAtLoginDateTime = dateTime.kDateTime();
                alTime = mAtLoginDateTime;
                break;
            case REMINDER_ALARM:
                // N.B. there can be a start offset but no valid date/time (e.g. in template)
                if (data.alarm->startOffset().asSeconds() / 60)
                {
                    mReminderActive = ACTIVE_REMINDER;
                    if (mReminderMinutes < 0)
                    {
                        mReminderAfterTime = dateTime;   // the reminder is AFTER the main alarm
                        mReminderAfterTime.setDateOnly(dateOnly);
                        if (data.hiddenReminder)
                            mReminderActive = HIDDEN_REMINDER;
                    }
                }
                break;
            case DEFERRED_REMINDER_ALARM:
            case DEFERRED_ALARM:
                mDeferral = (data.type == DEFERRED_REMINDER_ALARM) ? REMINDER_DEFERRAL : NORMAL_DEFERRAL;
                mDeferralTime = dateTime;
                if (!data.timedDeferral)
                    mDeferralTime.setDateOnly(true);
                if (data.alarm->hasStartOffset())
                    deferralOffset = data.alarm->startOffset();
                break;
            case DISPLAYING_ALARM:
            {
                mDisplaying      = true;
                mDisplayingFlags = data.displayingFlags;
                const bool dateOnly = (mDisplayingFlags & DEFERRAL) ? !(mDisplayingFlags & TIMED_FLAG)
                                      : mStartDateTime.isDateOnly();
                mDisplayingTime = dateTime;
                mDisplayingTime.setDateOnly(dateOnly);
                alTime = mDisplayingTime;
                break;
            }
            case PRE_ACTION_ALARM:
                mPreAction         = data.cleanText;
                mCancelOnPreActErr = data.cancelOnPreActErr;
                mDontShowPreActErr = data.dontShowPreActErr;
                break;
            case POST_ACTION_ALARM:
                mPostAction = data.cleanText;
                break;
            case INVALID_ALARM:
            default:
                break;
        }

        bool noSetNextTime = false;
        switch (data.type)
        {
            case DEFERRED_REMINDER_ALARM:
            case DEFERRED_ALARM:
                if (!set)
                {
                    // The recurrence has to be evaluated before we can
                    // calculate the time of a deferral alarm.
                    setDeferralTime = true;
                    noSetNextTime = true;
                }
                // fall through to REMINDER_ALARM
            case REMINDER_ALARM:
            case AT_LOGIN_ALARM:
            case DISPLAYING_ALARM:
                if (!set  &&  !noSetNextTime)
                    mNextMainDateTime = alTime;
                // fall through to MAIN_ALARM
            case MAIN_ALARM:
                // Ensure that the basic fields are set up even if there is no main
                // alarm in the event (if it has expired and then been deferred)
                if (!set)
                {
                    mActionSubType = (KAEvent::SubAction)data.action;
                    mText = (mActionSubType == COMMAND) ? data.cleanText.trimmed() : data.cleanText;
                    switch (data.action)
                    {
                        case KAAlarm::COMMAND:
                            mCommandScript = data.commandScript;
                            if (!mCommandDisplay)
                                break;
                            // fall through to MESSAGE
                        case KAAlarm::MESSAGE:
                            mFont           = data.font;
                            mUseDefaultFont = data.defaultFont;
                            if (data.isEmailText)
                                isEmailText = true;
                            // fall through to FILE
                        case KAAlarm::FILE:
                            mBgColour = data.bgColour;
                            mFgColour = data.fgColour;
                            break;
                        case KAAlarm::EMAIL:
                            mEmailFromIdentity = data.emailFromId;
                            mEmailAddresses    = data.alarm->mailAddresses();
                            mEmailSubject      = data.alarm->mailSubject();
                            mEmailAttachments  = data.alarm->mailAttachments();
                            break;
                        case KAAlarm::AUDIO:
                            // Already mostly handled above
                            mRepeatSound = data.repeatSound;
                            break;
                        default:
                            break;
                    }
                    set = true;
                }
                if (data.action == KAAlarm::FILE  &&  mActionSubType == MESSAGE)
                    mActionSubType = FILE;
                ++mAlarmCount;
                break;
            case AUDIO_ALARM:
            case PRE_ACTION_ALARM:
            case POST_ACTION_ALARM:
            case INVALID_ALARM:
            default:
                break;
        }
    }
    if (!isEmailText)
        mKMailSerialNumber = 0;

    Recurrence* recur = event->recurrence();
    if (recur  &&  recur->recurs())
    {
        const int nextRepeat = mNextRepeat;    // setRecurrence() clears mNextRepeat
        setRecurrence(*recur);
        if (nextRepeat <= mRepetition.count())
            mNextRepeat = nextRepeat;
    }
    else if (mRepetition)
    {
        // Convert a repetition with no recurrence into a recurrence
        if (mRepetition.isDaily())
            recur->setDaily(mRepetition.intervalDays());
        else
            recur->setMinutely(mRepetition.intervalMinutes());
        recur->setDuration(mRepetition.count() + 1);
        mRepetition.set(0, 0);
    }

    if (mRepeatAtLogin)
    {
        mArchiveRepeatAtLogin = false;
        if (mReminderMinutes > 0)
        {
            mReminderMinutes = 0;      // pre-alarm reminder not allowed for at-login alarm
            mReminderActive  = NO_REMINDER;
        }
        setRepeatAtLoginTrue(false);   // clear other incompatible statuses
    }

    if (mMainExpired  &&  deferralOffset  &&  checkRecur() != KARecurrence::NO_RECUR)
    {
        // Adjust the deferral time for an expired recurrence, since the
        // offset is relative to the first actual occurrence.
        DateTime dt = mRecurrence->getNextDateTime(mStartDateTime.addDays(-1).kDateTime());
        dt.setDateOnly(mStartDateTime.isDateOnly());
        if (mDeferralTime.isDateOnly())
        {
            mDeferralTime = deferralOffset.end(dt.kDateTime());
            mDeferralTime.setDateOnly(true);
        }
        else
            mDeferralTime = deferralOffset.end(dt.effectiveKDateTime());
    }
    if (mDeferral != NO_DEFERRAL)
    {
        if (setDeferralTime)
            mNextMainDateTime = mDeferralTime;
    }
    mTriggerChanged = true;
    endChanges();
}

void KAEvent::set(const KDateTime& dt, const QString& message, const QColor& bg, const QColor& fg,
                  const QFont& f, SubAction act, int lateCancel, Flags flags, bool changesPending)
{
    d->set(dt, message, bg, fg, f, act, lateCancel, flags, changesPending);
}

/******************************************************************************
* Initialise the instance with the specified parameters.
*/
void KAEvent::Private::set(const KDateTime& dateTime, const QString& text, const QColor& bg, const QColor& fg,
                           const QFont& font, SubAction action, int lateCancel, Flags flags, bool changesPending)
{
    clearRecur();
    mStartDateTime = dateTime;
    mStartDateTime.setDateOnly(flags & ANY_TIME);
    mNextMainDateTime = mStartDateTime;
    switch (action)
    {
        case MESSAGE:
        case FILE:
        case COMMAND:
        case EMAIL:
        case AUDIO:
            mActionSubType = (KAEvent::SubAction)action;
            break;
        default:
            mActionSubType = MESSAGE;
            break;
    }
    mEventID.clear();
    mTemplateName.clear();
#ifdef USE_AKONADI
    mItemId                 = -1;
    mOriginalCollectionId   = -1;
#else
    mResource               = 0;
    mOriginalResourceId.clear();
#endif
    mPreAction.clear();
    mPostAction.clear();
    mText                   = (mActionSubType == COMMAND) ? text.trimmed()
                              : (mActionSubType == AUDIO) ? QString() : text;
    mCategory               = KAlarm::CalEvent::ACTIVE;
    mAudioFile              = (mActionSubType == AUDIO) ? text : QString();
    mSoundVolume            = -1;
    mFadeVolume             = -1;
    mTemplateAfterTime      = -1;
    mFadeSeconds            = 0;
    mBgColour               = bg;
    mFgColour               = fg;
    mFont                   = font;
    mAlarmCount             = 1;
    mLateCancel             = lateCancel;     // do this before setting flags
    mDeferral               = NO_DEFERRAL;    // do this before setting flags

    mStartDateTime.setDateOnly(flags & ANY_TIME);
    set_deferral((flags & DEFERRAL) ? NORMAL_DEFERRAL : NO_DEFERRAL);
    mRepeatAtLogin          = flags & REPEAT_AT_LOGIN;
    mConfirmAck             = flags & CONFIRM_ACK;
    mUseDefaultFont         = flags & DEFAULT_FONT;
    mCommandScript          = flags & SCRIPT;
    mCommandXterm           = flags & EXEC_IN_XTERM;
    mCommandDisplay         = flags & DISPLAY_COMMAND;
    mCopyToKOrganizer       = flags & COPY_KORGANIZER;
    mExcludeHolidays        = (flags & EXCL_HOLIDAYS) ? mHolidays : 0;
    mWorkTimeOnly           = flags & WORK_TIME_ONLY;
    mEmailBcc               = flags & EMAIL_BCC;
    mEnabled                = !(flags & DISABLED);
    mDisplaying             = flags & DISPLAYING_;
    mReminderOnceOnly       = flags & REMINDER_ONCE;
    mAutoClose              = (flags & AUTO_CLOSE) && mLateCancel;
    mRepeatSound            = flags & REPEAT_SOUND;
    mSpeak                  = (flags & SPEAK) && action != AUDIO;
    mBeep                   = (flags & BEEP) && action != AUDIO && !mSpeak;
    if (mRepeatAtLogin)                       // do this after setting other flags
    {
        ++mAlarmCount;
        setRepeatAtLoginTrue(false);
    }

    mKMailSerialNumber      = 0;
    mReminderMinutes        = 0;
    mDeferDefaultMinutes    = 0;
    mDeferDefaultDateOnly   = false;
    mArchiveRepeatAtLogin   = false;
    mReminderActive         = NO_REMINDER;
    mDisplaying             = false;
    mMainExpired            = false;
    mDisplayingDefer        = false;
    mDisplayingEdit         = false;
    mArchive                = false;
    mCancelOnPreActErr      = false;
    mDontShowPreActErr      = false;
    mReminderAfterTime      = DateTime();
#ifdef USE_AKONADI
    mCompatibility          = KAlarm::Calendar::Current;
    mReadOnly               = false;
#endif
    mCommandError           = CMD_NO_ERROR;
    mChangeCount            = changesPending ? 1 : 0;
    mTriggerChanged         = true;
}

/******************************************************************************
* Update an existing KCal::Event with the KAEvent::Private data.
* If 'setCustomProperties' is true, all the KCal::Event's existing custom
* properties are cleared and replaced with the KAEvent's custom properties. If
* false, the KCal::Event's non-KAlarm custom properties are left untouched.
*/
#ifdef USE_AKONADI
bool KAEvent::updateKCalEvent(const KCalCore::Event::Ptr& e, UidAction u, bool setCustomProperties) const
{
    return d->updateKCalEvent(e, u, setCustomProperties);
}

#else
bool KAEvent::updateKCalEvent(KCal::Event* e, UidAction u) const
{
    return d->updateKCalEvent(e, u);
}
#endif

#ifdef USE_AKONADI
bool KAEvent::Private::updateKCalEvent(const Event::Ptr& ev, UidAction uidact, bool setCustomProperties) const
#else
bool KAEvent::Private::updateKCalEvent(Event* ev, UidAction uidact) const
#endif
{
    // If it's an archived event, the event start date/time will be adjusted to its original
    // value instead of its next occurrence, and the expired main alarm will be reinstated.
    const bool archived = (mCategory == KAlarm::CalEvent::ARCHIVED);

    if (!ev
    ||  (uidact == UID_CHECK  &&  !mEventID.isEmpty()  &&  mEventID != ev->uid())
    ||  (!mAlarmCount  &&  (!archived || !mMainExpired)))
        return false;

    ev->startUpdates();   // prevent multiple update notifications
    checkRecur();         // ensure recurrence/repetition data is consistent
    const bool readOnly = ev->isReadOnly();
    if (uidact == KAEvent::UID_SET)
        ev->setUid(mEventID);
#ifdef USE_AKONADI
    ev->setReadOnly(mReadOnly);
#else
    ev->setReadOnly(false);
#endif
    ev->setTransparency(Event::Transparent);

    // Set up event-specific data

    // Set up custom properties.
#ifdef USE_AKONADI
    if (setCustomProperties)
        ev->setCustomProperties(mCustomProperties);
#endif
    ev->removeCustomProperty(KAlarm::Calendar::APPNAME, FLAGS_PROPERTY);
    ev->removeCustomProperty(KAlarm::Calendar::APPNAME, NEXT_RECUR_PROPERTY);
    ev->removeCustomProperty(KAlarm::Calendar::APPNAME, REPEAT_PROPERTY);
    ev->removeCustomProperty(KAlarm::Calendar::APPNAME, LOG_PROPERTY);

    QString param;
    if (mCategory == KAlarm::CalEvent::DISPLAYING)
    {
#ifdef USE_AKONADI
        param = QString::number(mOriginalCollectionId);
#else
        param = mOriginalResourceId;
#endif
        if (mDisplayingDefer)
            param += SC + DISP_DEFER;
        if (mDisplayingEdit)
            param += SC + DISP_EDIT;
    }
#ifdef USE_AKONADI
    KAlarm::CalEvent::setStatus(ev, mCategory, param);
#else
    KAlarm::CalEvent::setStatus(ev, mCategory, param);
#endif
    QStringList flags;
    if (mStartDateTime.isDateOnly())
        flags += DATE_ONLY_FLAG;
    if (mConfirmAck)
        flags += CONFIRM_ACK_FLAG;
    if (mEmailBcc)
        flags += EMAIL_BCC_FLAG;
    if (mCopyToKOrganizer)
        flags += KORGANIZER_FLAG;
    if (mExcludeHolidays)
        flags += EXCLUDE_HOLIDAYS_FLAG;
    if (mWorkTimeOnly)
        flags += WORK_TIME_ONLY_FLAG;
    if (mLateCancel)
        (flags += (mAutoClose ? AUTO_CLOSE_FLAG : LATE_CANCEL_FLAG)) += QString::number(mLateCancel);
    if (mReminderMinutes)
    {
        flags += REMINDER_TYPE;
        if (mReminderOnceOnly)
            flags += REMINDER_ONCE_FLAG;
        flags += reminderToString(-mReminderMinutes);
    }
    if (mDeferDefaultMinutes)
    {
        QString param = QString::number(mDeferDefaultMinutes);
        if (mDeferDefaultDateOnly)
            param += 'D';
        (flags += DEFER_FLAG) += param;
    }
    if (!mTemplateName.isEmpty()  &&  mTemplateAfterTime >= 0)
        (flags += TEMPL_AFTER_TIME_FLAG) += QString::number(mTemplateAfterTime);
    if (mKMailSerialNumber)
        (flags += KMAIL_SERNUM_FLAG) += QString::number(mKMailSerialNumber);
    if (mArchive  &&  !archived)
    {
        flags += ARCHIVE_FLAG;
        if (mArchiveRepeatAtLogin)
            flags += AT_LOGIN_TYPE;
    }
    if (!flags.isEmpty())
        ev->setCustomProperty(KAlarm::Calendar::APPNAME, FLAGS_PROPERTY, flags.join(SC));

    if (mCommandXterm)
        ev->setCustomProperty(KAlarm::Calendar::APPNAME, LOG_PROPERTY, xtermURL);
    else if (mCommandDisplay)
        ev->setCustomProperty(KAlarm::Calendar::APPNAME, LOG_PROPERTY, displayURL);
    else if (!mLogFile.isEmpty())
        ev->setCustomProperty(KAlarm::Calendar::APPNAME, LOG_PROPERTY, mLogFile);

    ev->setCustomStatus(mEnabled ? QString() : DISABLED_STATUS);
    ev->setRevision(mRevision);
    ev->clearAlarms();

    /* Always set DTSTART as date/time, and use the category "DATE" to indicate
     * a date-only event, instead of calling setAllDay(). This is necessary to
     * allow a time zone to be specified for a date-only event. Also, KAlarm
     * allows the alarm to float within the 24-hour period defined by the
     * start-of-day time (which is user-dependent and therefore can't be
     * written into the calendar) rather than midnight to midnight, and there
     * is no RFC2445 conformant way to specify this.
     * RFC2445 states that alarm trigger times specified in absolute terms
     * (rather than relative to DTSTART or DTEND) can only be specified as a
     * UTC DATE-TIME value. So always use a time relative to DTSTART instead of
     * an absolute time.
     */
    ev->setDtStart(mStartDateTime.calendarKDateTime());
    ev->setAllDay(false);
    ev->setHasEndDate(false);

    const DateTime dtMain = archived ? mStartDateTime : mNextMainDateTime;
    int      ancillaryType = 0;   // 0 = invalid, 1 = time, 2 = offset
    DateTime ancillaryTime;       // time for ancillary alarms (pre-action, extra audio, etc)
    int      ancillaryOffset = 0; // start offset for ancillary alarms
    if (!mMainExpired  ||  archived)
    {
        /* The alarm offset must always be zero for the main alarm. To determine
         * which recurrence is due, the property X-KDE-KALARM_NEXTRECUR is used.
         * If the alarm offset was non-zero, exception dates and rules would not
         * work since they apply to the event time, not the alarm time.
         */
        if (!archived  &&  checkRecur() != KARecurrence::NO_RECUR)
        {
            QDateTime dt = mNextMainDateTime.kDateTime().toTimeSpec(mStartDateTime.timeSpec()).dateTime();
            ev->setCustomProperty(KAlarm::Calendar::APPNAME, NEXT_RECUR_PROPERTY,
                                  dt.toString(mNextMainDateTime.isDateOnly() ? "yyyyMMdd" : "yyyyMMddThhmmss"));
        }
        // Add the main alarm
        initKCalAlarm(ev, 0, QStringList(), MAIN_ALARM);
        ancillaryOffset = 0;
        ancillaryType = dtMain.isValid() ? 2 : 0;
    }
    else if (mRepetition)
    {
        // Alarm repetition is normally held in the main alarm, but since
        // the main alarm has expired, store in a custom property.
        const QString param = QString("%1:%2").arg(mRepetition.intervalMinutes()).arg(mRepetition.count());
        ev->setCustomProperty(KAlarm::Calendar::APPNAME, REPEAT_PROPERTY, param);
    }

    // Add subsidiary alarms
    if (mRepeatAtLogin  ||  (mArchiveRepeatAtLogin && archived))
    {
        DateTime dtl;
        if (mArchiveRepeatAtLogin)
            dtl = mStartDateTime.calendarKDateTime().addDays(-1);
        else if (mAtLoginDateTime.isValid())
            dtl = mAtLoginDateTime;
        else if (mStartDateTime.isDateOnly())
            dtl = DateTime(KDateTime::currentLocalDate().addDays(-1), mStartDateTime.timeSpec());
        else
            dtl = KDateTime::currentUtcDateTime();
        initKCalAlarm(ev, dtl, QStringList(AT_LOGIN_TYPE));
        if (!ancillaryType  &&  dtl.isValid())
        {
            ancillaryTime = dtl;
            ancillaryType = 1;
        }
    }

    // Find the base date/time for calculating alarm offsets
    DateTime nextDateTime = mNextMainDateTime;
    if (mMainExpired)
    {
        if (checkRecur() == KARecurrence::NO_RECUR)
            nextDateTime = mStartDateTime;
        else if (!archived)
        {
            // It's a deferral of an expired recurrence.
            // Need to ensure that the alarm offset is to an occurrence
            // which isn't excluded by an exception - otherwise, it will
            // never be triggered. So choose the first recurrence which
            // isn't an exception.
            KDateTime dt = mRecurrence->getNextDateTime(mStartDateTime.addDays(-1).kDateTime());
            dt.setDateOnly(mStartDateTime.isDateOnly());
            nextDateTime = dt;
        }
    }

    if (mReminderMinutes  &&  (mReminderActive != NO_REMINDER || archived))
    {
        int startOffset;
        if (mReminderMinutes < 0  &&  mReminderActive != NO_REMINDER)
        {
            // A reminder AFTER the main alarm is active or disabled
            startOffset = nextDateTime.calendarKDateTime().secsTo(mReminderAfterTime.calendarKDateTime());
        }
        else
        {
            // A reminder BEFORE the main alarm is active
            startOffset = -mReminderMinutes * 60;
        }
        initKCalAlarm(ev, startOffset, QStringList(REMINDER_TYPE));
        // Don't set ancillary time if the reminder AFTER is hidden by a deferral
        if (!ancillaryType  &&  (mReminderActive == ACTIVE_REMINDER || archived))
        {
            ancillaryOffset = startOffset;
            ancillaryType = 2;
        }
    }
    if (mDeferral != NO_DEFERRAL)
    {
        int startOffset;
        QStringList list;
        if (mDeferralTime.isDateOnly())
        {
            startOffset = nextDateTime.secsTo(mDeferralTime.calendarKDateTime());
            list += DATE_DEFERRAL_TYPE;
        }
        else
        {
            startOffset = nextDateTime.calendarKDateTime().secsTo(mDeferralTime.calendarKDateTime());
            list += TIME_DEFERRAL_TYPE;
        }
        if (mDeferral == REMINDER_DEFERRAL)
            list += REMINDER_TYPE;
        initKCalAlarm(ev, startOffset, list);
        if (!ancillaryType  &&  mDeferralTime.isValid())
        {
            ancillaryOffset = startOffset;
            ancillaryType = 2;
        }
    }
    if (!mTemplateName.isEmpty())
        ev->setSummary(mTemplateName);
    else if (mDisplaying)
    {
        QStringList list(DISPLAYING_TYPE);
        if (mDisplayingFlags & REPEAT_AT_LOGIN)
            list += AT_LOGIN_TYPE;
        else if (mDisplayingFlags & DEFERRAL)
        {
            if (mDisplayingFlags & TIMED_FLAG)
                list += TIME_DEFERRAL_TYPE;
            else
                list += DATE_DEFERRAL_TYPE;
        }
        if (mDisplayingFlags & REMINDER)
            list += REMINDER_TYPE;
        initKCalAlarm(ev, mDisplayingTime, list);
        if (!ancillaryType  &&  mDisplayingTime.isValid())
        {
            ancillaryTime = mDisplayingTime;
            ancillaryType = 1;
        }
    }
    if ((mBeep  ||  mSpeak  ||  !mAudioFile.isEmpty())  &&  mActionSubType != AUDIO)
    {
        // A sound is specified
        if (ancillaryType == 2)
            initKCalAlarm(ev, ancillaryOffset, QStringList(), AUDIO_ALARM);
        else
            initKCalAlarm(ev, ancillaryTime, QStringList(), AUDIO_ALARM);
    }
    if (!mPreAction.isEmpty())
    {
        // A pre-display action is specified
        if (ancillaryType == 2)
            initKCalAlarm(ev, ancillaryOffset, QStringList(PRE_ACTION_TYPE), PRE_ACTION_ALARM);
        else
            initKCalAlarm(ev, ancillaryTime, QStringList(PRE_ACTION_TYPE), PRE_ACTION_ALARM);
    }
    if (!mPostAction.isEmpty())
    {
        // A post-display action is specified
        if (ancillaryType == 2)
            initKCalAlarm(ev, ancillaryOffset, QStringList(POST_ACTION_TYPE), POST_ACTION_ALARM);
        else
            initKCalAlarm(ev, ancillaryTime, QStringList(POST_ACTION_TYPE), POST_ACTION_ALARM);
    }

    if (mRecurrence)
        mRecurrence->writeRecurrence(*ev->recurrence());
    else
        ev->clearRecurrence();
    if (mCreatedDateTime.isValid())
        ev->setCreated(mCreatedDateTime);
    ev->setReadOnly(readOnly);
    ev->endUpdates();     // finally issue an update notification
    return true;
}

/******************************************************************************
* Create a new alarm for a libkcal event, and initialise it according to the
* alarm action. If 'types' is non-null, it is appended to the X-KDE-KALARM-TYPE
* property value list.
* NOTE: The variant taking a DateTime calculates the offset from mStartDateTime,
*       which is not suitable for an alarm in a recurring event.
*/
#ifdef USE_AKONADI
Alarm::Ptr KAEvent::Private::initKCalAlarm(const Event::Ptr& event, const DateTime& dt, const QStringList& types, AlarmType type) const
#else
Alarm* KAEvent::Private::initKCalAlarm(Event* event, const DateTime& dt, const QStringList& types, AlarmType type) const
#endif
{
    const int startOffset = dt.isDateOnly() ? mStartDateTime.secsTo(dt)
                                      : mStartDateTime.calendarKDateTime().secsTo(dt.calendarKDateTime());
    return initKCalAlarm(event, startOffset, types, type);
}

#ifdef USE_AKONADI
Alarm::Ptr KAEvent::Private::initKCalAlarm(const Event::Ptr& event, int startOffsetSecs, const QStringList& types, AlarmType type) const
#else
Alarm* KAEvent::Private::initKCalAlarm(Event* event, int startOffsetSecs, const QStringList& types, AlarmType type) const
#endif
{
    QStringList alltypes;
    QStringList flags;
#ifdef USE_AKONADI
    Alarm::Ptr alarm = event->newAlarm();
#else
    Alarm* alarm = event->newAlarm();
#endif
    alarm->setEnabled(true);
    if (type != MAIN_ALARM)
    {
        // RFC2445 specifies that absolute alarm times must be stored as a UTC DATE-TIME value.
        // Set the alarm time as an offset to DTSTART for the reasons described in updateKCalEvent().
        alarm->setStartOffset(startOffsetSecs);
    }

    switch (type)
    {
        case AUDIO_ALARM:
            setAudioAlarm(alarm);
            if (mSpeak)
                flags << Private::SPEAK_FLAG;
            if (mRepeatSound)
            {
                alarm->setRepeatCount(-1);
                alarm->setSnoozeTime(0);
            }
            break;
        case PRE_ACTION_ALARM:
            setProcedureAlarm(alarm, mPreAction);
            if (mCancelOnPreActErr)
                flags << Private::CANCEL_ON_ERROR_FLAG;
            if (mDontShowPreActErr)
                flags << Private::DONT_SHOW_ERROR_FLAG;
            break;
        case POST_ACTION_ALARM:
            setProcedureAlarm(alarm, mPostAction);
            break;
        case MAIN_ALARM:
            alarm->setSnoozeTime(mRepetition.interval());
            alarm->setRepeatCount(mRepetition.count());
            if (mRepetition)
                alarm->setCustomProperty(KAlarm::Calendar::APPNAME, NEXT_REPEAT_PROPERTY,
                                         QString::number(mNextRepeat));
            // fall through to INVALID_ALARM
        case REMINDER_ALARM:
        case INVALID_ALARM:
        {
            if (types == QStringList(REMINDER_TYPE)
            &&  mReminderMinutes < 0  &&  mReminderActive == HIDDEN_REMINDER)
            {
                // It's a reminder AFTER the alarm which is currently disabled
                // due to the main alarm being deferred past it.
                flags << HIDDEN_REMINDER_FLAG;
            }
            bool display = false;
            switch (mActionSubType)
            {
                case FILE:
                    alltypes += FILE_TYPE;
                    // fall through to MESSAGE
                case MESSAGE:
                    alarm->setDisplayAlarm(AlarmText::toCalendarText(mText));
                    display = true;
                    break;
                case COMMAND:
                    if (mCommandScript)
                        alarm->setProcedureAlarm("", mText);
                    else
                        setProcedureAlarm(alarm, mText);
                    display = mCommandDisplay;
                    break;
                case EMAIL:
                    alarm->setEmailAlarm(mEmailSubject, mText, mEmailAddresses, mEmailAttachments);
                    if (mEmailFromIdentity)
                        flags << Private::EMAIL_ID_FLAG << QString::number(mEmailFromIdentity);
                    break;
                case AUDIO:
                    setAudioAlarm(alarm);
                    if (mRepeatSound)
                        alltypes += SOUND_REPEAT_TYPE;
                    break;
            }
            if (display)
                alarm->setCustomProperty(KAlarm::Calendar::APPNAME, FONT_COLOUR_PROPERTY,
                                         QString::fromLatin1("%1;%2;%3").arg(mBgColour.name())
                                                                        .arg(mFgColour.name())
                                                                        .arg(mUseDefaultFont ? QString() : mFont.toString()));
            break;
        }
        case DEFERRED_ALARM:
        case DEFERRED_REMINDER_ALARM:
        case AT_LOGIN_ALARM:
        case DISPLAYING_ALARM:
            break;
    }
    alltypes += types;
    if (!alltypes.isEmpty())
        alarm->setCustomProperty(KAlarm::Calendar::APPNAME, TYPE_PROPERTY, alltypes.join(","));
    if (!flags.isEmpty())
        alarm->setCustomProperty(KAlarm::Calendar::APPNAME, FLAGS_PROPERTY, flags.join(SC));
    return alarm;
}

bool KAEvent::isValid() const
{
    return d->mAlarmCount  &&  (d->mAlarmCount != 1 || !d->mRepeatAtLogin);
}

void KAEvent::setEnabled(bool enable)
{
    d->mEnabled = enable;
}

bool KAEvent::enabled() const
{
    return d->mEnabled;
}

#ifdef USE_AKONADI
void KAEvent::setReadOnly(bool ro)
{
    d->mReadOnly = ro;
}

bool KAEvent::isReadOnly() const
{
    return d->mReadOnly;
}
#endif

void KAEvent::setArchive()
{
    d->mArchive = true;
}

bool KAEvent::toBeArchived() const
{
    return d->mArchive;
}

bool KAEvent::mainExpired() const
{
    return d->mMainExpired;
}

bool KAEvent::expired() const
{
    return (d->mDisplaying && d->mMainExpired)  ||  d->mCategory == KAlarm::CalEvent::ARCHIVED;
}

KAEvent::Flags KAEvent::flags() const
{
    return d->flags();
}

KAEvent::Flags KAEvent::Private::flags() const
{
    Flags result(0);
    if (mBeep)                       result |= BEEP;
    if (mRepeatSound)                result |= REPEAT_SOUND;
    if (mEmailBcc)                   result |= EMAIL_BCC;
    if (mStartDateTime.isDateOnly()) result |= ANY_TIME;
    if (mDeferral != NO_DEFERRAL)    result |= static_cast<Flag>(DEFERRAL);
    if (mSpeak)                      result |= SPEAK;
    if (mRepeatAtLogin)              result |= REPEAT_AT_LOGIN;
    if (mConfirmAck)                 result |= CONFIRM_ACK;
    if (mUseDefaultFont)             result |= DEFAULT_FONT;
    if (mCommandScript)              result |= SCRIPT;
    if (mCommandXterm)               result |= EXEC_IN_XTERM;
    if (mCommandDisplay)             result |= DISPLAY_COMMAND;
    if (mCopyToKOrganizer)           result |= COPY_KORGANIZER;
    if (mExcludeHolidays)            result |= EXCL_HOLIDAYS;
    if (mWorkTimeOnly)               result |= WORK_TIME_ONLY;
    if (mReminderOnceOnly)           result |= REMINDER_ONCE;
    if (mAutoClose)                  result |= AUTO_CLOSE;
    if (mDisplaying)                 result |= static_cast<Flag>(DISPLAYING_);
    if (!mEnabled)                   result |= DISABLED;
    return result;
}

/******************************************************************************
* Change the type of an event.
* If it is being set to archived, set the archived indication in the event ID;
* otherwise, remove the archived indication from the event ID.
*/
void KAEvent::setCategory(KAlarm::CalEvent::Type s)
{
    d->setCategory(s);
}

void KAEvent::Private::setCategory(KAlarm::CalEvent::Type s)
{
    if (s == mCategory)
        return;
    mEventID = KAlarm::CalEvent::uid(mEventID, s);
    mCategory = s;
    mTriggerChanged = true;   // templates and archived don't have trigger times
}

KAlarm::CalEvent::Type KAEvent::category() const
{
    return d->mCategory;
}

void KAEvent::setEventId(const QString& id)
{
    d->mEventID = id;
}

QString KAEvent::id() const
{
    return d->mEventID;
}

void KAEvent::incrementRevision()
{
    ++d->mRevision;
}

int KAEvent::revision() const
{
    return d->mRevision;
}

#ifdef USE_AKONADI
void KAEvent::setItemId(Akonadi::Item::Id id)
{
    d->mItemId = id;
}

Akonadi::Item::Id KAEvent::itemId() const
{
    return d->mItemId;
}

/******************************************************************************
* Initialise an Item with the event.
* Note that the event is not updated with the Item ID.
* Reply = true if successful,
*         false if event's category does not match collection's mime types.
*/
bool KAEvent::setItemPayload(Akonadi::Item& item, const QStringList& collectionMimeTypes) const
{
    QString mimetype;
    switch (d->mCategory)
    {
        case KAlarm::CalEvent::ACTIVE:      mimetype = KAlarm::MIME_ACTIVE;    break;
        case KAlarm::CalEvent::ARCHIVED:    mimetype = KAlarm::MIME_ARCHIVED;  break;
        case KAlarm::CalEvent::TEMPLATE:    mimetype = KAlarm::MIME_TEMPLATE;  break;
        default:                            Q_ASSERT(0);  return false;
    }
    if (!collectionMimeTypes.contains(mimetype))
        return false;
    item.setMimeType(mimetype);
    item.setPayload<KAEvent>(*this);
    return true;
}

void KAEvent::setCompatibility(KAlarm::Calendar::Compat c)
{
    d->mCompatibility = c;
}

KAlarm::Calendar::Compat KAEvent::compatibility() const
{
    return d->mCompatibility;
}

QMap<QByteArray, QString> KAEvent::customProperties() const
{
    return d->mCustomProperties;
}

#else
void KAEvent::setResource(AlarmResource* r)
{
    d->mResource = r;
}

AlarmResource* KAEvent::resource() const
{
    return d->mResource;
}
#endif

KAEvent::SubAction KAEvent::actionSubType() const
{
    return d->mActionSubType;
}

KAEvent::Actions KAEvent::actionTypes() const
{
    switch (d->mActionSubType)
    {
        case MESSAGE:
        case FILE:     return ACT_DISPLAY;
        case COMMAND:  return d->mCommandDisplay ? ACT_DISPLAY_COMMAND : ACT_COMMAND;
        case EMAIL:    return ACT_EMAIL;
        case AUDIO:    return ACT_AUDIO;
        default:       return ACT_NONE;
    }
}

void KAEvent::setLateCancel(int minutes)
{
    if (d->mRepeatAtLogin)
        minutes = 0;
    d->mLateCancel = minutes;
    if (!minutes)
        d->mAutoClose = false;
}

int KAEvent::lateCancel() const
{
    return d->mLateCancel;
}

void KAEvent::setAutoClose(bool ac)
{
    d->mAutoClose = ac;
}

bool KAEvent::autoClose() const
{
    return d->mAutoClose;
}

void KAEvent::setKMailSerialNumber(unsigned long n)
{
    d->mKMailSerialNumber = n;
}

unsigned long KAEvent::kmailSerialNumber() const
{
    return d->mKMailSerialNumber;
}

QString KAEvent::cleanText() const
{
    return d->mText;
}

QString KAEvent::message() const
{
    return (d->mActionSubType == MESSAGE
         || d->mActionSubType == EMAIL) ? d->mText : QString();
}

QString KAEvent::displayMessage() const
{
    return (d->mActionSubType == MESSAGE) ? d->mText : QString();
}

QString KAEvent::fileName() const
{
    return (d->mActionSubType == FILE) ? d->mText : QString();
}

QColor KAEvent::bgColour() const
{
    return d->mBgColour;
}

QColor KAEvent::fgColour() const
{
    return d->mFgColour;
}

void KAEvent::setDefaultFont(const QFont& f)
{
    Private::mDefaultFont = f;
}

bool KAEvent::useDefaultFont() const
{
    return d->mUseDefaultFont;
}

QFont KAEvent::font() const
{
    return d->mUseDefaultFont ? Private::mDefaultFont : d->mFont;
}

QString KAEvent::command() const
{
    return (d->mActionSubType == COMMAND) ? d->mText : QString();
}

bool KAEvent::commandScript() const
{
    return d->mCommandScript;
}

bool KAEvent::commandXterm() const
{
    return d->mCommandXterm;
}

bool KAEvent::commandDisplay() const
{
    return d->mCommandDisplay;
}

#ifdef USE_AKONADI
void KAEvent::setCommandError(CmdErrType t) const
{
    d->mCommandError = t;
}

#else
/******************************************************************************
* Set the command last error status.
* If 'writeConfig' is true, the status is written to the config file.
*/
void KAEvent::setCommandError(CmdErrType t, bool writeConfig) const
{
    d->setCommandError(t, writeConfig);
}

void KAEvent::Private::setCommandError(CmdErrType error, bool writeConfig) const
{
    kDebug() << mEventID << "," << error;
    if (error == mCommandError)
        return;
    mCommandError = error;
    if (writeConfig)
    {
        KConfigGroup config(KGlobal::config(), mCmdErrConfigGroup);
        if (mCommandError == CMD_NO_ERROR)
            config.deleteEntry(mEventID);
        else
        {
            QString errtext;
            switch (mCommandError)
            {
                case CMD_ERROR:       errtext = CMD_ERROR_VALUE;  break;
                case CMD_ERROR_PRE:   errtext = CMD_ERROR_PRE_VALUE;  break;
                case CMD_ERROR_POST:  errtext = CMD_ERROR_POST_VALUE;  break;
                case CMD_ERROR_PRE_POST:
                    errtext = CMD_ERROR_PRE_VALUE + ',' + CMD_ERROR_POST_VALUE;
                    break;
                default:
                    break;
            }
            config.writeEntry(mEventID, errtext);
        }
        config.sync();
    }
}

/******************************************************************************
* Initialise the command last error status of the alarm from the config file.
*/
void KAEvent::setCommandError(const QString& configString)
{
    d->setCommandError(configString);
}

void KAEvent::Private::setCommandError(const QString& configString)
{
    mCommandError = CMD_NO_ERROR;
    const QStringList errs = configString.split(',');
    if (errs.indexOf(CMD_ERROR_VALUE) >= 0)
        mCommandError = CMD_ERROR;
    else
    {
        if (errs.indexOf(CMD_ERROR_PRE_VALUE) >= 0)
            mCommandError = CMD_ERROR_PRE;
        if (errs.indexOf(CMD_ERROR_POST_VALUE) >= 0)
            mCommandError = static_cast<CmdErrType>(mCommandError | CMD_ERROR_POST);
    }
}

QString KAEvent::commandErrorConfigGroup()
{
    return Private::mCmdErrConfigGroup;
}
#endif

KAEvent::CmdErrType KAEvent::commandError() const
{
    return d->mCommandError;
}

void KAEvent::setLogFile(const QString& logfile)
{
    d->mLogFile = logfile;
    if (!logfile.isEmpty())
        d->mCommandDisplay = d->mCommandXterm = false;
}

QString KAEvent::logFile() const
{
    return d->mLogFile;
}

bool KAEvent::confirmAck() const
{
    return d->mConfirmAck;
}

bool KAEvent::copyToKOrganizer() const
{
    return d->mCopyToKOrganizer;
}

#ifdef USE_AKONADI
void KAEvent::setEmail(uint from, const KCalCore::Person::List& addresses, const QString& subject,
                      const QStringList& attachments)
#else
void KAEvent::setEmail(uint from, const QList<KCal::Person>& addresses, const QString& subject,
                      const QStringList& attachments)
#endif
{
    d->mEmailFromIdentity = from;
    d->mEmailAddresses    = addresses;
    d->mEmailSubject      = subject;
    d->mEmailAttachments  = attachments;
}

QString KAEvent::emailMessage() const
{
    return (d->mActionSubType == EMAIL) ? d->mText : QString();
}

uint KAEvent::emailFromId() const
{
    return d->mEmailFromIdentity;
}

#ifdef USE_AKONADI
KCalCore::Person::List KAEvent::emailAddressees() const
#else
QList<KCal::Person> KAEvent::emailAddressees() const
#endif
{
    return d->mEmailAddresses;
}

QStringList KAEvent::emailAddresses() const
{
    return static_cast<QStringList>(d->mEmailAddresses);
}

QString KAEvent::emailAddresses(const QString& sep) const
{
    return d->mEmailAddresses.join(sep);
}

#ifdef USE_AKONADI
QString KAEvent::joinEmailAddresses(const KCalCore::Person::List& addresses, const QString& separator)
#else
QString KAEvent::joinEmailAddresses(const QList<KCal::Person>& addresses, const QString& separator)
#endif
{
    return EmailAddressList(addresses).join(separator);
}

QStringList KAEvent::emailPureAddresses() const
{
    return d->mEmailAddresses.pureAddresses();
}

QString KAEvent::emailPureAddresses(const QString& sep) const
{
    return d->mEmailAddresses.pureAddresses(sep);
}

QString KAEvent::emailSubject() const
{
    return d->mEmailSubject;
}

QStringList KAEvent::emailAttachments() const
{
    return d->mEmailAttachments;
}

QString KAEvent::emailAttachments(const QString& sep) const
{
    return d->mEmailAttachments.join(sep);
}

bool KAEvent::emailBcc() const
{
    return d->mEmailBcc;
}

void KAEvent::setAudioFile(const QString& filename, float volume, float fadeVolume, int fadeSeconds, bool allowEmptyFile)
{
    d->setAudioFile(filename, volume, fadeVolume, fadeSeconds, allowEmptyFile);
}

void KAEvent::Private::setAudioFile(const QString& filename, float volume, float fadeVolume, int fadeSeconds, bool allowEmptyFile)
{
    mAudioFile = filename;
    mSoundVolume = (!allowEmptyFile && filename.isEmpty()) ? -1 : volume;
    if (mSoundVolume >= 0)
    {
        mFadeVolume  = (fadeSeconds > 0) ? fadeVolume : -1;
        mFadeSeconds = (mFadeVolume >= 0) ? fadeSeconds : 0;
    }
    else
    {
        mFadeVolume  = -1;
        mFadeSeconds = 0;
    }
}

QString KAEvent::audioFile() const
{
    return d->mAudioFile;
}

float KAEvent::soundVolume() const
{
    return d->mSoundVolume;
}

float KAEvent::fadeVolume() const
{
    return d->mSoundVolume >= 0 && d->mFadeSeconds ? d->mFadeVolume : -1;
}

int KAEvent::fadeSeconds() const
{
    return d->mSoundVolume >= 0 && d->mFadeVolume >= 0 ? d->mFadeSeconds : 0;
}

bool KAEvent::repeatSound() const
{
    return d->mRepeatSound;
}

bool KAEvent::beep() const
{
    return d->mBeep;
}

bool KAEvent::speak() const
{
    return (d->mActionSubType == MESSAGE
            ||  (d->mActionSubType == COMMAND && d->mCommandDisplay))
        && d->mSpeak;
}

/******************************************************************************
* Set the event to be an alarm template.
*/
void KAEvent::setTemplate(const QString& name, int afterTime)
{
    d->setCategory(KAlarm::CalEvent::TEMPLATE);
    d->mTemplateName = name;
    d->mTemplateAfterTime = afterTime;
    d->mTriggerChanged = true;   // templates and archived don't have trigger times
}

bool KAEvent::isTemplate() const
{
    return !d->mTemplateName.isEmpty();
}

QString KAEvent::templateName() const
{
    return d->mTemplateName;
}

bool KAEvent::usingDefaultTime() const
{
    return d->mTemplateAfterTime == 0;
}

int KAEvent::templateAfterTime() const
{
    return d->mTemplateAfterTime;
}

void KAEvent::setActions(const QString& pre, const QString& post, bool cancelOnError, bool dontShowError)
{
    d->mPreAction = pre;
    d->mPostAction = post;
    d->mCancelOnPreActErr = cancelOnError;
    d->mDontShowPreActErr = dontShowError;
}

QString KAEvent::preAction() const
{
    return d->mPreAction;
}

QString KAEvent::postAction() const
{
    return d->mPostAction;
}

bool KAEvent::cancelOnPreActionError() const
{
    return d->mCancelOnPreActErr;
}

bool KAEvent::dontShowPreActionError() const
{
    return d->mDontShowPreActErr;
}

/******************************************************************************
* Set a reminder.
* 'minutes' = number of minutes BEFORE the main alarm.
*/
void KAEvent::setReminder(int minutes, bool onceOnly)
{
    d->setReminder(minutes, onceOnly);
}

void KAEvent::Private::setReminder(int minutes, bool onceOnly)
{
    if (minutes > 0  &&  mRepeatAtLogin)
        minutes = 0;
    if (minutes != mReminderMinutes  ||  (minutes && mReminderActive != ACTIVE_REMINDER))
    {
        if (minutes  &&  mReminderActive == NO_REMINDER)
            ++mAlarmCount;
        else if (!minutes  &&  mReminderActive != NO_REMINDER)
            --mAlarmCount;
        mReminderMinutes   = minutes;
        mReminderActive    = minutes ? ACTIVE_REMINDER : NO_REMINDER;
        mReminderOnceOnly  = onceOnly;
        mReminderAfterTime = DateTime();
        mTriggerChanged = true;
    }
}

/******************************************************************************
* Activate the event's reminder which occurs AFTER the given main alarm time.
* Reply = true if successful (i.e. reminder falls before the next main alarm).
*/
void KAEvent::activateReminderAfter(const DateTime& mainAlarmTime)
{
    d->activateReminderAfter(mainAlarmTime);
}

void KAEvent::Private::activateReminderAfter(const DateTime& mainAlarmTime)
{
    if (mReminderMinutes >= 0  ||  mReminderActive == ACTIVE_REMINDER  ||  !mainAlarmTime.isValid())
        return;
    // There is a reminder AFTER the main alarm.
    if (checkRecur() != KARecurrence::NO_RECUR)
    {
        // For a recurring alarm, the given alarm time must be a recurrence, not a sub-repetition.
        DateTime next;
        //???? For some unknown reason, addSecs(-1) returns the recurrence after the next,
        //???? so addSecs(-60) is used instead.
        if (nextRecurrence(mainAlarmTime.addSecs(-60).effectiveKDateTime(), next) == NO_OCCURRENCE
        ||  mainAlarmTime != next)
            return;
    }
    else if (!mRepeatAtLogin)
    {
        // For a non-recurring alarm, the given alarm time must be the main alarm time.
        if (mainAlarmTime != mStartDateTime)
            return;
    }

    const DateTime reminderTime = mainAlarmTime.addMins(-mReminderMinutes);
    DateTime next;
    if (nextOccurrence(mainAlarmTime.effectiveKDateTime(), next, RETURN_REPETITION) != NO_OCCURRENCE
    &&  reminderTime >= next)
        return;    // the reminder time is after the next occurrence of the main alarm

    kDebug() << "Setting reminder at" << reminderTime.effectiveKDateTime().dateTime();
    activate_reminder(true);
    mReminderAfterTime = reminderTime;
}

int KAEvent::reminderMinutes() const
{
    return d->mReminderMinutes;
}

bool KAEvent::reminderActive() const
{
    return d->mReminderActive == Private::ACTIVE_REMINDER;
}

bool KAEvent::reminderOnceOnly() const
{
    return d->mReminderOnceOnly;
}

bool KAEvent::reminderDeferral() const
{
    return d->mDeferral == Private::REMINDER_DEFERRAL;
}

/******************************************************************************
* Defer the event to the specified time.
* If the main alarm time has passed, the main alarm is marked as expired.
* If 'adjustRecurrence' is true, ensure that the next scheduled recurrence is
* after the current time.
*/
void KAEvent::defer(const DateTime& dt, bool reminder, bool adjustRecurrence)
{
    return d->defer(dt, reminder, adjustRecurrence);
}

void KAEvent::Private::defer(const DateTime& dateTime, bool reminder, bool adjustRecurrence)
{
    startChanges();   // prevent multiple trigger time evaluation here
    bool setNextRepetition = false;
    bool checkRepetition = false;
    bool checkReminderAfter = false;
    if (checkRecur() == KARecurrence::NO_RECUR)
    {
        // Deferring a non-recurring alarm
        if (mReminderMinutes)
        {
            bool deferReminder = false;
            if (mReminderMinutes > 0)
            {
                // There's a reminder BEFORE the main alarm
                if (dateTime < mNextMainDateTime.effectiveKDateTime())
                    deferReminder = true;
                else if (mReminderActive == ACTIVE_REMINDER  ||  mDeferral == REMINDER_DEFERRAL)
                {
                    // Deferring past the main alarm time, so adjust any existing deferral
                    set_deferral(NO_DEFERRAL);
                    mTriggerChanged = true;
                }
            }
            else if (mReminderMinutes < 0  &&  reminder)
                deferReminder = true;    // deferring a reminder AFTER the main alarm
            if (deferReminder)
            {
                set_deferral(REMINDER_DEFERRAL);   // defer reminder alarm
                mDeferralTime = dateTime;
                mTriggerChanged = true;
            }
            if (mReminderActive == ACTIVE_REMINDER)
            {
                activate_reminder(false);
                mTriggerChanged = true;
            }
        }
        if (mDeferral != REMINDER_DEFERRAL)
        {
            // We're deferring the main alarm.
            // Main alarm has now expired.
            mNextMainDateTime = mDeferralTime = dateTime;
            set_deferral(NORMAL_DEFERRAL);
            mTriggerChanged = true;
            checkReminderAfter = true;
            if (!mMainExpired)
            {
                // Mark the alarm as expired now
                mMainExpired = true;
                --mAlarmCount;
                if (mRepeatAtLogin)
                {
                    // Remove the repeat-at-login alarm, but keep a note of it for archiving purposes
                    mArchiveRepeatAtLogin = true;
                    mRepeatAtLogin = false;
                    --mAlarmCount;
                }
            }
        }
    }
    else if (reminder)
    {
        // Deferring a reminder for a recurring alarm
        if (dateTime >= mNextMainDateTime.effectiveKDateTime())
        {
            // Trying to defer it past the next main alarm (regardless of whether
            // the reminder triggered before or after the main alarm).
            set_deferral(NO_DEFERRAL);    // (error)
        }
        else
        {
            set_deferral(REMINDER_DEFERRAL);
            mDeferralTime = dateTime;
            checkRepetition = true;
        }
        mTriggerChanged = true;
    }
    else
    {
        // Deferring a recurring alarm
        mDeferralTime = dateTime;
        if (mDeferral == NO_DEFERRAL)
            set_deferral(NORMAL_DEFERRAL);
        mTriggerChanged = true;
        checkReminderAfter = true;
        if (adjustRecurrence)
        {
            const KDateTime now = KDateTime::currentUtcDateTime();
            if (mainEndRepeatTime() < now)
            {
                // The last repetition (if any) of the current recurrence has already passed.
                // Adjust to the next scheduled recurrence after now.
                if (!mMainExpired  &&  setNextOccurrence(now) == NO_OCCURRENCE)
                {
                    mMainExpired = true;
                    --mAlarmCount;
                }
            }
            else
                setNextRepetition = mRepetition;
        }
        else
            checkRepetition = true;
    }
    if (checkReminderAfter  &&  mReminderMinutes < 0  &&  mReminderActive != NO_REMINDER)
    {
        // Enable/disable the active reminder AFTER the main alarm,
        // depending on whether the deferral is before or after the reminder.
        mReminderActive = (mDeferralTime < mReminderAfterTime) ? ACTIVE_REMINDER : HIDDEN_REMINDER;
    }
    if (checkRepetition)
        setNextRepetition = (mRepetition  &&  mDeferralTime < mainEndRepeatTime());
    if (setNextRepetition)
    {
        // The alarm is repeated, and we're deferring to a time before the last repetition.
        // Set the next scheduled repetition to the one after the deferral.
        if (mNextMainDateTime >= mDeferralTime)
            mNextRepeat = 0;
        else
            mNextRepeat = mRepetition.nextRepeatCount(mNextMainDateTime.kDateTime(), mDeferralTime.kDateTime());
        mTriggerChanged = true;
    }
    endChanges();
}

/******************************************************************************
* Cancel any deferral alarm.
*/
void KAEvent::cancelDefer()
{
    d->cancelDefer();
}

void KAEvent::Private::cancelDefer()
{
    if (mDeferral != NO_DEFERRAL)
    {
        mDeferralTime = DateTime();
        set_deferral(NO_DEFERRAL);
        mTriggerChanged = true;
    }
}

void KAEvent::setDeferDefaultMinutes(int minutes, bool dateOnly)
{
    d->mDeferDefaultMinutes = minutes;
    d->mDeferDefaultDateOnly = dateOnly;
}

bool KAEvent::deferred() const
{
    return d->mDeferral > 0;
}

DateTime KAEvent::deferDateTime() const
{
    return d->mDeferralTime;
}

/******************************************************************************
* Find the latest time which the alarm can currently be deferred to.
*/
DateTime KAEvent::deferralLimit(DeferLimitType* limitType) const
{
    return d->deferralLimit(limitType);
}

DateTime KAEvent::Private::deferralLimit(DeferLimitType* limitType) const
{
    DeferLimitType ltype = LIMIT_NONE;
    DateTime endTime;
    if (checkRecur() != KARecurrence::NO_RECUR)
    {
        // It's a recurring alarm. Find the latest time it can be deferred to:
        // it cannot be deferred past its next occurrence or sub-repetition,
        // or any advance reminder before that.
        DateTime reminderTime;
        const KDateTime now = KDateTime::currentUtcDateTime();
        const OccurType type = nextOccurrence(now, endTime, RETURN_REPETITION);
        if (type & OCCURRENCE_REPEAT)
            ltype = LIMIT_REPETITION;
        else if (type == NO_OCCURRENCE)
            ltype = LIMIT_NONE;
        else if (mReminderActive == ACTIVE_REMINDER  &&  mReminderMinutes > 0
             &&  (now < (reminderTime = endTime.addMins(-mReminderMinutes))))
        {
            endTime = reminderTime;
            ltype = LIMIT_REMINDER;
        }
        else
            ltype = LIMIT_RECURRENCE;
    }
    else if (mReminderMinutes < 0)
    {
        // There is a reminder alarm which occurs AFTER the main alarm.
        // Don't allow the reminder to be deferred past the next main alarm time.
        if (KDateTime::currentUtcDateTime() < mNextMainDateTime.effectiveKDateTime())
        {
            endTime = mNextMainDateTime;
            ltype = LIMIT_MAIN;
        }
    }
    else if (mReminderMinutes > 0
         &&  KDateTime::currentUtcDateTime() < mNextMainDateTime.effectiveKDateTime())
    {
        // It's a reminder BEFORE the main alarm.
        // Don't allow it to be deferred past its main alarm time.
        endTime = mNextMainDateTime;
        ltype = LIMIT_MAIN;
    }
    if (ltype != LIMIT_NONE)
        endTime = endTime.addMins(-1);
    if (limitType)
        *limitType = ltype;
    return endTime;
}

int KAEvent::deferDefaultMinutes() const
{
    return d->mDeferDefaultMinutes;
}

bool KAEvent::deferDefaultDateOnly() const
{
    return d->mDeferDefaultDateOnly;
}

DateTime KAEvent::startDateTime() const
{
    return d->mStartDateTime;
}

void KAEvent::setTime(const KDateTime& dt)
{
    d->mNextMainDateTime = dt;
    d->mTriggerChanged = true;
}

DateTime KAEvent::mainDateTime(bool withRepeats) const
{
    return d->mainDateTime(withRepeats);
}

QDate KAEvent::mainDate() const
{
    return d->mNextMainDateTime.date();
}

QTime KAEvent::mainTime() const
{
    return d->mNextMainDateTime.effectiveTime();
}

DateTime KAEvent::mainEndRepeatTime() const
{
    return d->mainEndRepeatTime();
}

/******************************************************************************
* Set the start-of-day time for date-only alarms.
*/
void KAEvent::setStartOfDay(const QTime& startOfDay)
{
    DateTime::setStartOfDay(startOfDay);
#ifdef __GNUC__
#warning Does this need all trigger times for date-only alarms to be recalculated?
#endif
}

/******************************************************************************
* Called when the user changes the start-of-day time.
* Adjust the start time of the recurrence to match, for each date-only event in
* a list.
*/
void KAEvent::adjustStartOfDay(const KAEvent::List& events)
{
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
        Private* const p = events[i]->d;
        if (p->mStartDateTime.isDateOnly()  &&  p->checkRecur() != KARecurrence::NO_RECUR)
            p->mRecurrence->setStartDateTime(p->mStartDateTime.effectiveKDateTime(), true);
    }
}

DateTime KAEvent::nextTrigger(TriggerType type) const
{
    d->calcTriggerTimes();
    switch (type)
    {
        case ALL_TRIGGER:       return d->mAllTrigger;
        case MAIN_TRIGGER:      return d->mMainTrigger;
        case ALL_WORK_TRIGGER:  return d->mAllWorkTrigger;
        case WORK_TRIGGER:      return d->mMainWorkTrigger;
        case DISPLAY_TRIGGER:
        {
            const bool reminderAfter = d->mMainExpired && d->mReminderActive && d->mReminderMinutes < 0;
            return (d->mWorkTimeOnly || d->mExcludeHolidays)
                   ? (reminderAfter ? d->mAllWorkTrigger : d->mMainWorkTrigger)
                   : (reminderAfter ? d->mAllTrigger : d->mMainTrigger);
        }
        default:                return DateTime();
    }
}

void KAEvent::setCreatedDateTime(const KDateTime& dt)
{
    d->mCreatedDateTime = dt;
}

KDateTime KAEvent::createdDateTime() const
{
    return d->mCreatedDateTime;
}

/******************************************************************************
* Set or clear repeat-at-login.
*/
void KAEvent::setRepeatAtLogin(bool rl)
{
    d->setRepeatAtLogin(rl);
}

void KAEvent::Private::setRepeatAtLogin(bool rl)
{
    if (rl  &&  !mRepeatAtLogin)
    {
        setRepeatAtLoginTrue(true);   // clear incompatible statuses
        ++mAlarmCount;
    }
    else if (!rl  &&  mRepeatAtLogin)
        --mAlarmCount;
    mRepeatAtLogin = rl;
    mTriggerChanged = true;
}

/******************************************************************************
* Clear incompatible statuses when repeat-at-login is set.
*/
void KAEvent::Private::setRepeatAtLoginTrue(bool clearReminder)
{
    clearRecur();                // clear recurrences
    if (mReminderMinutes >= 0 && clearReminder)
        setReminder(0, false);   // clear pre-alarm reminder
    mLateCancel = 0;
    mAutoClose = false;
    mCopyToKOrganizer = false;
}

bool KAEvent::repeatAtLogin(bool includeArchived) const
{
    return d->mRepeatAtLogin || (includeArchived && d->mArchiveRepeatAtLogin);
}

void KAEvent::setExcludeHolidays(bool ex)
{
    d->mExcludeHolidays = ex ? Private::mHolidays : 0;
    d->mTriggerChanged = true;
}

bool KAEvent::holidaysExcluded() const
{
    return d->mExcludeHolidays;
}

/******************************************************************************
* Set a new holiday region.
* Alarms which exclude holidays record the pointer to the holiday definition
* at the time their next trigger times were last calculated. The change in
* holiday definition pointer will cause their next trigger times to be
* recalculated.
*/
void KAEvent::setHolidays(const HolidayRegion& h)
{
    Private::mHolidays = &h;
}

void KAEvent::setWorkTimeOnly(bool wto)
{
    d->mWorkTimeOnly = wto;
    d->mTriggerChanged = true;
}

bool KAEvent::workTimeOnly() const
{
    return d->mWorkTimeOnly;
}

/******************************************************************************
* Check whether a date/time is during working hours and/or holidays, depending
* on the flags set for the specified event.
*/
bool KAEvent::isWorkingTime(const KDateTime& dt) const
{
    return d->isWorkingTime(dt);
}

bool KAEvent::Private::isWorkingTime(const KDateTime& dt) const
{
    if ((mWorkTimeOnly  &&  !mWorkDays.testBit(dt.date().dayOfWeek() - 1))
    ||  (mExcludeHolidays  &&  mHolidays  &&  mHolidays->isHoliday(dt.date())))
        return false;
    if (!mWorkTimeOnly)
        return true;
    return dt.isDateOnly()
       ||  (dt.time() >= mWorkDayStart  &&  dt.time() < mWorkDayEnd);
}

/******************************************************************************
* Set new working days and times.
* Increment a counter so that working-time-only alarms can detect that they
* need to update their next trigger time.
*/
void KAEvent::setWorkTime(const QBitArray& days, const QTime& start, const QTime& end)
{
    if (days != Private::mWorkDays  ||  start != Private::mWorkDayStart  ||  end != Private::mWorkDayEnd)
    {
        Private::mWorkDays     = days;
        Private::mWorkDayStart = start;
        Private::mWorkDayEnd   = end;
        if (!++Private::mWorkTimeIndex)
            ++Private::mWorkTimeIndex;
    }
}

/******************************************************************************
* Clear the event's recurrence and alarm repetition data.
*/
void KAEvent::setNoRecur()
{
    d->clearRecur();
}

void KAEvent::Private::clearRecur()
{
    if (mRecurrence || mRepetition)
    {
        delete mRecurrence;
        mRecurrence = 0;
        mRepetition.set(0, 0);
        mTriggerChanged = true;
    }
    mNextRepeat = 0;
}

/******************************************************************************
* Initialise the event's recurrence from a KCal::Recurrence.
* The event's start date/time is not changed.
*/
void KAEvent::setRecurrence(const KARecurrence& recurrence)
{
    d->setRecurrence(recurrence);
}

void KAEvent::Private::setRecurrence(const KARecurrence& recurrence)
{
    startChanges();   // prevent multiple trigger time evaluation here
    delete mRecurrence;
    if (recurrence.recurs())
    {
        mRecurrence = new KARecurrence(recurrence);
        mRecurrence->setStartDateTime(mStartDateTime.effectiveKDateTime(), mStartDateTime.isDateOnly());
        mTriggerChanged = true;
    }
    else
    {
        if (mRecurrence)
            mTriggerChanged = true;
        mRecurrence = 0;
    }

    // Adjust sub-repetition values to fit the recurrence.
    setRepetition(mRepetition);

    endChanges();
}

/******************************************************************************
* Set the recurrence to recur at a minutes interval.
* Parameters:
*    freq  = how many minutes between recurrences.
*    count = number of occurrences, including first and last.
*          = -1 to recur indefinitely.
*          = 0 to use 'end' instead.
*    end   = end date/time (invalid to use 'count' instead).
* Reply = false if no recurrence was set up.
*/
bool KAEvent::setRecurMinutely(int freq, int count, const KDateTime& end)
{
    const bool success = d->setRecur(RecurrenceRule::rMinutely, freq, count, end);
    d->mTriggerChanged = true;
    return success;
}

/******************************************************************************
* Set the recurrence to recur daily.
* Parameters:
*    freq  = how many days between recurrences.
*    days  = which days of the week alarms are allowed to occur on.
*    count = number of occurrences, including first and last.
*          = -1 to recur indefinitely.
*          = 0 to use 'end' instead.
*    end   = end date (invalid to use 'count' instead).
* Reply = false if no recurrence was set up.
*/
bool KAEvent::setRecurDaily(int freq, const QBitArray& days, int count, const QDate& end)
{
    const bool success = d->setRecur(RecurrenceRule::rDaily, freq, count, end);
    if (success)
    {
        int n = 0;
        for (int i = 0;  i < 7;  ++i)
        {
            if (days.testBit(i))
                ++n;
        }
        if (n < 7)
            d->mRecurrence->addWeeklyDays(days);
    }
    d->mTriggerChanged = true;
    return success;
}

/******************************************************************************
* Set the recurrence to recur weekly, on the specified weekdays.
* Parameters:
*    freq  = how many weeks between recurrences.
*    days  = which days of the week alarms should occur on.
*    count = number of occurrences, including first and last.
*          = -1 to recur indefinitely.
*          = 0 to use 'end' instead.
*    end   = end date (invalid to use 'count' instead).
* Reply = false if no recurrence was set up.
*/
bool KAEvent::setRecurWeekly(int freq, const QBitArray& days, int count, const QDate& end)
{
    const bool success = d->setRecur(RecurrenceRule::rWeekly, freq, count, end);
    if (success)
        d->mRecurrence->addWeeklyDays(days);
    d->mTriggerChanged = true;
    return success;
}

/******************************************************************************
* Set the recurrence to recur monthly, on the specified days within the month.
* Parameters:
*    freq  = how many months between recurrences.
*    days  = which days of the month alarms should occur on.
*    count = number of occurrences, including first and last.
*          = -1 to recur indefinitely.
*          = 0 to use 'end' instead.
*    end   = end date (invalid to use 'count' instead).
* Reply = false if no recurrence was set up.
*/
bool KAEvent::setRecurMonthlyByDate(int freq, const QVector<int>& days, int count, const QDate& end)
{
    const bool success = d->setRecur(RecurrenceRule::rMonthly, freq, count, end);
    if (success)
    {
        for (int i = 0, end = days.count();  i < end;  ++i)
            d->mRecurrence->addMonthlyDate(days[i]);
    }
    d->mTriggerChanged = true;
    return success;
}

/******************************************************************************
* Set the recurrence to recur monthly, on the specified weekdays in the
* specified weeks of the month.
* Parameters:
*    freq  = how many months between recurrences.
*    posns = which days of the week/weeks of the month alarms should occur on.
*    count = number of occurrences, including first and last.
*          = -1 to recur indefinitely.
*          = 0 to use 'end' instead.
*    end   = end date (invalid to use 'count' instead).
* Reply = false if no recurrence was set up.
*/
bool KAEvent::setRecurMonthlyByPos(int freq, const QVector<MonthPos>& posns, int count, const QDate& end)
{
    const bool success = d->setRecur(RecurrenceRule::rMonthly, freq, count, end);
    if (success)
    {
        for (int i = 0, end = posns.count();  i < end;  ++i)
            d->mRecurrence->addMonthlyPos(posns[i].weeknum, posns[i].days);
    }
    d->mTriggerChanged = true;
    return success;
}

/******************************************************************************
* Set the recurrence to recur annually, on the specified start date in each
* of the specified months.
* Parameters:
*    freq   = how many years between recurrences.
*    months = which months of the year alarms should occur on.
*    day    = day of month, or 0 to use start date
*    feb29  = when February 29th should recur in non-leap years.
*    count  = number of occurrences, including first and last.
*           = -1 to recur indefinitely.
*           = 0 to use 'end' instead.
*    end    = end date (invalid to use 'count' instead).
* Reply = false if no recurrence was set up.
*/
bool KAEvent::setRecurAnnualByDate(int freq, const QVector<int>& months, int day, KARecurrence::Feb29Type feb29, int count, const QDate& end)
{
    const bool success = d->setRecur(RecurrenceRule::rYearly, freq, count, end, feb29);
    if (success)
    {
        for (int i = 0, end = months.count();  i < end;  ++i)
            d->mRecurrence->addYearlyMonth(months[i]);
        if (day)
            d->mRecurrence->addMonthlyDate(day);
    }
    d->mTriggerChanged = true;
    return success;
}

/******************************************************************************
* Set the recurrence to recur annually, on the specified weekdays in the
* specified weeks of the specified months.
* Parameters:
*    freq   = how many years between recurrences.
*    posns  = which days of the week/weeks of the month alarms should occur on.
*    months = which months of the year alarms should occur on.
*    count  = number of occurrences, including first and last.
*           = -1 to recur indefinitely.
*           = 0 to use 'end' instead.
*    end    = end date (invalid to use 'count' instead).
* Reply = false if no recurrence was set up.
*/
bool KAEvent::setRecurAnnualByPos(int freq, const QVector<MonthPos>& posns, const QVector<int>& months, int count, const QDate& end)
{
    const bool success = d->setRecur(RecurrenceRule::rYearly, freq, count, end);
    if (success)
    {
        int i = 0;
        int iend;
        for (iend = months.count();  i < iend;  ++i)
            d->mRecurrence->addYearlyMonth(months[i]);
        for (i = 0, iend = posns.count();  i < iend;  ++i)
            d->mRecurrence->addYearlyPos(posns[i].weeknum, posns[i].days);
    }
    d->mTriggerChanged = true;
    return success;
}

/******************************************************************************
* Initialise the event's recurrence data.
* Parameters:
*    freq  = how many intervals between recurrences.
*    count = number of occurrences, including first and last.
*          = -1 to recur indefinitely.
*          = 0 to use 'end' instead.
*    end   = end date/time (invalid to use 'count' instead).
* Reply = false if no recurrence was set up.
*/
bool KAEvent::Private::setRecur(RecurrenceRule::PeriodType recurType, int freq, int count, const QDate& end, KARecurrence::Feb29Type feb29)
{
    KDateTime edt = mNextMainDateTime.kDateTime();
    edt.setDate(end);
    return setRecur(recurType, freq, count, edt, feb29);
}
bool KAEvent::Private::setRecur(RecurrenceRule::PeriodType recurType, int freq, int count, const KDateTime& end, KARecurrence::Feb29Type feb29)
{
    if (count >= -1  &&  (count || end.date().isValid()))
    {
        if (!mRecurrence)
            mRecurrence = new KARecurrence;
        if (mRecurrence->init(recurType, freq, count, mNextMainDateTime.kDateTime(), end, feb29))
            return true;
    }
    clearRecur();
    return false;
}

bool KAEvent::recurs() const
{
    return d->checkRecur() != KARecurrence::NO_RECUR;
}

KARecurrence::Type KAEvent::recurType() const
{
    return d->checkRecur();
}

KARecurrence* KAEvent::recurrence() const
{
    return d->mRecurrence;
}

/******************************************************************************
* Return the recurrence interval in units of the recurrence period type.
*/
int KAEvent::recurInterval() const
{
    if (d->mRecurrence)
    {
        switch (d->mRecurrence->type())
        {
            case KARecurrence::MINUTELY:
            case KARecurrence::DAILY:
            case KARecurrence::WEEKLY:
            case KARecurrence::MONTHLY_DAY:
            case KARecurrence::MONTHLY_POS:
            case KARecurrence::ANNUAL_DATE:
            case KARecurrence::ANNUAL_POS:
                return d->mRecurrence->frequency();
            default:
                break;
        }
    }
    return 0;
}

Duration KAEvent::longestRecurrenceInterval() const
{
    return d->mRecurrence ? d->mRecurrence->longestInterval() : Duration(0);
}

/******************************************************************************
* Adjust the event date/time to the first recurrence of the event, on or after
* start date/time. The event start date may not be a recurrence date, in which
* case a later date will be set.
*/
void KAEvent::setFirstRecurrence()
{
    d->setFirstRecurrence();
}

void KAEvent::Private::setFirstRecurrence()
{
    switch (checkRecur())
    {
        case KARecurrence::NO_RECUR:
        case KARecurrence::MINUTELY:
            return;
        case KARecurrence::ANNUAL_DATE:
        case KARecurrence::ANNUAL_POS:
            if (mRecurrence->yearMonths().isEmpty())
                return;    // (presumably it's a template)
            break;
        case KARecurrence::DAILY:
        case KARecurrence::WEEKLY:
        case KARecurrence::MONTHLY_POS:
        case KARecurrence::MONTHLY_DAY:
            break;
    }
    const KDateTime recurStart = mRecurrence->startDateTime();
    if (mRecurrence->recursOn(recurStart.date(), recurStart.timeSpec()))
        return;           // it already recurs on the start date

    // Set the frequency to 1 to find the first possible occurrence
    const int frequency = mRecurrence->frequency();
    mRecurrence->setFrequency(1);
    DateTime next;
    nextRecurrence(mNextMainDateTime.effectiveKDateTime(), next);
    if (!next.isValid())
        mRecurrence->setStartDateTime(recurStart, mStartDateTime.isDateOnly());   // reinstate the old value
    else
    {
        mRecurrence->setStartDateTime(next.effectiveKDateTime(), next.isDateOnly());
        mStartDateTime = mNextMainDateTime = next;
        mTriggerChanged = true;
    }
    mRecurrence->setFrequency(frequency);    // restore the frequency
}

/******************************************************************************
* Return the recurrence interval as text suitable for display.
*/
QString KAEvent::recurrenceText(bool brief) const
{
    if (d->mRepeatAtLogin)
        return brief ? i18nc("@info/plain Brief form of 'At Login'", "Login") : i18nc("@info/plain", "At login");
    if (d->mRecurrence)
    {
        const int frequency = d->mRecurrence->frequency();
        switch (d->mRecurrence->defaultRRuleConst()->recurrenceType())
        {
            case RecurrenceRule::rMinutely:
                if (frequency < 60)
                    return i18ncp("@info/plain", "1 Minute", "%1 Minutes", frequency);
                else if (frequency % 60 == 0)
                    return i18ncp("@info/plain", "1 Hour", "%1 Hours", frequency/60);
                else
                {
                    QString mins;
                    return i18nc("@info/plain Hours and minutes", "%1h %2m", frequency/60, mins.sprintf("%02d", frequency%60));
                }
            case RecurrenceRule::rDaily:
                return i18ncp("@info/plain", "1 Day", "%1 Days", frequency);
            case RecurrenceRule::rWeekly:
                return i18ncp("@info/plain", "1 Week", "%1 Weeks", frequency);
            case RecurrenceRule::rMonthly:
                return i18ncp("@info/plain", "1 Month", "%1 Months", frequency);
            case RecurrenceRule::rYearly:
                return i18ncp("@info/plain", "1 Year", "%1 Years", frequency);
            case RecurrenceRule::rNone:
            default:
                break;
        }
    }
    return brief ? QString() : i18nc("@info/plain No recurrence", "None");
}

/******************************************************************************
* Initialise the event's sub-repetition.
* The repetition length is adjusted if necessary to fit the recurrence interval.
* If the event doesn't recur, the sub-repetition is cleared.
* Reply = false if a non-daily interval was specified for a date-only recurrence.
*/
bool KAEvent::setRepetition(const Repetition& r)
{
    return d->setRepetition(r);
}

bool KAEvent::Private::setRepetition(const Repetition& repetition)
{
    // Don't set mRepetition to zero at the start of this function, in case the
    // 'repetition' parameter passed in is a reference to mRepetition.
    mNextRepeat = 0;
    if (repetition  &&  !mRepeatAtLogin)
    {
        Q_ASSERT(checkRecur() != KARecurrence::NO_RECUR);
        if (!repetition.isDaily()  &&  mStartDateTime.isDateOnly())
        {
            mRepetition.set(0, 0);
            return false;    // interval must be in units of days for date-only alarms
        }
        Duration longestInterval = mRecurrence->longestInterval();
        if (repetition.duration() >= longestInterval)
        {
            const int count = mStartDateTime.isDateOnly()
                              ? (longestInterval.asDays() - 1) / repetition.intervalDays()
                              : (longestInterval.asSeconds() - 1) / repetition.intervalSeconds();
            mRepetition.set(repetition.interval(), count);
        }
        else
            mRepetition = repetition;
        mTriggerChanged = true;
    }
    else if (mRepetition)
    {
        mRepetition.set(0, 0);
        mTriggerChanged = true;
    }
    return true;
}

Repetition KAEvent::repetition() const
{
    return d->mRepetition;
}

int KAEvent::nextRepetition() const
{
    return d->mNextRepeat;
}

/******************************************************************************
* Return the repetition interval as text suitable for display.
*/
QString KAEvent::repetitionText(bool brief) const
{
    if (d->mRepetition)
    {
        if (!d->mRepetition.isDaily())
        {
            const int minutes = d->mRepetition.intervalMinutes();
            if (minutes < 60)
                return i18ncp("@info/plain", "1 Minute", "%1 Minutes", minutes);
            if (minutes % 60 == 0)
                return i18ncp("@info/plain", "1 Hour", "%1 Hours", minutes/60);
            QString mins;
            return i18nc("@info/plain Hours and minutes", "%1h %2m", minutes/60, mins.sprintf("%02d", minutes%60));
        }
        const int days = d->mRepetition.intervalDays();
        if (days % 7)
            return i18ncp("@info/plain", "1 Day", "%1 Days", days);
        return i18ncp("@info/plain", "1 Week", "%1 Weeks", days / 7);
    }
    return brief ? QString() : i18nc("@info/plain No repetition", "None");
}

/******************************************************************************
* Determine whether the event will occur after the specified date/time.
* If 'includeRepetitions' is true and the alarm has a sub-repetition, it
* returns true if any repetitions occur after the specified date/time.
*/
bool KAEvent::occursAfter(const KDateTime& preDateTime, bool includeRepetitions) const
{
    return d->occursAfter(preDateTime, includeRepetitions);
}

bool KAEvent::Private::occursAfter(const KDateTime& preDateTime, bool includeRepetitions) const
{
    KDateTime dt;
    if (checkRecur() != KARecurrence::NO_RECUR)
    {
        if (mRecurrence->duration() < 0)
            return true;    // infinite recurrence
        dt = mRecurrence->endDateTime();
    }
    else
        dt = mNextMainDateTime.effectiveKDateTime();
    if (mStartDateTime.isDateOnly())
    {
        QDate pre = preDateTime.date();
        if (preDateTime.toTimeSpec(mStartDateTime.timeSpec()).time() < DateTime::startOfDay())
            pre = pre.addDays(-1);    // today's recurrence (if today recurs) is still to come
        if (pre < dt.date())
            return true;
    }
    else if (preDateTime < dt)
        return true;

    if (includeRepetitions  &&  mRepetition)
    {
        if (preDateTime < mRepetition.duration().end(dt))
            return true;
    }
    return false;
}

/******************************************************************************
* Set the date/time of the event to the next scheduled occurrence after the
* specified date/time, provided that this is later than its current date/time.
* Any reminder alarm is adjusted accordingly.
* If the alarm has a sub-repetition, and a repetition of a previous recurrence
* occurs after the specified date/time, that repetition is set as the next
* occurrence.
*/
KAEvent::OccurType KAEvent::setNextOccurrence(const KDateTime& preDateTime)
{
    return d->setNextOccurrence(preDateTime);
}

KAEvent::OccurType KAEvent::Private::setNextOccurrence(const KDateTime& preDateTime)
{
    if (preDateTime < mNextMainDateTime.effectiveKDateTime())
        return FIRST_OR_ONLY_OCCURRENCE;    // it might not be the first recurrence - tant pis
    KDateTime pre = preDateTime;
    // If there are repetitions, adjust the comparison date/time so that
    // we find the earliest recurrence which has a repetition falling after
    // the specified preDateTime.
    if (mRepetition)
        pre = mRepetition.duration(-mRepetition.count()).end(preDateTime);

    DateTime afterPre;          // next recurrence after 'pre'
    OccurType type;
    if (pre < mNextMainDateTime.effectiveKDateTime())
    {
        afterPre = mNextMainDateTime;
        type = FIRST_OR_ONLY_OCCURRENCE;   // may not actually be the first occurrence
    }
    else if (checkRecur() != KARecurrence::NO_RECUR)
    {
        type = nextRecurrence(pre, afterPre);
        if (type == NO_OCCURRENCE)
            return NO_OCCURRENCE;
        if (type != FIRST_OR_ONLY_OCCURRENCE  &&  afterPre != mNextMainDateTime)
        {
            // Need to reschedule the next trigger date/time
            mNextMainDateTime = afterPre;
            if (mReminderMinutes > 0  &&  (mDeferral == REMINDER_DEFERRAL || mReminderActive != ACTIVE_REMINDER))
            {
                // Reinstate the advance reminder for the rescheduled recurrence.
                // Note that a reminder AFTER the main alarm will be left active.
                activate_reminder(!mReminderOnceOnly);
            }
            if (mDeferral == REMINDER_DEFERRAL)
                set_deferral(NO_DEFERRAL);
            mTriggerChanged = true;
        }
    }
    else
        return NO_OCCURRENCE;

    if (mRepetition)
    {
        if (afterPre <= preDateTime)
        {
            // The next occurrence is a sub-repetition.
            type = static_cast<OccurType>(type | OCCURRENCE_REPEAT);
            mNextRepeat = mRepetition.nextRepeatCount(afterPre.effectiveKDateTime(), preDateTime);
            // Repetitions can't have a reminder, so remove any.
            activate_reminder(false);
            if (mDeferral == REMINDER_DEFERRAL)
                set_deferral(NO_DEFERRAL);
            mTriggerChanged = true;
        }
        else if (mNextRepeat)
        {
            // The next occurrence is the main occurrence, not a repetition
            mNextRepeat = 0;
            mTriggerChanged = true;
        }
    }
    return type;
}

/******************************************************************************
* Get the date/time of the next occurrence of the event, after the specified
* date/time.
* 'result' = date/time of next occurrence, or invalid date/time if none.
*/
KAEvent::OccurType KAEvent::nextOccurrence(const KDateTime& preDateTime, DateTime& result, OccurOption o) const
{
    return d->nextOccurrence(preDateTime, result, o);
}

KAEvent::OccurType KAEvent::Private::nextOccurrence(const KDateTime& preDateTime, DateTime& result,
                                                    OccurOption includeRepetitions) const
{
    KDateTime pre = preDateTime;
    if (includeRepetitions != IGNORE_REPETITION)
    {                   // RETURN_REPETITION or ALLOW_FOR_REPETITION
        if (!mRepetition)
            includeRepetitions = IGNORE_REPETITION;
        else
            pre = mRepetition.duration(-mRepetition.count()).end(preDateTime);
    }

    OccurType type;
    const bool recurs = (checkRecur() != KARecurrence::NO_RECUR);
    if (recurs)
        type = nextRecurrence(pre, result);
    else if (pre < mNextMainDateTime.effectiveKDateTime())
    {
        result = mNextMainDateTime;
        type = FIRST_OR_ONLY_OCCURRENCE;
    }
    else
    {
        result = DateTime();
        type = NO_OCCURRENCE;
    }

    if (type != NO_OCCURRENCE  &&  result <= preDateTime  &&  includeRepetitions != IGNORE_REPETITION)
    {                   // RETURN_REPETITION or ALLOW_FOR_REPETITION
        // The next occurrence is a sub-repetition
        int repetition = mRepetition.nextRepeatCount(result.kDateTime(), preDateTime);
        const DateTime repeatDT = mRepetition.duration(repetition).end(result.kDateTime());
        if (recurs)
        {
            // We've found a recurrence before the specified date/time, which has
            // a sub-repetition after the date/time.
            // However, if the intervals between recurrences vary, we could possibly
            // have missed a later recurrence which fits the criterion, so check again.
            DateTime dt;
            const OccurType newType = previousOccurrence(repeatDT.effectiveKDateTime(), dt, false);
            if (dt > result)
            {
                type = newType;
                result = dt;
                if (includeRepetitions == RETURN_REPETITION  &&  result <= preDateTime)
                {
                    // The next occurrence is a sub-repetition
                    repetition = mRepetition.nextRepeatCount(result.kDateTime(), preDateTime);
                    result = mRepetition.duration(repetition).end(result.kDateTime());
                    type = static_cast<OccurType>(type | OCCURRENCE_REPEAT);
                }
                return type;
            }
        }
        if (includeRepetitions == RETURN_REPETITION)
        {
            // The next occurrence is a sub-repetition
            result = repeatDT;
            type = static_cast<OccurType>(type | OCCURRENCE_REPEAT);
        }
    }
    return type;
}

/******************************************************************************
* Get the date/time of the last previous occurrence of the event, before the
* specified date/time.
* If 'includeRepetitions' is true and the alarm has a sub-repetition, the
* last previous repetition is returned if appropriate.
* 'result' = date/time of previous occurrence, or invalid date/time if none.
*/
KAEvent::OccurType KAEvent::previousOccurrence(const KDateTime& afterDateTime, DateTime& result, bool includeRepetitions) const
{
    return d->previousOccurrence(afterDateTime, result, includeRepetitions);
}

KAEvent::OccurType KAEvent::Private::previousOccurrence(const KDateTime& afterDateTime, DateTime& result,
                                                        bool includeRepetitions) const
{
    Q_ASSERT(!afterDateTime.isDateOnly());
    if (mStartDateTime >= afterDateTime)
    {
        result = KDateTime();
        return NO_OCCURRENCE;     // the event starts after the specified date/time
    }

    // Find the latest recurrence of the event
    OccurType type;
    if (checkRecur() == KARecurrence::NO_RECUR)
    {
        result = mStartDateTime;
        type = FIRST_OR_ONLY_OCCURRENCE;
    }
    else
    {
        const KDateTime recurStart = mRecurrence->startDateTime();
        KDateTime after = afterDateTime.toTimeSpec(mStartDateTime.timeSpec());
        if (mStartDateTime.isDateOnly()  &&  afterDateTime.time() > DateTime::startOfDay())
            after = after.addDays(1);    // today's recurrence (if today recurs) has passed
        const KDateTime dt = mRecurrence->getPreviousDateTime(after);
        result = dt;
        result.setDateOnly(mStartDateTime.isDateOnly());
        if (!dt.isValid())
            return NO_OCCURRENCE;
        if (dt == recurStart)
            type = FIRST_OR_ONLY_OCCURRENCE;
        else if (mRecurrence->getNextDateTime(dt).isValid())
            type = result.isDateOnly() ? RECURRENCE_DATE : RECURRENCE_DATE_TIME;
        else
            type = LAST_RECURRENCE;
    }

    if (includeRepetitions  &&  mRepetition)
    {
        // Find the latest repetition which is before the specified time.
        const int repetition = mRepetition.previousRepeatCount(result.effectiveKDateTime(), afterDateTime);
        if (repetition > 0)
        {
            result = mRepetition.duration(qMin(repetition, mRepetition.count())).end(result.kDateTime());
            return static_cast<OccurType>(type | OCCURRENCE_REPEAT);
        }
    }
    return type;
}

/******************************************************************************
* Set the event to be a copy of the specified event, making the specified
* alarm the 'displaying' alarm.
* The purpose of setting up a 'displaying' alarm is to be able to reinstate
* the alarm message in case of a crash, or to reinstate it should the user
* choose to defer the alarm. Note that even repeat-at-login alarms need to be
* saved in case their end time expires before the next login.
* Reply = true if successful, false if alarm was not copied.
*/
#ifdef USE_AKONADI
bool KAEvent::setDisplaying(const KAEvent& e, KAAlarm::Type t, Akonadi::Collection::Id id, const KDateTime& dt, bool showEdit, bool showDefer)
#else
bool KAEvent::setDisplaying(const KAEvent& e, KAAlarm::Type t, const QString& id, const KDateTime& dt, bool showEdit, bool showDefer)
#endif
{
    return d->setDisplaying(*e.d, t, id, dt, showEdit, showDefer);
}

#ifdef USE_AKONADI
bool KAEvent::Private::setDisplaying(const KAEvent::Private& event, KAAlarm::Type alarmType, Akonadi::Collection::Id collectionId,
                                     const KDateTime& repeatAtLoginTime, bool showEdit, bool showDefer)
#else
bool KAEvent::Private::setDisplaying(const KAEvent::Private& event, KAAlarm::Type alarmType, const QString& resourceID,
                                     const KDateTime& repeatAtLoginTime, bool showEdit, bool showDefer)
#endif
{
    if (!mDisplaying
    &&  (alarmType == KAAlarm::MAIN_ALARM
      || alarmType == KAAlarm::REMINDER_ALARM
      || alarmType == KAAlarm::DEFERRED_REMINDER_ALARM
      || alarmType == KAAlarm::DEFERRED_ALARM
      || alarmType == KAAlarm::AT_LOGIN_ALARM))
    {
//kDebug()<<event.id()<<","<<(alarmType==KAAlarm::MAIN_ALARM?"MAIN":alarmType==KAAlarm::REMINDER_ALARM?"REMINDER":alarmType==KAAlarm::DEFERRED_REMINDER_ALARM?"REMINDER_DEFERRAL":alarmType==KAAlarm::DEFERRED_ALARM?"DEFERRAL":"LOGIN")<<"): time="<<repeatAtLoginTime.toString();
        KAAlarm al = event.alarm(alarmType);
        if (al.isValid())
        {
            *this = event;
            // Change the event ID to avoid duplicating the same unique ID as the original event
            setCategory(KAlarm::CalEvent::DISPLAYING);
#ifdef USE_AKONADI
            mItemId               = -1;    // the display event doesn't have an associated Item
            mOriginalCollectionId = collectionId;;
#else
            mOriginalResourceId   = resourceID;
#endif
            mDisplayingDefer      = showDefer;
            mDisplayingEdit       = showEdit;
            mDisplaying           = true;
            mDisplayingTime       = (alarmType == KAAlarm::AT_LOGIN_ALARM) ? repeatAtLoginTime : al.dateTime().kDateTime();
            switch (al.type())
            {
                case KAAlarm::AT_LOGIN_ALARM:           mDisplayingFlags = REPEAT_AT_LOGIN;  break;
                case KAAlarm::REMINDER_ALARM:           mDisplayingFlags = REMINDER;  break;
                case KAAlarm::DEFERRED_REMINDER_ALARM:  mDisplayingFlags = al.timedDeferral() ? (REMINDER | TIME_DEFERRAL) : (REMINDER | DATE_DEFERRAL);  break;
                case KAAlarm::DEFERRED_ALARM:           mDisplayingFlags = al.timedDeferral() ? TIME_DEFERRAL : DATE_DEFERRAL;  break;
                default:                                mDisplayingFlags = 0;  break;
            }
            ++mAlarmCount;
            return true;
        }
    }
    return false;
}

/******************************************************************************
* Reinstate the original event from the 'displaying' event.
*/
#ifdef USE_AKONADI
void KAEvent::reinstateFromDisplaying(const KCalCore::ConstEventPtr& e, Akonadi::Collection::Id& id, bool& showEdit, bool& showDefer)
#else
void KAEvent::reinstateFromDisplaying(const KCal::Event* e, QString& id, bool& showEdit, bool& showDefer)
#endif
{
    d->reinstateFromDisplaying(e, id, showEdit, showDefer);
}

#ifdef USE_AKONADI
void KAEvent::Private::reinstateFromDisplaying(const ConstEventPtr& kcalEvent, Akonadi::Collection::Id& collectionId, bool& showEdit, bool& showDefer)
#else
void KAEvent::Private::reinstateFromDisplaying(const Event* kcalEvent, QString& resourceID, bool& showEdit, bool& showDefer)
#endif
{
    set(kcalEvent);
    if (mDisplaying)
    {
        // Retrieve the original event's unique ID
        setCategory(KAlarm::CalEvent::ACTIVE);
#ifdef USE_AKONADI
        collectionId = mOriginalCollectionId;
        mOriginalCollectionId = -1;
#else
        resourceID   = mOriginalResourceId;
        mOriginalResourceId.clear();
#endif
        showDefer    = mDisplayingDefer;
        showEdit     = mDisplayingEdit;
        mDisplaying  = false;
        --mAlarmCount;
    }
}

/******************************************************************************
* Return the original alarm which the displaying alarm refers to.
* Note that the caller is responsible for ensuring that the event was a
* displaying event, since this is normally called after
* reinstateFromDisplaying(), which clears mDisplaying.
*/
KAAlarm KAEvent::convertDisplayingAlarm() const
{
    KAAlarm al = alarm(KAAlarm::DISPLAYING_ALARM);
    KAAlarm::Private* const al_d = al.d;
    const int displayingFlags = d->mDisplayingFlags;
    if (displayingFlags & REPEAT_AT_LOGIN)
    {
        al_d->mRepeatAtLogin = true;
        al_d->mType = KAAlarm::AT_LOGIN_ALARM;
    }
    else if (displayingFlags & Private::DEFERRAL)
    {
        al_d->mDeferred = true;
        al_d->mTimedDeferral = (displayingFlags & Private::TIMED_FLAG);
        al_d->mType = (displayingFlags & Private::REMINDER) ? KAAlarm::DEFERRED_REMINDER_ALARM : KAAlarm::DEFERRED_ALARM;
    }
    else if (displayingFlags & Private::REMINDER)
        al_d->mType = KAAlarm::REMINDER_ALARM;
    else
        al_d->mType = KAAlarm::MAIN_ALARM;
    return al;
}

bool KAEvent::displaying() const
{
    return d->mDisplaying;
}

/******************************************************************************
* Return the alarm of the specified type.
*/
KAAlarm KAEvent::alarm(KAAlarm::Type t) const
{
    return d->alarm(t);
}

KAAlarm KAEvent::Private::alarm(KAAlarm::Type type) const
{
    checkRecur();     // ensure recurrence/repetition data is consistent
    KAAlarm al;       // this sets type to INVALID_ALARM
    KAAlarm::Private* const al_d = al.d;
    if (mAlarmCount)
    {
        al_d->mActionType    = (KAAlarm::Action)mActionSubType;
        al_d->mRepeatAtLogin = false;
        al_d->mDeferred      = false;
        switch (type)
        {
            case KAAlarm::MAIN_ALARM:
                if (!mMainExpired)
                {
                    al_d->mType             = KAAlarm::MAIN_ALARM;
                    al_d->mNextMainDateTime = mNextMainDateTime;
                    al_d->mRepetition       = mRepetition;
                    al_d->mNextRepeat       = mNextRepeat;
                }
                break;
            case KAAlarm::REMINDER_ALARM:
                if (mReminderActive == ACTIVE_REMINDER)
                {
                    al_d->mType = KAAlarm::REMINDER_ALARM;
                    if (mReminderMinutes < 0)
                        al_d->mNextMainDateTime = mReminderAfterTime;
                    else if (mReminderOnceOnly)
                        al_d->mNextMainDateTime = mStartDateTime.addMins(-mReminderMinutes);
                    else
                        al_d->mNextMainDateTime = mNextMainDateTime.addMins(-mReminderMinutes);
                }
                break;
            case KAAlarm::DEFERRED_REMINDER_ALARM:
                if (mDeferral != REMINDER_DEFERRAL)
                    break;
                // fall through to DEFERRED_ALARM
            case KAAlarm::DEFERRED_ALARM:
                if (mDeferral != NO_DEFERRAL)
                {
                    al_d->mType             = (mDeferral == REMINDER_DEFERRAL) ? KAAlarm::DEFERRED_REMINDER_ALARM : KAAlarm::DEFERRED_ALARM;
                    al_d->mNextMainDateTime = mDeferralTime;
                    al_d->mDeferred         = true;
                    al_d->mTimedDeferral    = !mDeferralTime.isDateOnly();
                }
                break;
            case KAAlarm::AT_LOGIN_ALARM:
                if (mRepeatAtLogin)
                {
                    al_d->mType             = KAAlarm::AT_LOGIN_ALARM;
                    al_d->mNextMainDateTime = mAtLoginDateTime;
                    al_d->mRepeatAtLogin    = true;
                }
                break;
            case KAAlarm::DISPLAYING_ALARM:
                if (mDisplaying)
                {
                    al_d->mType             = KAAlarm::DISPLAYING_ALARM;
                    al_d->mNextMainDateTime = mDisplayingTime;
                }
                break;
            case KAAlarm::INVALID_ALARM:
            default:
                break;
        }
    }
    return al;
}

/******************************************************************************
* Return the main alarm for the event.
* If the main alarm does not exist, one of the subsidiary ones is returned if
* possible.
* N.B. a repeat-at-login alarm can only be returned if it has been read from/
* written to the calendar file.
*/
KAAlarm KAEvent::firstAlarm() const
{
    return d->firstAlarm();
}

KAAlarm KAEvent::Private::firstAlarm() const
{
    if (mAlarmCount)
    {
        if (!mMainExpired)
            return alarm(KAAlarm::MAIN_ALARM);
        return nextAlarm(KAAlarm::MAIN_ALARM);
    }
    return KAAlarm();
}

/******************************************************************************
* Return the next alarm for the event, after the specified alarm.
* N.B. a repeat-at-login alarm can only be returned if it has been read from/
* written to the calendar file.
*/
KAAlarm KAEvent::nextAlarm(const KAAlarm& previousAlarm) const
{
    return d->nextAlarm(previousAlarm.type());
}

KAAlarm KAEvent::nextAlarm(KAAlarm::Type previousType) const
{
    return d->nextAlarm(previousType);
}

KAAlarm KAEvent::Private::nextAlarm(KAAlarm::Type previousType) const
{
    switch (previousType)
    {
        case KAAlarm::MAIN_ALARM:
            if (mReminderActive == ACTIVE_REMINDER)
                return alarm(KAAlarm::REMINDER_ALARM);
            // fall through to REMINDER_ALARM
        case KAAlarm::REMINDER_ALARM:
            // There can only be one deferral alarm
            if (mDeferral == REMINDER_DEFERRAL)
                return alarm(KAAlarm::DEFERRED_REMINDER_ALARM);
            if (mDeferral == NORMAL_DEFERRAL)
                return alarm(KAAlarm::DEFERRED_ALARM);
            // fall through to DEFERRED_ALARM
        case KAAlarm::DEFERRED_REMINDER_ALARM:
        case KAAlarm::DEFERRED_ALARM:
            if (mRepeatAtLogin)
                return alarm(KAAlarm::AT_LOGIN_ALARM);
            // fall through to AT_LOGIN_ALARM
        case KAAlarm::AT_LOGIN_ALARM:
            if (mDisplaying)
                return alarm(KAAlarm::DISPLAYING_ALARM);
            // fall through to DISPLAYING_ALARM
        case KAAlarm::DISPLAYING_ALARM:
            // fall through to default
        case KAAlarm::INVALID_ALARM:
        default:
            break;
    }
    return KAAlarm();
}

int KAEvent::alarmCount() const
{
    return d->mAlarmCount;
}

/******************************************************************************
* Remove the alarm of the specified type from the event.
* This must only be called to remove an alarm which has expired, not to
* reconfigure the event.
*/
void KAEvent::removeExpiredAlarm(KAAlarm::Type type)
{
    d->removeExpiredAlarm(type);
}

void KAEvent::Private::removeExpiredAlarm(KAAlarm::Type type)
{
    const int count = mAlarmCount;
    switch (type)
    {
        case KAAlarm::MAIN_ALARM:
            if (!mReminderActive  ||  mReminderMinutes > 0)
            {
                mAlarmCount = 0;    // removing main alarm - also remove subsidiary alarms
                break;
            }
            // There is a reminder after the main alarm - retain the
            // reminder and remove other subsidiary alarms.
            mMainExpired = true;    // mark the alarm as expired now
            --mAlarmCount;
            set_deferral(NO_DEFERRAL);
            if (mDisplaying)
            {
                mDisplaying = false;
                --mAlarmCount;
            }
            // fall through to AT_LOGIN_ALARM
        case KAAlarm::AT_LOGIN_ALARM:
            if (mRepeatAtLogin)
            {
                // Remove the at-login alarm, but keep a note of it for archiving purposes
                mArchiveRepeatAtLogin = true;
                mRepeatAtLogin = false;
                --mAlarmCount;
            }
            break;
        case KAAlarm::REMINDER_ALARM:
            // Remove any reminder alarm, but keep a note of it for archiving purposes
            // and for restoration after the next recurrence.
            activate_reminder(false);
            break;
        case KAAlarm::DEFERRED_REMINDER_ALARM:
        case KAAlarm::DEFERRED_ALARM:
            set_deferral(NO_DEFERRAL);
            break;
        case KAAlarm::DISPLAYING_ALARM:
            if (mDisplaying)
            {
                mDisplaying = false;
                --mAlarmCount;
            }
            break;
        case KAAlarm::INVALID_ALARM:
        default:
            break;
    }
    if (mAlarmCount != count)
        mTriggerChanged = true;
}

void KAEvent::startChanges()
{
    d->startChanges();
}

/******************************************************************************
* Indicate that changes to the instance are complete.
* This allows trigger times to be recalculated if any changes have occurred.
*/
void KAEvent::endChanges()
{
    d->endChanges();
}

void KAEvent::Private::endChanges()
{
    if (mChangeCount > 0)
        --mChangeCount;
}

#ifdef USE_AKONADI
/******************************************************************************
* Return a list of pointers to KAEvent objects.
*/
KAEvent::List KAEvent::ptrList(QVector<KAEvent>& objList)
{
    KAEvent::List ptrs;
    for (int i = 0, count = objList.count();  i < count;  ++i)
        ptrs += &objList[i];
    return ptrs;
}
#endif

void KAEvent::dumpDebug() const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    d->dumpDebug();
#endif
}

#ifndef KDE_NO_DEBUG_OUTPUT
void KAEvent::Private::dumpDebug() const
{
    kDebug() << "KAEvent dump:";
#ifndef USE_AKONADI
    if (mResource) { kDebug() << "-- mResource:" << mResource->resourceName(); }
#endif
    kDebug() << "-- mEventID:" << mEventID;
    kDebug() << "-- mActionSubType:" << (mActionSubType == MESSAGE ? "MESSAGE" : mActionSubType == FILE ? "FILE" : mActionSubType == COMMAND ? "COMMAND" : mActionSubType == EMAIL ? "EMAIL" : mActionSubType == AUDIO ? "AUDIO" : "??");
    kDebug() << "-- mNextMainDateTime:" << mNextMainDateTime.toString();
    kDebug() << "-- mCommandError:" << mCommandError;
    kDebug() << "-- mAllTrigger:" << mAllTrigger.toString();
    kDebug() << "-- mMainTrigger:" << mMainTrigger.toString();
    kDebug() << "-- mAllWorkTrigger:" << mAllWorkTrigger.toString();
    kDebug() << "-- mMainWorkTrigger:" << mMainWorkTrigger.toString();
    kDebug() << "-- mCategory:" << mCategory;
    if (!mTemplateName.isEmpty())
    {
        kDebug() << "-- mTemplateName:" << mTemplateName;
        kDebug() << "-- mTemplateAfterTime:" << mTemplateAfterTime;
    }
    kDebug() << "-- mText:" << mText;
    if (mActionSubType == MESSAGE  ||  mActionSubType == FILE)
    {
        kDebug() << "-- mBgColour:" << mBgColour.name();
        kDebug() << "-- mFgColour:" << mFgColour.name();
        kDebug() << "-- mUseDefaultFont:" << mUseDefaultFont;
        if (!mUseDefaultFont)
            kDebug() << "-- mFont:" << mFont.toString();
        kDebug() << "-- mSpeak:" << mSpeak;
        kDebug() << "-- mAudioFile:" << mAudioFile;
        kDebug() << "-- mPreAction:" << mPreAction;
        kDebug() << "-- mCancelOnPreActErr:" << mCancelOnPreActErr;
        kDebug() << "-- mDontShowPreActErr:" << mDontShowPreActErr;
        kDebug() << "-- mPostAction:" << mPostAction;
        kDebug() << "-- mLateCancel:" << mLateCancel;
        kDebug() << "-- mAutoClose:" << mAutoClose;
    }
    else if (mActionSubType == COMMAND)
    {
        kDebug() << "-- mCommandScript:" << mCommandScript;
        kDebug() << "-- mCommandXterm:" << mCommandXterm;
        kDebug() << "-- mCommandDisplay:" << mCommandDisplay;
        kDebug() << "-- mLogFile:" << mLogFile;
    }
    else if (mActionSubType == EMAIL)
    {
        kDebug() << "-- mEmail: FromKMail:" << mEmailFromIdentity;
        kDebug() << "--         Addresses:" << mEmailAddresses.join(",");
        kDebug() << "--         Subject:" << mEmailSubject;
        kDebug() << "--         Attachments:" << mEmailAttachments.join(",");
        kDebug() << "--         Bcc:" << mEmailBcc;
    }
    else if (mActionSubType == AUDIO)
        kDebug() << "-- mAudioFile:" << mAudioFile;
    kDebug() << "-- mBeep:" << mBeep;
    if (mActionSubType == AUDIO  ||  !mAudioFile.isEmpty())
    {
        if (mSoundVolume >= 0)
        {
            kDebug() << "-- mSoundVolume:" << mSoundVolume;
            if (mFadeVolume >= 0)
            {
                kDebug() << "-- mFadeVolume:" << mFadeVolume;
                kDebug() << "-- mFadeSeconds:" << mFadeSeconds;
            }
            else
                kDebug() << "-- mFadeVolume:-:";
        }
        else
            kDebug() << "-- mSoundVolume:-:";
        kDebug() << "-- mRepeatSound:" << mRepeatSound;
    }
    kDebug() << "-- mKMailSerialNumber:" << mKMailSerialNumber;
    kDebug() << "-- mCopyToKOrganizer:" << mCopyToKOrganizer;
    kDebug() << "-- mExcludeHolidays:" << (bool)mExcludeHolidays;
    kDebug() << "-- mWorkTimeOnly:" << mWorkTimeOnly;
    kDebug() << "-- mStartDateTime:" << mStartDateTime.toString();
    kDebug() << "-- mCreatedDateTime:" << mCreatedDateTime;
    kDebug() << "-- mRepeatAtLogin:" << mRepeatAtLogin;
    if (mRepeatAtLogin)
        kDebug() << "-- mAtLoginDateTime:" << mAtLoginDateTime;
    kDebug() << "-- mArchiveRepeatAtLogin:" << mArchiveRepeatAtLogin;
    kDebug() << "-- mConfirmAck:" << mConfirmAck;
    kDebug() << "-- mEnabled:" << mEnabled;
#ifdef USE_AKONADI
    kDebug() << "-- mItemId:" << mItemId;
    kDebug() << "-- mCompatibility:" << mCompatibility;
    kDebug() << "-- mReadOnly:" << mReadOnly;
#endif
    if (mReminderMinutes)
    {
        kDebug() << "-- mReminderMinutes:" << mReminderMinutes;
        kDebug() << "-- mReminderActive:" << (mReminderActive == ACTIVE_REMINDER ? "active" : mReminderActive == HIDDEN_REMINDER ? "hidden" : "no");
        kDebug() << "-- mReminderOnceOnly:" << mReminderOnceOnly;
    }
    else if (mDeferral > 0)
    {
        kDebug() << "-- mDeferral:" << (mDeferral == NORMAL_DEFERRAL ? "normal" : "reminder");
        kDebug() << "-- mDeferralTime:" << mDeferralTime.toString();
    }
    kDebug() << "-- mDeferDefaultMinutes:" << mDeferDefaultMinutes;
    if (mDeferDefaultMinutes)
        kDebug() << "-- mDeferDefaultDateOnly:" << mDeferDefaultDateOnly;
    if (mDisplaying)
    {
        kDebug() << "-- mDisplayingTime:" << mDisplayingTime.toString();
        kDebug() << "-- mDisplayingFlags:" << mDisplayingFlags;
        kDebug() << "-- mDisplayingDefer:" << mDisplayingDefer;
        kDebug() << "-- mDisplayingEdit:" << mDisplayingEdit;
    }
    kDebug() << "-- mRevision:" << mRevision;
    kDebug() << "-- mRecurrence:" << mRecurrence;
    if (!mRepetition)
        kDebug() << "-- mRepetition: 0";
    else if (mRepetition.isDaily())
        kDebug() << "-- mRepetition: count:" << mRepetition.count() << ", interval:" << mRepetition.intervalDays() << "days";
    else
        kDebug() << "-- mRepetition: count:" << mRepetition.count() << ", interval:" << mRepetition.intervalMinutes() << "minutes";
    kDebug() << "-- mNextRepeat:" << mNextRepeat;
    kDebug() << "-- mAlarmCount:" << mAlarmCount;
    kDebug() << "-- mMainExpired:" << mMainExpired;
    kDebug() << "-- mDisplaying:" << mDisplaying;
    kDebug() << "KAEvent dump end";
}
#endif


/******************************************************************************
* Fetch the start and next date/time for a KCal::Event.
* Reply = next main date/time.
*/
#ifdef USE_AKONADI
DateTime KAEvent::Private::readDateTime(const ConstEventPtr& event, bool dateOnly, DateTime& start)
#else
DateTime KAEvent::Private::readDateTime(const Event* event, bool dateOnly, DateTime& start)
#endif
{
    start = event->dtStart();
    if (dateOnly)
    {
        // A date-only event is indicated by the X-KDE-KALARM-FLAGS:DATE property, not
        // by a date-only start date/time (for the reasons given in updateKCalEvent()).
        start.setDateOnly(true);
    }
    DateTime next = start;
    QString prop = event->customProperty(KAlarm::Calendar::APPNAME, Private::NEXT_RECUR_PROPERTY);
    if (prop.length() >= 8)
    {
        // The next due recurrence time is specified
        const QDate d(prop.left(4).toInt(), prop.mid(4,2).toInt(), prop.mid(6,2).toInt());
        if (d.isValid())
        {
            if (dateOnly  &&  prop.length() == 8)
                next.setDate(d);
            else if (!dateOnly  &&  prop.length() == 15  &&  prop[8] == QChar('T'))
            {
                const QTime t(prop.mid(9,2).toInt(), prop.mid(11,2).toInt(), prop.mid(13,2).toInt());
                if (t.isValid())
                {
                    next.setDate(d);
                    next.setTime(t);
                }
            }
            if (next < start)
                next = start;   // ensure next recurrence time is valid
        }
    }
    return next;
}

/******************************************************************************
* Parse the alarms for a KCal::Event.
* Reply = map of alarm data, indexed by KAAlarm::Type
*/
#ifdef USE_AKONADI
void KAEvent::Private::readAlarms(const ConstEventPtr& event, void* almap, bool cmdDisplay)
#else
void KAEvent::Private::readAlarms(const Event* event, void* almap, bool cmdDisplay)
#endif
{
    AlarmMap* alarmMap = (AlarmMap*)almap;
    Alarm::List alarms = event->alarms();

    // Check if it's an audio event with no display alarm
    bool audioOnly = false;
    for (int i = 0, end = alarms.count();  i < end;  ++i)
    {
        switch (alarms[i]->type())
        {
            case Alarm::Display:
            case Alarm::Procedure:
                audioOnly = false;
                i = end;   // exit from the 'for' loop
                break;
            case Alarm::Audio:
                audioOnly = true;
                break;
            default:
                break;
        }
    }

    for (int i = 0, end = alarms.count();  i < end;  ++i)
    {
        // Parse the next alarm's text
        AlarmData data;
        readAlarm(alarms[i], data, audioOnly, cmdDisplay);
        if (data.type != INVALID_ALARM)
            alarmMap->insert(data.type, data);
    }
}

/******************************************************************************
* Parse a KCal::Alarm.
* If 'audioMain' is true, the event contains an audio alarm but no display alarm.
* Reply = alarm ID (sequence number)
*/
#ifdef USE_AKONADI
void KAEvent::Private::readAlarm(const ConstAlarmPtr& alarm, AlarmData& data, bool audioMain, bool cmdDisplay)
#else
void KAEvent::Private::readAlarm(const Alarm* alarm, AlarmData& data, bool audioMain, bool cmdDisplay)
#endif
{
    // Parse the next alarm's text
    data.alarm           = alarm;
    data.displayingFlags = 0;
    data.isEmailText     = false;
    data.speak           = false;
    data.hiddenReminder  = false;
    data.timedDeferral   = false;
    data.nextRepeat      = 0;
    if (alarm->repeatCount())
    {
        bool ok;
        const QString property = alarm->customProperty(KAlarm::Calendar::APPNAME, Private::NEXT_REPEAT_PROPERTY);
        int n = static_cast<int>(property.toUInt(&ok));
        if (ok)
            data.nextRepeat = n;
    }
    QString property = alarm->customProperty(KAlarm::Calendar::APPNAME, Private::FLAGS_PROPERTY);
    const QStringList flags = property.split(Private::SC, QString::SkipEmptyParts);
    switch (alarm->type())
    {
        case Alarm::Procedure:
            data.action        = KAAlarm::COMMAND;
            data.cleanText     = alarm->programFile();
            data.commandScript = data.cleanText.isEmpty();   // blank command indicates a script
            if (!alarm->programArguments().isEmpty())
            {
                if (!data.commandScript)
                    data.cleanText += ' ';
                data.cleanText += alarm->programArguments();
            }
            data.cancelOnPreActErr = flags.contains(Private::CANCEL_ON_ERROR_FLAG);
            data.dontShowPreActErr = flags.contains(Private::DONT_SHOW_ERROR_FLAG);
            if (!cmdDisplay)
                break;
            // fall through to Display
        case Alarm::Display:
        {
            if (alarm->type() == Alarm::Display)
            {
                data.action    = KAAlarm::MESSAGE;
                data.cleanText = AlarmText::fromCalendarText(alarm->text(), data.isEmailText);
            }
            const QString property = alarm->customProperty(KAlarm::Calendar::APPNAME, Private::FONT_COLOUR_PROPERTY);
            const QStringList list = property.split(QLatin1Char(';'), QString::KeepEmptyParts);
            data.bgColour = QColor(255, 255, 255);   // white
            data.fgColour = QColor(0, 0, 0);         // black
            const int n = list.count();
            if (n > 0)
            {
                if (!list[0].isEmpty())
                {
                    QColor c(list[0]);
                    if (c.isValid())
                        data.bgColour = c;
                }
                if (n > 1  &&  !list[1].isEmpty())
                {
                    QColor c(list[1]);
                    if (c.isValid())
                        data.fgColour = c;
                }
            }
            data.defaultFont = (n <= 2 || list[2].isEmpty());
            if (!data.defaultFont)
                data.font.fromString(list[2]);
            break;
        }
        case Alarm::Email:
        {
            data.action    = KAAlarm::EMAIL;
            data.cleanText = alarm->mailText();
            const int i = flags.indexOf(Private::EMAIL_ID_FLAG);
            data.emailFromId = (i >= 0  &&  i + 1 < flags.count()) ? flags[i + 1].toUInt() : 0;
            break;
        }
        case Alarm::Audio:
        {
            data.action      = KAAlarm::AUDIO;
            data.cleanText   = alarm->audioFile();
            data.soundVolume = -1;
            data.fadeVolume  = -1;
            data.fadeSeconds = 0;
            QString property = alarm->customProperty(KAlarm::Calendar::APPNAME, Private::VOLUME_PROPERTY);
            if (!property.isEmpty())
            {
                bool ok;
                float fadeVolume;
                int   fadeSecs = 0;
                const QStringList list = property.split(QLatin1Char(';'), QString::KeepEmptyParts);
                data.soundVolume = list[0].toFloat(&ok);
                if (!ok  ||  data.soundVolume > 1.0f)
                    data.soundVolume = -1;
                if (data.soundVolume >= 0  &&  list.count() >= 3)
                {
                    fadeVolume = list[1].toFloat(&ok);
                    if (ok)
                        fadeSecs = static_cast<int>(list[2].toUInt(&ok));
                    if (ok  &&  fadeVolume >= 0  &&  fadeVolume <= 1.0f  &&  fadeSecs > 0)
                    {
                        data.fadeVolume  = fadeVolume;
                        data.fadeSeconds = fadeSecs;
                    }
                }
            }
            if (!audioMain)
            {
                data.type  = AUDIO_ALARM;
                data.speak = flags.contains(Private::SPEAK_FLAG);
                return;
            }
            break;
        }
        case Alarm::Invalid:
            data.type = INVALID_ALARM;
            return;
    }

    bool atLogin          = false;
    bool reminder         = false;
    bool deferral         = false;
    bool dateDeferral     = false;
    data.repeatSound      = false;
    data.type = MAIN_ALARM;
    property = alarm->customProperty(KAlarm::Calendar::APPNAME, Private::TYPE_PROPERTY);
    const QStringList types = property.split(QLatin1Char(','), QString::SkipEmptyParts);
    for (int i = 0, end = types.count();  i < end;  ++i)
    {
        const QString type = types[i];
        if (type == Private::AT_LOGIN_TYPE)
            atLogin = true;
        else if (type == Private::FILE_TYPE  &&  data.action == KAAlarm::MESSAGE)
            data.action = KAAlarm::FILE;
        else if (type == Private::REMINDER_TYPE)
            reminder = true;
        else if (type == Private::TIME_DEFERRAL_TYPE)
            deferral = true;
        else if (type == Private::DATE_DEFERRAL_TYPE)
            dateDeferral = deferral = true;
        else if (type == Private::DISPLAYING_TYPE)
            data.type = DISPLAYING_ALARM;
        else if (type == Private::PRE_ACTION_TYPE  &&  data.action == KAAlarm::COMMAND)
            data.type = PRE_ACTION_ALARM;
        else if (type == Private::POST_ACTION_TYPE  &&  data.action == KAAlarm::COMMAND)
            data.type = POST_ACTION_ALARM;
        else if (type == Private::SOUND_REPEAT_TYPE  &&  data.action == KAAlarm::AUDIO)
            data.repeatSound = true;
    }

    if (reminder)
    {
        if (data.type == MAIN_ALARM)
        {
            data.type = deferral ? DEFERRED_REMINDER_ALARM : REMINDER_ALARM;
            data.timedDeferral = (deferral && !dateDeferral);
        }
        else if (data.type == DISPLAYING_ALARM)
            data.displayingFlags = dateDeferral ? REMINDER | DATE_DEFERRAL
                                 : deferral ? REMINDER | TIME_DEFERRAL : REMINDER;
        else if (data.type == REMINDER_ALARM
             &&  flags.contains(Private::HIDDEN_REMINDER_FLAG))
            data.hiddenReminder = true;
    }
    else if (deferral)
    {
        if (data.type == MAIN_ALARM)
        {
            data.type = DEFERRED_ALARM;
            data.timedDeferral = !dateDeferral;
        }
        else if (data.type == DISPLAYING_ALARM)
            data.displayingFlags = dateDeferral ? DATE_DEFERRAL : TIME_DEFERRAL;
    }
    if (atLogin)
    {
        if (data.type == MAIN_ALARM)
            data.type = AT_LOGIN_ALARM;
        else if (data.type == DISPLAYING_ALARM)
            data.displayingFlags = REPEAT_AT_LOGIN;
    }
//kDebug()<<"text="<<alarm->text()<<", time="<<alarm->time().toString()<<", valid time="<<alarm->time().isValid();
}

/******************************************************************************
* Calculate the next trigger times of the alarm.
* This should only be called when changes have actually occurred which might
* affect the event's trigger times.
* mMainTrigger is set to the next scheduled recurrence/sub-repetition, or the
*              deferral time if a deferral is pending.
* mAllTrigger is the same as mMainTrigger, but takes account of reminders.
* mMainWorkTrigger is set to the next scheduled recurrence/sub-repetition
*                  which occurs in working hours, if working-time-only is set.
* mAllWorkTrigger is the same as mMainWorkTrigger, but takes account of reminders.
*/
void KAEvent::Private::calcTriggerTimes() const
{
    if (mChangeCount)
        return;
#warning Only allow work time or exclude holidays if recurring
    if ((mWorkTimeOnly  &&  mWorkTimeOnly != mWorkTimeIndex)
    ||  (mExcludeHolidays  &&  mExcludeHolidays != mHolidays))
    {
        // It's a work time alarm, and work days/times have changed, or
        // it excludes holidays, and the holidays definition has changed.
        mTriggerChanged = true;
    }
    else if (!mTriggerChanged)
        return;
    mTriggerChanged = false;
    if (mWorkTimeOnly)
        mWorkTimeOnly = mWorkTimeIndex;   // note which work time definition was used in calculation
    if (mExcludeHolidays)
        mExcludeHolidays = mHolidays;     // note which holiday definition was used in calculation

    if (mCategory == KAlarm::CalEvent::ARCHIVED  ||  mCategory == KAlarm::CalEvent::TEMPLATE)
    {
        // It's a template or archived
        mAllTrigger = mMainTrigger = mAllWorkTrigger = mMainWorkTrigger = KDateTime();
    }
    else if (mDeferral == NORMAL_DEFERRAL)
    {
        // For a deferred alarm, working time setting is ignored
        mAllTrigger = mMainTrigger = mAllWorkTrigger = mMainWorkTrigger = mDeferralTime;
    }
    else
    {
        mMainTrigger = mainDateTime(true);   // next recurrence or sub-repetition
        mAllTrigger = (mDeferral == REMINDER_DEFERRAL)     ? mDeferralTime
                    : (mReminderActive != ACTIVE_REMINDER) ? mMainTrigger
                    : (mReminderMinutes < 0)               ? mReminderAfterTime
                    :                                        mMainTrigger.addMins(-mReminderMinutes);
        // It's not deferred.
        // If only-during-working-time is set and it recurs, it won't actually trigger
        // unless it falls during working hours.
        if ((!mWorkTimeOnly && !mExcludeHolidays)
        ||  checkRecur() == KARecurrence::NO_RECUR
        ||  isWorkingTime(mMainTrigger.kDateTime()))
        {
            // It only occurs once, or it complies with any working hours/holiday
            // restrictions.
            mMainWorkTrigger = mMainTrigger;
            mAllWorkTrigger = mAllTrigger;
        }
        else if (mWorkTimeOnly)
        {
            // The alarm is restricted to working hours.
            // Finding the next occurrence during working hours can sometimes take a long time,
            // so mark the next actual trigger as invalid until the calculation completes.
            // Note that reminders are only triggered if the main alarm is during working time.
            if (!mExcludeHolidays)
            {
                // There are no holiday restrictions.
                calcNextWorkingTime(mMainTrigger);
            }
            else if (mHolidays)
            {
                // Holidays are excluded.
                DateTime nextTrigger = mMainTrigger;
                KDateTime kdt;
                for (int i = 0;  i < 20;  ++i)
                {
                    calcNextWorkingTime(nextTrigger);
                    if (!mHolidays->isHoliday(mMainWorkTrigger.date()))
                        return;   // found a non-holiday occurrence
                    kdt = mMainWorkTrigger.effectiveKDateTime();
                    kdt.setTime(QTime(23,59,59));
                    const OccurType type = nextOccurrence(kdt, nextTrigger, RETURN_REPETITION);
                    if (!nextTrigger.isValid())
                        break;
                    if (isWorkingTime(nextTrigger.kDateTime()))
                    {
                        const int reminder = (mReminderMinutes > 0) ? mReminderMinutes : 0;   // only interested in reminders BEFORE the alarm
                        mMainWorkTrigger = nextTrigger;
                        mAllWorkTrigger = (type & OCCURRENCE_REPEAT) ? mMainWorkTrigger : mMainWorkTrigger.addMins(-reminder);
                        return;   // found a non-holiday occurrence
                    }
                }
                mMainWorkTrigger = mAllWorkTrigger = DateTime();
            }
        }
        else if (mExcludeHolidays  &&  mHolidays)
        {
            // Holidays are excluded.
            DateTime nextTrigger = mMainTrigger;
            KDateTime kdt;
            for (int i = 0;  i < 20;  ++i)
            {
                kdt = nextTrigger.effectiveKDateTime();
                kdt.setTime(QTime(23,59,59));
                const OccurType type = nextOccurrence(kdt, nextTrigger, RETURN_REPETITION);
                if (!nextTrigger.isValid())
                    break;
                if (!mHolidays->isHoliday(nextTrigger.date()))
                {
                    const int reminder = (mReminderMinutes > 0) ? mReminderMinutes : 0;   // only interested in reminders BEFORE the alarm
                    mMainWorkTrigger = nextTrigger;
                    mAllWorkTrigger = (type & OCCURRENCE_REPEAT) ? mMainWorkTrigger : mMainWorkTrigger.addMins(-reminder);
                    return;   // found a non-holiday occurrence
                }
            }
            mMainWorkTrigger = mAllWorkTrigger = DateTime();
        }
    }
}

/******************************************************************************
* Return the time of the next scheduled occurrence of the event during working
* hours, for an alarm which is restricted to working hours.
* On entry, 'nextTrigger' = the next recurrence or repetition (as returned by
* mainDateTime(true) ).
*/
void KAEvent::Private::calcNextWorkingTime(const DateTime& nextTrigger) const
{
    kDebug() << "next=" << nextTrigger.kDateTime().dateTime();
    mMainWorkTrigger = mAllWorkTrigger = DateTime();

    for (int i = 0;  ;  ++i)
    {
        if (i >= 7)
            return;   // no working days are defined
        if (mWorkDays.testBit(i))
            break;
    }
    const KARecurrence::Type recurType = checkRecur();
    KDateTime kdt = nextTrigger.effectiveKDateTime();
    const int reminder = (mReminderMinutes > 0) ? mReminderMinutes : 0;   // only interested in reminders BEFORE the alarm
    // Check if it always falls on the same day(s) of the week.
    const RecurrenceRule* rrule = mRecurrence->defaultRRuleConst();
    if (!rrule)
        return;   // no recurrence rule!
    unsigned allDaysMask = 0x7F;  // mask bits for all days of week
    bool noWorkPos = false;  // true if no recurrence day position is working day
    const QList<RecurrenceRule::WDayPos> pos = rrule->byDays();
    const int nDayPos = pos.count();  // number of day positions
    if (nDayPos)
    {
        noWorkPos = true;
        allDaysMask = 0;
        for (int i = 0;  i < nDayPos;  ++i)
        {
            const int day = pos[i].day() - 1;  // Monday = 0
            if (mWorkDays.testBit(day))
                noWorkPos = false;   // found a working day occurrence
            allDaysMask |= 1 << day;
        }
        if (noWorkPos  &&  !mRepetition)
            return;   // never occurs on a working day
    }
    DateTime newdt;

    if (mStartDateTime.isDateOnly())
    {
        // It's a date-only alarm.
        // Sub-repetitions also have to be date-only.
        const int repeatFreq = mRepetition.intervalDays();
        const bool weeklyRepeat = mRepetition && !(repeatFreq % 7);
        const Duration interval = mRecurrence->regularInterval();
        if ((interval  &&  !(interval.asDays() % 7))
        ||  nDayPos == 1)
        {
            // It recurs on the same day each week
            if (!mRepetition || weeklyRepeat)
                return;   // any repetitions are also weekly

            // It's a weekly recurrence with a non-weekly sub-repetition.
            // Check one cycle of repetitions for the next one that lands
            // on a working day.
            KDateTime dt(nextTrigger.kDateTime().addDays(1));
            dt.setTime(QTime(0,0,0));
            previousOccurrence(dt, newdt, false);
            if (!newdt.isValid())
                return;   // this should never happen
            kdt = newdt.effectiveKDateTime();
            const int day = kdt.date().dayOfWeek() - 1;   // Monday = 0
            for (int repeatNum = mNextRepeat + 1;  ;  ++repeatNum)
            {
                if (repeatNum > mRepetition.count())
                    repeatNum = 0;
                if (repeatNum == mNextRepeat)
                    break;
                if (!repeatNum)
                {
                    nextOccurrence(newdt.kDateTime(), newdt, IGNORE_REPETITION);
                    if (mWorkDays.testBit(day))
                    {
                        mMainWorkTrigger = newdt;
                        mAllWorkTrigger  = mMainWorkTrigger.addMins(-reminder);
                        return;
                    }
                    kdt = newdt.effectiveKDateTime();
                }
                else
                {
                    const int inc = repeatFreq * repeatNum;
                    if (mWorkDays.testBit((day + inc) % 7))
                    {
                        kdt = kdt.addDays(inc);
                        kdt.setDateOnly(true);
                        mMainWorkTrigger = mAllWorkTrigger = kdt;
                        return;
                    }
                }
            }
            return;
        }
        if (!mRepetition  ||  weeklyRepeat)
        {
            // It's a date-only alarm with either no sub-repetition or a
            // sub-repetition which always falls on the same day of the week
            // as the recurrence (if any).
            unsigned days = 0;
            for ( ; ; )
            {
                kdt.setTime(QTime(23,59,59));
                nextOccurrence(kdt, newdt, IGNORE_REPETITION);
                if (!newdt.isValid())
                    return;
                kdt = newdt.effectiveKDateTime();
                const int day = kdt.date().dayOfWeek() - 1;
                if (mWorkDays.testBit(day))
                    break;   // found a working day occurrence
                // Prevent indefinite looping (which should never happen anyway)
                if ((days & allDaysMask) == allDaysMask)
                    return;  // found a recurrence on every possible day of the week!?!
                days |= 1 << day;
            }
            kdt.setDateOnly(true);
            mMainWorkTrigger = kdt;
            mAllWorkTrigger  = kdt.addSecs(-60 * reminder);
            return;
        }

        // It's a date-only alarm which recurs on different days of the week,
        // as does the sub-repetition.
        // Find the previous recurrence (as opposed to sub-repetition)
        unsigned days = 1 << (kdt.date().dayOfWeek() - 1);
        KDateTime dt(nextTrigger.kDateTime().addDays(1));
        dt.setTime(QTime(0,0,0));
        previousOccurrence(dt, newdt, false);
        if (!newdt.isValid())
            return;   // this should never happen
        kdt = newdt.effectiveKDateTime();
        int day = kdt.date().dayOfWeek() - 1;   // Monday = 0
        for (int repeatNum = mNextRepeat;  ;  repeatNum = 0)
        {
            while (++repeatNum <= mRepetition.count())
            {
                const int inc = repeatFreq * repeatNum;
                if (mWorkDays.testBit((day + inc) % 7))
                {
                    kdt = kdt.addDays(inc);
                    kdt.setDateOnly(true);
                    mMainWorkTrigger = mAllWorkTrigger = kdt;
                    return;
                }
                if ((days & allDaysMask) == allDaysMask)
                    return;  // found an occurrence on every possible day of the week!?!
                days |= 1 << day;
            }
            nextOccurrence(kdt, newdt, IGNORE_REPETITION);
            if (!newdt.isValid())
                return;
            kdt = newdt.effectiveKDateTime();
            day = kdt.date().dayOfWeek() - 1;
            if (mWorkDays.testBit(day))
            {
                kdt.setDateOnly(true);
                mMainWorkTrigger = kdt;
                mAllWorkTrigger  = kdt.addSecs(-60 * reminder);
                return;
            }
            if ((days & allDaysMask) == allDaysMask)
                return;  // found an occurrence on every possible day of the week!?!
            days |= 1 << day;
        }
        return;
    }

    // It's a date-time alarm.

    /* Check whether the recurrence or sub-repetition occurs at the same time
     * every day. Note that because of seasonal time changes, a recurrence
     * defined in terms of minutes will vary its time of day even if its value
     * is a multiple of a day (24*60 minutes). Sub-repetitions are considered
     * to repeat at the same time of day regardless of time changes if they
     * are multiples of a day, which doesn't strictly conform to the iCalendar
     * format because this only allows their interval to be recorded in seconds.
     */
    const bool recurTimeVaries = (recurType == KARecurrence::MINUTELY);
    const bool repeatTimeVaries = (mRepetition  &&  !mRepetition.isDaily());

    if (!recurTimeVaries  &&  !repeatTimeVaries)
    {
        // The alarm always occurs at the same time of day.
        // Check whether it can ever occur during working hours.
        if (!mayOccurDailyDuringWork(kdt))
            return;   // never occurs during working hours

        // Find the next working day it occurs on
        bool repetition = false;
        unsigned days = 0;
        for ( ; ; )
        {
            OccurType type = nextOccurrence(kdt, newdt, RETURN_REPETITION);
            if (!newdt.isValid())
                return;
            repetition = (type & OCCURRENCE_REPEAT);
            kdt = newdt.effectiveKDateTime();
            const int day = kdt.date().dayOfWeek() - 1;
            if (mWorkDays.testBit(day))
                break;   // found a working day occurrence
            // Prevent indefinite looping (which should never happen anyway)
            if (!repetition)
            {
                if ((days & allDaysMask) == allDaysMask)
                    return;  // found a recurrence on every possible day of the week!?!
                days |= 1 << day;
            }
        }
        mMainWorkTrigger = nextTrigger;
        mMainWorkTrigger.setDate(kdt.date());
        mAllWorkTrigger = repetition ? mMainWorkTrigger : mMainWorkTrigger.addMins(-reminder);
        return;
    }

    // The alarm occurs at different times of day.
    // We may need to check for a full annual cycle of seasonal time changes, in
    // case it only occurs during working hours after a time change.
    KTimeZone tz = kdt.timeZone();
    if (tz.isValid()  &&  tz.type() == "KSystemTimeZone")
    {
        // It's a system time zone, so fetch full transition information
        const KTimeZone ktz = KSystemTimeZones::readZone(tz.name());
        if (ktz.isValid())
            tz = ktz;
    }
    const QList<KTimeZone::Transition> tzTransitions = tz.transitions();

    if (recurTimeVaries)
    {
        /* The alarm recurs at regular clock intervals, at different times of day.
         * Note that for this type of recurrence, it's necessary to avoid the
         * performance overhead of Recurrence class calls since these can in the
         * worst case cause the program to hang for a significant length of time.
         * In this case, we can calculate the next recurrence by simply adding the
         * recurrence interval, since KAlarm offers no facility to regularly miss
         * recurrences. (But exception dates/times need to be taken into account.)
         */
        KDateTime kdtRecur;
        int repeatFreq = 0;
        int repeatNum = 0;
        if (mRepetition)
        {
            // It's a repetition inside a recurrence, each of which occurs
            // at different times of day (bearing in mind that the repetition
            // may occur at daily intervals after each recurrence).
            // Find the previous recurrence (as opposed to sub-repetition)
            repeatFreq = mRepetition.intervalSeconds();
            previousOccurrence(kdt.addSecs(1), newdt, false);
            if (!newdt.isValid())
                return;   // this should never happen
            kdtRecur = newdt.effectiveKDateTime();
            repeatNum = kdtRecur.secsTo(kdt) / repeatFreq;
            kdt = kdtRecur.addSecs(repeatNum * repeatFreq);
        }
        else
        {
            // There is no sub-repetition.
            // (N.B. Sub-repetitions can't exist without a recurrence.)
            // Check until the original time wraps round, but ensure that
            // if there are seasonal time changes, that all other subsequent
            // time offsets within the next year are checked.
            // This does not guarantee to find the next working time,
            // particularly if there are exceptions, but it's a
            // reasonable try.
            kdtRecur = kdt;
        }
        QTime firstTime = kdtRecur.time();
        int firstOffset = kdtRecur.utcOffset();
        int currentOffset = firstOffset;
        int dayRecur = kdtRecur.date().dayOfWeek() - 1;   // Monday = 0
        int firstDay = dayRecur;
        QDate finalDate;
        const bool subdaily = (repeatFreq < 24*3600);
//        int period = mRecurrence->frequency() % (24*60);  // it is by definition a MINUTELY recurrence
//        int limit = (24*60 + period - 1) / period;  // number of times until recurrence wraps round
        int transitionIndex = -1;
        for (int n = 0;  n < 7*24*60;  ++n)
        {
            if (mRepetition)
            {
                // Check the sub-repetitions for this recurrence
                for ( ; ; )
                {
                    // Find the repeat count to the next start of the working day
                    const int inc = subdaily ? nextWorkRepetition(kdt) : 1;
                    repeatNum += inc;
                    if (repeatNum > mRepetition.count())
                        break;
                    kdt = kdt.addSecs(inc * repeatFreq);
                    const QTime t = kdt.time();
                    if (t >= mWorkDayStart  &&  t < mWorkDayEnd)
                    {
                        if (mWorkDays.testBit(kdt.date().dayOfWeek() - 1))
                        {
                            mMainWorkTrigger = mAllWorkTrigger = kdt;
                            return;
                        }
                    }
                }
                repeatNum = 0;
            }
            nextOccurrence(kdtRecur, newdt, IGNORE_REPETITION);
            if (!newdt.isValid())
                return;
            kdtRecur = newdt.effectiveKDateTime();
            dayRecur = kdtRecur.date().dayOfWeek() - 1;   // Monday = 0
            const QTime t = kdtRecur.time();
            if (t >= mWorkDayStart  &&  t < mWorkDayEnd)
            {
                if (mWorkDays.testBit(dayRecur))
                {
                    mMainWorkTrigger = kdtRecur;
                    mAllWorkTrigger  = kdtRecur.addSecs(-60 * reminder);
                    return;
                }
            }
            if (kdtRecur.utcOffset() != currentOffset)
                currentOffset = kdtRecur.utcOffset();
            if (t == firstTime  &&  dayRecur == firstDay  &&  currentOffset == firstOffset)
            {
                // We've wrapped round to the starting day and time.
                // If there are seasonal time changes, check for up
                // to the next year in other time offsets in case the
                // alarm occurs inside working hours then.
                if (!finalDate.isValid())
                    finalDate = kdtRecur.date();
                const int i = tz.transitionIndex(kdtRecur.toUtc().dateTime());
                if (i < 0)
                    return;
                if (i > transitionIndex)
                    transitionIndex = i;
                if (++transitionIndex >= static_cast<int>(tzTransitions.count()))
                    return;
                previousOccurrence(KDateTime(tzTransitions[transitionIndex].time(), KDateTime::UTC), newdt, IGNORE_REPETITION);
                kdtRecur = newdt.effectiveKDateTime();
                if (finalDate.daysTo(kdtRecur.date()) > 365)
                    return;
                firstTime = kdtRecur.time();
                firstOffset = kdtRecur.utcOffset();
                currentOffset = firstOffset;
                firstDay = kdtRecur.date().dayOfWeek() - 1;
            }
            kdt = kdtRecur;
        }
//kDebug()<<"-----exit loop: count="<<limit<<endl;
        return;   // too many iterations
    }

    if (repeatTimeVaries)
    {
        /* There's a sub-repetition which occurs at different times of
         * day, inside a recurrence which occurs at the same time of day.
         * We potentially need to check recurrences starting on each day.
         * Then, it is still possible that a working time sub-repetition
         * could occur immediately after a seasonal time change.
         */
        // Find the previous recurrence (as opposed to sub-repetition)
        const int repeatFreq = mRepetition.intervalSeconds();
        previousOccurrence(kdt.addSecs(1), newdt, false);
        if (!newdt.isValid())
            return;   // this should never happen
        KDateTime kdtRecur = newdt.effectiveKDateTime();
        const bool recurDuringWork = (kdtRecur.time() >= mWorkDayStart  &&  kdtRecur.time() < mWorkDayEnd);

        // Use the previous recurrence as a base for checking whether
        // our tests have wrapped round to the same time/day of week.
        const bool subdaily = (repeatFreq < 24*3600);
        unsigned days = 0;
        bool checkTimeChangeOnly = false;
        int transitionIndex = -1;
        for (int limit = 10;  --limit >= 0;  )
        {
            // Check the next seasonal time change (for an arbitrary 10 times,
            // even though that might not guarantee the correct result)
            QDate dateRecur = kdtRecur.date();
            int dayRecur = dateRecur.dayOfWeek() - 1;   // Monday = 0
            int repeatNum = kdtRecur.secsTo(kdt) / repeatFreq;
            kdt = kdtRecur.addSecs(repeatNum * repeatFreq);

            // Find the next recurrence, which sets the limit on possible sub-repetitions.
            // Note that for a monthly recurrence, for example, a sub-repetition could
            // be defined which is longer than the recurrence interval in short months.
            // In these cases, the sub-repetition is truncated by the following
            // recurrence.
            nextOccurrence(kdtRecur, newdt, IGNORE_REPETITION);
            KDateTime kdtNextRecur = newdt.effectiveKDateTime();

            int repeatsToCheck = mRepetition.count();
            int repeatsDuringWork = 0;  // 0=unknown, 1=does, -1=never
            for ( ; ; )
            {
                // Check the sub-repetitions for this recurrence
                if (repeatsDuringWork >= 0)
                {
                    for ( ; ; )
                    {
                        // Find the repeat count to the next start of the working day
                        int inc = subdaily ? nextWorkRepetition(kdt) : 1;
                        repeatNum += inc;
                        const bool pastEnd = (repeatNum > mRepetition.count());
                        if (pastEnd)
                            inc -= repeatNum - mRepetition.count();
                        repeatsToCheck -= inc;
                        kdt = kdt.addSecs(inc * repeatFreq);
                        if (kdtNextRecur.isValid()  &&  kdt >= kdtNextRecur)
                        {
                            // This sub-repetition is past the next recurrence,
                            // so start the check again from the next recurrence.
                            repeatsToCheck = mRepetition.count();
                            break;
                        }
                        if (pastEnd)
                            break;
                        const QTime t = kdt.time();
                        if (t >= mWorkDayStart  &&  t < mWorkDayEnd)
                        {
                            if (mWorkDays.testBit(kdt.date().dayOfWeek() - 1))
                            {
                                mMainWorkTrigger = mAllWorkTrigger = kdt;
                                return;
                            }
                            repeatsDuringWork = 1;
                        }
                        else if (!repeatsDuringWork  &&  repeatsToCheck <= 0)
                        {
                            // Sub-repetitions never occur during working hours
                            repeatsDuringWork = -1;
                            break;
                        }
                    }
                }
                repeatNum = 0;
                if (repeatsDuringWork < 0  &&  !recurDuringWork)
                    break;   // it never occurs during working hours

                // Check the next recurrence
                if (!kdtNextRecur.isValid())
                    return;
                if (checkTimeChangeOnly  ||  (days & allDaysMask) == allDaysMask)
                    break;  // found a recurrence on every possible day of the week!?!
                kdtRecur = kdtNextRecur;
                nextOccurrence(kdtRecur, newdt, IGNORE_REPETITION);
                kdtNextRecur = newdt.effectiveKDateTime();
                dateRecur = kdtRecur.date();
                dayRecur = dateRecur.dayOfWeek() - 1;
                if (recurDuringWork  &&  mWorkDays.testBit(dayRecur))
                {
                    mMainWorkTrigger = kdtRecur;
                    mAllWorkTrigger  = kdtRecur.addSecs(-60 * reminder);
                    return;
                }
                days |= 1 << dayRecur;
                kdt = kdtRecur;
            }

            // Find the next recurrence before a seasonal time change,
            // and ensure the time change is after the last one processed.
            checkTimeChangeOnly = true;
            const int i = tz.transitionIndex(kdtRecur.toUtc().dateTime());
            if (i < 0)
                return;
            if (i > transitionIndex)
                transitionIndex = i;
            if (++transitionIndex >= static_cast<int>(tzTransitions.count()))
                return;
            kdt = KDateTime(tzTransitions[transitionIndex].time(), KDateTime::UTC);
            previousOccurrence(kdt, newdt, IGNORE_REPETITION);
            kdtRecur = newdt.effectiveKDateTime();
        }
        return;  // not found - give up
    }
}

/******************************************************************************
* Find the repeat count to the next start of a working day.
* This allows for possible daylight saving time changes during the repetition.
* Use for repetitions which occur at different times of day.
*/
int KAEvent::Private::nextWorkRepetition(const KDateTime& pre) const
{
    KDateTime nextWork(pre);
    if (pre.time() < mWorkDayStart)
        nextWork.setTime(mWorkDayStart);
    else
    {
        const int preDay = pre.date().dayOfWeek() - 1;   // Monday = 0
        for (int n = 1;  ;  ++n)
        {
            if (n >= 7)
                return mRepetition.count() + 1;  // should never happen
            if (mWorkDays.testBit((preDay + n) % 7))
            {
                nextWork = nextWork.addDays(n);
                nextWork.setTime(mWorkDayStart);
                break;
            }
        }
    }
    return (pre.secsTo(nextWork) - 1) / mRepetition.intervalSeconds() + 1;
}

/******************************************************************************
* Check whether an alarm which recurs at the same time of day can possibly
* occur during working hours.
* This does not determine whether it actually does, but rather whether it could
* potentially given enough repetitions.
* Reply = false if it can never occur during working hours, true if it might.
*/
bool KAEvent::Private::mayOccurDailyDuringWork(const KDateTime& kdt) const
{
    if (!kdt.isDateOnly()
    &&  (kdt.time() < mWorkDayStart || kdt.time() >= mWorkDayEnd))
        return false;   // its time is outside working hours
    // Check if it always occurs on the same day of the week
    const Duration interval = mRecurrence->regularInterval();
    if (interval  &&  interval.isDaily()  &&  !(interval.asDays() % 7))
    {
        // It recurs weekly
        if (!mRepetition  ||  (mRepetition.isDaily() && !(mRepetition.intervalDays() % 7)))
            return false;   // any repetitions are also weekly
        // Repetitions are daily. Check if any occur on working days
        // by checking the first recurrence and up to 6 repetitions.
        int day = mRecurrence->startDateTime().date().dayOfWeek() - 1;   // Monday = 0
        const int repeatDays = mRepetition.intervalDays();
        const int maxRepeat = (mRepetition.count() < 6) ? mRepetition.count() : 6;
        for (int i = 0;  !mWorkDays.testBit(day);  ++i, day = (day + repeatDays) % 7)
        {
            if (i >= maxRepeat)
                return false;  // no working day occurrences
        }
    }
    return true;
}

/******************************************************************************
* Set the specified alarm to be an audio alarm with the given file name.
*/
#ifdef USE_AKONADI
void KAEvent::Private::setAudioAlarm(const Alarm::Ptr& alarm) const
#else
void KAEvent::Private::setAudioAlarm(Alarm* alarm) const
#endif
{
    alarm->setAudioAlarm(mAudioFile);  // empty for a beep or for speaking
    if (mSoundVolume >= 0)
        alarm->setCustomProperty(KAlarm::Calendar::APPNAME, VOLUME_PROPERTY,
                      QString::fromLatin1("%1;%2;%3").arg(QString::number(mSoundVolume, 'f', 2))
                                                     .arg(QString::number(mFadeVolume, 'f', 2))
                                                     .arg(mFadeSeconds));
}

/******************************************************************************
* Get the date/time of the next recurrence of the event, after the specified
* date/time.
* 'result' = date/time of next occurrence, or invalid date/time if none.
*/
KAEvent::OccurType KAEvent::Private::nextRecurrence(const KDateTime& preDateTime, DateTime& result) const
{
    const KDateTime recurStart = mRecurrence->startDateTime();
    KDateTime pre = preDateTime.toTimeSpec(mStartDateTime.timeSpec());
    if (mStartDateTime.isDateOnly()  &&  !pre.isDateOnly()  &&  pre.time() < DateTime::startOfDay())
    {
        pre = pre.addDays(-1);    // today's recurrence (if today recurs) is still to come
        pre.setTime(DateTime::startOfDay());
    }
    const KDateTime dt = mRecurrence->getNextDateTime(pre);
    result = dt;
    result.setDateOnly(mStartDateTime.isDateOnly());
    if (!dt.isValid())
        return NO_OCCURRENCE;
    if (dt == recurStart)
        return FIRST_OR_ONLY_OCCURRENCE;
    if (mRecurrence->duration() >= 0  &&  dt == mRecurrence->endDateTime())
        return LAST_RECURRENCE;
    return result.isDateOnly() ? RECURRENCE_DATE : RECURRENCE_DATE_TIME;
}

/******************************************************************************
* Validate the event's recurrence data, correcting any inconsistencies (which
* should never occur!).
* Reply = recurrence period type.
*/
KARecurrence::Type KAEvent::Private::checkRecur() const
{
    if (mRecurrence)
    {
        KARecurrence::Type type = mRecurrence->type();
        switch (type)
        {
            case KARecurrence::MINUTELY:     // hourly
            case KARecurrence::DAILY:        // daily
            case KARecurrence::WEEKLY:       // weekly on multiple days of week
            case KARecurrence::MONTHLY_DAY:  // monthly on multiple dates in month
            case KARecurrence::MONTHLY_POS:  // monthly on multiple nth day of week
            case KARecurrence::ANNUAL_DATE:  // annually on multiple months (day of month = start date)
            case KARecurrence::ANNUAL_POS:   // annually on multiple nth day of week in multiple months
                return type;
            default:
                if (mRecurrence)
                    const_cast<KAEvent::Private*>(this)->clearRecur();  // this shouldn't ever be necessary!!
                break;
        }
    }
    if (mRepetition)    // can't have a repetition without a recurrence
        const_cast<KAEvent::Private*>(this)->clearRecur();  // this shouldn't ever be necessary!!
    return KARecurrence::NO_RECUR;
}

/******************************************************************************
* If the calendar was written by a previous version of KAlarm, do any
* necessary format conversions on the events to ensure that when the calendar
* is saved, no information is lost or corrupted.
* Reply = true if any conversions were done.
*/
#ifdef USE_AKONADI
bool KAEvent::convertKCalEvents(const Calendar::Ptr& calendar, int calendarVersion)
#else
bool KAEvent::convertKCalEvents(CalendarLocal& calendar, int calendarVersion)
#endif
{
    // KAlarm pre-0.9 codes held in the alarm's DESCRIPTION property
    static const QChar   SEPARATOR        = QLatin1Char(';');
    static const QChar   LATE_CANCEL_CODE = QLatin1Char('C');
    static const QChar   AT_LOGIN_CODE    = QLatin1Char('L');   // subsidiary alarm at every login
    static const QChar   DEFERRAL_CODE    = QLatin1Char('D');   // extra deferred alarm
    static const QString TEXT_PREFIX      = QLatin1String("TEXT:");
    static const QString FILE_PREFIX      = QLatin1String("FILE:");
    static const QString COMMAND_PREFIX   = QLatin1String("CMD:");

    // KAlarm pre-0.9.2 codes held in the event's CATEGORY property
    static const QString BEEP_CATEGORY    = QLatin1String("BEEP");

    // KAlarm pre-1.1.1 LATECANCEL category with no parameter
    static const QString LATE_CANCEL_CAT = QLatin1String("LATECANCEL");

    // KAlarm pre-1.3.0 TMPLDEFTIME category with no parameter
    static const QString TEMPL_DEF_TIME_CAT = QLatin1String("TMPLDEFTIME");

    // KAlarm pre-1.3.1 XTERM category
    static const QString EXEC_IN_XTERM_CAT  = QLatin1String("XTERM");

    // KAlarm pre-1.9.0 categories
    static const QString DATE_ONLY_CATEGORY        = QLatin1String("DATE");
    static const QString EMAIL_BCC_CATEGORY        = QLatin1String("BCC");
    static const QString CONFIRM_ACK_CATEGORY      = QLatin1String("ACKCONF");
    static const QString KORGANIZER_CATEGORY       = QLatin1String("KORG");
    static const QString DEFER_CATEGORY            = QLatin1String("DEFER;");
    static const QString ARCHIVE_CATEGORY          = QLatin1String("SAVE");
    static const QString ARCHIVE_CATEGORIES        = QLatin1String("SAVE:");
    static const QString LATE_CANCEL_CATEGORY      = QLatin1String("LATECANCEL;");
    static const QString AUTO_CLOSE_CATEGORY       = QLatin1String("LATECLOSE;");
    static const QString TEMPL_AFTER_TIME_CATEGORY = QLatin1String("TMPLAFTTIME;");
    static const QString KMAIL_SERNUM_CATEGORY     = QLatin1String("KMAIL:");
    static const QString LOG_CATEGORY              = QLatin1String("LOG:");

    // KAlarm pre-1.5.0/1.9.9 properties
    static const QByteArray KMAIL_ID_PROPERTY("KMAILID");    // X-KDE-KALARM-KMAILID property

    // KAlarm pre-2.6.0 properties
    static const QByteArray ARCHIVE_PROPERTY("ARCHIVE");     // X-KDE-KALARM-ARCHIVE property
    static const QString ARCHIVE_REMINDER_ONCE_TYPE = QLatin1String("ONCE");
    static const QString REMINDER_ONCE_TYPE         = QLatin1String("REMINDER_ONCE");
    static const QByteArray EMAIL_ID_PROPERTY("EMAILID");         // X-KDE-KALARM-EMAILID property
    static const QByteArray SPEAK_PROPERTY("SPEAK");              // X-KDE-KALARM-SPEAK property
    static const QByteArray CANCEL_ON_ERROR_PROPERTY("ERRCANCEL");// X-KDE-KALARM-ERRCANCEL property
    static const QByteArray DONT_SHOW_ERROR_PROPERTY("ERRNOSHOW");// X-KDE-KALARM-ERRNOSHOW property

    bool adjustSummerTime = false;
    if (calendarVersion == -KAlarm::Version(0,5,7))
    {
        // The calendar file was written by the KDE 3.0.0 version of KAlarm 0.5.7.
        // Summer time was ignored when converting to UTC.
        calendarVersion = -calendarVersion;
        adjustSummerTime = true;
    }

    if (calendarVersion >= currentCalendarVersion())
        return false;

    kDebug() << "Adjusting version" << calendarVersion;
    const bool pre_0_7    = (calendarVersion < KAlarm::Version(0,7,0));
    const bool pre_0_9    = (calendarVersion < KAlarm::Version(0,9,0));
    const bool pre_0_9_2  = (calendarVersion < KAlarm::Version(0,9,2));
    const bool pre_1_1_1  = (calendarVersion < KAlarm::Version(1,1,1));
    const bool pre_1_2_1  = (calendarVersion < KAlarm::Version(1,2,1));
    const bool pre_1_3_0  = (calendarVersion < KAlarm::Version(1,3,0));
    const bool pre_1_3_1  = (calendarVersion < KAlarm::Version(1,3,1));
    const bool pre_1_4_14 = (calendarVersion < KAlarm::Version(1,4,14));
    const bool pre_1_5_0  = (calendarVersion < KAlarm::Version(1,5,0));
    const bool pre_1_9_0  = (calendarVersion < KAlarm::Version(1,9,0));
    const bool pre_1_9_2  = (calendarVersion < KAlarm::Version(1,9,2));
    const bool pre_1_9_7  = (calendarVersion < KAlarm::Version(1,9,7));
    const bool pre_1_9_9  = (calendarVersion < KAlarm::Version(1,9,9));
    const bool pre_1_9_10 = (calendarVersion < KAlarm::Version(1,9,10));
    const bool pre_2_2_9  = (calendarVersion < KAlarm::Version(2,2,9));
    const bool pre_2_3_0  = (calendarVersion < KAlarm::Version(2,3,0));
    const bool pre_2_3_2  = (calendarVersion < KAlarm::Version(2,3,2));
    const bool pre_2_7_0  = (calendarVersion < KAlarm::Version(2,7,0));
    Q_ASSERT(currentCalendarVersion() == KAlarm::Version(2,7,0));

    KTimeZone localZone;
    if (pre_1_9_2)
        localZone = KSystemTimeZones::local();

    bool converted = false;
#ifdef USE_AKONADI
    const Event::List events = calendar->rawEvents();
#else
    const Event::List events = calendar.rawEvents();
#endif
    for (int ei = 0, eend = events.count();  ei < eend;  ++ei)
    {
#ifdef USE_AKONADI
        Event::Ptr event = events[ei];
#else
        Event* event = events[ei];
#endif
        const Alarm::List alarms = event->alarms();
        if (alarms.isEmpty())
            continue;    // KAlarm isn't interested in events without alarms
        event->startUpdates();   // prevent multiple update notifications
        const bool readOnly = event->isReadOnly();
        if (readOnly)
            event->setReadOnly(false);
        QStringList cats = event->categories();
        bool addLateCancel = false;
        QStringList flags;

        if (pre_0_7  &&  event->allDay())
        {
            // It's a KAlarm pre-0.7 calendar file.
            // Ensure that when the calendar is saved, the alarm time isn't lost.
            event->setAllDay(false);
        }

        if (pre_0_9)
        {
            /*
             * It's a KAlarm pre-0.9 calendar file.
             * All alarms were of type DISPLAY. Instead of the X-KDE-KALARM-TYPE
             * alarm property, characteristics were stored as a prefix to the
             * alarm DESCRIPTION property, as follows:
             *   SEQNO;[FLAGS];TYPE:TEXT
             * where
             *   SEQNO = sequence number of alarm within the event
             *   FLAGS = C for late-cancel, L for repeat-at-login, D for deferral
             *   TYPE = TEXT or FILE or CMD
             *   TEXT = message text, file name/URL or command
             */
            for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
            {
#ifdef USE_AKONADI
                Alarm::Ptr alarm = alarms[ai];
#else
                Alarm* alarm = alarms[ai];
#endif
                bool atLogin    = false;
                bool deferral   = false;
                bool lateCancel = false;
                KAAlarm::Action action = KAAlarm::MESSAGE;
                QString txt = alarm->text();
                const int length = txt.length();
                int i = 0;
                if (txt[0].isDigit())
                {
                    while (++i < length  &&  txt[i].isDigit()) ;
                    if (i < length  &&  txt[i++] == SEPARATOR)
                    {
                        while (i < length)
                        {
                            const QChar ch = txt[i++];
                            if (ch == SEPARATOR)
                                break;
                            if (ch == LATE_CANCEL_CODE)
                                lateCancel = true;
                            else if (ch == AT_LOGIN_CODE)
                                atLogin = true;
                            else if (ch == DEFERRAL_CODE)
                                deferral = true;
                        }
                    }
                    else
                        i = 0;     // invalid prefix
                }
                if (txt.indexOf(TEXT_PREFIX, i) == i)
                    i += TEXT_PREFIX.length();
                else if (txt.indexOf(FILE_PREFIX, i) == i)
                {
                    action = KAAlarm::FILE;
                    i += FILE_PREFIX.length();
                }
                else if (txt.indexOf(COMMAND_PREFIX, i) == i)
                {
                    action = KAAlarm::COMMAND;
                    i += COMMAND_PREFIX.length();
                }
                else
                    i = 0;
                txt = txt.mid(i);

                QStringList types;
                switch (action)
                {
                    case KAAlarm::FILE:
                        types += Private::FILE_TYPE;
                        // fall through to MESSAGE
                    case KAAlarm::MESSAGE:
                        alarm->setDisplayAlarm(txt);
                        break;
                    case KAAlarm::COMMAND:
                        setProcedureAlarm(alarm, txt);
                        break;
                    case KAAlarm::EMAIL:     // email alarms were introduced in KAlarm 0.9
                    case KAAlarm::AUDIO:     // audio alarms (with no display) were introduced in KAlarm 2.3.2
                        break;
                }
                if (atLogin)
                {
                    types += Private::AT_LOGIN_TYPE;
                    lateCancel = false;
                }
                else if (deferral)
                    types += Private::TIME_DEFERRAL_TYPE;
                if (lateCancel)
                    addLateCancel = true;
                if (types.count() > 0)
                    alarm->setCustomProperty(KAlarm::Calendar::APPNAME, Private::TYPE_PROPERTY, types.join(","));

                if (pre_0_7  &&  alarm->repeatCount() > 0  &&  alarm->snoozeTime().value() > 0)
                {
                    // It's a KAlarm pre-0.7 calendar file.
                    // Minutely recurrences were stored differently.
                    Recurrence* recur = event->recurrence();
                    if (recur  &&  recur->recurs())
                    {
                        recur->setMinutely(alarm->snoozeTime().asSeconds() / 60);
                        recur->setDuration(alarm->repeatCount() + 1);
                        alarm->setRepeatCount(0);
                        alarm->setSnoozeTime(0);
                    }
                }

                if (adjustSummerTime)
                {
                    // The calendar file was written by the KDE 3.0.0 version of KAlarm 0.5.7.
                    // Summer time was ignored when converting to UTC.
                    KDateTime dt = alarm->time();
                    const time_t t = dt.toTime_t();
                    const struct tm* dtm = localtime(&t);
                    if (dtm->tm_isdst)
                    {
                        dt = dt.addSecs(-3600);
                        alarm->setTime(dt);
                    }
                }
            }
        }

        if (pre_0_9_2)
        {
            /*
             * It's a KAlarm pre-0.9.2 calendar file.
             * For the archive calendar, set the CREATED time to the DTEND value.
             * Convert date-only DTSTART to date/time, and add category "DATE".
             * Set the DTEND time to the DTSTART time.
             * Convert all alarm times to DTSTART offsets.
             * For display alarms, convert the first unlabelled category to an
             * X-KDE-KALARM-FONTCOLOUR property.
             * Convert BEEP category into an audio alarm with no audio file.
             */
            if (KAlarm::CalEvent::status(event) == KAlarm::CalEvent::ARCHIVED)
                event->setCreated(event->dtEnd());
            KDateTime start = event->dtStart();
            if (event->allDay())
            {
                event->setAllDay(false);
                start.setTime(QTime(0, 0));
                flags += Private::DATE_ONLY_FLAG;
            }
            event->setHasEndDate(false);

            for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
            {
#ifdef USE_AKONADI
                Alarm::Ptr alarm = alarms[ai];
#else
                Alarm* alarm = alarms[ai];
#endif
                alarm->setStartOffset(start.secsTo(alarm->time()));
            }

            if (!cats.isEmpty())
            {
                for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
                {
#ifdef USE_AKONADI
                    Alarm::Ptr alarm = alarms[ai];
#else
                    Alarm* alarm = alarms[ai];
#endif
                    if (alarm->type() == Alarm::Display)
                        alarm->setCustomProperty(KAlarm::Calendar::APPNAME, Private::FONT_COLOUR_PROPERTY,
                                                 QString::fromLatin1("%1;;").arg(cats[0]));
                }
                cats.removeAt(0);
            }

            for (int i = 0, end = cats.count();  i < end;  ++i)
            {
                if (cats[i] == BEEP_CATEGORY)
                {
                    cats.removeAt(i);

#ifdef USE_AKONADI
                    Alarm::Ptr alarm = event->newAlarm();
#else
                    Alarm* alarm = event->newAlarm();
#endif
                    alarm->setEnabled(true);
                    alarm->setAudioAlarm();
                    KDateTime dt = event->dtStart();    // default

                    // Parse and order the alarms to know which one's date/time to use
                    Private::AlarmMap alarmMap;
                    Private::readAlarms(event, &alarmMap);
                    Private::AlarmMap::ConstIterator it = alarmMap.constBegin();
                    if (it != alarmMap.constEnd())
                    {
                        dt = it.value().alarm->time();
                        break;
                    }
                    alarm->setStartOffset(start.secsTo(dt));
                    break;
                }
            }
        }

        if (pre_1_1_1)
        {
            /*
             * It's a KAlarm pre-1.1.1 calendar file.
             * Convert simple LATECANCEL category to LATECANCEL:n where n = minutes late.
             */
            int i;
            while ((i = cats.indexOf(LATE_CANCEL_CAT)) >= 0)
            {
                cats.removeAt(i);
                addLateCancel = true;
            }
        }

        if (pre_1_2_1)
        {
            /*
             * It's a KAlarm pre-1.2.1 calendar file.
             * Convert email display alarms from translated to untranslated header prefixes.
             */
            for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
            {
#ifdef USE_AKONADI
                Alarm::Ptr alarm = alarms[ai];
#else
                Alarm* alarm = alarms[ai];
#endif
                if (alarm->type() == Alarm::Display)
                {
                    const QString oldtext = alarm->text();
                    const QString newtext = AlarmText::toCalendarText(oldtext);
                    if (oldtext != newtext)
                        alarm->setDisplayAlarm(newtext);
                }
            }
        }

        if (pre_1_3_0)
        {
            /*
             * It's a KAlarm pre-1.3.0 calendar file.
             * Convert simple TMPLDEFTIME category to TMPLAFTTIME:n where n = minutes after.
             */
            int i;
            while ((i = cats.indexOf(TEMPL_DEF_TIME_CAT)) >= 0)
            {
                cats.removeAt(i);
                (flags += Private::TEMPL_AFTER_TIME_FLAG) += QLatin1String("0");
            }
        }

        if (pre_1_3_1)
        {
            /*
             * It's a KAlarm pre-1.3.1 calendar file.
             * Convert simple XTERM category to LOG:xterm:
             */
            int i;
            while ((i = cats.indexOf(EXEC_IN_XTERM_CAT)) >= 0)
            {
                cats.removeAt(i);
                event->setCustomProperty(KAlarm::Calendar::APPNAME, Private::LOG_PROPERTY, Private::xtermURL);
            }
        }

        if (pre_1_9_0)
        {
            /*
             * It's a KAlarm pre-1.9 calendar file.
             * Add the X-KDE-KALARM-STATUS custom property.
             * Convert KAlarm categories to custom fields.
             */
            KAlarm::CalEvent::setStatus(event, KAlarm::CalEvent::status(event));
            for (int i = 0;  i < cats.count(); )
            {
                QString cat = cats[i];
                if (cat == DATE_ONLY_CATEGORY)
                    flags += Private::DATE_ONLY_FLAG;
                else if (cat == CONFIRM_ACK_CATEGORY)
                    flags += Private::CONFIRM_ACK_FLAG;
                else if (cat == EMAIL_BCC_CATEGORY)
                    flags += Private::EMAIL_BCC_FLAG;
                else if (cat == KORGANIZER_CATEGORY)
                    flags += Private::KORGANIZER_FLAG;
                else if (cat.startsWith(DEFER_CATEGORY))
                    (flags += Private::DEFER_FLAG) += cat.mid(DEFER_CATEGORY.length());
                else if (cat.startsWith(TEMPL_AFTER_TIME_CATEGORY))
                    (flags += Private::TEMPL_AFTER_TIME_FLAG) += cat.mid(TEMPL_AFTER_TIME_CATEGORY.length());
                else if (cat.startsWith(LATE_CANCEL_CATEGORY))
                    (flags += Private::LATE_CANCEL_FLAG) += cat.mid(LATE_CANCEL_CATEGORY.length());
                else if (cat.startsWith(AUTO_CLOSE_CATEGORY))
                    (flags += Private::AUTO_CLOSE_FLAG) += cat.mid(AUTO_CLOSE_CATEGORY.length());
                else if (cat.startsWith(KMAIL_SERNUM_CATEGORY))
                    (flags += Private::KMAIL_SERNUM_FLAG) += cat.mid(KMAIL_SERNUM_CATEGORY.length());
                else if (cat == ARCHIVE_CATEGORY)
                    event->setCustomProperty(KAlarm::Calendar::APPNAME, ARCHIVE_PROPERTY, QLatin1String("0"));
                else if (cat.startsWith(ARCHIVE_CATEGORIES))
                    event->setCustomProperty(KAlarm::Calendar::APPNAME, ARCHIVE_PROPERTY, cat.mid(ARCHIVE_CATEGORIES.length()));
                else if (cat.startsWith(LOG_CATEGORY))
                    event->setCustomProperty(KAlarm::Calendar::APPNAME, Private::LOG_PROPERTY, cat.mid(LOG_CATEGORY.length()));
                else
                {
                    ++i;   // Not a KAlarm category, so leave it
                    continue;
                }
                cats.removeAt(i);
            }
        }

        if (pre_1_9_2)
        {
            /*
             * It's a KAlarm pre-1.9.2 calendar file.
             * Convert from clock time to the local system time zone.
             */
            event->shiftTimes(KDateTime::ClockTime, localZone);
            converted = true;
        }

        if (addLateCancel)
            (flags += Private::LATE_CANCEL_FLAG) += QLatin1String("1");
        if (!flags.isEmpty())
            event->setCustomProperty(KAlarm::Calendar::APPNAME, Private::FLAGS_PROPERTY, flags.join(Private::SC));
        event->setCategories(cats);


        if ((pre_1_4_14  ||  (pre_1_9_7 && !pre_1_9_0))
        &&  event->recurrence()  &&  event->recurrence()->recurs())
        {
            /*
             * It's a KAlarm pre-1.4.14 or KAlarm 1.9 series pre-1.9.7 calendar file.
             * For recurring events, convert the main alarm offset to an absolute
             * time in the X-KDE-KALARM-NEXTRECUR property, and set main alarm
             * offsets to zero, and convert deferral alarm offsets to be relative to
             * the next recurrence.
             */
            const QStringList flags = event->customProperty(KAlarm::Calendar::APPNAME, Private::FLAGS_PROPERTY).split(Private::SC, QString::SkipEmptyParts);
            const bool dateOnly = flags.contains(Private::DATE_ONLY_FLAG);
            KDateTime startDateTime = event->dtStart();
            if (dateOnly)
                startDateTime.setDateOnly(true);
            // Convert the main alarm and get the next main trigger time from it
            KDateTime nextMainDateTime;
            bool mainExpired = true;
            for (int i = 0, alend = alarms.count();  i < alend;  ++i)
            {
#ifdef USE_AKONADI
                Alarm::Ptr alarm = alarms[i];
#else
                Alarm* alarm = alarms[i];
#endif
                if (!alarm->hasStartOffset())
                    continue;
                // Find whether the alarm triggers at the same time as the main
                // alarm, in which case its offset needs to be set to 0. The
                // following trigger with the main alarm:
                //  - Additional audio alarm
                //  - PRE_ACTION_TYPE
                //  - POST_ACTION_TYPE
                //  - DISPLAYING_TYPE
                bool mainAlarm = true;
                QString property = alarm->customProperty(KAlarm::Calendar::APPNAME, Private::TYPE_PROPERTY);
                QStringList types = property.split(QChar(','), QString::SkipEmptyParts);
                for (int t = 0;  t < types.count();  ++t)
                {
                    QString type = types[t];
                    if (type == Private::AT_LOGIN_TYPE
                    ||  type == Private::TIME_DEFERRAL_TYPE
                    ||  type == Private::DATE_DEFERRAL_TYPE
                    ||  type == Private::REMINDER_TYPE
                    ||  type == REMINDER_ONCE_TYPE)
                    {
                        mainAlarm = false;
                        break;
                    }
                }
                if (mainAlarm)
                {
                    if (mainExpired)
                    {
                        // All main alarms are supposed to be at the same time, so
                        // don't readjust the event's time for subsequent main alarms.
                        mainExpired = false;
                        nextMainDateTime = alarm->time();
                        nextMainDateTime.setDateOnly(dateOnly);
                        nextMainDateTime = nextMainDateTime.toTimeSpec(startDateTime);
                        if (nextMainDateTime != startDateTime)
                        {
                            QDateTime dt = nextMainDateTime.dateTime();
                            event->setCustomProperty(KAlarm::Calendar::APPNAME, Private::NEXT_RECUR_PROPERTY,
                                                     dt.toString(dateOnly ? "yyyyMMdd" : "yyyyMMddThhmmss"));
                        }
                    }
                    alarm->setStartOffset(0);
                    converted = true;
                }
            }
            int adjustment;
            if (mainExpired)
            {
                // It's an expired recurrence.
                // Set the alarm offset relative to the first actual occurrence
                // (taking account of possible exceptions).
                KDateTime dt = event->recurrence()->getNextDateTime(startDateTime.addDays(-1));
                dt.setDateOnly(dateOnly);
                adjustment = startDateTime.secsTo(dt);
            }
            else
                adjustment = startDateTime.secsTo(nextMainDateTime);
            if (adjustment)
            {
                // Convert deferred alarms
                for (int i = 0, alend = alarms.count();  i < alend;  ++i)
                {
#ifdef USE_AKONADI
                    Alarm::Ptr alarm = alarms[i];
#else
                    Alarm* alarm = alarms[i];
#endif
                    if (!alarm->hasStartOffset())
                        continue;
                    const QString property = alarm->customProperty(KAlarm::Calendar::APPNAME, Private::TYPE_PROPERTY);
                    const QStringList types = property.split(QChar(','), QString::SkipEmptyParts);
                    for (int t = 0;  t < types.count();  ++t)
                    {
                        const QString type = types[t];
                        if (type == Private::TIME_DEFERRAL_TYPE
                        ||  type == Private::DATE_DEFERRAL_TYPE)
                        {
                            alarm->setStartOffset(alarm->startOffset().asSeconds() - adjustment);
                            converted = true;
                            break;
                        }
                    }
                }
            }
        }

        if (pre_1_5_0  ||  (pre_1_9_9 && !pre_1_9_0))
        {
            /*
             * It's a KAlarm pre-1.5.0 or KAlarm 1.9 series pre-1.9.9 calendar file.
             * Convert email identity names to uoids.
             */
            for (int i = 0, alend = alarms.count();  i < alend;  ++i)
            {
#ifdef USE_AKONADI
                Alarm::Ptr alarm = alarms[i];
#else
                Alarm* alarm = alarms[i];
#endif
                const QString name = alarm->customProperty(KAlarm::Calendar::APPNAME, KMAIL_ID_PROPERTY);
                if (name.isEmpty())
                    continue;
                const uint id = Identities::identityUoid(name);
                if (id)
                    alarm->setCustomProperty(KAlarm::Calendar::APPNAME, EMAIL_ID_PROPERTY, QString::number(id));
                alarm->removeCustomProperty(KAlarm::Calendar::APPNAME, KMAIL_ID_PROPERTY);
                converted = true;
            }
        }

        if (pre_1_9_10)
        {
            /*
             * It's a KAlarm pre-1.9.10 calendar file.
             * Convert simple repetitions without a recurrence, to a recurrence.
             */
            if (Private::convertRepetition(event))
                converted = true;
        }

        if (pre_2_2_9  ||  (pre_2_3_2 && !pre_2_3_0))
        {
            /*
             * It's a KAlarm pre-2.2.9 or KAlarm 2.3 series pre-2.3.2 calendar file.
             * Set the time in the calendar for all date-only alarms to 00:00.
             */
            if (Private::convertStartOfDay(event))
                converted = true;
        }

        if (pre_2_7_0)
        {
            /*
             * It's a KAlarm pre-2.7.0 calendar file.
             * Archive and at-login flags were stored in event's ARCHIVE property when the main alarm had expired.
             * Reminder parameters were stored in event's ARCHIVE property when no reminder was pending.
             * Negative reminder periods (i.e. alarm offset > 0) were invalid, so convert to 0.
             * Now store reminder information in FLAGS property, whether reminder is pending or not.
             * Move EMAILID, SPEAK, ERRCANCEL and ERRNOSHOW alarm properties into new FLAGS property.
             */
            bool flagsValid = false;
            QStringList flags;
            QString reminder;
            bool    reminderOnce = false;
            const QString prop = event->customProperty(KAlarm::Calendar::APPNAME, ARCHIVE_PROPERTY);
            if (!prop.isEmpty())
            {
                // Convert the event's ARCHIVE property to parameters in the FLAGS property
                flags = event->customProperty(KAlarm::Calendar::APPNAME, Private::FLAGS_PROPERTY).split(Private::SC, QString::SkipEmptyParts);
                flags << Private::ARCHIVE_FLAG;
                flagsValid = true;
                if (prop != QLatin1String("0"))   // "0" was a dummy parameter if no others were present
                {
                    // It's the archive property containing a reminder time and/or repeat-at-login flag.
                    // This was present when no reminder/at-login alarm was pending.
                    const QStringList list = prop.split(Private::SC, QString::SkipEmptyParts);
                    for (int i = 0;  i < list.count();  ++i)
                    {
                        if (list[i] == Private::AT_LOGIN_TYPE)
                            flags << Private::AT_LOGIN_TYPE;
                        else if (list[i] == ARCHIVE_REMINDER_ONCE_TYPE)
                            reminderOnce = true;
                        else if (!list[i].isEmpty()  &&  !list[i].startsWith(QChar::fromLatin1('-')))
                            reminder = list[i];
                    }
                }
                event->setCustomProperty(KAlarm::Calendar::APPNAME, Private::FLAGS_PROPERTY, flags.join(Private::SC));
                event->removeCustomProperty(KAlarm::Calendar::APPNAME, ARCHIVE_PROPERTY);
            }

            for (int i = 0, alend = alarms.count();  i < alend;  ++i)
            {
#ifdef USE_AKONADI
                Alarm::Ptr alarm = alarms[i];
#else
                Alarm* alarm = alarms[i];
#endif
                // Convert EMAILID, SPEAK, ERRCANCEL, ERRNOSHOW properties
                QStringList flags;
                QString property = alarm->customProperty(KAlarm::Calendar::APPNAME, EMAIL_ID_PROPERTY);
                if (!property.isEmpty())
                {
                    flags << Private::EMAIL_ID_FLAG << property;
                    alarm->removeCustomProperty(KAlarm::Calendar::APPNAME, EMAIL_ID_PROPERTY);
                }
                if (!alarm->customProperty(KAlarm::Calendar::APPNAME, SPEAK_PROPERTY).isEmpty())
                {
                    flags << Private::SPEAK_FLAG;
                    alarm->removeCustomProperty(KAlarm::Calendar::APPNAME, SPEAK_PROPERTY);
                }
                if (!alarm->customProperty(KAlarm::Calendar::APPNAME, CANCEL_ON_ERROR_PROPERTY).isEmpty())
                {
                    flags << Private::CANCEL_ON_ERROR_FLAG;
                    alarm->removeCustomProperty(KAlarm::Calendar::APPNAME, CANCEL_ON_ERROR_PROPERTY);
                }
                if (!alarm->customProperty(KAlarm::Calendar::APPNAME, DONT_SHOW_ERROR_PROPERTY).isEmpty())
                {
                    flags << Private::DONT_SHOW_ERROR_FLAG;
                    alarm->removeCustomProperty(KAlarm::Calendar::APPNAME, DONT_SHOW_ERROR_PROPERTY);
                }
                if (!flags.isEmpty())
                    alarm->setCustomProperty(KAlarm::Calendar::APPNAME, Private::FLAGS_PROPERTY, flags.join(Private::SC));

                // Invalidate negative reminder periods in alarms
                if (!alarm->hasStartOffset())
                    continue;
                property = alarm->customProperty(KAlarm::Calendar::APPNAME, Private::TYPE_PROPERTY);
                QStringList types = property.split(QChar::fromLatin1(','), QString::SkipEmptyParts);
                const int r = types.indexOf(REMINDER_ONCE_TYPE);
                if (r >= 0)
                {
                    // Move reminder-once indicator from the alarm to the event's FLAGS property
                    types[r] = Private::REMINDER_TYPE;
                    alarm->setCustomProperty(KAlarm::Calendar::APPNAME, Private::TYPE_PROPERTY, types.join(QChar::fromLatin1(',')));
                    reminderOnce = true;
                }
                if (r >= 0  ||  types.contains(Private::REMINDER_TYPE))
                {
                    // The alarm is a reminder alarm
                    const int offset = alarm->startOffset().asSeconds();
                    if (offset > 0)
                    {
                        alarm->setStartOffset(0);
                        converted = true;
                    }
                    else if (offset < 0)
                        reminder = reminderToString(offset / 60);
                }
            }
            if (!reminder.isEmpty())
            {
                // Write reminder parameters into the event's FLAGS property
                if (!flagsValid)
                    flags = event->customProperty(KAlarm::Calendar::APPNAME, Private::FLAGS_PROPERTY).split(Private::SC, QString::SkipEmptyParts);
                if (flags.indexOf(Private::REMINDER_TYPE) < 0)
                {
                    flags += Private::REMINDER_TYPE;
                    if (reminderOnce)
                        flags += Private::REMINDER_ONCE_FLAG;
                    flags += reminder;
                }
            }
        }

        if (readOnly)
            event->setReadOnly(true);
        event->endUpdates();     // finally issue an update notification
    }
    return converted;
}

/******************************************************************************
* Set the time for a date-only event to 00:00.
* Reply = true if the event was updated.
*/
#ifdef USE_AKONADI
bool KAEvent::Private::convertStartOfDay(const Event::Ptr& event)
#else
bool KAEvent::Private::convertStartOfDay(Event* event)
#endif
{
    bool changed = false;
    const QTime midnight(0, 0);
    const QStringList flags = event->customProperty(KAlarm::Calendar::APPNAME, Private::FLAGS_PROPERTY).split(Private::SC, QString::SkipEmptyParts);
    if (flags.indexOf(Private::DATE_ONLY_FLAG) >= 0)
    {
        // It's an untimed event, so fix it
        const KDateTime oldDt = event->dtStart();
        const int adjustment = oldDt.time().secsTo(midnight);
        if (adjustment)
        {
            event->setDtStart(KDateTime(oldDt.date(), midnight, oldDt.timeSpec()));
            int deferralOffset = 0;
            AlarmMap alarmMap;
            readAlarms(event, &alarmMap);
            for (AlarmMap::ConstIterator it = alarmMap.constBegin();  it != alarmMap.constEnd();  ++it)
            {
                const AlarmData& data = it.value();
                if (!data.alarm->hasStartOffset())
                    continue;
                if (data.timedDeferral)
                {
                    // Found a timed deferral alarm, so adjust the offset
                    deferralOffset = data.alarm->startOffset().asSeconds();
#ifdef USE_AKONADI
                    const_cast<Alarm*>(data.alarm.data())->setStartOffset(deferralOffset - adjustment);
#else
                    const_cast<Alarm*>(data.alarm)->setStartOffset(deferralOffset - adjustment);
#endif
                }
                else if (data.type == AUDIO_ALARM
                &&       data.alarm->startOffset().asSeconds() == deferralOffset)
                {
                    // Audio alarm is set for the same time as the above deferral alarm
#ifdef USE_AKONADI
                    const_cast<Alarm*>(data.alarm.data())->setStartOffset(deferralOffset - adjustment);
#else
                    const_cast<Alarm*>(data.alarm)->setStartOffset(deferralOffset - adjustment);
#endif
                }
            }
            changed = true;
        }
    }
    else
    {
        // It's a timed event. Fix any untimed alarms.
        bool foundDeferral = false;
        int deferralOffset = 0;
        int newDeferralOffset = 0;
        DateTime start;
        const KDateTime nextMainDateTime = readDateTime(event, false, start).kDateTime();
        AlarmMap alarmMap;
        readAlarms(event, &alarmMap);
        for (AlarmMap::ConstIterator it = alarmMap.constBegin();  it != alarmMap.constEnd();  ++it)
        {
            const AlarmData& data = it.value();
            if (!data.alarm->hasStartOffset())
                continue;
            if ((data.type & DEFERRED_ALARM)  &&  !data.timedDeferral)
            {
                // Found a date-only deferral alarm, so adjust its time
                KDateTime altime = data.alarm->startOffset().end(nextMainDateTime);
                altime.setTime(midnight);
                deferralOffset = data.alarm->startOffset().asSeconds();
                newDeferralOffset = event->dtStart().secsTo(altime);
#ifdef USE_AKONADI
                const_cast<Alarm*>(data.alarm.data())->setStartOffset(newDeferralOffset);
#else
                const_cast<Alarm*>(data.alarm)->setStartOffset(newDeferralOffset);
#endif
                foundDeferral = true;
                changed = true;
            }
            else if (foundDeferral
                 &&  data.type == AUDIO_ALARM
                 &&  data.alarm->startOffset().asSeconds() == deferralOffset)
            {
                // Audio alarm is set for the same time as the above deferral alarm
#ifdef USE_AKONADI
                const_cast<Alarm*>(data.alarm.data())->setStartOffset(newDeferralOffset);
#else
                const_cast<Alarm*>(data.alarm)->setStartOffset(newDeferralOffset);
#endif
                changed = true;
            }
        }
    }
    return changed;
}

/******************************************************************************
* Convert simple repetitions in an event without a recurrence, to a
* recurrence. Repetitions which are an exact multiple of 24 hours are converted
* to daily recurrences; else they are converted to minutely recurrences. Note
* that daily and minutely recurrences produce different results when they span
* a daylight saving time change.
* Reply = true if any conversions were done.
*/
#ifdef USE_AKONADI
bool KAEvent::Private::convertRepetition(const Event::Ptr& event)
#else
bool KAEvent::Private::convertRepetition(Event* event)
#endif
{
    const Alarm::List alarms = event->alarms();
    if (alarms.isEmpty())
        return false;
    Recurrence* recur = event->recurrence();   // guaranteed to return non-null
    if (recur->recurs())
        return false;
    bool converted = false;
    const bool readOnly = event->isReadOnly();
    for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
    {
#ifdef USE_AKONADI
        Alarm::Ptr alarm = alarms[ai];
#else
        Alarm* alarm = alarms[ai];
#endif
        if (alarm->repeatCount() > 0  &&  alarm->snoozeTime().value() > 0)
        {
            if (!converted)
            {
                event->startUpdates();   // prevent multiple update notifications
                if (readOnly)
                    event->setReadOnly(false);
                if ((alarm->snoozeTime().asSeconds() % (24*3600)) != 0)
                    recur->setMinutely(alarm->snoozeTime().asSeconds() / 60);
                else
                    recur->setDaily(alarm->snoozeTime().asDays());
                recur->setDuration(alarm->repeatCount() + 1);
                converted = true;
            }
            alarm->setRepeatCount(0);
            alarm->setSnoozeTime(0);
        }
    }
    if (converted)
    {
        if (readOnly)
            event->setReadOnly(true);
        event->endUpdates();     // finally issue an update notification
    }
    return converted;
}



/*=============================================================================
= Class KAAlarm
= Corresponds to a single KCal::Alarm instance.
=============================================================================*/

KAAlarm::KAAlarm()
    : d(new Private)
{
}

KAAlarm::Private::Private()
    : mType(INVALID_ALARM),
      mNextRepeat(0),
      mRepeatAtLogin(false),
      mDeferred(false)
{
}

KAAlarm::KAAlarm(const KAAlarm& other)
    : d(new Private(*other.d))
{
}

KAAlarm::~KAAlarm()
{
    delete d;
}

KAAlarm& KAAlarm::operator=(const KAAlarm& other)
{
    if (&other != this)
        *d = *other.d;
    return *this;
}

KAAlarm::Action KAAlarm::action() const
{
    return d->mActionType;
}

bool KAAlarm::isValid() const
{
    return d->mType != INVALID_ALARM;
}

KAAlarm::Type KAAlarm::type() const
{
    return d->mType;
}

DateTime KAAlarm::dateTime(bool withRepeats) const
{
    return (withRepeats && d->mNextRepeat && d->mRepetition)
         ? d->mRepetition.duration(d->mNextRepeat).end(d->mNextMainDateTime.kDateTime())
         : d->mNextMainDateTime;
}

QDate KAAlarm::date() const
{
    return d->mNextMainDateTime.date();
}

QTime KAAlarm::time() const
{
    return d->mNextMainDateTime.effectiveTime();
}

bool KAAlarm::repeatAtLogin() const
{
    return d->mRepeatAtLogin;
}

bool KAAlarm::isReminder() const
{
    return d->mType == REMINDER_ALARM;
}

bool KAAlarm::deferred() const
{
    return d->mDeferred;
}

bool KAAlarm::timedDeferral() const
{
    return d->mDeferred && d->mTimedDeferral;
}

void KAAlarm::setTime(const DateTime& dt)
{
    d->mNextMainDateTime = dt;
}

void KAAlarm::setTime(const KDateTime& dt)
{
    d->mNextMainDateTime = dt;
}

#ifdef KDE_NO_DEBUG_OUTPUT
const char* KAAlarm::debugType(Type)  { return ""; }
#else
const char* KAAlarm::debugType(Type type)
{
    switch (type)
    {
        case MAIN_ALARM:               return "MAIN";
        case REMINDER_ALARM:           return "REMINDER";
        case DEFERRED_ALARM:           return "DEFERRED";
        case DEFERRED_REMINDER_ALARM:  return "DEFERRED_REMINDER";
        case AT_LOGIN_ALARM:           return "LOGIN";
        case DISPLAYING_ALARM:         return "DISPLAYING";
        default:                       return "INVALID";
    }
}
#endif


/*=============================================================================
= Class EmailAddressList
=============================================================================*/

/******************************************************************************
* Sets the list of email addresses, removing any empty addresses.
* Reply = false if empty addresses were found.
*/
#ifdef USE_AKONADI
EmailAddressList& EmailAddressList::operator=(const Person::List& addresses)
#else
EmailAddressList& EmailAddressList::operator=(const QList<Person>& addresses)
#endif
{
    clear();
    for (int p = 0, end = addresses.count();  p < end;  ++p)
    {
#ifdef USE_AKONADI
        if (!addresses[p]->email().isEmpty())
#else
        if (!addresses[p].email().isEmpty())
#endif
            append(addresses[p]);
    }
    return *this;
}

/******************************************************************************
* Return the email address list as a string list of email addresses.
*/
EmailAddressList::operator QStringList() const
{
    QStringList list;
    for (int p = 0, end = count();  p < end;  ++p)
        list += address(p);
    return list;
}

/******************************************************************************
* Return the email address list as a string, each address being delimited by
* the specified separator string.
*/
QString EmailAddressList::join(const QString& separator) const
{
    QString result;
    bool first = true;
    for (int p = 0, end = count();  p < end;  ++p)
    {
        if (first)
            first = false;
        else
            result += separator;
        result += address(p);
    }
    return result;
}

/******************************************************************************
* Convert one item into an email address, including name.
*/
QString EmailAddressList::address(int index) const
{
    if (index < 0  ||  index > count())
        return QString();
    QString result;
    bool quote = false;
#ifdef USE_AKONADI
    const Person::Ptr person = (*this)[index];
    const QString name = person->name();
#else
    const Person person = (*this)[index];
    const QString name = person.name();
#endif
    if (!name.isEmpty())
    {
        // Need to enclose the name in quotes if it has any special characters
        for (int i = 0, len = name.length();  i < len;  ++i)
        {
            const QChar ch = name[i];
            if (!ch.isLetterOrNumber())
            {
                quote = true;
                result += '\"';
                break;
            }
        }
#ifdef USE_AKONADI
        result += (*this)[index]->name();
#else
        result += (*this)[index].name();
#endif
        result += (quote ? "\" <" : " <");
        quote = true;    // need angle brackets round email address
    }

#ifdef USE_AKONADI
    result += person->email();
#else
    result += person.email();
#endif
    if (quote)
        result += '>';
    return result;
}

/******************************************************************************
* Return a list of the pure email addresses, excluding names.
*/
QStringList EmailAddressList::pureAddresses() const
{
    QStringList list;
    for (int p = 0, end = count();  p < end;  ++p)
#ifdef USE_AKONADI
        list += at(p)->email();
#else
        list += at(p).email();
#endif
    return list;
}

/******************************************************************************
* Return a list of the pure email addresses, excluding names, as a string.
*/
QString EmailAddressList::pureAddresses(const QString& separator) const
{
    QString result;
    bool first = true;
    for (int p = 0, end = count();  p < end;  ++p)
    {
        if (first)
            first = false;
        else
            result += separator;
#ifdef USE_AKONADI
        result += at(p)->email();
#else
        result += at(p).email();
#endif
    }
    return result;
}


/*=============================================================================
= Static functions
=============================================================================*/

/******************************************************************************
* Set the specified alarm to be a procedure alarm with the given command line.
* The command line is first split into its program file and arguments before
* initialising the alarm.
*/
#ifdef USE_AKONADI
static void setProcedureAlarm(const Alarm::Ptr& alarm, const QString& commandLine)
#else
static void setProcedureAlarm(Alarm* alarm, const QString& commandLine)
#endif
{
    QString command;
    QString arguments;
    QChar quoteChar;
    bool quoted = false;
    const uint posMax = commandLine.length();
    uint pos;
    for (pos = 0;  pos < posMax;  ++pos)
    {
        const QChar ch = commandLine[pos];
        if (quoted)
        {
            if (ch == quoteChar)
            {
                ++pos;    // omit the quote character
                break;
            }
            command += ch;
        }
        else
        {
            bool done = false;
            switch (ch.toAscii())
            {
                case ' ':
                case ';':
                case '|':
                case '<':
                case '>':
                    done = !command.isEmpty();
                    break;
                case '\'':
                case '"':
                    if (command.isEmpty())
                    {
                        // Start of a quoted string. Omit the quote character.
                        quoted = true;
                        quoteChar = ch;
                        break;
                    }
                    // fall through to default
                default:
                    command += ch;
                    break;
            }
            if (done)
                break;
        }
    }

    // Skip any spaces after the command
    for ( ;  pos < posMax  &&  commandLine[pos] == QLatin1Char(' ');  ++pos) ;
    arguments = commandLine.mid(pos);

    alarm->setProcedureAlarm(command, arguments);
}

/******************************************************************************
* Converts a reminder interval into a parameter string for the
* X-KDE-KALARM-FLAGS property.
*/
QString reminderToString(int minutes)
{
        char unit = 'M';
        int count = abs(minutes);
        if (count % 1440 == 0)
        {
            unit = 'D';
            count /= 1440;
        }
        else if (count % 60 == 0)
        {
            unit = 'H';
            count /= 60;
        }
        if (minutes < 0)
            count = -count;
        return QString("%1%2").arg(count).arg(unit);
}

// vim: et sw=4:
