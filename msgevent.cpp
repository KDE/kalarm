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
#include "msgevent.h"
using namespace KCal;


/*
 * Each alarm DESCRIPTION field contains the following:
 *   SEQNO;[FLAGS];TYPE:TEXT
 * where
 *   SEQNO = sequence number of alarm within the event
 *   FLAGS = C for late-cancel, L for repeat-at-login
 *   TYPE = TEXT or FILE or CMD
 *   TEXT = message text, file name/URL or command
 */
static const QChar   SEPARATOR        = QChar(';');
static const QString TEXT_PREFIX      = QString::fromLatin1("TEXT:");
static const QString FILE_PREFIX      = QString::fromLatin1("FILE:");
static const QString COMMAND_PREFIX   = QString::fromLatin1("CMD:");
static const QString LATE_CANCEL_CODE = QString::fromLatin1("C");
static const QString AT_LOGIN_CODE    = QString::fromLatin1("L");   // subsidiary alarm at every login
static const QString BEEP_CATEGORY    = QString::fromLatin1("BEEP");

struct AlarmData
{
	AlarmData() : type(KAlarmAlarm::MESSAGE), lateCancel(false), repeatAtLogin(false) { }
	QString           cleanText;
	QDateTime         dateTime;
	int               repeatCount;
	int               repeatMinutes;
	KAlarmAlarm::Type type;
	bool              lateCancel;
	bool              repeatAtLogin;
};
typedef QMap<int, AlarmData> AlarmMap;


/*=============================================================================
= Class KAlarmEvent
= Corresponds to a KCal::Event instance.
=============================================================================*/

const int KAlarmEvent::REPEAT_AT_LOGIN_OFFSET = 1;


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
		Alarm* alarm = it.current();
		int sequence = 1;
		AlarmData data;
		const QString& txt = alarm->text();
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
						}
					}
					else
					{
						i = 0;
						sequence = 1;
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
		data.dateTime      = alarm->time();
		data.repeatCount   = alarm->repeatCount();
		data.repeatMinutes = alarm->snoozeTime();

		alarmMap.insert(sequence, data);
	}

	// Incorporate the alarms' details into the overall event
	AlarmMap::ConstIterator it = alarmMap.begin();
	mMainAlarmID = -1;    // initialise as invalid
	mAlarmCount = 0;
	bool set = false;
	for (  ;  it != alarmMap.end();  ++it)
	{
		const AlarmData& data = it.data();
		if (data.repeatAtLogin)
		{
			mRepeatAtLogin = true;
			mRepeatAtLoginDateTime = data.dateTime;
			mRepeatAtLoginAlarmID = it.key();
		}
		else
			mMainAlarmID = it.key();

		// Ensure that the basic fields are set up even if the repeat-at-login
		// alarm is the only alarm in the event (which shouldn't happen!)
		if (!data.repeatAtLogin  ||  !set)
		{
			mType           = data.type;
			mCleanText      = (mType == KAlarmAlarm::COMMAND) ? data.cleanText.stripWhiteSpace() : data.cleanText;
			mDateTime       = data.dateTime;
			mRepeatDuration = data.repeatCount;
			mRepeatMinutes  = data.repeatMinutes;
			mLateCancel     = data.lateCancel;
			set = true;
		}
		++mAlarmCount;
	}

	Recurrence* recur = event.recurrence();
	if (recur)
	{
		// Copy the recurrence details.
		// This will clear any hours/minutes repetition details.
		switch (recur->doesRecur())
		{
			case Recurrence::rDaily:
				setRecurDaily(recur->frequency(), recur->duration(), recur->endDate());
					break;
			case Recurrence::rWeekly:
				setRecurWeekly(recur->frequency(), recur->days(), recur->duration(), recur->endDate());
				break;
			case Recurrence::rMonthlyDay:
				setRecurMonthlyByDate(recur->frequency(), recur->monthDays(), recur->duration(), recur->endDate());
				break;
			case Recurrence::rMonthlyPos:
				setRecurMonthlyByPos(recur->frequency(), recur->monthPositions(), recur->duration(), recur->endDate());
				break;
			case Recurrence::rYearlyMonth:
				setRecurAnnualByDate(recur->frequency(), recur->yearNums(), recur->duration(), recur->endDate());
				break;
			case Recurrence::rYearlyDay:
				setRecurAnnualByDay(recur->frequency(), recur->yearNums(), recur->duration(), recur->endDate());
				break;
			default:
				break;
		}
	}

	mUpdated = false;
}

void KAlarmEvent::set(const QDateTime& dateTime, const QString& text, const QColor& colour, KAlarmAlarm::Type type, int flags, int repeatCount, int repeatInterval)
{
	initRecur(false);
	mDateTime       = dateTime;
	mCleanText      = (mType == KAlarmAlarm::COMMAND) ? text.stripWhiteSpace() : text;
	mType           = type;
	mColour         = colour;
	mRepeatDuration = repeatCount;
	mRepeatMinutes  = repeatInterval;
	set(flags);
	mUpdated = false;
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
	     | (mAnyTime       ? ANY_TIME : 0);
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

	// Add the appropriate alarms
	int sequence = 1;
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
	al->setText(QString::number(sequence) + SEPARATOR + suffix);
	al->setTime(mDateTime);
	al->setRepeatCount(mRepeatMinutes ? mRepeatDuration : 0);
	al->setSnoozeTime(mRepeatMinutes);
	QDateTime dt = mDateTime;
	if (mRepeatAtLogin)
	{
		al = ev.newAlarm();
		al->setEnabled(true);        // enable the alarm
		al->setText(QString::number(sequence + REPEAT_AT_LOGIN_OFFSET)
		            + SEPARATOR + AT_LOGIN_CODE + suffix);
		QDateTime dtl = mRepeatAtLoginDateTime.isValid() ? mRepeatAtLoginDateTime
		                : QDateTime::currentDateTime();
		al->setTime(dtl);
		if (dtl < dt)
			dt = dtl;
	}

	// Add recurrence data
	if (mRecurrence)
	{
		Recurrence* recur = ev.recurrence();
		int frequency = mRecurrence->frequency();
		int duration  = mRecurrence->duration();
		const QDate& endDate = mRecurrence->endDate();
		recur->setRecurStart(mDateTime.date());
		switch (mRecurrence->doesRecur())
		{
			case Recurrence::rDaily:
				if (duration)
					recur->setDaily(frequency, duration);
				else
					recur->setDaily(frequency, endDate);
				break;
			case Recurrence::rWeekly:
				if (duration)
					recur->setWeekly(frequency, mRecurrence->days(), duration);
				else
					recur->setWeekly(frequency, mRecurrence->days(), endDate);
				break;
			case Recurrence::rMonthlyDay:
			{
				if (duration)
					recur->setMonthly(Recurrence::rMonthlyDay, frequency, duration);
				else
					recur->setMonthly(Recurrence::rMonthlyDay, frequency, endDate);
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
					recur->setMonthly(Recurrence::rMonthlyPos, frequency, endDate);
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
				if (duration)
					recur->setYearly(Recurrence::rYearlyMonth, frequency, duration);
				else
					recur->setYearly(Recurrence::rYearlyMonth, frequency, endDate);
				break;
			case Recurrence::rYearlyDay:
			{
				if (duration)
					recur->setYearly(Recurrence::rYearlyDay, frequency, duration);
				else
					recur->setYearly(Recurrence::rYearlyDay, frequency, endDate);
				const QPtrList<int>& ynums = mRecurrence->yearNums();
				for (QPtrListIterator<int> it(ynums);  it.current();  ++it)
					recur->addYearlyNum(*it.current());
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
		al.mRepeatCount   = mRepeatDuration;
		al.mRepeatMinutes = mRepeatMinutes;
		al.mLateCancel    = mLateCancel;
	}
	else if (alarmID == mRepeatAtLoginAlarmID  &&  mRepeatAtLogin)
	{
		al.mAlarmSeq      = mRepeatAtLoginAlarmID;
		al.mDateTime      = mRepeatAtLoginDateTime;
		al.mRepeatAtLogin = true;
	}
	return al;
}

KAlarmAlarm KAlarmEvent::firstAlarm() const
{
	if (mMainAlarmID > 0)
		return alarm(mMainAlarmID);
	if (mRepeatAtLogin)
		return alarm(mRepeatAtLoginAlarmID);
	return KAlarmAlarm();
}

KAlarmAlarm KAlarmEvent::nextAlarm(const KAlarmAlarm& alrm) const
{
	if (alrm.id() != mMainAlarmID  ||  !mRepeatAtLogin)
		return KAlarmAlarm();
	return alarm(mRepeatAtLoginAlarmID);
}

void KAlarmEvent::removeAlarm(int alarmID)
{
	if (alarmID == mMainAlarmID)
		mAlarmCount = 0;    // removing main alarm - also remove subsidiary alarms
	else if (alarmID == mRepeatAtLoginAlarmID)
	{
	   mRepeatAtLogin = false;
	   --mAlarmCount;
	}
}

/******************************************************************************
 * Check whether the event repeats - with a recurrence specification and/or an
 * alarm repetition.
 */
bool KAlarmEvent::repeats() const
{
	return (checkRecur() || mRepeatDuration);
}

/******************************************************************************
 * Get the date/time of the next occurrence of the event, after the specified
 * date/time.
 * 'result' = date/time of next occurrence, or invalid date/time if none.
 */
KAlarmEvent::NextOccurType KAlarmEvent::getNextOccurrence(const QDateTime& preDateTime, QDateTime& result) const
{
	if (checkRecur())
		return nextRecurrence(preDateTime, result);
	if (mRepeatDuration)
	{
		int remainingCount;
		return nextRepetition(preDateTime, result, remainingCount);
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
 * Set the date/time of the event to the next scheduled occurrence after the
 * specified date/time.
 */
KAlarmEvent::NextOccurType KAlarmEvent::setNextOccurrence(const QDateTime& preDateTime)
{
	if (preDateTime < mDateTime)
		return FIRST_OCCURRENCE;
	NextOccurType type;
	if (checkRecur())
	{
		QDateTime newTime;
		type = nextRecurrence(preDateTime, newTime);
		if (type != FIRST_OCCURRENCE  &&  type != NO_OCCURRENCE)
			mDateTime = newTime;
	}
	else if (mRepeatDuration)
	{
		int remainingCount;
		QDateTime newTime;
		type = nextRepetition(preDateTime, newTime, remainingCount);
		if (type != FIRST_OCCURRENCE  &&  type != NO_OCCURRENCE)
		{
			mDateTime = newTime;
			mRepeatDuration = remainingCount;
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
 */
KAlarmEvent::NextOccurType KAlarmEvent::nextRecurrence(const QDateTime& preDateTime, QDateTime& result) const
{
	result = mRecurrence->getNextRecurrence(preDateTime.date());
	if (!result.isValid())
		return NO_OCCURRENCE;
	QDateTime recurStart = mRecurrence->recurStart();
	if (!mAnyTime)
		result.setTime(recurStart.time());
	if (result.date() == recurStart.date())
		return FIRST_OCCURRENCE;
	return mAnyTime ? RECURRENCE_DATE : RECURRENCE_DATE_TIME;
}

/******************************************************************************
 * Get the date/time of the next repetition of the event, after the specified
 * date/time.
 * 'result' = date/time of next occurrence, or invalid date/time if none.
 */
KAlarmEvent::NextOccurType KAlarmEvent::nextRepetition(const QDateTime& preDateTime, QDateTime& result, int& remainingCount) const
{
	KAlarmAlarm al = alarm(mMainAlarmID);
	remainingCount = al.nextRepetition(preDateTime, result);
	if (remainingCount == 0)
		return NO_OCCURRENCE;
	if (remainingCount > al.repeatCount())
		return FIRST_OCCURRENCE;
	return RECURRENCE_DATE_TIME;
}

/******************************************************************************
 * Set the event to recur at an hours/minutes interval.
 */
void KAlarmEvent::setRecurSubDaily(int freq, int minuteCount, const QDateTime& end)
{
	initRecur(false);
	if (minuteCount || end.isValid())
	{
		mRepeatMinutes = freq;
		if (minuteCount)
			mRepeatDuration = minuteCount;
		else
			mRepeatDuration = (mDateTime.secsTo(end) / 60) / freq  + 1;
	}
}

/******************************************************************************
 * Set the event to recur daily.
 */
void KAlarmEvent::setRecurDaily(int freq, int dayCount, const QDate& end)
{
	if (initRecur(dayCount || end.isValid()))
	{
		if (dayCount)
			mRecurrence->setDaily(freq, dayCount);
		else
			mRecurrence->setDaily(freq, end);
	}
}

/******************************************************************************
 * Set the event to recur weekly, on the specified weekdays.
 */
void KAlarmEvent::setRecurWeekly(int freq, const QBitArray& days, int weekCount, const QDate& end)
{
	if (initRecur(weekCount || end.isValid()))
	{
		if (weekCount)
			mRecurrence->setWeekly(freq, days, weekCount);
		else
			mRecurrence->setWeekly(freq, days, end);
	}
}

/******************************************************************************
 * Set the event to recur monthly, on the specified days within the month.
 */
void KAlarmEvent::setRecurMonthlyByDate(int freq, const QValueList<int>& days, int monthCount, const QDate& end)
{
	if (initRecur(monthCount || end.isValid()))
	{
		if (monthCount)
			mRecurrence->setMonthly(Recurrence::rMonthlyDay, freq, monthCount);
		else
			mRecurrence->setMonthly(Recurrence::rMonthlyDay, freq, end);
		for (QValueListConstIterator<int> it = days.begin();  it != days.end();  ++it)
			mRecurrence->addMonthlyDay(*it);
	}
}

void KAlarmEvent::setRecurMonthlyByDate(int freq, const QPtrList<int>& days, int monthCount, const QDate& end)
{
	if (initRecur(monthCount || end.isValid()))
	{
		if (monthCount)
			mRecurrence->setMonthly(Recurrence::rMonthlyDay, freq, monthCount);
		else
			mRecurrence->setMonthly(Recurrence::rMonthlyDay, freq, end);
		for (QPtrListIterator<int> it(days);  it.current();  ++it)
			mRecurrence->addMonthlyDay(*it.current());
	}
}

/******************************************************************************
 * Set the event to recur monthly, on the specified weekdays in the specified
 * weeks of the month.
 */
void KAlarmEvent::setRecurMonthlyByPos(int freq, const QValueList<MonthPos>& posns, int monthCount, const QDate& end)
{
	if (initRecur(monthCount || end.isValid()))
	{
		if (monthCount)
			mRecurrence->setMonthly(Recurrence::rMonthlyPos, freq, monthCount);
		else
			mRecurrence->setMonthly(Recurrence::rMonthlyPos, freq, end);
		for (QValueListConstIterator<MonthPos> it = posns.begin();  it != posns.end();  ++it)
			mRecurrence->addMonthlyPos((*it).weeknum, (*it).days);
	}
}

void KAlarmEvent::setRecurMonthlyByPos(int freq, const QPtrList<Recurrence::rMonthPos>& posns, int monthCount, const QDate& end)
{
	if (initRecur(monthCount || end.isValid()))
	{
		if (monthCount)
			mRecurrence->setMonthly(Recurrence::rMonthlyPos, freq, monthCount);
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
 */
void KAlarmEvent::setRecurAnnualByDate(int freq, const QValueList<int>& months, int yearCount, const QDate& end)
{
	if (initRecur(yearCount || end.isValid()))
	{
		if (yearCount)
			mRecurrence->setYearly(Recurrence::rYearlyMonth, freq, yearCount);
		else
			mRecurrence->setYearly(Recurrence::rYearlyMonth, freq, end);
		for (QValueListConstIterator<int> it = months.begin();  it != months.end();  ++it)
			mRecurrence->addYearlyNum(*it);
	}
}

void KAlarmEvent::setRecurAnnualByDate(int freq, const QPtrList<int>& months, int yearCount, const QDate& end)
{
	if (initRecur(yearCount || end.isValid()))
	{
		if (yearCount)
			mRecurrence->setYearly(Recurrence::rYearlyMonth, freq, yearCount);
		else
			mRecurrence->setYearly(Recurrence::rYearlyMonth, freq, end);
		for (QPtrListIterator<int> it(months);  it.current();  ++it)
			mRecurrence->addYearlyNum(*it.current());
	}
}

/******************************************************************************
 * Set the event to recur annually, on the specified day numbers.
 */
void KAlarmEvent::setRecurAnnualByDay(int freq, const QValueList<int>& days, int yearCount, const QDate& end)
{
	if (initRecur(yearCount || end.isValid()))
	{
		if (yearCount)
			mRecurrence->setYearly(Recurrence::rYearlyDay, freq, yearCount);
		else
			mRecurrence->setYearly(Recurrence::rYearlyDay, freq, end);
		for (QValueListConstIterator<int> it = days.begin();  it != days.end();  ++it)
			mRecurrence->addYearlyNum(*it);
	}
}

void KAlarmEvent::setRecurAnnualByDay(int freq, const QPtrList<int>& days, int yearCount, const QDate& end)
{
	if (initRecur(yearCount || end.isValid()))
	{
		if (yearCount)
			mRecurrence->setYearly(Recurrence::rYearlyDay, freq, yearCount);
		else
			mRecurrence->setYearly(Recurrence::rYearlyDay, freq, end);
		for (QPtrListIterator<int> it(days);  it.current();  ++it)
			mRecurrence->addYearlyNum(*it.current());
	}
}

/******************************************************************************
 * Initialise the event's recurrence and alarm repetition data, and set the
 * recurrence start date if applicable.
 */
bool KAlarmEvent::initRecur(bool recurs)
{
	if (recurs)
	{
		if (!mRecurrence)
			mRecurrence = new KAlarmRecurrence(0L);
		mRecurrence->setRecurStart(mDateTime);
	}
	else
	{
		delete mRecurrence;
		mRecurrence = 0L;
	}
	mRepeatDuration = mRepeatMinutes = 0;
	return recurs;
}

/******************************************************************************
 * Validate the event's recurrence and alarm repetition data, correcting any
 * inconsistencies (which should never occur!).
 * Reply = true if a recurrence (as opposed to a repetition) exists.
 */
bool KAlarmEvent::checkRecur() const
{
	if (mRecurrence)
	{
		KAlarmEvent* ev = const_cast<KAlarmEvent*>(this);
		switch (mRecurrence->doesRecur())
		{
			case Recurrence::rDaily:           // daily
			case Recurrence::rWeekly:          // weekly on multiple days of week
			case Recurrence::rMonthlyDay:      // monthly on multiple dates in month
			case Recurrence::rMonthlyPos:      // monthly on multiple nth day of week
			case Recurrence::rYearlyMonth:     // annually on multiple months (day of month = start date)
			case Recurrence::rYearlyDay:       // annually on multiple day numbers in year
				ev->mRepeatDuration = ev->mRepeatMinutes = 0;
				return true;
			case Recurrence::rNone:
			default:
				delete mRecurrence;
				ev->mRecurrence = 0L;
				break;
		}
	}
	return false;
}

#ifndef NDEBUG
void KAlarmEvent::dumpDebug() const
{
	kdDebug(5950) << "KAlarmEvent dump:\n";
	kdDebug(5950) << "-- mEventID:" << mEventID << ":\n";
	kdDebug(5950) << "-- mCleanText:" << mCleanText << ":\n";
	kdDebug(5950) << "-- mDateTime:" << mDateTime.toString() << ":\n";
	kdDebug(5950) << "-- mRepeatAtLoginDateTime:" << mRepeatAtLoginDateTime.toString() << ":\n";
	kdDebug(5950) << "-- mColour:" << mColour.name() << ":\n";
	kdDebug(5950) << "-- mRevision:" << mRevision << ":\n";
	kdDebug(5950) << "-- mMainAlarmID:" << mMainAlarmID << ":\n";
	kdDebug(5950) << "-- mRepeatAtLoginAlarmID:" << mRepeatAtLoginAlarmID << ":\n";
	kdDebug(5950) << "-- mRepeatDuration:" << mRepeatDuration << ":\n";
	kdDebug(5950) << "-- mRepeatMinutes:" << mRepeatMinutes << ":\n";
	kdDebug(5950) << "-- mBeep:" << (mBeep ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mType:" << mType << ":\n";
	kdDebug(5950) << "-- mRepeatAtLogin:" << (mRepeatAtLogin ? "true" : "false") << ":\n";
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
}

int KAlarmAlarm::flags() const
{
	return (mBeep          ? KAlarmEvent::BEEP : 0)
	     | (mRepeatAtLogin ? KAlarmEvent::REPEAT_AT_LOGIN : 0)
	     | (mLateCancel    ? KAlarmEvent::LATE_CANCEL : 0);
}

/******************************************************************************
 * Get the date/time of the next repetition, after the specified date/time.
 * 'result' = date/time of next repetition, or invalid date/time if none.
 * Reply = number of repetitions still due, including the first occurrence
 */
int KAlarmAlarm::nextRepetition(const QDateTime& preDateTime, QDateTime& result) const
{
	QDateTime earliestTime = preDateTime.addSecs(1);
	int secs = mDateTime.secsTo(earliestTime);
	if (secs <= 0)
	{
		// Alarm is not due by the specified time
		result = mDateTime;
		return mRepeatCount + 1;
	}

	int repeatSecs = mRepeatMinutes * 60;
	if (repeatSecs)
	{
		int nextRepeatCount = (secs + repeatSecs - 1) / repeatSecs;
		int remainingCount = mRepeatCount - nextRepeatCount;
		if (remainingCount >= 0)
		{
			result = mDateTime.addSecs(nextRepeatCount * repeatSecs);
			return remainingCount + 1;
		}
	}
	result = QDateTime();
	return 0;
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
	kdDebug(5950) << "-- mRepeatCount:" << mRepeatCount << ":\n";
	kdDebug(5950) << "-- mRepeatMinutes:" << mRepeatMinutes << ":\n";
	kdDebug(5950) << "-- mBeep:" << (mBeep ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mType:" << mType << ":\n";
	kdDebug(5950) << "-- mRepeatAtLogin:" << (mRepeatAtLogin ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mLateCancel:" << (mLateCancel ? "true" : "false") << ":\n";
	kdDebug(5950) << "KAlarmAlarm dump end\n";
}
#endif





/******************************************************************************
 * Get the date of the next recurrence, after the specified date.
 * Reply = date of next recurrence, or invalid date if none.
 */
QDate KAlarmRecurrence::getNextRecurrence(const QDate& preDate) const
{
	QDate dStart = recurStart().date();
	if (preDate < dStart)
		return dStart;
	QDate nextDate;
	QDate endDate = rEndDate;
	int endCount = rDuration - 1 + recurExDatesCount();
	switch (recurs)
	{
		case rDaily:
			nextDate = preDate.addDays(rFreq);
			endDate  = dStart.addDays(endCount * rFreq);
			break;

		case rWeekly:
		{
			int day = preDate.dayOfWeek();     // 1 .. 7
			// First check for any remaining day this week
			int weekday = getFirstDayInWeek(day + 1);
			if (!weekday)
			{
				// Check for a day in the next scheduled week
				weekday = getFirstDayInWeek(1) + rFreq*7;
			}
			if (weekday)
				nextDate = preDate.addDays(weekday - day);
			endDate = dStart.addDays((endCount * 7) + (7 - dStart.dayOfWeek()) * rFreq);
			break;
		}
		case rMonthlyDay:
		case rMonthlyPos:
		{
			// Check for the first later day in the current month
			int nextDay = preDate.day() + 1;
			if (nextDay <= preDate.daysInMonth())
				nextDate = getFirstDateInMonth(preDate.addDays(1));
			if (!nextDate.isValid())
			{
				// Check for a day in the next scheduled month
				int month = preDate.month() + rFreq - 1;
				int year = preDate.year() + month/12;
				month %= 12;
				nextDate = getFirstDateInMonth(QDate(year, month + 1, 1));
			}
			int months = dStart.month() + endCount * rFreq;   // month after end
			endDate = QDate(dStart.year() + months/12, months%12 + 1, 1).addDays(-1);
			break;
		}
		case rYearlyMonth:
		{
			// Check for the first later month in the current year
			int day  = preDate.day();
			int year = preDate.year();
			int nextMonth = preDate.month() + 1;
			if (nextMonth <= 12)
				nextDate = getFirstMonthInYear(QDate(year, nextMonth, day));
			if (!nextDate.isValid())
			{
				// Check for a month in the next scheduled year
				nextDate = getFirstMonthInYear(QDate(year + rFreq, 1, day));
			}
			endDate = QDate(dStart.year() + endCount * rFreq, 12, 31);
			break;
		}
		case rYearlyDay:
		{
			// Check for the first later day in the current year
			QDate date(preDate.addDays(1));
			int year = preDate.year();
			if (date.year() == year)
				nextDate = getFirstDayInYear(date);
			if (!nextDate.isValid())
			{
				// Check for a day in the next scheduled year
				nextDate = getFirstDayInYear(QDate(year + rFreq, 1, 1));
			}
			endDate = QDate(dStart.year() + endCount * rFreq, 12, 31);
			break;
		}
		case rNone:
		default:
			return QDate();
	}

	if (nextDate.isValid())
	{
		// Check that the date found is within the range of the recurrence
		if (rDuration >= 0  &&  nextDate > endDate)
			return QDate();
	}
	return nextDate;
}

/******************************************************************************
 * From the recurrence day of the week list, get the earliest day in the
 * specified week which is >= the startDay.
 * Parameters:  startDay = 1..7
 * Reply = day of the week (1..7), or 0 if none found.
 */
int KAlarmRecurrence::getFirstDayInWeek(int startDay) const
{
	for (int i = startDay - 1;  i < 7;  ++i)
		if (rDays.testBit(i))
			return i + 1;
	return 0;
}

/******************************************************************************
 * From the recurrence monthly day number list or monthly day of week/week of
 * month list, get the earliest day in the specified month which is >= the
 * earliestDate.
 */
QDate KAlarmRecurrence::getFirstDateInMonth(const QDate& earliestDate) const
{
	int earliestDay = earliestDate.day();
	int daysInMonth = earliestDate.daysInMonth();
	int minday = daysInMonth + 1;
	if (recurs == rMonthlyDay)
	{
		for (QPtrListIterator<int> it(rMonthDays);  it.current();  ++it)
		{
			int day = *it.current();
			if (day < 0)
				day = daysInMonth + day + 1;
			if (day >= earliestDay  &&  day < minday)
				minday = day;
		}
	}
	else
	{
		QDate monthBegin(earliestDate.year(), earliestDate.month(), 1);
		QDate monthEnd(monthBegin.addDays(daysInMonth - 1));
		int monthEndDayOfWeek = monthEnd.dayOfWeek();
		int earliestWeek = (earliestDay + 6)/7;     // 1..5
		int earliestDayOfWeek = monthBegin.addDays(earliestDay - 1).dayOfWeek();
		for (QPtrListIterator<rMonthPos> it(rMonthPositions);  it.current();  ++it)
		{
			if (it.current()->negative)
			{
				int latestDay = daysInMonth - (it.current()->rPos - 1) * 7;
				if (latestDay >= earliestDay)
				{
					for (int i = 0;  i < 7;  ++i)
						if (rDays.testBit(i))
						{
							int dayno = latestDay - (monthEndDayOfWeek - i + 7) % 7;
							if (dayno < minday)
								minday = dayno;
						}
				}
			}
			else
			{
				int diff = it.current()->rPos - earliestWeek;
				if (diff >= 0)
				{
					for (int i = (diff ? 0 : earliestDayOfWeek - 1);  i < 7;  ++i)
						if (rDays.testBit(i))
						{
							int dayno = earliestDay + diff*7 + i - (earliestDayOfWeek - 1);
							if (dayno < minday)
								minday = dayno;
							break;
						}
				}
			}
		}
	}
	if (minday > daysInMonth)
		return QDate();
	return QDate(earliestDate.year(), earliestDate.month(), minday);
}

/******************************************************************************
 * From the recurrence yearly month list, get the earliest month in the
 * specified year which is >= the earliestDate.
 */
QDate KAlarmRecurrence::getFirstMonthInYear(const QDate& earliestDate) const
{
	int earliestYear  = earliestDate.year();
	int earliestMonth = earliestDate.month();
	int earliestDay   = earliestDate.day();
	int minmonth = 13;
	for (QPtrListIterator<int> it(rYearNums);  it.current();  ++it)
	{
		int month = *it.current();
		if (month >= earliestMonth  &&  month < minmonth
		&&  QDate::isValid(earliestYear, month, earliestDay))
			minmonth = month;
	}
	if (minmonth > 12)
		return QDate();
	return QDate(earliestYear, minmonth, earliestDay);
}

/******************************************************************************
 * From the recurrence yearly day list, get the earliest day in the
 * specified year which is >= the earliestDate.
 */
QDate KAlarmRecurrence::getFirstDayInYear(const QDate& earliestDate) const
{
	int earliestDay = earliestDate.dayOfYear();
	int minday = 1000;
	for (QPtrListIterator<int> it(rYearNums);  it.current();  ++it)
	{
		int day = *it.current();
		if (day >= earliestDay  &&  day < minday)
			minday = day;
	}
	if (minday > earliestDate.daysInYear())
		return QDate();
	return QDate(earliestDate.year(), 1, 1).addDays(minday - 1);
}
