/*
 *  alarmevent.h  -  represents calendar alarms and events
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef KALARMEVENT_H
#define KALARMEVENT_H

#include <qcolor.h>
#include <qfont.h>

#include <libkcal/person.h>
#include <libkcal/event.h>
#include <libkcal/recurrence.h>

#include "datetime.h"

class AlarmCalendar;
struct AlarmData;


typedef KCal::Person  EmailAddress;
class EmailAddressList : public QValueList<KCal::Person>
{
	public:
		EmailAddressList() : QValueList<KCal::Person>() { }
		EmailAddressList(const QValueList<KCal::Person>& list)  { operator=(list); }
		EmailAddressList& operator=(const QValueList<KCal::Person>&);
		QString join(const QString& separator) const;
};


// Base class containing data common to KAAlarm and KAEvent
class KAAlarmEventBase
{
	public:
		~KAAlarmEventBase()  { }
		const QString&     cleanText() const           { return mText; }
		QString            message() const             { return (mActionType == T_MESSAGE || mActionType == T_EMAIL) ? mText : QString::null; }
		QString            fileName() const            { return (mActionType == T_FILE) ? mText : QString::null; }
		QString            command() const             { return (mActionType == T_COMMAND) ? mText : QString::null; }
		QString            emailFromKMail() const      { return mEmailFromKMail; }
		const EmailAddressList& emailAddresses() const { return mEmailAddresses; }
		QString            emailAddresses(const QString& sep) const  { return mEmailAddresses.join(sep); }
		const QString&     emailSubject() const        { return mEmailSubject; }
		const QStringList& emailAttachments() const    { return mEmailAttachments; }
		QString            emailAttachments(const QString& sep) const  { return mEmailAttachments.join(sep); }
		bool               emailBcc() const            { return mEmailBcc; }
		const QColor&      bgColour() const            { return mBgColour; }
		const QColor&      fgColour() const            { return mFgColour; }
		bool               defaultFont() const         { return mDefaultFont; }
		const QFont&       font() const;
		int                lateCancel() const          { return mLateCancel; }
		bool               autoClose() const           { return mAutoClose; }
		bool               confirmAck() const          { return mConfirmAck; }
		bool               repeatAtLogin() const       { return mRepeatAtLogin; }
		int                repeatCount() const         { return mRepeatCount; }
		int                repeatInterval() const      { return mRepeatInterval; }
		bool               displaying() const          { return mDisplaying; }
		bool               beep() const                { return mBeep; }
		int                flags() const;
#ifdef NDEBUG
		void               dumpDebug() const  { }
#else
		void               dumpDebug() const;
#endif

	protected:
		enum Type  { T_MESSAGE, T_FILE, T_COMMAND, T_AUDIO, T_EMAIL };

		KAAlarmEventBase() : mLateCancel(0), mAutoClose(false), mBeep(false), mRepeatAtLogin(false),
		                     mDisplaying(false), mEmailBcc(false), mConfirmAck(false) { }
		KAAlarmEventBase(const KAAlarmEventBase& rhs)             { copy(rhs); }
		KAAlarmEventBase& operator=(const KAAlarmEventBase& rhs)  { copy(rhs);  return *this; }
		void               copy(const KAAlarmEventBase&);
		void               set(int flags);

		QString            mEventID;          // UID: KCal::Event unique ID
		QString            mText;             // message text, file URL, command, email body [or audio file for KAAlarm]
		DateTime           mNextMainDateTime; // next time to display the alarm, excluding repetitions
		QColor             mBgColour;         // background colour of alarm message
		QColor             mFgColour;         // foreground colour of alarm message, or invalid for default
		QFont              mFont;             // font of alarm message (ignored if mDefaultFont true)
		QString            mEmailFromKMail;   // KMail identity for email 'From' field, or empty
		EmailAddressList   mEmailAddresses;   // ATTENDEE: addresses to send email to
		QString            mEmailSubject;     // SUMMARY: subject line of email
		QStringList        mEmailAttachments; // ATTACH: email attachment file names
		float              mSoundVolume;      // volume for sound file, or < 0 for unspecified
		Type               mActionType;       // alarm action type
		int                mRepeatCount;      // simple repetition count (excluding the first time)
		int                mRepeatInterval;   // simple repetition interval (minutes)
		int                mLateCancel;       // how many minutes late will cancel the alarm, or 0 for no cancellation
		bool               mAutoClose;        // whether to close the alarm window after the late-cancel period
		bool               mBeep;             // whether to beep when the alarm is displayed
		bool               mRepeatSound;      // whether to repeat the sound file while the alarm is displayed
		bool               mRepeatAtLogin;    // whether to repeat the alarm at every login
		bool               mDisplaying;       // whether the alarm is currently being displayed
		bool               mEmailBcc;         // blind copy the email to the user
		bool               mConfirmAck;       // alarm acknowledgement requires confirmation by user
		bool               mDefaultFont;      // use default message font, not mFont

	friend class AlarmData;
};


// KAAlarm corresponds to a single KCal::Alarm instance.
// A KAEvent may contain multiple KAAlarm's.
class KAAlarm : public KAAlarmEventBase
{
	public:
		// Define the basic KAAlaem action types
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
			// The following values may be used in combination as a bitmask 0x0E
			REMINDER__ALARM               = REMINDER_ALARM,
			TIMED_DEFERRAL_FLAG           = 0x08,  // deferral has a time; if clear, it is date-only
			DEFERRED_DATE__ALARM          = DEFERRED_ALARM,  // deferred alarm - date-only
			DEFERRED_TIME__ALARM          = DEFERRED_ALARM | TIMED_DEFERRAL_FLAG,
			DEFERRED_REMINDER_DATE__ALARM = REMINDER_ALARM | DEFERRED_ALARM,  // deferred early warning (date-only)
			DEFERRED_REMINDER_TIME__ALARM = REMINDER_ALARM | DEFERRED_ALARM | TIMED_DEFERRAL_FLAG,  // deferred early warning (date/time)
			// The following values must be greater than the preceding ones, to
			// ensure that in ordered processing they are processed afterwards.
			AT_LOGIN__ALARM               = AT_LOGIN_ALARM,
			DISPLAYING__ALARM             = DISPLAYING_ALARM,
			// The following values are for internal KAEvent use only
			AUDIO__ALARM                  = AUDIO_ALARM,
			PRE_ACTION__ALARM             = PRE_ACTION_ALARM,
			POST_ACTION__ALARM            = POST_ACTION_ALARM
		};

		KAAlarm()          : mType(INVALID__ALARM), mDeferred(false) { }
		KAAlarm(const KAAlarm&);
		~KAAlarm()  { }
		Action             action() const               { return (Action)mActionType; }
		bool               valid() const                { return mType != INVALID__ALARM; }
		Type               type() const                 { return static_cast<Type>(mType & ~TIMED_DEFERRAL_FLAG); }
		SubType            subType() const              { return mType; }
		const QString&     eventID() const              { return mEventID; }
		const DateTime&    dateTime() const             { return mNextMainDateTime; }
		QDate              date() const                 { return mNextMainDateTime.date(); }
		QTime              time() const                 { return mNextMainDateTime.time(); }
		QString            audioFile() const            { return (mActionType == T_AUDIO) && !mBeep ? mText : QString::null; }
		float              soundVolume() const          { return (mActionType == T_AUDIO) && mSoundVolume >= 0 && !mBeep && !mText.isEmpty() ? mSoundVolume : -1; }
		bool               repeatSound() const          { return (mActionType == T_AUDIO) && mRepeatSound && !mBeep && !mText.isEmpty(); }
		bool               reminder() const             { return mType == REMINDER__ALARM; }
		bool               deferred() const             { return mDeferred; }
		void               setTime(const DateTime& dt)  { mNextMainDateTime = dt; }
		void               setTime(const QDateTime& dt) { mNextMainDateTime = dt; }
		int                flags() const;
#ifdef NDEBUG
		void               dumpDebug() const  { }
		static const char* debugType(Type)   { return ""; }
#else
		void               dumpDebug() const;
		static const char* debugType(Type);
#endif

	private:
		void               set(int flags);

		SubType            mType;             // alarm type
		bool               mRecurs;           // there is a recurrence rule for the alarm
		bool               mDeferred;         // whether the alarm is an extra deferred/deferred-reminder alarm

	friend class KAEvent;
};


// KAEvent corresponds to a KCal::Event instance
class KAEvent : public KAAlarmEventBase
{
	public:
		enum            // flags for use in DCOP calls, etc.
		{
#ifdef OLD_DCOP
			// *** DON'T CHANGE THESE VALUES ***
			// because they are part of KAlarm's external DCOP interface.
			// (But it's alright to add new values.)
			LATE_CANCEL     = 0x01,    // cancel alarm if not triggered within a minute of its scheduled time
#endif
			BEEP            = 0x02,    // sound audible beep when alarm is displayed
			REPEAT_AT_LOGIN = 0x04,    // repeat alarm at every login
			ANY_TIME        = 0x08,    // only a date is specified for the alarm, not a time
			CONFIRM_ACK     = 0x10,    // closing the alarm message window requires confirmation prompt
			EMAIL_BCC       = 0x20,    // blind copy the email to the user
			DEFAULT_FONT    = 0x40,    // use default alarm message font
			REPEAT_SOUND    = 0x80,    // repeat sound file while alarm is displayed
			DISABLED        = 0x100,   // alarm is currently disabled
			AUTO_CLOSE      = 0x200,   // auto-close alarm window after late-cancel period
#ifdef OLD_DCOP
			// The following are read-only internal values, and may be changed
#else
			// The following are read-only internal values
#endif
			REMINDER        = 0x1000,
			DEFERRAL        = 0x2000,
			TIMED_FLAG      = 0x4000,
			DATE_DEFERRAL   = DEFERRAL,
			TIME_DEFERRAL   = DEFERRAL | TIMED_FLAG,
			DISPLAYING_     = 0x8000,
			READ_ONLY_FLAGS = 0xF000   // mask for all read-only internal values
		};
		enum RecurType
		{
#ifdef OLD_DCOP
			// *** DON'T CHANGE THESE VALUES ***
			// because they are part of KAlarm's external DCOP interface.
			// (But it's alright to add new values.)
#endif
			NO_RECUR    = 0,
			MINUTELY    = 1,
			DAILY       = 3,
			WEEKLY      = 4,
			MONTHLY_POS = 5,
			MONTHLY_DAY = 6,
			ANNUAL_DATE = 7,
			ANNUAL_POS  = 9,
			// The following values are not implemented in KAlarm
			ANNUAL_DAY  = 8
		};
		enum Status
		{
			ACTIVE,      // the event is currently active
			EXPIRED,     // the event has expired
			DISPLAYING,  // the event is currently being displayed
			TEMPLATE     // the event is an alarm template
		};
		enum Action
		{
			MESSAGE = T_MESSAGE,
			FILE    = T_FILE,
			COMMAND = T_COMMAND,
			EMAIL   = T_EMAIL
		};
		enum OccurType     // what type of occurrence is due
		{
			NO_OCCURRENCE               = 0,      // no occurrence is due
			FIRST_OCCURRENCE            = 0x01,   // the first occurrence (takes precedence over LAST_RECURRENCE)
			RECURRENCE_DATE             = 0x02,   // a recurrence with only a date, not a time
			RECURRENCE_DATE_TIME        = 0x03,   // a recurrence with a date and time
			LAST_RECURRENCE             = 0x04,   // the last recurrence
			OCCURRENCE_REPEAT = 0x10,    // (bitmask for a repetition of an occurrence)
			FIRST_OCCURRENCE_REPEAT     = OCCURRENCE_REPEAT | FIRST_OCCURRENCE,     // a repetition of the first occurrence
			RECURRENCE_DATE_REPEAT      = OCCURRENCE_REPEAT | RECURRENCE_DATE,      // a repetition of a date-only recurrence
			RECURRENCE_DATE_TIME_REPEAT = OCCURRENCE_REPEAT | RECURRENCE_DATE_TIME, // a repetition of a date/time recurrence
			LAST_RECURRENCE_REPEAT      = OCCURRENCE_REPEAT | LAST_RECURRENCE       // a repetition of the last recurrence
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

		KAEvent()          : mRevision(0), mRecurrence(0), mAlarmCount(0) { }
		KAEvent(const QDateTime& dt, const QString& message, const QColor& bg, const QColor& fg, const QFont& f, Action action, int lateCancel, int flags)
		                                        : mRecurrence(0) { set(dt, message, bg, fg, f, action, lateCancel, flags); }
		explicit KAEvent(const KCal::Event& e)  : mRecurrence(0) { set(e); }
		KAEvent(const KAEvent& e)               : KAAlarmEventBase(e), mRecurrence(0) { copy(e); }
		~KAEvent()         { delete mRecurrence; }
		KAEvent&           operator=(const KAEvent& e)       { if (&e != this) copy(e);  return *this; }
		void               set(const KCal::Event&);
		void               set(const QDate& d, const QString& message, const QColor& bg, const QColor& fg, const QFont& f, Action action, int lateCancel, int flags)
		                            { set(d, message, bg, fg, f, action, lateCancel, flags | ANY_TIME); }
		void               set(const QDateTime&, const QString& message, const QColor& bg, const QColor& fg, const QFont&, Action, int lateCancel, int flags);
		void               setMessage(const QDate& d, const QString& message, const QColor& bg, const QColor& fg, const QFont& f, int lateCancel, int flags)
		                            { set(d, message, bg, fg, f, MESSAGE, lateCancel, flags | ANY_TIME); }
		void               setMessage(const QDateTime& dt, const QString& message, const QColor& bg, const QColor& fg, const QFont& f, int lateCancel, int flags)
		                            { set(dt, message, bg, fg, f, MESSAGE, lateCancel, flags); }
		void               setFileName(const QDate& d, const QString& filename, const QColor& bg, const QColor& fg, const QFont& f, int lateCancel, int flags)
		                            { set(d, filename, bg, fg, f, FILE, lateCancel, flags | ANY_TIME); }
		void               setFileName(const QDateTime& dt, const QString& filename, const QColor& bg, const QColor& fg, const QFont& f, int lateCancel, int flags)
		                            { set(dt, filename, bg, fg, f, FILE, lateCancel, flags); }
		void               setCommand(const QDate& d, const QString& command, int lateCancel, int flags)
		                            { set(d, command, QColor(), QColor(), QFont(), COMMAND, lateCancel, flags | ANY_TIME); }
		void               setCommand(const QDateTime& dt, const QString& command, int lateCancel, int flags)
		                            { set(dt, command, QColor(), QColor(), QFont(), COMMAND, lateCancel, flags); }
		void               setEmail(const QDate&, const QString& from, const EmailAddressList&, const QString& subject,
		                            const QString& message, const QStringList& attachments, int lateCancel, int flags);
		void               setEmail(const QDateTime&, const QString& from, const EmailAddressList&, const QString& subject,
		                            const QString& message, const QStringList& attachments, int lateCancel, int flags);
		void               setEmail(const QString& from, const EmailAddressList&, const QString& subject, const QStringList& attachments);
		void               setAudioFile(const QString& filename, float volume) { mAudioFile = filename;  mSoundVolume = volume;  mUpdated = true; }
		void               setTemplate(const QString& name, bool defaultTime)  { mTemplateName = name; mTemplateDefaultTime = defaultTime;  mUpdated = true; }
		void               setActions(const QString& pre, const QString& post) { mPreAction = pre;  mPostAction = post;  mUpdated = true; }
		OccurType          setNextOccurrence(const QDateTime& preDateTime, bool includeRepetitions = false);
		void               setFirstRecurrence();
		void               setEventID(const QString& id)                     { mEventID = id;  mUpdated = true; }
		void               adjustStartDate(const QDate&);
		void               setDate(const QDate& d)                           { mNextMainDateTime.set(d);  mUpdated = true; }
		void               setTime(const QDateTime& dt)                      { mNextMainDateTime.set(dt);  mUpdated = true; }
		void               setSaveDateTime(const QDateTime& dt)              { mSaveDateTime = dt;  mUpdated = true; }
		void               setLateCancel(int lc)                             { mLateCancel = lc;  mUpdated = true; }
		void               setAutoClose(bool ac)                             { mAutoClose = ac;  mUpdated = true; }
		void               setRepeatAtLogin(bool rl)                         { mRepeatAtLogin = rl;  mUpdated = true; }
		void               set(int flags);
		void               setUid(Status s)                                  { mEventID = uid(mEventID, s);  mUpdated = true; }
		void               setReminder(int minutes, bool onceOnly);
		bool               defer(const DateTime&, bool reminder, bool adjustRecurrence = false);
		void               cancelDefer();
		void               cancelCancelledDeferral();
		bool               setDisplaying(const KAEvent&, KAAlarm::Type, const QDateTime&);
		void               reinstateFromDisplaying(const KAEvent& dispEvent);
		void               setArchive()                                      { mArchive = true;  mUpdated = true; }
		void               setEnabled(bool enable)                           { mEnabled = enable;  mUpdated = true; }
		void               setUpdated()                                      { mUpdated = true; }
		void               clearUpdated() const                              { mUpdated = false; }
		void               removeExpiredAlarm(KAAlarm::Type);
		void               incrementRevision()                               { ++mRevision;  mUpdated = true; }

		KCal::Event*       event() const;    // convert to new Event
		bool               isTemplate() const             { return !mTemplateName.isEmpty(); }
		const QString&     templateName() const           { return mTemplateName; }
		bool               usingDefaultTime() const       { return mTemplateDefaultTime; }
		KAAlarm            alarm(KAAlarm::Type) const;
		KAAlarm            firstAlarm() const;
		KAAlarm            nextAlarm(const KAAlarm& al) const  { return nextAlarm(al.type()); }
		KAAlarm            nextAlarm(KAAlarm::Type) const;
		KAAlarm            convertDisplayingAlarm() const;
		bool               updateKCalEvent(KCal::Event&, bool checkUid = true, bool original = false, bool cancelCancelledDefer = false) const;
		Action             action() const                 { return (Action)mActionType; }
		bool               displayAction() const          { return mActionType == T_MESSAGE || mActionType == T_FILE; }
		const QString&     id() const                     { return mEventID; }
		bool               valid() const                  { return mAlarmCount  &&  (mAlarmCount != 1 || !mRepeatAtLogin); }
		int                alarmCount() const             { return mAlarmCount; }
		const DateTime&    startDateTime() const          { return mStartDateTime; }
		const DateTime&    mainDateTime() const           { return mNextMainDateTime; }
		QDate              mainDate() const               { return mNextMainDateTime.date(); }
		QTime              mainTime() const               { return mNextMainDateTime.time(); }
		DateTime           mainEndRepeatTime() const      { return mRepeatCount ? mNextMainDateTime.addSecs(mRepeatCount * mRepeatInterval * 60) : mNextMainDateTime; }
		int                reminder() const               { return mReminderMinutes; }
		bool               reminderOnceOnly() const       { return mReminderOnceOnly; }
		bool               reminderDeferral() const       { return mDeferral == REMINDER_DEFERRAL; }
		int                reminderArchived() const       { return mArchiveReminderMinutes; }
		DateTime           deferDateTime() const          { return mDeferralTime; }
		DateTime           deferralLimit(DeferLimitType* = 0) const;
		DateTime           nextDateTime(bool includeReminders = true) const;
		const QString&     messageFileOrCommand() const   { return mText; }
		const QString&     audioFile() const              { return mAudioFile; }
		float              soundVolume() const            { return mSoundVolume >= 0  &&  !mAudioFile.isEmpty() ? mSoundVolume : -1; }
		bool               repeatSound() const            { return mRepeatSound  &&  !mAudioFile.isEmpty(); }
		const QString&     preAction() const              { return mPreAction; }
		const QString&     postAction() const             { return mPostAction; }
		bool               recurs() const                 { return checkRecur() != NO_RECUR; }
		RecurType          recurType() const              { return checkRecur(); }
		KCal::Recurrence*  recurrence() const             { return mRecurrence; }
		bool               recursFeb29() const            { return mRecursFeb29; }
		int                recurInterval() const;    // recurrence period in units of the recurrence period type (minutes, days, etc)
		int                longestRecurrenceInterval() const    { return mRecurrence ? longestRecurrenceInterval(*mRecurrence) : 0; }
		static int         longestRecurrenceInterval(const KCal::Recurrence&);   // longest interval between any recurrences, in minutes
		QString            recurrenceText(bool brief = false) const;
		QString            repetitionText(bool brief = false) const;
		int                remainingRecurrences() const   { return mRemainingRecurrences; }
		bool               occursAfter(const QDateTime& preDateTime, bool includeRepetitions) const;
		OccurType          nextOccurrence(const QDateTime& preDateTime, DateTime& result, OccurOption = IGNORE_REPETITION) const;
		OccurType          previousOccurrence(const QDateTime& afterDateTime, DateTime& result, bool includeRepetitions = false) const;
		const KCal::DateList& exceptionDates() const      { return mExceptionDates; }
		const KCal::DateTimeList& exceptionDateTimes() const { return mExceptionDateTimes; }
		int                flags() const;
		bool               deferred() const               { return mDeferral > 0; }
		bool               toBeArchived() const           { return mArchive; }
		bool               enabled() const                { return mEnabled; }
		bool               updated() const                { return mUpdated; }
		bool               mainExpired() const            { return mMainExpired; }
		bool               expired() const                { return mDisplaying && mMainExpired  ||  uidStatus(mEventID) == EXPIRED; }
		Status             uidStatus() const              { return uidStatus(mEventID); }
		static Status      uidStatus(const QString& uid);
		static QString     uid(const QString& id, Status);
		static KAEvent     findTemplateName(AlarmCalendar&, const QString& name);

		struct MonthPos
		{
			MonthPos() : days(7) { }
			int        weeknum;     // week in month, or < 0 to count from end of month
			QBitArray  days;        // days in week
		};
		bool               setRepetition(int interval, int count);
		void               setExceptionDates(const KCal::DateList& d)      { mExceptionDates = d;  mUpdated = true; }
		void               setExceptionDates(const KCal::DateTimeList& dt) { mExceptionDateTimes = dt;  mUpdated = true; }
		void               setNoRecur()            { initRecur(); }
		void               setRecurrence(const KCal::Recurrence&);
		void               setRecurMinutely(int freq, int count)
		                                           { setRecurMinutely(freq, count, QDateTime()); }
		void               setRecurMinutely(int freq, const QDateTime& end)
		                                           { setRecurMinutely(freq, 0, end); }
		void               setRecurDaily(int freq, int count)
		                                           { setRecurDaily(freq, count, QDate()); }
		void               setRecurDaily(int freq, const QDate& end)
		                                           { setRecurDaily(freq, 0, end); }
		void               setRecurWeekly(int freq, const QBitArray& days, int count)
		                                           { setRecurWeekly(freq, days, count, QDate()); }
		void               setRecurWeekly(int freq, const QBitArray& days, const QDate& end)
		                                           { setRecurWeekly(freq, days, 0, end); }
		void               setRecurMonthlyByDate(int freq, const QValueList<int>& days, int count)
		                                           { setRecurMonthlyByDate(freq, days, count, QDate()); }
		void               setRecurMonthlyByDate(int freq, const QValueList<int>& days, const QDate& end)
		                                           { setRecurMonthlyByDate(freq, days, 0, end); }
		void               setRecurMonthlyByPos(int freq, const QValueList<MonthPos>& mp, int count)
		                                           { setRecurMonthlyByPos(freq, mp, count, QDate()); }
		void               setRecurMonthlyByPos(int freq, const QValueList<MonthPos>& mp, const QDate& end)
		                                           { setRecurMonthlyByPos(freq, mp, 0, end); }
		void               setRecurMonthlyByPos(int freq, const QPtrList<KCal::Recurrence::rMonthPos>& mp, int count)
		                                           { setRecurMonthlyByPos(freq, mp, count, QDate()); }
		void               setRecurMonthlyByPos(int freq, const QPtrList<KCal::Recurrence::rMonthPos>& mp, const QDate& end)
		                                           { setRecurMonthlyByPos(freq, mp, 0, end); }
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, int day, int count)
		                                           { setRecurAnnualByDate(freq, months, day, -1, count, QDate()); }
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, int day, const QDate& end)
		                                           { setRecurAnnualByDate(freq, months, day, -1, 0, end); }
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, int day, bool feb29, int count)
		                                           { setRecurAnnualByDate(freq, months, day, feb29, count, QDate()); }
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, int day, bool feb29, const QDate& end)
		                                           { setRecurAnnualByDate(freq, months, day, feb29, 0, end); }
		void               setRecurAnnualByPos(int freq, const QValueList<MonthPos>& mp, const QValueList<int>& months, int count)
		                                           { setRecurAnnualByPos(freq, mp, months, count, QDate()); }
		void               setRecurAnnualByPos(int freq, const QValueList<MonthPos>& mp, const QValueList<int>& months, const QDate& end)
		                                           { setRecurAnnualByPos(freq, mp, months, 0, end); }
		void               setRecurAnnualByDay(int freq, const QValueList<int>& days, int count)
		                                           { setRecurAnnualByDay(freq, days, count, QDate()); }
		void               setRecurAnnualByDay(int freq, const QValueList<int>& days, const QDate& end)
		                                           { setRecurAnnualByDay(freq, days, 0, end); }

		void               setRecurMinutely(int freq, int count, const QDateTime& end)
		                                           { if (initRecur(end.date(), count))  setRecurMinutely(*mRecurrence, freq, count, end); }
		void               setRecurDaily(int freq, int count, const QDate& end)
		                                           { if (initRecur(end, count))  setRecurDaily(*mRecurrence, freq, count, end); }
		void               setRecurWeekly(int freq, const QBitArray& days, int count, const QDate& end)
		                                           { if (initRecur(end, count))  setRecurWeekly(*mRecurrence, freq, days, count, end); }
		void               setRecurMonthlyByDate(int freq, const QValueList<int>& days, int count, const QDate& end)
		                                           { if (initRecur(end, count))  setRecurMonthlyByDate(*mRecurrence, freq, days, count, end); }
		void               setRecurMonthlyByDate(int freq, const QPtrList<int>& days, int count, const QDate& end)
		                                           { if (initRecur(end, count))  setRecurMonthlyByDate(*mRecurrence, freq, days, count, end); }
		void               setRecurMonthlyByPos(int freq, const QValueList<MonthPos>& pos, int count, const QDate& end)
		                                           { if (initRecur(end, count))  setRecurMonthlyByPos(*mRecurrence, freq, pos, count, end); }
		void               setRecurMonthlyByPos(int freq, const QPtrList<KCal::Recurrence::rMonthPos>& pos, int count, const QDate& end)
		                                           { if (initRecur(end, count))  setRecurMonthlyByPos(*mRecurrence, freq, pos, count, end); }
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, int day, bool feb29, int count, const QDate& end)
		                                           { if (initRecur(end, count, feb29))  setRecurAnnualByDate(*mRecurrence, freq, months, day, count, end); }
		void               setRecurAnnualByDate(int freq, const QPtrList<int>& months, int day, bool feb29, int count, const QDate& end)
		                                           { if (initRecur(end, count, feb29))  setRecurAnnualByDate(*mRecurrence, freq, months, day, count, end); }
		void               setRecurAnnualByPos(int freq, const QValueList<MonthPos>& pos, const QValueList<int>& months, int count, const QDate& end)
		                                           { if (initRecur(end, count))  setRecurAnnualByPos(*mRecurrence, freq, pos, months, count, end); }
		void               setRecurAnnualByPos(int freq, const QPtrList<KCal::Recurrence::rMonthPos>& pos, const QPtrList<int>& months, int count, const QDate& end)
		                                           { if (initRecur(end, count))  setRecurAnnualByPos(*mRecurrence, freq, pos, months, count, end); }
		void               setRecurAnnualByDay(int freq, const QValueList<int>& days, int count, const QDate& end)
		                                           { if (initRecur(end, count))  setRecurAnnualByDay(*mRecurrence, freq, days, count, end); }
		void               setRecurAnnualByDay(int freq, const QPtrList<int>& days, int count, const QDate& end)
		                                           { if (initRecur(end, count))  setRecurAnnualByDay(*mRecurrence, freq, days, count, end); }

		static bool        setRecurMinutely(KCal::Recurrence&, int freq, int count, const QDateTime& end);
		static bool        setRecurDaily(KCal::Recurrence&, int freq, int count, const QDate& end);
		static bool        setRecurWeekly(KCal::Recurrence&, int freq, const QBitArray& days, int count, const QDate& end);
		static bool        setRecurMonthlyByDate(KCal::Recurrence&, int freq, const QValueList<int>& days, int count, const QDate& end);
		static bool        setRecurMonthlyByDate(KCal::Recurrence&, int freq, const QPtrList<int>& days, int count, const QDate& end);
		static bool        setRecurMonthlyByPos(KCal::Recurrence&, int freq, const QValueList<MonthPos>&, int count, const QDate& end);
		static bool        setRecurMonthlyByPos(KCal::Recurrence&, int freq, const QPtrList<KCal::Recurrence::rMonthPos>&, int count, const QDate& end);
		static bool        setRecurAnnualByDate(KCal::Recurrence&, int freq, const QValueList<int>& months, int day, int count, const QDate& end);
		static bool        setRecurAnnualByDate(KCal::Recurrence&, int freq, const QPtrList<int>& months, int day, int count, const QDate& end);
		static bool        setRecurAnnualByPos(KCal::Recurrence&, int freq, const QValueList<MonthPos>&, const QValueList<int>& months, int count, const QDate& end);
		static bool        setRecurAnnualByPos(KCal::Recurrence&, int freq, const QPtrList<KCal::Recurrence::rMonthPos>&, const QPtrList<int>& months, int count, const QDate& end);
		static bool        setRecurAnnualByDay(KCal::Recurrence&, int freq, const QValueList<int>& days, int count, const QDate& end);
		static bool        setRecurAnnualByDay(KCal::Recurrence&, int freq, const QPtrList<int>& days, int count, const QDate& end);
		static bool        setRecurrence(KCal::Recurrence&, RecurType, int repeatInterval, int repeatCount, const DateTime& start, const QDateTime& end);
#ifdef NDEBUG
		void               dumpDebug() const  { }
#else
		void               dumpDebug() const;
#endif
		static void        setFeb29RecurType();
		static bool        adjustStartOfDay(const KCal::Event::List&);
		static void        convertKCalEvents(AlarmCalendar&);

	private:
		enum DeferType {
			CANCEL_DEFERRAL = -1,   // there was a deferred alarm, but it has been cancelled
			NO_DEFERRAL = 0,        // there is no deferred alarm
			NORMAL_DEFERRAL,        // the main alarm, a recurrence or a repeat is deferred
			REMINDER_DEFERRAL       // a reminder alarm is deferred
		};

		void               copy(const KAEvent&);
		bool               initRecur(const QDate& end = QDate(), int count = 0, bool feb29 = false);
		RecurType          checkRecur() const;
		OccurType          nextRecurrence(const QDateTime& preDateTime, DateTime& result, int& remainingCount) const;
		OccurType          previousRecurrence(const QDateTime& afterDateTime, DateTime& result) const;
		KCal::Alarm*       initKcalAlarm(KCal::Event&, const DateTime&, const QStringList& types, KAAlarm::Type = KAAlarm::INVALID_ALARM) const;
		static void        readAlarms(const KCal::Event&, void* alarmMap);
		static void        readAlarm(const KCal::Alarm&, AlarmData&);
		inline void        set_deferral(DeferType);
		inline void        set_reminder(int minutes);
		inline void        set_archiveReminder();

		QString            mTemplateName;     // alarm template's name, or null if normal event
		QString            mAudioFile;        // ATTACH: audio file to play
		QString            mPreAction;        // command to execute before alarm is displayed
		QString            mPostAction;       // command to execute after alarm window is closed
		DateTime           mStartDateTime;    // DTSTART and DTEND: start and end time for event
		QDateTime          mSaveDateTime;     // CREATED: date event was created, or saved in expired calendar
		QDateTime          mAtLoginDateTime;  // repeat-at-login time
		DateTime           mDeferralTime;     // extra time to trigger alarm (if alarm or reminder deferred)
		DateTime           mDisplayingTime;   // date/time shown in the alarm currently being displayed
		int                mDisplayingFlags;  // type of alarm which is currently being displayed
		int                mReminderMinutes;  // how long in advance reminder is to be, or 0 if none
		int                mArchiveReminderMinutes;  // original reminder period if now expired, or 0 if none
		int                mRevision;         // SEQUENCE: revision number of the original alarm, or 0
		KCal::Recurrence*  mRecurrence;       // RECUR: recurrence specification, or 0 if none
		int                mRemainingRecurrences; // remaining number of alarm recurrences including initial time, -1 to repeat indefinitely
		KCal::DateList     mExceptionDates;   // list of dates to exclude from the recurrence
		KCal::DateTimeList mExceptionDateTimes; // list of date/times to exclude from the recurrence
		int                mAlarmCount;       // number of alarms: count of !mMainExpired, mRepeatAtLogin, mDeferral, mReminderMinutes, mDisplaying
		DeferType          mDeferral;         // whether the alarm is an extra deferred/deferred-reminder alarm
		bool               mTemplateDefaultTime; // time not specified: use default time (applies to templates only)
		bool               mRecursFeb29;      // the recurrence is yearly on February 29th
		bool               mReminderOnceOnly; // the reminder is output only for the first recurrence
		bool               mMainExpired;      // main alarm has expired (in which case a deferral alarm will exist)
		bool               mArchiveRepeatAtLogin; // if now expired, original event was repeat-at-login
		bool               mArchive;          // event has triggered in the past, so archive it when closed
		bool               mEnabled;          // false if event is disabled
		mutable bool       mUpdated;          // event has been updated but not written to calendar file
};

#endif // KALARMEVENT_H
