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
 */

#ifndef KALARMEVENT_H
#define KALARMEVENT_H

#include <libkcal/event.h>
#include <libkcal/recurrence.h>

struct AlarmData;

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
 */

// KAlarmAlarm corresponds to a single KCal::Alarm instance
class KAlarmAlarm
{
	public:
		enum Type  { MESSAGE, FILE, COMMAND, AUDIO };

		KAlarmAlarm()    : mAlarmSeq(-1), mBeep(false), mRepeatAtLogin(false), mDeferral(false), mLateCancel(false) { }
		~KAlarmAlarm()  { }
		void             set(int flags);
		bool             valid() const              { return mAlarmSeq > 0; }
		Type             type() const               { return mType; }
		int              id() const                 { return mAlarmSeq; }
		int              sequence() const           { return mAlarmSeq; }
		const QString&   eventID() const            { return mEventID; }
		const QDateTime& dateTime() const           { return mDateTime; }
		QDate            date() const               { return mDateTime.date(); }
		QTime            time() const               { return mDateTime.time(); }
		const QString&   cleanText() const          { return mCleanText; }
		QString          message() const            { return (mType == MESSAGE) ? mCleanText : QString::null; }
		QString          fileName() const           { return (mType == FILE) ? mCleanText : QString::null; }
		QString          command() const            { return (mType == COMMAND) ? mCleanText : QString::null; }
		QString          audioFile() const          { return (mType == AUDIO) ? mCleanText : QString::null; }
		void             commandArgs(QStringList&) const;
		const QColor&    colour() const             { return mColour; }
		bool             lateCancel() const         { return mLateCancel; }
		bool             repeatAtLogin() const      { return mRepeatAtLogin; }
		bool             deferred() const           { return mDeferral; }
		bool             beep() const               { return mBeep; }
		int              flags() const;
#ifdef NDEBUG
		void             dumpDebug() const  { }
#else
		void             dumpDebug() const;
#endif
		static QString   commandFromArgs(const QStringList&);

		QString          mEventID;          // KCal::Event unique ID
		QString          mCleanText;        // message text, file URL, command or audio file
		QDateTime        mDateTime;         // next time to display the alarm
		QColor           mColour;           // background colour of alarm message
		Type             mType;             // message/file/command
		int              mAlarmSeq;         // sequence number of this alarm
		bool             mRecurs;           // there is a recurrence rule for the alarm
		bool             mBeep;             // whether to beep when the alarm is displayed
		bool             mRepeatAtLogin;    // whether to repeat the alarm at every login
		bool             mDeferral;         // whether the alarm is an extra deferred alarm
		bool             mLateCancel;       // whether to cancel the alarm if it can't be displayed on time
};


// KAlarmEvent corresponds to a KCal::Event instance
class KAlarmEvent
{
	public:
		enum            // flags for use in DCOP calls, etc.
		{
			LATE_CANCEL     = 0x01,
			BEEP            = 0x02,
			REPEAT_AT_LOGIN = 0x04,
			ANY_TIME        = 0x08,    // only a date is specified, not a time
			// The following values are read-only
			DEFERRAL        = 0x80
		};
		enum RecurType
		{
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
		enum OccurType
		{
			NO_OCCURRENCE,        // no occurrence is due
			FIRST_OCCURRENCE,     // the first occurrence is due (takes precedence over LAST_OCCURRENCE)
			RECURRENCE_DATE,      // a recurrence is due with only a date, not a time
			RECURRENCE_DATE_TIME, // a recurrence is due with a date and time
			LAST_OCCURRENCE       // the last occurrence is due
		};

		KAlarmEvent()    : mRevision(0), mRecurrence(0), mMainAlarmID(1) { }
		KAlarmEvent(const QDateTime& dt, const QString& message, const QColor& c, KAlarmAlarm::Type type, int flags)
																								: mRecurrence(0L) { set(dt, message, c, type, flags); }
		explicit KAlarmEvent(const KCal::Event& e)  : mRecurrence(0L) { set(e); }
		~KAlarmEvent()    { delete mRecurrence; }
		void              set(const KCal::Event&);
		void              set(const QDate& d, const QString& message, const QColor& c, KAlarmAlarm::Type type, int flags)
		                           { set(d, message, c, type, flags | ANY_TIME); }
		void              set(const QDateTime&, const QString& message, const QColor&, KAlarmAlarm::Type, int flags);
		void              setMessage(const QDate& d, const QString& message, const QColor& c, int flags)
		                           { set(d, message, c, KAlarmAlarm::MESSAGE, flags | ANY_TIME); }
		void              setMessage(const QDateTime& dt, const QString& message, const QColor& c, int flags)
		                           { set(dt, message, c, KAlarmAlarm::MESSAGE, flags); }
		void              setFileName(const QDate& d, const QString& filename, const QColor& c, int flags)
		                           { set(d, filename, c, KAlarmAlarm::FILE, flags | ANY_TIME); }
		void              setFileName(const QDateTime& dt, const QString& filename, const QColor& c, int flags)
		                           { set(dt, filename, c, KAlarmAlarm::FILE, flags); }
		void              setCommand(const QDate& d, const QString& command, int flags)
		                           { set(d, command, QColor(), KAlarmAlarm::COMMAND, flags | ANY_TIME); }
		void              setCommand(const QDateTime& dt, const QString& command, int flags = 0)
		                           { set(dt, command, QColor(), KAlarmAlarm::COMMAND, flags); }
		void              setAudioFile(const QString& filename)             { mAudioFile = filename; }
		OccurType         setNextOccurrence(const QDateTime& preDateTime);
		void              setEventID(const QString& id)                     { mEventID = id; }
		void              setDate(const QDate& d)                           { mDateTime = d; mAnyTime = true; }
		void              setTime(const QDateTime& dt)                      { mDateTime = dt; mAnyTime = false; }
		void              setLateCancel(bool lc)                            { mLateCancel = lc; }
		void              set(int flags);
		void              defer(const QDateTime&);
		void              cancelDefer();
		KCal::Event*      event() const;    // convert to new Event
		KAlarmAlarm       alarm(int alarmID) const;
		KAlarmAlarm       firstAlarm() const;
		KAlarmAlarm       nextAlarm(const KAlarmAlarm&) const;
		bool              updateEvent(KCal::Event&) const;
		void              removeAlarm(int alarmID);
		void              incrementRevision()          { ++mRevision; }
		void              setUpdated()                 { mUpdated = true; }
		bool              updated() const              { return mUpdated; }

		bool              operator==(const KAlarmEvent&);
		bool              operator!=(const KAlarmEvent& e)  { return !operator==(e); }
		KAlarmAlarm::Type type() const                 { return mType; }
		const QString&    id() const                   { return mEventID; }
		int               alarmCount() const           { return mAlarmCount; }
		const QDateTime&  mainDateTime() const         { return mDateTime; }
		QDate             mainDate() const             { return mDateTime.date(); }
		QTime             mainTime() const             { return mDateTime.time(); }
		bool              anyTime() const              { return mAnyTime; }
		const QDateTime&  deferDateTime() const        { return mDeferralTime; }
		QDateTime         dateTime() const             { return mDeferral ? QMIN(mDeferralTime, mDateTime) : mDateTime; }
		const QString&    cleanText() const            { return mCleanText; }
		QString           message() const              { return (mType == KAlarmAlarm::MESSAGE) ? mCleanText : QString::null; }
		QString           fileName() const             { return (mType == KAlarmAlarm::FILE) ? mCleanText : QString::null; }
		QString           command() const              { return (mType == KAlarmAlarm::COMMAND) ? mCleanText : QString::null; }
		QString           messageFileOrCommand() const { return mCleanText; }
		QString           audioFile() const            { return mAudioFile; }
		const QColor&     colour() const               { return mColour; }
		RecurType         recurs() const;
		KCal::Recurrence* recurrence() const           { return mRecurrence; }
		int               recurInterval() const;    // recurrence period in units of the recurrence period type (minutes, days, etc)
		int               repeatCount() const          { return mRepeatDuration; }
		OccurType         nextOccurrence(const QDateTime& preDateTime, QDateTime& result) const;
		OccurType         previousOccurrence(const QDateTime& afterDateTime, QDateTime& result) const;
		bool              lateCancel() const           { return mLateCancel; }
		bool              repeatAtLogin() const        { return mRepeatAtLogin; }
		bool              deferred() const             { return mDeferral; }
		bool              beep() const                 { return mBeep; }
		int               flags() const;

		struct MonthPos
		{
			MonthPos() : days(7) { }
			int        weeknum;     // week in month, or < 0 to count from end of month
			QBitArray  days;        // days in week
		};
		bool              initRecur(bool endDate, int count = 0);
		void              setRecurMinutely(int freq, int count)                                            { setRecurMinutely(freq, count, QDateTime()); }
		void              setRecurMinutely(int freq, const QDateTime& end)                                 { setRecurMinutely(freq, 0, end); }
		void              setRecurDaily(int freq, int count)                                               { setRecurDaily(freq, count, QDate()); }
		void              setRecurDaily(int freq, const QDate& end)                                        { setRecurDaily(freq, 0, end); }
		void              setRecurWeekly(int freq, const QBitArray& days, int count)                       { setRecurWeekly(freq, days, count, QDate()); }
		void              setRecurWeekly(int freq, const QBitArray& days, const QDate& end)                { setRecurWeekly(freq, days, 0, end); }
		void              setRecurMonthlyByDate(int freq, const QValueList<int>& days, int count)          { setRecurMonthlyByDate(freq, days, count, QDate()); }
		void              setRecurMonthlyByDate(int freq, const QValueList<int>& days, const QDate& end)   { setRecurMonthlyByDate(freq, days, 0, end); }
		void              setRecurMonthlyByPos(int freq, const QValueList<MonthPos>& mp, int count)        { setRecurMonthlyByPos(freq, mp, count, QDate()); }
		void              setRecurMonthlyByPos(int freq, const QValueList<MonthPos>& mp, const QDate& end) { setRecurMonthlyByPos(freq, mp, 0, end); }
		void              setRecurMonthlyByPos(int freq, const QPtrList<KCal::Recurrence::rMonthPos>& mp, int count)   { setRecurMonthlyByPos(freq, mp, count, QDate()); }
		void              setRecurMonthlyByPos(int freq, const QPtrList<KCal::Recurrence::rMonthPos>& mp, const QDate& end) { setRecurMonthlyByPos(freq, mp, 0, end); }
		void              setRecurAnnualByDate(int freq, const QValueList<int>& months, int count)         { setRecurAnnualByDate(freq, months, count, QDate()); }
		void              setRecurAnnualByDate(int freq, const QValueList<int>& months, const QDate& end)  { setRecurAnnualByDate(freq, months, 0, end); }
		void              setRecurAnnualByPos(int freq, const QValueList<MonthPos>& mp, const QValueList<int>& months, int count)     { setRecurAnnualByPos(freq, mp, months, count, QDate()); }
		void              setRecurAnnualByPos(int freq, const QValueList<MonthPos>& mp, const QValueList<int>& months, const QDate& end)  { setRecurAnnualByPos(freq, mp, months, 0, end); }
		void              setRecurAnnualByDay(int freq, const QValueList<int>& days, int count)            { setRecurAnnualByDay(freq, days, count, QDate()); }
		void              setRecurAnnualByDay(int freq, const QValueList<int>& days, const QDate& end)     { setRecurAnnualByDay(freq, days, 0, end); }

		void              setRecurMinutely(int freq, int count, const QDateTime& end);
		void              setRecurDaily(int freq, int count, const QDate& end);
		void              setRecurWeekly(int freq, const QBitArray& days, int count, const QDate& end);
		void              setRecurMonthlyByDate(int freq, const QValueList<int>& days, int count, const QDate& end);
		void              setRecurMonthlyByDate(int freq, const QPtrList<int>& days, int count, const QDate& end);
		void              setRecurMonthlyByPos(int freq, const QValueList<MonthPos>&, int count, const QDate& end);
		void              setRecurMonthlyByPos(int freq, const QPtrList<KCal::Recurrence::rMonthPos>&, int count, const QDate& end);
		void              setRecurAnnualByDate(int freq, const QValueList<int>& months, int count, const QDate& end);
		void              setRecurAnnualByDate(int freq, const QPtrList<int>& months, int count, const QDate& end);
		void              setRecurAnnualByPos(int freq, const QValueList<MonthPos>&, const QValueList<int>& months, int count, const QDate& end);
		void              setRecurAnnualByPos(int freq, const QPtrList<KCal::Recurrence::rMonthPos>&, const QPtrList<int>& months, int count, const QDate& end);
		void              setRecurAnnualByDay(int freq, const QValueList<int>& days, int count, const QDate& end);
		void              setRecurAnnualByDay(int freq, const QPtrList<int>& days, int count, const QDate& end);
#ifdef NDEBUG
		void              dumpDebug() const  { }
#else
		void              dumpDebug() const;
#endif
		static bool       adjustStartOfDay(const QPtrList<KCal::Event>&);

		static const int  MAIN_ALARM_ID;            // alarm ID for main alarm
		static const int  AUDIO_ALARM_ID;           // alarm ID for audio alarm
		static const int  REPEAT_AT_LOGIN_OFFSET;   // alarm ID offset for repeat-at-login alarm
		static const int  DEFERRAL_OFFSET;          // alarm ID offset for deferral alarm
	private:
		RecurType         checkRecur() const;
		OccurType         nextRecurrence(const QDateTime& preDateTime, QDateTime& result, int& remainingCount) const;
		OccurType         previousRecurrence(const QDateTime& afterDateTime, QDateTime& result) const;
		static int        readAlarm(const KCal::Alarm&, AlarmData&);

		QString           mEventID;          // KCal::Event unique ID
		QString           mCleanText;        // message text, file URL or command
		QString           mAudioFile;        // audio file to play
		QDateTime         mDateTime;         // next time to display the alarm (except deferrals)
		QDateTime         mRepeatAtLoginDateTime;  // repeat at login time
		QDateTime         mDeferralTime;     // extra time to trigger alarm (if alarm deferred)
		QColor            mColour;           // background colour of alarm message
		KAlarmAlarm::Type mType;             // message/file/command (not audio)
		int               mRevision;         // revision number of the original alarm, or 0
		KCal::Recurrence* mRecurrence;       // recurrence specification, or 0 if none
		int               mRepeatDuration;   // remaining number of alarm repetitions including initial time, -1 to repeat indefinitely
		int               mAlarmCount;       // number of alarms
		int               mMainAlarmID;      // sequence number of main alarm
		int               mRepeatAtLoginAlarmID; // sequence number of repeat-at-login alarm (only if read from calendar file)
		int               mDeferralAlarmID;  // sequence number of deferral alarm (only if read from calendar file)
		bool              mAnyTime;          // event has only a date, not a time
		bool              mBeep;             // whether to beep when the alarm is displayed
		bool              mRepeatAtLogin;    // whether to repeat the alarm at every login
		bool              mDeferral;         // whether the alarm has an extra deferred time
		bool              mLateCancel;       // whether to cancel the alarm if it can't be displayed on time
		bool              mUpdated;          // event has been updated but not written to calendar file
};

#endif // KALARMEVENT_H
