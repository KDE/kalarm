/*
 *  kaevent.h  -  represents calendar events
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

#ifndef KAEVENT_H
#define KAEVENT_H

#include "kalarm_cal_export.h"

#include "datetime.h"
#include "karecurrence.h"
#include "kacalendar.h"
#include "repetition.h"

#ifdef USE_AKONADI
#include "kcalcore_constptr.h"
#include <akonadi/collection.h>
#include <akonadi/item.h>
#include <kcalcore/alarm.h>
#include <kcalcore/person.h>
#include <kcalcore/calendar.h>
#else
#include <kcal/person.h>
#endif

#include <QBitArray>
#include <QColor>
#include <QFont>
#include <QList>
#include <QMetaType>

namespace KHolidays { class HolidayRegion; }
#ifndef USE_AKONADI
namespace KCal {
    class CalendarLocal;
    class Event;
}
class AlarmResource;
#endif
class AlarmData;


#ifdef USE_AKONADI
typedef KCalCore::Person  EmailAddress;
class KALARM_CAL_EXPORT EmailAddressList : public KCalCore::Person::List
#else
typedef KCal::Person  EmailAddress;
class KALARM_CAL_EXPORT EmailAddressList : public QList<KCal::Person>
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


// Base class containing data common to KAAlarm and KAEvent::Private
class KALARM_CAL_EXPORT KAAlarmEventBase
{
    public:
        ~KAAlarmEventBase()  { }
        int                lateCancel() const          { return mLateCancel; }

    protected:
        enum Type  { T_MESSAGE, T_FILE, T_COMMAND, T_EMAIL, T_AUDIO };

        KAAlarmEventBase() : mNextRepeat(0), mLateCancel(0), mAutoClose(false), mRepeatAtLogin(false)  {}
        KAAlarmEventBase(const KAAlarmEventBase& rhs)             { copy(rhs); }
        KAAlarmEventBase& operator=(const KAAlarmEventBase& rhs)  { copy(rhs);  return *this; }
        void               copy(const KAAlarmEventBase&);
        void               set(int flags);
        int                baseFlags() const;
#ifdef KDE_NO_DEBUG_OUTPUT
        void               baseDumpDebug() const  { }
#else
        void               baseDumpDebug() const;
#endif

        QString            mEventID;          // UID: KCal::Event unique ID
        DateTime           mNextMainDateTime; // next time to display the alarm, excluding repetitions
        QString            mText;             // message text, file URL, command, email body [or audio file for KAAlarm]
        QColor             mBgColour;         // background colour of alarm message
        QColor             mFgColour;         // foreground colour of alarm message, or invalid for default
        QFont              mFont;             // font of alarm message (ignored if mUseDefaultFont true)
        Type               mActionType;       // alarm action type
        Repetition         mRepetition;       // sub-repetition count and interval
        int                mNextRepeat;       // repetition count of next due sub-repetition
        int                mLateCancel;       // how many minutes late will cancel the alarm, or 0 for no cancellation
        bool               mAutoClose;        // whether to close the alarm window after the late-cancel period
        bool               mCommandScript;    // the command text is a script, not a shell command line
        bool               mRepeatAtLogin;    // whether to repeat the alarm at every login
        bool               mUseDefaultFont;   // use default message font, not mFont

    friend class KAEvent;
    friend class AlarmData;
};


// KAAlarm corresponds to a single KCal::Alarm instance.
// A KAEvent may contain multiple KAAlarm's.
class KALARM_CAL_EXPORT KAAlarm : public KAAlarmEventBase
{
    public:
        // Define the basic KAAlarm action types
        enum Action
        {
            MESSAGE = T_MESSAGE,   // KCal::Alarm::Display type: display a text message
            FILE    = T_FILE,      // KCal::Alarm::Display type: display a file (URL given by the alarm text)
            COMMAND = T_COMMAND,   // KCal::Alarm::Procedure type: execute a shell command
            EMAIL   = T_EMAIL,     // KCal::Alarm::Email type: send an email
            AUDIO   = T_AUDIO      // KCal::Alarm::Audio type: play a sound file
        };
        // Define the KAAlarm types.
        // KAAlarm's of different types may be contained in a KAEvent,
        // each defining a different component of the overall alarm.
        enum Type
        {
            INVALID_ALARM       = 0,     // not an alarm
            MAIN_ALARM          = 1,     // THE real alarm. Must be the first in the enumeration.
            // The following values may be used in combination as a bitmask 0x0E
            REMINDER_ALARM      = 0x02,  // reminder in advance of main alarm
            DEFERRED_ALARM      = 0x04,  // deferred alarm
            DEFERRED_REMINDER_ALARM = REMINDER_ALARM | DEFERRED_ALARM,  // deferred early warning
            // The following values must be greater than the preceding ones, to
            // ensure that in ordered processing they are processed afterwards.
            AT_LOGIN_ALARM      = 0x10,  // additional repeat-at-login trigger
            DISPLAYING_ALARM    = 0x20,  // copy of the alarm currently being displayed
            // The following values are for internal KAEvent use only
            AUDIO_ALARM         = 0x30,  // sound to play when displaying the alarm
            PRE_ACTION_ALARM    = 0x40,  // command to execute before displaying the alarm
            POST_ACTION_ALARM   = 0x50   // command to execute after the alarm window is closed
        };
        enum SubType
        {
            INVALID__ALARM                = INVALID_ALARM,
            MAIN__ALARM                   = MAIN_ALARM,
            AT_LOGIN__ALARM               = AT_LOGIN_ALARM,
            // The following values may be used in combination as a bitmask 0x0E
            REMINDER__ALARM               = REMINDER_ALARM,
            TIMED_DEFERRAL_FLAG           = 0x08,  // deferral has a time; if clear, it is date-only
            DEFERRED_DATE__ALARM          = DEFERRED_ALARM,  // deferred alarm - date-only
            DEFERRED_TIME__ALARM          = DEFERRED_ALARM | TIMED_DEFERRAL_FLAG,
            DEFERRED_REMINDER_DATE__ALARM = REMINDER_ALARM | DEFERRED_ALARM,  // deferred early warning (date-only)
            DEFERRED_REMINDER_TIME__ALARM = REMINDER_ALARM | DEFERRED_ALARM | TIMED_DEFERRAL_FLAG,  // deferred early warning (date/time)
            // The following values must be greater than the preceding ones, to
            // ensure that in ordered processing they are processed afterwards.
            DISPLAYING__ALARM             = DISPLAYING_ALARM,
            // The following values are for internal KAEvent use only
            AUDIO__ALARM                  = AUDIO_ALARM,   // audio alarm for main display alarm
            PRE_ACTION__ALARM             = PRE_ACTION_ALARM,
            POST_ACTION__ALARM            = POST_ACTION_ALARM
        };

        KAAlarm()          : mType(INVALID__ALARM), mDeferred(false) { }
        KAAlarm(const KAAlarm&);
        ~KAAlarm()  { }
        Action             action() const               { return (Action)mActionType; }
        bool               isValid() const              { return mType != INVALID__ALARM; }
        Type               type() const                 { return static_cast<Type>(mType & ~TIMED_DEFERRAL_FLAG); }
        SubType            subType() const              { return mType; }
        const QString&     eventId() const              { return mEventID; }
        DateTime           dateTime(bool withRepeats = false) const
                                                        { return (withRepeats && mNextRepeat && mRepetition)
                                                            ? mRepetition.duration(mNextRepeat).end(mNextMainDateTime.kDateTime()) : mNextMainDateTime; }
        QDate              date() const                 { return mNextMainDateTime.date(); }
        QTime              time() const                 { return mNextMainDateTime.effectiveTime(); }
        bool               repeatAtLogin() const        { return mRepeatAtLogin; }
        bool               isReminder() const           { return mType == REMINDER__ALARM; }
        bool               deferred() const             { return mDeferred; }
        void               setTime(const DateTime& dt)  { mNextMainDateTime = dt; }
        void               setTime(const KDateTime& dt) { mNextMainDateTime = dt; }
#ifdef KDE_NO_DEBUG_OUTPUT
        void               dumpDebug() const  { }
        static const char* debugType(Type)   { return ""; }
#else
        void               dumpDebug() const;
        static const char* debugType(Type);
#endif

    private:
        SubType            mType;             // alarm type
        bool               mRecurs;           // there is a recurrence rule for the alarm
        bool               mDeferred;         // whether the alarm is an extra deferred/deferred-reminder alarm

    friend class KAEvent;
};


/** KAEvent corresponds to a KCal::Event instance */
class KALARM_CAL_EXPORT KAEvent
{
    public:
        typedef QList<KAEvent*> List;
        enum          // Flags for use in D-Bus calls, etc.
        {
            BEEP            = 0x02,    // sound audible beep when alarm is displayed
            REPEAT_AT_LOGIN = 0x04,    // repeat alarm at every login
            ANY_TIME        = 0x08,    // only a date is specified for the alarm, not a time
            CONFIRM_ACK     = 0x10,    // closing the alarm message window requires confirmation prompt
            EMAIL_BCC       = 0x20,    // blind copy the email to the user
            DEFAULT_FONT    = 0x40,    // use default alarm message font
            REPEAT_SOUND    = 0x80,    // repeat sound file while alarm is displayed
            DISABLED        = 0x100,   // alarm is currently disabled
            AUTO_CLOSE      = 0x200,   // auto-close alarm window after late-cancel period
            SCRIPT          = 0x400,   // command is a script, not a shell command line
            EXEC_IN_XTERM   = 0x800,   // execute command in terminal window
            SPEAK           = 0x1000,  // speak the message when the alarm is displayed
            COPY_KORGANIZER = 0x2000,  // KOrganizer should hold a copy of the event
            EXCL_HOLIDAYS   = 0x4000,  // don't trigger alarm on holidays
            WORK_TIME_ONLY  = 0x8000,  // trigger alarm only during working hours
            DISPLAY_COMMAND = 0x10000, // display command output in alarm window
            REMINDER_ONCE   = 0x20000, // only trigger reminder on first recurrence
            // The following are read-only internal values
            REMINDER        = 0x100000,
            DEFERRAL        = 0x200000,
            TIMED_FLAG      = 0x400000,
            DATE_DEFERRAL   = DEFERRAL,
            TIME_DEFERRAL   = DEFERRAL | TIMED_FLAG,
            DISPLAYING_     = 0x800000,
            READ_ONLY_FLAGS = 0xF00000  // mask for all read-only internal values
        };
        enum Actions     // The alarm's basic action type(s)
        {
            ACT_NONE    = 0,
            ACT_DISPLAY = 0x01,   // the alarm displays something
            ACT_COMMAND = 0x02,   // the alarm executes a command
            ACT_EMAIL   = 0x04,   // the alarm sends an email
            ACT_AUDIO   = 0x08,   // the alarm plays an audio file (without any display)
            ACT_DISPLAY_COMMAND = ACT_DISPLAY | ACT_COMMAND,  // the alarm displays command output
            ACT_ALL     = ACT_DISPLAY | ACT_COMMAND | ACT_EMAIL | ACT_AUDIO   // all types mask
        };
        enum Action
        {
            MESSAGE = KAAlarmEventBase::T_MESSAGE,
            FILE    = KAAlarmEventBase::T_FILE,
            COMMAND = KAAlarmEventBase::T_COMMAND,
            EMAIL   = KAAlarmEventBase::T_EMAIL,
            AUDIO   = KAAlarmEventBase::T_AUDIO
        };
        enum OccurType     // what type of occurrence is due
        {
            NO_OCCURRENCE               = 0,      // no occurrence is due
            FIRST_OR_ONLY_OCCURRENCE    = 0x01,   // the first occurrence (takes precedence over LAST_RECURRENCE)
            RECURRENCE_DATE             = 0x02,   // a recurrence with only a date, not a time
            RECURRENCE_DATE_TIME        = 0x03,   // a recurrence with a date and time
            LAST_RECURRENCE             = 0x04,   // the last recurrence
            OCCURRENCE_REPEAT = 0x10,    // (bitmask for a repetition of an occurrence)
            FIRST_OR_ONLY_OCCURRENCE_REPEAT = OCCURRENCE_REPEAT | FIRST_OR_ONLY_OCCURRENCE,     // a repetition of the first occurrence
            RECURRENCE_DATE_REPEAT          = OCCURRENCE_REPEAT | RECURRENCE_DATE,      // a repetition of a date-only recurrence
            RECURRENCE_DATE_TIME_REPEAT     = OCCURRENCE_REPEAT | RECURRENCE_DATE_TIME, // a repetition of a date/time recurrence
            LAST_RECURRENCE_REPEAT          = OCCURRENCE_REPEAT | LAST_RECURRENCE       // a repetition of the last recurrence
        };
        enum OccurOption     // options for nextOccurrence()
        {
            IGNORE_REPETITION,    // check for recurrences only, ignore repetitions
            RETURN_REPETITION,    // return repetition if it's the next occurrence
            ALLOW_FOR_REPETITION  // check for repetition being the next occurrence, but return recurrence
        };
        enum DeferLimitType    // what type of occurrence currently limits a deferral
        {
            LIMIT_NONE,
            LIMIT_MAIN,
            LIMIT_RECURRENCE,
            LIMIT_REPETITION,
            LIMIT_REMINDER
        };
        enum TriggerType    // alarm trigger type
        {
            ALL_TRIGGER,       // next trigger, including reminders, ignoring working hours & holidays
            MAIN_TRIGGER,      // next trigger, excluding reminders, ignoring working hours & holidays
            WORK_TRIGGER,      // next main working time trigger, excluding reminders
            ALL_WORK_TRIGGER,  // next actual working time trigger, including reminders
            DISPLAY_TRIGGER    // next trigger time for display purposes (i.e. excluding reminders)
        };
        enum CmdErrType     // command execution error type for last time the alarm was triggered
        {
            CMD_NO_ERROR   = 0,      // no error
            CMD_ERROR      = 0x01,   // command alarm execution failed
            CMD_ERROR_PRE  = 0x02,   // pre-alarm command execution failed
            CMD_ERROR_POST = 0x04,   // post-alarm command execution failed
            CMD_ERROR_PRE_POST = CMD_ERROR_PRE | CMD_ERROR_POST
        };
        enum UidAction      // how to deal with event UID in updateKCalEvent()
        {
            UID_IGNORE,        // leave KCal::Event UID unchanged
            UID_CHECK,         // verify that the KCal::Event UID is already the same as the KAEvent ID, if the latter is non-empty
            UID_SET            // set the KCal::Event UID to the KAEvent ID
        };

        KAEvent();
        KAEvent(const KDateTime&, const QString& message, const QColor& bg, const QColor& fg,
                const QFont& f, Action, int lateCancel, int flags, bool changesPending = false);
#ifdef USE_AKONADI
//        explicit KAEvent(const QSharedPointer<const KCalCore::Event>&);
        explicit KAEvent(const KCalCore::ConstEventPtr&);
        void               set(const KCalCore::ConstEventPtr& e)   { d->set(e); }
#else
        explicit KAEvent(const KCal::Event*);
        void               set(const KCal::Event* e)               { d->set(e); }
#endif
        void               set(const KDateTime& dt, const QString& message, const QColor& bg, const QColor& fg, const QFont& f, Action act, int lateCancel, int flags, bool changesPending = false)
                                                                   { d->set(dt, message, bg, fg, f, act, lateCancel, flags, changesPending); }
        void               setEmail(uint from, const EmailAddressList&, const QString& subject, const QStringList& attachments);
#ifndef USE_AKONADI
        void               setResource(AlarmResource* r)           { d->mResource = r; }
#endif
        void               setAudioFile(const QString& filename, float volume, float fadeVolume, int fadeSeconds, bool allowEmptyFile = false)
                                                                   { d->setAudioFile(filename, volume, fadeVolume, fadeSeconds, allowEmptyFile); }
        void               setTemplate(const QString& name, int afterTime = -1);
        void               setActions(const QString& pre, const QString& post, bool cancelOnError, bool dontShowError)
                                                                   { d->mPreAction = pre;  d->mPostAction = post;  d->mCancelOnPreActErr = cancelOnError;  d->mDontShowPreActErr = dontShowError; }
        OccurType          setNextOccurrence(const KDateTime& preDateTime)  { return d->setNextOccurrence(preDateTime); }
        void               setFirstRecurrence()                    { d->setFirstRecurrence(); }
        void               setCategory(KAlarm::CalEvent::Type s)   { d->setCategory(s); }
        void               setUid(KAlarm::CalEvent::Type s)        { d->mEventID = KAlarm::CalEvent::uid(d->mEventID, s); }
        void               setEventId(const QString& id)           { d->mEventID = id; }
#ifdef USE_AKONADI
        void               setItemId(Akonadi::Item::Id id)         { d->mItemId = id; }
        void               setCompatibility(KAlarm::Calendar::Compat c) { if (c != d->mCompatibility) { d->mCompatibility = c; } }
        void               setReadOnly(bool ro)                    { if (ro != d->mReadOnly) { d->mReadOnly = ro; } }
#endif
        void               setTime(const KDateTime& dt)            { d->mNextMainDateTime = dt; d->mTriggerChanged = true; }
        void               setSaveDateTime(const KDateTime& dt)    { d->mSaveDateTime = dt; }
        void               setLateCancel(int lc)                   { d->mLateCancel = lc; }
        void               setAutoClose(bool ac)                   { d->mAutoClose = ac; }
        void               setRepeatAtLogin(bool rl)               { d->setRepeatAtLogin(rl); }
        void               setExcludeHolidays(bool ex)             { d->mExcludeHolidays = ex ? Private::mHolidays : 0; d->mTriggerChanged = true; }
        void               setWorkTimeOnly(bool wto)               { d->mWorkTimeOnly = wto; d->mTriggerChanged = true; }
        void               setKMailSerialNumber(unsigned long n)   { d->mKMailSerialNumber = n; }
        void               setLogFile(const QString& logfile);
        void               setReminder(int minutes, bool onceOnly) { d->setReminder(minutes, onceOnly); }
        void               activateReminderAfter(const DateTime& mainAlarmTime)  { d->activateReminderAfter(mainAlarmTime); }
        bool               defer(const DateTime& dt, bool reminder, bool adjustRecurrence = false)
                                                                   { return d->defer(dt, reminder, adjustRecurrence); }
        void               cancelDefer()                           { d->cancelDefer(); }
        void               setDeferDefaultMinutes(int minutes, bool dateOnly = false)
                                                                   { d->mDeferDefaultMinutes = minutes;  d->mDeferDefaultDateOnly = dateOnly; }
#ifdef USE_AKONADI
        bool               setDisplaying(const KAEvent& e, KAAlarm::Type t, Akonadi::Collection::Id colId, const KDateTime& dt, bool showEdit, bool showDefer)
                                                                   { return d->setDisplaying(*e.d, t, colId, dt, showEdit, showDefer); }
        void               reinstateFromDisplaying(const KCalCore::ConstEventPtr& e, Akonadi::Collection::Id& colId, bool& showEdit, bool& showDefer)
                                                                   { d->reinstateFromDisplaying(e, colId, showEdit, showDefer); }
        void               setCommandError(CmdErrType t) const     { d->setCommandError(t); }
#else
        bool               setDisplaying(const KAEvent& e, KAAlarm::Type t, const QString& resourceID, const KDateTime& dt, bool showEdit, bool showDefer)
                                                                   { return d->setDisplaying(*e.d, t, resourceID, dt, showEdit, showDefer); }
        void               reinstateFromDisplaying(const KCal::Event* e, QString& resourceID, bool& showEdit, bool& showDefer)
                                                                   { d->reinstateFromDisplaying(e, resourceID, showEdit, showDefer); }
        void               setCommandError(const QString& configString) { d->setCommandError(configString); }
        void               setCommandError(CmdErrType t, bool writeConfig = true) const
                                                                   { d->setCommandError(t, writeConfig); }
#endif
        void               setArchive()                            { d->mArchive = true; }
        void               setEnabled(bool enable)                 { d->mEnabled = enable; }
        void               startChanges()                          { d->startChanges(); }
        void               endChanges()                            { d->endChanges(); }
#ifdef USE_AKONADI
        void               clearCollectionId()                     { d->mCollectionId = -1; }
#else
        void               clearResourceId()                       { d->mResourceId.clear(); }
#endif
        void               removeExpiredAlarm(KAAlarm::Type t)     { d->removeExpiredAlarm(t); }
        void               incrementRevision()                     { ++d->mRevision; }

        const QString&     cleanText() const              { return d->mText; }
        QString            message() const                { return (d->mActionType == KAAlarmEventBase::T_MESSAGE || d->mActionType == KAAlarmEventBase::T_EMAIL) ? d->mText : QString(); }
        QString            displayMessage() const         { return (d->mActionType == KAAlarmEventBase::T_MESSAGE) ? d->mText : QString(); }
        QString            fileName() const               { return (d->mActionType == KAAlarmEventBase::T_FILE) ? d->mText : QString(); }
        QString            command() const                { return (d->mActionType == KAAlarmEventBase::T_COMMAND) ? d->mText : QString(); }
        QString            emailMessage() const           { return (d->mActionType == KAAlarmEventBase::T_EMAIL) ? d->mText : QString(); }
        uint               emailFromId() const            { return d->mEmailFromIdentity; }
        const EmailAddressList& emailAddresses() const    { return d->mEmailAddresses; }
        QString            emailAddresses(const QString& sep) const  { return d->mEmailAddresses.join(sep); }
        QStringList        emailPureAddresses() const     { return d->mEmailAddresses.pureAddresses(); }
        QString            emailPureAddresses(const QString& sep) const  { return d->mEmailAddresses.pureAddresses(sep); }
        const QString&     emailSubject() const           { return d->mEmailSubject; }
        const QStringList& emailAttachments() const       { return d->mEmailAttachments; }
        QString            emailAttachments(const QString& sep) const  { return d->mEmailAttachments.join(sep); }
        bool               emailBcc() const               { return d->mEmailBcc; }
        const QColor&      bgColour() const               { return d->mBgColour; }
        const QColor&      fgColour() const               { return d->mFgColour; }
        bool               useDefaultFont() const         { return d->mUseDefaultFont; }
        QFont              font() const                   { return d->mUseDefaultFont ? Private::mDefaultFont : d->mFont; }
        int                lateCancel() const             { return d->lateCancel(); }
        bool               autoClose() const              { return d->mAutoClose; }
        bool               commandScript() const          { return d->mCommandScript; }
        bool               confirmAck() const             { return d->mConfirmAck; }
        bool               repeatAtLogin(bool includeArchived = false) const  { return d->mRepeatAtLogin || (includeArchived && d->mArchiveRepeatAtLogin); }
        const Repetition&  repetition() const             { return d->mRepetition; }
        int                nextRepetition() const         { return d->mNextRepeat; }
        bool               beep() const                   { return d->mBeep; }
        bool               isTemplate() const             { return !d->mTemplateName.isEmpty(); }
        const QString&     templateName() const           { return d->mTemplateName; }
        bool               usingDefaultTime() const       { return d->mTemplateAfterTime == 0; }
        int                templateAfterTime() const      { return d->mTemplateAfterTime; }
        KAAlarm            alarm(KAAlarm::Type t) const   { return d->alarm(t); }
        KAAlarm            firstAlarm() const             { return d->firstAlarm(); }
        KAAlarm            nextAlarm(const KAAlarm& al) const  { return d->nextAlarm(al.type()); }
        KAAlarm            nextAlarm(KAAlarm::Type t) const    { return d->nextAlarm(t); }
        KAAlarm            convertDisplayingAlarm() const;
#ifdef USE_AKONADI
        bool               setItemPayload(Akonadi::Item&, const QStringList& collectionMimeTypes) const;
        bool               updateKCalEvent(const KCalCore::Event::Ptr& e, UidAction u, bool setCustomProperties = true) const
                                                          { return d->updateKCalEvent(e, u, setCustomProperties); }
#else
        bool               updateKCalEvent(KCal::Event* e, UidAction u) const
                                                          { return d->updateKCalEvent(e, u); }
#endif
        Action             action() const                 { return (Action)d->mActionType; }
        Actions            actions() const;
        bool               displayAction() const          { return d->mActionType == KAAlarmEventBase::T_MESSAGE || d->mActionType == KAAlarmEventBase::T_FILE || (d->mActionType == KAAlarmEventBase::T_COMMAND && d->mCommandDisplay); }
        const QString&     id() const                     { return d->mEventID; }
#ifdef USE_AKONADI
        Akonadi::Item::Id  itemId() const                 { return d->mItemId; }
        KAlarm::Calendar::Compat compatibility() const    { return d->mCompatibility; }
        bool               isReadOnly() const             { return d->mReadOnly; }
#endif
        int                revision() const               { return d->mRevision; }
        bool               isValid() const                { return d->mAlarmCount  &&  (d->mAlarmCount != 1 || !d->mRepeatAtLogin); }
        int                alarmCount() const             { return d->mAlarmCount; }
        KDateTime          createdDateTime() const        { return d->mSaveDateTime; }
        const DateTime&    startDateTime() const          { return d->mStartDateTime; }
        DateTime           mainDateTime(bool withRepeats = false) const
                                                          { return d->mainDateTime(withRepeats); }
        QDate              mainDate() const               { return d->mNextMainDateTime.date(); }
        QTime              mainTime() const               { return d->mNextMainDateTime.effectiveTime(); }
        DateTime           mainEndRepeatTime() const      { return d->mainEndRepeatTime(); }
        DateTime           nextTrigger(TriggerType) const;
        int                reminderMinutes() const        { return d->mReminderMinutes; }
        bool               reminderActive() const         { return d->mReminderActive == Private::ACTIVE_REMINDER; }
        bool               reminderOnceOnly() const       { return d->mReminderOnceOnly; }
        bool               reminderDeferral() const       { return d->mDeferral == Private::REMINDER_DEFERRAL; }
        DateTime           deferDateTime() const          { return d->mDeferralTime; }
        DateTime           deferralLimit(DeferLimitType* t = 0) const
                                                          { return d->deferralLimit(t); }
        int                deferDefaultMinutes() const    { return d->mDeferDefaultMinutes; }
        bool               deferDefaultDateOnly() const   { return d->mDeferDefaultDateOnly; }
        const QString&     messageFileOrCommand() const   { return d->mText; }
        QString            logFile() const                { return d->mLogFile; }
        bool               commandXterm() const           { return d->mCommandXterm; }
        bool               commandDisplay() const         { return d->mCommandDisplay; }
        unsigned long      kmailSerialNumber() const      { return d->mKMailSerialNumber; }
        bool               copyToKOrganizer() const       { return d->mCopyToKOrganizer; }
        bool               holidaysExcluded() const       { return d->mExcludeHolidays; }
        bool               workTimeOnly() const           { return d->mWorkTimeOnly; }
        bool               speak() const                  { return (d->mActionType == KAAlarmEventBase::T_MESSAGE  ||  (d->mActionType == KAAlarmEventBase::T_COMMAND && d->mCommandDisplay)) && d->mSpeak; }
        const QString&     audioFile() const              { return d->mAudioFile; }
        float              soundVolume() const            { return d->mSoundVolume; }
        float              fadeVolume() const             { return d->mSoundVolume >= 0 && d->mFadeSeconds ? d->mFadeVolume : -1; }
        int                fadeSeconds() const            { return d->mSoundVolume >= 0 && d->mFadeVolume >= 0 ? d->mFadeSeconds : 0; }
        bool               repeatSound() const            { return d->mRepeatSound; }
        const QString&     preAction() const              { return d->mPreAction; }
        const QString&     postAction() const             { return d->mPostAction; }
        bool               cancelOnPreActionError() const { return d->mCancelOnPreActErr; }
        bool               dontShowPreActionError() const { return d->mDontShowPreActErr; }
        int                flags() const                  { return d->flags(); }
        bool               deferred() const               { return d->mDeferral > 0; }
        bool               toBeArchived() const           { return d->mArchive; }
        bool               enabled() const                { return d->mEnabled; }
        bool               mainExpired() const            { return d->mMainExpired; }
        bool               expired() const                { return (d->mDisplaying && d->mMainExpired)  ||  d->mCategory == KAlarm::CalEvent::ARCHIVED; }
        KAlarm::CalEvent::Type category() const           { return d->mCategory; }
        bool               displaying() const             { return d->mDisplaying; }
#ifdef USE_AKONADI
        Akonadi::Collection::Id collectionId() const      { return d->mCollectionId; }
#else
        QString            resourceId() const             { return d->mResourceId; }
        AlarmResource*     resource() const               { return d->mResource; }
#endif
        CmdErrType         commandError() const           { return d->mCommandError; }
#ifndef USE_AKONADI
        static QString     commandErrorConfigGroup()      { return Private::mCmdErrConfigGroup; }
#endif

        bool               isWorkingTime(const KDateTime& dt) const  { return d->isWorkingTime(dt); }

        struct MonthPos
        {
            MonthPos() : days(7) { }
            int        weeknum;     // week in month, or < 0 to count from end of month
            QBitArray  days;        // days in week
        };
        bool               setRepetition(const Repetition& r)  { return d->setRepetition(r); }
        bool               recurs() const                 { return d->checkRecur() != KARecurrence::NO_RECUR; }
        KARecurrence::Type recurType() const              { return d->checkRecur(); }
        KARecurrence*      recurrence() const             { return d->mRecurrence; }
        int                recurInterval() const;    // recurrence period in units of the recurrence period type (minutes, days, etc)
#ifdef USE_AKONADI
        KCalCore::Duration longestRecurrenceInterval() const  { return d->mRecurrence ? d->mRecurrence->longestInterval() : KCalCore::Duration(0); }
#else
        KCal::Duration     longestRecurrenceInterval() const  { return d->mRecurrence ? d->mRecurrence->longestInterval() : KCal::Duration(0); }
#endif
        QString            recurrenceText(bool brief = false) const;
        QString            repetitionText(bool brief = false) const;
        bool               occursAfter(const KDateTime& preDateTime, bool includeRepetitions) const
                                                          { return d->occursAfter(preDateTime, includeRepetitions); }
        OccurType          nextOccurrence(const KDateTime& preDateTime, DateTime& result, OccurOption o = IGNORE_REPETITION) const
                                                          { return d->nextOccurrence(preDateTime, result, o); }
        OccurType          previousOccurrence(const KDateTime& afterDateTime, DateTime& result, bool includeRepetitions = false) const
                                                          { return d->previousOccurrence(afterDateTime, result, includeRepetitions); }
        void               setNoRecur()                   { d->clearRecur(); }
        void               setRecurrence(const KARecurrence& r)   { d->setRecurrence(r); }
        bool               setRecurMinutely(int freq, int count, const KDateTime& end);
        bool               setRecurDaily(int freq, const QBitArray& days, int count, const QDate& end);
        bool               setRecurWeekly(int freq, const QBitArray& days, int count, const QDate& end);
        bool               setRecurMonthlyByDate(int freq, const QList<int>& days, int count, const QDate& end);
        bool               setRecurMonthlyByPos(int freq, const QList<MonthPos>& pos, int count, const QDate& end);
        bool               setRecurAnnualByDate(int freq, const QList<int>& months, int day, KARecurrence::Feb29Type, int count, const QDate& end);
        bool               setRecurAnnualByPos(int freq, const QList<MonthPos>& pos, const QList<int>& months, int count, const QDate& end);
//        static QValueList<MonthPos> convRecurPos(const QValueList<KCal::RecurrenceRule::WDayPos>&);
        void               adjustRecurrenceStartOfDay();
#ifdef KDE_NO_DEBUG_OUTPUT
        void               dumpDebug() const  { }
#else
        void               dumpDebug() const  { d->dumpDebug(); }
#endif
        static int         currentCalendarVersion();
        static QByteArray  currentCalendarVersionString();
#ifdef USE_AKONADI
        static bool        convertKCalEvents(const KCalCore::Calendar::Ptr&, int calendarVersion, bool adjustSummerTime);
//        static bool        convertRepetitions(KCalCore::MemoryCalendar&);
        static List        ptrList(QList<KAEvent>&);
#else
        static bool        convertKCalEvents(KCal::CalendarLocal&, int calendarVersion, bool adjustSummerTime);
//        static bool        convertRepetitions(KCal::CalendarLocal&);
#endif

        // Methods to set and get global defaults
        static void setDefaultFont(const QFont& f)        { Private::mDefaultFont = f; }
        static void setStartOfDay(const QTime&);
        static void setHolidays(const KHolidays::HolidayRegion&);
        static void setWorkTime(const QBitArray& days, const QTime& start, const QTime& end);

    private:
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

        class Private : public KAAlarmEventBase, public QSharedData
        {
            public:
                enum ReminderType   // current active state of reminder
                {
                    NO_REMINDER,       // reminder is not due
                    ACTIVE_REMINDER,   // reminder is due
                    HIDDEN_REMINDER    // reminder-after is disabled due to main alarm being deferred past it
                };
                enum DeferType {
                    NO_DEFERRAL = 0,   // there is no deferred alarm
                    NORMAL_DEFERRAL,   // the main alarm, a recurrence or a repeat is deferred
                    REMINDER_DEFERRAL  // a reminder alarm is deferred
                };

                Private();
                Private(const KDateTime&, const QString& message, const QColor& bg, const QColor& fg,
                        const QFont& f, Action, int lateCancel, int flags, bool changesPending = false);
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
                void               set(const KDateTime&, const QString& message, const QColor& bg, const QColor& fg, const QFont&, Action, int lateCancel, int flags, bool changesPending = false);
                void               setAudioFile(const QString& filename, float volume, float fadeVolume, int fadeSeconds, bool allowEmptyFile);
                OccurType          setNextOccurrence(const KDateTime& preDateTime);
                void               setFirstRecurrence();
                void               setCategory(KAlarm::CalEvent::Type);
                void               setRepeatAtLogin(bool);
                void               setReminder(int minutes, bool onceOnly);
                void               activateReminderAfter(const DateTime& mainAlarmTime);
                bool               defer(const DateTime&, bool reminder, bool adjustRecurrence = false);
                void               cancelDefer();
#ifdef USE_AKONADI
                bool               setDisplaying(const Private&, KAAlarm::Type, Akonadi::Collection::Id, const KDateTime& dt, bool showEdit, bool showDefer);
                void               reinstateFromDisplaying(const KCalCore::ConstEventPtr&, Akonadi::Collection::Id&, bool& showEdit, bool& showDefer);
                void               setCommandError(CmdErrType t) const  { mCommandError = t; }
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
                DateTime           mainEndRepeatTime() const      { return mRepetition ? mRepetition.duration().end(mNextMainDateTime.kDateTime()) : mNextMainDateTime; }
                DateTime           deferralLimit(DeferLimitType* = 0) const;
                int                flags() const;
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
            private:
                void               copy(const Private&);
                bool               mayOccurDailyDuringWork(const KDateTime&) const;
                int                nextWorkRepetition(const KDateTime& pre) const;
                void               calcNextWorkingTime(const DateTime& nextTrigger) const;
                DateTime           nextWorkingTime() const;
                OccurType          nextRecurrence(const KDateTime& preDateTime, DateTime& result) const;
#ifdef USE_AKONADI
                void               setAudioAlarm(const KCalCore::Alarm::Ptr&) const;
                KCalCore::Alarm::Ptr initKCalAlarm(const KCalCore::Event::Ptr&, const DateTime&, const QStringList& types, KAAlarm::Type = KAAlarm::INVALID_ALARM) const;
                KCalCore::Alarm::Ptr initKCalAlarm(const KCalCore::Event::Ptr&, int startOffsetSecs, const QStringList& types, KAAlarm::Type = KAAlarm::INVALID_ALARM) const;
#else
                void               setAudioAlarm(KCal::Alarm*) const;
                KCal::Alarm*       initKCalAlarm(KCal::Event*, const DateTime&, const QStringList& types, KAAlarm::Type = KAAlarm::INVALID_ALARM) const;
                KCal::Alarm*       initKCalAlarm(KCal::Event*, int startOffsetSecs, const QStringList& types, KAAlarm::Type = KAAlarm::INVALID_ALARM) const;
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

                QString            mTemplateName;      // alarm template's name, or null if normal event
#ifdef USE_AKONADI
                QMap<QByteArray, QString> mCustomProperties;  // KCal::Event's non-KAlarm custom properties
                Akonadi::Item::Id  mItemId;            // Akonadi::Item ID for this event
                Akonadi::Collection::Id mCollectionId; // saved collection ID (not the collection the event is in)
#else
                QString            mResourceId;        // saved resource ID (not the resource the event is in)
#endif
                QString            mAudioFile;         // ATTACH: audio file to play
                QString            mPreAction;         // command to execute before alarm is displayed
                QString            mPostAction;        // command to execute after alarm window is closed
                DateTime           mStartDateTime;     // DTSTART and DTEND: start and end time for event
                KDateTime          mSaveDateTime;      // CREATED: date event was created, or saved in archive calendar
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
                int                mAlarmCount;        // number of alarms: count of !mMainExpired, mRepeatAtLogin, mDeferral, mReminderActive, mDisplaying
                DeferType          mDeferral;          // whether the alarm is an extra deferred/deferred-reminder alarm
                unsigned long      mKMailSerialNumber; // if email text, message's KMail serial number
                int                mTemplateAfterTime; // time not specified: use n minutes after default time, or -1 (applies to templates only)
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
                mutable const KHolidays::HolidayRegion*
                                   mExcludeHolidays;   // non-null to not trigger alarms on holidays (= mHolidays when trigger calculated)
                mutable int        mWorkTimeOnly;      // non-zero to trigger alarm only during working hours (= mWorkTimeIndex when trigger calculated)
                KAlarm::CalEvent::Type mCategory;      // event category (active, archived, template, ...)
#ifdef USE_AKONADI
                KAlarm::Calendar::Compat mCompatibility; // event's storage format compatibility
                bool               mReadOnly;          // event is read-only in its original calendar file
#endif
                bool               mCancelOnPreActErr; // cancel alarm if pre-alarm action fails
                bool               mDontShowPreActErr; // don't notify error if pre-alarm action fails
                bool               mConfirmAck;        // alarm acknowledgement requires confirmation by user
                bool               mCommandXterm;      // command alarm is to be executed in a terminal window
                bool               mCommandDisplay;    // command output is to be displayed in an alarm window
                bool               mEmailBcc;          // blind copy the email to the user
                bool               mBeep;              // whether to beep when the alarm is displayed
                bool               mRepeatSound;       // whether to repeat the sound file while the alarm is displayed
                bool               mSpeak;             // whether to speak the message when the alarm is displayed
                bool               mCopyToKOrganizer;  // KOrganizer should hold a copy of the event
                bool               mReminderOnceOnly;  // the reminder is output only for the first recurrence
                bool               mMainExpired;       // main alarm has expired (in which case a deferral alarm will exist)
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
                static const QByteArray HIDDEN_REMINDER_FLAG;
                static const QByteArray FONT_COLOUR_PROPERTY;
                static const QByteArray EMAIL_ID_PROPERTY;
                static const QByteArray VOLUME_PROPERTY;
                static const QByteArray SPEAK_PROPERTY;
                static const QByteArray CANCEL_ON_ERROR_PROPERTY;
                static const QByteArray DONT_SHOW_ERROR_PROPERTY;
                static const QString DISABLED_STATUS;
                static const QString DISP_DEFER;
                static const QString DISP_EDIT;
                static const QString CMD_ERROR_VALUE;
                static const QString CMD_ERROR_PRE_VALUE;
                static const QString CMD_ERROR_POST_VALUE;
                static const QString SC;
        };

        QSharedDataPointer<class Private> d;
};

Q_DECLARE_METATYPE(KAEvent)

#endif // KAEVENT_H

// vim: et sw=4:
