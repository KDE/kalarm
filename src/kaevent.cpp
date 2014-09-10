/*
 *  kaevent.cpp  -  represents calendar events
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  Copyright © 2001-2013 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#include "kaevent.h"

#include "alarmtext.h"
#include "identities.h"
#include "version.h"

#include <kcalcore/memorycalendar.h>
#include <kholidays/holidays.h>
using namespace KHolidays;

#include <ksystemtimezone.h>
#include <klocalizedstring.h>
#include <kglobal.h>
#include <qdebug.h>

using namespace KCalCore;
using namespace KHolidays;

namespace KAlarmCal
{

//=============================================================================

typedef KCalCore::Person  EmailAddress;
class EmailAddressList : public KCalCore::Person::List
{
public:
    EmailAddressList() : KCalCore::Person::List() { }
    EmailAddressList(const KCalCore::Person::List &list)
    {
        operator=(list);
    }
    EmailAddressList &operator=(const KCalCore::Person::List &);
    operator QStringList() const;
    QString     join(const QString &separator) const;
    QStringList pureAddresses() const;
    QString     pureAddresses(const QString &separator) const;
private:
    QString     address(int index) const;
};

//=============================================================================

class KAAlarm::Private
{
public:
    Private();

    Action      mActionType;       // alarm action type
    Type        mType;             // alarm type
    DateTime    mNextMainDateTime; // next time to display the alarm, excluding repetitions
    Repetition  mRepetition;       // sub-repetition count and interval
    int         mNextRepeat;       // repetition count of next due sub-repetition
    bool        mRepeatAtLogin;    // whether to repeat the alarm at every login
    bool        mRecurs;           // there is a recurrence rule for the alarm
    bool        mDeferred;         // whether the alarm is an extra deferred/deferred-reminder alarm
    bool        mTimedDeferral;    // if mDeferred = true: true if the deferral is timed, false if date-only
};

//=============================================================================

class KAEventPrivate : public QSharedData
{
public:
    // Read-only internal flags additional to KAEvent::Flags enum values.
    // NOTE: If any values are added to those in KAEvent::Flags, ensure
    //       that these values don't overlap them.
    enum {
        REMINDER        = 0x100000,
        DEFERRAL        = 0x200000,
        TIMED_FLAG      = 0x400000,
        DATE_DEFERRAL   = DEFERRAL,
        TIME_DEFERRAL   = DEFERRAL | TIMED_FLAG,
        DISPLAYING_     = 0x800000,
        READ_ONLY_FLAGS = 0xF00000  //!< mask for all read-only internal values
    };
    enum ReminderType { // current active state of reminder
        NO_REMINDER,       // reminder is not due
        ACTIVE_REMINDER,   // reminder is due
        HIDDEN_REMINDER    // reminder-after is disabled due to main alarm being deferred past it
    };
    enum DeferType {
        NO_DEFERRAL = 0,   // there is no deferred alarm
        NORMAL_DEFERRAL,   // the main alarm, a recurrence or a repeat is deferred
        REMINDER_DEFERRAL  // a reminder alarm is deferred
    };
    // Alarm types.
    // This uses the same scheme as KAAlarm::Type, with some extra values.
    // Note that the actual enum values need not be the same as in KAAlarm::Type.
    enum AlarmType {
        INVALID_ALARM       = 0,     // Not an alarm
        MAIN_ALARM          = 1,     // THE real alarm. Must be the first in the enumeration.
        REMINDER_ALARM      = 0x02,  // Reminder in advance of/after the main alarm
        DEFERRED_ALARM      = 0x04,  // Deferred alarm
        DEFERRED_REMINDER_ALARM = REMINDER_ALARM | DEFERRED_ALARM,  // Deferred reminder alarm
        // The following values must be greater than the preceding ones, to
        // ensure that in ordered processing they are processed afterwards.
        AT_LOGIN_ALARM      = 0x10,  // Additional repeat-at-login trigger
        DISPLAYING_ALARM    = 0x20,  // Copy of the alarm currently being displayed
        // The following are extra internal KAEvent values
        AUDIO_ALARM         = 0x30,  // sound to play when displaying the alarm
        PRE_ACTION_ALARM    = 0x40,  // command to execute before displaying the alarm
        POST_ACTION_ALARM   = 0x50   // command to execute after the alarm window is closed
    };

    struct AlarmData {
        Alarm::Ptr                  alarm;
        QString                     cleanText;       // text or audio file name
        uint                        emailFromId;
        QFont                       font;
        QColor                      bgColour, fgColour;
        float                       soundVolume;
        float                       fadeVolume;
        int                         fadeSeconds;
        int                         repeatSoundPause;
        int                         nextRepeat;
        bool                        speak;
        KAEventPrivate::AlarmType   type;
        KAAlarm::Action             action;
        int                         displayingFlags;
        KAEvent::ExtraActionOptions extraActionOptions;
        bool                        defaultFont;
        bool                        isEmailText;
        bool                        commandScript;
        bool                        timedDeferral;
        bool                        hiddenReminder;
    };
    typedef QMap<AlarmType, AlarmData> AlarmMap;

    KAEventPrivate();
    KAEventPrivate(const KDateTime &, const QString &message, const QColor &bg, const QColor &fg,
                   const QFont &f, KAEvent::SubAction, int lateCancel, KAEvent::Flags flags,
                   bool changesPending = false);
    explicit KAEventPrivate(const KCalCore::Event::Ptr &);
    KAEventPrivate(const KAEventPrivate &);
    ~KAEventPrivate()
    {
        delete mRecurrence;
    }
    KAEventPrivate    &operator=(const KAEventPrivate &e)
    {
        if (&e != this) {
            copy(e);
        }  return *this;
    }
    void               set(const KCalCore::Event::Ptr &);
    void               set(const KDateTime &, const QString &message, const QColor &bg, const QColor &fg,
                           const QFont &, KAEvent::SubAction, int lateCancel, KAEvent::Flags flags,
                           bool changesPending = false);
    void               setAudioFile(const QString &filename, float volume, float fadeVolume, int fadeSeconds, int repeatPause, bool allowEmptyFile);
    KAEvent::OccurType setNextOccurrence(const KDateTime &preDateTime);
    void               setFirstRecurrence();
    void               setCategory(CalEvent::Type);
    void               setRepeatAtLogin(bool);
    void               setRepeatAtLoginTrue(bool clearReminder);
    void               setReminder(int minutes, bool onceOnly);
    void               activateReminderAfter(const DateTime &mainAlarmTime);
    void               defer(const DateTime &, bool reminder, bool adjustRecurrence = false);
    void               cancelDefer();
    bool               setDisplaying(const KAEventPrivate &, KAAlarm::Type, Akonadi::Collection::Id, const KDateTime &dt, bool showEdit, bool showDefer);
    void               reinstateFromDisplaying(const KCalCore::Event::Ptr &, Akonadi::Collection::Id &, bool &showEdit, bool &showDefer);
    void               startChanges()
    {
        ++mChangeCount;
    }
    void               endChanges();
    void               removeExpiredAlarm(KAAlarm::Type);
    KAAlarm            alarm(KAAlarm::Type) const;
    KAAlarm            firstAlarm() const;
    KAAlarm            nextAlarm(KAAlarm::Type) const;
    bool               updateKCalEvent(const KCalCore::Event::Ptr &, KAEvent::UidAction, bool setCustomProperties = true) const;
    DateTime           mainDateTime(bool withRepeats = false) const
    {
        return (withRepeats && mNextRepeat && mRepetition)
             ? mRepetition.duration(mNextRepeat).end(mNextMainDateTime.kDateTime()) : mNextMainDateTime;
    }
    DateTime           mainEndRepeatTime() const
    {
        return mRepetition ? mRepetition.duration().end(mNextMainDateTime.kDateTime()) : mNextMainDateTime;
    }
    DateTime           deferralLimit(KAEvent::DeferLimitType * = 0) const;
    KAEvent::Flags     flags() const;
    bool               isWorkingTime(const KDateTime &) const;
    bool               setRepetition(const Repetition &);
    bool               occursAfter(const KDateTime &preDateTime, bool includeRepetitions) const;
    KAEvent::OccurType nextOccurrence(const KDateTime &preDateTime, DateTime &result, KAEvent::OccurOption = KAEvent::IGNORE_REPETITION) const;
    KAEvent::OccurType previousOccurrence(const KDateTime &afterDateTime, DateTime &result, bool includeRepetitions = false) const;
    void               setRecurrence(const KARecurrence &);
    bool               setRecur(KCalCore::RecurrenceRule::PeriodType, int freq, int count, const QDate &end, KARecurrence::Feb29Type = KARecurrence::Feb29_None);
    bool               setRecur(KCalCore::RecurrenceRule::PeriodType, int freq, int count, const KDateTime &end, KARecurrence::Feb29Type = KARecurrence::Feb29_None);
    KARecurrence::Type checkRecur() const;
    void               clearRecur();
    void               calcTriggerTimes() const;
#ifdef KDE_NO_DEBUG_OUTPUT
    void               dumpDebug() const  { }
#else
    void               dumpDebug() const;
#endif
    static bool        convertRepetition(const KCalCore::Event::Ptr &);
    static bool        convertStartOfDay(const KCalCore::Event::Ptr &);
    static DateTime    readDateTime(const KCalCore::Event::Ptr &, bool dateOnly, DateTime &start);
    static void        readAlarms(const KCalCore::Event::Ptr &, void *alarmMap, bool cmdDisplay = false);
    static void        readAlarm(const KCalCore::Alarm::Ptr &, AlarmData &, bool audioMain, bool cmdDisplay = false);
private:
    void               copy(const KAEventPrivate &);
    bool               mayOccurDailyDuringWork(const KDateTime &) const;
    int                nextWorkRepetition(const KDateTime &pre) const;
    void               calcNextWorkingTime(const DateTime &nextTrigger) const;
    DateTime           nextWorkingTime() const;
    KAEvent::OccurType nextRecurrence(const KDateTime &preDateTime, DateTime &result) const;
    void               setAudioAlarm(const KCalCore::Alarm::Ptr &) const;
    KCalCore::Alarm::Ptr initKCalAlarm(const KCalCore::Event::Ptr &, const DateTime &, const QStringList &types, AlarmType = INVALID_ALARM) const;
    KCalCore::Alarm::Ptr initKCalAlarm(const KCalCore::Event::Ptr &, int startOffsetSecs, const QStringList &types, AlarmType = INVALID_ALARM) const;
    inline void        set_deferral(DeferType);
    inline void        activate_reminder(bool activate);

public:
    static QFont       mDefaultFont;       // default alarm message font
    static const KHolidays::HolidayRegion *mHolidays;  // holiday region to use
    static QBitArray   mWorkDays;          // working days of the week
    static QTime       mWorkDayStart;      // start time of the working day
    static QTime       mWorkDayEnd;        // end time of the working day
    static int         mWorkTimeIndex;     // incremented every time working days/times are changed
    mutable DateTime   mAllTrigger;        // next trigger time, including reminders, ignoring working hours
    mutable DateTime   mMainTrigger;       // next trigger time, ignoring reminders and working hours
    mutable DateTime   mAllWorkTrigger;    // next trigger time, taking account of reminders and working hours
    mutable DateTime   mMainWorkTrigger;   // next trigger time, ignoring reminders but taking account of working hours
    mutable KAEvent::CmdErrType mCommandError; // command execution error last time the alarm triggered

    QString            mEventID;           // UID: KCal::Event unique ID
    QString            mTemplateName;      // alarm template's name, or null if normal event
    QMap<QByteArray, QString> mCustomProperties;  // KCal::Event's non-KAlarm custom properties
    Akonadi::Item::Id  mItemId;            // Akonadi::Item ID for this event
    mutable Akonadi::Collection::Id mCollectionId; // ID of collection containing the event, or for a displaying event,
    // saved collection ID (not the collection the event is in)
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
    KARecurrence      *mRecurrence;        // RECUR: recurrence specification, or 0 if none
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
    int                mFadeSeconds;       // fade time (seconds) for sound file, or 0 if none
    int                mRepeatSoundPause;  // seconds to pause between sound file repetitions, or -1 if no repetition
    int                mLateCancel;        // how many minutes late will cancel the alarm, or 0 for no cancellation
    mutable const KHolidays::HolidayRegion *
    mExcludeHolidays;   // non-null to not trigger alarms on holidays (= mHolidays when trigger calculated)
    mutable int        mWorkTimeOnly;      // non-zero to trigger alarm only during working hours (= mWorkTimeIndex when trigger calculated)
    KAEvent::SubAction mActionSubType;     // sub-action type for the event's main alarm
    CalEvent::Type     mCategory;      // event category (active, archived, template, ...)
    KAEvent::ExtraActionOptions mExtraActionOptions;// options for pre- or post-alarm actions
    KACalendar::Compat mCompatibility; // event's storage format compatibility
    bool               mReadOnly;          // event is read-only in its original calendar file
    bool               mConfirmAck;        // alarm acknowledgement requires confirmation by user
    bool               mUseDefaultFont;    // use default message font, not mFont
    bool               mCommandScript;     // the command text is a script, not a shell command line
    bool               mCommandXterm;      // command alarm is to be executed in a terminal window
    bool               mCommandDisplay;    // command output is to be displayed in an alarm window
    bool               mEmailBcc;          // blind copy the email to the user
    bool               mBeep;              // whether to beep when the alarm is displayed
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
    static const QString EXEC_ON_DEFERRAL_FLAG;
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

//=============================================================================

// KAlarm version which first used the current calendar/event format.
// If this changes, KAEvent::convertKCalEvents() must be changed correspondingly.
// The string version is the KAlarm version string used in the calendar file.

//QT5 Fix library version
QByteArray KAEvent::currentCalendarVersionString()
{
    return QByteArray("2.7.0");
}
int        KAEvent::currentCalendarVersion()
{
    return Version(2, 7, 0);
}

// Custom calendar properties.
// Note that all custom property names are prefixed with X-KDE-KALARM- in the calendar file.

// Event properties
const QByteArray KAEventPrivate::FLAGS_PROPERTY("FLAGS");              // X-KDE-KALARM-FLAGS property
const QString    KAEventPrivate::DATE_ONLY_FLAG        = QLatin1String("DATE");
const QString    KAEventPrivate::EMAIL_BCC_FLAG        = QLatin1String("BCC");
const QString    KAEventPrivate::CONFIRM_ACK_FLAG      = QLatin1String("ACKCONF");
const QString    KAEventPrivate::KORGANIZER_FLAG       = QLatin1String("KORG");
const QString    KAEventPrivate::EXCLUDE_HOLIDAYS_FLAG = QLatin1String("EXHOLIDAYS");
const QString    KAEventPrivate::WORK_TIME_ONLY_FLAG   = QLatin1String("WORKTIME");
const QString    KAEventPrivate::REMINDER_ONCE_FLAG    = QLatin1String("ONCE");
const QString    KAEventPrivate::DEFER_FLAG            = QLatin1String("DEFER");   // default defer interval for this alarm
const QString    KAEventPrivate::LATE_CANCEL_FLAG      = QLatin1String("LATECANCEL");
const QString    KAEventPrivate::AUTO_CLOSE_FLAG       = QLatin1String("LATECLOSE");
const QString    KAEventPrivate::TEMPL_AFTER_TIME_FLAG = QLatin1String("TMPLAFTTIME");
const QString    KAEventPrivate::KMAIL_SERNUM_FLAG     = QLatin1String("KMAIL");
const QString    KAEventPrivate::ARCHIVE_FLAG          = QLatin1String("ARCHIVE");

const QByteArray KAEventPrivate::NEXT_RECUR_PROPERTY("NEXTRECUR");     // X-KDE-KALARM-NEXTRECUR property
const QByteArray KAEventPrivate::REPEAT_PROPERTY("REPEAT");            // X-KDE-KALARM-REPEAT property
const QByteArray KAEventPrivate::LOG_PROPERTY("LOG");                  // X-KDE-KALARM-LOG property
const QString    KAEventPrivate::xtermURL = QLatin1String("xterm:");
const QString    KAEventPrivate::displayURL = QLatin1String("display:");

// - General alarm properties
const QByteArray KAEventPrivate::TYPE_PROPERTY("TYPE");                // X-KDE-KALARM-TYPE property
const QString    KAEventPrivate::FILE_TYPE                  = QLatin1String("FILE");
const QString    KAEventPrivate::AT_LOGIN_TYPE              = QLatin1String("LOGIN");
const QString    KAEventPrivate::REMINDER_TYPE              = QLatin1String("REMINDER");
const QString    KAEventPrivate::TIME_DEFERRAL_TYPE         = QLatin1String("DEFERRAL");
const QString    KAEventPrivate::DATE_DEFERRAL_TYPE         = QLatin1String("DATE_DEFERRAL");
const QString    KAEventPrivate::DISPLAYING_TYPE            = QLatin1String("DISPLAYING");   // used only in displaying calendar
const QString    KAEventPrivate::PRE_ACTION_TYPE            = QLatin1String("PRE");
const QString    KAEventPrivate::POST_ACTION_TYPE           = QLatin1String("POST");
const QString    KAEventPrivate::SOUND_REPEAT_TYPE          = QLatin1String("SOUNDREPEAT");
const QByteArray KAEventPrivate::NEXT_REPEAT_PROPERTY("NEXTREPEAT");   // X-KDE-KALARM-NEXTREPEAT property
const QString    KAEventPrivate::HIDDEN_REMINDER_FLAG = QLatin1String("HIDE");
// - Display alarm properties
const QByteArray KAEventPrivate::FONT_COLOUR_PROPERTY("FONTCOLOR");    // X-KDE-KALARM-FONTCOLOR property
// - Email alarm properties
const QString    KAEventPrivate::EMAIL_ID_FLAG        = QLatin1String("EMAILID");
// - Audio alarm properties
const QByteArray KAEventPrivate::VOLUME_PROPERTY("VOLUME");            // X-KDE-KALARM-VOLUME property
const QString    KAEventPrivate::SPEAK_FLAG           = QLatin1String("SPEAK");
// - Command alarm properties
const QString    KAEventPrivate::EXEC_ON_DEFERRAL_FLAG = QLatin1String("EXECDEFER");
const QString    KAEventPrivate::CANCEL_ON_ERROR_FLAG  = QLatin1String("ERRCANCEL");
const QString    KAEventPrivate::DONT_SHOW_ERROR_FLAG  = QLatin1String("ERRNOSHOW");

// Event status strings
const QString    KAEventPrivate::DISABLED_STATUS            = QLatin1String("DISABLED");

// Displaying event ID identifier
const QString    KAEventPrivate::DISP_DEFER = QLatin1String("DEFER");
const QString    KAEventPrivate::DISP_EDIT  = QLatin1String("EDIT");

// Command error strings
const QString    KAEventPrivate::CMD_ERROR_VALUE      = QLatin1String("MAIN");
const QString    KAEventPrivate::CMD_ERROR_PRE_VALUE  = QLatin1String("PRE");
const QString    KAEventPrivate::CMD_ERROR_POST_VALUE = QLatin1String("POST");

const QString    KAEventPrivate::SC = QLatin1String(";");

QFont                           KAEventPrivate::mDefaultFont;
const KHolidays::HolidayRegion *KAEventPrivate::mHolidays = 0;
QBitArray                       KAEventPrivate::mWorkDays(7);
QTime                           KAEventPrivate::mWorkDayStart(9, 0, 0);
QTime                           KAEventPrivate::mWorkDayEnd(17, 0, 0);
int                             KAEventPrivate::mWorkTimeIndex = 1;

static void setProcedureAlarm(const Alarm::Ptr &, const QString &commandLine);
static QString reminderToString(int minutes);

/*=============================================================================
= Class KAEvent
= Corresponds to a KCal::Event instance.
=============================================================================*/

inline void KAEventPrivate::set_deferral(DeferType type)
{
    if (type) {
        if (mDeferral == NO_DEFERRAL) {
            ++mAlarmCount;
        }
    } else {
        if (mDeferral != NO_DEFERRAL) {
            --mAlarmCount;
        }
    }
    mDeferral = type;
}

inline void KAEventPrivate::activate_reminder(bool activate)
{
    if (activate  &&  mReminderActive != ACTIVE_REMINDER  &&  mReminderMinutes) {
        if (mReminderActive == NO_REMINDER) {
            ++mAlarmCount;
        }
        mReminderActive = ACTIVE_REMINDER;
    } else if (!activate  &&  mReminderActive != NO_REMINDER) {
        mReminderActive = NO_REMINDER;
        mReminderAfterTime = DateTime();
        --mAlarmCount;
    }
}

K_GLOBAL_STATIC_WITH_ARGS(QSharedDataPointer<KAEventPrivate>,
                          emptyKAEventPrivate, (new KAEventPrivate))

KAEvent::KAEvent()
    : d(*emptyKAEventPrivate)
{ }

KAEventPrivate::KAEventPrivate()
    :
    mCommandError(KAEvent::CMD_NO_ERROR),
    mItemId(-1),
    mCollectionId(-1),
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
    mCategory(CalEvent::EMPTY),
    mCompatibility(KACalendar::Current),
    mReadOnly(false),
    mConfirmAck(false),
    mEmailBcc(false),
    mBeep(false),
    mAutoClose(false),
    mRepeatAtLogin(false),
    mDisplaying(false)
{ }

KAEvent::KAEvent(const KDateTime &dt, const QString &message, const QColor &bg, const QColor &fg, const QFont &f,
                 SubAction action, int lateCancel, Flags flags, bool changesPending)
    : d(new KAEventPrivate(dt, message, bg, fg, f, action, lateCancel, flags, changesPending))
{
}

KAEventPrivate::KAEventPrivate(const KDateTime &dt, const QString &message, const QColor &bg, const QColor &fg, const QFont &f,
                               KAEvent::SubAction action, int lateCancel, KAEvent::Flags flags, bool changesPending)
    : mRecurrence(0)
{
    set(dt, message, bg, fg, f, action, lateCancel, flags, changesPending);
}

KAEvent::KAEvent(const Event::Ptr &e)
    : d(new KAEventPrivate(e))
{
}

KAEventPrivate::KAEventPrivate(const Event::Ptr &e)
    : mRecurrence(0)
{
    set(e);
}

KAEventPrivate::KAEventPrivate(const KAEventPrivate &e)
    : QSharedData(e),
      mRecurrence(0)
{
    copy(e);
}

KAEvent::KAEvent(const KAEvent &other)
    : d(other.d)
{ }

KAEvent::~KAEvent()
{ }

KAEvent &KAEvent::operator=(const KAEvent &other)
{
    if (&other != this) {
        d = other.d;
    }
    return *this;
}

/******************************************************************************
* Copies the data from another instance.
*/
void KAEventPrivate::copy(const KAEventPrivate &event)
{
    mAllTrigger              = event.mAllTrigger;
    mMainTrigger             = event.mMainTrigger;
    mAllWorkTrigger          = event.mAllWorkTrigger;
    mMainWorkTrigger         = event.mMainWorkTrigger;
    mCommandError            = event.mCommandError;
    mEventID                 = event.mEventID;
    mTemplateName            = event.mTemplateName;
    mCustomProperties        = event.mCustomProperties;
    mItemId                  = event.mItemId;
    mCollectionId            = event.mCollectionId;
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
    mRepeatSoundPause        = event.mRepeatSoundPause;
    mLateCancel              = event.mLateCancel;
    mExcludeHolidays         = event.mExcludeHolidays;
    mWorkTimeOnly            = event.mWorkTimeOnly;
    mActionSubType           = event.mActionSubType;
    mCategory                = event.mCategory;
    mExtraActionOptions      = event.mExtraActionOptions;
    mCompatibility           = event.mCompatibility;
    mReadOnly                = event.mReadOnly;
    mConfirmAck              = event.mConfirmAck;
    mUseDefaultFont          = event.mUseDefaultFont;
    mCommandScript           = event.mCommandScript;
    mCommandXterm            = event.mCommandXterm;
    mCommandDisplay          = event.mCommandDisplay;
    mEmailBcc                = event.mEmailBcc;
    mBeep                    = event.mBeep;
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
    if (event.mRecurrence) {
        mRecurrence = new KARecurrence(*event.mRecurrence);
    } else {
        mRecurrence = 0;
    }
}

void KAEvent::set(const Event::Ptr &e)
{
    d->set(e);
}

/******************************************************************************
* Initialise the KAEventPrivate from a KCal::Event.
*/
void KAEventPrivate::set(const Event::Ptr &event)
{
    startChanges();
    // Extract status from the event
    mCommandError           = KAEvent::CMD_NO_ERROR;
    mEventID                = event->uid();
    mRevision               = event->revision();
    mTemplateName.clear();
    mLogFile.clear();
    mItemId                 = -1;
    mCollectionId           = -1;
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
    mCompatibility          = KACalendar::Current;
    mReadOnly               = event->isReadOnly();
    mUseDefaultFont         = true;
    mEnabled                = true;
    clearRecur();
    QString param;
    bool ok;
    mCategory               = CalEvent::status(event, &param);
    if (mCategory == CalEvent::DISPLAYING) {
        // It's a displaying calendar event - set values specific to displaying alarms
        const QStringList params = param.split(SC, QString::KeepEmptyParts);
        int n = params.count();
        if (n) {
            const qlonglong id = params[0].toLongLong(&ok);
            if (ok) {
                mCollectionId = id;    // original collection ID which contained the event
            }
            for (int i = 1;  i < n;  ++i) {
                if (params[i] == DISP_DEFER) {
                    mDisplayingDefer = true;
                }
                if (params[i] == DISP_EDIT) {
                    mDisplayingEdit = true;
                }
            }
        }
    }
    // Store the non-KAlarm custom properties of the event
    const QByteArray kalarmKey = "X-KDE-" + KACalendar::APPNAME + '-';
    mCustomProperties = event->customProperties();
    for (QMap<QByteArray, QString>::Iterator it = mCustomProperties.begin();  it != mCustomProperties.end();) {
        if (it.key().startsWith(kalarmKey)) {
            it = mCustomProperties.erase(it);
        } else {
            ++it;
        }
    }

    bool dateOnly = false;
    QStringList flags = event->customProperty(KACalendar::APPNAME, FLAGS_PROPERTY).split(SC, QString::SkipEmptyParts);
    flags << QString() << QString();    // to avoid having to check for end of list
    for (int i = 0, end = flags.count() - 1;  i < end;  ++i) {
        QString flag = flags.at(i);
        if (flag == DATE_ONLY_FLAG) {
            dateOnly = true;
        } else if (flag == CONFIRM_ACK_FLAG) {
            mConfirmAck = true;
        } else if (flag == EMAIL_BCC_FLAG) {
            mEmailBcc = true;
        } else if (flag == KORGANIZER_FLAG) {
            mCopyToKOrganizer = true;
        } else if (flag == EXCLUDE_HOLIDAYS_FLAG) {
            mExcludeHolidays = mHolidays;
        } else if (flag == WORK_TIME_ONLY_FLAG) {
            mWorkTimeOnly = 1;
        } else if (flag == KMAIL_SERNUM_FLAG) {
            const unsigned long n = flags.at(i + 1).toULong(&ok);
            if (!ok) {
                continue;
            }
            mKMailSerialNumber = n;
            ++i;
        } else if (flag == KAEventPrivate::ARCHIVE_FLAG) {
            mArchive = true;
        } else if (flag == KAEventPrivate::AT_LOGIN_TYPE) {
            mArchiveRepeatAtLogin = true;
        } else if (flag == KAEventPrivate::REMINDER_TYPE) {
            flag = flags.at(++i);
            if (flag == KAEventPrivate::REMINDER_ONCE_FLAG) {
                mReminderOnceOnly = true;
                ++i;
            }
            const int len = flag.length() - 1;
            mReminderMinutes = -flag.left(len).toInt();    // -> 0 if conversion fails
            switch (flag.at(len).toLatin1()) {
                case 'M':  break;
                case 'H':  mReminderMinutes *= 60;  break;
                case 'D':  mReminderMinutes *= 1440;  break;
                default:   mReminderMinutes = 0;  break;
            }
        } else if (flag == DEFER_FLAG) {
            QString mins = flags.at(i + 1);
            if (mins.endsWith(QLatin1Char('D'))) {
                mDeferDefaultDateOnly = true;
                mins.truncate(mins.length() - 1);
            }
            const int n = static_cast<int>(mins.toUInt(&ok));
            if (!ok) {
                continue;
            }
            mDeferDefaultMinutes = n;
            ++i;
        } else if (flag == TEMPL_AFTER_TIME_FLAG) {
            const int n = static_cast<int>(flags.at(i + 1).toUInt(&ok));
            if (!ok) {
                continue;
            }
            mTemplateAfterTime = n;
            ++i;
        } else if (flag == LATE_CANCEL_FLAG) {
            mLateCancel = static_cast<int>(flags.at(i + 1).toUInt(&ok));
            if (ok) {
                ++i;
            }
            if (!ok  ||  !mLateCancel) {
                mLateCancel = 1;    // invalid parameter defaults to 1 minute
            }
        } else if (flag == AUTO_CLOSE_FLAG) {
            mLateCancel = static_cast<int>(flags.at(i + 1).toUInt(&ok));
            if (ok) {
                ++i;
            }
            if (!ok  ||  !mLateCancel) {
                mLateCancel = 1;    // invalid parameter defaults to 1 minute
            }
            mAutoClose = true;
        }
    }

    QString prop = event->customProperty(KACalendar::APPNAME, LOG_PROPERTY);
    if (!prop.isEmpty()) {
        if (prop == xtermURL) {
            mCommandXterm = true;
        } else if (prop == displayURL) {
            mCommandDisplay = true;
        } else {
            mLogFile = prop;
        }
    }
    prop = event->customProperty(KACalendar::APPNAME, REPEAT_PROPERTY);
    if (!prop.isEmpty()) {
        // This property is used when the main alarm has expired
        const QStringList list = prop.split(QLatin1Char(':'));
        if (list.count() >= 2) {
            const int interval = static_cast<int>(list[0].toUInt());
            const int count = static_cast<int>(list[1].toUInt());
            if (interval && count) {
                if (interval % (24 * 60)) {
                    mRepetition.set(Duration(interval * 60, Duration::Seconds), count);
                } else {
                    mRepetition.set(Duration(interval / (24 * 60), Duration::Days), count);
                }
            }
        }
    }
    mNextMainDateTime = readDateTime(event, dateOnly, mStartDateTime);
    mCreatedDateTime = event->created();
    if (dateOnly  &&  !mRepetition.isDaily()) {
        mRepetition.set(Duration(mRepetition.intervalDays(), Duration::Days));
    }
    if (mCategory == CalEvent::TEMPLATE) {
        mTemplateName = event->summary();
    }
    if (event->customStatus() == DISABLED_STATUS) {
        mEnabled = false;
    }

    // Extract status from the event's alarms.
    // First set up defaults.
    mActionSubType      = KAEvent::MESSAGE;
    mMainExpired        = true;
    mRepeatAtLogin      = false;
    mDisplaying         = false;
    mCommandScript      = false;
    mExtraActionOptions = 0;
    mDeferral           = NO_DEFERRAL;
    mSoundVolume        = -1;
    mFadeVolume         = -1;
    mRepeatSoundPause   = -1;
    mFadeSeconds        = 0;
    mEmailFromIdentity  = 0;
    mReminderAfterTime  = DateTime();
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
    for (AlarmMap::ConstIterator it = alarmMap.constBegin();  it != alarmMap.constEnd();  ++it) {
        const AlarmData &data = it.value();
        const DateTime dateTime = data.alarm->hasStartOffset() ? data.alarm->startOffset().end(mNextMainDateTime.effectiveKDateTime()) : data.alarm->time();
        switch (data.type) {
            case MAIN_ALARM:
                mMainExpired = false;
                alTime = dateTime;
                alTime.setDateOnly(mStartDateTime.isDateOnly());
                if (data.alarm->repeatCount()  &&  data.alarm->snoozeTime()) {
                    mRepetition.set(data.alarm->snoozeTime(), data.alarm->repeatCount());   // values may be adjusted in setRecurrence()
                    mNextRepeat = data.nextRepeat;
                }
                if (data.action != KAAlarm::AUDIO) {
                    break;
                }
            // Fall through to AUDIO_ALARM
            case AUDIO_ALARM:
                mAudioFile   = data.cleanText;
                mSpeak       = data.speak  &&  mAudioFile.isEmpty();
                mBeep        = !mSpeak  &&  mAudioFile.isEmpty();
                mSoundVolume = (!mBeep && !mSpeak) ? data.soundVolume : -1;
                mFadeVolume  = (mSoundVolume >= 0  &&  data.fadeSeconds > 0) ? data.fadeVolume : -1;
                mFadeSeconds = (mFadeVolume >= 0) ? data.fadeSeconds : 0;
                mRepeatSoundPause = (!mBeep && !mSpeak) ? data.repeatSoundPause : -1;
                break;
            case AT_LOGIN_ALARM:
                mRepeatAtLogin   = true;
                mAtLoginDateTime = dateTime.kDateTime();
                alTime = mAtLoginDateTime;
                break;
            case REMINDER_ALARM:
                // N.B. there can be a start offset but no valid date/time (e.g. in template)
                if (data.alarm->startOffset().asSeconds() / 60) {
                    mReminderActive = ACTIVE_REMINDER;
                    if (mReminderMinutes < 0) {
                        mReminderAfterTime = dateTime;   // the reminder is AFTER the main alarm
                        mReminderAfterTime.setDateOnly(dateOnly);
                        if (data.hiddenReminder) {
                            mReminderActive = HIDDEN_REMINDER;
                        }
                    }
                }
                break;
            case DEFERRED_REMINDER_ALARM:
            case DEFERRED_ALARM:
                mDeferral = (data.type == DEFERRED_REMINDER_ALARM) ? REMINDER_DEFERRAL : NORMAL_DEFERRAL;
                mDeferralTime = dateTime;
                if (!data.timedDeferral) {
                    mDeferralTime.setDateOnly(true);
                }
                if (data.alarm->hasStartOffset()) {
                    deferralOffset = data.alarm->startOffset();
                }
                break;
            case DISPLAYING_ALARM: {
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
                mPreAction          = data.cleanText;
                mExtraActionOptions = data.extraActionOptions;
                break;
            case POST_ACTION_ALARM:
                mPostAction = data.cleanText;
                break;
            case INVALID_ALARM:
            default:
                break;
        }

        bool noSetNextTime = false;
        switch (data.type) {
            case DEFERRED_REMINDER_ALARM:
            case DEFERRED_ALARM:
                if (!set) {
                    // The recurrence has to be evaluated before we can
                    // calculate the time of a deferral alarm.
                    setDeferralTime = true;
                    noSetNextTime = true;
                }
            // fall through to REMINDER_ALARM
            case REMINDER_ALARM:
            case AT_LOGIN_ALARM:
            case DISPLAYING_ALARM:
                if (!set  &&  !noSetNextTime) {
                    mNextMainDateTime = alTime;
                }
            // fall through to MAIN_ALARM
            case MAIN_ALARM:
                // Ensure that the basic fields are set up even if there is no main
                // alarm in the event (if it has expired and then been deferred)
                if (!set) {
                    mActionSubType = (KAEvent::SubAction)data.action;
                    mText = (mActionSubType == KAEvent::COMMAND) ? data.cleanText.trimmed() : data.cleanText;
                    switch (data.action) {
                        case KAAlarm::COMMAND:
                            mCommandScript = data.commandScript;
                            if (!mCommandDisplay) {
                                break;
                            }
                        // fall through to MESSAGE
                        case KAAlarm::MESSAGE:
                            mFont           = data.font;
                            mUseDefaultFont = data.defaultFont;
                            if (data.isEmailText) {
                                isEmailText = true;
                            }
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
                            mRepeatSoundPause = data.repeatSoundPause;
                            break;
                        default:
                            break;
                    }
                    set = true;
                }
                if (data.action == KAAlarm::FILE  &&  mActionSubType == KAEvent::MESSAGE) {
                    mActionSubType = KAEvent::FILE;
                }
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
    if (!isEmailText) {
        mKMailSerialNumber = 0;
    }

    Recurrence *recur = event->recurrence();
    if (recur  &&  recur->recurs()) {
        const int nextRepeat = mNextRepeat;    // setRecurrence() clears mNextRepeat
        setRecurrence(*recur);
        if (nextRepeat <= mRepetition.count()) {
            mNextRepeat = nextRepeat;
        }
    } else if (mRepetition) {
        // Convert a repetition with no recurrence into a recurrence
        if (mRepetition.isDaily()) {
            recur->setDaily(mRepetition.intervalDays());
        } else {
            recur->setMinutely(mRepetition.intervalMinutes());
        }
        recur->setDuration(mRepetition.count() + 1);
        mRepetition.set(0, 0);
    }

    if (mRepeatAtLogin) {
        mArchiveRepeatAtLogin = false;
        if (mReminderMinutes > 0) {
            mReminderMinutes = 0;      // pre-alarm reminder not allowed for at-login alarm
            mReminderActive  = NO_REMINDER;
        }
        setRepeatAtLoginTrue(false);   // clear other incompatible statuses
    }

    if (mMainExpired  &&  deferralOffset  &&  checkRecur() != KARecurrence::NO_RECUR) {
        // Adjust the deferral time for an expired recurrence, since the
        // offset is relative to the first actual occurrence.
        DateTime dt = mRecurrence->getNextDateTime(mStartDateTime.addDays(-1).kDateTime());
        dt.setDateOnly(mStartDateTime.isDateOnly());
        if (mDeferralTime.isDateOnly()) {
            mDeferralTime = deferralOffset.end(dt.kDateTime());
            mDeferralTime.setDateOnly(true);
        } else {
            mDeferralTime = deferralOffset.end(dt.effectiveKDateTime());
        }
    }
    if (mDeferral != NO_DEFERRAL) {
        if (setDeferralTime) {
            mNextMainDateTime = mDeferralTime;
        }
    }
    mTriggerChanged = true;
    endChanges();
}

void KAEvent::set(const KDateTime &dt, const QString &message, const QColor &bg, const QColor &fg,
                  const QFont &f, SubAction act, int lateCancel, Flags flags, bool changesPending)
{
    d->set(dt, message, bg, fg, f, act, lateCancel, flags, changesPending);
}

/******************************************************************************
* Initialise the instance with the specified parameters.
*/
void KAEventPrivate::set(const KDateTime &dateTime, const QString &text, const QColor &bg, const QColor &fg,
                         const QFont &font, KAEvent::SubAction action, int lateCancel, KAEvent::Flags flags,
                         bool changesPending)
{
    clearRecur();
    mStartDateTime = dateTime;
    mStartDateTime.setDateOnly(flags & KAEvent::ANY_TIME);
    mNextMainDateTime = mStartDateTime;
    switch (action) {
        case KAEvent::MESSAGE:
        case KAEvent::FILE:
        case KAEvent::COMMAND:
        case KAEvent::EMAIL:
        case KAEvent::AUDIO:
            mActionSubType = (KAEvent::SubAction)action;
            break;
        default:
            mActionSubType = KAEvent::MESSAGE;
            break;
    }
    mEventID.clear();
    mTemplateName.clear();
    mItemId                 = -1;
    mCollectionId           = -1;
    mPreAction.clear();
    mPostAction.clear();
    mText                   = (mActionSubType == KAEvent::COMMAND) ? text.trimmed()
                            : (mActionSubType == KAEvent::AUDIO)   ? QString() : text;
    mCategory               = CalEvent::ACTIVE;
    mAudioFile              = (mActionSubType == KAEvent::AUDIO) ? text : QString();
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

    mStartDateTime.setDateOnly(flags & KAEvent::ANY_TIME);
    set_deferral((flags & DEFERRAL) ? NORMAL_DEFERRAL : NO_DEFERRAL);
    mRepeatAtLogin          = flags & KAEvent::REPEAT_AT_LOGIN;
    mConfirmAck             = flags & KAEvent::CONFIRM_ACK;
    mUseDefaultFont         = flags & KAEvent::DEFAULT_FONT;
    mCommandScript          = flags & KAEvent::SCRIPT;
    mCommandXterm           = flags & KAEvent::EXEC_IN_XTERM;
    mCommandDisplay         = flags & KAEvent::DISPLAY_COMMAND;
    mCopyToKOrganizer       = flags & KAEvent::COPY_KORGANIZER;
    mExcludeHolidays        = (flags & KAEvent::EXCL_HOLIDAYS) ? mHolidays : 0;
    mWorkTimeOnly           = flags & KAEvent::WORK_TIME_ONLY;
    mEmailBcc               = flags & KAEvent::EMAIL_BCC;
    mEnabled                = !(flags & KAEvent::DISABLED);
    mDisplaying             = flags & DISPLAYING_;
    mReminderOnceOnly       = flags & KAEvent::REMINDER_ONCE;
    mAutoClose              = (flags & KAEvent::AUTO_CLOSE) && mLateCancel;
    mRepeatSoundPause       = (flags & KAEvent::REPEAT_SOUND) ? 0 : -1;
    mSpeak                  = (flags & KAEvent::SPEAK) && action != KAEvent::AUDIO;
    mBeep                   = (flags & KAEvent::BEEP) && action != KAEvent::AUDIO && !mSpeak;
    if (mRepeatAtLogin) {                     // do this after setting other flags
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
    mReminderAfterTime      = DateTime();
    mExtraActionOptions     = 0;
    mCompatibility          = KACalendar::Current;
    mReadOnly               = false;
    mCommandError           = KAEvent::CMD_NO_ERROR;
    mChangeCount            = changesPending ? 1 : 0;
    mTriggerChanged         = true;
}

/******************************************************************************
* Update an existing KCal::Event with the KAEventPrivate data.
* If 'setCustomProperties' is true, all the KCal::Event's existing custom
* properties are cleared and replaced with the KAEvent's custom properties. If
* false, the KCal::Event's non-KAlarm custom properties are left untouched.
*/
bool KAEvent::updateKCalEvent(const KCalCore::Event::Ptr &e, UidAction u, bool setCustomProperties) const
{
    return d->updateKCalEvent(e, u, setCustomProperties);
}

bool KAEventPrivate::updateKCalEvent(const Event::Ptr &ev, KAEvent::UidAction uidact, bool setCustomProperties) const
{
    // If it's an archived event, the event start date/time will be adjusted to its original
    // value instead of its next occurrence, and the expired main alarm will be reinstated.
    const bool archived = (mCategory == CalEvent::ARCHIVED);

    if (!ev
    ||  (uidact == KAEvent::UID_CHECK  &&  !mEventID.isEmpty()  &&  mEventID != ev->uid())
    ||  (!mAlarmCount  && (!archived || !mMainExpired))) {
        return false;
    }

    ev->startUpdates();   // prevent multiple update notifications
    checkRecur();         // ensure recurrence/repetition data is consistent
    const bool readOnly = ev->isReadOnly();
    if (uidact == KAEvent::UID_SET) {
        ev->setUid(mEventID);
    }
    ev->setReadOnly(mReadOnly);
    ev->setTransparency(Event::Transparent);

    // Set up event-specific data

    // Set up custom properties.
    if (setCustomProperties) {
        ev->setCustomProperties(mCustomProperties);
    }
    ev->removeCustomProperty(KACalendar::APPNAME, FLAGS_PROPERTY);
    ev->removeCustomProperty(KACalendar::APPNAME, NEXT_RECUR_PROPERTY);
    ev->removeCustomProperty(KACalendar::APPNAME, REPEAT_PROPERTY);
    ev->removeCustomProperty(KACalendar::APPNAME, LOG_PROPERTY);

    QString param;
    if (mCategory == CalEvent::DISPLAYING) {
        param = QString::number(mCollectionId);   // original collection ID which contained the event
        if (mDisplayingDefer) {
            param += SC + DISP_DEFER;
        }
        if (mDisplayingEdit) {
            param += SC + DISP_EDIT;
        }
    }
    CalEvent::setStatus(ev, mCategory, param);
    QStringList flags;
    if (mStartDateTime.isDateOnly()) {
        flags += DATE_ONLY_FLAG;
    }
    if (mConfirmAck) {
        flags += CONFIRM_ACK_FLAG;
    }
    if (mEmailBcc) {
        flags += EMAIL_BCC_FLAG;
    }
    if (mCopyToKOrganizer) {
        flags += KORGANIZER_FLAG;
    }
    if (mExcludeHolidays) {
        flags += EXCLUDE_HOLIDAYS_FLAG;
    }
    if (mWorkTimeOnly) {
        flags += WORK_TIME_ONLY_FLAG;
    }
    if (mLateCancel) {
        (flags += (mAutoClose ? AUTO_CLOSE_FLAG : LATE_CANCEL_FLAG)) += QString::number(mLateCancel);
    }
    if (mReminderMinutes) {
        flags += REMINDER_TYPE;
        if (mReminderOnceOnly) {
            flags += REMINDER_ONCE_FLAG;
        }
        flags += reminderToString(-mReminderMinutes);
    }
    if (mDeferDefaultMinutes) {
        QString param = QString::number(mDeferDefaultMinutes);
        if (mDeferDefaultDateOnly) {
            param += QLatin1Char('D');
        }
        (flags += DEFER_FLAG) += param;
    }
    if (!mTemplateName.isEmpty()  &&  mTemplateAfterTime >= 0) {
        (flags += TEMPL_AFTER_TIME_FLAG) += QString::number(mTemplateAfterTime);
    }
    if (mKMailSerialNumber) {
        (flags += KMAIL_SERNUM_FLAG) += QString::number(mKMailSerialNumber);
    }
    if (mArchive  &&  !archived) {
        flags += ARCHIVE_FLAG;
        if (mArchiveRepeatAtLogin) {
            flags += AT_LOGIN_TYPE;
        }
    }
    if (!flags.isEmpty()) {
        ev->setCustomProperty(KACalendar::APPNAME, FLAGS_PROPERTY, flags.join(SC));
    }

    if (mCommandXterm) {
        ev->setCustomProperty(KACalendar::APPNAME, LOG_PROPERTY, xtermURL);
    } else if (mCommandDisplay) {
        ev->setCustomProperty(KACalendar::APPNAME, LOG_PROPERTY, displayURL);
    } else if (!mLogFile.isEmpty()) {
        ev->setCustomProperty(KACalendar::APPNAME, LOG_PROPERTY, mLogFile);
    }

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
    ev->setDtEnd(KDateTime());

    const DateTime dtMain = archived ? mStartDateTime : mNextMainDateTime;
    int      ancillaryType = 0;   // 0 = invalid, 1 = time, 2 = offset
    DateTime ancillaryTime;       // time for ancillary alarms (pre-action, extra audio, etc)
    int      ancillaryOffset = 0; // start offset for ancillary alarms
    if (!mMainExpired  ||  archived) {
        /* The alarm offset must always be zero for the main alarm. To determine
         * which recurrence is due, the property X-KDE-KALARM_NEXTRECUR is used.
         * If the alarm offset was non-zero, exception dates and rules would not
         * work since they apply to the event time, not the alarm time.
         */
        if (!archived  &&  checkRecur() != KARecurrence::NO_RECUR) {
            QDateTime dt = mNextMainDateTime.kDateTime().toTimeSpec(mStartDateTime.timeSpec()).dateTime();
            ev->setCustomProperty(KACalendar::APPNAME, NEXT_RECUR_PROPERTY,
                                  dt.toString(mNextMainDateTime.isDateOnly() ? QLatin1String("yyyyMMdd") : QLatin1String("yyyyMMddThhmmss")));
        }
        // Add the main alarm
        initKCalAlarm(ev, 0, QStringList(), MAIN_ALARM);
        ancillaryOffset = 0;
        ancillaryType = dtMain.isValid() ? 2 : 0;
    } else if (mRepetition) {
        // Alarm repetition is normally held in the main alarm, but since
        // the main alarm has expired, store in a custom property.
        const QString param = QString::fromLatin1("%1:%2").arg(mRepetition.intervalMinutes()).arg(mRepetition.count());
        ev->setCustomProperty(KACalendar::APPNAME, REPEAT_PROPERTY, param);
    }

    // Add subsidiary alarms
    if (mRepeatAtLogin  || (mArchiveRepeatAtLogin && archived)) {
        DateTime dtl;
        if (mArchiveRepeatAtLogin) {
            dtl = mStartDateTime.calendarKDateTime().addDays(-1);
        } else if (mAtLoginDateTime.isValid()) {
            dtl = mAtLoginDateTime;
        } else if (mStartDateTime.isDateOnly()) {
            dtl = DateTime(KDateTime::currentLocalDate().addDays(-1), mStartDateTime.timeSpec());
        } else {
            dtl = KDateTime::currentUtcDateTime();
        }
        initKCalAlarm(ev, dtl, QStringList(AT_LOGIN_TYPE));
        if (!ancillaryType  &&  dtl.isValid()) {
            ancillaryTime = dtl;
            ancillaryType = 1;
        }
    }

    // Find the base date/time for calculating alarm offsets
    DateTime nextDateTime = mNextMainDateTime;
    if (mMainExpired) {
        if (checkRecur() == KARecurrence::NO_RECUR) {
            nextDateTime = mStartDateTime;
        } else if (!archived) {
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

    if (mReminderMinutes  && (mReminderActive != NO_REMINDER || archived)) {
        int startOffset;
        if (mReminderMinutes < 0  &&  mReminderActive != NO_REMINDER) {
            // A reminder AFTER the main alarm is active or disabled
            startOffset = nextDateTime.calendarKDateTime().secsTo(mReminderAfterTime.calendarKDateTime());
        } else {
            // A reminder BEFORE the main alarm is active
            startOffset = -mReminderMinutes * 60;
        }
        initKCalAlarm(ev, startOffset, QStringList(REMINDER_TYPE));
        // Don't set ancillary time if the reminder AFTER is hidden by a deferral
        if (!ancillaryType  && (mReminderActive == ACTIVE_REMINDER || archived)) {
            ancillaryOffset = startOffset;
            ancillaryType = 2;
        }
    }
    if (mDeferral != NO_DEFERRAL) {
        int startOffset;
        QStringList list;
        if (mDeferralTime.isDateOnly()) {
            startOffset = nextDateTime.secsTo(mDeferralTime.calendarKDateTime());
            list += DATE_DEFERRAL_TYPE;
        } else {
            startOffset = nextDateTime.calendarKDateTime().secsTo(mDeferralTime.calendarKDateTime());
            list += TIME_DEFERRAL_TYPE;
        }
        if (mDeferral == REMINDER_DEFERRAL) {
            list += REMINDER_TYPE;
        }
        initKCalAlarm(ev, startOffset, list);
        if (!ancillaryType  &&  mDeferralTime.isValid()) {
            ancillaryOffset = startOffset;
            ancillaryType = 2;
        }
    }
    if (!mTemplateName.isEmpty()) {
        ev->setSummary(mTemplateName);
    } else if (mDisplaying) {
        QStringList list(DISPLAYING_TYPE);
        if (mDisplayingFlags & KAEvent::REPEAT_AT_LOGIN) {
            list += AT_LOGIN_TYPE;
        } else if (mDisplayingFlags & DEFERRAL) {
            if (mDisplayingFlags & TIMED_FLAG) {
                list += TIME_DEFERRAL_TYPE;
            } else {
                list += DATE_DEFERRAL_TYPE;
            }
        }
        if (mDisplayingFlags & REMINDER) {
            list += REMINDER_TYPE;
        }
        initKCalAlarm(ev, mDisplayingTime, list);
        if (!ancillaryType  &&  mDisplayingTime.isValid()) {
            ancillaryTime = mDisplayingTime;
            ancillaryType = 1;
        }
    }
    if ((mBeep  ||  mSpeak  ||  !mAudioFile.isEmpty())  &&  mActionSubType != KAEvent::AUDIO) {
        // A sound is specified
        if (ancillaryType == 2) {
            initKCalAlarm(ev, ancillaryOffset, QStringList(), AUDIO_ALARM);
        } else {
            initKCalAlarm(ev, ancillaryTime, QStringList(), AUDIO_ALARM);
        }
    }
    if (!mPreAction.isEmpty()) {
        // A pre-display action is specified
        if (ancillaryType == 2) {
            initKCalAlarm(ev, ancillaryOffset, QStringList(PRE_ACTION_TYPE), PRE_ACTION_ALARM);
        } else {
            initKCalAlarm(ev, ancillaryTime, QStringList(PRE_ACTION_TYPE), PRE_ACTION_ALARM);
        }
    }
    if (!mPostAction.isEmpty()) {
        // A post-display action is specified
        if (ancillaryType == 2) {
            initKCalAlarm(ev, ancillaryOffset, QStringList(POST_ACTION_TYPE), POST_ACTION_ALARM);
        } else {
            initKCalAlarm(ev, ancillaryTime, QStringList(POST_ACTION_TYPE), POST_ACTION_ALARM);
        }
    }

    if (mRecurrence) {
        mRecurrence->writeRecurrence(*ev->recurrence());
    } else {
        ev->clearRecurrence();
    }
    if (mCreatedDateTime.isValid()) {
        ev->setCreated(mCreatedDateTime);
    }
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
Alarm::Ptr KAEventPrivate::initKCalAlarm(const Event::Ptr &event, const DateTime &dt, const QStringList &types, AlarmType type) const
{
    const int startOffset = dt.isDateOnly() ? mStartDateTime.secsTo(dt)
                                            : mStartDateTime.calendarKDateTime().secsTo(dt.calendarKDateTime());
    return initKCalAlarm(event, startOffset, types, type);
}

Alarm::Ptr KAEventPrivate::initKCalAlarm(const Event::Ptr &event, int startOffsetSecs, const QStringList &types, AlarmType type) const
{
    QStringList alltypes;
    QStringList flags;
    Alarm::Ptr alarm = event->newAlarm();
    alarm->setEnabled(true);
    if (type != MAIN_ALARM) {
        // RFC2445 specifies that absolute alarm times must be stored as a UTC DATE-TIME value.
        // Set the alarm time as an offset to DTSTART for the reasons described in updateKCalEvent().
        alarm->setStartOffset(startOffsetSecs);
    }

    switch (type) {
        case AUDIO_ALARM:
            setAudioAlarm(alarm);
            if (mSpeak) {
                flags << KAEventPrivate::SPEAK_FLAG;
            }
            if (mRepeatSoundPause >= 0) {
                // Alarm::setSnoozeTime() sets 5 seconds if duration parameter is zero,
                // so repeat count = -1 represents 0 pause, -2 represents non-zero pause.
                alarm->setRepeatCount(mRepeatSoundPause ? -2 : -1);
                alarm->setSnoozeTime(Duration(mRepeatSoundPause, Duration::Seconds));
            }
            break;
        case PRE_ACTION_ALARM:
            setProcedureAlarm(alarm, mPreAction);
            if (mExtraActionOptions & KAEvent::ExecPreActOnDeferral) {
                flags << KAEventPrivate::EXEC_ON_DEFERRAL_FLAG;
            }
            if (mExtraActionOptions & KAEvent::CancelOnPreActError) {
                flags << KAEventPrivate::CANCEL_ON_ERROR_FLAG;
            }
            if (mExtraActionOptions & KAEvent::DontShowPreActError) {
                flags << KAEventPrivate::DONT_SHOW_ERROR_FLAG;
            }
            break;
        case POST_ACTION_ALARM:
            setProcedureAlarm(alarm, mPostAction);
            break;
        case MAIN_ALARM:
            alarm->setSnoozeTime(mRepetition.interval());
            alarm->setRepeatCount(mRepetition.count());
            if (mRepetition)
                alarm->setCustomProperty(KACalendar::APPNAME, NEXT_REPEAT_PROPERTY,
                                         QString::number(mNextRepeat));
        // fall through to INVALID_ALARM
        case REMINDER_ALARM:
        case INVALID_ALARM: {
            if (types == QStringList(REMINDER_TYPE)
            &&  mReminderMinutes < 0  &&  mReminderActive == HIDDEN_REMINDER) {
                // It's a reminder AFTER the alarm which is currently disabled
                // due to the main alarm being deferred past it.
                flags << HIDDEN_REMINDER_FLAG;
            }
            bool display = false;
            switch (mActionSubType) {
                case KAEvent::FILE:
                    alltypes += FILE_TYPE;
                // fall through to MESSAGE
                case KAEvent::MESSAGE:
                    alarm->setDisplayAlarm(AlarmText::toCalendarText(mText));
                    display = true;
                    break;
                case KAEvent::COMMAND:
                    if (mCommandScript) {
                        alarm->setProcedureAlarm(QString(), mText);
                    } else {
                        setProcedureAlarm(alarm, mText);
                    }
                    display = mCommandDisplay;
                    break;
                case KAEvent::EMAIL:
                    alarm->setEmailAlarm(mEmailSubject, mText, mEmailAddresses, mEmailAttachments);
                    if (mEmailFromIdentity) {
                        flags << KAEventPrivate::EMAIL_ID_FLAG << QString::number(mEmailFromIdentity);
                    }
                    break;
                case KAEvent::AUDIO:
                    setAudioAlarm(alarm);
                    if (mRepeatSoundPause >= 0) {
                        alltypes += SOUND_REPEAT_TYPE;
                        if (type == MAIN_ALARM) {
                            alltypes += QString::number(mRepeatSoundPause);
                        }
                    }
                    break;
            }
            if (display)
                alarm->setCustomProperty(KACalendar::APPNAME, FONT_COLOUR_PROPERTY,
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
    if (!alltypes.isEmpty()) {
        alarm->setCustomProperty(KACalendar::APPNAME, TYPE_PROPERTY, alltypes.join(QLatin1String(",")));
    }
    if (!flags.isEmpty()) {
        alarm->setCustomProperty(KACalendar::APPNAME, FLAGS_PROPERTY, flags.join(SC));
    }
    return alarm;
}

bool KAEvent::isValid() const
{
    return d->mAlarmCount  && (d->mAlarmCount != 1 || !d->mRepeatAtLogin);
}

void KAEvent::setEnabled(bool enable)
{
    d->mEnabled = enable;
}

bool KAEvent::enabled() const
{
    return d->mEnabled;
}

void KAEvent::setReadOnly(bool ro)
{
    d->mReadOnly = ro;
}

bool KAEvent::isReadOnly() const
{
    return d->mReadOnly;
}

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
    return (d->mDisplaying && d->mMainExpired)  ||  d->mCategory == CalEvent::ARCHIVED;
}

KAEvent::Flags KAEvent::flags() const
{
    return d->flags();
}

KAEvent::Flags KAEventPrivate::flags() const
{
    KAEvent::Flags result(0);
    if (mBeep) {
        result |= KAEvent::BEEP;
    }
    if (mRepeatSoundPause >= 0) {
        result |= KAEvent::REPEAT_SOUND;
    }
    if (mEmailBcc) {
        result |= KAEvent::EMAIL_BCC;
    }
    if (mStartDateTime.isDateOnly()) {
        result |= KAEvent::ANY_TIME;
    }
    if (mSpeak) {
        result |= KAEvent::SPEAK;
    }
    if (mRepeatAtLogin) {
        result |= KAEvent::REPEAT_AT_LOGIN;
    }
    if (mConfirmAck) {
        result |= KAEvent::CONFIRM_ACK;
    }
    if (mUseDefaultFont) {
        result |= KAEvent::DEFAULT_FONT;
    }
    if (mCommandScript) {
        result |= KAEvent::SCRIPT;
    }
    if (mCommandXterm) {
        result |= KAEvent::EXEC_IN_XTERM;
    }
    if (mCommandDisplay) {
        result |= KAEvent::DISPLAY_COMMAND;
    }
    if (mCopyToKOrganizer) {
        result |= KAEvent::COPY_KORGANIZER;
    }
    if (mExcludeHolidays) {
        result |= KAEvent::EXCL_HOLIDAYS;
    }
    if (mWorkTimeOnly) {
        result |= KAEvent::WORK_TIME_ONLY;
    }
    if (mReminderOnceOnly) {
        result |= KAEvent::REMINDER_ONCE;
    }
    if (mAutoClose) {
        result |= KAEvent::AUTO_CLOSE;
    }
    if (!mEnabled) {
        result |= KAEvent::DISABLED;
    }
    return result;
}

/******************************************************************************
* Change the type of an event.
* If it is being set to archived, set the archived indication in the event ID;
* otherwise, remove the archived indication from the event ID.
*/
void KAEvent::setCategory(CalEvent::Type s)
{
    d->setCategory(s);
}

void KAEventPrivate::setCategory(CalEvent::Type s)
{
    if (s == mCategory) {
        return;
    }
    mEventID = CalEvent::uid(mEventID, s);
    mCategory = s;
    mTriggerChanged = true;   // templates and archived don't have trigger times
}

CalEvent::Type KAEvent::category() const
{
    return d->mCategory;
}

void KAEvent::setEventId(const QString &id)
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

void KAEvent::setCollectionId(Akonadi::Collection::Id id)
{
    d->mCollectionId = id;
}

void KAEvent::setCollectionId_const(Akonadi::Collection::Id id) const
{
    d->mCollectionId = id;
}

Akonadi::Collection::Id KAEvent::collectionId() const
{
    // A displaying alarm contains the event's original collection ID
    return d->mDisplaying ? -1 : d->mCollectionId;
}

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
bool KAEvent::setItemPayload(Akonadi::Item &item, const QStringList &collectionMimeTypes) const
{
    QString mimetype;
    switch (d->mCategory) {
        case CalEvent::ACTIVE:      mimetype = MIME_ACTIVE;    break;
        case CalEvent::ARCHIVED:    mimetype = MIME_ARCHIVED;  break;
        case CalEvent::TEMPLATE:    mimetype = MIME_TEMPLATE;  break;
        default:                            Q_ASSERT(0);  return false;
    }
    if (!collectionMimeTypes.contains(mimetype)) {
        return false;
    }
    item.setMimeType(mimetype);
    item.setPayload<KAEvent>(*this);
    return true;
}

void KAEvent::setCompatibility(KACalendar::Compat c)
{
    d->mCompatibility = c;
}

KACalendar::Compat KAEvent::compatibility() const
{
    return d->mCompatibility;
}

QMap<QByteArray, QString> KAEvent::customProperties() const
{
    return d->mCustomProperties;
}

KAEvent::SubAction KAEvent::actionSubType() const
{
    return d->mActionSubType;
}

KAEvent::Actions KAEvent::actionTypes() const
{
    switch (d->mActionSubType) {
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
    if (d->mRepeatAtLogin) {
        minutes = 0;
    }
    d->mLateCancel = minutes;
    if (!minutes) {
        d->mAutoClose = false;
    }
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
        ||  d->mActionSubType == EMAIL) ? d->mText : QString();
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

void KAEvent::setDefaultFont(const QFont &f)
{
    KAEventPrivate::mDefaultFont = f;
}

bool KAEvent::useDefaultFont() const
{
    return d->mUseDefaultFont;
}

QFont KAEvent::font() const
{
    return d->mUseDefaultFont ? KAEventPrivate::mDefaultFont : d->mFont;
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

void KAEvent::setCommandError(CmdErrType t) const
{
    d->mCommandError = t;
}

KAEvent::CmdErrType KAEvent::commandError() const
{
    return d->mCommandError;
}

void KAEvent::setLogFile(const QString &logfile)
{
    d->mLogFile = logfile;
    if (!logfile.isEmpty()) {
        d->mCommandDisplay = d->mCommandXterm = false;
    }
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

void KAEvent::setEmail(uint from, const KCalCore::Person::List &addresses, const QString &subject,
                       const QStringList &attachments)
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

KCalCore::Person::List KAEvent::emailAddressees() const
{
    return d->mEmailAddresses;
}

QStringList KAEvent::emailAddresses() const
{
    return static_cast<QStringList>(d->mEmailAddresses);
}

QString KAEvent::emailAddresses(const QString &sep) const
{
    return d->mEmailAddresses.join(sep);
}

QString KAEvent::joinEmailAddresses(const KCalCore::Person::List &addresses, const QString &separator)
{
    return EmailAddressList(addresses).join(separator);
}

QStringList KAEvent::emailPureAddresses() const
{
    return d->mEmailAddresses.pureAddresses();
}

QString KAEvent::emailPureAddresses(const QString &sep) const
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

QString KAEvent::emailAttachments(const QString &sep) const
{
    return d->mEmailAttachments.join(sep);
}

bool KAEvent::emailBcc() const
{
    return d->mEmailBcc;
}

void KAEvent::setAudioFile(const QString &filename, float volume, float fadeVolume, int fadeSeconds, int repeatPause, bool allowEmptyFile)
{
    d->setAudioFile(filename, volume, fadeVolume, fadeSeconds, repeatPause, allowEmptyFile);
}

void KAEventPrivate::setAudioFile(const QString &filename, float volume, float fadeVolume, int fadeSeconds, int repeatPause, bool allowEmptyFile)
{
    mAudioFile = filename;
    mSoundVolume = (!allowEmptyFile && filename.isEmpty()) ? -1 : volume;
    if (mSoundVolume >= 0) {
        mFadeVolume  = (fadeSeconds > 0) ? fadeVolume : -1;
        mFadeSeconds = (mFadeVolume >= 0) ? fadeSeconds : 0;
    } else {
        mFadeVolume  = -1;
        mFadeSeconds = 0;
    }
    mRepeatSoundPause = repeatPause;
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
    return d->mRepeatSoundPause >= 0;
}

int KAEvent::repeatSoundPause() const
{
    return d->mRepeatSoundPause;
}

bool KAEvent::beep() const
{
    return d->mBeep;
}

bool KAEvent::speak() const
{
    return (d->mActionSubType == MESSAGE
        ||  (d->mActionSubType == COMMAND && d->mCommandDisplay))
       &&  d->mSpeak;
}

/******************************************************************************
* Set the event to be an alarm template.
*/
void KAEvent::setTemplate(const QString &name, int afterTime)
{
    d->setCategory(CalEvent::TEMPLATE);
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

void KAEvent::setActions(const QString &pre, const QString &post, ExtraActionOptions options)
{
    d->mPreAction = pre;
    d->mPostAction = post;
    d->mExtraActionOptions = options;
}

QString KAEvent::preAction() const
{
    return d->mPreAction;
}

QString KAEvent::postAction() const
{
    return d->mPostAction;
}

KAEvent::ExtraActionOptions KAEvent::extraActionOptions() const
{
    return d->mExtraActionOptions;
}

/******************************************************************************
* Set a reminder.
* 'minutes' = number of minutes BEFORE the main alarm.
*/
void KAEvent::setReminder(int minutes, bool onceOnly)
{
    d->setReminder(minutes, onceOnly);
}

void KAEventPrivate::setReminder(int minutes, bool onceOnly)
{
    if (minutes > 0  &&  mRepeatAtLogin) {
        minutes = 0;
    }
    if (minutes != mReminderMinutes  || (minutes && mReminderActive != ACTIVE_REMINDER)) {
        if (minutes  &&  mReminderActive == NO_REMINDER) {
            ++mAlarmCount;
        } else if (!minutes  &&  mReminderActive != NO_REMINDER) {
            --mAlarmCount;
        }
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
void KAEvent::activateReminderAfter(const DateTime &mainAlarmTime)
{
    d->activateReminderAfter(mainAlarmTime);
}

void KAEventPrivate::activateReminderAfter(const DateTime &mainAlarmTime)
{
    if (mReminderMinutes >= 0  ||  mReminderActive == ACTIVE_REMINDER  ||  !mainAlarmTime.isValid()) {
        return;
    }
    // There is a reminder AFTER the main alarm.
    if (checkRecur() != KARecurrence::NO_RECUR) {
        // For a recurring alarm, the given alarm time must be a recurrence, not a sub-repetition.
        DateTime next;
        //???? For some unknown reason, addSecs(-1) returns the recurrence after the next,
        //???? so addSecs(-60) is used instead.
        if (nextRecurrence(mainAlarmTime.addSecs(-60).effectiveKDateTime(), next) == KAEvent::NO_OCCURRENCE
        ||  mainAlarmTime != next) {
            return;
        }
    } else if (!mRepeatAtLogin) {
        // For a non-recurring alarm, the given alarm time must be the main alarm time.
        if (mainAlarmTime != mStartDateTime) {
            return;
        }
    }

    const DateTime reminderTime = mainAlarmTime.addMins(-mReminderMinutes);
    DateTime next;
    if (nextOccurrence(mainAlarmTime.effectiveKDateTime(), next, KAEvent::RETURN_REPETITION) != KAEvent::NO_OCCURRENCE
    &&  reminderTime >= next) {
        return;    // the reminder time is after the next occurrence of the main alarm
    }

    qDebug() << "Setting reminder at" << reminderTime.effectiveKDateTime().dateTime();
    activate_reminder(true);
    mReminderAfterTime = reminderTime;
}

int KAEvent::reminderMinutes() const
{
    return d->mReminderMinutes;
}

bool KAEvent::reminderActive() const
{
    return d->mReminderActive == KAEventPrivate::ACTIVE_REMINDER;
}

bool KAEvent::reminderOnceOnly() const
{
    return d->mReminderOnceOnly;
}

bool KAEvent::reminderDeferral() const
{
    return d->mDeferral == KAEventPrivate::REMINDER_DEFERRAL;
}

/******************************************************************************
* Defer the event to the specified time.
* If the main alarm time has passed, the main alarm is marked as expired.
* If 'adjustRecurrence' is true, ensure that the next scheduled recurrence is
* after the current time.
*/
void KAEvent::defer(const DateTime &dt, bool reminder, bool adjustRecurrence)
{
    return d->defer(dt, reminder, adjustRecurrence);
}

void KAEventPrivate::defer(const DateTime &dateTime, bool reminder, bool adjustRecurrence)
{
    startChanges();   // prevent multiple trigger time evaluation here
    bool setNextRepetition = false;
    bool checkRepetition = false;
    bool checkReminderAfter = false;
    if (checkRecur() == KARecurrence::NO_RECUR) {
        // Deferring a non-recurring alarm
        if (mReminderMinutes) {
            bool deferReminder = false;
            if (mReminderMinutes > 0) {
                // There's a reminder BEFORE the main alarm
                if (dateTime < mNextMainDateTime.effectiveKDateTime()) {
                    deferReminder = true;
                } else if (mReminderActive == ACTIVE_REMINDER  ||  mDeferral == REMINDER_DEFERRAL) {
                    // Deferring past the main alarm time, so adjust any existing deferral
                    set_deferral(NO_DEFERRAL);
                    mTriggerChanged = true;
                }
            } else if (mReminderMinutes < 0  &&  reminder) {
                deferReminder = true;    // deferring a reminder AFTER the main alarm
            }
            if (deferReminder) {
                set_deferral(REMINDER_DEFERRAL);   // defer reminder alarm
                mDeferralTime = dateTime;
                mTriggerChanged = true;
            }
            if (mReminderActive == ACTIVE_REMINDER) {
                activate_reminder(false);
                mTriggerChanged = true;
            }
        }
        if (mDeferral != REMINDER_DEFERRAL) {
            // We're deferring the main alarm.
            // Main alarm has now expired.
            mNextMainDateTime = mDeferralTime = dateTime;
            set_deferral(NORMAL_DEFERRAL);
            mTriggerChanged = true;
            checkReminderAfter = true;
            if (!mMainExpired) {
                // Mark the alarm as expired now
                mMainExpired = true;
                --mAlarmCount;
                if (mRepeatAtLogin) {
                    // Remove the repeat-at-login alarm, but keep a note of it for archiving purposes
                    mArchiveRepeatAtLogin = true;
                    mRepeatAtLogin = false;
                    --mAlarmCount;
                }
            }
        }
    } else if (reminder) {
        // Deferring a reminder for a recurring alarm
        if (dateTime >= mNextMainDateTime.effectiveKDateTime()) {
            // Trying to defer it past the next main alarm (regardless of whether
            // the reminder triggered before or after the main alarm).
            set_deferral(NO_DEFERRAL);    // (error)
        } else {
            set_deferral(REMINDER_DEFERRAL);
            mDeferralTime = dateTime;
            checkRepetition = true;
        }
        mTriggerChanged = true;
    } else {
        // Deferring a recurring alarm
        mDeferralTime = dateTime;
        if (mDeferral == NO_DEFERRAL) {
            set_deferral(NORMAL_DEFERRAL);
        }
        mTriggerChanged = true;
        checkReminderAfter = true;
        if (adjustRecurrence) {
            const KDateTime now = KDateTime::currentUtcDateTime();
            if (mainEndRepeatTime() < now) {
                // The last repetition (if any) of the current recurrence has already passed.
                // Adjust to the next scheduled recurrence after now.
                if (!mMainExpired  &&  setNextOccurrence(now) == KAEvent::NO_OCCURRENCE) {
                    mMainExpired = true;
                    --mAlarmCount;
                }
            } else {
                setNextRepetition = mRepetition;
            }
        } else {
            checkRepetition = true;
        }
    }
    if (checkReminderAfter  &&  mReminderMinutes < 0  &&  mReminderActive != NO_REMINDER) {
        // Enable/disable the active reminder AFTER the main alarm,
        // depending on whether the deferral is before or after the reminder.
        mReminderActive = (mDeferralTime < mReminderAfterTime) ? ACTIVE_REMINDER : HIDDEN_REMINDER;
    }
    if (checkRepetition) {
        setNextRepetition = (mRepetition  &&  mDeferralTime < mainEndRepeatTime());
    }
    if (setNextRepetition) {
        // The alarm is repeated, and we're deferring to a time before the last repetition.
        // Set the next scheduled repetition to the one after the deferral.
        if (mNextMainDateTime >= mDeferralTime) {
            mNextRepeat = 0;
        } else {
            mNextRepeat = mRepetition.nextRepeatCount(mNextMainDateTime.kDateTime(), mDeferralTime.kDateTime());
        }
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

void KAEventPrivate::cancelDefer()
{
    if (mDeferral != NO_DEFERRAL) {
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
DateTime KAEvent::deferralLimit(DeferLimitType *limitType) const
{
    return d->deferralLimit(limitType);
}

DateTime KAEventPrivate::deferralLimit(KAEvent::DeferLimitType *limitType) const
{
    KAEvent::DeferLimitType ltype = KAEvent::LIMIT_NONE;
    DateTime endTime;
    if (checkRecur() != KARecurrence::NO_RECUR) {
        // It's a recurring alarm. Find the latest time it can be deferred to:
        // it cannot be deferred past its next occurrence or sub-repetition,
        // or any advance reminder before that.
        DateTime reminderTime;
        const KDateTime now = KDateTime::currentUtcDateTime();
        const KAEvent::OccurType type = nextOccurrence(now, endTime, KAEvent::RETURN_REPETITION);
        if (type & KAEvent::OCCURRENCE_REPEAT) {
            ltype = KAEvent::LIMIT_REPETITION;
        } else if (type == KAEvent::NO_OCCURRENCE) {
            ltype = KAEvent::LIMIT_NONE;
        } else if (mReminderActive == ACTIVE_REMINDER  &&  mReminderMinutes > 0
               &&  (now < (reminderTime = endTime.addMins(-mReminderMinutes)))) {
            endTime = reminderTime;
            ltype = KAEvent::LIMIT_REMINDER;
        } else {
            ltype = KAEvent::LIMIT_RECURRENCE;
        }
    } else if (mReminderMinutes < 0) {
        // There is a reminder alarm which occurs AFTER the main alarm.
        // Don't allow the reminder to be deferred past the next main alarm time.
        if (KDateTime::currentUtcDateTime() < mNextMainDateTime.effectiveKDateTime()) {
            endTime = mNextMainDateTime;
            ltype = KAEvent::LIMIT_MAIN;
        }
    } else if (mReminderMinutes > 0
           &&  KDateTime::currentUtcDateTime() < mNextMainDateTime.effectiveKDateTime()) {
        // It's a reminder BEFORE the main alarm.
        // Don't allow it to be deferred past its main alarm time.
        endTime = mNextMainDateTime;
        ltype = KAEvent::LIMIT_MAIN;
    }
    if (ltype != KAEvent::LIMIT_NONE) {
        endTime = endTime.addMins(-1);
    }
    if (limitType) {
        *limitType = ltype;
    }
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

void KAEvent::setTime(const KDateTime &dt)
{
    d->mNextMainDateTime = dt;
    d->mTriggerChanged = true;
}

DateTime KAEvent::mainDateTime(bool withRepeats) const
{
    return d->mainDateTime(withRepeats);
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
void KAEvent::setStartOfDay(const QTime &startOfDay)
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
void KAEvent::adjustStartOfDay(const KAEvent::List &events)
{
    for (int i = 0, end = events.count();  i < end;  ++i) {
        KAEventPrivate *const p = events[i]->d;
        if (p->mStartDateTime.isDateOnly()  &&  p->checkRecur() != KARecurrence::NO_RECUR) {
            p->mRecurrence->setStartDateTime(p->mStartDateTime.effectiveKDateTime(), true);
        }
    }
}

DateTime KAEvent::nextTrigger(TriggerType type) const
{
    d->calcTriggerTimes();
    switch (type) {
        case ALL_TRIGGER:       return d->mAllTrigger;
        case MAIN_TRIGGER:      return d->mMainTrigger;
        case ALL_WORK_TRIGGER:  return d->mAllWorkTrigger;
        case WORK_TRIGGER:      return d->mMainWorkTrigger;
        case DISPLAY_TRIGGER: {
            const bool reminderAfter = d->mMainExpired && d->mReminderActive && d->mReminderMinutes < 0;
            return d->checkRecur() != KARecurrence::NO_RECUR  && (d->mWorkTimeOnly || d->mExcludeHolidays)
                 ? (reminderAfter ? d->mAllWorkTrigger : d->mMainWorkTrigger)
                 : (reminderAfter ? d->mAllTrigger : d->mMainTrigger);
        }
        default:                return DateTime();
    }
}

void KAEvent::setCreatedDateTime(const KDateTime &dt)
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

void KAEventPrivate::setRepeatAtLogin(bool rl)
{
    if (rl  &&  !mRepeatAtLogin) {
        setRepeatAtLoginTrue(true);   // clear incompatible statuses
        ++mAlarmCount;
    } else if (!rl  &&  mRepeatAtLogin) {
        --mAlarmCount;
    }
    mRepeatAtLogin = rl;
    mTriggerChanged = true;
}

/******************************************************************************
* Clear incompatible statuses when repeat-at-login is set.
*/
void KAEventPrivate::setRepeatAtLoginTrue(bool clearReminder)
{
    clearRecur();                // clear recurrences
    if (mReminderMinutes >= 0 && clearReminder) {
        setReminder(0, false);    // clear pre-alarm reminder
    }
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
    d->mExcludeHolidays = ex ? KAEventPrivate::mHolidays : 0;
    // Option only affects recurring alarms
    d->mTriggerChanged = (d->checkRecur() != KARecurrence::NO_RECUR);
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
void KAEvent::setHolidays(const HolidayRegion &h)
{
    KAEventPrivate::mHolidays = &h;
}

void KAEvent::setWorkTimeOnly(bool wto)
{
    d->mWorkTimeOnly = wto;
    // Option only affects recurring alarms
    d->mTriggerChanged = (d->checkRecur() != KARecurrence::NO_RECUR);
}

bool KAEvent::workTimeOnly() const
{
    return d->mWorkTimeOnly;
}

/******************************************************************************
* Check whether a date/time is during working hours and/or holidays, depending
* on the flags set for the specified event.
*/
bool KAEvent::isWorkingTime(const KDateTime &dt) const
{
    return d->isWorkingTime(dt);
}

bool KAEventPrivate::isWorkingTime(const KDateTime &dt) const
{
    if ((mWorkTimeOnly  &&  !mWorkDays.testBit(dt.date().dayOfWeek() - 1))
    ||  (mExcludeHolidays  &&  mHolidays  &&  mHolidays->isHoliday(dt.date()))) {
        return false;
    }
    if (!mWorkTimeOnly) {
        return true;
    }
    return dt.isDateOnly()
       ||  (dt.time() >= mWorkDayStart  &&  dt.time() < mWorkDayEnd);
}

/******************************************************************************
* Set new working days and times.
* Increment a counter so that working-time-only alarms can detect that they
* need to update their next trigger time.
*/
void KAEvent::setWorkTime(const QBitArray &days, const QTime &start, const QTime &end)
{
    if (days != KAEventPrivate::mWorkDays  ||  start != KAEventPrivate::mWorkDayStart  ||  end != KAEventPrivate::mWorkDayEnd) {
        KAEventPrivate::mWorkDays     = days;
        KAEventPrivate::mWorkDayStart = start;
        KAEventPrivate::mWorkDayEnd   = end;
        if (!++KAEventPrivate::mWorkTimeIndex) {
            ++KAEventPrivate::mWorkTimeIndex;
        }
    }
}

/******************************************************************************
* Clear the event's recurrence and alarm repetition data.
*/
void KAEvent::setNoRecur()
{
    d->clearRecur();
}

void KAEventPrivate::clearRecur()
{
    if (mRecurrence || mRepetition) {
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
void KAEvent::setRecurrence(const KARecurrence &recurrence)
{
    d->setRecurrence(recurrence);
}

void KAEventPrivate::setRecurrence(const KARecurrence &recurrence)
{
    startChanges();   // prevent multiple trigger time evaluation here
    delete mRecurrence;
    if (recurrence.recurs()) {
        mRecurrence = new KARecurrence(recurrence);
        mRecurrence->setStartDateTime(mStartDateTime.effectiveKDateTime(), mStartDateTime.isDateOnly());
        mTriggerChanged = true;
    } else {
        if (mRecurrence) {
            mTriggerChanged = true;
        }
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
bool KAEvent::setRecurMinutely(int freq, int count, const KDateTime &end)
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
bool KAEvent::setRecurDaily(int freq, const QBitArray &days, int count, const QDate &end)
{
    const bool success = d->setRecur(RecurrenceRule::rDaily, freq, count, end);
    if (success) {
        int n = 0;
        for (int i = 0;  i < 7;  ++i) {
            if (days.testBit(i)) {
                ++n;
            }
        }
        if (n < 7) {
            d->mRecurrence->addWeeklyDays(days);
        }
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
bool KAEvent::setRecurWeekly(int freq, const QBitArray &days, int count, const QDate &end)
{
    const bool success = d->setRecur(RecurrenceRule::rWeekly, freq, count, end);
    if (success) {
        d->mRecurrence->addWeeklyDays(days);
    }
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
bool KAEvent::setRecurMonthlyByDate(int freq, const QVector<int> &days, int count, const QDate &end)
{
    const bool success = d->setRecur(RecurrenceRule::rMonthly, freq, count, end);
    if (success) {
        for (int i = 0, end = days.count();  i < end;  ++i) {
            d->mRecurrence->addMonthlyDate(days[i]);
        }
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
bool KAEvent::setRecurMonthlyByPos(int freq, const QVector<MonthPos> &posns, int count, const QDate &end)
{
    const bool success = d->setRecur(RecurrenceRule::rMonthly, freq, count, end);
    if (success) {
        for (int i = 0, end = posns.count();  i < end;  ++i) {
            d->mRecurrence->addMonthlyPos(posns[i].weeknum, posns[i].days);
        }
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
bool KAEvent::setRecurAnnualByDate(int freq, const QVector<int> &months, int day, KARecurrence::Feb29Type feb29, int count, const QDate &end)
{
    const bool success = d->setRecur(RecurrenceRule::rYearly, freq, count, end, feb29);
    if (success) {
        for (int i = 0, end = months.count();  i < end;  ++i) {
            d->mRecurrence->addYearlyMonth(months[i]);
        }
        if (day) {
            d->mRecurrence->addMonthlyDate(day);
        }
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
bool KAEvent::setRecurAnnualByPos(int freq, const QVector<MonthPos> &posns, const QVector<int> &months, int count, const QDate &end)
{
    const bool success = d->setRecur(RecurrenceRule::rYearly, freq, count, end);
    if (success) {
        int i = 0;
        int iend;
        for (iend = months.count();  i < iend;  ++i) {
            d->mRecurrence->addYearlyMonth(months[i]);
        }
        for (i = 0, iend = posns.count();  i < iend;  ++i) {
            d->mRecurrence->addYearlyPos(posns[i].weeknum, posns[i].days);
        }
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
bool KAEventPrivate::setRecur(RecurrenceRule::PeriodType recurType, int freq, int count, const QDate &end, KARecurrence::Feb29Type feb29)
{
    KDateTime edt = mNextMainDateTime.kDateTime();
    edt.setDate(end);
    return setRecur(recurType, freq, count, edt, feb29);
}
bool KAEventPrivate::setRecur(RecurrenceRule::PeriodType recurType, int freq, int count, const KDateTime &end, KARecurrence::Feb29Type feb29)
{
    if (count >= -1  && (count || end.date().isValid())) {
        if (!mRecurrence) {
            mRecurrence = new KARecurrence;
        }
        if (mRecurrence->init(recurType, freq, count, mNextMainDateTime.kDateTime(), end, feb29)) {
            return true;
        }
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

KARecurrence *KAEvent::recurrence() const
{
    return d->mRecurrence;
}

/******************************************************************************
* Return the recurrence interval in units of the recurrence period type.
*/
int KAEvent::recurInterval() const
{
    if (d->mRecurrence) {
        switch (d->mRecurrence->type()) {
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

void KAEventPrivate::setFirstRecurrence()
{
    switch (checkRecur()) {
        case KARecurrence::NO_RECUR:
        case KARecurrence::MINUTELY:
            return;
        case KARecurrence::ANNUAL_DATE:
        case KARecurrence::ANNUAL_POS:
            if (mRecurrence->yearMonths().isEmpty()) {
                return;    // (presumably it's a template)
            }
            break;
        case KARecurrence::DAILY:
        case KARecurrence::WEEKLY:
        case KARecurrence::MONTHLY_POS:
        case KARecurrence::MONTHLY_DAY:
            break;
    }
    const KDateTime recurStart = mRecurrence->startDateTime();
    if (mRecurrence->recursOn(recurStart.date(), recurStart.timeSpec())) {
        return;    // it already recurs on the start date
    }

    // Set the frequency to 1 to find the first possible occurrence
    const int frequency = mRecurrence->frequency();
    mRecurrence->setFrequency(1);
    DateTime next;
    nextRecurrence(mNextMainDateTime.effectiveKDateTime(), next);
    if (!next.isValid()) {
        mRecurrence->setStartDateTime(recurStart, mStartDateTime.isDateOnly());    // reinstate the old value
    } else {
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
    if (d->mRepeatAtLogin) {
        return brief ? i18nc("@info Brief form of 'At Login'", "Login") : i18nc("@info", "At login");
    }
    if (d->mRecurrence) {
        const int frequency = d->mRecurrence->frequency();
        switch (d->mRecurrence->defaultRRuleConst()->recurrenceType()) {
            case RecurrenceRule::rMinutely:
                if (frequency < 60) {
                    return i18ncp("@info", "1 Minute", "%1 Minutes", frequency);
                } else if (frequency % 60 == 0) {
                    return i18ncp("@info", "1 Hour", "%1 Hours", frequency / 60);
                } else {
                    QString mins;
                    return i18nc("@info Hours and minutes", "%1h %2m", frequency / 60, mins.sprintf("%02d", frequency % 60));
                }
            case RecurrenceRule::rDaily:
                return i18ncp("@info", "1 Day", "%1 Days", frequency);
            case RecurrenceRule::rWeekly:
                return i18ncp("@info", "1 Week", "%1 Weeks", frequency);
            case RecurrenceRule::rMonthly:
                return i18ncp("@info", "1 Month", "%1 Months", frequency);
            case RecurrenceRule::rYearly:
                return i18ncp("@info", "1 Year", "%1 Years", frequency);
            case RecurrenceRule::rNone:
            default:
                break;
        }
    }
    return brief ? QString() : i18nc("@info No recurrence", "None");
}

/******************************************************************************
* Initialise the event's sub-repetition.
* The repetition length is adjusted if necessary to fit the recurrence interval.
* If the event doesn't recur, the sub-repetition is cleared.
* Reply = false if a non-daily interval was specified for a date-only recurrence.
*/
bool KAEvent::setRepetition(const Repetition &r)
{
    return d->setRepetition(r);
}

bool KAEventPrivate::setRepetition(const Repetition &repetition)
{
    // Don't set mRepetition to zero at the start of this function, in case the
    // 'repetition' parameter passed in is a reference to mRepetition.
    mNextRepeat = 0;
    if (repetition  &&  !mRepeatAtLogin) {
        Q_ASSERT(checkRecur() != KARecurrence::NO_RECUR);
        if (!repetition.isDaily()  &&  mStartDateTime.isDateOnly()) {
            mRepetition.set(0, 0);
            return false;    // interval must be in units of days for date-only alarms
        }
        Duration longestInterval = mRecurrence->longestInterval();
        if (repetition.duration() >= longestInterval) {
            const int count = mStartDateTime.isDateOnly()
                              ? (longestInterval.asDays() - 1) / repetition.intervalDays()
                              : (longestInterval.asSeconds() - 1) / repetition.intervalSeconds();
            mRepetition.set(repetition.interval(), count);
        } else {
            mRepetition = repetition;
        }
        mTriggerChanged = true;
    } else if (mRepetition) {
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
    if (d->mRepetition) {
        if (!d->mRepetition.isDaily()) {
            const int minutes = d->mRepetition.intervalMinutes();
            if (minutes < 60) {
                return i18ncp("@info", "1 Minute", "%1 Minutes", minutes);
            }
            if (minutes % 60 == 0) {
                return i18ncp("@info", "1 Hour", "%1 Hours", minutes / 60);
            }
            QString mins;
            return i18nc("@info Hours and minutes", "%1h %2m", minutes / 60, mins.sprintf("%02d", minutes % 60));
        }
        const int days = d->mRepetition.intervalDays();
        if (days % 7) {
            return i18ncp("@info", "1 Day", "%1 Days", days);
        }
        return i18ncp("@info", "1 Week", "%1 Weeks", days / 7);
    }
    return brief ? QString() : i18nc("@info No repetition", "None");
}

/******************************************************************************
* Determine whether the event will occur after the specified date/time.
* If 'includeRepetitions' is true and the alarm has a sub-repetition, it
* returns true if any repetitions occur after the specified date/time.
*/
bool KAEvent::occursAfter(const KDateTime &preDateTime, bool includeRepetitions) const
{
    return d->occursAfter(preDateTime, includeRepetitions);
}

bool KAEventPrivate::occursAfter(const KDateTime &preDateTime, bool includeRepetitions) const
{
    KDateTime dt;
    if (checkRecur() != KARecurrence::NO_RECUR) {
        if (mRecurrence->duration() < 0) {
            return true;    // infinite recurrence
        }
        dt = mRecurrence->endDateTime();
    } else {
        dt = mNextMainDateTime.effectiveKDateTime();
    }
    if (mStartDateTime.isDateOnly()) {
        QDate pre = preDateTime.date();
        if (preDateTime.toTimeSpec(mStartDateTime.timeSpec()).time() < DateTime::startOfDay()) {
            pre = pre.addDays(-1);    // today's recurrence (if today recurs) is still to come
        }
        if (pre < dt.date()) {
            return true;
        }
    } else if (preDateTime < dt) {
        return true;
    }

    if (includeRepetitions  &&  mRepetition) {
        if (preDateTime < mRepetition.duration().end(dt)) {
            return true;
        }
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
KAEvent::OccurType KAEvent::setNextOccurrence(const KDateTime &preDateTime)
{
    return d->setNextOccurrence(preDateTime);
}

KAEvent::OccurType KAEventPrivate::setNextOccurrence(const KDateTime &preDateTime)
{
    if (preDateTime < mNextMainDateTime.effectiveKDateTime()) {
        return KAEvent::FIRST_OR_ONLY_OCCURRENCE;    // it might not be the first recurrence - tant pis
    }
    KDateTime pre = preDateTime;
    // If there are repetitions, adjust the comparison date/time so that
    // we find the earliest recurrence which has a repetition falling after
    // the specified preDateTime.
    if (mRepetition) {
        pre = mRepetition.duration(-mRepetition.count()).end(preDateTime);
    }

    DateTime afterPre;          // next recurrence after 'pre'
    KAEvent::OccurType type;
    if (pre < mNextMainDateTime.effectiveKDateTime()) {
        afterPre = mNextMainDateTime;
        type = KAEvent::FIRST_OR_ONLY_OCCURRENCE;   // may not actually be the first occurrence
    } else if (checkRecur() != KARecurrence::NO_RECUR) {
        type = nextRecurrence(pre, afterPre);
        if (type == KAEvent::NO_OCCURRENCE) {
            return KAEvent::NO_OCCURRENCE;
        }
        if (type != KAEvent::FIRST_OR_ONLY_OCCURRENCE  &&  afterPre != mNextMainDateTime) {
            // Need to reschedule the next trigger date/time
            mNextMainDateTime = afterPre;
            if (mReminderMinutes > 0  && (mDeferral == REMINDER_DEFERRAL || mReminderActive != ACTIVE_REMINDER)) {
                // Reinstate the advance reminder for the rescheduled recurrence.
                // Note that a reminder AFTER the main alarm will be left active.
                activate_reminder(!mReminderOnceOnly);
            }
            if (mDeferral == REMINDER_DEFERRAL) {
                set_deferral(NO_DEFERRAL);
            }
            mTriggerChanged = true;
        }
    } else {
        return KAEvent::NO_OCCURRENCE;
    }

    if (mRepetition) {
        if (afterPre <= preDateTime) {
            // The next occurrence is a sub-repetition.
            type = static_cast<KAEvent::OccurType>(type | KAEvent::OCCURRENCE_REPEAT);
            mNextRepeat = mRepetition.nextRepeatCount(afterPre.effectiveKDateTime(), preDateTime);
            // Repetitions can't have a reminder, so remove any.
            activate_reminder(false);
            if (mDeferral == REMINDER_DEFERRAL) {
                set_deferral(NO_DEFERRAL);
            }
            mTriggerChanged = true;
        } else if (mNextRepeat) {
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
KAEvent::OccurType KAEvent::nextOccurrence(const KDateTime &preDateTime, DateTime &result, OccurOption o) const
{
    return d->nextOccurrence(preDateTime, result, o);
}

KAEvent::OccurType KAEventPrivate::nextOccurrence(const KDateTime &preDateTime, DateTime &result,
        KAEvent::OccurOption includeRepetitions) const
{
    KDateTime pre = preDateTime;
    if (includeRepetitions != KAEvent::IGNORE_REPETITION) {
        // RETURN_REPETITION or ALLOW_FOR_REPETITION
        if (!mRepetition) {
            includeRepetitions = KAEvent::IGNORE_REPETITION;
        } else {
            pre = mRepetition.duration(-mRepetition.count()).end(preDateTime);
        }
    }

    KAEvent::OccurType type;
    const bool recurs = (checkRecur() != KARecurrence::NO_RECUR);
    if (recurs) {
        type = nextRecurrence(pre, result);
    } else if (pre < mNextMainDateTime.effectiveKDateTime()) {
        result = mNextMainDateTime;
        type = KAEvent::FIRST_OR_ONLY_OCCURRENCE;
    } else {
        result = DateTime();
        type = KAEvent::NO_OCCURRENCE;
    }

    if (type != KAEvent::NO_OCCURRENCE  &&  result <= preDateTime  &&  includeRepetitions != KAEvent::IGNORE_REPETITION) {
        // RETURN_REPETITION or ALLOW_FOR_REPETITION
        // The next occurrence is a sub-repetition
        int repetition = mRepetition.nextRepeatCount(result.kDateTime(), preDateTime);
        const DateTime repeatDT = mRepetition.duration(repetition).end(result.kDateTime());
        if (recurs) {
            // We've found a recurrence before the specified date/time, which has
            // a sub-repetition after the date/time.
            // However, if the intervals between recurrences vary, we could possibly
            // have missed a later recurrence which fits the criterion, so check again.
            DateTime dt;
            const KAEvent::OccurType newType = previousOccurrence(repeatDT.effectiveKDateTime(), dt, false);
            if (dt > result) {
                type = newType;
                result = dt;
                if (includeRepetitions == KAEvent::RETURN_REPETITION  &&  result <= preDateTime) {
                    // The next occurrence is a sub-repetition
                    repetition = mRepetition.nextRepeatCount(result.kDateTime(), preDateTime);
                    result = mRepetition.duration(repetition).end(result.kDateTime());
                    type = static_cast<KAEvent::OccurType>(type | KAEvent::OCCURRENCE_REPEAT);
                }
                return type;
            }
        }
        if (includeRepetitions == KAEvent::RETURN_REPETITION) {
            // The next occurrence is a sub-repetition
            result = repeatDT;
            type = static_cast<KAEvent::OccurType>(type | KAEvent::OCCURRENCE_REPEAT);
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
KAEvent::OccurType KAEvent::previousOccurrence(const KDateTime &afterDateTime, DateTime &result, bool includeRepetitions) const
{
    return d->previousOccurrence(afterDateTime, result, includeRepetitions);
}

KAEvent::OccurType KAEventPrivate::previousOccurrence(const KDateTime &afterDateTime, DateTime &result,
        bool includeRepetitions) const
{
    Q_ASSERT(!afterDateTime.isDateOnly());
    if (mStartDateTime >= afterDateTime) {
        result = KDateTime();
        return KAEvent::NO_OCCURRENCE;     // the event starts after the specified date/time
    }

    // Find the latest recurrence of the event
    KAEvent::OccurType type;
    if (checkRecur() == KARecurrence::NO_RECUR) {
        result = mStartDateTime;
        type = KAEvent::FIRST_OR_ONLY_OCCURRENCE;
    } else {
        const KDateTime recurStart = mRecurrence->startDateTime();
        KDateTime after = afterDateTime.toTimeSpec(mStartDateTime.timeSpec());
        if (mStartDateTime.isDateOnly()  &&  afterDateTime.time() > DateTime::startOfDay()) {
            after = after.addDays(1);    // today's recurrence (if today recurs) has passed
        }
        const KDateTime dt = mRecurrence->getPreviousDateTime(after);
        result = dt;
        result.setDateOnly(mStartDateTime.isDateOnly());
        if (!dt.isValid()) {
            return KAEvent::NO_OCCURRENCE;
        }
        if (dt == recurStart) {
            type = KAEvent::FIRST_OR_ONLY_OCCURRENCE;
        } else if (mRecurrence->getNextDateTime(dt).isValid()) {
            type = result.isDateOnly() ? KAEvent::RECURRENCE_DATE : KAEvent::RECURRENCE_DATE_TIME;
        } else {
            type = KAEvent::LAST_RECURRENCE;
        }
    }

    if (includeRepetitions  &&  mRepetition) {
        // Find the latest repetition which is before the specified time.
        const int repetition = mRepetition.previousRepeatCount(result.effectiveKDateTime(), afterDateTime);
        if (repetition > 0) {
            result = mRepetition.duration(qMin(repetition, mRepetition.count())).end(result.kDateTime());
            return static_cast<KAEvent::OccurType>(type | KAEvent::OCCURRENCE_REPEAT);
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
bool KAEvent::setDisplaying(const KAEvent &e, KAAlarm::Type t, Akonadi::Collection::Id id, const KDateTime &dt, bool showEdit, bool showDefer)
{
    return d->setDisplaying(*e.d, t, id, dt, showEdit, showDefer);
}

bool KAEventPrivate::setDisplaying(const KAEventPrivate &event, KAAlarm::Type alarmType, Akonadi::Collection::Id collectionId,
                                   const KDateTime &repeatAtLoginTime, bool showEdit, bool showDefer)
{
    if (!mDisplaying
    &&  (alarmType == KAAlarm::MAIN_ALARM
      || alarmType == KAAlarm::REMINDER_ALARM
      || alarmType == KAAlarm::DEFERRED_REMINDER_ALARM
      || alarmType == KAAlarm::DEFERRED_ALARM
      || alarmType == KAAlarm::AT_LOGIN_ALARM)) {
//qDebug()<<event.id()<<","<<(alarmType==KAAlarm::MAIN_ALARM?"MAIN":alarmType==KAAlarm::REMINDER_ALARM?"REMINDER":alarmType==KAAlarm::DEFERRED_REMINDER_ALARM?"REMINDER_DEFERRAL":alarmType==KAAlarm::DEFERRED_ALARM?"DEFERRAL":"LOGIN")<<"): time="<<repeatAtLoginTime.toString();
        KAAlarm al = event.alarm(alarmType);
        if (al.isValid()) {
            *this = event;
            // Change the event ID to avoid duplicating the same unique ID as the original event
            setCategory(CalEvent::DISPLAYING);
            mItemId             = -1;    // the display event doesn't have an associated Item
            mCollectionId       = collectionId;  // original collection ID which contained the event
            mDisplayingDefer    = showDefer;
            mDisplayingEdit     = showEdit;
            mDisplaying         = true;
            mDisplayingTime     = (alarmType == KAAlarm::AT_LOGIN_ALARM) ? repeatAtLoginTime : al.dateTime().kDateTime();
            switch (al.type()) {
                case KAAlarm::AT_LOGIN_ALARM:           mDisplayingFlags = KAEvent::REPEAT_AT_LOGIN;  break;
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
void KAEvent::reinstateFromDisplaying(const KCalCore::Event::Ptr &e, Akonadi::Collection::Id &id, bool &showEdit, bool &showDefer)
{
    d->reinstateFromDisplaying(e, id, showEdit, showDefer);
}

void KAEventPrivate::reinstateFromDisplaying(const Event::Ptr &kcalEvent, Akonadi::Collection::Id &collectionId, bool &showEdit, bool &showDefer)
{
    set(kcalEvent);
    if (mDisplaying) {
        // Retrieve the original event's unique ID
        setCategory(CalEvent::ACTIVE);
        collectionId = mCollectionId;
        mCollectionId = -1;
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
    KAAlarm::Private *const al_d = al.d;
    const int displayingFlags = d->mDisplayingFlags;
    if (displayingFlags & REPEAT_AT_LOGIN) {
        al_d->mRepeatAtLogin = true;
        al_d->mType = KAAlarm::AT_LOGIN_ALARM;
    } else if (displayingFlags & KAEventPrivate::DEFERRAL) {
        al_d->mDeferred = true;
        al_d->mTimedDeferral = (displayingFlags & KAEventPrivate::TIMED_FLAG);
        al_d->mType = (displayingFlags & KAEventPrivate::REMINDER) ? KAAlarm::DEFERRED_REMINDER_ALARM : KAAlarm::DEFERRED_ALARM;
    } else if (displayingFlags & KAEventPrivate::REMINDER) {
        al_d->mType = KAAlarm::REMINDER_ALARM;
    } else {
        al_d->mType = KAAlarm::MAIN_ALARM;
    }
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

KAAlarm KAEventPrivate::alarm(KAAlarm::Type type) const
{
    checkRecur();     // ensure recurrence/repetition data is consistent
    KAAlarm al;       // this sets type to INVALID_ALARM
    KAAlarm::Private *const al_d = al.d;
    if (mAlarmCount) {
        al_d->mActionType    = (KAAlarm::Action)mActionSubType;
        al_d->mRepeatAtLogin = false;
        al_d->mDeferred      = false;
        switch (type) {
            case KAAlarm::MAIN_ALARM:
                if (!mMainExpired) {
                    al_d->mType             = KAAlarm::MAIN_ALARM;
                    al_d->mNextMainDateTime = mNextMainDateTime;
                    al_d->mRepetition       = mRepetition;
                    al_d->mNextRepeat       = mNextRepeat;
                }
                break;
            case KAAlarm::REMINDER_ALARM:
                if (mReminderActive == ACTIVE_REMINDER) {
                    al_d->mType = KAAlarm::REMINDER_ALARM;
                    if (mReminderMinutes < 0) {
                        al_d->mNextMainDateTime = mReminderAfterTime;
                    } else if (mReminderOnceOnly) {
                        al_d->mNextMainDateTime = mStartDateTime.addMins(-mReminderMinutes);
                    } else {
                        al_d->mNextMainDateTime = mNextMainDateTime.addMins(-mReminderMinutes);
                    }
                }
                break;
            case KAAlarm::DEFERRED_REMINDER_ALARM:
                if (mDeferral != REMINDER_DEFERRAL) {
                    break;
                }
            // fall through to DEFERRED_ALARM
            case KAAlarm::DEFERRED_ALARM:
                if (mDeferral != NO_DEFERRAL) {
                    al_d->mType             = (mDeferral == REMINDER_DEFERRAL) ? KAAlarm::DEFERRED_REMINDER_ALARM : KAAlarm::DEFERRED_ALARM;
                    al_d->mNextMainDateTime = mDeferralTime;
                    al_d->mDeferred         = true;
                    al_d->mTimedDeferral    = !mDeferralTime.isDateOnly();
                }
                break;
            case KAAlarm::AT_LOGIN_ALARM:
                if (mRepeatAtLogin) {
                    al_d->mType             = KAAlarm::AT_LOGIN_ALARM;
                    al_d->mNextMainDateTime = mAtLoginDateTime;
                    al_d->mRepeatAtLogin    = true;
                }
                break;
            case KAAlarm::DISPLAYING_ALARM:
                if (mDisplaying) {
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

KAAlarm KAEventPrivate::firstAlarm() const
{
    if (mAlarmCount) {
        if (!mMainExpired) {
            return alarm(KAAlarm::MAIN_ALARM);
        }
        return nextAlarm(KAAlarm::MAIN_ALARM);
    }
    return KAAlarm();
}

/******************************************************************************
* Return the next alarm for the event, after the specified alarm.
* N.B. a repeat-at-login alarm can only be returned if it has been read from/
* written to the calendar file.
*/
KAAlarm KAEvent::nextAlarm(const KAAlarm &previousAlarm) const
{
    return d->nextAlarm(previousAlarm.type());
}

KAAlarm KAEvent::nextAlarm(KAAlarm::Type previousType) const
{
    return d->nextAlarm(previousType);
}

KAAlarm KAEventPrivate::nextAlarm(KAAlarm::Type previousType) const
{
    switch (previousType) {
        case KAAlarm::MAIN_ALARM:
            if (mReminderActive == ACTIVE_REMINDER) {
                return alarm(KAAlarm::REMINDER_ALARM);
            }
        // fall through to REMINDER_ALARM
        case KAAlarm::REMINDER_ALARM:
            // There can only be one deferral alarm
            if (mDeferral == REMINDER_DEFERRAL) {
                return alarm(KAAlarm::DEFERRED_REMINDER_ALARM);
            }
            if (mDeferral == NORMAL_DEFERRAL) {
                return alarm(KAAlarm::DEFERRED_ALARM);
            }
        // fall through to DEFERRED_ALARM
        case KAAlarm::DEFERRED_REMINDER_ALARM:
        case KAAlarm::DEFERRED_ALARM:
            if (mRepeatAtLogin) {
                return alarm(KAAlarm::AT_LOGIN_ALARM);
            }
        // fall through to AT_LOGIN_ALARM
        case KAAlarm::AT_LOGIN_ALARM:
            if (mDisplaying) {
                return alarm(KAAlarm::DISPLAYING_ALARM);
            }
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

void KAEventPrivate::removeExpiredAlarm(KAAlarm::Type type)
{
    const int count = mAlarmCount;
    switch (type) {
        case KAAlarm::MAIN_ALARM:
            if (!mReminderActive  ||  mReminderMinutes > 0) {
                mAlarmCount = 0;    // removing main alarm - also remove subsidiary alarms
                break;
            }
            // There is a reminder after the main alarm - retain the
            // reminder and remove other subsidiary alarms.
            mMainExpired = true;    // mark the alarm as expired now
            --mAlarmCount;
            set_deferral(NO_DEFERRAL);
            if (mDisplaying) {
                mDisplaying = false;
                --mAlarmCount;
            }
        // fall through to AT_LOGIN_ALARM
        case KAAlarm::AT_LOGIN_ALARM:
            if (mRepeatAtLogin) {
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
            if (mDisplaying) {
                mDisplaying = false;
                --mAlarmCount;
            }
            break;
        case KAAlarm::INVALID_ALARM:
        default:
            break;
    }
    if (mAlarmCount != count) {
        mTriggerChanged = true;
    }
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

void KAEventPrivate::endChanges()
{
    if (mChangeCount > 0) {
        --mChangeCount;
    }
}

/******************************************************************************
* Return a list of pointers to KAEvent objects.
*/
KAEvent::List KAEvent::ptrList(QVector<KAEvent> &objList)
{
    KAEvent::List ptrs;
    for (int i = 0, count = objList.count();  i < count;  ++i) {
        ptrs += &objList[i];
    }
    return ptrs;
}

void KAEvent::dumpDebug() const
{
#ifndef KDE_NO_DEBUG_OUTPUT
    d->dumpDebug();
#endif
}

#ifndef KDE_NO_DEBUG_OUTPUT
void KAEventPrivate::dumpDebug() const
{
    qDebug() << "KAEvent dump:";
    qDebug() << "-- mEventID:" << mEventID;
    qDebug() << "-- mActionSubType:" << (mActionSubType == KAEvent::MESSAGE ? "MESSAGE" : mActionSubType == KAEvent::FILE ? "FILE" : mActionSubType == KAEvent::COMMAND ? "COMMAND" : mActionSubType == KAEvent::EMAIL ? "EMAIL" : mActionSubType == KAEvent::AUDIO ? "AUDIO" : "??");
    qDebug() << "-- mNextMainDateTime:" << mNextMainDateTime.toString();
    qDebug() << "-- mCommandError:" << mCommandError;
    qDebug() << "-- mAllTrigger:" << mAllTrigger.toString();
    qDebug() << "-- mMainTrigger:" << mMainTrigger.toString();
    qDebug() << "-- mAllWorkTrigger:" << mAllWorkTrigger.toString();
    qDebug() << "-- mMainWorkTrigger:" << mMainWorkTrigger.toString();
    qDebug() << "-- mCategory:" << mCategory;
    if (!mTemplateName.isEmpty()) {
        qDebug() << "-- mTemplateName:" << mTemplateName;
        qDebug() << "-- mTemplateAfterTime:" << mTemplateAfterTime;
    }
    qDebug() << "-- mText:" << mText;
    if (mActionSubType == KAEvent::MESSAGE  ||  mActionSubType == KAEvent::FILE) {
        qDebug() << "-- mBgColour:" << mBgColour.name();
        qDebug() << "-- mFgColour:" << mFgColour.name();
        qDebug() << "-- mUseDefaultFont:" << mUseDefaultFont;
        if (!mUseDefaultFont) {
            qDebug() << "-- mFont:" << mFont.toString();
        }
        qDebug() << "-- mSpeak:" << mSpeak;
        qDebug() << "-- mAudioFile:" << mAudioFile;
        qDebug() << "-- mPreAction:" << mPreAction;
        qDebug() << "-- mExecPreActOnDeferral:" << (mExtraActionOptions & KAEvent::ExecPreActOnDeferral);
        qDebug() << "-- mCancelOnPreActErr:" << (mExtraActionOptions & KAEvent::CancelOnPreActError);
        qDebug() << "-- mDontShowPreActErr:" << (mExtraActionOptions & KAEvent::DontShowPreActError);
        qDebug() << "-- mPostAction:" << mPostAction;
        qDebug() << "-- mLateCancel:" << mLateCancel;
        qDebug() << "-- mAutoClose:" << mAutoClose;
    } else if (mActionSubType == KAEvent::COMMAND) {
        qDebug() << "-- mCommandScript:" << mCommandScript;
        qDebug() << "-- mCommandXterm:" << mCommandXterm;
        qDebug() << "-- mCommandDisplay:" << mCommandDisplay;
        qDebug() << "-- mLogFile:" << mLogFile;
    } else if (mActionSubType == KAEvent::EMAIL) {
        qDebug() << "-- mEmail: FromKMail:" << mEmailFromIdentity;
        qDebug() << "--         Addresses:" << mEmailAddresses.join(QLatin1String(","));
        qDebug() << "--         Subject:" << mEmailSubject;
        qDebug() << "--         Attachments:" << mEmailAttachments.join(QLatin1String(","));
        qDebug() << "--         Bcc:" << mEmailBcc;
    } else if (mActionSubType == KAEvent::AUDIO) {
        qDebug() << "-- mAudioFile:" << mAudioFile;
    }
    qDebug() << "-- mBeep:" << mBeep;
    if (mActionSubType == KAEvent::AUDIO  ||  !mAudioFile.isEmpty()) {
        if (mSoundVolume >= 0) {
            qDebug() << "-- mSoundVolume:" << mSoundVolume;
            if (mFadeVolume >= 0) {
                qDebug() << "-- mFadeVolume:" << mFadeVolume;
                qDebug() << "-- mFadeSeconds:" << mFadeSeconds;
            } else {
                qDebug() << "-- mFadeVolume:-:";
            }
        } else {
            qDebug() << "-- mSoundVolume:-:";
        }
        qDebug() << "-- mRepeatSoundPause:" << mRepeatSoundPause;
    }
    qDebug() << "-- mKMailSerialNumber:" << mKMailSerialNumber;
    qDebug() << "-- mCopyToKOrganizer:" << mCopyToKOrganizer;
    qDebug() << "-- mExcludeHolidays:" << (bool)mExcludeHolidays;
    qDebug() << "-- mWorkTimeOnly:" << mWorkTimeOnly;
    qDebug() << "-- mStartDateTime:" << mStartDateTime.toString();
//     qDebug() << "-- mCreatedDateTime:" << mCreatedDateTime;
    qDebug() << "-- mRepeatAtLogin:" << mRepeatAtLogin;
//     if (mRepeatAtLogin)
//         qDebug() << "-- mAtLoginDateTime:" << mAtLoginDateTime;
    qDebug() << "-- mArchiveRepeatAtLogin:" << mArchiveRepeatAtLogin;
    qDebug() << "-- mConfirmAck:" << mConfirmAck;
    qDebug() << "-- mEnabled:" << mEnabled;
    qDebug() << "-- mItemId:" << mItemId;
    qDebug() << "-- mCollectionId:" << mCollectionId;
    qDebug() << "-- mCompatibility:" << mCompatibility;
    qDebug() << "-- mReadOnly:" << mReadOnly;
    if (mReminderMinutes) {
        qDebug() << "-- mReminderMinutes:" << mReminderMinutes;
        qDebug() << "-- mReminderActive:" << (mReminderActive == ACTIVE_REMINDER ? "active" : mReminderActive == HIDDEN_REMINDER ? "hidden" : "no");
        qDebug() << "-- mReminderOnceOnly:" << mReminderOnceOnly;
    } else if (mDeferral > 0) {
        qDebug() << "-- mDeferral:" << (mDeferral == NORMAL_DEFERRAL ? "normal" : "reminder");
        qDebug() << "-- mDeferralTime:" << mDeferralTime.toString();
    }
    qDebug() << "-- mDeferDefaultMinutes:" << mDeferDefaultMinutes;
    if (mDeferDefaultMinutes) {
        qDebug() << "-- mDeferDefaultDateOnly:" << mDeferDefaultDateOnly;
    }
    if (mDisplaying) {
        qDebug() << "-- mDisplayingTime:" << mDisplayingTime.toString();
        qDebug() << "-- mDisplayingFlags:" << mDisplayingFlags;
        qDebug() << "-- mDisplayingDefer:" << mDisplayingDefer;
        qDebug() << "-- mDisplayingEdit:" << mDisplayingEdit;
    }
    qDebug() << "-- mRevision:" << mRevision;
    qDebug() << "-- mRecurrence:" << mRecurrence;
    if (!mRepetition) {
        qDebug() << "-- mRepetition: 0";
    } else if (mRepetition.isDaily()) {
        qDebug() << "-- mRepetition: count:" << mRepetition.count() << ", interval:" << mRepetition.intervalDays() << "days";
    } else {
        qDebug() << "-- mRepetition: count:" << mRepetition.count() << ", interval:" << mRepetition.intervalMinutes() << "minutes";
    }
    qDebug() << "-- mNextRepeat:" << mNextRepeat;
    qDebug() << "-- mAlarmCount:" << mAlarmCount;
    qDebug() << "-- mMainExpired:" << mMainExpired;
    qDebug() << "-- mDisplaying:" << mDisplaying;
    qDebug() << "KAEvent dump end";
}
#endif

/******************************************************************************
* Fetch the start and next date/time for a KCal::Event.
* Reply = next main date/time.
*/
DateTime KAEventPrivate::readDateTime(const Event::Ptr &event, bool dateOnly, DateTime &start)
{
    start = event->dtStart();
    if (dateOnly) {
        // A date-only event is indicated by the X-KDE-KALARM-FLAGS:DATE property, not
        // by a date-only start date/time (for the reasons given in updateKCalEvent()).
        start.setDateOnly(true);
    }
    DateTime next = start;
    const QString prop = event->customProperty(KACalendar::APPNAME, KAEventPrivate::NEXT_RECUR_PROPERTY);
    if (prop.length() >= 8) {
        // The next due recurrence time is specified
        const QDate d(prop.left(4).toInt(), prop.mid(4, 2).toInt(), prop.mid(6, 2).toInt());
        if (d.isValid()) {
            if (dateOnly  &&  prop.length() == 8) {
                next.setDate(d);
            } else if (!dateOnly  &&  prop.length() == 15  &&  prop[8] == QLatin1Char('T')) {
                const QTime t(prop.mid(9, 2).toInt(), prop.mid(11, 2).toInt(), prop.mid(13, 2).toInt());
                if (t.isValid()) {
                    next.setDate(d);
                    next.setTime(t);
                }
            }
            if (next < start) {
                next = start;    // ensure next recurrence time is valid
            }
        }
    }
    return next;
}

/******************************************************************************
* Parse the alarms for a KCal::Event.
* Reply = map of alarm data, indexed by KAAlarm::Type
*/
void KAEventPrivate::readAlarms(const Event::Ptr &event, void *almap, bool cmdDisplay)
{
    AlarmMap *alarmMap = (AlarmMap *)almap;
    const Alarm::List alarms = event->alarms();

    // Check if it's an audio event with no display alarm
    bool audioOnly = false;
    for (int i = 0, end = alarms.count();  i < end;  ++i) {
        switch (alarms[i]->type()) {
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

    for (int i = 0, end = alarms.count();  i < end;  ++i) {
        // Parse the next alarm's text
        AlarmData data;
        readAlarm(alarms[i], data, audioOnly, cmdDisplay);
        if (data.type != INVALID_ALARM) {
            alarmMap->insert(data.type, data);
        }
    }
}

/******************************************************************************
* Parse a KCal::Alarm.
* If 'audioMain' is true, the event contains an audio alarm but no display alarm.
* Reply = alarm ID (sequence number)
*/
void KAEventPrivate::readAlarm(const Alarm::Ptr &alarm, AlarmData &data, bool audioMain, bool cmdDisplay)
{
    // Parse the next alarm's text
    data.alarm            = alarm;
    data.displayingFlags  = 0;
    data.isEmailText      = false;
    data.speak            = false;
    data.hiddenReminder   = false;
    data.timedDeferral    = false;
    data.nextRepeat       = 0;
    data.repeatSoundPause = -1;
    if (alarm->repeatCount()) {
        bool ok;
        const QString property = alarm->customProperty(KACalendar::APPNAME, KAEventPrivate::NEXT_REPEAT_PROPERTY);
        int n = static_cast<int>(property.toUInt(&ok));
        if (ok) {
            data.nextRepeat = n;
        }
    }
    QString property = alarm->customProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY);
    const QStringList flags = property.split(KAEventPrivate::SC, QString::SkipEmptyParts);
    switch (alarm->type()) {
        case Alarm::Procedure:
            data.action        = KAAlarm::COMMAND;
            data.cleanText     = alarm->programFile();
            data.commandScript = data.cleanText.isEmpty();   // blank command indicates a script
            if (!alarm->programArguments().isEmpty()) {
                if (!data.commandScript) {
                    data.cleanText += QLatin1Char(' ');
                }
                data.cleanText += alarm->programArguments();
            }
            data.extraActionOptions = 0;
            if (flags.contains(KAEventPrivate::EXEC_ON_DEFERRAL_FLAG)) {
                data.extraActionOptions |= KAEvent::ExecPreActOnDeferral;
            }
            if (flags.contains(KAEventPrivate::CANCEL_ON_ERROR_FLAG)) {
                data.extraActionOptions |= KAEvent::CancelOnPreActError;
            }
            if (flags.contains(KAEventPrivate::DONT_SHOW_ERROR_FLAG)) {
                data.extraActionOptions |= KAEvent::DontShowPreActError;
            }
            if (!cmdDisplay) {
                break;
            }
        // fall through to Display
        case Alarm::Display: {
            if (alarm->type() == Alarm::Display) {
                data.action    = KAAlarm::MESSAGE;
                data.cleanText = AlarmText::fromCalendarText(alarm->text(), data.isEmailText);
            }
            const QString property = alarm->customProperty(KACalendar::APPNAME, KAEventPrivate::FONT_COLOUR_PROPERTY);
            const QStringList list = property.split(QLatin1Char(';'), QString::KeepEmptyParts);
            data.bgColour = QColor(255, 255, 255);   // white
            data.fgColour = QColor(0, 0, 0);         // black
            const int n = list.count();
            if (n > 0) {
                if (!list[0].isEmpty()) {
                    QColor c(list[0]);
                    if (c.isValid()) {
                        data.bgColour = c;
                    }
                }
                if (n > 1  &&  !list[1].isEmpty()) {
                    QColor c(list[1]);
                    if (c.isValid()) {
                        data.fgColour = c;
                    }
                }
            }
            data.defaultFont = (n <= 2 || list[2].isEmpty());
            if (!data.defaultFont) {
                data.font.fromString(list[2]);
            }
            break;
        }
        case Alarm::Email: {
            data.action    = KAAlarm::EMAIL;
            data.cleanText = alarm->mailText();
            const int i = flags.indexOf(KAEventPrivate::EMAIL_ID_FLAG);
            data.emailFromId = (i >= 0  &&  i + 1 < flags.count()) ? flags[i + 1].toUInt() : 0;
            break;
        }
        case Alarm::Audio: {
            data.action      = KAAlarm::AUDIO;
            data.cleanText   = alarm->audioFile();
            data.repeatSoundPause = (alarm->repeatCount() == -2) ? alarm->snoozeTime().asSeconds()
                                  : (alarm->repeatCount() == -1) ? 0 : -1;
            data.soundVolume = -1;
            data.fadeVolume  = -1;
            data.fadeSeconds = 0;
            QString property = alarm->customProperty(KACalendar::APPNAME, KAEventPrivate::VOLUME_PROPERTY);
            if (!property.isEmpty()) {
                bool ok;
                float fadeVolume;
                int   fadeSecs = 0;
                const QStringList list = property.split(QLatin1Char(';'), QString::KeepEmptyParts);
                data.soundVolume = list[0].toFloat(&ok);
                if (!ok  ||  data.soundVolume > 1.0f) {
                    data.soundVolume = -1;
                }
                if (data.soundVolume >= 0  &&  list.count() >= 3) {
                    fadeVolume = list[1].toFloat(&ok);
                    if (ok) {
                        fadeSecs = static_cast<int>(list[2].toUInt(&ok));
                    }
                    if (ok  &&  fadeVolume >= 0  &&  fadeVolume <= 1.0f  &&  fadeSecs > 0) {
                        data.fadeVolume  = fadeVolume;
                        data.fadeSeconds = fadeSecs;
                    }
                }
            }
            if (!audioMain) {
                data.type  = AUDIO_ALARM;
                data.speak = flags.contains(KAEventPrivate::SPEAK_FLAG);
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
    bool repeatSound      = false;
    data.type = MAIN_ALARM;
    property = alarm->customProperty(KACalendar::APPNAME, KAEventPrivate::TYPE_PROPERTY);
    const QStringList types = property.split(QLatin1Char(','), QString::SkipEmptyParts);
    for (int i = 0, end = types.count();  i < end;  ++i) {
        const QString type = types[i];
        if (type == KAEventPrivate::AT_LOGIN_TYPE) {
            atLogin = true;
        } else if (type == KAEventPrivate::FILE_TYPE  &&  data.action == KAAlarm::MESSAGE) {
            data.action = KAAlarm::FILE;
        } else if (type == KAEventPrivate::REMINDER_TYPE) {
            reminder = true;
        } else if (type == KAEventPrivate::TIME_DEFERRAL_TYPE) {
            deferral = true;
        } else if (type == KAEventPrivate::DATE_DEFERRAL_TYPE) {
            dateDeferral = deferral = true;
        } else if (type == KAEventPrivate::DISPLAYING_TYPE) {
            data.type = DISPLAYING_ALARM;
        } else if (type == KAEventPrivate::PRE_ACTION_TYPE  &&  data.action == KAAlarm::COMMAND) {
            data.type = PRE_ACTION_ALARM;
        } else if (type == KAEventPrivate::POST_ACTION_TYPE  &&  data.action == KAAlarm::COMMAND) {
            data.type = POST_ACTION_ALARM;
        } else if (type == KAEventPrivate::SOUND_REPEAT_TYPE  &&  data.action == KAAlarm::AUDIO) {
            repeatSound = true;
            if (i + 1 < end) {
                bool ok;
                uint n = types[i + 1].toUInt(&ok);
                if (ok) {
                    data.repeatSoundPause = n;
                    ++i;
                }
            }
        }
    }
    if (repeatSound && data.repeatSoundPause < 0) {
        data.repeatSoundPause = 0;
    } else if (!repeatSound) {
        data.repeatSoundPause = -1;
    }

    if (reminder) {
        if (data.type == MAIN_ALARM) {
            data.type = deferral ? DEFERRED_REMINDER_ALARM : REMINDER_ALARM;
            data.timedDeferral = (deferral && !dateDeferral);
        } else if (data.type == DISPLAYING_ALARM)
            data.displayingFlags = dateDeferral ? REMINDER | DATE_DEFERRAL
                                 : deferral ? REMINDER | TIME_DEFERRAL : REMINDER;
        else if (data.type == REMINDER_ALARM
             &&  flags.contains(KAEventPrivate::HIDDEN_REMINDER_FLAG)) {
            data.hiddenReminder = true;
        }
    } else if (deferral) {
        if (data.type == MAIN_ALARM) {
            data.type = DEFERRED_ALARM;
            data.timedDeferral = !dateDeferral;
        } else if (data.type == DISPLAYING_ALARM) {
            data.displayingFlags = dateDeferral ? DATE_DEFERRAL : TIME_DEFERRAL;
        }
    }
    if (atLogin) {
        if (data.type == MAIN_ALARM) {
            data.type = AT_LOGIN_ALARM;
        } else if (data.type == DISPLAYING_ALARM) {
            data.displayingFlags = KAEvent::REPEAT_AT_LOGIN;
        }
    }
//qDebug()<<"text="<<alarm->text()<<", time="<<alarm->time().toString()<<", valid time="<<alarm->time().isValid();
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
void KAEventPrivate::calcTriggerTimes() const
{
    if (mChangeCount) {
        return;
    }
#ifdef __GNUC__
#warning May need to set date-only alarms to after start-of-day time in working-time checks
#endif
    bool recurs = (checkRecur() != KARecurrence::NO_RECUR);
    if ((recurs  &&  mWorkTimeOnly  &&  mWorkTimeOnly != mWorkTimeIndex)
    ||  (recurs  &&  mExcludeHolidays  &&  mExcludeHolidays != mHolidays)) {
        // It's a work time alarm, and work days/times have changed, or
        // it excludes holidays, and the holidays definition has changed.
        mTriggerChanged = true;
    } else if (!mTriggerChanged) {
        return;
    }
    mTriggerChanged = false;
    if (recurs  &&  mWorkTimeOnly) {
        mWorkTimeOnly = mWorkTimeIndex;    // note which work time definition was used in calculation
    }
    if (recurs  &&  mExcludeHolidays) {
        mExcludeHolidays = mHolidays;    // note which holiday definition was used in calculation
    }

    if (mCategory == CalEvent::ARCHIVED  ||  mCategory == CalEvent::TEMPLATE) {
        // It's a template or archived
        mAllTrigger = mMainTrigger = mAllWorkTrigger = mMainWorkTrigger = KDateTime();
    } else if (mDeferral == NORMAL_DEFERRAL) {
        // For a deferred alarm, working time setting is ignored
        mAllTrigger = mMainTrigger = mAllWorkTrigger = mMainWorkTrigger = mDeferralTime;
    } else {
        mMainTrigger = mainDateTime(true);   // next recurrence or sub-repetition
        mAllTrigger = (mDeferral == REMINDER_DEFERRAL)     ? mDeferralTime
                    : (mReminderActive != ACTIVE_REMINDER) ? mMainTrigger
                    : (mReminderMinutes < 0)               ? mReminderAfterTime
                    :                                        mMainTrigger.addMins(-mReminderMinutes);
        // It's not deferred.
        // If only-during-working-time is set and it recurs, it won't actually trigger
        // unless it falls during working hours.
        if ((!mWorkTimeOnly && !mExcludeHolidays)
        ||  !recurs
        ||  isWorkingTime(mMainTrigger.kDateTime())) {
            // It only occurs once, or it complies with any working hours/holiday
            // restrictions.
            mMainWorkTrigger = mMainTrigger;
            mAllWorkTrigger = mAllTrigger;
        } else if (mWorkTimeOnly) {
            // The alarm is restricted to working hours.
            // Finding the next occurrence during working hours can sometimes take a long time,
            // so mark the next actual trigger as invalid until the calculation completes.
            // Note that reminders are only triggered if the main alarm is during working time.
            if (!mExcludeHolidays) {
                // There are no holiday restrictions.
                calcNextWorkingTime(mMainTrigger);
            } else if (mHolidays) {
                // Holidays are excluded.
                DateTime nextTrigger = mMainTrigger;
                KDateTime kdt;
                for (int i = 0;  i < 20;  ++i) {
                    calcNextWorkingTime(nextTrigger);
                    if (!mHolidays->isHoliday(mMainWorkTrigger.date())) {
                        return;    // found a non-holiday occurrence
                    }
                    kdt = mMainWorkTrigger.effectiveKDateTime();
                    kdt.setTime(QTime(23, 59, 59));
                    const KAEvent::OccurType type = nextOccurrence(kdt, nextTrigger, KAEvent::RETURN_REPETITION);
                    if (!nextTrigger.isValid()) {
                        break;
                    }
                    if (isWorkingTime(nextTrigger.kDateTime())) {
                        const int reminder = (mReminderMinutes > 0) ? mReminderMinutes : 0;   // only interested in reminders BEFORE the alarm
                        mMainWorkTrigger = nextTrigger;
                        mAllWorkTrigger = (type & KAEvent::OCCURRENCE_REPEAT) ? mMainWorkTrigger : mMainWorkTrigger.addMins(-reminder);
                        return;   // found a non-holiday occurrence
                    }
                }
                mMainWorkTrigger = mAllWorkTrigger = DateTime();
            }
        } else if (mExcludeHolidays  &&  mHolidays) {
            // Holidays are excluded.
            DateTime nextTrigger = mMainTrigger;
            KDateTime kdt;
            for (int i = 0;  i < 20;  ++i) {
                kdt = nextTrigger.effectiveKDateTime();
                kdt.setTime(QTime(23, 59, 59));
                const KAEvent::OccurType type = nextOccurrence(kdt, nextTrigger, KAEvent::RETURN_REPETITION);
                if (!nextTrigger.isValid()) {
                    break;
                }
                if (!mHolidays->isHoliday(nextTrigger.date())) {
                    const int reminder = (mReminderMinutes > 0) ? mReminderMinutes : 0;   // only interested in reminders BEFORE the alarm
                    mMainWorkTrigger = nextTrigger;
                    mAllWorkTrigger = (type & KAEvent::OCCURRENCE_REPEAT) ? mMainWorkTrigger : mMainWorkTrigger.addMins(-reminder);
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
void KAEventPrivate::calcNextWorkingTime(const DateTime &nextTrigger) const
{
    qDebug() << "next=" << nextTrigger.kDateTime().dateTime();
    mMainWorkTrigger = mAllWorkTrigger = DateTime();

    for (int i = 0;  ;  ++i) {
        if (i >= 7) {
            return;    // no working days are defined
        }
        if (mWorkDays.testBit(i)) {
            break;
        }
    }
    const KARecurrence::Type recurType = checkRecur();
    KDateTime kdt = nextTrigger.effectiveKDateTime();
    const int reminder = (mReminderMinutes > 0) ? mReminderMinutes : 0;   // only interested in reminders BEFORE the alarm
    // Check if it always falls on the same day(s) of the week.
    const RecurrenceRule *rrule = mRecurrence->defaultRRuleConst();
    if (!rrule) {
        return;    // no recurrence rule!
    }
    unsigned allDaysMask = 0x7F;  // mask bits for all days of week
    bool noWorkPos = false;  // true if no recurrence day position is working day
    const QList<RecurrenceRule::WDayPos> pos = rrule->byDays();
    const int nDayPos = pos.count();  // number of day positions
    if (nDayPos) {
        noWorkPos = true;
        allDaysMask = 0;
        for (int i = 0;  i < nDayPos;  ++i) {
            const int day = pos[i].day() - 1;  // Monday = 0
            if (mWorkDays.testBit(day)) {
                noWorkPos = false;    // found a working day occurrence
            }
            allDaysMask |= 1 << day;
        }
        if (noWorkPos  &&  !mRepetition) {
            return;    // never occurs on a working day
        }
    }
    DateTime newdt;

    if (mStartDateTime.isDateOnly()) {
        // It's a date-only alarm.
        // Sub-repetitions also have to be date-only.
        const int repeatFreq = mRepetition.intervalDays();
        const bool weeklyRepeat = mRepetition && !(repeatFreq % 7);
        const Duration interval = mRecurrence->regularInterval();
        if ((interval  &&  !(interval.asDays() % 7))
        ||  nDayPos == 1) {
            // It recurs on the same day each week
            if (!mRepetition || weeklyRepeat) {
                return;    // any repetitions are also weekly
            }

            // It's a weekly recurrence with a non-weekly sub-repetition.
            // Check one cycle of repetitions for the next one that lands
            // on a working day.
            KDateTime dt(nextTrigger.kDateTime().addDays(1));
            dt.setTime(QTime(0, 0, 0));
            previousOccurrence(dt, newdt, false);
            if (!newdt.isValid()) {
                return;    // this should never happen
            }
            kdt = newdt.effectiveKDateTime();
            const int day = kdt.date().dayOfWeek() - 1;   // Monday = 0
            for (int repeatNum = mNextRepeat + 1;  ;  ++repeatNum) {
                if (repeatNum > mRepetition.count()) {
                    repeatNum = 0;
                }
                if (repeatNum == mNextRepeat) {
                    break;
                }
                if (!repeatNum) {
                    nextOccurrence(newdt.kDateTime(), newdt, KAEvent::IGNORE_REPETITION);
                    if (mWorkDays.testBit(day)) {
                        mMainWorkTrigger = newdt;
                        mAllWorkTrigger  = mMainWorkTrigger.addMins(-reminder);
                        return;
                    }
                    kdt = newdt.effectiveKDateTime();
                } else {
                    const int inc = repeatFreq * repeatNum;
                    if (mWorkDays.testBit((day + inc) % 7)) {
                        kdt = kdt.addDays(inc);
                        kdt.setDateOnly(true);
                        mMainWorkTrigger = mAllWorkTrigger = kdt;
                        return;
                    }
                }
            }
            return;
        }
        if (!mRepetition  ||  weeklyRepeat) {
            // It's a date-only alarm with either no sub-repetition or a
            // sub-repetition which always falls on the same day of the week
            // as the recurrence (if any).
            unsigned days = 0;
            for (; ;) {
                kdt.setTime(QTime(23, 59, 59));
                nextOccurrence(kdt, newdt, KAEvent::IGNORE_REPETITION);
                if (!newdt.isValid()) {
                    return;
                }
                kdt = newdt.effectiveKDateTime();
                const int day = kdt.date().dayOfWeek() - 1;
                if (mWorkDays.testBit(day)) {
                    break;    // found a working day occurrence
                }
                // Prevent indefinite looping (which should never happen anyway)
                if ((days & allDaysMask) == allDaysMask) {
                    return;    // found a recurrence on every possible day of the week!?!
                }
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
        dt.setTime(QTime(0, 0, 0));
        previousOccurrence(dt, newdt, false);
        if (!newdt.isValid()) {
            return;    // this should never happen
        }
        kdt = newdt.effectiveKDateTime();
        int day = kdt.date().dayOfWeek() - 1;   // Monday = 0
        for (int repeatNum = mNextRepeat;  ;  repeatNum = 0) {
            while (++repeatNum <= mRepetition.count()) {
                const int inc = repeatFreq * repeatNum;
                if (mWorkDays.testBit((day + inc) % 7)) {
                    kdt = kdt.addDays(inc);
                    kdt.setDateOnly(true);
                    mMainWorkTrigger = mAllWorkTrigger = kdt;
                    return;
                }
                if ((days & allDaysMask) == allDaysMask) {
                    return;    // found an occurrence on every possible day of the week!?!
                }
                days |= 1 << day;
            }
            nextOccurrence(kdt, newdt, KAEvent::IGNORE_REPETITION);
            if (!newdt.isValid()) {
                return;
            }
            kdt = newdt.effectiveKDateTime();
            day = kdt.date().dayOfWeek() - 1;
            if (mWorkDays.testBit(day)) {
                kdt.setDateOnly(true);
                mMainWorkTrigger = kdt;
                mAllWorkTrigger  = kdt.addSecs(-60 * reminder);
                return;
            }
            if ((days & allDaysMask) == allDaysMask) {
                return;    // found an occurrence on every possible day of the week!?!
            }
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

    if (!recurTimeVaries  &&  !repeatTimeVaries) {
        // The alarm always occurs at the same time of day.
        // Check whether it can ever occur during working hours.
        if (!mayOccurDailyDuringWork(kdt)) {
            return;    // never occurs during working hours
        }

        // Find the next working day it occurs on
        bool repetition = false;
        unsigned days = 0;
        for (; ;) {
            KAEvent::OccurType type = nextOccurrence(kdt, newdt, KAEvent::RETURN_REPETITION);
            if (!newdt.isValid()) {
                return;
            }
            repetition = (type & KAEvent::OCCURRENCE_REPEAT);
            kdt = newdt.effectiveKDateTime();
            const int day = kdt.date().dayOfWeek() - 1;
            if (mWorkDays.testBit(day)) {
                break;    // found a working day occurrence
            }
            // Prevent indefinite looping (which should never happen anyway)
            if (!repetition) {
                if ((days & allDaysMask) == allDaysMask) {
                    return;    // found a recurrence on every possible day of the week!?!
                }
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
    if (tz.isValid()  &&  tz.type() == "KSystemTimeZone") {
        // It's a system time zone, so fetch full transition information
        const KTimeZone ktz = KSystemTimeZones::readZone(tz.name());
        if (ktz.isValid()) {
            tz = ktz;
        }
    }
    const QList<KTimeZone::Transition> tzTransitions = tz.transitions();

    if (recurTimeVaries) {
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
        if (mRepetition) {
            // It's a repetition inside a recurrence, each of which occurs
            // at different times of day (bearing in mind that the repetition
            // may occur at daily intervals after each recurrence).
            // Find the previous recurrence (as opposed to sub-repetition)
            repeatFreq = mRepetition.intervalSeconds();
            previousOccurrence(kdt.addSecs(1), newdt, false);
            if (!newdt.isValid()) {
                return;    // this should never happen
            }
            kdtRecur = newdt.effectiveKDateTime();
            repeatNum = kdtRecur.secsTo(kdt) / repeatFreq;
            kdt = kdtRecur.addSecs(repeatNum * repeatFreq);
        } else {
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
        const bool subdaily = (repeatFreq < 24 * 3600);
//        int period = mRecurrence->frequency() % (24*60);  // it is by definition a MINUTELY recurrence
//        int limit = (24*60 + period - 1) / period;  // number of times until recurrence wraps round
        int transitionIndex = -1;
        for (int n = 0;  n < 7 * 24 * 60;  ++n) {
            if (mRepetition) {
                // Check the sub-repetitions for this recurrence
                for (; ;) {
                    // Find the repeat count to the next start of the working day
                    const int inc = subdaily ? nextWorkRepetition(kdt) : 1;
                    repeatNum += inc;
                    if (repeatNum > mRepetition.count()) {
                        break;
                    }
                    kdt = kdt.addSecs(inc * repeatFreq);
                    const QTime t = kdt.time();
                    if (t >= mWorkDayStart  &&  t < mWorkDayEnd) {
                        if (mWorkDays.testBit(kdt.date().dayOfWeek() - 1)) {
                            mMainWorkTrigger = mAllWorkTrigger = kdt;
                            return;
                        }
                    }
                }
                repeatNum = 0;
            }
            nextOccurrence(kdtRecur, newdt, KAEvent::IGNORE_REPETITION);
            if (!newdt.isValid()) {
                return;
            }
            kdtRecur = newdt.effectiveKDateTime();
            dayRecur = kdtRecur.date().dayOfWeek() - 1;   // Monday = 0
            const QTime t = kdtRecur.time();
            if (t >= mWorkDayStart  &&  t < mWorkDayEnd) {
                if (mWorkDays.testBit(dayRecur)) {
                    mMainWorkTrigger = kdtRecur;
                    mAllWorkTrigger  = kdtRecur.addSecs(-60 * reminder);
                    return;
                }
            }
            if (kdtRecur.utcOffset() != currentOffset) {
                currentOffset = kdtRecur.utcOffset();
            }
            if (t == firstTime  &&  dayRecur == firstDay  &&  currentOffset == firstOffset) {
                // We've wrapped round to the starting day and time.
                // If there are seasonal time changes, check for up
                // to the next year in other time offsets in case the
                // alarm occurs inside working hours then.
                if (!finalDate.isValid()) {
                    finalDate = kdtRecur.date();
                }
                const int i = tz.transitionIndex(kdtRecur.toUtc().dateTime());
                if (i < 0) {
                    return;
                }
                if (i > transitionIndex) {
                    transitionIndex = i;
                }
                if (++transitionIndex >= static_cast<int>(tzTransitions.count())) {
                    return;
                }
                previousOccurrence(KDateTime(tzTransitions[transitionIndex].time(), KDateTime::UTC), newdt, KAEvent::IGNORE_REPETITION);
                kdtRecur = newdt.effectiveKDateTime();
                if (finalDate.daysTo(kdtRecur.date()) > 365) {
                    return;
                }
                firstTime = kdtRecur.time();
                firstOffset = kdtRecur.utcOffset();
                currentOffset = firstOffset;
                firstDay = kdtRecur.date().dayOfWeek() - 1;
            }
            kdt = kdtRecur;
        }
//qDebug()<<"-----exit loop: count="<<limit<<endl;
        return;   // too many iterations
    }

    if (repeatTimeVaries) {
        /* There's a sub-repetition which occurs at different times of
         * day, inside a recurrence which occurs at the same time of day.
         * We potentially need to check recurrences starting on each day.
         * Then, it is still possible that a working time sub-repetition
         * could occur immediately after a seasonal time change.
         */
        // Find the previous recurrence (as opposed to sub-repetition)
        const int repeatFreq = mRepetition.intervalSeconds();
        previousOccurrence(kdt.addSecs(1), newdt, false);
        if (!newdt.isValid()) {
            return;    // this should never happen
        }
        KDateTime kdtRecur = newdt.effectiveKDateTime();
        const bool recurDuringWork = (kdtRecur.time() >= mWorkDayStart  &&  kdtRecur.time() < mWorkDayEnd);

        // Use the previous recurrence as a base for checking whether
        // our tests have wrapped round to the same time/day of week.
        const bool subdaily = (repeatFreq < 24 * 3600);
        unsigned days = 0;
        bool checkTimeChangeOnly = false;
        int transitionIndex = -1;
        for (int limit = 10;  --limit >= 0;) {
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
            nextOccurrence(kdtRecur, newdt, KAEvent::IGNORE_REPETITION);
            KDateTime kdtNextRecur = newdt.effectiveKDateTime();

            int repeatsToCheck = mRepetition.count();
            int repeatsDuringWork = 0;  // 0=unknown, 1=does, -1=never
            for (; ;) {
                // Check the sub-repetitions for this recurrence
                if (repeatsDuringWork >= 0) {
                    for (; ;) {
                        // Find the repeat count to the next start of the working day
                        int inc = subdaily ? nextWorkRepetition(kdt) : 1;
                        repeatNum += inc;
                        const bool pastEnd = (repeatNum > mRepetition.count());
                        if (pastEnd) {
                            inc -= repeatNum - mRepetition.count();
                        }
                        repeatsToCheck -= inc;
                        kdt = kdt.addSecs(inc * repeatFreq);
                        if (kdtNextRecur.isValid()  &&  kdt >= kdtNextRecur) {
                            // This sub-repetition is past the next recurrence,
                            // so start the check again from the next recurrence.
                            repeatsToCheck = mRepetition.count();
                            break;
                        }
                        if (pastEnd) {
                            break;
                        }
                        const QTime t = kdt.time();
                        if (t >= mWorkDayStart  &&  t < mWorkDayEnd) {
                            if (mWorkDays.testBit(kdt.date().dayOfWeek() - 1)) {
                                mMainWorkTrigger = mAllWorkTrigger = kdt;
                                return;
                            }
                            repeatsDuringWork = 1;
                        } else if (!repeatsDuringWork  &&  repeatsToCheck <= 0) {
                            // Sub-repetitions never occur during working hours
                            repeatsDuringWork = -1;
                            break;
                        }
                    }
                }
                repeatNum = 0;
                if (repeatsDuringWork < 0  &&  !recurDuringWork) {
                    break;    // it never occurs during working hours
                }

                // Check the next recurrence
                if (!kdtNextRecur.isValid()) {
                    return;
                }
                if (checkTimeChangeOnly  || (days & allDaysMask) == allDaysMask) {
                    break;    // found a recurrence on every possible day of the week!?!
                }
                kdtRecur = kdtNextRecur;
                nextOccurrence(kdtRecur, newdt, KAEvent::IGNORE_REPETITION);
                kdtNextRecur = newdt.effectiveKDateTime();
                dateRecur = kdtRecur.date();
                dayRecur = dateRecur.dayOfWeek() - 1;
                if (recurDuringWork  &&  mWorkDays.testBit(dayRecur)) {
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
            if (i < 0) {
                return;
            }
            if (i > transitionIndex) {
                transitionIndex = i;
            }
            if (++transitionIndex >= static_cast<int>(tzTransitions.count())) {
                return;
            }
            kdt = KDateTime(tzTransitions[transitionIndex].time(), KDateTime::UTC);
            previousOccurrence(kdt, newdt, KAEvent::IGNORE_REPETITION);
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
int KAEventPrivate::nextWorkRepetition(const KDateTime &pre) const
{
    KDateTime nextWork(pre);
    if (pre.time() < mWorkDayStart) {
        nextWork.setTime(mWorkDayStart);
    } else {
        const int preDay = pre.date().dayOfWeek() - 1;   // Monday = 0
        for (int n = 1;  ;  ++n) {
            if (n >= 7) {
                return mRepetition.count() + 1;    // should never happen
            }
            if (mWorkDays.testBit((preDay + n) % 7)) {
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
bool KAEventPrivate::mayOccurDailyDuringWork(const KDateTime &kdt) const
{
    if (!kdt.isDateOnly()
    &&  (kdt.time() < mWorkDayStart || kdt.time() >= mWorkDayEnd)) {
        return false;    // its time is outside working hours
    }
    // Check if it always occurs on the same day of the week
    const Duration interval = mRecurrence->regularInterval();
    if (interval  &&  interval.isDaily()  &&  !(interval.asDays() % 7)) {
        // It recurs weekly
        if (!mRepetition  || (mRepetition.isDaily() && !(mRepetition.intervalDays() % 7))) {
            return false;    // any repetitions are also weekly
        }
        // Repetitions are daily. Check if any occur on working days
        // by checking the first recurrence and up to 6 repetitions.
        int day = mRecurrence->startDateTime().date().dayOfWeek() - 1;   // Monday = 0
        const int repeatDays = mRepetition.intervalDays();
        const int maxRepeat = (mRepetition.count() < 6) ? mRepetition.count() : 6;
        for (int i = 0;  !mWorkDays.testBit(day);  ++i, day = (day + repeatDays) % 7) {
            if (i >= maxRepeat) {
                return false;    // no working day occurrences
            }
        }
    }
    return true;
}

/******************************************************************************
* Set the specified alarm to be an audio alarm with the given file name.
*/
void KAEventPrivate::setAudioAlarm(const Alarm::Ptr &alarm) const
{
    alarm->setAudioAlarm(mAudioFile);  // empty for a beep or for speaking
    if (mSoundVolume >= 0)
        alarm->setCustomProperty(KACalendar::APPNAME, VOLUME_PROPERTY,
                                 QString::fromLatin1("%1;%2;%3;%4").arg(QString::number(mSoundVolume, 'f', 2))
                                 .arg(QString::number(mFadeVolume, 'f', 2))
                                 .arg(mFadeSeconds));
}

/******************************************************************************
* Get the date/time of the next recurrence of the event, after the specified
* date/time.
* 'result' = date/time of next occurrence, or invalid date/time if none.
*/
KAEvent::OccurType KAEventPrivate::nextRecurrence(const KDateTime &preDateTime, DateTime &result) const
{
    const KDateTime recurStart = mRecurrence->startDateTime();
    KDateTime pre = preDateTime.toTimeSpec(mStartDateTime.timeSpec());
    if (mStartDateTime.isDateOnly()  &&  !pre.isDateOnly()  &&  pre.time() < DateTime::startOfDay()) {
        pre = pre.addDays(-1);    // today's recurrence (if today recurs) is still to come
        pre.setTime(DateTime::startOfDay());
    }
    const KDateTime dt = mRecurrence->getNextDateTime(pre);
    result = dt;
    result.setDateOnly(mStartDateTime.isDateOnly());
    if (!dt.isValid()) {
        return KAEvent::NO_OCCURRENCE;
    }
    if (dt == recurStart) {
        return KAEvent::FIRST_OR_ONLY_OCCURRENCE;
    }
    if (mRecurrence->duration() >= 0  &&  dt == mRecurrence->endDateTime()) {
        return KAEvent::LAST_RECURRENCE;
    }
    return result.isDateOnly() ? KAEvent::RECURRENCE_DATE : KAEvent::RECURRENCE_DATE_TIME;
}

/******************************************************************************
* Validate the event's recurrence data, correcting any inconsistencies (which
* should never occur!).
* Reply = recurrence period type.
*/
KARecurrence::Type KAEventPrivate::checkRecur() const
{
    if (mRecurrence) {
        KARecurrence::Type type = mRecurrence->type();
        switch (type) {
            case KARecurrence::MINUTELY:     // hourly
            case KARecurrence::DAILY:        // daily
            case KARecurrence::WEEKLY:       // weekly on multiple days of week
            case KARecurrence::MONTHLY_DAY:  // monthly on multiple dates in month
            case KARecurrence::MONTHLY_POS:  // monthly on multiple nth day of week
            case KARecurrence::ANNUAL_DATE:  // annually on multiple months (day of month = start date)
            case KARecurrence::ANNUAL_POS:   // annually on multiple nth day of week in multiple months
                return type;
            default:
                if (mRecurrence) {
                    const_cast<KAEventPrivate *>(this)->clearRecur();    // this shouldn't ever be necessary!!
                }
                break;
        }
    }
    if (mRepetition) {  // can't have a repetition without a recurrence
        const_cast<KAEventPrivate *>(this)->clearRecur();    // this shouldn't ever be necessary!!
    }
    return KARecurrence::NO_RECUR;
}

/******************************************************************************
* If the calendar was written by a previous version of KAlarm, do any
* necessary format conversions on the events to ensure that when the calendar
* is saved, no information is lost or corrupted.
* Reply = true if any conversions were done.
*/
bool KAEvent::convertKCalEvents(const Calendar::Ptr &calendar, int calendarVersion)
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
    if (calendarVersion == -Version(0, 5, 7)) {
        // The calendar file was written by the KDE 3.0.0 version of KAlarm 0.5.7.
        // Summer time was ignored when converting to UTC.
        calendarVersion = -calendarVersion;
        adjustSummerTime = true;
    }

    if (calendarVersion >= currentCalendarVersion()) {
        return false;
    }

    qDebug() << "Adjusting version" << calendarVersion;
    const bool pre_0_7    = (calendarVersion < Version(0, 7, 0));
    const bool pre_0_9    = (calendarVersion < Version(0, 9, 0));
    const bool pre_0_9_2  = (calendarVersion < Version(0, 9, 2));
    const bool pre_1_1_1  = (calendarVersion < Version(1, 1, 1));
    const bool pre_1_2_1  = (calendarVersion < Version(1, 2, 1));
    const bool pre_1_3_0  = (calendarVersion < Version(1, 3, 0));
    const bool pre_1_3_1  = (calendarVersion < Version(1, 3, 1));
    const bool pre_1_4_14 = (calendarVersion < Version(1, 4, 14));
    const bool pre_1_5_0  = (calendarVersion < Version(1, 5, 0));
    const bool pre_1_9_0  = (calendarVersion < Version(1, 9, 0));
    const bool pre_1_9_2  = (calendarVersion < Version(1, 9, 2));
    const bool pre_1_9_7  = (calendarVersion < Version(1, 9, 7));
    const bool pre_1_9_9  = (calendarVersion < Version(1, 9, 9));
    const bool pre_1_9_10 = (calendarVersion < Version(1, 9, 10));
    const bool pre_2_2_9  = (calendarVersion < Version(2, 2, 9));
    const bool pre_2_3_0  = (calendarVersion < Version(2, 3, 0));
    const bool pre_2_3_2  = (calendarVersion < Version(2, 3, 2));
    const bool pre_2_7_0  = (calendarVersion < Version(2, 7, 0));
    Q_ASSERT(currentCalendarVersion() == Version(2, 7, 0));

    KTimeZone localZone;
    if (pre_1_9_2) {
        localZone = KSystemTimeZones::local();
    }

    bool converted = false;
    const Event::List events = calendar->rawEvents();
    for (int ei = 0, eend = events.count();  ei < eend;  ++ei) {
        Event::Ptr event = events[ei];
        const Alarm::List alarms = event->alarms();
        if (alarms.isEmpty()) {
            continue;    // KAlarm isn't interested in events without alarms
        }
        event->startUpdates();   // prevent multiple update notifications
        const bool readOnly = event->isReadOnly();
        if (readOnly) {
            event->setReadOnly(false);
        }
        QStringList cats = event->categories();
        bool addLateCancel = false;
        QStringList flags;

        if (pre_0_7  &&  event->allDay()) {
            // It's a KAlarm pre-0.7 calendar file.
            // Ensure that when the calendar is saved, the alarm time isn't lost.
            event->setAllDay(false);
        }

        if (pre_0_9) {
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
            for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai) {
                Alarm::Ptr alarm = alarms[ai];
                bool atLogin    = false;
                bool deferral   = false;
                bool lateCancel = false;
                KAAlarm::Action action = KAAlarm::MESSAGE;
                const QString txt = alarm->text();
                const int length = txt.length();
                int i = 0;
                if (txt[0].isDigit()) {
                    while (++i < length  &&  txt[i].isDigit()) ;
                    if (i < length  &&  txt[i++] == SEPARATOR) {
                        while (i < length) {
                            const QChar ch = txt[i++];
                            if (ch == SEPARATOR) {
                                break;
                            }
                            if (ch == LATE_CANCEL_CODE) {
                                lateCancel = true;
                            } else if (ch == AT_LOGIN_CODE) {
                                atLogin = true;
                            } else if (ch == DEFERRAL_CODE) {
                                deferral = true;
                            }
                        }
                    } else {
                        i = 0;    // invalid prefix
                    }
                }
                if (txt.indexOf(TEXT_PREFIX, i) == i) {
                    i += TEXT_PREFIX.length();
                } else if (txt.indexOf(FILE_PREFIX, i) == i) {
                    action = KAAlarm::FILE;
                    i += FILE_PREFIX.length();
                } else if (txt.indexOf(COMMAND_PREFIX, i) == i) {
                    action = KAAlarm::COMMAND;
                    i += COMMAND_PREFIX.length();
                } else {
                    i = 0;
                }
                const QString altxt = txt.mid(i);

                QStringList types;
                switch (action) {
                    case KAAlarm::FILE:
                        types += KAEventPrivate::FILE_TYPE;
                    // fall through to MESSAGE
                    case KAAlarm::MESSAGE:
                        alarm->setDisplayAlarm(altxt);
                        break;
                    case KAAlarm::COMMAND:
                        setProcedureAlarm(alarm, altxt);
                        break;
                    case KAAlarm::EMAIL:     // email alarms were introduced in KAlarm 0.9
                    case KAAlarm::AUDIO:     // audio alarms (with no display) were introduced in KAlarm 2.3.2
                        break;
                }
                if (atLogin) {
                    types += KAEventPrivate::AT_LOGIN_TYPE;
                    lateCancel = false;
                } else if (deferral) {
                    types += KAEventPrivate::TIME_DEFERRAL_TYPE;
                }
                if (lateCancel) {
                    addLateCancel = true;
                }
                if (types.count() > 0) {
                    alarm->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::TYPE_PROPERTY, types.join(QLatin1String(",")));
                }

                if (pre_0_7  &&  alarm->repeatCount() > 0  &&  alarm->snoozeTime().value() > 0) {
                    // It's a KAlarm pre-0.7 calendar file.
                    // Minutely recurrences were stored differently.
                    Recurrence *recur = event->recurrence();
                    if (recur  &&  recur->recurs()) {
                        recur->setMinutely(alarm->snoozeTime().asSeconds() / 60);
                        recur->setDuration(alarm->repeatCount() + 1);
                        alarm->setRepeatCount(0);
                        alarm->setSnoozeTime(0);
                    }
                }

                if (adjustSummerTime) {
                    // The calendar file was written by the KDE 3.0.0 version of KAlarm 0.5.7.
                    // Summer time was ignored when converting to UTC.
                    KDateTime dt = alarm->time();
                    const time_t t = dt.toTime_t();
                    const struct tm *dtm = localtime(&t);
                    if (dtm->tm_isdst) {
                        dt = dt.addSecs(-3600);
                        alarm->setTime(dt);
                    }
                }
            }
        }

        if (pre_0_9_2) {
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
            if (CalEvent::status(event) == CalEvent::ARCHIVED) {
                event->setCreated(event->dtEnd());
            }
            KDateTime start = event->dtStart();
            if (event->allDay()) {
                event->setAllDay(false);
                start.setTime(QTime(0, 0));
                flags += KAEventPrivate::DATE_ONLY_FLAG;
            }
            event->setDtEnd(KDateTime());

            for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai) {
                Alarm::Ptr alarm = alarms[ai];
                alarm->setStartOffset(start.secsTo(alarm->time()));
            }

            if (!cats.isEmpty()) {
                for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai) {
                    Alarm::Ptr alarm = alarms[ai];
                    if (alarm->type() == Alarm::Display)
                        alarm->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::FONT_COLOUR_PROPERTY,
                                                 QString::fromLatin1("%1;;").arg(cats.at(0)));
                }
                cats.removeAt(0);
            }

            for (int i = 0, end = cats.count();  i < end;  ++i) {
                if (cats.at(i) == BEEP_CATEGORY) {
                    cats.removeAt(i);

                    Alarm::Ptr alarm = event->newAlarm();
                    alarm->setEnabled(true);
                    alarm->setAudioAlarm();
                    KDateTime dt = event->dtStart();    // default

                    // Parse and order the alarms to know which one's date/time to use
                    KAEventPrivate::AlarmMap alarmMap;
                    KAEventPrivate::readAlarms(event, &alarmMap);
                    KAEventPrivate::AlarmMap::ConstIterator it = alarmMap.constBegin();
                    if (it != alarmMap.constEnd()) {
                        dt = it.value().alarm->time();
                        break;
                    }
                    alarm->setStartOffset(start.secsTo(dt));
                    break;
                }
            }
        }

        if (pre_1_1_1) {
            /*
             * It's a KAlarm pre-1.1.1 calendar file.
             * Convert simple LATECANCEL category to LATECANCEL:n where n = minutes late.
             */
            int i;
            while ((i = cats.indexOf(LATE_CANCEL_CAT)) >= 0) {
                cats.removeAt(i);
                addLateCancel = true;
            }
        }

        if (pre_1_2_1) {
            /*
             * It's a KAlarm pre-1.2.1 calendar file.
             * Convert email display alarms from translated to untranslated header prefixes.
             */
            for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai) {
                Alarm::Ptr alarm = alarms[ai];
                if (alarm->type() == Alarm::Display) {
                    const QString oldtext = alarm->text();
                    const QString newtext = AlarmText::toCalendarText(oldtext);
                    if (oldtext != newtext) {
                        alarm->setDisplayAlarm(newtext);
                    }
                }
            }
        }

        if (pre_1_3_0) {
            /*
             * It's a KAlarm pre-1.3.0 calendar file.
             * Convert simple TMPLDEFTIME category to TMPLAFTTIME:n where n = minutes after.
             */
            int i;
            while ((i = cats.indexOf(TEMPL_DEF_TIME_CAT)) >= 0) {
                cats.removeAt(i);
                (flags += KAEventPrivate::TEMPL_AFTER_TIME_FLAG) += QLatin1String("0");
            }
        }

        if (pre_1_3_1) {
            /*
             * It's a KAlarm pre-1.3.1 calendar file.
             * Convert simple XTERM category to LOG:xterm:
             */
            int i;
            while ((i = cats.indexOf(EXEC_IN_XTERM_CAT)) >= 0) {
                cats.removeAt(i);
                event->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::LOG_PROPERTY, KAEventPrivate::xtermURL);
            }
        }

        if (pre_1_9_0) {
            /*
             * It's a KAlarm pre-1.9 calendar file.
             * Add the X-KDE-KALARM-STATUS custom property.
             * Convert KAlarm categories to custom fields.
             */
            CalEvent::setStatus(event, CalEvent::status(event));
            for (int i = 0;  i < cats.count();) {
                const QString cat = cats.at(i);
                if (cat == DATE_ONLY_CATEGORY) {
                    flags += KAEventPrivate::DATE_ONLY_FLAG;
                } else if (cat == CONFIRM_ACK_CATEGORY) {
                    flags += KAEventPrivate::CONFIRM_ACK_FLAG;
                } else if (cat == EMAIL_BCC_CATEGORY) {
                    flags += KAEventPrivate::EMAIL_BCC_FLAG;
                } else if (cat == KORGANIZER_CATEGORY) {
                    flags += KAEventPrivate::KORGANIZER_FLAG;
                } else if (cat.startsWith(DEFER_CATEGORY)) {
                    (flags += KAEventPrivate::DEFER_FLAG) += cat.mid(DEFER_CATEGORY.length());
                } else if (cat.startsWith(TEMPL_AFTER_TIME_CATEGORY)) {
                    (flags += KAEventPrivate::TEMPL_AFTER_TIME_FLAG) += cat.mid(TEMPL_AFTER_TIME_CATEGORY.length());
                } else if (cat.startsWith(LATE_CANCEL_CATEGORY)) {
                    (flags += KAEventPrivate::LATE_CANCEL_FLAG) += cat.mid(LATE_CANCEL_CATEGORY.length());
                } else if (cat.startsWith(AUTO_CLOSE_CATEGORY)) {
                    (flags += KAEventPrivate::AUTO_CLOSE_FLAG) += cat.mid(AUTO_CLOSE_CATEGORY.length());
                } else if (cat.startsWith(KMAIL_SERNUM_CATEGORY)) {
                    (flags += KAEventPrivate::KMAIL_SERNUM_FLAG) += cat.mid(KMAIL_SERNUM_CATEGORY.length());
                } else if (cat == ARCHIVE_CATEGORY) {
                    event->setCustomProperty(KACalendar::APPNAME, ARCHIVE_PROPERTY, QLatin1String("0"));
                } else if (cat.startsWith(ARCHIVE_CATEGORIES)) {
                    event->setCustomProperty(KACalendar::APPNAME, ARCHIVE_PROPERTY, cat.mid(ARCHIVE_CATEGORIES.length()));
                } else if (cat.startsWith(LOG_CATEGORY)) {
                    event->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::LOG_PROPERTY, cat.mid(LOG_CATEGORY.length()));
                } else {
                    ++i;   // Not a KAlarm category, so leave it
                    continue;
                }
                cats.removeAt(i);
            }
        }

        if (pre_1_9_2) {
            /*
             * It's a KAlarm pre-1.9.2 calendar file.
             * Convert from clock time to the local system time zone.
             */
            event->shiftTimes(KDateTime::ClockTime, localZone);
            converted = true;
        }

        if (addLateCancel) {
            (flags += KAEventPrivate::LATE_CANCEL_FLAG) += QLatin1String("1");
        }
        if (!flags.isEmpty()) {
            event->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY, flags.join(KAEventPrivate::SC));
        }
        event->setCategories(cats);

        if ((pre_1_4_14  || (pre_1_9_7 && !pre_1_9_0))
        &&  event->recurrence()  &&  event->recurrence()->recurs()) {
            /*
             * It's a KAlarm pre-1.4.14 or KAlarm 1.9 series pre-1.9.7 calendar file.
             * For recurring events, convert the main alarm offset to an absolute
             * time in the X-KDE-KALARM-NEXTRECUR property, and set main alarm
             * offsets to zero, and convert deferral alarm offsets to be relative to
             * the next recurrence.
             */
            const QStringList flags = event->customProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY).split(KAEventPrivate::SC, QString::SkipEmptyParts);
            const bool dateOnly = flags.contains(KAEventPrivate::DATE_ONLY_FLAG);
            KDateTime startDateTime = event->dtStart();
            if (dateOnly) {
                startDateTime.setDateOnly(true);
            }
            // Convert the main alarm and get the next main trigger time from it
            KDateTime nextMainDateTime;
            bool mainExpired = true;
            for (int i = 0, alend = alarms.count();  i < alend;  ++i) {
                Alarm::Ptr alarm = alarms[i];
                if (!alarm->hasStartOffset()) {
                    continue;
                }
                // Find whether the alarm triggers at the same time as the main
                // alarm, in which case its offset needs to be set to 0. The
                // following trigger with the main alarm:
                //  - Additional audio alarm
                //  - PRE_ACTION_TYPE
                //  - POST_ACTION_TYPE
                //  - DISPLAYING_TYPE
                bool mainAlarm = true;
                QString property = alarm->customProperty(KACalendar::APPNAME, KAEventPrivate::TYPE_PROPERTY);
                const QStringList types = property.split(QLatin1Char(','), QString::SkipEmptyParts);
                for (int t = 0;  t < types.count();  ++t) {
                    QString type = types[t];
                    if (type == KAEventPrivate::AT_LOGIN_TYPE
                    ||  type == KAEventPrivate::TIME_DEFERRAL_TYPE
                    ||  type == KAEventPrivate::DATE_DEFERRAL_TYPE
                    ||  type == KAEventPrivate::REMINDER_TYPE
                    ||  type == REMINDER_ONCE_TYPE) {
                        mainAlarm = false;
                        break;
                    }
                }
                if (mainAlarm) {
                    if (mainExpired) {
                        // All main alarms are supposed to be at the same time, so
                        // don't readjust the event's time for subsequent main alarms.
                        mainExpired = false;
                        nextMainDateTime = alarm->time();
                        nextMainDateTime.setDateOnly(dateOnly);
                        nextMainDateTime = nextMainDateTime.toTimeSpec(startDateTime);
                        if (nextMainDateTime != startDateTime) {
                            QDateTime dt = nextMainDateTime.dateTime();
                            event->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::NEXT_RECUR_PROPERTY,
                                                     dt.toString(dateOnly ? QLatin1String("yyyyMMdd") : QLatin1String("yyyyMMddThhmmss")));
                        }
                    }
                    alarm->setStartOffset(0);
                    converted = true;
                }
            }
            int adjustment;
            if (mainExpired) {
                // It's an expired recurrence.
                // Set the alarm offset relative to the first actual occurrence
                // (taking account of possible exceptions).
                KDateTime dt = event->recurrence()->getNextDateTime(startDateTime.addDays(-1));
                dt.setDateOnly(dateOnly);
                adjustment = startDateTime.secsTo(dt);
            } else {
                adjustment = startDateTime.secsTo(nextMainDateTime);
            }
            if (adjustment) {
                // Convert deferred alarms
                for (int i = 0, alend = alarms.count();  i < alend;  ++i) {
                    Alarm::Ptr alarm = alarms[i];
                    if (!alarm->hasStartOffset()) {
                        continue;
                    }
                    const QString property = alarm->customProperty(KACalendar::APPNAME, KAEventPrivate::TYPE_PROPERTY);
                    const QStringList types = property.split(QLatin1Char(','), QString::SkipEmptyParts);
                    for (int t = 0;  t < types.count();  ++t) {
                        const QString type = types[t];
                        if (type == KAEventPrivate::TIME_DEFERRAL_TYPE
                        ||  type == KAEventPrivate::DATE_DEFERRAL_TYPE) {
                            alarm->setStartOffset(alarm->startOffset().asSeconds() - adjustment);
                            converted = true;
                            break;
                        }
                    }
                }
            }
        }

        if (pre_1_5_0  || (pre_1_9_9 && !pre_1_9_0)) {
            /*
             * It's a KAlarm pre-1.5.0 or KAlarm 1.9 series pre-1.9.9 calendar file.
             * Convert email identity names to uoids.
             */
            for (int i = 0, alend = alarms.count();  i < alend;  ++i) {
                Alarm::Ptr alarm = alarms[i];
                const QString name = alarm->customProperty(KACalendar::APPNAME, KMAIL_ID_PROPERTY);
                if (name.isEmpty()) {
                    continue;
                }
                const uint id = Identities::identityUoid(name);
                if (id) {
                    alarm->setCustomProperty(KACalendar::APPNAME, EMAIL_ID_PROPERTY, QString::number(id));
                }
                alarm->removeCustomProperty(KACalendar::APPNAME, KMAIL_ID_PROPERTY);
                converted = true;
            }
        }

        if (pre_1_9_10) {
            /*
             * It's a KAlarm pre-1.9.10 calendar file.
             * Convert simple repetitions without a recurrence, to a recurrence.
             */
            if (KAEventPrivate::convertRepetition(event)) {
                converted = true;
            }
        }

        if (pre_2_2_9  || (pre_2_3_2 && !pre_2_3_0)) {
            /*
             * It's a KAlarm pre-2.2.9 or KAlarm 2.3 series pre-2.3.2 calendar file.
             * Set the time in the calendar for all date-only alarms to 00:00.
             */
            if (KAEventPrivate::convertStartOfDay(event)) {
                converted = true;
            }
        }

        if (pre_2_7_0) {
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
            const QString prop = event->customProperty(KACalendar::APPNAME, ARCHIVE_PROPERTY);
            if (!prop.isEmpty()) {
                // Convert the event's ARCHIVE property to parameters in the FLAGS property
                flags = event->customProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY).split(KAEventPrivate::SC, QString::SkipEmptyParts);
                flags << KAEventPrivate::ARCHIVE_FLAG;
                flagsValid = true;
                if (prop != QLatin1String("0")) { // "0" was a dummy parameter if no others were present
                    // It's the archive property containing a reminder time and/or repeat-at-login flag.
                    // This was present when no reminder/at-login alarm was pending.
                    const QStringList list = prop.split(KAEventPrivate::SC, QString::SkipEmptyParts);
                    for (int i = 0;  i < list.count();  ++i) {
                        if (list[i] == KAEventPrivate::AT_LOGIN_TYPE) {
                            flags << KAEventPrivate::AT_LOGIN_TYPE;
                        } else if (list[i] == ARCHIVE_REMINDER_ONCE_TYPE) {
                            reminderOnce = true;
                        } else if (!list[i].isEmpty()  &&  !list[i].startsWith(QChar::fromLatin1('-'))) {
                            reminder = list[i];
                        }
                    }
                }
                event->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY, flags.join(KAEventPrivate::SC));
                event->removeCustomProperty(KACalendar::APPNAME, ARCHIVE_PROPERTY);
            }

            for (int i = 0, alend = alarms.count();  i < alend;  ++i) {
                Alarm::Ptr alarm = alarms[i];
                // Convert EMAILID, SPEAK, ERRCANCEL, ERRNOSHOW properties
                QStringList flags;
                QString property = alarm->customProperty(KACalendar::APPNAME, EMAIL_ID_PROPERTY);
                if (!property.isEmpty()) {
                    flags << KAEventPrivate::EMAIL_ID_FLAG << property;
                    alarm->removeCustomProperty(KACalendar::APPNAME, EMAIL_ID_PROPERTY);
                }
                if (!alarm->customProperty(KACalendar::APPNAME, SPEAK_PROPERTY).isEmpty()) {
                    flags << KAEventPrivate::SPEAK_FLAG;
                    alarm->removeCustomProperty(KACalendar::APPNAME, SPEAK_PROPERTY);
                }
                if (!alarm->customProperty(KACalendar::APPNAME, CANCEL_ON_ERROR_PROPERTY).isEmpty()) {
                    flags << KAEventPrivate::CANCEL_ON_ERROR_FLAG;
                    alarm->removeCustomProperty(KACalendar::APPNAME, CANCEL_ON_ERROR_PROPERTY);
                }
                if (!alarm->customProperty(KACalendar::APPNAME, DONT_SHOW_ERROR_PROPERTY).isEmpty()) {
                    flags << KAEventPrivate::DONT_SHOW_ERROR_FLAG;
                    alarm->removeCustomProperty(KACalendar::APPNAME, DONT_SHOW_ERROR_PROPERTY);
                }
                if (!flags.isEmpty()) {
                    alarm->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY, flags.join(KAEventPrivate::SC));
                }

                // Invalidate negative reminder periods in alarms
                if (!alarm->hasStartOffset()) {
                    continue;
                }
                property = alarm->customProperty(KACalendar::APPNAME, KAEventPrivate::TYPE_PROPERTY);
                QStringList types = property.split(QChar::fromLatin1(','), QString::SkipEmptyParts);
                const int r = types.indexOf(REMINDER_ONCE_TYPE);
                if (r >= 0) {
                    // Move reminder-once indicator from the alarm to the event's FLAGS property
                    types[r] = KAEventPrivate::REMINDER_TYPE;
                    alarm->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::TYPE_PROPERTY, types.join(QChar::fromLatin1(',')));
                    reminderOnce = true;
                }
                if (r >= 0  ||  types.contains(KAEventPrivate::REMINDER_TYPE)) {
                    // The alarm is a reminder alarm
                    const int offset = alarm->startOffset().asSeconds();
                    if (offset > 0) {
                        alarm->setStartOffset(0);
                        converted = true;
                    } else if (offset < 0) {
                        reminder = reminderToString(offset / 60);
                    }
                }
            }
            if (!reminder.isEmpty()) {
                // Write reminder parameters into the event's FLAGS property
                if (!flagsValid) {
                    flags = event->customProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY).split(KAEventPrivate::SC, QString::SkipEmptyParts);
                }
                if (flags.indexOf(KAEventPrivate::REMINDER_TYPE) < 0) {
                    flags += KAEventPrivate::REMINDER_TYPE;
                    if (reminderOnce) {
                        flags += KAEventPrivate::REMINDER_ONCE_FLAG;
                    }
                    flags += reminder;
                }
            }
        }

        if (readOnly) {
            event->setReadOnly(true);
        }
        event->endUpdates();     // finally issue an update notification
    }
    return converted;
}

/******************************************************************************
* Set the time for a date-only event to 00:00.
* Reply = true if the event was updated.
*/
bool KAEventPrivate::convertStartOfDay(const Event::Ptr &event)
{
    bool changed = false;
    const QTime midnight(0, 0);
    const QStringList flags = event->customProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY).split(KAEventPrivate::SC, QString::SkipEmptyParts);
    if (flags.indexOf(KAEventPrivate::DATE_ONLY_FLAG) >= 0) {
        // It's an untimed event, so fix it
        const KDateTime oldDt = event->dtStart();
        const int adjustment = oldDt.time().secsTo(midnight);
        if (adjustment) {
            event->setDtStart(KDateTime(oldDt.date(), midnight, oldDt.timeSpec()));
            int deferralOffset = 0;
            AlarmMap alarmMap;
            readAlarms(event, &alarmMap);
            for (AlarmMap::ConstIterator it = alarmMap.constBegin();  it != alarmMap.constEnd();  ++it) {
                const AlarmData &data = it.value();
                if (!data.alarm->hasStartOffset()) {
                    continue;
                }
                if (data.timedDeferral) {
                    // Found a timed deferral alarm, so adjust the offset
                    deferralOffset = data.alarm->startOffset().asSeconds();
                    const_cast<Alarm *>(data.alarm.data())->setStartOffset(deferralOffset - adjustment);
                } else if (data.type == AUDIO_ALARM
                       &&  data.alarm->startOffset().asSeconds() == deferralOffset) {
                    // Audio alarm is set for the same time as the above deferral alarm
                    const_cast<Alarm *>(data.alarm.data())->setStartOffset(deferralOffset - adjustment);
                }
            }
            changed = true;
        }
    } else {
        // It's a timed event. Fix any untimed alarms.
        bool foundDeferral = false;
        int deferralOffset = 0;
        int newDeferralOffset = 0;
        DateTime start;
        const KDateTime nextMainDateTime = readDateTime(event, false, start).kDateTime();
        AlarmMap alarmMap;
        readAlarms(event, &alarmMap);
        for (AlarmMap::ConstIterator it = alarmMap.constBegin();  it != alarmMap.constEnd();  ++it) {
            const AlarmData &data = it.value();
            if (!data.alarm->hasStartOffset()) {
                continue;
            }
            if ((data.type & DEFERRED_ALARM)  &&  !data.timedDeferral) {
                // Found a date-only deferral alarm, so adjust its time
                KDateTime altime = data.alarm->startOffset().end(nextMainDateTime);
                altime.setTime(midnight);
                deferralOffset = data.alarm->startOffset().asSeconds();
                newDeferralOffset = event->dtStart().secsTo(altime);
                const_cast<Alarm *>(data.alarm.data())->setStartOffset(newDeferralOffset);
                foundDeferral = true;
                changed = true;
            } else if (foundDeferral
                   &&  data.type == AUDIO_ALARM
                   &&  data.alarm->startOffset().asSeconds() == deferralOffset) {
                // Audio alarm is set for the same time as the above deferral alarm
                const_cast<Alarm *>(data.alarm.data())->setStartOffset(newDeferralOffset);
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
bool KAEventPrivate::convertRepetition(const Event::Ptr &event)
{
    const Alarm::List alarms = event->alarms();
    if (alarms.isEmpty()) {
        return false;
    }
    Recurrence *recur = event->recurrence();   // guaranteed to return non-null
    if (recur->recurs()) {
        return false;
    }
    bool converted = false;
    const bool readOnly = event->isReadOnly();
    for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai) {
        Alarm::Ptr alarm = alarms[ai];
        if (alarm->repeatCount() > 0  &&  alarm->snoozeTime().value() > 0) {
            if (!converted) {
                event->startUpdates();   // prevent multiple update notifications
                if (readOnly) {
                    event->setReadOnly(false);
                }
                if ((alarm->snoozeTime().asSeconds() % (24 * 3600)) != 0) {
                    recur->setMinutely(alarm->snoozeTime().asSeconds() / 60);
                } else {
                    recur->setDaily(alarm->snoozeTime().asDays());
                }
                recur->setDuration(alarm->repeatCount() + 1);
                converted = true;
            }
            alarm->setRepeatCount(0);
            alarm->setSnoozeTime(0);
        }
    }
    if (converted) {
        if (readOnly) {
            event->setReadOnly(true);
        }
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

KAAlarm::KAAlarm(const KAAlarm &other)
    : d(new Private(*other.d))
{
}

KAAlarm::~KAAlarm()
{
    delete d;
}

KAAlarm &KAAlarm::operator=(const KAAlarm &other)
{
    if (&other != this) {
        *d = *other.d;
    }
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

void KAAlarm::setTime(const DateTime &dt)
{
    d->mNextMainDateTime = dt;
}

void KAAlarm::setTime(const KDateTime &dt)
{
    d->mNextMainDateTime = dt;
}

#ifdef KDE_NO_DEBUG_OUTPUT
const char *KAAlarm::debugType(Type)
{
    return "";
}
#else
const char *KAAlarm::debugType(Type type)
{
    switch (type) {
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
EmailAddressList &EmailAddressList::operator=(const Person::List &addresses)
{
    clear();
    for (int p = 0, end = addresses.count();  p < end;  ++p) {
        if (!addresses[p]->email().isEmpty()) {
            append(addresses[p]);
        }
    }
    return *this;
}

/******************************************************************************
* Return the email address list as a string list of email addresses.
*/
EmailAddressList::operator QStringList() const
{
    QStringList list;
    for (int p = 0, end = count();  p < end;  ++p) {
        list += address(p);
    }
    return list;
}

/******************************************************************************
* Return the email address list as a string, each address being delimited by
* the specified separator string.
*/
QString EmailAddressList::join(const QString &separator) const
{
    QString result;
    bool first = true;
    for (int p = 0, end = count();  p < end;  ++p) {
        if (first) {
            first = false;
        } else {
            result += separator;
        }
        result += address(p);
    }
    return result;
}

/******************************************************************************
* Convert one item into an email address, including name.
*/
QString EmailAddressList::address(int index) const
{
    if (index < 0  ||  index > count()) {
        return QString();
    }
    QString result;
    bool quote = false;
    const Person::Ptr person = (*this)[index];
    const QString name = person->name();
    if (!name.isEmpty()) {
        // Need to enclose the name in quotes if it has any special characters
        for (int i = 0, len = name.length();  i < len;  ++i) {
            const QChar ch = name[i];
            if (!ch.isLetterOrNumber()) {
                quote = true;
                result += QLatin1Char('\"');
                break;
            }
        }
        result += (*this)[index]->name();
        result += (quote ? QLatin1String("\" <") : QLatin1String(" <"));
        quote = true;    // need angle brackets round email address
    }

    result += person->email();
    if (quote) {
        result += QLatin1Char('>');
    }
    return result;
}

/******************************************************************************
* Return a list of the pure email addresses, excluding names.
*/
QStringList EmailAddressList::pureAddresses() const
{
    QStringList list;
    for (int p = 0, end = count();  p < end;  ++p) {
        list += at(p)->email();
    }
    return list;
}

/******************************************************************************
* Return a list of the pure email addresses, excluding names, as a string.
*/
QString EmailAddressList::pureAddresses(const QString &separator) const
{
    QString result;
    bool first = true;
    for (int p = 0, end = count();  p < end;  ++p) {
        if (first) {
            first = false;
        } else {
            result += separator;
        }
        result += at(p)->email();
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
static void setProcedureAlarm(const Alarm::Ptr &alarm, const QString &commandLine)
{
    QString command;
    QString arguments;
    QChar quoteChar;
    bool quoted = false;
    const uint posMax = commandLine.length();
    uint pos;
    for (pos = 0;  pos < posMax;  ++pos) {
        const QChar ch = commandLine[pos];
        if (quoted) {
            if (ch == quoteChar) {
                ++pos;    // omit the quote character
                break;
            }
            command += ch;
        } else {
            bool done = false;
            switch (ch.toLatin1()) {
                case ' ':
                case ';':
                case '|':
                case '<':
                case '>':
                    done = !command.isEmpty();
                    break;
                case '\'':
                case '"':
                    if (command.isEmpty()) {
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
            if (done) {
                break;
            }
        }
    }

    // Skip any spaces after the command
    for (;  pos < posMax  &&  commandLine[pos] == QLatin1Char(' ');  ++pos) ;
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
    if (count % 1440 == 0) {
        unit = 'D';
        count /= 1440;
    } else if (count % 60 == 0) {
        unit = 'H';
        count /= 60;
    }
    if (minutes < 0) {
        count = -count;
    }
    return QString::fromLatin1("%1%2").arg(count).arg(unit);
}

} // namespace KAlarmCal

