/*
 *  alarmevent.cpp  -  represents calendar alarms and events
 *  Program:  kalarm
 *  Copyright © 2001-2009 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"   //krazy:exclude=includes (kalarm.h must be first)
#include "alarmevent.h"

#include "alarmresource.h"
#include "functions.h"

#include <kholidays/holidays.h>

#include <ksystemtimezone.h>
#include <kdebug.h>

using namespace KCal;
using namespace KHolidays;


// Command error strings
///DONE
QString KAEvent::mCmdErrConfigGroup = QLatin1String("CommandErrors");
static const QString CMD_ERROR_VALUE      = QLatin1String("MAIN");
static const QString CMD_ERROR_PRE_VALUE  = QLatin1String("PRE");
static const QString CMD_ERROR_POST_VALUE = QLatin1String("POST");


/*=============================================================================
= Class KAEvent
= Corresponds to a KCal::Event instance.
=============================================================================*/

KAEvent::KAEvent()
	: mResource(0),
	  mCommandError(CMD_NO_ERROR),
	  mEventData(new KAEventData(this))
{ }

KAEvent::KAEvent(const KDateTime& dt, const QString& message, const QColor& bg, const QColor& fg, const QFont& f, KAEventData::Action action, int lateCancel, int flags, bool changesPending)
	: mResource(0),
	  mCommandError(CMD_NO_ERROR),
	  mEventData(new KAEventData(this, dt, message, bg, fg, f, action, lateCancel, flags, changesPending))
{
	eventUpdated(mEventData);
}

KAEvent::KAEvent(const KCal::Event* e)
	: mResource(0),
	  mCommandError(CMD_NO_ERROR),
	  mEventData(new KAEventData(this, e))
{
	eventUpdated(mEventData);
}

KAEvent::KAEvent(const KAEvent& e)
{
	copy(e, false);
	mEventData = new KAEventData(this, *e.mEventData);
	eventUpdated(mEventData);
}

/******************************************************************************
* Initialise the KAEvent from a KCal::Event.
*/
void KAEvent::set(const Event* event)
{
	mCommandError = CMD_NO_ERROR;
	mEventData->set(event);
}

void KAEvent::copy(const KAEvent& event, bool copyEventData)
{
	mResource        = event.mResource;
	mAllTrigger      = event.mAllTrigger;
	mMainTrigger     = event.mMainTrigger;
	mAllWorkTrigger  = event.mAllWorkTrigger;
	mMainWorkTrigger = event.mMainWorkTrigger;
	mCommandError    = event.mCommandError;
	if (copyEventData)
	{
		// Finally copy the event data, which will trigger an update of the trigger times.
		*mEventData = *event.mEventData;
	}
}

/******************************************************************************
* Initialise the KAEvent with the specified parameters.
*/
void KAEvent::set(const KDateTime& dateTime, const QString& text, const QColor& bg, const QColor& fg,
                  const QFont& font, KAEventData::Action action, int lateCancel, int flags, bool changesPending)
{
	mResource     = 0;
	mCommandError = CMD_NO_ERROR;
	mEventData->set(dateTime, text, bg, fg, font, action, lateCancel, flags, changesPending);
}

QFont KAEvent::font() const
{
	return mEventData->useDefaultFont() ? Preferences::messageFont() : mEventData->font();
}

DateTime KAEvent::nextTrigger(TriggerType type) const
{
	switch (type)
	{
		case ALL_TRIGGER:       return mAllTrigger;
		case MAIN_TRIGGER:      return mMainTrigger;
		case ALL_WORK_TRIGGER:  return mAllWorkTrigger;
		case WORK_TRIGGER:      return mMainWorkTrigger;
		case DISPLAY_TRIGGER:   return (mEventData->workTimeOnly() || mEventData->holidaysExcluded()) ? mMainWorkTrigger : mMainTrigger;
		default:                return DateTime();
	}
}

/******************************************************************************
* Calculate the next trigger times of the alarm.
* This is called when the KAEventData instance notifies that changes have
* occurred which might affect the trigger times.
* mMainTrigger is set to the next scheduled recurrence/sub-repetition, or the
*              deferral time if a deferral is pending.
* mAllTrigger is the same as mMainTrigger, but takes account of reminders.
* mMainWorkTrigger is set to the next scheduled recurrence/sub-repetition
*                  which occurs in working hours, if working-time-only is set.
* mAllWorkTrigger is the same as mMainWorkTrigger, but takes account of reminders.
*/
void KAEvent::eventUpdated(const KAEventData* event)
{
	if (event != mEventData)
		kFatal() << "Wrong event data: have="<<mEventData<<", event="<<event;
kDebug();
	if (!event->templateName().isEmpty())
	{
		// It's a template
		mAllTrigger = mMainTrigger = mAllWorkTrigger = mMainWorkTrigger = KDateTime();
	}
	else if (event->deferred()  &&  !event->reminderDeferral())
	{
		// For a deferred alarm, working time setting is ignored
		mAllTrigger = mMainTrigger = mAllWorkTrigger = mMainWorkTrigger = event->deferDateTime();
	}
	else
	{
		mMainTrigger = event->mainDateTime(true);   // next recurrence or sub-repetition
		// N.B. mReminderMinutes is only set when the reminder is pending -
		// it is cleared once the reminder has triggered.
		mAllTrigger = event->reminderDeferral() ? event->deferDateTime() : mMainTrigger.addMins(-event->reminder());
		// It's not deferred.
		// If only-during-working-time is set and it recurs, it won't actually trigger
		// unless it falls during working hours.
		if ((!event->workTimeOnly() && !event->holidaysExcluded())
		||  ((!event->repeatCount() || !event->repeatInterval()) && event->checkRecur() == KARecurrence::NO_RECUR)
		||  KAlarm::isWorkingTime(mMainTrigger.kDateTime(), this))
		{
			mMainWorkTrigger = mMainTrigger;
			mAllWorkTrigger = mAllTrigger;
		}
		else if (event->workTimeOnly())
		{
			// The alarm is restricted to working hours.
			// Finding the next occurrence during working hours can sometimes take a long time,
			// so mark the next actual trigger as invalid until the calculation completes.
			if (!event->holidaysExcluded())
			{
				// There are no holiday restrictions.
				calcNextWorkingTime(mMainTrigger);
			}
			else
			{
				// Holidays are excluded.
				const HolidayRegion& holidays = Preferences::holidays();
				DateTime nextTrigger = mMainTrigger;
				KDateTime kdt;
				for (int i = 0;  i < 20;  ++i)
				{
					calcNextWorkingTime(nextTrigger);
					if (!holidays.isHoliday(mMainWorkTrigger.date()))
						return;   // found a non-holiday occurrence
					kdt = mMainWorkTrigger.effectiveKDateTime();
					kdt.setTime(QTime(23,59,59));
					KAEventData::OccurType type = nextOccurrence(kdt, nextTrigger, KAEventData::RETURN_REPETITION);
					if (!nextTrigger.isValid())
						break;
					if (KAlarm::isWorkingTime(nextTrigger.kDateTime(), this))
					{
						int reminder = event->reminder() ? event->reminder() : event->reminderArchived();
						mMainWorkTrigger = nextTrigger;
						mAllWorkTrigger = (type & KAEventData::OCCURRENCE_REPEAT) ? mMainWorkTrigger : mMainWorkTrigger.addMins(-reminder);
						return;   // found a non-holiday occurrence
					}
				}
				mMainWorkTrigger = mAllWorkTrigger = DateTime();
			}
		}
		else if (event->holidaysExcluded())
		{
			// Holidays are excluded.
			const HolidayRegion& holidays = Preferences::holidays();
			DateTime nextTrigger = mMainTrigger;
			KDateTime kdt;
			for (int i = 0;  i < 20;  ++i)
			{
				kdt = nextTrigger.effectiveKDateTime();
				kdt.setTime(QTime(23,59,59));
				KAEventData::OccurType type = nextOccurrence(kdt, nextTrigger, KAEventData::RETURN_REPETITION);
				if (!nextTrigger.isValid())
					break;
				if (!holidays.isHoliday(nextTrigger.date()))
				{
					int reminder = event->reminder() ? event->reminder() : event->reminderArchived();
					mMainWorkTrigger = nextTrigger;
					mAllWorkTrigger = (type & KAEventData::OCCURRENCE_REPEAT) ? mMainWorkTrigger : mMainWorkTrigger.addMins(-reminder);
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
void KAEvent::calcNextWorkingTime(const DateTime& nextTrigger) const
{
	kDebug() << "next=" << nextTrigger.kDateTime().dateTime();
	mMainWorkTrigger = mAllWorkTrigger = DateTime();

	QBitArray workDays = Preferences::workDays();
	for (int i = 0;  ;  ++i)
	{
		if (i >= 7)
			return;   // no working days are defined
		if (workDays.testBit(i))
			break;
	}
	QTime workStart = Preferences::workDayStart();
	QTime workEnd   = Preferences::workDayEnd();
	KARecurrence::Type recurType = mEventData->checkRecur();
	KDateTime kdt = nextTrigger.effectiveKDateTime();
	mEventData->checkRepetition();   // ensure data consistency
	int reminder = mEventData->reminder() ? mEventData->reminder() : mEventData->reminderArchived();
	// Check if it always falls on the same day(s) of the week.
	RecurrenceRule* rrule = mEventData->recurrence()->defaultRRuleConst();
	if (!rrule)
		return;   // no recurrence rule!
	unsigned allDaysMask = 0x7F;  // mask bits for all days of week
	bool noWorkPos = false;  // true if no recurrence day position is working day
	QList<RecurrenceRule::WDayPos> pos = rrule->byDays();
	int nDayPos = pos.count();  // number of day positions
	if (nDayPos)
	{
		noWorkPos = true;
		allDaysMask = 0;
		for (int i = 0;  i < nDayPos;  ++i)
		{
			int day = pos[i].day() - 1;  // Monday = 0
			if (workDays.testBit(day))
				noWorkPos = false;   // found a working day occurrence
			allDaysMask |= 1 << day;
		}
		if (noWorkPos  &&  !mEventData->repeatCount())
			return;   // never occurs on a working day
	}
	DateTime newdt;

	if (mEventData->startDateTime().isDateOnly())
	{
		// It's a date-only alarm.
		// Sub-repetitions also have to be date-only.
		int repeatFreq = mEventData->repeatInterval().asDays();
		bool weeklyRepeat = mEventData->repeatCount() && !(repeatFreq % 7);
		Duration interval = mEventData->recurrence()->regularInterval();
		if ((interval  &&  !(interval.asDays() % 7))
		||  nDayPos == 1)
		{
			// It recurs on the same day each week
			if (!mEventData->repeatCount() || weeklyRepeat)
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
			int day = kdt.date().dayOfWeek() - 1;   // Monday = 0
			for (int repeatNum = mEventData->nextRepetition() + 1;  ;  ++repeatNum)
			{
				if (repeatNum > mEventData->repeatCount())
					repeatNum = 0;
				if (repeatNum == mEventData->nextRepetition())
					break;
				if (!repeatNum)
				{
					nextOccurrence(newdt.kDateTime(), newdt, KAEventData::IGNORE_REPETITION);
					if (workDays.testBit(day))
					{
						mMainWorkTrigger = newdt;
						mAllWorkTrigger  = mMainWorkTrigger.addMins(-reminder);
						return;
					}
					kdt = newdt.effectiveKDateTime();
				}
				else
				{
					int inc = repeatFreq * repeatNum;
					if (workDays.testBit((day + inc) % 7))
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
		if (!mEventData->repeatCount()  ||  weeklyRepeat)
		{
			// It's a date-only alarm with either no sub-repetition or a
			// sub-repetition which always falls on the same day of the week
			// as the recurrence (if any).
			unsigned days = 0;
			for ( ; ; )
			{
				kdt.setTime(QTime(23,59,59));
				nextOccurrence(kdt, newdt, KAEventData::IGNORE_REPETITION);
				if (!newdt.isValid())
					return;
				kdt = newdt.effectiveKDateTime();
				int day = kdt.date().dayOfWeek() - 1;
				if (workDays.testBit(day))
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
		for (int repeatNum = mEventData->nextRepetition();  ;  repeatNum = 0)
		{
			while (++repeatNum <= mEventData->repeatCount())
			{
				int inc = repeatFreq * repeatNum;
				if (workDays.testBit((day + inc) % 7))
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
			nextOccurrence(kdt, newdt, KAEventData::IGNORE_REPETITION);
			if (!newdt.isValid())
				return;
			kdt = newdt.effectiveKDateTime();
			day = kdt.date().dayOfWeek() - 1;
			if (workDays.testBit(day))
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
	bool recurTimeVaries = (recurType == KARecurrence::MINUTELY);
	bool repeatTimeVaries = (mEventData->repeatCount()  &&  !mEventData->repeatInterval().isDaily());

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
			KAEventData::OccurType type = nextOccurrence(kdt, newdt, KAEventData::RETURN_REPETITION);
			if (!newdt.isValid())
				return;
			repetition = (type & KAEventData::OCCURRENCE_REPEAT);
			kdt = newdt.effectiveKDateTime();
			int day = kdt.date().dayOfWeek() - 1;
			if (workDays.testBit(day))
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
		KTimeZone ktz = KSystemTimeZones::readZone(tz.name());
		if (ktz.isValid())
			tz = ktz;
	}
	QList<KTimeZone::Transition> tzTransitions = tz.transitions();

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
		if (mEventData->repeatCount())
		{
			// It's a repetition inside a recurrence, each of which occurs
			// at different times of day (bearing in mind that the repetition
			// may occur at daily intervals after each recurrence).
			// Find the previous recurrence (as opposed to sub-repetition)
			repeatFreq = mEventData->repeatInterval().asSeconds();
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
		bool subdaily = (repeatFreq < 24*3600);
//		int period = mRecurrence->frequency() % (24*60);  // it is by definition a MINUTELY recurrence
//		int limit = (24*60 + period - 1) / period;  // number of times until recurrence wraps round
		int transitionIndex = -1;
		for (int n = 0;  n < 7*24*60;  ++n)
		{
			if (mEventData->repeatCount())
			{
				// Check the sub-repetitions for this recurrence
				for ( ; ; )
				{
					// Find the repeat count to the next start of the working day
					int inc = subdaily ? nextWorkRepetition(kdt) : 1;
					repeatNum += inc;
					if (repeatNum > mEventData->repeatCount())
						break;
					kdt = kdt.addSecs(inc * repeatFreq);
					QTime t = kdt.time();
					if (t >= workStart  &&  t < workEnd)
					{
						if (workDays.testBit(kdt.date().dayOfWeek() - 1))
						{
							mMainWorkTrigger = mAllWorkTrigger = kdt;
							return;
						}
					}
				}
				repeatNum = 0;
			}
			nextOccurrence(kdtRecur, newdt, KAEventData::IGNORE_REPETITION);
			if (!newdt.isValid())
				return;
			kdtRecur = newdt.effectiveKDateTime();
			dayRecur = kdtRecur.date().dayOfWeek() - 1;   // Monday = 0
			QTime t = kdtRecur.time();
			if (t >= workStart  &&  t < workEnd)
			{
				if (workDays.testBit(dayRecur))
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
				int i = tz.transitionIndex(kdtRecur.toUtc().dateTime());
				if (i < 0)
					return;
				if (i > transitionIndex)
					transitionIndex = i;
				if (++transitionIndex >= static_cast<int>(tzTransitions.count()))
					return;
				previousOccurrence(KDateTime(tzTransitions[transitionIndex].time(), KDateTime::UTC), newdt, KAEventData::IGNORE_REPETITION);
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
		int repeatFreq = mEventData->repeatInterval().asSeconds();
		previousOccurrence(kdt.addSecs(1), newdt, false);
		if (!newdt.isValid())
			return;   // this should never happen
		KDateTime kdtRecur = newdt.effectiveKDateTime();
		bool recurDuringWork = (kdtRecur.time() >= workStart  &&  kdtRecur.time() < workEnd);

		// Use the previous recurrence as a base for checking whether
		// our tests have wrapped round to the same time/day of week.
		bool subdaily = (repeatFreq < 24*3600);
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
			nextOccurrence(kdtRecur, newdt, KAEventData::IGNORE_REPETITION);
			KDateTime kdtNextRecur = newdt.effectiveKDateTime();

			int repeatsToCheck = mEventData->repeatCount();
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
						bool pastEnd = (repeatNum > mEventData->repeatCount());
						if (pastEnd)
							inc -= repeatNum - mEventData->repeatCount();
						repeatsToCheck -= inc;
						kdt = kdt.addSecs(inc * repeatFreq);
						if (kdtNextRecur.isValid()  &&  kdt >= kdtNextRecur)
						{
							// This sub-repetition is past the next recurrence,
							// so start the check again from the next recurrence.
							repeatsToCheck = mEventData->repeatCount();
							break;
						}
						if (pastEnd)
							break;
						QTime t = kdt.time();
						if (t >= workStart  &&  t < workEnd)
						{
							if (workDays.testBit(kdt.date().dayOfWeek() - 1))
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
				nextOccurrence(kdtRecur, newdt, KAEventData::IGNORE_REPETITION);
				kdtNextRecur = newdt.effectiveKDateTime();
				dateRecur = kdtRecur.date();
				dayRecur = dateRecur.dayOfWeek() - 1;
				if (recurDuringWork  &&  workDays.testBit(dayRecur))
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
			int i = tz.transitionIndex(kdtRecur.toUtc().dateTime());
			if (i < 0)
				return;
			if (i > transitionIndex)
				transitionIndex = i;
			if (++transitionIndex >= static_cast<int>(tzTransitions.count()))
				return;
			kdt = KDateTime(tzTransitions[transitionIndex].time(), KDateTime::UTC);
			previousOccurrence(kdt, newdt, KAEventData::IGNORE_REPETITION);
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
int KAEvent::nextWorkRepetition(const KDateTime& pre) const
{
	KDateTime nextWork(pre);
	QTime workStart = Preferences::workDayStart();
	if (pre.time() < workStart)
		nextWork.setTime(workStart);
	else
	{
		int preDay = pre.date().dayOfWeek() - 1;   // Monday = 0
		for (int n = 1;  ;  ++n)
		{
			if (n >= 7)
				return mEventData->repeatCount() + 1;  // should never happen
			if (Preferences::workDays().testBit((preDay + n) % 7))
			{
				nextWork = nextWork.addDays(n);
				nextWork.setTime(workStart);
				break;
			}
		}
	}
	return (pre.secsTo(nextWork) - 1) / mEventData->repeatInterval().asSeconds() + 1;
}

/******************************************************************************
* Check whether an alarm which recurs at the same time of day can possibly
* occur during working hours.
* This does not determine whether it actually does, but rather whether it could
* potentially given enough repetitions.
* Reply = false if it can never occur during working hours, true if it might.
*/
bool KAEvent::mayOccurDailyDuringWork(const KDateTime& kdt) const
{
	if (!kdt.isDateOnly()
	&&  (kdt.time() < Preferences::workDayStart() || kdt.time() >= Preferences::workDayEnd()))
		return false;   // its time is outside working hours
	// Check if it always occurs on the same day of the week
	Duration interval = mEventData->recurrence()->regularInterval();
	if (interval  &&  interval.isDaily()  &&  !(interval.asDays() % 7))
	{
		// It recurs weekly
		if (!mEventData->repeatCount()  ||  (mEventData->repeatInterval().isDaily() && !(mEventData->repeatInterval().asDays() % 7)))
			return false;   // any repetitions are also weekly
		// Repetitions are daily. Check if any occur on working days
		// by checking the first recurrence and up to 6 repetitions.
		QBitArray workDays = Preferences::workDays();
		int day = mEventData->recurrence()->startDateTime().date().dayOfWeek() - 1;   // Monday = 0
		int repeatDays = mEventData->repeatInterval().asDays();
		int maxRepeat = (mEventData->repeatCount() < 6) ? mEventData->repeatCount() : 6;
		for (int i = 0;  !workDays.testBit(day);  ++i, day = (day + repeatDays) % 7)
		{
			if (i >= maxRepeat)
				return false;  // no working day occurrences
		}
	}
	return true;
}

/******************************************************************************
* Initialise the command last error status of the alarm from the config file.
*/
void KAEvent::setCommandError(const QString& configString)
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

/******************************************************************************
* Set the command last error status.
* The status is written to the config file.
*/
void KAEvent::setCommandError(CmdErrType error) const
{
	kDebug() << mEventData->id() << "," << error;
	if (error == mCommandError)
		return;
	mCommandError = error;
	KConfigGroup config(KGlobal::config(), mCmdErrConfigGroup);
	if (mCommandError == CMD_NO_ERROR)
		config.deleteEntry(mEventData->id());
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
		config.writeEntry(mEventData->id(), errtext);
	}
	config.sync();
}

bool KAEvent::setDisplaying(const KAEvent& e, KAAlarm::Type t, const QString& resourceID, const KDateTime& dt, bool showEdit, bool showDefer)
{
	bool result = mEventData->setDisplaying(*e.mEventData, t, resourceID, dt, showEdit, showDefer);
	if (result)
		copy(e, false);
	return result;
}


#ifndef NDEBUG
void KAEvent::dumpDebug() const
{
	kDebug() << "KAEvent dump:";
	mEventData->dumpDebug();
	if (mResource)
		kDebug() << "-- mResource:" << mResource->resourceName();
	kDebug() << "-- mCommandError:" << mCommandError;
	kDebug() << "-- mAllTrigger:" << mAllTrigger.toString();
	kDebug() << "-- mMainTrigger:" << mMainTrigger.toString();
	kDebug() << "-- mAllWorkTrigger:" << mAllWorkTrigger.toString();
	kDebug() << "-- mMainWorkTrigger:" << mMainWorkTrigger.toString();
	kDebug() << "KAEvent dump end";
}
#endif
