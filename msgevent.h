/*
 *  msgevent.h  -  the event object for messages
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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


/*
 * KAlarm events are stored as alarms in the calendar file, as follows:
 * In the alarm object:
 *   next time/date - stored as the alarm time (alarm TRIGGER field)
 *   message text - stored as the alarm description, with prefix TEXT: (alarm DESCRIPTION field)
 *   file name to display text from - stored as the alarm description, with prefix FILE: (alarm DESCRIPTION field)
 *   command to execute - stored as the alarm description, with prefix CMD: (alarm DESCRIPTION field)
 *   late cancel, repeat at login, deferral - stored in prefix to the alarm description (alarm DESCRIPTION field)
 * In the event object:
 *   colour - stored as a hex string prefixed by #, as the first category (event CATEGORIES field)
 *   elapsed repeat count - stored as the revision number (event SEQUENCE field)
 *   beep - stored as a "BEEP" category (event CATEGORIES field)
 *   confirmAck - stored as a "ACKCONF" category (event CATEGORIES field)
 */

// Base class containing data common to KAlarmAlarm and KAlarmEvent
class KAAlarmEventBase
{
	public:
		~KAAlarmEventBase()  { }
		void               set(int flags);
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
		const QColor&      colour() const             { return mColour; }
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

		QString            mEventID;          // UID: KCal::Event unique ID
		QString            mText;             // message text, file URL, command, email body [or audio file for KAlarmAlarm]
		QDateTime          mDateTime;         // next time to display the alarm
		QColor             mColour;           // background colour of alarm message
		EmailAddressList   mEmailAddresses;   // ATTENDEE: addresses to send email to
		QString            mEmailSubject;     // SUMMARY: subject line of email
		QStringList        mEmailAttachments; // ATTACH: email attachment file names
		Type               mActionType;       // alarm action type
		bool               mBeep;             // whether to beep when the alarm is displayed
		bool               mRepeatAtLogin;    // whether to repeat the alarm at every login
		bool               mDeferral;         // whether the alarm is an extra deferred alarm
		bool               mDisplaying;       // whether the alarm is currently being displayed
		bool               mLateCancel;       // whether to cancel the alarm if it can't be displayed on time
		bool               mEmailBcc;         // blind copy the email to the user
		bool               mConfirmAck;       // alarm acknowledgement requires confirmation by user

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
			INVALID_ALARM,      // not an alarm
			MAIN_ALARM,         // THE real alarm. Must be the first in the enumeration.
			DEFERRAL_ALARM,     // deferred alarm
			AT_LOGIN_ALARM,     // additional repeat-at-login trigger
			DISPLAYING_ALARM,   // copy of the alarm currently being displayed
			AUDIO_ALARM         // sound to play when displaying the alarm
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
			// The following are read-only internal values, and may be changed
			DEFERRAL        = 0x80,
			DISPLAYING_     = 0x100
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
		KAlarmEvent(const QDateTime& dt, const QString& message, const QColor& c, Action action, int flags)
		                                            : mRecurrence(0) { set(dt, message, c, action, flags); }
		explicit KAlarmEvent(const KCal::Event& e)  : mRecurrence(0) { set(e); }
		KAlarmEvent(const KAlarmEvent& e)           : KAAlarmEventBase(e), mRecurrence(0) { copy(e); }
		~KAlarmEvent()     { delete mRecurrence; }
		KAlarmEvent&       operator=(const KAlarmEvent& e)   { if (&e != this) copy(e);  return *this; }
		void               set(const KCal::Event&);
		void               set(const QDate& d, const QString& message, const QColor& c, Action action, int flags)
		                            { set(d, message, c, action, flags | ANY_TIME); }
		void               set(const QDateTime&, const QString& message, const QColor&, Action, int flags);
		void               setMessage(const QDate& d, const QString& message, const QColor& c, int flags)
		                            { set(d, message, c, MESSAGE, flags | ANY_TIME); }
		void               setMessage(const QDateTime& dt, const QString& message, const QColor& c, int flags)
		                            { set(dt, message, c, MESSAGE, flags); }
		void               setFileName(const QDate& d, const QString& filename, const QColor& c, int flags)
		                            { set(d, filename, c, FILE, flags | ANY_TIME); }
		void               setFileName(const QDateTime& dt, const QString& filename, const QColor& c, int flags)
		                            { set(dt, filename, c, FILE, flags); }
		void               setCommand(const QDate& d, const QString& command, int flags)
		                            { set(d, command, QColor(), COMMAND, flags | ANY_TIME); }
		void               setCommand(const QDateTime& dt, const QString& command, int flags)
		                            { set(dt, command, QColor(), COMMAND, flags); }
		void               setEmail(const QDate&, const EmailAddressList&, const QString& subject,
		                            const QString& message, const QStringList& attachments, int flags);
		void               setEmail(const QDateTime&, const EmailAddressList&, const QString& subject,
		                            const QString& message, const QStringList& attachments, int flags);
		void               setEmail(const EmailAddressList&, const QString& subject, const QStringList& attachments);
		void               setAudioFile(const QString& filename)             { mAudioFile = filename; }
		OccurType          setNextOccurrence(const QDateTime& preDateTime);
		void               setEventID(const QString& id)                     { mEventID = id; }
		void               setDate(const QDate& d)                           { mDateTime = d; mAnyTime = true; }
		void               setTime(const QDateTime& dt)                      { mDateTime = dt; mAnyTime = false; }
		void               setEndTime(const QDateTime& dt)                   { mEndDateTime = dt; }
		void               setLateCancel(bool lc)                            { mLateCancel = lc; }
		void               set(int flags);
		void               setUid(Status s)                                  { mEventID = uid(mEventID, s); }
		static QString     uid(const QString& id, Status);
		void               defer(const QDateTime&, bool adjustRecurrence = false);
		void               cancelDefer();
		bool               setDisplaying(const KAlarmEvent&, KAlarmAlarm::Type, const QDateTime&);
		void               reinstateFromDisplaying(const KAlarmEvent& dispEvent);
		void               setArchive()                                      { mArchive = true; }
		void               setUpdated()                                      { mUpdated = true; }
		KCal::Event*       event() const;    // convert to new Event
		KAlarmAlarm        alarm(KAlarmAlarm::Type) const;
		KAlarmAlarm        firstAlarm() const;
		KAlarmAlarm        nextAlarm(const KAlarmAlarm& al) const            { return nextAlarm(al.type()); }
		KAlarmAlarm        nextAlarm(KAlarmAlarm::Type) const;
		KAlarmAlarm        convertDisplayingAlarm() const;
		bool               updateEvent(KCal::Event&, bool checkUid = true) const;
		void               removeAlarm(KAlarmAlarm::Type);
		void               incrementRevision()                               { ++mRevision; }

		Action             action() const               { return (Action)mActionType; }
		const QString&     id() const                   { return mEventID; }
		bool               valid() const                { return !!mAlarmCount; }
		int                alarmCount() const           { return mAlarmCount; }
		const QDateTime&   mainDateTime() const         { return mDateTime; }
		QDate              mainDate() const             { return mDateTime.date(); }
		QTime              mainTime() const             { return mDateTime.time(); }
		const QDateTime&   endDateTime() const          { return mEndDateTime; }
		bool               anyTime() const              { return mAnyTime; }
		const QDateTime&   deferDateTime() const        { return mDeferralTime; }
		QDateTime          dateTime() const             { return mDeferral ? QMIN(mDeferralTime, mDateTime) : mDateTime; }
		const QString&     messageFileOrCommand() const { return mText; }
		const QString&     audioFile() const            { return mAudioFile; }
		RecurType          recurs() const               { return checkRecur(); }
		KCal::Recurrence*  recurrence() const           { return mRecurrence; }
		int                recurInterval() const;    // recurrence period in units of the recurrence period type (minutes, days, etc)
		int                remainingRecurrences() const { return mRemainingRecurrences; }
		OccurType          nextOccurrence(const QDateTime& preDateTime, QDateTime& result) const;
		OccurType          previousOccurrence(const QDateTime& afterDateTime, QDateTime& result) const;
		int                flags() const;
		bool               toBeArchived() const         { return mArchive; }
		bool               updated() const              { return mUpdated; }
		bool               expired() const              { return mDisplaying && mExpired  ||  uidStatus(mEventID) == EXPIRED; }
		Status             uidStatus() const            { return uidStatus(mEventID); }
		static Status      uidStatus(const QString& uid);

		struct MonthPos
		{
			MonthPos() : days(7) { }
			int        weeknum;     // week in month, or < 0 to count from end of month
			QBitArray  days;        // days in week
		};
		bool               initRecur(bool endDate, int count = 0);
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
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, int count)         { setRecurAnnualByDate(freq, months, count, QDate()); }
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, const QDate& end)  { setRecurAnnualByDate(freq, months, 0, end); }
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
		void               setRecurAnnualByDate(int freq, const QValueList<int>& months, int count, const QDate& end);
		void               setRecurAnnualByDate(int freq, const QPtrList<int>& months, int count, const QDate& end);
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
		void               addDefer(const QDateTime&);
		RecurType          checkRecur() const;
		OccurType          nextRecurrence(const QDateTime& preDateTime, QDateTime& result, int& remainingCount) const;
		OccurType          previousRecurrence(const QDateTime& afterDateTime, QDateTime& result) const;
		KCal::Alarm*       initKcalAlarm(KCal::Event&, const QDateTime&, const QStringList& types) const;
		static void        readAlarm(const KCal::Alarm&, AlarmData&);

		QString            mAudioFile;        // ATTACH: audio file to play
		QDateTime          mEndDateTime;      // DTEND: end time for event
		QDateTime          mAtLoginDateTime;  // repeat-at-login time
		QDateTime          mDeferralTime;     // extra time to trigger alarm (if alarm deferred)
		QDateTime          mDisplayingTime;   // date/time shown in the alarm currently being displayed
		int                mDisplayingFlags;  // type of alarm which is currently being displayed
		int                mRevision;         // SEQUENCE: revision number of the original alarm, or 0
		KCal::Recurrence*  mRecurrence;       // RECUR: recurrence specification, or 0 if none
		int                mRemainingRecurrences; // remaining number of alarm recurrences including initial time, -1 to repeat indefinitely
		int                mAlarmCount;       // number of alarms
		bool               mAnyTime;          // event has only a date, not a time
		bool               mExpired;          // main alarm has expired
		bool               mArchive;          // event has triggered in the past, so archive it when closed
		bool               mUpdated;          // event has been updated but not written to calendar file
};

#endif // KALARMEVENT_H
