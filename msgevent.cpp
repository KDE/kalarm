/*
 *  msgevent.cpp  -  the event object for messages
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

#include <stdlib.h>
#include <ctype.h>
#include <qcolor.h>
#include <qregexp.h>
#include <kdebug.h>

#include "kalarm.h"
#include "kalarmapp.h"
#include "prefsettings.h"
#include "msgevent.h"
using namespace KCal;


/*
 * Each alarm DESCRIPTION field contains the following:
 *   SEQNO;[FLAGS];TYPE:TEXT
 * where
 *   SEQNO = sequence number of alarm within the event
 *   FLAGS = C for late-cancel, L for repeat-at-login, D for deferral
 *   TYPE = TEXT or FILE or CMD
 *   TEXT = message text, file name/URL or command
 */
static const QChar   SEPARATOR        = QChar(';');
static const QString TEXT_PREFIX      = QString::fromLatin1("TEXT:");
static const QString FILE_PREFIX      = QString::fromLatin1("FILE:");
static const QString COMMAND_PREFIX   = QString::fromLatin1("CMD:");
static const QString LATE_CANCEL_CODE = QString::fromLatin1("C");
static const QString AT_LOGIN_CODE    = QString::fromLatin1("L");   // subsidiary alarm at every login
static const QString DEFERRAL_CODE    = QString::fromLatin1("D");   // extra deferred alarm
static const QString BEEP_CATEGORY    = QString::fromLatin1("BEEP");

struct AlarmData
{
	QString           cleanText;
	QDateTime         dateTime;
	int               repeatCount;     // backwards compatibility with KAlarm pre-0.7 calendar files
	int               repeatMinutes;   // backwards compatibility with KAlarm pre-0.7 calendar files
	KAlarmAlarm::Type type;
	bool              lateCancel;
	bool              repeatAtLogin;
	bool              deferral;
};
typedef QMap<int, AlarmData> AlarmMap;


/*=============================================================================
= Class KAlarmEvent
= Corresponds to a KCal::Event instance.
=============================================================================*/

const int KAlarmEvent::MAIN_ALARM_ID          = 1;
const int KAlarmEvent::REPEAT_AT_LOGIN_OFFSET = 1;
const int KAlarmEvent::DEFERRAL_OFFSET        = 2;


/******************************************************************************
 * Initialise the KAlarmEvent from a KCal::Event.
 */
void KAlarmEvent::set(const Event& event)
{
	// Extract status from the event
	mEventID  = event.uid();
	mRevision = event.revision();
	const QStringList& cats = event.categories();
	mBeep   = false;
	mColour = QColor(255, 255, 255);    // missing/invalid colour - return white
	if (cats.count() > 0)
	{
		QColor colour(cats[0]);
		if (colour.isValid())
			 mColour = colour;

		for (unsigned int i = 1;  i < cats.count();  ++i)
			if (cats[i] == BEEP_CATEGORY)
				mBeep = true;
	}

	// Extract status from the event's alarms.
	// First set up defaults.
	mType          = KAlarmAlarm::MESSAGE;
	mLateCancel    = false;
	mRepeatAtLogin = false;
	mDeferral      = false;
	mCleanText     = "";
	mDateTime      = event.dtStart();
	mAnyTime       = event.doesFloat();
	initRecur(false);

	// Extract data from all the event's alarms and index the alarms by sequence number
	AlarmMap alarmMap;
	QPtrList<Alarm> alarms = event.alarms();
	for (QPtrListIterator<Alarm> it(alarms);  it.current();  ++it)
	{
		// Parse the next alarm's text
		AlarmData data;
		int sequence = readAlarm(*it.current(), data);
		alarmMap.insert(sequence, data);
	}

	// Incorporate the alarms' details into the overall event
	AlarmMap::ConstIterator it = alarmMap.begin();
	mMainAlarmID = -1;    // initialise as invalid
	mAlarmCount = 0;
	int repeatCount   = 0;    // backwards compatibility with KAlarm pre-0.7 calendar files
	int repeatMinutes = 0;    // backwards compatibility with KAlarm pre-0.7 calendar files
	bool set = false;
	for (  ;  it != alarmMap.end();  ++it)
	{
		bool main = false;
		const AlarmData& data = it.data();
		if (data.repeatAtLogin)
		{
			mRepeatAtLogin         = true;
			mRepeatAtLoginDateTime = data.dateTime;
			mRepeatAtLoginAlarmID  = it.key();
		}
		else if (data.deferral)
		{
			mDeferral        = true;
			mDeferralTime    = data.dateTime;
			mDeferralAlarmID = it.key();
		}
		else
		{
			mMainAlarmID = it.key();
			main = true;
		}

		// Ensure that the basic fields are set up even if a repeat-at-login or
		// deferral alarm is the only alarm in the event (which shouldn't happen!)
		if (main  ||  !set)
		{
			mType           = data.type;
			mCleanText      = (mType == KAlarmAlarm::COMMAND) ? data.cleanText.stripWhiteSpace() : data.cleanText;
			mDateTime       = data.dateTime;
			if (mAnyTime)
				mDateTime.setTime(QTime());
			repeatCount     = data.repeatCount;     // backwards compatibility with KAlarm pre-0.7 calendar files
			repeatMinutes   = data.repeatMinutes;   // backwards compatibility with KAlarm pre-0.7 calendar files
			mLateCancel     = data.lateCancel;
			set = true;
		}
		++mAlarmCount;
	}

	Recurrence* recur = event.recurrence();
	if (recur)
	{
		// Copy the recurrence details.
		QDateTime savedDT = mDateTime;
		switch (recur->doesRecur())
		{
			case Recurrence::rMinutely:
			case Recurrence::rHourly:
			case Recurrence::rDaily:
			case Recurrence::rWeekly:
			case Recurrence::rMonthlyDay:
			case Recurrence::rMonthlyPos:
			case Recurrence::rYearlyMonth:
			case Recurrence::rYearlyPos:
			case Recurrence::rYearlyDay:
				delete mRecurrence;
				mRecurrence = new Recurrence(*recur, 0);
				mRepeatDuration = recur->duration();
				if (mRepeatDuration > 0)
					mRepeatDuration -= recur->durationTo(savedDT) - 1;
				break;
			default:
				mDateTime = savedDT;
				break;
		}
	}
	else if (repeatCount > 0 && repeatMinutes > 0)
	{
		// Backwards compatibility with KAlarm pre-0.7 calendar files
		delete mRecurrence;
		mRecurrence = new Recurrence(0);
		mRepeatDuration = repeatCount + 1;
		mRecurrence->setMinutely(repeatMinutes, mRepeatDuration);
	}

	mUpdated = false;
}

/******************************************************************************
 * Parse a KCal::Alarm.
 * Reply = alarm ID (sequence number)
 */
int KAlarmEvent::readAlarm(const Alarm& alarm, AlarmData& data)
{
	// Parse the next alarm's text
	int sequence = MAIN_ALARM_ID;      // default main alarm ID
	data.type          = KAlarmAlarm::MESSAGE;
	data.lateCancel    = false;
	data.repeatAtLogin = false;
	data.deferral      = false;
	const QString& txt = alarm.text();
	int length = txt.length();
	int i = 0;
	if (txt[0].isDigit())
	{
		sequence = txt[0].digitValue();
		for (i = 1;  i < length;  ++i)
			if (txt[i].isDigit())
				sequence = sequence * 10 + txt[i].digitValue();
			else
			{
				if (txt[i++] == SEPARATOR)
				{
					while (i < length)
					{
						QChar ch = txt[i++];
						if (ch == SEPARATOR)
							break;
						if (ch == LATE_CANCEL_CODE)
							data.lateCancel = true;
						else if (ch == AT_LOGIN_CODE)
							data.repeatAtLogin = true;
						else if (ch == DEFERRAL_CODE)
							data.deferral = true;
					}
				}
				else
				{
					i = 0;
					sequence = MAIN_ALARM_ID;     // invalid sequence number - use default
				}
				break;
			}
	}
	if (txt.find(TEXT_PREFIX, i) == i)
		i += TEXT_PREFIX.length();
	else if (txt.find(FILE_PREFIX, i) == i)
	{
		data.type = KAlarmAlarm::FILE;
		i += FILE_PREFIX.length();
	}
	else if (txt.find(COMMAND_PREFIX, i) == i)
	{
		data.type = KAlarmAlarm::COMMAND;
		i += COMMAND_PREFIX.length();
	}
	else
		i = 0;

	data.cleanText     = txt.mid(i);
	data.dateTime      = alarm.time();
	data.repeatCount   = alarm.repeatCount();  // backwards compatibility with KAlarm pre-0.7 calendar files
	data.repeatMinutes = alarm.snoozeTime();   // backwards compatibility with KAlarm pre-0.7 calendar files
	return sequence;
}

/******************************************************************************
 * Initialise the KAlarmEvent with the specified parameters.
 */
void KAlarmEvent::set(const QDateTime& dateTime, const QString& text, const QColor& colour, KAlarmAlarm::Type type, int flags)
{
	initRecur(false);
	mMainAlarmID    = MAIN_ALARM_ID;
	mDateTime       = dateTime;
	mCleanText      = (type == KAlarmAlarm::COMMAND) ? text.stripWhiteSpace() : text;
	mType           = type;
	mColour         = colour;
	set(flags);
	mDeferral       = false;
	mUpdated        = false;
}

void KAlarmEvent::set(int flags)
{
	mBeep          = flags & BEEP;
	mRepeatAtLogin = flags & REPEAT_AT_LOGIN;
	mLateCancel    = flags & LATE_CANCEL;
	mAnyTime       = flags & ANY_TIME;
}

int KAlarmEvent::flags() const
{
	return (mBeep          ? BEEP : 0)
	     | (mRepeatAtLogin ? REPEAT_AT_LOGIN : 0)
	     | (mLateCancel    ? LATE_CANCEL : 0)
	     | (mAnyTime       ? ANY_TIME : 0)
	     | (mDeferral      ? DEFERRAL : 0);
}

/******************************************************************************
 * Create a new Event from the KAlarmEvent data.
 */
Event* KAlarmEvent::event() const
{
	Event* ev = new KCal::Event;
	updateEvent(*ev);
	return ev;
}

/******************************************************************************
 * Update an existing KCal::Event with the KAlarmEvent data.
 */
bool KAlarmEvent::updateEvent(Event& ev) const
{
	if (!mEventID.isEmpty()  &&  mEventID != ev.uid())
		return false;
	checkRecur();     // ensure recurrence/repetition data is consistent
	bool readOnly = ev.isReadOnly();
	ev.setReadOnly(false);

	// Set up event-specific data
	QStringList cats;
	cats.append(mColour.name());
	if (mBeep)
		cats.append(BEEP_CATEGORY);
	ev.setCategories(cats);
	ev.setRevision(mRevision);

	// Add the main alarm
	ev.clearAlarms();
	Alarm* al = ev.newAlarm();
	al->setEnabled(true);
	QString suffix;
	if (mLateCancel)
		suffix = LATE_CANCEL_CODE;
	suffix += SEPARATOR;
	switch (mType)
	{
		case KAlarmAlarm::MESSAGE:  suffix += TEXT_PREFIX;  break;
		case KAlarmAlarm::FILE:     suffix += FILE_PREFIX;  break;
		case KAlarmAlarm::COMMAND:  suffix += COMMAND_PREFIX;  break;
	}
	suffix += mCleanText;
	al->setText(QString::number(MAIN_ALARM_ID) + SEPARATOR + suffix);
	QDateTime aldt = mDateTime;
	if (mAnyTime)
		aldt.setTime(theApp()->settings()->startOfDay());
	al->setTime(aldt);
	QDateTime dt = mDateTime;

	// Add subsidiary alarms
	if (mRepeatAtLogin)
	{
		al = ev.newAlarm();
		al->setEnabled(true);        // enable the alarm
		al->setText(QString::number(MAIN_ALARM_ID + REPEAT_AT_LOGIN_OFFSET)
		            + SEPARATOR + AT_LOGIN_CODE + suffix);
		QDateTime dtl = mRepeatAtLoginDateTime.isValid() ? mRepeatAtLoginDateTime
		                : QDateTime::currentDateTime();
		al->setTime(dtl);
		if (dtl < dt)
			dt = dtl;
	}
	if (mDeferral)
	{
		al = ev.newAlarm();
		al->setEnabled(true);        // enable the alarm
		al->setText(QString::number(MAIN_ALARM_ID + DEFERRAL_OFFSET)
		            + SEPARATOR + DEFERRAL_CODE + suffix);
		al->setTime(mDeferralTime);
		if (mDeferralTime < dt)
			dt = mDeferralTime;
	}

	// Add recurrence data
	if (mRecurrence)
	{
		Recurrence* recur = ev.recurrence();
		int frequency = mRecurrence->frequency();
		int duration  = mRecurrence->duration();
		const QDateTime& endDateTime = mRecurrence->endDateTime();
		dt = mRecurrence->recurStart();
		recur->setRecurStart(dt);
		ushort rectype = mRecurrence->doesRecur();
		switch (rectype)
		{
			case Recurrence::rHourly:
				frequency *= 60;
				// fall through to Recurrence::rMinutely
			case Recurrence::rMinutely:
				if (duration)
					recur->setMinutely(frequency, duration);
				else
					recur->setMinutely(frequency, endDateTime);
				break;
			case Recurrence::rDaily:
				if (duration)
					recur->setDaily(frequency, duration);
				else
					recur->setDaily(frequency, endDateTime.date());
				break;
			case Recurrence::rWeekly:
				if (duration)
					recur->setWeekly(frequency, mRecurrence->days(), duration);
				else
					recur->setWeekly(frequency, mRecurrence->days(), endDateTime.date());
				break;
			case Recurrence::rMonthlyDay:
			{
				if (duration)
					recur->setMonthly(Recurrence::rMonthlyDay, frequency, duration);
				else
					recur->setMonthly(Recurrence::rMonthlyDay, frequency, endDateTime.date());
				const QPtrList<int>& mdays = mRecurrence->monthDays();
				for (QPtrListIterator<int> it(mdays);  it.current();  ++it)
					recur->addMonthlyDay(*it.current());
				break;
			}
			case Recurrence::rMonthlyPos:
			{
				if (duration)
					recur->setMonthly(Recurrence::rMonthlyPos, frequency, duration);
				else
					recur->setMonthly(Recurrence::rMonthlyPos, frequency, endDateTime.date());
				const QPtrList<Recurrence::rMonthPos>& mpos = mRecurrence->monthPositions();
				for (QPtrListIterator<Recurrence::rMonthPos> it(mpos);  it.current();  ++it)
				{
					short weekno = it.current()->rPos;
					if (it.current()->negative)
						weekno = -weekno;
					recur->addMonthlyPos(weekno, it.current()->rDays);
				}
				break;
			}
			case Recurrence::rYearlyMonth:
			case Recurrence::rYearlyPos:
			case Recurrence::rYearlyDay:
			{
				if (duration)
					recur->setYearly(rectype, frequency, duration);
				else
					recur->setYearly(rectype, frequency, endDateTime.date());
				const QPtrList<int>& ynums = mRecurrence->yearNums();
				for (QPtrListIterator<int> it(ynums);  it.current();  ++it)
					recur->addYearlyNum(*it.current());
				if (rectype == Recurrence::rYearlyPos)
				{
					const QPtrList<Recurrence::rMonthPos>& mpos = mRecurrence->yearMonthPositions();
					for (QPtrListIterator<Recurrence::rMonthPos> it(mpos);  it.current();  ++it)
					{
						short weekno = it.current()->rPos;
						if (it.current()->negative)
							weekno = -weekno;
						recur->addYearlyMonthPos(weekno, it.current()->rDays);
					}
				}
				break;
			}
			default:
				break;
		}
	}

	ev.setDtStart(dt);
	ev.setDtEnd(dt);
	ev.setFloats(mAnyTime);
	ev.setReadOnly(readOnly);
	return true;
}

/******************************************************************************
 * Return the alarm with the specified ID.
 */
KAlarmAlarm KAlarmEvent::alarm(int alarmID) const
{
	checkRecur();     // ensure recurrence/repetition data is consistent
	KAlarmAlarm al;
	al.mEventID       = mEventID;
	al.mCleanText     = mCleanText;
	al.mType          = mType;
	al.mColour        = mColour;
	al.mBeep          = mBeep;
	if (alarmID == mMainAlarmID  &&  mMainAlarmID >= 0)
	{
		al.mAlarmSeq      = mMainAlarmID;
		al.mDateTime      = mDateTime;
		al.mLateCancel    = mLateCancel;
	}
	else if (alarmID == mRepeatAtLoginAlarmID  &&  mRepeatAtLogin)
	{
		al.mAlarmSeq      = mRepeatAtLoginAlarmID;
		al.mDateTime      = mRepeatAtLoginDateTime;
		al.mRepeatAtLogin = true;
	}
	else if (alarmID == mDeferralAlarmID  &&  mDeferral)
	{
		al.mAlarmSeq = mDeferralAlarmID;
		al.mDateTime = mDeferralTime;
		al.mDeferral = true;
	}
	return al;
}

/******************************************************************************
 * Return the main alarm for the event.
 * If for some strange reason the main alarm does not exist, one of the
 * subsidiary ones is returned if possible.
 * N.B. a repeat-at-login alarm can only be returned if it has been read from/
 * written to the calendar file.
 */
KAlarmAlarm KAlarmEvent::firstAlarm() const
{
	if (mMainAlarmID > 0)
		return alarm(mMainAlarmID);
	if (mDeferral)
		return alarm(mDeferralAlarmID);
	if (mRepeatAtLogin)
		return alarm(mRepeatAtLoginAlarmID);
	return KAlarmAlarm();
}

/******************************************************************************
 * Return the next alarm for the event, after the specified alarm.
 * N.B. a repeat-at-login alarm can only be returned if it has been read from/
 * written to the calendar file.
 */
KAlarmAlarm KAlarmEvent::nextAlarm(const KAlarmAlarm& alrm) const
{
	int next;
	if (alrm.id() == mMainAlarmID)  next = 1;
	else if (alrm.id() == mDeferralAlarmID)  next = 2;
	else next = -1;
	switch (next)
	{
		case 1:
			if (mDeferral)
				return alarm(mDeferralAlarmID);
			// fall through to REPEAT
		case 2:
			if (mRepeatAtLogin)
				return alarm(mRepeatAtLoginAlarmID);
			// fall through to default
		default:
			break;
	}
	return KAlarmAlarm();
}

/******************************************************************************
 * Remove the alarm with the specified ID from the event.
 */
void KAlarmEvent::removeAlarm(int alarmID)
{
	if (alarmID == mMainAlarmID)
		mAlarmCount = 0;    // removing main alarm - also remove subsidiary alarms
	else if (alarmID == mRepeatAtLoginAlarmID)
	{
	   mRepeatAtLogin = false;
	   --mAlarmCount;
	}
	else if (alarmID == mDeferralAlarmID)
	{
	   mDeferral = false;
	   --mAlarmCount;
	}
}

/******************************************************************************
 * Add a deferral alarm with the specified trigger time.
 */
void KAlarmEvent::defer(const QDateTime& dateTime)
{
	mDeferralTime    = dateTime;
	mDeferralAlarmID = MAIN_ALARM_ID + DEFERRAL_OFFSET;
	mDeferral        = true;
}

/******************************************************************************
 * Check whether the event regularly repeats - with a recurrence specification
 * and/or an alarm repetition.
 */
KAlarmEvent::RecurType KAlarmEvent::recurs() const
{
	RecurType type = checkRecur();
	if (type == NO_RECUR  &&  mRepeatDuration)
		return MINUTELY;
	return type;
}

/******************************************************************************
 * Get the date/time of the next occurrence of the event, after the specified
 * date/time.
 * 'result' = date/time of next occurrence, or invalid date/time if none.
 */
KAlarmEvent::OccurType KAlarmEvent::nextOccurrence(const QDateTime& preDateTime, QDateTime& result) const
{
	if (checkRecur() != NO_RECUR)
	{
		int remainingCount;
		return nextRecurrence(preDateTime, result, remainingCount);
	}
	if (preDateTime < mDateTime)
	{
		result = mDateTime;
		return FIRST_OCCURRENCE;
	}
	result = QDateTime();
	return NO_OCCURRENCE;
}

/******************************************************************************
 * Get the date/time of the last previous occurrence of the event, before the
 * specified date/time.
 * 'result' = date/time of previous occurrence, or invalid date/time if none.
 */
KAlarmEvent::OccurType KAlarmEvent::previousOccurrence(const QDateTime& afterDateTime, QDateTime& result) const
{
	if (checkRecur() != NO_RECUR)
		return previousRecurrence(afterDateTime, result);
	result = QDateTime();
	return NO_OCCURRENCE;
}

/******************************************************************************
 * Set the date/time of the event to the next scheduled occurrence after the
 * specified date/time.
 */
KAlarmEvent::OccurType KAlarmEvent::setNextOccurrence(const QDateTime& preDateTime)
{
	if (preDateTime < mDateTime)
		return FIRST_OCCURRENCE;
	OccurType type;
	if (checkRecur() != NO_RECUR)
	{
		int remainingCount;
		QDateTime newTime;
		type = nextRecurrence(preDateTime, newTime, remainingCount);
		if (type != FIRST_OCCURRENCE  &&  type != NO_OCCURRENCE)
		{
			mDateTime = newTime;
			if (mRecurrence->duration() > 0)
				mRepeatDuration = remainingCount;
			mUpdated = true;
		}
	}
	else
		return NO_OCCURRENCE;
	return type;
}

/******************************************************************************
 * Get the date/time of the next recurrence of the event, after the specified
 * date/time.
 * 'result' = date/time of next occurrence, or invalid date/time if none.
 * 'remainingCount' = number of repetitions due, including the next occurrence.
 */
KAlarmEvent::OccurType KAlarmEvent::nextRecurrence(const QDateTime& preDateTime, QDateTime& result, int& remainingCount) const
{
	QDateTime recurStart = mRecurrence->recurStart();
	QDateTime pre = preDateTime;
	if (mAnyTime  &&  preDateTime.time() < theApp()->settings()->startOfDay())
		pre = pre.addDays(-1);    // today's recurrence (if today recurs) is still to come
	remainingCount = 0;
	bool last;
	result = mRecurrence->getNextDateTime(pre, &last);
	if (!result.isValid())
		return NO_OCCURRENCE;
	if (result == recurStart)
	{
		remainingCount = mRecurrence->duration();
		return FIRST_OCCURRENCE;
	}
	if (last)
	{
		remainingCount = 1;
		return LAST_OCCURRENCE;
	}
	remainingCount = mRecurrence->duration() - mRecurrence->durationTo(result) + 1;
	return mAnyTime ? RECURRENCE_DATE : RECURRENCE_DATE_TIME;
}

/******************************************************************************
 * Get the date/time of the last previous recurrence of the event, before the
 * specified date/time.
 * 'result' = date/time of previous occurrence, or invalid date/time if none.
 */
KAlarmEvent::OccurType KAlarmEvent::previousRecurrence(const QDateTime& afterDateTime, QDateTime& result) const
{
	QDateTime recurStart = mRecurrence->recurStart();
	QDateTime after = afterDateTime;
	if (mAnyTime  &&  afterDateTime.time() > theApp()->settings()->startOfDay())
		after = after.addDays(1);    // today's recurrence (if today recurs) has passed
	bool last;
	result = mRecurrence->getPreviousDateTime(after, &last);
	if (!result.isValid())
		return NO_OCCURRENCE;
	if (result == recurStart)
		return FIRST_OCCURRENCE;
	if (last)
		return LAST_OCCURRENCE;
	return mAnyTime ? RECURRENCE_DATE : RECURRENCE_DATE_TIME;
}

/******************************************************************************
 * Set the event to recur at a minutes interval.
 * Parameters:
 *    freq  = how many minutes between recurrences.
 *    count = number of occurrences, including first and last.
 *          = 0 to use 'end' instead.
 *    end   = end date/time (invalid to use 'count' instead).
 */
void KAlarmEvent::setRecurMinutely(int freq, int count, const QDateTime& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setMinutely(freq, count);
		else
			mRecurrence->setMinutely(freq, end);
	}
}

/******************************************************************************
 * Set the event to recur daily.
 * Parameters:
 *    freq  = how many days between recurrences.
 *    count = number of occurrences, including first and last.
 *          = 0 to use 'end' instead.
 *    end   = end date (invalid to use 'count' instead).
 */
void KAlarmEvent::setRecurDaily(int freq, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setDaily(freq, count);
		else
			mRecurrence->setDaily(freq, end);
	}
}

/******************************************************************************
 * Set the event to recur weekly, on the specified weekdays.
 * Parameters:
 *    freq  = how many weeks between recurrences.
 *    days  = which days of the week alarms should occur on.
 *    count = number of occurrences, including first and last.
 *          = 0 to use 'end' instead.
 *    end   = end date (invalid to use 'count' instead).
 */
void KAlarmEvent::setRecurWeekly(int freq, const QBitArray& days, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setWeekly(freq, days, count);
		else
			mRecurrence->setWeekly(freq, days, end);
	}
}

/******************************************************************************
 * Set the event to recur monthly, on the specified days within the month.
 * Parameters:
 *    freq  = how many months between recurrences.
 *    days  = which days of the month alarms should occur on.
 *    count = number of occurrences, including first and last.
 *          = 0 to use 'end' instead.
 *    end   = end date (invalid to use 'count' instead).
 */
void KAlarmEvent::setRecurMonthlyByDate(int freq, const QValueList<int>& days, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setMonthly(Recurrence::rMonthlyDay, freq, count);
		else
			mRecurrence->setMonthly(Recurrence::rMonthlyDay, freq, end);
		for (QValueListConstIterator<int> it = days.begin();  it != days.end();  ++it)
			mRecurrence->addMonthlyDay(*it);
	}
}

void KAlarmEvent::setRecurMonthlyByDate(int freq, const QPtrList<int>& days, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setMonthly(Recurrence::rMonthlyDay, freq, count);
		else
			mRecurrence->setMonthly(Recurrence::rMonthlyDay, freq, end);
		for (QPtrListIterator<int> it(days);  it.current();  ++it)
			mRecurrence->addMonthlyDay(*it.current());
	}
}

/******************************************************************************
 * Set the event to recur monthly, on the specified weekdays in the specified
 * weeks of the month.
 * Parameters:
 *    freq  = how many months between recurrences.
 *    posns = which days of the week/weeks of the month alarms should occur on.
 *    count = number of occurrences, including first and last.
 *          = 0 to use 'end' instead.
 *    end   = end date (invalid to use 'count' instead).
 */
void KAlarmEvent::setRecurMonthlyByPos(int freq, const QValueList<MonthPos>& posns, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setMonthly(Recurrence::rMonthlyPos, freq, count);
		else
			mRecurrence->setMonthly(Recurrence::rMonthlyPos, freq, end);
		for (QValueListConstIterator<MonthPos> it = posns.begin();  it != posns.end();  ++it)
			mRecurrence->addMonthlyPos((*it).weeknum, (*it).days);
	}
}

void KAlarmEvent::setRecurMonthlyByPos(int freq, const QPtrList<Recurrence::rMonthPos>& posns, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setMonthly(Recurrence::rMonthlyPos, freq, count);
		else
			mRecurrence->setMonthly(Recurrence::rMonthlyPos, freq, end);
		for (QPtrListIterator<Recurrence::rMonthPos> it(posns);  it.current();  ++it)
		{
			short weekno = it.current()->rPos;
			if (it.current()->negative)
				weekno = -weekno;
			mRecurrence->addMonthlyPos(weekno, it.current()->rDays);
		}
	}
}

/******************************************************************************
 * Set the event to recur annually, on the recurrence start date in each of the
 * specified months.
 * Parameters:
 *    freq   = how many years between recurrences.
 *    months = which months of the year alarms should occur on.
 *    count  = number of occurrences, including first and last.
 *           = 0 to use 'end' instead.
 *    end    = end date (invalid to use 'count' instead).
 */
void KAlarmEvent::setRecurAnnualByDate(int freq, const QValueList<int>& months, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setYearly(Recurrence::rYearlyMonth, freq, count);
		else
			mRecurrence->setYearly(Recurrence::rYearlyMonth, freq, end);
		for (QValueListConstIterator<int> it = months.begin();  it != months.end();  ++it)
			mRecurrence->addYearlyNum(*it);
	}
}

void KAlarmEvent::setRecurAnnualByDate(int freq, const QPtrList<int>& months, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setYearly(Recurrence::rYearlyMonth, freq, count);
		else
			mRecurrence->setYearly(Recurrence::rYearlyMonth, freq, end);
		for (QPtrListIterator<int> it(months);  it.current();  ++it)
			mRecurrence->addYearlyNum(*it.current());
	}
}

/******************************************************************************
 * Set the event to recur annually, on the specified weekdays in the specified
 * weeks of the specified month.
 * Parameters:
 *    freq   = how many years between recurrences.
 *    posns  = which days of the week/weeks of the month alarms should occur on.
 *    months = which months of the year alarms should occur on.
 *    count  = number of occurrences, including first and last.
 *           = 0 to use 'end' instead.
 *    end    = end date (invalid to use 'count' instead).
 */
void KAlarmEvent::setRecurAnnualByPos(int freq, const QValueList<MonthPos>& posns, const QValueList<int>& months, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setYearly(Recurrence::rYearlyPos, freq, count);
		else
			mRecurrence->setYearly(Recurrence::rYearlyPos, freq, end);
		for (QValueListConstIterator<int> it = months.begin();  it != months.end();  ++it)
			mRecurrence->addYearlyNum(*it);
		for (QValueListConstIterator<MonthPos> it = posns.begin();  it != posns.end();  ++it)
			mRecurrence->addYearlyMonthPos((*it).weeknum, (*it).days);
	}
}

void KAlarmEvent::setRecurAnnualByPos(int freq, const QPtrList<Recurrence::rMonthPos>& posns, const QPtrList<int>& months, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setYearly(Recurrence::rYearlyMonth, freq, count);
		else
			mRecurrence->setYearly(Recurrence::rYearlyMonth, freq, end);
		for (QPtrListIterator<int> it(months);  it.current();  ++it)
			mRecurrence->addYearlyNum(*it.current());
		for (QPtrListIterator<Recurrence::rMonthPos> it(posns);  it.current();  ++it)
		{
			short weekno = it.current()->rPos;
			if (it.current()->negative)
			weekno = -weekno;
			mRecurrence->addYearlyMonthPos(weekno, it.current()->rDays);
		}
	}
}

/******************************************************************************
 * Set the event to recur annually, on the specified day numbers.
 * Parameters:
 *    freq  = how many years between recurrences.
 *    days  = which days of the year alarms should occur on.
 *    count = number of occurrences, including first and last.
 *          = 0 to use 'end' instead.
 *    end   = end date (invalid to use 'count' instead).
 */
void KAlarmEvent::setRecurAnnualByDay(int freq, const QValueList<int>& days, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setYearly(Recurrence::rYearlyDay, freq, count);
		else
			mRecurrence->setYearly(Recurrence::rYearlyDay, freq, end);
		for (QValueListConstIterator<int> it = days.begin();  it != days.end();  ++it)
			mRecurrence->addYearlyNum(*it);
	}
}

void KAlarmEvent::setRecurAnnualByDay(int freq, const QPtrList<int>& days, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count))
	{
		if (count)
			mRecurrence->setYearly(Recurrence::rYearlyDay, freq, count);
		else
			mRecurrence->setYearly(Recurrence::rYearlyDay, freq, end);
		for (QPtrListIterator<int> it(days);  it.current();  ++it)
			mRecurrence->addYearlyNum(*it.current());
	}
}

/******************************************************************************
 * Initialise the event's recurrence and alarm repetition data, and set the
 * recurrence start date and repetition count if applicable.
 */
bool KAlarmEvent::initRecur(bool endDate, int count)
{
	mUpdated = true;
	if (endDate || count)
	{
		if (!mRecurrence)
			mRecurrence = new Recurrence(0);
		mRecurrence->setRecurStart(mDateTime);
		mRepeatDuration = count;
		return true;
	}
	else
	{
		delete mRecurrence;
		mRecurrence = 0L;
		mRepeatDuration = 0;
		return false;
	}
}

/******************************************************************************
 * Validate the event's recurrence and alarm repetition data, correcting any
 * inconsistencies (which should never occur!).
 * Reply = true if a recurrence (as opposed to a repetition) exists.
 */
KAlarmEvent::RecurType KAlarmEvent::checkRecur() const
{
	if (mRecurrence)
	{
		KAlarmEvent* ev = const_cast<KAlarmEvent*>(this);
		RecurType type = static_cast<RecurType>(mRecurrence->doesRecur());
		switch (type)
		{
			case Recurrence::rMinutely:        // minutely
			case Recurrence::rDaily:           // daily
			case Recurrence::rWeekly:          // weekly on multiple days of week
			case Recurrence::rMonthlyDay:      // monthly on multiple dates in month
			case Recurrence::rMonthlyPos:      // monthly on multiple nth day of week
			case Recurrence::rYearlyMonth:     // annually on multiple months (day of month = start date)
			case Recurrence::rYearlyPos:       // annually on multiple nth day of week in multiple months
			case Recurrence::rYearlyDay:       // annually on multiple day numbers in year
				return type;
			case Recurrence::rHourly:          // hourly
				return MINUTELY;
			case Recurrence::rNone:
			default:
				delete mRecurrence;
				ev->mRecurrence = 0;
				break;
		}
	}
	return NO_RECUR;
}

/******************************************************************************
 * Return the recurrence interval in units of the recurrence period type.
 */
int KAlarmEvent::recurInterval() const
{
	if (mRecurrence)
	{
		switch (mRecurrence->doesRecur())
		{
			case Recurrence::rMinutely:
			case Recurrence::rDaily:
			case Recurrence::rWeekly:
			case Recurrence::rMonthlyDay:
			case Recurrence::rMonthlyPos:
			case Recurrence::rYearlyMonth:
			case Recurrence::rYearlyPos:
			case Recurrence::rYearlyDay:
				return mRecurrence->frequency();
			case Recurrence::rHourly:
				return mRecurrence->frequency() * 60;
			case Recurrence::rNone:
			default:
				break;
		}
	}
	return 0;
}

/******************************************************************************
 * Adjust the time at which date-only events will occur for each of the events
 * in a list. Events for which both date and time are specified are left
 * unchanged.
 * Reply = true if any events have been updated.
 */
bool KAlarmEvent::adjustStartOfDay(const QPtrList<Event>& events)
{
	bool changed = false;
	QTime startOfDay = theApp()->settings()->startOfDay();
	for (QPtrListIterator<Event> it(events);  it.current();  ++it)
	{
		Event* event = it.current();
		if (event->doesFloat())
		{
			// It's an untimed event, so fix it
			QPtrList<Alarm> alarms = event->alarms();
			for (QPtrListIterator<Alarm> it(alarms);  it.current();  ++it)
			{
				// Parse the next alarm's text
				Alarm& alarm = *it.current();
				AlarmData data;
				if (readAlarm(alarm, data) == MAIN_ALARM_ID)
				{
					alarm.setTime(QDateTime(alarm.time().date(), startOfDay));
					changed = true;
					break;
				}
			}
		}
	}
	return changed;
}

#ifndef NDEBUG
void KAlarmEvent::dumpDebug() const
{
	kdDebug(5950) << "KAlarmEvent dump:\n";
	kdDebug(5950) << "-- mEventID:" << mEventID << ":\n";
	kdDebug(5950) << "-- mCleanText:" << mCleanText << ":\n";
	kdDebug(5950) << "-- mDateTime:" << mDateTime.toString() << ":\n";
	kdDebug(5950) << "-- mRepeatAtLoginDateTime:" << mRepeatAtLoginDateTime.toString() << ":\n";
	kdDebug(5950) << "-- mDeferralTime:" << mDeferralTime.toString() << ":\n";
	kdDebug(5950) << "-- mColour:" << mColour.name() << ":\n";
	kdDebug(5950) << "-- mRevision:" << mRevision << ":\n";
	kdDebug(5950) << "-- mMainAlarmID:" << mMainAlarmID << ":\n";
	kdDebug(5950) << "-- mRepeatAtLoginAlarmID:" << mRepeatAtLoginAlarmID << ":\n";
	kdDebug(5950) << "-- mRecurrence:" << (mRecurrence ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mRepeatDuration:" << mRepeatDuration << ":\n";
	kdDebug(5950) << "-- mBeep:" << (mBeep ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mType:" << mType << ":\n";
	kdDebug(5950) << "-- mRepeatAtLogin:" << (mRepeatAtLogin ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mDeferral:" << (mDeferral ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mLateCancel:" << (mLateCancel ? "true" : "false") << ":\n";
	kdDebug(5950) << "KAlarmEvent dump end\n";
}
#endif


/*=============================================================================
= Class KAlarmAlarm
= Corresponds to a single KCal::Alarm instance.
=============================================================================*/

void KAlarmAlarm::set(int flags)
{
	mBeep          = flags & KAlarmEvent::BEEP;
	mRepeatAtLogin = flags & KAlarmEvent::REPEAT_AT_LOGIN;
	mLateCancel    = flags & KAlarmEvent::LATE_CANCEL;
	mDeferral      = flags & KAlarmEvent::DEFERRAL;
}

int KAlarmAlarm::flags() const
{
	return (mBeep          ? KAlarmEvent::BEEP : 0)
	     | (mRepeatAtLogin ? KAlarmEvent::REPEAT_AT_LOGIN : 0)
	     | (mLateCancel    ? KAlarmEvent::LATE_CANCEL : 0)
	     | (mDeferral      ? KAlarmEvent::DEFERRAL : 0);
}

// Convert a string to command arguments
void KAlarmAlarm::commandArgs(QStringList& list) const
{
	list.clear();
	if (mType != COMMAND)
		return;
	int imax = mCleanText.length();
	for (int i = 0;  i < imax;  )
	{
		// Find the first non-space
		if ((i = mCleanText.find(QRegExp("[^\\s]"), i)) < 0)
			break;

		// Find the end of the next parameter.
		// Allow for quoted parameters, and also for escaped characters.
		int j, jmax;
		QChar quote = mCleanText[i];
		if (quote == QChar('\'')  ||  quote == QChar('"'))
		{
			for (j = i + 1;  j < imax; )
			{
				QChar ch = mCleanText[j++];
				if (ch == quote)
					break;
				if (ch == QChar('\\')  &&  j < imax)
					++j;
			}
			jmax = j;
		}
		else
		{
			for (j = i;  j < imax;  ++j)
			{
				QChar ch = mCleanText[j];
				if (ch.isSpace())
					break;
				if (ch == QChar('\\')  &&  j < imax - 1)
					++j;
			}
			jmax = j;
		}
		list.append(mCleanText.mid(i, jmax - i));
		i = j;
	}
}

// Convert a command with arguments to a string
QString KAlarmAlarm::commandFromArgs(const QStringList& list)
{
	if (list.isEmpty())
		return QString("");
	QString cmd;
	QStringList::ConstIterator it = list.begin();
	for ( ;  it != list.end();  ++it)
	{
		QString value = *it;
		if (value.find(QRegExp("\\s")) >= 0)
		{
			// Argument has spaces in it, so enclose it in quotes and
			// escape any quotes within it.
			const QChar quote('"');
			cmd += quote;
			for (unsigned i = 0;  i < value.length();  ++i)
			{
				if (value[i] == quote  ||  value[i] == QChar('\\'))
					cmd += QChar('\\');
				cmd += value[i];
			}
			cmd += quote;
		}
		else
		{
			// Argument has no spaces in it
			for (unsigned i = 0;  i < value.length();  ++i)
			{
				if (value[i] == QChar('\\'))
					cmd += QChar('\\');
				cmd += value[i];
			}
		}
		cmd += QChar(' ');
	}
	cmd.truncate(cmd.length() - 1);      // remove the trailing space
	return cmd;
}

#ifndef NDEBUG
void KAlarmAlarm::dumpDebug() const
{
	kdDebug(5950) << "KAlarmAlarm dump:\n";
	kdDebug(5950) << "-- mEventID:" << mEventID << ":\n";
	kdDebug(5950) << "-- mCleanText:" << mCleanText << ":\n";
	kdDebug(5950) << "-- mDateTime:" << mDateTime.toString() << ":\n";
	kdDebug(5950) << "-- mColour:" << mColour.name() << ":\n";
	kdDebug(5950) << "-- mAlarmSeq:" << mAlarmSeq << ":\n";
	kdDebug(5950) << "-- mBeep:" << (mBeep ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mType:" << mType << ":\n";
	kdDebug(5950) << "-- mRepeatAtLogin:" << (mRepeatAtLogin ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mDeferral:" << (mDeferral ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mLateCancel:" << (mLateCancel ? "true" : "false") << ":\n";
	kdDebug(5950) << "KAlarmAlarm dump end\n";
}
#endif
