/*
 *  msgevent.h  -  the event object for messages
 *  Program:  kalarm
 *  (C) 2001 - 2003 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#ifndef KALARMEVENT_H
#define KALARMEVENT_H

#include <qcolor.h>

#include <libkcal/person.h>
#include <libkcal/event.h>
#include <libkcal/recurrence.h>

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


// Base class containing data common to KAlarmAlarm and KAlarmEvent
class KAAlarmEventBase
{
	public:
		~KAAlarmEventBase()  { }
		const QString&     cleanText() const          { return mText; }
		QString            message() const            { return (mActionType == T_MESSAGE || mActionType == T_EMAIL) ? mText : QString::null; }
		QString            fileName() const           { return (mActionType == T_FILE) ? mText : QString::null; }
		QString            command() const            { return (mActionType == T_COMMAND) ? mText : QString::null; }
		const EmailAddressList& emailAddresses() const { return mEmailAddresses; }
		QString            emailAddresses(const QString& sep) const  { return mEmailAddresses.join(sep); }
		const QString&     emailSubject() const       { return mEmailSubject; }
		const QStringList& emailAttachments() const   { return mEmailAttachments; }
		QString            emailAttachments(const QString& sep) const  { return mEmailAttachments.join(sep); }
		bool               emailBcc() const           { return mEmailBcc; }
		const QColor&      bgColour() const           { return mBgColour; }
		bool               defaultFont() const        { return mDefaultFont; }
		const QFont&       font() const;
		bool               confirmAck() const         { return mConfirmAck; }
		bool               lateCancel() const         { return mLateCancel; }
		bool               repeatAtLogin() const      { return mRepeatAtLogin; }
		bool               deferred() const           { return mDeferral; }
		bool               displaying() const         { return mDisplaying; }
		bool               beep() const               { return mBeep; }
		int                flags() const;
#ifdef NDEBUG
		void               dumpDebug() const  { }
#else
		void               dumpDebug() const;
#endif

	protected:
		enum Type  { T_MESSAGE, T_FILE, T_COMMAND, T_AUDIO, T_EMAIL };

		KAAlarmEventBase() : mBeep(false), mRepeatAtLogin(false), mDeferral(false), mDisplaying(false), mLateCancel(false), mEmailBcc(false), mConfirmAck(false) { }
		KAAlarmEventBase(const KAAlarmEventBase& rhs)             { copy(rhs); }
		KAAlarmEventBase& operator=(const KAAlarmEventBase& rhs)  { copy(rhs);  return *this; }
		void               copy(const KAAlarmEventBase&);
		void               set(int flags);

		QString            mEventID;          // UID: KCal::Event unique ID
		QString            mText;             // message text, file URL, command, email body [or audio file for KAlarmAlarm]
		QDateTime          mDateTime;         // next time to display the alarm
		QColor             mBgColour;         // background colour of alarm message
		QFont              mFont;             // font of alarm message (ignored if mDefaultFont true)
		EmailAddressList   mEmailAddresses;   // ATTENDEE: addresses to send email to
		QString            mEmailSubject;     // SUMMARY: subject line of email
		QStringList        mEmailAttachments; // ATTACH: email attachment file names
		Type               mActionType;       // alarm action type
		bool               mBeep;             // whether to beep when the alarm is displayed
		bool               mRepeatAtLogin;    // whether to repeat the alarm at every login
		bool               mDeferral;         // whether the alarm is an extra deferred/deferred-reminder alarm
		bool               mDisplaying;       // whether the alarm is currently being displayed
		bool               mLateCancel;       // whether to cancel the alarm if it can't be displayed on time
		bool               mEmailBcc;         // blind copy the email to the user
		bool               mConfirmAck;       // alarm acknowledgement requires confirmation by user
		bool               mDefaultFont;      // use default message font, not mFont

	friend class AlarmData;
};


// KAlarmAlarm corresponds to a single KCal::Alarm instance
class KAlarmAlarm : public KAAlarmEventBase
{
	public:
		enum Action
		{
			MESSAGE = T_MESSAGE,
			FILE    = T_FILE,
			COMMAND = T_COMMAND,
			EMAIL   = T_EMAIL,
			AUDIO   = T_AUDIO
		};
		enum Type
		{
			INVALID_ALARM    = 0,     // not an alarm
			MAIN_ALARM       = 1,     // THE real alarm. Must be the first in the enumeration.
			REMINDER_ALARM   = 0x02,  // reminder in advance of main alarm
			DEFERRAL_ALARM   = 0x04,  // deferred alarm
			REMINDER_DEFERRAL_ALARM = REMINDER_ALARM | DEFERRAL_ALARM,  // deferred early warning
			// The following values must be greater than the preceding ones, to
			// ensure that in ordered processing they are processed afterwards.
			AT_LOGIN_ALARM   = 0x10,  // additional repeat-at-login trigger
			DISPLAYING_ALARM = 0x20,  // copy of the alarm currently being displayed
			// The following values are for internal KAlarmEvent use only
			AUDIO_ALARM      = 0x30   // sound to play when displaying the alarm.
		};

		KAlarmAlarm()      : mType(INVALID_ALARM) { }
		KAlarmAlarm(const KAlarmAlarm&);
		~KAlarmAlarm()  { }
		Action            action() const               { return (Action)mActionType; }
		bool              valid() const                { return mType != INVALID_ALARM; }
		Type              type() const                 { return mType; }
		void              setType(Type t)              { mType = t; }
		const QString&    eventID() const              { return mEventID; }
		const QDateTime&  dateTime() const             { return mDateTime; }
		QDate             date() const                 { return mDateTime.date(); }
		QTime             time() const                 { return mDateTime.time(); }
		QString           audioFile() const            { return (mActionType == T_AUDIO) ? mText : QString::null; }
		bool              reminder() const             { return mType == REMINDER_ALARM; }
		void              setTime(const QDateTime& dt) { mDateTime = dt; }
#ifdef NDEBUG
		void              dumpDebug() const  { }
#else
		void              dumpDebug() const;
#endif

	private:
		Type              mType;             // alarm type
		bool              mRecurs;           // there is a recurrence rule for the alarm

	friend class KAlarmEvent;
};


// KAlarmEvent corresponds to a KCal::Event instance
class KAlarmEvent : public KAAlarmEventBase
{
	public:
		enum            // flags for use in DCOP calls, etc.
		{
			// *** DON'T CHANGE THESE VALUES ***
			// because they are part of KAlarm's external DCOP interface.
			// (But it's alright to add new values.)
			LATE_CANCEL     = 0x01,    // cancel alarm if not triggered within a minute of its scheduled time
			BEEP            = 0x02,    // sound audible beep when alarm is displayed
			REPEAT_AT_LOGIN = 0x04,    // repeat alarm at every login
			ANY_TIME        = 0x08,    // only a date is specified for the alarm, not a time
			CONFIRM_ACK     = 0x10,    // closing the alarm message window requires confirmation prompt
			EMAIL_BCC       = 0x20,    // blind copy the email to the user
			DEFAULT_FONT    = 0x40,    // use default alarm message font
			// The following are read-only internal values, and may be changed
			REMINDER        = 0x100,
			DEFERRAL        = 0x200,
			DISPLAYING_     = 0x400,
			READ_ONLY_FLAGS = 0xF00    // mask for all read-only internal values
		};
		enum RecurType
		{
			// *** DON'T CHANGE THESE VALUES ***
			// because they are part of KAlarm's external DCOP interface.
			// (But it's alright to add new values.)
			NO_RECUR    = KCal::Recurrence::rNone,
			MINUTELY    = KCal::Recurrence::rMinutely,
			DAILY       = KCal::Recurrence::rDaily,
			WEEKLY      = KCal::Recurrence::rWeekly,
			MONTHLY_DAY = KCal::Recurrence::rMonthlyDay,
			MONTHLY_POS = KCal::Recurrence::rMonthlyPos,
			ANNUAL_DATE = KCal::Recurrence::rYearlyMonth,
			ANNUAL_POS  = KCal::Recurrence::rYearlyPos,
			// The following values are not implemented in KAlarm
			ANNUAL_DAY  = KCal::Recurrence::rYearlyDay
		};
		enum Status
		{
			ACTIVE,      // the event is currently active
			EXPIRED,     // the event has expired
			DISPLAYING   // the event is currently being displayed
		};
		enum Action
		{
			MESSAGE = T_MESSAGE,
			FILE    = T_FILE,
			COMMAND = T_COMMAND,
			EMAIL   = T_EMAIL
		};
		enum OccurType
		{
			NO_OCCURRENCE,        // no occurrence is due
			FIRST_OCCURRENCE,     // the first occurrence is due (takes precedence over LAST_OCCURRENCE)
			RECURRENCE_DATE,      // a recurrence is due with only a date, not a time
			RECURRENCE_DATE_TIME, // a recurrence is due with a date and time
			LAST_OCCURRENCE       // the last occurrence is due
		};

		KAlarmEvent()      : mRevision(0), mRecurrence(0), mAlarmCount(0) { }
		KAlarmEvent(const QDateTime& dt, const QString& message, const QColor& c, const QFont& f, Action action, int flags)
		                                            : mRecurrence(0) { set(dt, message, c, f, action, flags); }
		explicit KAlarmEvent(const KCal::Event& e)  : mRecurrence(0) { set(e); }
		KAlarmEvent(const KAlarmEvent& e)           : KAAlarmEventBase(e), mRecurrence(0) { copy(e); }
		~KAlarmEvent()     { delete mRecurrence; }
		KAlarmEvent&       operator=(const KAlarmEvent& e)   { if (&e != this) copy(e);  return *this; }
		void               set(const KCal::Event&);
		void               set(const QDate& d, const QString& message, const QColor& c, const QFont& f, Action action, int flags)
		                            { set(d, message, c, f, action, flags | ANY_TIME); }
		void               set(const QDateTime&, const QString& message, const QColor&, const QFont&, Action, int flags);
		void               setMessage(const QDate& d, const QString& message, const QColor& c, const QFont& f, int flags)
		                            { set(d, message, c, f, MESSAGE, flags | ANY_TIME); }
		void               setMessage(const QDateTime& dt, const QString& message, const QColor& c, const QFont& f, int flags)
		                            { set(dt, message, c, f, MESSAGE, flags); }
		void               setFileName(const QDate& d, const QString& filename, const QColor& c, const QFont& f, int flags)
		                            { set(d, filename, c, f, FILE, flags | ANY_TIME); }
		void               setFileName(const QDateTime& dt, const QString& filename, const QColor& c, const QFont& f, int flags)
		                            { set(dt, filename, c, f, FILE, flags); }
		void               setCommand(const QDate& d, const QString& command, int flags)
		                            { set(d, command, QColor(), QFont(), COMMAND, flags | ANY_TIME); }
		void               setCommand(const QDateTime& dt, const QString& command, int flags)
		                            { set(dt, command, QColor(), QFont(), COMMAND, flags); }
		void               setEmail(const QDate&, const EmailAddressList&, const QString& subject,
		                            const QString& message, const QStringList& attachments, int flags);
		void               setEmail(const QDateTime&, const EmailAddressList&, const QString& subject,
		                            const QString& message, const QStringList& attachments, int flags);
		void               setEmail(const EmailAddressList&, const QString& subject, const QStringList& attachments);
		void               setAudioFile(const QString& filename)             { mAudioFile = filename;  mUpdated = true; }
		OccurType          setNextOccurrence(const QDateTime& preDateTime);
		void               setFirstRecurrence();
		void               setEventID(const QString& id)                     { mEventID = id;  mUpdated = true; }
		void               setDate(const QDate& d)                           { mDateTime = d; mAnyTime = true;  mUpdated = true; }
		void               setTime(const QDateTime& dt)                      { mDateTime = dt; mAnyTime = false;  mUpdated = true; }
		void               setEndTime(const QDateTime& dt)                   { mEndDateTime = dt;  mUpdated = true; }
		void               setLateCancel(bool lc)                            { mLateCancel = lc;  mUpdated = true; }
		void               set(int flags);
		void               setUid(Status s)                                  { mEventID = uid(mEventID, s);  mUpdated = true; }
		void               setReminder(int minutes)                          { mReminderMinutes = minutes;  mUpdated = true; }
		void               defer(const QDateTime&, bool reminder, bool adjustRecurrence = false);
		void               cancelDefer();
		bool               setDisplaying(const KAlarmEvent&, KAlarmAlarm::Type, const QDateTime&);
		void               reinstateFromDisplaying(const KAlarmEvent& dispEvent);
		void               setArchive()                                      { mArchive = true;  mUpdated = true;}
		void               setUpdated()                                      { mUpdated = true; }
		void               clearUpdated() const                              { mUpdated = false; }
		void               removeExpiredAlarm(KAlarmAlarm::Type);
		void               incrementRevision()                               { ++mRevision;  mUpdated = true; }

		KCal::Event*       event() const;    // convert to new Event
		KAlarmAlarm        alarm(KAlarmAlarm::Type) const;
		KAlarmAlarm        firstAlarm() const;
		KAlarmAlarm        nextAlarm(const KAlarmAlarm& al) const            { return nextAlarm(al.type()); }
		KAlarmAlarm        nextAlarm(KAlarmAlarm::Type) const;
		KAlarmAlarm        convertDisplayingAlarm() const;
		bool               updateKCalEvent(KCal::Event&, bool checkUid = true, bool original = false) const;
		Action             action() const                 { return (Action)mActionType; }
		const QString&     id() const                     { return mEventID; }
		bool               valid() const                  { return mAlarmCount  &&  (mAlarmCount != 1 || !mRepeatAtLogin); }
		int                alarmCount() const             { return mAlarmCount; }
		const QDateTime&   startDateTime() const          { return mStartDateTime; }
		const QDateTime&   endDateTime() const            { return mEndDateTime; }
		const QDateTime&   mainDateTime() const           { return mDateTime; }
		QDate              mainDate() const               { return mDateTime.date(); }
		QTime              mainTime() const               { return mDateTime.time(); }
		bool               anyTime() const                { return mAnyTime; }
		int                reminder() const               { return mReminderMinutes; }
		int                reminderDeferral() const       { return mReminderDeferralMinutes; }
		int                reminderArchived() const       { return mReminderArchiveMinutes; }
		int                nextReminder() const           { return mReminderDeferralMinutes ? mReminderDeferralMinutes : mReminderMinutes; }
		QDateTime          deferDateTime() const;
		QDateTime          nextDateTime() const;
		const QString&     messageFileOrCommand() const   { return mText; }
		const QString&     audioFile() const              { return mAudioFile; }
		bool               recurs() const                 { return checkRecur() != NO_RECUR; }
		RecurType          recurType() const              { return checkRecur(); }
		KCal::Recurrence*  recurrence() const             { return mRecurrence; }
		bool               recursFeb29() const            { return mRecursFeb29; }
		int                recurInterval() const;    // recurrence period in units of the recurrence period type (minutes, days, etc)
		int                remainingRecurrences() const   { return mRemainingRecurrences; }
		OccurType          nextOccurrence(const QDateTime& preDateTime, QDateTime& result) const;
		OccurType          previousOccurrence(const QDateTime& afterDateTime, QDateTime& result) const;
		int                flags() const;
		bool               toBeArchived() const           { return mArchive; }
		bool               updated() const                { return mUpdated; }
		bool               mainExpired() const            { return mMainExpired; }
		bool               expired() const                { return mDisplaying && mMainExpired  ||  uidStatus(mEventID) == EXPIRED; }
		Status             uidStatus() const              { return uidStatus(mEventID); }
		static Status      uidStatus(const QString& uid);
		static QString     uid(const QString& id, Status);

		struct MonthPos
		{
			MonthPos() : days(7) { }
			int        weeknum;     // week in month, or < 0 to count from end of month
			QBitArray  days;        // days in week
		};
		void               setRecurMinutely(int freq, int count)                                            { setRecurMinutely(freq, count, QDateTime()); }
		void               setRecurMinutely(int freq, const QDateTime& end)                                 { setRecurMinutely(freq, 0, end); }
		void               setRecurDaily(int freq, int count)                                               { setRecurDaily(freq, count, QDate()); }
		void               setRecurDaily(int freq, const QDate& end)                                        { setRecurDaily(freq, 0, end); }
		void               setRecurWeekly(int freq, const QBitArray& days, int count)                       { setRecurWeekly(freq, days, count, QDate()); }
		void               setRecurWeekly(int freq, const QBitArray& days, const QDate& end)                { setRecurWeekly(freq, days, 0, end); }
		void               setRecurMonthlyByDate(int freq, const QValueList<int>& days, int count)          { setRecurMonthlyByDate(freq, days, count, QDate()); }
		void               setRecurMonthlyByDate(int freq, const QValueList<int>& days, const QDate& end)   { setRecurMonthlyByDate(freq, days, 0, end); }
		void               setRecurMonthlyByPos(int freq, const QValueList<MonthPos>& mp, int count)        { setRecurMonthlyByPos(freq, mp, count, QDate()); }
		void               setRecurMonthlyByPos(int freq, const QValueList<MonthPos>& mp, const QDate& end) { setRecurMonthlyByPos(freq, mp, 0, end); }
		void               setRecurMonthlyByPos(int freq, const QPtrList<KCal::Recurrence::rMonthPos>& mp, int count)   { setRecurMonthlyByPos(freq, mp, count, QDate()); }
		void               setRecurMonthlyByPos(int freq, const QPtrList<KCal::Recurrence::rMonthPos>& mp, const QDate& end) { setRecurMonthlyByPos(freq, mp, 0, end); }
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, int count)         { setRecurAnnualByDate(freq, months, -1, count, QDate()); }
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, const QDate& end)  { setRecurAnnualByDate(freq, months, -1, 0, end); }
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, bool feb29, int count)        { setRecurAnnualByDate(freq, months, feb29, count, QDate()); }
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, bool feb29, const QDate& end) { setRecurAnnualByDate(freq, months, feb29, 0, end); }
		void               setRecurAnnualByPos(int freq, const QValueList<MonthPos>& mp, const QValueList<int>& months, int count)     { setRecurAnnualByPos(freq, mp, months, count, QDate()); }
		void               setRecurAnnualByPos(int freq, const QValueList<MonthPos>& mp, const QValueList<int>& months, const QDate& end)  { setRecurAnnualByPos(freq, mp, months, 0, end); }
		void               setRecurAnnualByDay(int freq, const QValueList<int>& days, int count)            { setRecurAnnualByDay(freq, days, count, QDate()); }
		void               setRecurAnnualByDay(int freq, const QValueList<int>& days, const QDate& end)     { setRecurAnnualByDay(freq, days, 0, end); }

		void               setRecurMinutely(int freq, int count, const QDateTime& end);
		void               setRecurDaily(int freq, int count, const QDate& end);
		void               setRecurWeekly(int freq, const QBitArray& days, int count, const QDate& end);
		void               setRecurMonthlyByDate(int freq, const QValueList<int>& days, int count, const QDate& end);
		void               setRecurMonthlyByDate(int freq, const QPtrList<int>& days, int count, const QDate& end);
		void               setRecurMonthlyByPos(int freq, const QValueList<MonthPos>&, int count, const QDate& end);
		void               setRecurMonthlyByPos(int freq, const QPtrList<KCal::Recurrence::rMonthPos>&, int count, const QDate& end);
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, bool feb29, int count, const QDate& end);
		void               setRecurAnnualByDate(int freq, const QPtrList<int>& months, bool feb29, int count, const QDate& end);
		void               setRecurAnnualByPos(int freq, const QValueList<MonthPos>&, const QValueList<int>& months, int count, const QDate& end);
		void               setRecurAnnualByPos(int freq, const QPtrList<KCal::Recurrence::rMonthPos>&, const QPtrList<int>& months, int count, const QDate& end);
		void               setRecurAnnualByDay(int freq, const QValueList<int>& days, int count, const QDate& end);
		void               setRecurAnnualByDay(int freq, const QPtrList<int>& days, int count, const QDate& end);
#ifdef NDEBUG
		void               dumpDebug() const  { }
#else
		void               dumpDebug() const;
#endif
		static bool        adjustStartOfDay(const QPtrList<KCal::Event>&);
		static void        convertKCalEvents(AlarmCalendar&);

	private:
		void               copy(const KAlarmEvent&);
		bool               initRecur(bool endDate, int count = 0, bool feb29 = false);
		RecurType          checkRecur() const;
		OccurType          nextRecurrence(const QDateTime& preDateTime, QDateTime& result, int& remainingCount) const;
		OccurType          previousRecurrence(const QDateTime& afterDateTime, QDateTime& result) const;
		KCal::Alarm*       initKcalAlarm(KCal::Event&, const QDateTime&, const QStringList& types) const;
		static void        readAlarms(const KCal::Event&, void* alarmMap);
		static void        readAlarm(const KCal::Alarm&, AlarmData&);

		QString            mAudioFile;        // ATTACH: audio file to play
		QDateTime          mStartDateTime;    // DTSTART: start time for event
		QDateTime          mEndDateTime;      // DTEND: end time for event
		QDateTime          mAtLoginDateTime;  // repeat-at-login time
		QDateTime          mDeferralTime;     // extra time to trigger alarm (if alarm deferred)
		QDateTime          mDisplayingTime;   // date/time shown in the alarm currently being displayed
		int                mDisplayingFlags;  // type of alarm which is currently being displayed
		int                mReminderMinutes;  // how long in advance reminder is to be, or 0 if none
		int                mReminderDeferralMinutes; // deferred reminder period, or 0 if none
		int                mReminderArchiveMinutes;  // original reminder period if now expired, or 0 if none
		int                mRevision;         // SEQUENCE: revision number of the original alarm, or 0
		KCal::Recurrence*  mRecurrence;       // RECUR: recurrence specification, or 0 if none
		int                mRemainingRecurrences; // remaining number of alarm recurrences including initial time, -1 to repeat indefinitely
		int                mAlarmCount;       // number of alarms
		bool               mRecursFeb29;      // the recurrence is yearly on February 29th
		bool               mAnyTime;          // event has only a date, not a time
		bool               mMainExpired;      // main alarm has expired (in which case a deferral alarm will exist)
		bool               mArchive;          // event has triggered in the past, so archive it when closed
		mutable bool       mUpdated;          // event has been updated but not written to calendar file
};

#endif // KALARMEVENT_H
