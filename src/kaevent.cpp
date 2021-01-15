/*
 *  kaevent.cpp  -  represents KAlarm calendar events
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kaevent.h"

#include "akonadi.h"   // for deprecated setItemPayload() only
#include "alarmtext.h"
#include "identities.h"
#include "version.h"

#include <KCalendarCore/MemoryCalendar>
#include <KHolidays/Holiday>
#include <KHolidays/HolidayRegion>

#include <klocalizedstring.h>

#include "kalarmcal_debug.h"

using namespace KCalendarCore;
using namespace KHolidays;

namespace KAlarmCal
{

//=============================================================================

using EmailAddress = KCalendarCore::Person;
class EmailAddressList : public KCalendarCore::Person::List
{
public:
    EmailAddressList() : KCalendarCore::Person::List() { }
    EmailAddressList(const KCalendarCore::Person::List &list)
    {
        operator=(list);
    }
    EmailAddressList &operator=(const KCalendarCore::Person::List &);
    operator QStringList() const;
    QString     join(const QString &separator) const;
    QStringList pureAddresses() const;
    QString     pureAddresses(const QString &separator) const;
private:
    QString     address(int index) const;
};

//=============================================================================

class Q_DECL_HIDDEN KAAlarm::Private
{
public:
    Private();

    Action      mActionType;           // alarm action type
    Type        mType{INVALID_ALARM};  // alarm type
    DateTime    mNextMainDateTime;     // next time to display the alarm, excluding repetitions
    Repetition  mRepetition;           // sub-repetition count and interval
    int         mNextRepeat{0};        // repetition count of next due sub-repetition
    bool        mRepeatAtLogin{false}; // whether to repeat the alarm at every login
    bool        mRecurs;               // there is a recurrence rule for the alarm
    bool        mDeferred{false};      // whether the alarm is an extra deferred/deferred-reminder alarm
    bool        mTimedDeferral;        // if mDeferred = true: true if the deferral is timed, false if date-only
};

//=============================================================================

class Q_DECL_HIDDEN KAEventPrivate : public QSharedData
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
        QFont                       font;
        QColor                      bgColour, fgColour;
        float                       soundVolume;
        float                       fadeVolume;
        int                         fadeSeconds;
        int                         repeatSoundPause;
        int                         nextRepeat;
        uint                        emailFromId;
        KAEventPrivate::AlarmType   type;
        KAAlarm::Action             action;
        int                         displayingFlags;
        KAEvent::ExtraActionOptions extraActionOptions;
        bool                        speak;
        bool                        defaultFont;
        bool                        isEmailText;
        bool                        commandScript;
        bool                        timedDeferral;
        bool                        hiddenReminder;
    };
    typedef QMap<AlarmType, AlarmData> AlarmMap;

    KAEventPrivate();
    KAEventPrivate(const KADateTime &, const QString &name, const QString &message,
                   const QColor &bg, const QColor &fg, const QFont &f,
                   KAEvent::SubAction, int lateCancel, KAEvent::Flags flags,
                   bool changesPending = false);
    explicit KAEventPrivate(const KCalendarCore::Event::Ptr &);
    KAEventPrivate(const KAEventPrivate &);
    ~KAEventPrivate()
    {
        delete mRecurrence;
    }
    KAEventPrivate &operator=(const KAEventPrivate &e)
    {
        if (&e != this) {
            copy(e);
        }
        return *this;
    }
    void               setAudioFile(const QString &filename, float volume, float fadeVolume, int fadeSeconds, int repeatPause, bool allowEmptyFile);
    KAEvent::OccurType setNextOccurrence(const KADateTime &preDateTime);
    void               setFirstRecurrence();
    void               setCategory(CalEvent::Type);
    void               setRepeatAtLogin(bool);
    void               setRepeatAtLoginTrue(bool clearReminder);
    void               setReminder(int minutes, bool onceOnly);
    void               activateReminderAfter(const DateTime &mainAlarmTime);
    void               defer(const DateTime &, bool reminder, bool adjustRecurrence = false);
    void               cancelDefer();
    bool               setDisplaying(const KAEventPrivate &, KAAlarm::Type, ResourceId, const KADateTime &dt, bool showEdit, bool showDefer);
    void               reinstateFromDisplaying(const KCalendarCore::Event::Ptr &, ResourceId &, bool &showEdit, bool &showDefer);
    void               startChanges()
    {
        ++mChangeCount;
    }
    void               endChanges();
    void               removeExpiredAlarm(KAAlarm::Type);
    bool               compare(const KAEventPrivate&, KAEvent::Comparison) const;
    KAAlarm            alarm(KAAlarm::Type) const;
    KAAlarm            firstAlarm() const;
    KAAlarm            nextAlarm(KAAlarm::Type) const;
    bool               updateKCalEvent(const KCalendarCore::Event::Ptr &, KAEvent::UidAction, bool setCustomProperties = true) const;
    DateTime           mainDateTime(bool withRepeats = false) const
    {
        return (withRepeats && mNextRepeat && mRepetition)
               ? DateTime(mRepetition.duration(mNextRepeat).end(mNextMainDateTime.qDateTime())) : mNextMainDateTime;
    }
    DateTime           mainEndRepeatTime() const
    {
        return mRepetition ? DateTime(mRepetition.duration().end(mNextMainDateTime.qDateTime())) : mNextMainDateTime;
    }
    DateTime           deferralLimit(KAEvent::DeferLimitType * = nullptr) const;
    KAEvent::Flags     flags() const;
    bool               isWorkingTime(const KADateTime &) const;
    bool               setRepetition(const Repetition &);
    bool               occursAfter(const KADateTime &preDateTime, bool includeRepetitions) const;
    KAEvent::OccurType nextOccurrence(const KADateTime &preDateTime, DateTime &result, KAEvent::OccurOption = KAEvent::IGNORE_REPETITION) const;
    KAEvent::OccurType previousOccurrence(const KADateTime &afterDateTime, DateTime &result, bool includeRepetitions = false) const;
    void               setRecurrence(const KARecurrence &);
    bool               setRecur(KCalendarCore::RecurrenceRule::PeriodType, int freq, int count, const QDate &end, KARecurrence::Feb29Type = KARecurrence::Feb29_None);
    bool               setRecur(KCalendarCore::RecurrenceRule::PeriodType, int freq, int count, const KADateTime &end, KARecurrence::Feb29Type = KARecurrence::Feb29_None);
    KARecurrence::Type checkRecur() const;
    void               clearRecur();
    void               calcTriggerTimes() const;
#ifdef KDE_NO_DEBUG_OUTPUT
    void               dumpDebug() const  { }
#else
    void               dumpDebug() const;
#endif
    static bool        convertRepetition(const KCalendarCore::Event::Ptr &);
    static bool        convertStartOfDay(const KCalendarCore::Event::Ptr &);
    static DateTime    readDateTime(const KCalendarCore::Event::Ptr &, bool localZone, bool dateOnly, DateTime &start);
    static void        readAlarms(const KCalendarCore::Event::Ptr &, AlarmMap *, bool cmdDisplay = false);
    static void        readAlarm(const KCalendarCore::Alarm::Ptr &, AlarmData &, bool audioMain, bool cmdDisplay = false);
    static QSharedPointer<const HolidayRegion> holidays();
private:
    void               copy(const KAEventPrivate &);
    bool               mayOccurDailyDuringWork(const KADateTime &) const;
    int                nextWorkRepetition(const KADateTime &pre) const;
    void               calcNextWorkingTime(const DateTime &nextTrigger) const;
    DateTime           nextWorkingTime() const;
    KAEvent::OccurType nextRecurrence(const KADateTime &preDateTime, DateTime &result) const;
    void               setAudioAlarm(const KCalendarCore::Alarm::Ptr &) const;
    KCalendarCore::Alarm::Ptr initKCalAlarm(const KCalendarCore::Event::Ptr &, const DateTime &, const QStringList &types, AlarmType = INVALID_ALARM) const;
    KCalendarCore::Alarm::Ptr initKCalAlarm(const KCalendarCore::Event::Ptr &, int startOffsetSecs, const QStringList &types, AlarmType = INVALID_ALARM) const;
    inline void        set_deferral(DeferType);
    inline void        activate_reminder(bool activate);
    static int         transitionIndex(const QDateTime &utc, const QTimeZone::OffsetDataList& transitions);

public:
    static QFont       mDefaultFont;       // default alarm message font
    static QSharedPointer<const HolidayRegion> mHolidays;  // holiday region to use
    static QBitArray   mWorkDays;          // working days of the week
    static QTime       mWorkDayStart;      // start time of the working day
    static QTime       mWorkDayEnd;        // end time of the working day
    static int         mWorkTimeIndex;     // incremented every time working days/times are changed
    mutable DateTime   mAllTrigger;        // next trigger time, including reminders, ignoring working hours
    mutable DateTime   mMainTrigger;       // next trigger time, ignoring reminders and working hours
    mutable DateTime   mAllWorkTrigger;    // next trigger time, taking account of reminders and working hours
    mutable DateTime   mMainWorkTrigger;   // next trigger time, ignoring reminders but taking account of working hours
    mutable KAEvent::CmdErrType mCommandError{KAEvent::CMD_NO_ERROR}; // command execution error last time the alarm triggered

    QString            mEventID;           // UID: KCalendarCore::Event unique ID
    QMap<QByteArray, QString> mCustomProperties; // KCalendarCore::Event's non-KAlarm custom properties
    Akonadi::Item::Id  mItemId{-1};        // Akonadi::Item ID for this event
    mutable ResourceId mResourceId{-1};    // ID of resource containing the event, or for a displaying event,
                                           // saved resource ID (not the resource the event is in)
    QString            mName;              // name of the alarm
    QString            mText;              // message text, file URL, command, email body [or audio file for KAAlarm]
    QString            mAudioFile;         // ATTACH: audio file to play
    QString            mPreAction;         // command to execute before alarm is displayed
    QString            mPostAction;        // command to execute after alarm window is closed
    DateTime           mStartDateTime;     // DTSTART and DTEND: start and end time for event
    KADateTime         mCreatedDateTime;   // CREATED: date event was created, or saved in archive calendar
    DateTime           mNextMainDateTime;  // next time to display the alarm, excluding repetitions
    KADateTime         mAtLoginDateTime;   // repeat-at-login end time
    DateTime           mDeferralTime;      // extra time to trigger alarm (if alarm or reminder deferred)
    DateTime           mDisplayingTime;    // date/time shown in the alarm currently being displayed
    int                mDisplayingFlags;   // type of alarm which is currently being displayed (for display alarm)
    int                mReminderMinutes{0};// how long in advance reminder is to be, or 0 if none (<0 for reminder AFTER the alarm)
    DateTime           mReminderAfterTime; // if mReminderActive true, time to trigger reminder AFTER the main alarm, or invalid if not pending
    ReminderType       mReminderActive{NO_REMINDER}; // whether a reminder is due (before next, or after last, main alarm/recurrence)
    int                mDeferDefaultMinutes{0}; // default number of minutes for deferral dialog, or 0 to select time control
    bool               mDeferDefaultDateOnly{false}; // select date-only by default in deferral dialog
    int                mRevision{0};           // SEQUENCE: revision number of the original alarm, or 0
    KARecurrence      *mRecurrence{nullptr};   // RECUR: recurrence specification, or 0 if none
    Repetition         mRepetition;            // sub-repetition count and interval
    int                mNextRepeat{0};         // repetition count of next due sub-repetition
    int                mAlarmCount{0};         // number of alarms: count of !mMainExpired, mRepeatAtLogin, mDeferral, mReminderActive, mDisplaying
    DeferType          mDeferral{NO_DEFERRAL}; // whether the alarm is an extra deferred/deferred-reminder alarm
    Akonadi::Item::Id  mAkonadiItemId{-1};     // if email text, message's Akonadi item ID
    int                mTemplateAfterTime{-1}; // time not specified: use n minutes after default time, or -1 (applies to templates only)
    QColor             mBgColour;              // background colour of alarm message
    QColor             mFgColour;              // foreground colour of alarm message, or invalid for default
    QFont              mFont;                  // font of alarm message (ignored if mUseDefaultFont true)
    uint               mEmailFromIdentity{0};  // standard email identity uoid for 'From' field, or empty
    EmailAddressList   mEmailAddresses;        // ATTENDEE: addresses to send email to
    QString            mEmailSubject;          // SUMMARY: subject line of email
    QStringList        mEmailAttachments;      // ATTACH: email attachment file names
    mutable int        mChangeCount{0};        // >0 = inhibit calling calcTriggerTimes()
    mutable bool       mTriggerChanged{false}; // true if need to recalculate trigger times
    QString            mLogFile;               // alarm output is to be logged to this URL
    float              mSoundVolume{-1.0f};    // volume for sound file (range 0 - 1), or < 0 for unspecified
    float              mFadeVolume{-1.0f};     // initial volume for sound file (range 0 - 1), or < 0 for no fade
    int                mFadeSeconds{0};        // fade time (seconds) for sound file, or 0 if none
    int                mRepeatSoundPause{-1};  // seconds to pause between sound file repetitions, or -1 if no repetition
    int                mLateCancel{0};         // how many minutes late will cancel the alarm, or 0 for no cancellation
    bool               mExcludeHolidays{false}; // don't trigger alarms on holidays
    mutable QSharedPointer<const HolidayRegion> mExcludeHolidayRegion; // holiday region used to exclude alarms on holidays (= mHolidays when trigger calculated)
    mutable int        mWorkTimeOnly{0};         // non-zero to trigger alarm only during working hours (= mWorkTimeIndex when trigger calculated)
    KAEvent::SubAction mActionSubType;           // sub-action type for the event's main alarm
    CalEvent::Type     mCategory{CalEvent::EMPTY};   // event category (active, archived, template, ...)
    KAEvent::ExtraActionOptions mExtraActionOptions; // options for pre- or post-alarm actions
    KACalendar::Compat mCompatibility{KACalendar::Current}; // event's storage format compatibility
    bool               mReadOnly{false};         // event is read-only in its original calendar file
    bool               mConfirmAck{false};       // alarm acknowledgement requires confirmation by user
    bool               mUseDefaultFont;          // use default message font, not mFont
    bool               mCommandScript{false};    // the command text is a script, not a shell command line
    bool               mCommandXterm{false};     // command alarm is to be executed in a terminal window
    bool               mCommandDisplay{false};   // command output is to be displayed in an alarm window
    bool               mCommandHideError{false}; // don't show command execution errors to user
    bool               mEmailBcc{false};         // blind copy the email to the user
    bool               mBeep{false};             // whether to beep when the alarm is displayed
    bool               mSpeak{false};            // whether to speak the message when the alarm is displayed
    bool               mCopyToKOrganizer{false}; // KOrganizer should hold a copy of the event
    bool               mReminderOnceOnly{false}; // the reminder is output only for the first recurrence
    bool               mAutoClose{false};        // whether to close the alarm window after the late-cancel period
    bool               mNotify{false};           // alarm should be shown by the notification system, not in a window
    bool               mMainExpired;             // main alarm has expired (in which case a deferral alarm will exist)
    bool               mRepeatAtLogin{false};    // whether to repeat the alarm at every login
    bool               mArchiveRepeatAtLogin{false}; // if now archived, original event was repeat-at-login
    bool               mArchive{false};          // event has triggered in the past, so archive it when closed
    bool               mDisplaying{false};       // whether the alarm is currently being displayed (i.e. in displaying calendar)
    bool               mDisplayingDefer{false};  // show Defer button (applies to displaying calendar only)
    bool               mDisplayingEdit{false};   // show Edit button (applies to displaying calendar only)
    bool               mEnabled;                 // false if event is disabled

public:
    static const QByteArray FLAGS_PROPERTY;
    static const QString DATE_ONLY_FLAG;
    static const QString LOCAL_ZONE_FLAG;
    static const QString EMAIL_BCC_FLAG;
    static const QString CONFIRM_ACK_FLAG;
    static const QString KORGANIZER_FLAG;
    static const QString EXCLUDE_HOLIDAYS_FLAG;
    static const QString WORK_TIME_ONLY_FLAG;
    static const QString REMINDER_ONCE_FLAG;
    static const QString DEFER_FLAG;
    static const QString LATE_CANCEL_FLAG;
    static const QString AUTO_CLOSE_FLAG;
    static const QString NOTIFY_FLAG;
    static const QString TEMPL_AFTER_TIME_FLAG;
    static const QString KMAIL_ITEM_FLAG;
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

QByteArray KAEvent::currentCalendarVersionString()
{
    return QByteArray("2.7.0");   // This is NOT the KAlarmCal library .so version!
}
int        KAEvent::currentCalendarVersion()
{
    return Version(2, 7, 0);   // This is NOT the KAlarmCal library .so version!
}

// Custom calendar properties.
// Note that all custom property names are prefixed with X-KDE-KALARM- in the calendar file.

// Event properties
const QByteArray KAEventPrivate::FLAGS_PROPERTY("FLAGS");              // X-KDE-KALARM-FLAGS property
const QString    KAEventPrivate::DATE_ONLY_FLAG        = QStringLiteral("DATE");
const QString    KAEventPrivate::LOCAL_ZONE_FLAG       = QStringLiteral("LOCAL");
const QString    KAEventPrivate::EMAIL_BCC_FLAG        = QStringLiteral("BCC");
const QString    KAEventPrivate::CONFIRM_ACK_FLAG      = QStringLiteral("ACKCONF");
const QString    KAEventPrivate::KORGANIZER_FLAG       = QStringLiteral("KORG");
const QString    KAEventPrivate::EXCLUDE_HOLIDAYS_FLAG = QStringLiteral("EXHOLIDAYS");
const QString    KAEventPrivate::WORK_TIME_ONLY_FLAG   = QStringLiteral("WORKTIME");
const QString    KAEventPrivate::REMINDER_ONCE_FLAG    = QStringLiteral("ONCE");
const QString    KAEventPrivate::DEFER_FLAG            = QStringLiteral("DEFER");   // default defer interval for this alarm
const QString    KAEventPrivate::LATE_CANCEL_FLAG      = QStringLiteral("LATECANCEL");
const QString    KAEventPrivate::AUTO_CLOSE_FLAG       = QStringLiteral("LATECLOSE");
const QString    KAEventPrivate::NOTIFY_FLAG           = QStringLiteral("NOTIFY");
const QString    KAEventPrivate::TEMPL_AFTER_TIME_FLAG = QStringLiteral("TMPLAFTTIME");
const QString    KAEventPrivate::KMAIL_ITEM_FLAG       = QStringLiteral("KMAIL");
const QString    KAEventPrivate::ARCHIVE_FLAG          = QStringLiteral("ARCHIVE");

const QByteArray KAEventPrivate::NEXT_RECUR_PROPERTY("NEXTRECUR");     // X-KDE-KALARM-NEXTRECUR property
const QByteArray KAEventPrivate::REPEAT_PROPERTY("REPEAT");            // X-KDE-KALARM-REPEAT property
const QByteArray KAEventPrivate::LOG_PROPERTY("LOG");                  // X-KDE-KALARM-LOG property
const QString    KAEventPrivate::xtermURL = QStringLiteral("xterm:");
const QString    KAEventPrivate::displayURL = QStringLiteral("display:");

// - General alarm properties
const QByteArray KAEventPrivate::TYPE_PROPERTY("TYPE");                // X-KDE-KALARM-TYPE property
const QString    KAEventPrivate::FILE_TYPE             = QStringLiteral("FILE");
const QString    KAEventPrivate::AT_LOGIN_TYPE         = QStringLiteral("LOGIN");
const QString    KAEventPrivate::REMINDER_TYPE         = QStringLiteral("REMINDER");
const QString    KAEventPrivate::TIME_DEFERRAL_TYPE    = QStringLiteral("DEFERRAL");
const QString    KAEventPrivate::DATE_DEFERRAL_TYPE    = QStringLiteral("DATE_DEFERRAL");
const QString    KAEventPrivate::DISPLAYING_TYPE       = QStringLiteral("DISPLAYING");   // used only in displaying calendar
const QString    KAEventPrivate::PRE_ACTION_TYPE       = QStringLiteral("PRE");
const QString    KAEventPrivate::POST_ACTION_TYPE      = QStringLiteral("POST");
const QString    KAEventPrivate::SOUND_REPEAT_TYPE     = QStringLiteral("SOUNDREPEAT");
const QByteArray KAEventPrivate::NEXT_REPEAT_PROPERTY("NEXTREPEAT");   // X-KDE-KALARM-NEXTREPEAT property
const QString    KAEventPrivate::HIDDEN_REMINDER_FLAG  = QStringLiteral("HIDE");
// - Display alarm properties
const QByteArray KAEventPrivate::FONT_COLOUR_PROPERTY("FONTCOLOR");    // X-KDE-KALARM-FONTCOLOR property
// - Email alarm properties
const QString    KAEventPrivate::EMAIL_ID_FLAG         = QStringLiteral("EMAILID");
// - Audio alarm properties
const QByteArray KAEventPrivate::VOLUME_PROPERTY("VOLUME");            // X-KDE-KALARM-VOLUME property
const QString    KAEventPrivate::SPEAK_FLAG            = QStringLiteral("SPEAK");
// - Command alarm properties
const QString    KAEventPrivate::EXEC_ON_DEFERRAL_FLAG = QStringLiteral("EXECDEFER");
const QString    KAEventPrivate::CANCEL_ON_ERROR_FLAG  = QStringLiteral("ERRCANCEL");
const QString    KAEventPrivate::DONT_SHOW_ERROR_FLAG  = QStringLiteral("ERRNOSHOW");

// Event status strings
const QString    KAEventPrivate::DISABLED_STATUS       = QStringLiteral("DISABLED");

// Displaying event ID identifier
const QString    KAEventPrivate::DISP_DEFER = QStringLiteral("DEFER");
const QString    KAEventPrivate::DISP_EDIT  = QStringLiteral("EDIT");

// Command error strings
const QString    KAEventPrivate::CMD_ERROR_VALUE      = QStringLiteral("MAIN");
const QString    KAEventPrivate::CMD_ERROR_PRE_VALUE  = QStringLiteral("PRE");
const QString    KAEventPrivate::CMD_ERROR_POST_VALUE = QStringLiteral("POST");

const QString    KAEventPrivate::SC = QStringLiteral(";");

QFont                               KAEventPrivate::mDefaultFont;
QSharedPointer<const HolidayRegion> KAEventPrivate::mHolidays;
QBitArray                           KAEventPrivate::mWorkDays(7);
QTime                               KAEventPrivate::mWorkDayStart(9, 0, 0);
QTime                               KAEventPrivate::mWorkDayEnd(17, 0, 0);
int                                 KAEventPrivate::mWorkTimeIndex = 1;

static void setProcedureAlarm(const Alarm::Ptr &, const QString &commandLine);
static QString reminderToString(int minutes);

/*=============================================================================
= Class KAEvent
= Corresponds to a KCalendarCore::Event instance.
=============================================================================*/

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

Q_GLOBAL_STATIC_WITH_ARGS(QSharedDataPointer<KAEventPrivate>,
                          emptyKAEventPrivate, (new KAEventPrivate))

KAEvent::KAEvent()
    : d(*emptyKAEventPrivate)
{ }

KAEventPrivate::KAEventPrivate()
{ }

/******************************************************************************
* Initialise the instance with the specified parameters.
*/
KAEvent::KAEvent(const KADateTime &dt, const QString &name, const QString &message,
                 const QColor &bg, const QColor &fg, const QFont &f,
                 SubAction action, int lateCancel, Flags flags, bool changesPending)
    : d(new KAEventPrivate(dt, name, message, bg, fg, f, action, lateCancel, flags, changesPending))
{
}

KAEvent::KAEvent(const KADateTime &dt, const QString &message,
                 const QColor &bg, const QColor &fg, const QFont &f,
                 SubAction action, int lateCancel, Flags flags, bool changesPending)
    : d(new KAEventPrivate(dt, QString(), message, bg, fg, f, action, lateCancel, flags, changesPending))
{
}

KAEventPrivate::KAEventPrivate(const KADateTime &dateTime, const QString &name, const QString &text,
                               const QColor &bg, const QColor &fg, const QFont &font,
                               KAEvent::SubAction action, int lateCancel, KAEvent::Flags flags,
                               bool changesPending)
    : mName(name)
    , mAlarmCount(1)
    , mBgColour(bg)
    , mFgColour(fg)
    , mFont(font)
    , mLateCancel(lateCancel)     // do this before setting flags
    , mCategory(CalEvent::ACTIVE)
{
    mStartDateTime = dateTime;
    if (flags & KAEvent::ANY_TIME)
        mStartDateTime.setDateOnly(true);
    mNextMainDateTime = mStartDateTime;
    switch (action) {
    case KAEvent::MESSAGE:
    case KAEvent::FILE:
    case KAEvent::COMMAND:
    case KAEvent::EMAIL:
    case KAEvent::AUDIO:
        mActionSubType = static_cast<KAEvent::SubAction>(action);
        break;
    default:
        mActionSubType = KAEvent::MESSAGE;
        break;
    }
    mText                   = (mActionSubType == KAEvent::COMMAND) ? text.trimmed()
                            : (mActionSubType == KAEvent::AUDIO)   ? QString() : text;
    mAudioFile              = (mActionSubType == KAEvent::AUDIO) ? text : QString();
    set_deferral((flags & DEFERRAL) ? NORMAL_DEFERRAL : NO_DEFERRAL);
    mRepeatAtLogin          = flags & KAEvent::REPEAT_AT_LOGIN;
    mConfirmAck             = flags & KAEvent::CONFIRM_ACK;
    mUseDefaultFont         = flags & KAEvent::DEFAULT_FONT;
    mCommandScript          = flags & KAEvent::SCRIPT;
    mCommandXterm           = flags & KAEvent::EXEC_IN_XTERM;
    mCommandDisplay         = flags & KAEvent::DISPLAY_COMMAND;
    mCommandHideError       = flags & KAEvent::DONT_SHOW_ERROR;
    mCopyToKOrganizer       = flags & KAEvent::COPY_KORGANIZER;
    mExcludeHolidays        = flags & KAEvent::EXCL_HOLIDAYS;
    mExcludeHolidayRegion   = holidays();
    mWorkTimeOnly           = flags & KAEvent::WORK_TIME_ONLY;
    mEmailBcc               = flags & KAEvent::EMAIL_BCC;
    mEnabled                = !(flags & KAEvent::DISABLED);
    mDisplaying             = flags & DISPLAYING_;
    mReminderOnceOnly       = flags & KAEvent::REMINDER_ONCE;
    mAutoClose              = (flags & KAEvent::AUTO_CLOSE) && mLateCancel;
    mNotify                 = flags & KAEvent::NOTIFY;
    mRepeatSoundPause       = (flags & KAEvent::REPEAT_SOUND) ? 0 : -1;
    mSpeak                  = (flags & KAEvent::SPEAK) && action != KAEvent::AUDIO;
    mBeep                   = (flags & KAEvent::BEEP) && action != KAEvent::AUDIO && !mSpeak;
    if (mRepeatAtLogin) {              // do this after setting other flags
        ++mAlarmCount;
        setRepeatAtLoginTrue(false);
    }

    mMainExpired            = false;
    mChangeCount            = changesPending ? 1 : 0;
    mTriggerChanged         = true;
}

/******************************************************************************
* Initialise the KAEvent from a KCalendarCore::Event.
*/
KAEvent::KAEvent(const KCalendarCore::Event::Ptr &event)
    : d(new KAEventPrivate(event))
{
}

KAEventPrivate::KAEventPrivate(const KCalendarCore::Event::Ptr &event)
{
    startChanges();
    // Extract status from the event
    mEventID        = event->uid();
    mRevision       = event->revision();
    mName           = event->summary();
    mBgColour       = QColor(255, 255, 255);    // missing/invalid colour - return white background
    mFgColour       = QColor(0, 0, 0);          // and black foreground
    mReadOnly       = event->isReadOnly();
    mUseDefaultFont = true;
    mEnabled        = true;
    QString param;
    bool ok;
    mCategory               = CalEvent::status(event, &param);
    if (mCategory == CalEvent::DISPLAYING) {
        // It's a displaying calendar event - set values specific to displaying alarms
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        const QStringList params = param.split(SC, QString::KeepEmptyParts);
#else
        const QStringList params = param.split(SC, Qt::KeepEmptyParts);
#endif
        int n = params.count();
        if (n) {
            const qlonglong id = params[0].toLongLong(&ok);
            if (ok) {
                mResourceId = id;    // original resource ID which contained the event
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
    bool localZone = false;
    QStringList flags = event->customProperty(KACalendar::APPNAME, FLAGS_PROPERTY).split(SC, Qt::SkipEmptyParts);
    flags << QString() << QString();    // to avoid having to check for end of list
    for (int i = 0, end = flags.count() - 1;  i < end;  ++i) {
        QString flag = flags.at(i);
        if (flag == DATE_ONLY_FLAG) {
            dateOnly = true;
        } else if (flag == LOCAL_ZONE_FLAG) {
            localZone = true;
        } else if (flag == CONFIRM_ACK_FLAG) {
            mConfirmAck = true;
        } else if (flag == EMAIL_BCC_FLAG) {
            mEmailBcc = true;
        } else if (flag == KORGANIZER_FLAG) {
            mCopyToKOrganizer = true;
        } else if (flag == EXCLUDE_HOLIDAYS_FLAG) {
            mExcludeHolidays      = true;
            mExcludeHolidayRegion = holidays();
        } else if (flag == WORK_TIME_ONLY_FLAG) {
            mWorkTimeOnly = 1;
        } else if (flag == NOTIFY_FLAG) {
            mNotify = true;
        } else if (flag == KMAIL_ITEM_FLAG) {
            const Akonadi::Item::Id id = flags.at(i + 1).toLongLong(&ok);
            if (!ok) {
                continue;
            }
            mAkonadiItemId = id;
            ++i;
        } else if (flag == KAEventPrivate::ARCHIVE_FLAG) {
            mArchive = true;
        } else if (flag == KAEventPrivate::AT_LOGIN_TYPE) {
            mArchiveRepeatAtLogin = true;
        } else if (flag == KAEventPrivate::REMINDER_TYPE) {
            flag = flags.at(++i);
            if (flag == KAEventPrivate::REMINDER_ONCE_FLAG) {
                mReminderOnceOnly = true;
                flag = flags.at(++i);
            }
            const int len = flag.length() - 1;
            mReminderMinutes = -flag.leftRef(len).toInt();    // -> 0 if conversion fails
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
                mins.chop(1);
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
        // This property is used only when the main alarm has expired.
        // If a main alarm is found, this property is ignored (see below).
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
    mNextMainDateTime = readDateTime(event, localZone, dateOnly, mStartDateTime);
    mCreatedDateTime = KADateTime(event->created());
    if (dateOnly  &&  !mRepetition.isDaily()) {
        mRepetition.set(Duration(mRepetition.intervalDays(), Duration::Days));
    }
    if (event->customStatus() == DISABLED_STATUS) {
        mEnabled = false;
    }

    // Extract status from the event's alarms.
    // First set up defaults.
    mActionSubType = KAEvent::MESSAGE;
    mMainExpired   = true;

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
        const DateTime dateTime(data.alarm->hasStartOffset() ? data.alarm->startOffset().end(mNextMainDateTime.effectiveDateTime()) : data.alarm->time());
        switch (data.type) {
        case MAIN_ALARM:
            mMainExpired = false;
            alTime = dateTime;
            alTime.setDateOnly(mStartDateTime.isDateOnly());
            mRepetition.set(0, 0);   // ignore X-KDE-KALARM-REPEAT if main alarm exists
            if (data.alarm->repeatCount()  &&  !data.alarm->snoozeTime().isNull()) {
                mRepetition.set(data.alarm->snoozeTime(), data.alarm->repeatCount());   // values may be adjusted in setRecurrence()
                mNextRepeat = data.nextRepeat;
            }
            if (data.action != KAAlarm::AUDIO) {
                break;
            }
            // Fall through to AUDIO_ALARM
            Q_FALLTHROUGH();
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
            if (data.timedDeferral) {
                // Don't use start-of-day time for applying timed deferral alarm offset
                mDeferralTime = DateTime(data.alarm->hasStartOffset() ? data.alarm->startOffset().end(mNextMainDateTime.calendarDateTime()) : data.alarm->time());
            } else {
                mDeferralTime = dateTime;
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
            Q_FALLTHROUGH();
        case REMINDER_ALARM:
        case AT_LOGIN_ALARM:
        case DISPLAYING_ALARM:
            if (!set  &&  !noSetNextTime) {
                mNextMainDateTime = alTime;
            }
            // fall through to MAIN_ALARM
            Q_FALLTHROUGH();
        case MAIN_ALARM:
            // Ensure that the basic fields are set up even if there is no main
            // alarm in the event (if it has expired and then been deferred)
            if (!set) {
                mActionSubType = static_cast<KAEvent::SubAction>(data.action);
                mText = (mActionSubType == KAEvent::COMMAND) ? data.cleanText.trimmed() : data.cleanText;
                switch (data.action) {
                case KAAlarm::COMMAND:
                    mCommandScript = data.commandScript;
                    if (data.extraActionOptions & KAEvent::DontShowPreActError) {
                        mCommandHideError = true;
                    }
                    if (!mCommandDisplay) {
                        break;
                    }
                    // fall through to MESSAGE
                    Q_FALLTHROUGH();
                case KAAlarm::MESSAGE:
                    mFont           = data.font;
                    mUseDefaultFont = data.defaultFont;
                    if (data.isEmailText) {
                        isEmailText = true;
                    }
                    // fall through to FILE
                    Q_FALLTHROUGH();
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
        mAkonadiItemId = -1;
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
            setRecur(RecurrenceRule::rDaily, mRepetition.intervalDays(), mRepetition.count() + 1, QDate());
        } else {
            setRecur(RecurrenceRule::rMinutely, mRepetition.intervalMinutes(), mRepetition.count() + 1, KADateTime());
        }
        mRepetition.set(0, 0);
        mTriggerChanged = true;
    }

    if (mRepeatAtLogin) {
        mArchiveRepeatAtLogin = false;
        if (mReminderMinutes > 0) {
            mReminderMinutes = 0;      // pre-alarm reminder not allowed for at-login alarm
            mReminderActive  = NO_REMINDER;
        }
        setRepeatAtLoginTrue(false);   // clear other incompatible statuses
    }

    if (mMainExpired  &&  !deferralOffset.isNull()  &&  checkRecur() != KARecurrence::NO_RECUR) {
        // Adjust the deferral time for an expired recurrence, since the
        // offset is relative to the first actual occurrence.
        DateTime dt = mRecurrence->getNextDateTime(mStartDateTime.addDays(-1).kDateTime());
        dt.setDateOnly(mStartDateTime.isDateOnly());
        if (mDeferralTime.isDateOnly()) {
            mDeferralTime = DateTime(deferralOffset.end(dt.qDateTime()));
            mDeferralTime.setDateOnly(true);
        } else {
            mDeferralTime = DateTime(deferralOffset.end(dt.effectiveDateTime()));
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

KAEventPrivate::KAEventPrivate(const KAEventPrivate &other)
    : QSharedData(other)
    , mRecurrence(nullptr)
{
    copy(other);
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
    mCustomProperties        = event.mCustomProperties;
    mItemId                  = event.mItemId;
    mResourceId              = event.mResourceId;
    mName                    = event.mName;
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
    mAkonadiItemId           = event.mAkonadiItemId;
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
    mExcludeHolidayRegion    = event.mExcludeHolidayRegion;
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
    mCommandHideError        = event.mCommandHideError;
    mEmailBcc                = event.mEmailBcc;
    mBeep                    = event.mBeep;
    mSpeak                   = event.mSpeak;
    mCopyToKOrganizer        = event.mCopyToKOrganizer;
    mReminderOnceOnly        = event.mReminderOnceOnly;
    mAutoClose               = event.mAutoClose;
    mNotify                  = event.mNotify;
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
        mRecurrence = nullptr;
    }
}

// Deprecated
void KAEvent::set(const KCalendarCore::Event::Ptr &e)
{
    *this = KAEvent(e);
}

// Deprecated
void KAEvent::set(const KADateTime &dt, const QString &message, const QColor &bg, const QColor &fg,
                  const QFont &f, SubAction act, int lateCancel, Flags flags, bool changesPending)
{
    *this = KAEvent(dt, QString(), message, bg, fg, f, act, lateCancel, flags, changesPending);
}

/******************************************************************************
* Update an existing KCalendarCore::Event with the KAEventPrivate data.
* If 'setCustomProperties' is true, all the KCalendarCore::Event's existing
* custom properties are cleared and replaced with the KAEvent's custom
* properties. If false, the KCalendarCore::Event's non-KAlarm custom properties
* are left untouched.
*/
bool KAEvent::updateKCalEvent(const KCalendarCore::Event::Ptr &e, UidAction u, bool setCustomProperties) const
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
    ev->setSummary(mName);

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
        param = QString::number(mResourceId);   // original resource ID which contained the event
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
    if (mStartDateTime.timeType() == KADateTime::LocalZone) {
        flags += LOCAL_ZONE_FLAG;
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
    if (mNotify) {
        flags += NOTIFY_FLAG;
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
    if (mCategory == CalEvent::TEMPLATE  &&  mTemplateAfterTime >= 0) {
        (flags += TEMPL_AFTER_TIME_FLAG) += QString::number(mTemplateAfterTime);
    }
    if (mAkonadiItemId >= 0) {
        (flags += KMAIL_ITEM_FLAG) += QString::number(mAkonadiItemId);
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
    ev->setDtStart(mStartDateTime.calendarDateTime());
    ev->setAllDay(false);
    ev->setDtEnd(QDateTime());

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
            QDateTime dt = mNextMainDateTime.kDateTime().toTimeSpec(mStartDateTime.timeSpec()).qDateTime();
            ev->setCustomProperty(KACalendar::APPNAME, NEXT_RECUR_PROPERTY,
                                  dt.toString(mNextMainDateTime.isDateOnly() ? QStringLiteral("yyyyMMdd") : QStringLiteral("yyyyMMddThhmmss")));
        }
        // Add the main alarm
        initKCalAlarm(ev, 0, QStringList(), MAIN_ALARM);
        ancillaryOffset = 0;
        ancillaryType = dtMain.isValid() ? 2 : 0;
    } else if (mRepetition) {
        // Alarm repetition is normally held in the main alarm, but since
        // the main alarm has expired, store in a custom property.
        const QString param = QStringLiteral("%1:%2").arg(mRepetition.intervalMinutes()).arg(mRepetition.count());
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
            dtl = DateTime(KADateTime::currentLocalDate().addDays(-1), mStartDateTime.timeSpec());
        } else {
            dtl = KADateTime::currentUtcDateTime();
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
            KADateTime dt = mRecurrence->getNextDateTime(mStartDateTime.addDays(-1).kDateTime());
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
    if (mDisplaying  &&  mCategory != CalEvent::TEMPLATE) {
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
        ev->setCreated(mCreatedDateTime.qDateTime());
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
Alarm::Ptr KAEventPrivate::initKCalAlarm(const KCalendarCore::Event::Ptr &event, const DateTime &dt, const QStringList &types, AlarmType type) const
{
    const int startOffset = dt.isDateOnly() ? mStartDateTime.secsTo(dt)
                            : mStartDateTime.calendarKDateTime().secsTo(dt.calendarKDateTime());
    return initKCalAlarm(event, startOffset, types, type);
}

Alarm::Ptr KAEventPrivate::initKCalAlarm(const KCalendarCore::Event::Ptr &event, int startOffsetSecs, const QStringList &types, AlarmType type) const
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
        Q_FALLTHROUGH();
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
            Q_FALLTHROUGH();
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
            if (mCommandHideError) {
                flags += DONT_SHOW_ERROR_FLAG;
            }
            break;
        case KAEvent::EMAIL:
            alarm->setEmailAlarm(mEmailSubject, mText, mEmailAddresses, mEmailAttachments);
            if (mEmailFromIdentity) {
                flags << KAEventPrivate::EMAIL_ID_FLAG << QString::number(mEmailFromIdentity);
            }
            break;
        case KAEvent::AUDIO:
            setAudioAlarm(alarm);
            if (mRepeatSoundPause >= 0  &&  type == MAIN_ALARM) {
                // Indicate repeating sound in the main alarm by a non-standard
                // method, since it might have a sub-repetition too.
                alltypes << SOUND_REPEAT_TYPE << QString::number(mRepeatSoundPause);
            }
            break;
        }
        if (display  &&  !mNotify)
            alarm->setCustomProperty(KACalendar::APPNAME, FONT_COLOUR_PROPERTY,
                                     QStringLiteral("%1;%2;%3").arg(mBgColour.name(), mFgColour.name(), mUseDefaultFont ? QString() : mFont.toString()));
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
        alarm->setCustomProperty(KACalendar::APPNAME, TYPE_PROPERTY, alltypes.join(QLatin1Char(',')));
    }
    if (!flags.isEmpty()) {
        alarm->setCustomProperty(KACalendar::APPNAME, FLAGS_PROPERTY, flags.join(SC));
    }
    return alarm;
}

/******************************************************************************
* Find the index to the last daylight savings time transition at or before a
* given UTC time.
* Returns index, or -1 if before the first transition.
*/
int KAEventPrivate::transitionIndex(const QDateTime &utc, const QTimeZone::OffsetDataList& transitions)
{
    if (utc.timeSpec() != Qt::UTC  ||  transitions.isEmpty())
        return -1;
    int start = 0;
    int end = transitions.size() - 1;
    while (start != end) {
        int i = (start + end + 1) / 2;
        if (transitions[i].atUtc == utc)
            return i;
        if (transitions[i].atUtc > utc) {
            end = i - 1;
            if (end < 0)
                return -1;
        }
        else
            start = i;
    }
    return start;
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
    KAEvent::Flags result{};
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
    if (mCommandHideError) {
        result |= KAEvent::DONT_SHOW_ERROR;
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
    if (mNotify) {
        result |= KAEvent::NOTIFY;
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

void KAEvent::setResourceId(ResourceId id)
{
    d->mResourceId = id;
}

void KAEvent::setResourceId_const(ResourceId id) const
{
    d->mResourceId = id;
}

ResourceId KAEvent::resourceId() const
{
    // A displaying alarm contains the event's original resource ID
    return d->mDisplaying ? -1 : d->mResourceId;
}

void KAEvent::setCollectionId(Akonadi::Collection::Id id)
{
    setResourceId(id);
}

void KAEvent::setCollectionId_const(Akonadi::Collection::Id id) const
{
    setResourceId_const(id);
}

Akonadi::Collection::Id KAEvent::collectionId() const
{
    // A displaying alarm contains the event's original collection ID
    return d->mDisplaying ? -1 : d->mResourceId;
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
    return KAlarmCal::setItemPayload(item, *this, collectionMimeTypes);
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

void KAEvent::setNotify(bool useNotify)
{
    d->mNotify = useNotify;
}

bool KAEvent::notify() const
{
    return d->mNotify;
}

void KAEvent::setAkonadiItemId(Akonadi::Item::Id id)
{
    d->mAkonadiItemId = id;
}

Akonadi::Item::Id KAEvent::akonadiItemId() const
{
    return d->mAkonadiItemId;
}

QString KAEvent::name() const
{
    return d->mName;
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

bool KAEvent::commandHideError() const
{
    return d->mCommandHideError;
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

void KAEvent::setEmail(uint from, const KCalendarCore::Person::List &addresses, const QString &subject,
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

KCalendarCore::Person::List KAEvent::emailAddressees() const
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

QString KAEvent::joinEmailAddresses(const KCalendarCore::Person::List &addresses, const QString &separator)
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
            || (d->mActionSubType == COMMAND && d->mCommandDisplay))
       &&  d->mSpeak;
}

/******************************************************************************
* Set the event to be an alarm template.
*/
void KAEvent::setTemplate(const QString &name, int afterTime)
{
    d->setCategory(CalEvent::TEMPLATE);
    d->mName = name;
    d->mTemplateAfterTime = afterTime;
    d->mTriggerChanged = true;   // templates and archived don't have trigger times
}

bool KAEvent::isTemplate() const
{
    return d->mCategory == CalEvent::TEMPLATE;
}

QString KAEvent::templateName() const
{
    return d->mName;
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

    qCDebug(KALARMCAL_LOG) << "Setting reminder at" << reminderTime.effectiveKDateTime().toString(QStringLiteral("%Y-%m-%d %H:%M"));
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
    d->defer(dt, reminder, adjustRecurrence);
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
            const KADateTime now = KADateTime::currentUtcDateTime();
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
    return d->mDeferral != KAEventPrivate::NO_DEFERRAL;
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
        const KADateTime now = KADateTime::currentUtcDateTime();
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
        if (KADateTime::currentUtcDateTime() < mNextMainDateTime.effectiveKDateTime()) {
            endTime = mNextMainDateTime;
            ltype = KAEvent::LIMIT_MAIN;
        }
    } else if (mReminderMinutes > 0
           &&  KADateTime::currentUtcDateTime() < mNextMainDateTime.effectiveKDateTime()) {
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

void KAEvent::setTime(const KADateTime &dt)
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
#pragma message("Does this need all trigger times for date-only alarms to be recalculated?")
}

/******************************************************************************
* Called when the user changes the start-of-day time.
* Adjust the start time of the recurrence to match, for each date-only event in
* a list.
*/
void KAEvent::adjustStartOfDay(const KAEvent::List &events)
{
    for (KAEvent *event : events) {
        KAEventPrivate *const p = event->d;
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

void KAEvent::setCreatedDateTime(const KADateTime &dt)
{
    d->mCreatedDateTime = dt;
}

KADateTime KAEvent::createdDateTime() const
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
    d->mExcludeHolidays      = ex;
    d->mExcludeHolidayRegion = KAEventPrivate::holidays();
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
    KAEventPrivate::mHolidays.reset(new HolidayRegion(h.regionCode()));
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
bool KAEvent::isWorkingTime(const KADateTime &dt) const
{
    return d->isWorkingTime(dt);
}

bool KAEventPrivate::isWorkingTime(const KADateTime &dt) const
{
    if ((mWorkTimeOnly  &&  !mWorkDays.testBit(dt.date().dayOfWeek() - 1))
    ||  (mExcludeHolidays  &&  holidays()->isHoliday(dt.date()))) {
        return false;
    }
    if (!mWorkTimeOnly) {
        return true;
    }
    return dt.isDateOnly()
        || (dt.time() >= mWorkDayStart  &&  dt.time() < mWorkDayEnd);
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
        mRecurrence = nullptr;
        mRepetition.set(0, 0);
        mTriggerChanged = true;
    }
    mNextRepeat = 0;
}

/******************************************************************************
* Initialise the event's recurrence from a KCalendarCore::Recurrence.
* The event's start date/time is not changed.
*/
void KAEvent::setRecurrence(const KARecurrence &recurrence)
{
    d->setRecurrence(recurrence);
}

void KAEventPrivate::setRecurrence(const KARecurrence &recurrence)
{
    startChanges();   // prevent multiple trigger time evaluation here
    if (recurrence.recurs()) {
        delete mRecurrence;
        mRecurrence = new KARecurrence(recurrence);
        mRecurrence->setStartDateTime(mStartDateTime.effectiveKDateTime(), mStartDateTime.isDateOnly());
        mTriggerChanged = true;

        // Adjust sub-repetition values to fit the recurrence.
        setRepetition(mRepetition);
    } else {
        clearRecur();
    }

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
bool KAEvent::setRecurMinutely(int freq, int count, const KADateTime &end)
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
        if (days.size() != 7) {
            qCWarning(KALARMCAL_LOG) << "KAEvent::setRecurDaily: Error! 'days' parameter must have 7 elements: actual size" << days.size();
        } else {
            int n = days.count(true);   // number of days when alarms occur
            if (n < 7) {
                d->mRecurrence->addWeeklyDays(days);
            }
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
        for (int day : days) {
            d->mRecurrence->addMonthlyDate(day);
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
        for (const MonthPos& posn : posns) {
            d->mRecurrence->addMonthlyPos(posn.weeknum, posn.days);
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
        for (int month : months) {
            d->mRecurrence->addYearlyMonth(month);
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
        for (int month : months) {
            d->mRecurrence->addYearlyMonth(month);
        }
        for (const MonthPos& posn : posns) {
            d->mRecurrence->addYearlyPos(posn.weeknum, posn.days);
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
bool KAEventPrivate::setRecur(KCalendarCore::RecurrenceRule::PeriodType recurType, int freq, int count, const QDate &end, KARecurrence::Feb29Type feb29)
{
    KADateTime edt = mNextMainDateTime.kDateTime();
    edt.setDate(end);
    return setRecur(recurType, freq, count, edt, feb29);
}
bool KAEventPrivate::setRecur(KCalendarCore::RecurrenceRule::PeriodType recurType, int freq, int count, const KADateTime &end, KARecurrence::Feb29Type feb29)
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
    const KADateTime recurStart = mRecurrence->startDateTime();
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
                return i18nc("@info Hours and minutes", "%1h %2m", frequency / 60, QString::asprintf("%02d", frequency % 60));
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
            return i18nc("@info Hours and minutes", "%1h %2m", minutes / 60, QString::asprintf("%02d", minutes % 60));
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
bool KAEvent::occursAfter(const KADateTime &preDateTime, bool includeRepetitions) const
{
    return d->occursAfter(preDateTime, includeRepetitions);
}

bool KAEventPrivate::occursAfter(const KADateTime &preDateTime, bool includeRepetitions) const
{
    KADateTime dt;
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
        if (preDateTime < KADateTime(mRepetition.duration().end(dt.qDateTime()))) {
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
KAEvent::OccurType KAEvent::setNextOccurrence(const KADateTime &preDateTime)
{
    return d->setNextOccurrence(preDateTime);
}

KAEvent::OccurType KAEventPrivate::setNextOccurrence(const KADateTime &preDateTime)
{
    if (preDateTime < mNextMainDateTime.effectiveKDateTime()) {
        return KAEvent::FIRST_OR_ONLY_OCCURRENCE;    // it might not be the first recurrence - tant pis
    }
    KADateTime pre = preDateTime;
    // If there are repetitions, adjust the comparison date/time so that
    // we find the earliest recurrence which has a repetition falling after
    // the specified preDateTime.
    if (mRepetition) {
        pre = KADateTime(mRepetition.duration(-mRepetition.count()).end(preDateTime.qDateTime()));
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
KAEvent::OccurType KAEvent::nextOccurrence(const KADateTime &preDateTime, DateTime &result, OccurOption o) const
{
    return d->nextOccurrence(preDateTime, result, o);
}

KAEvent::OccurType KAEventPrivate::nextOccurrence(const KADateTime &preDateTime, DateTime &result,
        KAEvent::OccurOption includeRepetitions) const
{
    KADateTime pre = preDateTime;
    if (includeRepetitions != KAEvent::IGNORE_REPETITION) {
        // RETURN_REPETITION or ALLOW_FOR_REPETITION
        if (!mRepetition) {
            includeRepetitions = KAEvent::IGNORE_REPETITION;
        } else {
            pre = KADateTime(mRepetition.duration(-mRepetition.count()).end(preDateTime.qDateTime()));
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
        const DateTime repeatDT(mRepetition.duration(repetition).end(result.qDateTime()));
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
                    result = DateTime(mRepetition.duration(repetition).end(result.qDateTime()));
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
KAEvent::OccurType KAEvent::previousOccurrence(const KADateTime &afterDateTime, DateTime &result, bool includeRepetitions) const
{
    return d->previousOccurrence(afterDateTime, result, includeRepetitions);
}

KAEvent::OccurType KAEventPrivate::previousOccurrence(const KADateTime &afterDateTime, DateTime &result,
        bool includeRepetitions) const
{
    Q_ASSERT(!afterDateTime.isDateOnly());
    if (mStartDateTime >= afterDateTime) {
        result = KADateTime();
        return KAEvent::NO_OCCURRENCE;     // the event starts after the specified date/time
    }

    // Find the latest recurrence of the event
    KAEvent::OccurType type;
    if (checkRecur() == KARecurrence::NO_RECUR) {
        result = mStartDateTime;
        type = KAEvent::FIRST_OR_ONLY_OCCURRENCE;
    } else {
        const KADateTime recurStart = mRecurrence->startDateTime();
        KADateTime after = afterDateTime.toTimeSpec(mStartDateTime.timeSpec());
        if (mStartDateTime.isDateOnly()  &&  afterDateTime.time() > DateTime::startOfDay()) {
            after = after.addDays(1);    // today's recurrence (if today recurs) has passed
        }
        const KADateTime dt = mRecurrence->getPreviousDateTime(after);
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
            result = DateTime(mRepetition.duration(qMin(repetition, mRepetition.count())).end(result.qDateTime()));
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
bool KAEvent::setDisplaying(const KAEvent &e, KAAlarm::Type t, ResourceId id, const KADateTime &dt, bool showEdit, bool showDefer)
{
    return d->setDisplaying(*e.d, t, id, dt, showEdit, showDefer);
}

bool KAEventPrivate::setDisplaying(const KAEventPrivate &event, KAAlarm::Type alarmType, ResourceId resourceId,
                                   const KADateTime &repeatAtLoginTime, bool showEdit, bool showDefer)
{
    if (!mDisplaying
    &&  (alarmType == KAAlarm::MAIN_ALARM
         || alarmType == KAAlarm::REMINDER_ALARM
         || alarmType == KAAlarm::DEFERRED_REMINDER_ALARM
         || alarmType == KAAlarm::DEFERRED_ALARM
         || alarmType == KAAlarm::AT_LOGIN_ALARM)) {
//qCDebug(KALARMCAL_LOG)<<event.id()<<","<<(alarmType==KAAlarm::MAIN_ALARM?"MAIN":alarmType==KAAlarm::REMINDER_ALARM?"REMINDER":alarmType==KAAlarm::DEFERRED_REMINDER_ALARM?"REMINDER_DEFERRAL":alarmType==KAAlarm::DEFERRED_ALARM?"DEFERRAL":"LOGIN")<<"): time="<<repeatAtLoginTime.toString();
        KAAlarm al = event.alarm(alarmType);
        if (al.isValid()) {
            *this = event;
            // Change the event ID to avoid duplicating the same unique ID as the original event
            setCategory(CalEvent::DISPLAYING);
            mItemId             = -1;    // the display event doesn't have an associated Item
            mResourceId         = resourceId;  // original resource ID which contained the event
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
void KAEvent::reinstateFromDisplaying(const KCalendarCore::Event::Ptr &e, ResourceId &id, bool &showEdit, bool &showDefer)
{
    d->reinstateFromDisplaying(e, id, showEdit, showDefer);
}

void KAEventPrivate::reinstateFromDisplaying(const Event::Ptr &kcalEvent, ResourceId &resourceId, bool &showEdit, bool &showDefer)
{
    *this = KAEventPrivate(kcalEvent);
    if (mDisplaying) {
        // Retrieve the original event's unique ID
        setCategory(CalEvent::ACTIVE);
        resourceId   = mResourceId;
        mResourceId  = -1;
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
        al_d->mActionType    = static_cast<KAAlarm::Action>(mActionSubType);
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
            Q_FALLTHROUGH();
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
        Q_FALLTHROUGH();
    case KAAlarm::REMINDER_ALARM:
        // There can only be one deferral alarm
        if (mDeferral == REMINDER_DEFERRAL) {
            return alarm(KAAlarm::DEFERRED_REMINDER_ALARM);
        }
        if (mDeferral == NORMAL_DEFERRAL) {
            return alarm(KAAlarm::DEFERRED_ALARM);
        }
        // fall through to DEFERRED_ALARM
        Q_FALLTHROUGH();
    case KAAlarm::DEFERRED_REMINDER_ALARM:
    case KAAlarm::DEFERRED_ALARM:
        if (mRepeatAtLogin) {
            return alarm(KAAlarm::AT_LOGIN_ALARM);
        }
        // fall through to AT_LOGIN_ALARM
        Q_FALLTHROUGH();
    case KAAlarm::AT_LOGIN_ALARM:
        if (mDisplaying) {
            return alarm(KAAlarm::DISPLAYING_ALARM);
        }
        // fall through to DISPLAYING_ALARM
        Q_FALLTHROUGH();
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
        Q_FALLTHROUGH();
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

/******************************************************************************
* Compare this instance with another.
*/
bool KAEvent::compare(const KAEvent& other, Comparison comparison) const
{
    return d->compare(*other.d, comparison);
}
bool KAEventPrivate::compare(const KAEventPrivate& other, KAEvent::Comparison comparison) const
{
    if (comparison & KAEvent::Compare::Id) {
        if (mEventID != other.mEventID) {
            return false;
        }
    }
    if (mCategory         != other.mCategory
    ||  mActionSubType    != other.mActionSubType
    ||  mDisplaying       != other.mDisplaying
    ||  mName             != other.mName
    ||  mText             != other.mText
    ||  mStartDateTime    != other.mStartDateTime
    ||  mLateCancel       != other.mLateCancel
    ||  mCopyToKOrganizer != other.mCopyToKOrganizer
    ||  mCompatibility    != other.mCompatibility
    ||  mEnabled          != other.mEnabled
    ||  mReadOnly         != other.mReadOnly) {
        return false;
    }
    if (mRecurrence) {
        if (!other.mRecurrence
        ||  *mRecurrence     != *other.mRecurrence
        ||  mExcludeHolidays != other.mExcludeHolidays
        ||  mWorkTimeOnly    != other.mWorkTimeOnly
        ||  mRepetition      != mRepetition) {
            return false;
        }
    } else {
        if (other.mRecurrence
        ||  mRepeatAtLogin        != other.mRepeatAtLogin
        ||  mArchiveRepeatAtLogin != other.mArchiveRepeatAtLogin
        ||  (mRepeatAtLogin  &&  mAtLoginDateTime != other.mAtLoginDateTime)) {
            return false;
        }
    }
    if (mDisplaying) {
        if (mDisplayingTime  !=  other.mDisplayingTime
        ||  mDisplayingFlags !=  other.mDisplayingFlags
        ||  mDisplayingDefer !=  other.mDisplayingDefer
        ||  mDisplayingEdit  !=  other.mDisplayingEdit) {
            return false;
        }
    }
    if (comparison & KAEvent::Compare::ICalendar) {
        if (mCreatedDateTime  != other.mCreatedDateTime
        ||  mCustomProperties != other.mCustomProperties
        ||  mRevision         != other.mRevision) {
            return false;
        }
    }
    if (comparison & KAEvent::Compare::UserSettable)
    {
        if (mItemId     != other.mItemId
        ||  mResourceId != other.mResourceId) {
            return false;
        }
    }
    if (comparison & KAEvent::Compare::CurrentState) {
        if (mNextMainDateTime != other.mNextMainDateTime
        ||  mMainExpired      != other.mMainExpired
        ||  (mRepetition  &&  mNextRepeat != other.mNextRepeat)) {
            return false;
        }
    }
    switch (mCategory) {
        case CalEvent::ACTIVE:
            if (mArchive != other.mArchive) {
                return false;
            }
            break;
        case CalEvent::TEMPLATE:
            if (mTemplateAfterTime != other.mTemplateAfterTime) {
                return false;
            }
            break;
        default:
            break;
    }

    switch (mActionSubType) {
        case KAEvent::COMMAND:
            if (mCommandScript    != other.mCommandScript
            ||  mCommandXterm     != other.mCommandXterm
            ||  mCommandDisplay   != other.mCommandDisplay
            ||  mCommandError     != other.mCommandError
            ||  mCommandHideError != other.mCommandHideError
            ||  mLogFile          != other.mLogFile)
                return false;
            if (!mCommandDisplay) {
                break;
            }
            Q_FALLTHROUGH(); // fall through to MESSAGE
        case KAEvent::FILE:
        case KAEvent::MESSAGE:
            if (mReminderMinutes      != other.mReminderMinutes
            ||  mBgColour             != other.mBgColour
            ||  mFgColour             != other.mFgColour
            ||  mUseDefaultFont       != other.mUseDefaultFont
            ||  (!mUseDefaultFont  &&  mFont != other.mFont)
            ||  mLateCancel           != other.mLateCancel
            ||  (mLateCancel  &&  mAutoClose != other.mAutoClose)
            ||  mDeferDefaultMinutes  != other.mDeferDefaultMinutes
            ||  (mDeferDefaultMinutes  &&  mDeferDefaultDateOnly != other.mDeferDefaultDateOnly)
            ||  mPreAction            != other.mPreAction
            ||  mPostAction           != other.mPostAction
            ||  mExtraActionOptions   != other.mExtraActionOptions
            ||  mCommandError         != other.mCommandError
            ||  mConfirmAck           != other.mConfirmAck
            ||  mNotify               != other.mNotify
            ||  mAkonadiItemId        != other.mAkonadiItemId
            ||  mBeep                 != other.mBeep
            ||  mSpeak                != other.mSpeak
            ||  mAudioFile            != other.mAudioFile) {
                return false;
            }
            if (mReminderMinutes) {
                if (mReminderOnceOnly != other.mReminderOnceOnly) {
                    return false;
                }
                if (comparison & KAEvent::Compare::CurrentState) {
                    if (mReminderActive != other.mReminderActive
                    ||  (mReminderActive  &&  mReminderAfterTime != other.mReminderAfterTime)) {
                        return false;
                    }
                }
            }
            if (comparison & KAEvent::Compare::CurrentState) {
                if (mDeferral != other.mDeferral
                ||  (mDeferral != NO_DEFERRAL  &&  mDeferralTime != other.mDeferralTime)) {
                    return false;
                }
            }
            if (mAudioFile.isEmpty()) {
                break;
            }
            Q_FALLTHROUGH(); // fall through to AUDIO
        case KAEvent::AUDIO:
            if (mRepeatSoundPause != other.mRepeatSoundPause) {
                return false;
            }
            if (mSoundVolume >= 0) {
                if (mSoundVolume != other.mSoundVolume) {
                    return false;
                }
                if (mFadeVolume >= 0) {
                    if (mFadeVolume  != other.mFadeVolume
                    ||  mFadeSeconds != other.mFadeSeconds) {
                        return false;
                    }
                } else if (other.mFadeVolume >= 0) {
                    return false;
                }
            } else if (other.mSoundVolume >= 0) {
                return false;
            }
            break;
        case KAEvent::EMAIL:
            if (mEmailFromIdentity != other.mEmailFromIdentity
            ||  mEmailAddresses    != other.mEmailAddresses
            ||  mEmailSubject      != other.mEmailSubject
            ||  mEmailAttachments  != other.mEmailAttachments
            ||  mEmailBcc          != other.mEmailBcc) {
                return false;
            }
            break;
    }
    return true;
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
    const int count = objList.count();
    ptrs.reserve(count);
    for (int i = 0; i < count; ++i) {
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
    qCDebug(KALARMCAL_LOG) << "KAEvent dump:";
    qCDebug(KALARMCAL_LOG) << "-- mEventID:" << mEventID;
    qCDebug(KALARMCAL_LOG) << "-- mActionSubType:" << (mActionSubType == KAEvent::MESSAGE ? "MESSAGE" : mActionSubType == KAEvent::FILE ? "FILE" : mActionSubType == KAEvent::COMMAND ? "COMMAND" : mActionSubType == KAEvent::EMAIL ? "EMAIL" : mActionSubType == KAEvent::AUDIO ? "AUDIO" : "??");
    qCDebug(KALARMCAL_LOG) << "-- mNextMainDateTime:" << mNextMainDateTime.toString();
    qCDebug(KALARMCAL_LOG) << "-- mCommandError:" << mCommandError;
    qCDebug(KALARMCAL_LOG) << "-- mAllTrigger:" << mAllTrigger.toString();
    qCDebug(KALARMCAL_LOG) << "-- mMainTrigger:" << mMainTrigger.toString();
    qCDebug(KALARMCAL_LOG) << "-- mAllWorkTrigger:" << mAllWorkTrigger.toString();
    qCDebug(KALARMCAL_LOG) << "-- mMainWorkTrigger:" << mMainWorkTrigger.toString();
    qCDebug(KALARMCAL_LOG) << "-- mCategory:" << mCategory;
    qCDebug(KALARMCAL_LOG) << "-- mName:" << mName;
    if (mCategory == CalEvent::TEMPLATE) {
        qCDebug(KALARMCAL_LOG) << "-- mTemplateAfterTime:" << mTemplateAfterTime;
    }
    qCDebug(KALARMCAL_LOG) << "-- mText:" << mText;
    if (mActionSubType == KAEvent::MESSAGE
    ||  mActionSubType == KAEvent::FILE
    ||  (mActionSubType == KAEvent::COMMAND && mCommandDisplay)) {
        if (mCommandDisplay) {
            qCDebug(KALARMCAL_LOG) << "-- mCommandScript:" << mCommandScript;
        }
        qCDebug(KALARMCAL_LOG) << "-- mBgColour:" << mBgColour.name();
        qCDebug(KALARMCAL_LOG) << "-- mFgColour:" << mFgColour.name();
        qCDebug(KALARMCAL_LOG) << "-- mUseDefaultFont:" << mUseDefaultFont;
        if (!mUseDefaultFont) {
            qCDebug(KALARMCAL_LOG) << "-- mFont:" << mFont.toString();
        }
        qCDebug(KALARMCAL_LOG) << "-- mSpeak:" << mSpeak;
        qCDebug(KALARMCAL_LOG) << "-- mAudioFile:" << mAudioFile;
        qCDebug(KALARMCAL_LOG) << "-- mPreAction:" << mPreAction;
        qCDebug(KALARMCAL_LOG) << "-- mExecPreActOnDeferral:" << (mExtraActionOptions & KAEvent::ExecPreActOnDeferral);
        qCDebug(KALARMCAL_LOG) << "-- mCancelOnPreActErr:" << (mExtraActionOptions & KAEvent::CancelOnPreActError);
        qCDebug(KALARMCAL_LOG) << "-- mDontShowPreActErr:" << (mExtraActionOptions & KAEvent::DontShowPreActError);
        qCDebug(KALARMCAL_LOG) << "-- mPostAction:" << mPostAction;
        qCDebug(KALARMCAL_LOG) << "-- mLateCancel:" << mLateCancel;
        qCDebug(KALARMCAL_LOG) << "-- mAutoClose:" << mAutoClose;
        qCDebug(KALARMCAL_LOG) << "-- mNotify:" << mNotify;
    } else if (mActionSubType == KAEvent::COMMAND) {
        qCDebug(KALARMCAL_LOG) << "-- mCommandScript:" << mCommandScript;
        qCDebug(KALARMCAL_LOG) << "-- mCommandXterm:" << mCommandXterm;
        qCDebug(KALARMCAL_LOG) << "-- mCommandDisplay:" << mCommandDisplay;
        qCDebug(KALARMCAL_LOG) << "-- mCommandHideError:" << mCommandHideError;
        qCDebug(KALARMCAL_LOG) << "-- mLogFile:" << mLogFile;
    } else if (mActionSubType == KAEvent::EMAIL) {
        qCDebug(KALARMCAL_LOG) << "-- mEmail: FromKMail:" << mEmailFromIdentity;
        qCDebug(KALARMCAL_LOG) << "--         Addresses:" << mEmailAddresses.join(QStringLiteral(","));
        qCDebug(KALARMCAL_LOG) << "--         Subject:" << mEmailSubject;
        qCDebug(KALARMCAL_LOG) << "--         Attachments:" << mEmailAttachments.join(QLatin1Char(','));
        qCDebug(KALARMCAL_LOG) << "--         Bcc:" << mEmailBcc;
    } else if (mActionSubType == KAEvent::AUDIO) {
        qCDebug(KALARMCAL_LOG) << "-- mAudioFile:" << mAudioFile;
    }
    qCDebug(KALARMCAL_LOG) << "-- mBeep:" << mBeep;
    if (mActionSubType == KAEvent::AUDIO  ||  !mAudioFile.isEmpty()) {
        if (mSoundVolume >= 0) {
            qCDebug(KALARMCAL_LOG) << "-- mSoundVolume:" << mSoundVolume;
            if (mFadeVolume >= 0) {
                qCDebug(KALARMCAL_LOG) << "-- mFadeVolume:" << mFadeVolume;
                qCDebug(KALARMCAL_LOG) << "-- mFadeSeconds:" << mFadeSeconds;
            } else {
                qCDebug(KALARMCAL_LOG) << "-- mFadeVolume:-:";
            }
        } else {
            qCDebug(KALARMCAL_LOG) << "-- mSoundVolume:-:";
        }
        qCDebug(KALARMCAL_LOG) << "-- mRepeatSoundPause:" << mRepeatSoundPause;
    }
    qCDebug(KALARMCAL_LOG) << "-- mAkonadiItemId:" << mAkonadiItemId;
    qCDebug(KALARMCAL_LOG) << "-- mCopyToKOrganizer:" << mCopyToKOrganizer;
    qCDebug(KALARMCAL_LOG) << "-- mExcludeHolidays:" << mExcludeHolidays;
    qCDebug(KALARMCAL_LOG) << "-- mWorkTimeOnly:" << mWorkTimeOnly;
    qCDebug(KALARMCAL_LOG) << "-- mStartDateTime:" << mStartDateTime.toString();
//     qCDebug(KALARMCAL_LOG) << "-- mCreatedDateTime:" << mCreatedDateTime;
    qCDebug(KALARMCAL_LOG) << "-- mRepeatAtLogin:" << mRepeatAtLogin;
//     if (mRepeatAtLogin)
//         qCDebug(KALARMCAL_LOG) << "-- mAtLoginDateTime:" << mAtLoginDateTime;
    qCDebug(KALARMCAL_LOG) << "-- mArchiveRepeatAtLogin:" << mArchiveRepeatAtLogin;
    qCDebug(KALARMCAL_LOG) << "-- mConfirmAck:" << mConfirmAck;
    qCDebug(KALARMCAL_LOG) << "-- mEnabled:" << mEnabled;
    qCDebug(KALARMCAL_LOG) << "-- mItemId:" << mItemId;
    qCDebug(KALARMCAL_LOG) << "-- mResourceId:" << mResourceId;
    qCDebug(KALARMCAL_LOG) << "-- mCompatibility:" << mCompatibility;
    qCDebug(KALARMCAL_LOG) << "-- mReadOnly:" << mReadOnly;
    if (mReminderMinutes) {
        qCDebug(KALARMCAL_LOG) << "-- mReminderMinutes:" << mReminderMinutes;
        qCDebug(KALARMCAL_LOG) << "-- mReminderActive:" << (mReminderActive == ACTIVE_REMINDER ? "active" : mReminderActive == HIDDEN_REMINDER ? "hidden" : "no");
        qCDebug(KALARMCAL_LOG) << "-- mReminderOnceOnly:" << mReminderOnceOnly;
    }
    if (mDeferral != NO_DEFERRAL) {
        qCDebug(KALARMCAL_LOG) << "-- mDeferral:" << (mDeferral == NORMAL_DEFERRAL ? "normal" : "reminder");
        qCDebug(KALARMCAL_LOG) << "-- mDeferralTime:" << mDeferralTime.toString();
    }
    qCDebug(KALARMCAL_LOG) << "-- mDeferDefaultMinutes:" << mDeferDefaultMinutes;
    if (mDeferDefaultMinutes) {
        qCDebug(KALARMCAL_LOG) << "-- mDeferDefaultDateOnly:" << mDeferDefaultDateOnly;
    }
    if (mDisplaying) {
        qCDebug(KALARMCAL_LOG) << "-- mDisplayingTime:" << mDisplayingTime.toString();
        qCDebug(KALARMCAL_LOG) << "-- mDisplayingFlags:" << mDisplayingFlags;
        qCDebug(KALARMCAL_LOG) << "-- mDisplayingDefer:" << mDisplayingDefer;
        qCDebug(KALARMCAL_LOG) << "-- mDisplayingEdit:" << mDisplayingEdit;
    }
    qCDebug(KALARMCAL_LOG) << "-- mRevision:" << mRevision;
    qCDebug(KALARMCAL_LOG) << "-- mRecurrence:" << mRecurrence;
    if (!mRepetition) {
        qCDebug(KALARMCAL_LOG) << "-- mRepetition: 0";
    } else if (mRepetition.isDaily()) {
        qCDebug(KALARMCAL_LOG) << "-- mRepetition: count:" << mRepetition.count() << ", interval:" << mRepetition.intervalDays() << "days";
    } else {
        qCDebug(KALARMCAL_LOG) << "-- mRepetition: count:" << mRepetition.count() << ", interval:" << mRepetition.intervalMinutes() << "minutes";
    }
    qCDebug(KALARMCAL_LOG) << "-- mNextRepeat:" << mNextRepeat;
    qCDebug(KALARMCAL_LOG) << "-- mAlarmCount:" << mAlarmCount;
    qCDebug(KALARMCAL_LOG) << "-- mMainExpired:" << mMainExpired;
    qCDebug(KALARMCAL_LOG) << "-- mDisplaying:" << mDisplaying;
    qCDebug(KALARMCAL_LOG) << "KAEvent dump end";
}
#endif

/******************************************************************************
* Fetch the start and next date/time for a KCalendarCore::Event.
* Reply = next main date/time.
*/
DateTime KAEventPrivate::readDateTime(const Event::Ptr &event, bool localZone, bool dateOnly, DateTime &start)
{
    start = DateTime(event->dtStart());
    if (dateOnly) {
        // A date-only event is indicated by the X-KDE-KALARM-FLAGS:DATE property, not
        // by a date-only start date/time (for the reasons given in updateKCalEvent()).
        start.setDateOnly(true);
    }
    if (localZone) {
        // The local system time zone is indicated by the X-KDE-KALARM-FLAGS:LOCAL
        // property, because QDateTime values with time spec Qt::LocalTime are not
        // stored correctly in the calendar file.
        start.setTimeSpec(KADateTime::LocalZone);
    }
    DateTime next = start;
    const int SZ_YEAR  = 4;                           // number of digits in year value
    const int SZ_MONTH = 2;                           // number of digits in month value
    const int SZ_DAY   = 2;                           // number of digits in day value
    const int SZ_DATE  = SZ_YEAR + SZ_MONTH + SZ_DAY; // total size of date value
    const int IX_TIME  = SZ_DATE + 1;                 // offset to time value
    const int SZ_HOUR  = 2;                           // number of digits in hour value
    const int SZ_MIN   = 2;                           // number of digits in minute value
    const int SZ_SEC   = 2;                           // number of digits in second value
    const int SZ_TIME  = SZ_HOUR + SZ_MIN + SZ_SEC;   // total size of time value
    const QString prop = event->customProperty(KACalendar::APPNAME, KAEventPrivate::NEXT_RECUR_PROPERTY);
    if (prop.length() >= SZ_DATE) {
        // The next due recurrence time is specified
        const QDate d(prop.leftRef(SZ_YEAR).toInt(),
                      prop.midRef(SZ_YEAR, SZ_MONTH).toInt(),
                      prop.midRef(SZ_YEAR + SZ_MONTH, SZ_DAY).toInt());
        if (d.isValid()) {
            if (dateOnly  &&  prop.length() == SZ_DATE) {
                next.setDate(d);
            } else if (!dateOnly  &&  prop.length() == IX_TIME + SZ_TIME  &&  prop[SZ_DATE] == QLatin1Char('T')) {
                const QTime t(prop.midRef(IX_TIME, SZ_HOUR).toInt(),
                              prop.midRef(IX_TIME + SZ_HOUR, SZ_MIN).toInt(),
                              prop.midRef(IX_TIME + SZ_HOUR + SZ_MIN, SZ_SEC).toInt());
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
* Parse the alarms for a KCalendarCore::Event.
* Reply = map of alarm data, indexed by KAAlarm::Type
*/
void KAEventPrivate::readAlarms(const Event::Ptr &event, AlarmMap *alarmMap, bool cmdDisplay)
{
    const Alarm::List alarms = event->alarms();

    // Check if it's an audio event with no display alarm
    bool audioOnly = false;
    for (const Alarm::Ptr &alarm : alarms) {
        bool done = false;
        switch (alarm->type()) {
        case Alarm::Display:
        case Alarm::Procedure:
            audioOnly = false;
            done = true;   // exit from the 'for' loop
            break;
        case Alarm::Audio:
            audioOnly = true;
            break;
        default:
            break;
        }
        if (done) {
            break;
        }
    }

    for (const Alarm::Ptr &alarm : alarms) {
        // Parse the next alarm's text
        AlarmData data;
        readAlarm(alarm, data, audioOnly, cmdDisplay);
        if (data.type != INVALID_ALARM) {
            alarmMap->insert(data.type, data);
        }
    }
}

/******************************************************************************
* Parse a KCalendarCore::Alarm.
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
    const QStringList flags = property.split(KAEventPrivate::SC, Qt::SkipEmptyParts);
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
        data.extraActionOptions = {};
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
        Q_FALLTHROUGH();
    case Alarm::Display: {
        if (alarm->type() == Alarm::Display) {
            data.action    = KAAlarm::MESSAGE;
            data.cleanText = AlarmText::fromCalendarText(alarm->text(), data.isEmailText);
        }
        const QString property = alarm->customProperty(KACalendar::APPNAME, KAEventPrivate::FONT_COLOUR_PROPERTY);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        const QStringList list = property.split(QLatin1Char(';'), QString::KeepEmptyParts);
#else
        const QStringList list = property.split(QLatin1Char(';'), Qt::KeepEmptyParts);
#endif
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
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
            const QStringList list = property.split(QLatin1Char(';'), QString::KeepEmptyParts);
#else
            const QStringList list = property.split(QLatin1Char(';'), Qt::KeepEmptyParts);
#endif
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

    bool atLogin      = false;
    bool reminder     = false;
    bool deferral     = false;
    bool dateDeferral = false;
    bool repeatSound  = false;
    data.type = MAIN_ALARM;
    property = alarm->customProperty(KACalendar::APPNAME, KAEventPrivate::TYPE_PROPERTY);
    const QStringList types = property.split(QLatin1Char(','), Qt::SkipEmptyParts);
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
            if (data.type == REMINDER_ALARM
            &&  flags.contains(KAEventPrivate::HIDDEN_REMINDER_FLAG)) {
                data.hiddenReminder = true;
            }
        } else if (data.type == DISPLAYING_ALARM)
            data.displayingFlags = dateDeferral ? REMINDER | DATE_DEFERRAL
                                   : deferral ? REMINDER | TIME_DEFERRAL : REMINDER;
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
//qCDebug(KALARMCAL_LOG)<<"text="<<alarm->text()<<", time="<<alarm->time().toString()<<", valid time="<<alarm->time().isValid();
}

QSharedPointer<const HolidayRegion> KAEventPrivate::holidays()
{
    if (!mHolidays)
        mHolidays.reset(new HolidayRegion());
    return mHolidays;
}

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
#pragma message("May need to set date-only alarms to after start-of-day time in working-time checks")
    holidays();   // initialise mHolidays if necessary
    bool recurs = (checkRecur() != KARecurrence::NO_RECUR);
    if ((recurs  &&  mWorkTimeOnly  &&  mWorkTimeOnly != mWorkTimeIndex)
    ||  (recurs  &&  mExcludeHolidays  &&  mExcludeHolidayRegion->regionCode() != mHolidays->regionCode())) {
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
        mExcludeHolidayRegion = mHolidays;    // note which holiday definition was used in calculation
    }
    bool excludeHolidays = mExcludeHolidays && mExcludeHolidayRegion->isValid();

    if (mCategory == CalEvent::ARCHIVED  ||  mCategory == CalEvent::TEMPLATE) {
        // It's a template or archived
        mAllTrigger = mMainTrigger = mAllWorkTrigger = mMainWorkTrigger = KADateTime();
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
        if ((!mWorkTimeOnly && !excludeHolidays)
        ||  !recurs
        ||  isWorkingTime(mMainTrigger.kDateTime())) {
            // It only occurs once, or it complies with any working hours/holiday
            // restrictions.
            mMainWorkTrigger = mMainTrigger;
            mAllWorkTrigger  = mAllTrigger;
        } else if (mWorkTimeOnly) {
            // The alarm is restricted to working hours.
            // Finding the next occurrence during working hours can sometimes take a long time,
            // so mark the next actual trigger as invalid until the calculation completes.
            // Note that reminders are only triggered if the main alarm is during working time.
            if (!excludeHolidays) {
                // There are no holiday restrictions.
                calcNextWorkingTime(mMainTrigger);
            } else if (mHolidays->isValid()) {
                // Holidays are excluded.
                DateTime nextTrigger = mMainTrigger;
                KADateTime kdt;
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
        } else if (excludeHolidays  &&  mHolidays->isValid()) {
            // Holidays are excluded.
            DateTime nextTrigger = mMainTrigger;
            KADateTime kdt;
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
    qCDebug(KALARMCAL_LOG) << "next=" << nextTrigger.kDateTime().toString(QStringLiteral("%Y-%m-%d %H:%M"));
    mMainWorkTrigger = mAllWorkTrigger = DateTime();

    if (!mWorkDays.count(true)) {
        return;    // no working days are defined
    }
    const KARecurrence::Type recurType = checkRecur();
    KADateTime kdt = nextTrigger.effectiveKDateTime();
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
        for (const RecurrenceRule::WDayPos &p : pos) {
            const int day = p.day() - 1;  // Monday = 0
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
        if ((!interval.isNull()  &&  !(interval.asDays() % 7))
        ||  nDayPos == 1) {
            // It recurs on the same day each week
            if (!mRepetition || weeklyRepeat) {
                return;    // any repetitions are also weekly
            }

            // It's a weekly recurrence with a non-weekly sub-repetition.
            // Check one cycle of repetitions for the next one that lands
            // on a working day.
            KADateTime dt(nextTrigger.kDateTime().addDays(1));
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
        KADateTime dt(nextTrigger.kDateTime().addDays(1));
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
            const KAEvent::OccurType type = nextOccurrence(kdt, newdt, KAEvent::RETURN_REPETITION);
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
    const QTimeZone tz = kdt.timeZone();
    // Get time zone transitions for the next 10 years.
    const QDateTime endTransitionsTime = QDateTime::currentDateTimeUtc().addYears(10);
    const QTimeZone::OffsetDataList tzTransitions = tz.transitions(mStartDateTime.qDateTime(), endTransitionsTime);

    if (recurTimeVaries) {
        /* The alarm recurs at regular clock intervals, at different times of day.
         * Note that for this type of recurrence, it's necessary to avoid the
         * performance overhead of Recurrence class calls since these can in the
         * worst case cause the program to hang for a significant length of time.
         * In this case, we can calculate the next recurrence by simply adding the
         * recurrence interval, since KAlarm offers no facility to regularly miss
         * recurrences. (But exception dates/times need to be taken into account.)
         */
        KADateTime kdtRecur;
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
                const int i = KAEventPrivate::transitionIndex(kdtRecur.toUtc().qDateTime(), tzTransitions);
                if (i < 0) {
                    return;
                }
                if (i > transitionIndex) {
                    transitionIndex = i;
                }
                if (++transitionIndex >= tzTransitions.count()) {
                    return;
                }
                previousOccurrence(KADateTime(tzTransitions[transitionIndex].atUtc), newdt, KAEvent::IGNORE_REPETITION);
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
//qCDebug(KALARMCAL_LOG)<<"-----exit loop: count="<<limit<<endl;
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
        KADateTime kdtRecur = newdt.effectiveKDateTime();
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
            KADateTime kdtNextRecur = newdt.effectiveKDateTime();

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
            const int i = KAEventPrivate::transitionIndex(kdtRecur.toUtc().qDateTime(), tzTransitions);
            if (i < 0) {
                return;
            }
            if (i > transitionIndex) {
                transitionIndex = i;
            }
            if (++transitionIndex >= tzTransitions.count()) {
                return;
            }
            kdt = KADateTime(tzTransitions[transitionIndex].atUtc);
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
int KAEventPrivate::nextWorkRepetition(const KADateTime &pre) const
{
    KADateTime nextWork(pre);
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
bool KAEventPrivate::mayOccurDailyDuringWork(const KADateTime &kdt) const
{
    if (!kdt.isDateOnly()
    &&  (kdt.time() < mWorkDayStart || kdt.time() >= mWorkDayEnd)) {
        return false;    // its time is outside working hours
    }
    // Check if it always occurs on the same day of the week
    const Duration interval = mRecurrence->regularInterval();
    if (!interval.isNull()  &&  interval.isDaily()  &&  !(interval.asDays() % 7)) {
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
                                 QStringLiteral("%1;%2;%3").arg(QString::number(mSoundVolume, 'f', 2), QString::number(mFadeVolume, 'f', 2), QString::number(mFadeSeconds)));
}

/******************************************************************************
* Get the date/time of the next recurrence of the event, after the specified
* date/time.
* 'result' = date/time of next occurrence, or invalid date/time if none.
*/
KAEvent::OccurType KAEventPrivate::nextRecurrence(const KADateTime &preDateTime, DateTime &result) const
{
    const KADateTime recurStart = mRecurrence->startDateTime();
    KADateTime pre = preDateTime.toTimeSpec(mStartDateTime.timeSpec());
    if (mStartDateTime.isDateOnly()  &&  !pre.isDateOnly()  &&  pre.time() < DateTime::startOfDay()) {
        pre = pre.addDays(-1);    // today's recurrence (if today recurs) is still to come
        pre.setTime(DateTime::startOfDay());
    }
    const KADateTime dt = mRecurrence->getNextDateTime(pre);
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
    static const QString TEXT_PREFIX      = QStringLiteral("TEXT:");
    static const QString FILE_PREFIX      = QStringLiteral("FILE:");
    static const QString COMMAND_PREFIX   = QStringLiteral("CMD:");

    // KAlarm pre-0.9.2 codes held in the event's CATEGORY property
    static const QString BEEP_CATEGORY    = QStringLiteral("BEEP");

    // KAlarm pre-1.1.1 LATECANCEL category with no parameter
    static const QString LATE_CANCEL_CAT = QStringLiteral("LATECANCEL");

    // KAlarm pre-1.3.0 TMPLDEFTIME category with no parameter
    static const QString TEMPL_DEF_TIME_CAT = QStringLiteral("TMPLDEFTIME");

    // KAlarm pre-1.3.1 XTERM category
    static const QString EXEC_IN_XTERM_CAT  = QStringLiteral("XTERM");

    // KAlarm pre-1.9.0 categories
    static const QString DATE_ONLY_CATEGORY        = QStringLiteral("DATE");
    static const QString EMAIL_BCC_CATEGORY        = QStringLiteral("BCC");
    static const QString CONFIRM_ACK_CATEGORY      = QStringLiteral("ACKCONF");
    static const QString KORGANIZER_CATEGORY       = QStringLiteral("KORG");
    static const QString DEFER_CATEGORY            = QStringLiteral("DEFER;");
    static const QString ARCHIVE_CATEGORY          = QStringLiteral("SAVE");
    static const QString ARCHIVE_CATEGORIES        = QStringLiteral("SAVE:");
    static const QString LATE_CANCEL_CATEGORY      = QStringLiteral("LATECANCEL;");
    static const QString AUTO_CLOSE_CATEGORY       = QStringLiteral("LATECLOSE;");
    static const QString TEMPL_AFTER_TIME_CATEGORY = QStringLiteral("TMPLAFTTIME;");
    static const QString KMAIL_SERNUM_CATEGORY     = QStringLiteral("KMAIL:");
    static const QString LOG_CATEGORY              = QStringLiteral("LOG:");

    // KAlarm pre-1.5.0/1.9.9 properties
    static const QByteArray KMAIL_ID_PROPERTY("KMAILID");    // X-KDE-KALARM-KMAILID property

    // KAlarm pre-2.6.0 properties
    static const QByteArray ARCHIVE_PROPERTY("ARCHIVE");     // X-KDE-KALARM-ARCHIVE property
    static const QString ARCHIVE_REMINDER_ONCE_TYPE = QStringLiteral("ONCE");
    static const QString REMINDER_ONCE_TYPE         = QStringLiteral("REMINDER_ONCE");
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

    qCDebug(KALARMCAL_LOG) << "Adjusting version" << calendarVersion;
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

    const QTimeZone localZone = QTimeZone::systemTimeZone();

    bool converted = false;
    const Event::List events = calendar->rawEvents();
    for (Event::Ptr event : events) {
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
            for (Alarm::Ptr alarm : alarms) {
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
                    Q_FALLTHROUGH();
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
                    alarm->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::TYPE_PROPERTY, types.join(QLatin1Char(',')));
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
                    KADateTime dt(alarm->time());
                    const qint64 t64 = dt.toSecsSinceEpoch();
                    const time_t t = (static_cast<quint64>(t64) >= uint(-1)) ? uint(-1) : static_cast<uint>(t64);
                    const struct tm *dtm = localtime(&t);
                    if (dtm->tm_isdst) {
                        dt = dt.addSecs(-3600);
                        alarm->setTime(dt.qDateTime());
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
             * X-KDE-KALARM-FONTCOLOR property.
             * Convert BEEP category into an audio alarm with no audio file.
             */
            if (CalEvent::status(event) == CalEvent::ARCHIVED) {
                event->setCreated(event->dtEnd());
            }
            QDateTime start = event->dtStart();
            if (event->allDay()) {
                start.setTime(QTime(0, 0));
                flags += KAEventPrivate::DATE_ONLY_FLAG;
            }
            event->setDtEnd(QDateTime());

            for (Alarm::Ptr alarm : alarms) {
                alarm->setStartOffset(start.secsTo(alarm->time()));
            }

            if (!cats.isEmpty()) {
                for (Alarm::Ptr alarm : alarms) {     //clazy:exclude=range-loop   Can't use reference because 'alarms' is const
                    if (alarm->type() == Alarm::Display)
                        alarm->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::FONT_COLOUR_PROPERTY,
                                                 QStringLiteral("%1;;").arg(cats.at(0)));
                }
                cats.removeAt(0);
            }

            {
                int i = cats.indexOf(BEEP_CATEGORY);
                if (i >= 0) {
                    cats.removeAt(i);

                    Alarm::Ptr alarm = event->newAlarm();
                    alarm->setEnabled(true);
                    alarm->setAudioAlarm();
                    QDateTime dt = event->dtStart();    // default

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
            for (Alarm::Ptr alarm : alarms) {
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
                (flags += KAEventPrivate::TEMPL_AFTER_TIME_FLAG) += QStringLiteral("0");
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
                    (flags += KAEventPrivate::KMAIL_ITEM_FLAG) += cat.mid(KMAIL_SERNUM_CATEGORY.length());
                } else if (cat == ARCHIVE_CATEGORY) {
                    event->setCustomProperty(KACalendar::APPNAME, ARCHIVE_PROPERTY, QStringLiteral("0"));
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
            event->shiftTimes(localZone, localZone);
            converted = true;
        }

        if (addLateCancel) {
            (flags += KAEventPrivate::LATE_CANCEL_FLAG) += QStringLiteral("1");
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
            const QStringList flags = event->customProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY).split(KAEventPrivate::SC, Qt::SkipEmptyParts);
            const bool dateOnly = flags.contains(KAEventPrivate::DATE_ONLY_FLAG);
            KADateTime startDateTime(event->dtStart());
            if (dateOnly) {
                startDateTime.setDateOnly(true);
            }
            // Convert the main alarm and get the next main trigger time from it
            KADateTime nextMainDateTime;
            bool mainExpired = true;
            for (Alarm::Ptr alarm : alarms) {
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
                const QStringList types = property.split(QLatin1Char(','), Qt::SkipEmptyParts);
                for (const QString &type : types) {
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
                        nextMainDateTime = KADateTime(alarm->time());
                        nextMainDateTime.setDateOnly(dateOnly);
                        nextMainDateTime = nextMainDateTime.toTimeSpec(startDateTime);
                        if (nextMainDateTime != startDateTime) {
                            QDateTime dt = nextMainDateTime.qDateTime();
                            event->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::NEXT_RECUR_PROPERTY,
                                                     dt.toString(dateOnly ? QStringLiteral("yyyyMMdd") : QStringLiteral("yyyyMMddThhmmss")));
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
                KADateTime dt(event->recurrence()->getNextDateTime(startDateTime.qDateTime().addDays(-1)));
                dt.setDateOnly(dateOnly);
                adjustment = startDateTime.secsTo(dt);
            } else {
                adjustment = startDateTime.secsTo(nextMainDateTime);
            }
            if (adjustment) {
                // Convert deferred alarms
                for (Alarm::Ptr alarm : alarms) {
                    if (!alarm->hasStartOffset()) {
                        continue;
                    }
                    const QString property = alarm->customProperty(KACalendar::APPNAME, KAEventPrivate::TYPE_PROPERTY);
                    const QStringList types = property.split(QLatin1Char(','), Qt::SkipEmptyParts);
                    for (const QString &type : types) {
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
            for (const Alarm::Ptr &alarm : alarms) {
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
                flags = event->customProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY).split(KAEventPrivate::SC, Qt::SkipEmptyParts);
                flags << KAEventPrivate::ARCHIVE_FLAG;
                flagsValid = true;
                if (prop != QLatin1String("0")) { // "0" was a dummy parameter if no others were present
                    // It's the archive property containing a reminder time and/or repeat-at-login flag.
                    // This was present when no reminder/at-login alarm was pending.
                    const QStringList list = prop.split(KAEventPrivate::SC, Qt::SkipEmptyParts);
                    for (const QString &pr : list) {
                        if (pr == KAEventPrivate::AT_LOGIN_TYPE) {
                            flags << KAEventPrivate::AT_LOGIN_TYPE;
                        } else if (pr == ARCHIVE_REMINDER_ONCE_TYPE) {
                            reminderOnce = true;
                        } else if (!pr.isEmpty()  &&  !pr.startsWith(QChar::fromLatin1('-'))) {
                            reminder = pr;
                        }
                    }
                }
                event->setCustomProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY, flags.join(KAEventPrivate::SC));
                event->removeCustomProperty(KACalendar::APPNAME, ARCHIVE_PROPERTY);
            }

            for (Alarm::Ptr alarm : alarms) {
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
                QStringList types = property.split(QChar::fromLatin1(','), Qt::SkipEmptyParts);
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
                    flags = event->customProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY).split(KAEventPrivate::SC, Qt::SkipEmptyParts);
                }
                if (!flags.contains(KAEventPrivate::REMINDER_TYPE)) {
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
    const QStringList flags = event->customProperty(KACalendar::APPNAME, KAEventPrivate::FLAGS_PROPERTY).split(KAEventPrivate::SC, Qt::SkipEmptyParts);
    if (flags.contains(KAEventPrivate::DATE_ONLY_FLAG)) {
        // It's an untimed event, so fix it
        const QDateTime oldDt = event->dtStart();
        const int adjustment = oldDt.time().secsTo(midnight);
        if (adjustment) {
            event->setDtStart(QDateTime(oldDt.date(), midnight, oldDt.timeSpec()));
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
        const KADateTime nextMainDateTime = readDateTime(event, false, false, start).kDateTime();
        AlarmMap alarmMap;
        readAlarms(event, &alarmMap);
        for (AlarmMap::ConstIterator it = alarmMap.constBegin();  it != alarmMap.constEnd();  ++it) {
            const AlarmData &data = it.value();
            if (!data.alarm->hasStartOffset()) {
                continue;
            }
            if ((data.type & DEFERRED_ALARM)  &&  !data.timedDeferral) {
                // Found a date-only deferral alarm, so adjust its time
                QDateTime altime = data.alarm->startOffset().end(nextMainDateTime.qDateTime());
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
    for (Alarm::Ptr alarm : alarms) {
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
= Corresponds to a single KCalendarCore::Alarm instance.
=============================================================================*/

KAAlarm::KAAlarm()
    : d(new Private)
{
}

KAAlarm::Private::Private()
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
           ? DateTime(d->mRepetition.duration(d->mNextRepeat).end(d->mNextMainDateTime.qDateTime()))
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

void KAAlarm::setTime(const KADateTime &dt)
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
    for (const Person &addr : addresses) {
        if (!addr.email().isEmpty()) {
            append(addr);
        }
    }
    return *this;
}

/******************************************************************************
* Return the email address list as a string list of email addresses.
*/
EmailAddressList::operator QStringList() const
{
    int cnt = count();
    QStringList list;
    list.reserve(cnt);
    for (int p = 0;  p < cnt;  ++p) {
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
    const Person &person = (*this)[index];
    const QString name = person.name();
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
        result += (*this)[index].name();
        result += (quote ? QLatin1String("\" <") : QLatin1String(" <"));
        quote = true;    // need angle brackets round email address
    }

    result += person.email();
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
    int cnt = count();
    QStringList list;
    list.reserve(cnt);
    for (int p = 0;  p < cnt;  ++p) {
        list += at(p).email();
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
        result += at(p).email();
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
    //TODO: cater for environment variables prefixed to command
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
                Q_FALLTHROUGH();
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
    return QStringLiteral("%1%2").arg(count).arg(unit);
}

} // namespace KAlarmCal

// vim: et sw=4:
