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
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <qcolor.h>
#include <qregexp.h>
#include <kdebug.h>

#include "kalarm.h"
#include "kalarmapp.h"
#include "prefsettings.h"
#include "alarmcalendar.h"
#include "msgevent.h"
using namespace KCal;


const QCString APPNAME("KALARM");

// Custom calendar properties
static const QCString TYPE_PROPERTY("TYPE");    // X-KDE-KALARM-TYPE property
static const QString FILE_TYPE        = QString::fromLatin1("FILE");
static const QString AT_LOGIN_TYPE    = QString::fromLatin1("LOGIN");
static const QString DEFERRAL_TYPE    = QString::fromLatin1("DEFERRAL");
static const QString DISPLAYING_TYPE  = QString::fromLatin1("DISPLAYING");   // used only in displaying calendar

// Event categories
static const QString BEEP_CATEGORY        = QString::fromLatin1("BEEP");
static const QString EMAIL_BCC_CATEGORY   = QString::fromLatin1("BCC");
static const QString CONFIRM_ACK_CATEGORY = QString::fromLatin1("ACKCONF");
static const QString LATE_CANCEL_CATEGORY = QString::fromLatin1("LATECANCEL");
static const QString ARCHIVE_CATEGORY     = QString::fromLatin1("SAVE");

static const QString EXPIRED_UID          = QString::fromLatin1("-exp-");
static const QString DISPLAYING_UID       = QString::fromLatin1("-disp-");

struct AlarmData
{
	QString                cleanText;       // text or audio file name
	EmailAddressList       emailAddresses;
	QString                emailSubject;
	QStringList            emailAttachments;
	QDateTime              dateTime;
	KAlarmAlarm::Type      type;
	KAAlarmEventBase::Type action;
	int                    displayingFlags;
};
typedef QMap<KAlarmAlarm::Type, AlarmData> AlarmMap;


/*=============================================================================
= Class KAlarmEvent
= Corresponds to a KCal::Event instance.
=============================================================================*/

void KAlarmEvent::copy(const KAlarmEvent& event)
{
	KAAlarmEventBase::copy(event);
	mAudioFile            = event.mAudioFile;
	mEndDateTime          = event.mEndDateTime;
	mAtLoginDateTime      = event.mAtLoginDateTime;
	mDeferralTime         = event.mDeferralTime;
	mDisplayingTime       = event.mDisplayingTime;
	mDisplayingFlags      = event.mDisplayingFlags;
	mRevision             = event.mRevision;
	mRemainingRecurrences = event.mRemainingRecurrences;
	mAlarmCount           = event.mAlarmCount;
	mAnyTime              = event.mAnyTime;
	mExpired              = event.mExpired;
	mArchive              = event.mArchive;
	mUpdated              = event.mUpdated;
	delete mRecurrence;
	if (event.mRecurrence)
		mRecurrence = new Recurrence(*event.mRecurrence, 0);
	else
		mRecurrence = 0;
}

/******************************************************************************
 * Initialise the KAlarmEvent from a KCal::Event.
 */
void KAlarmEvent::set(const Event& event)
{
	// Extract status from the event
	mEventID  = event.uid();
	mRevision = event.revision();
	const QStringList& cats = event.categories();
	mBeep       = false;
	mEmailBcc   = false;
	mConfirmAck = false;
	mLateCancel = false;
	mArchive    = false;
	mColour = QColor(255, 255, 255);    // missing/invalid colour - return white
	if (cats.count() > 0)
	{
		QColor colour(cats[0]);
		if (colour.isValid())
			 mColour = colour;

		for (unsigned int i = 1;  i < cats.count();  ++i)
			if (cats[i] == BEEP_CATEGORY)
				mBeep = true;
			else if (cats[i] == CONFIRM_ACK_CATEGORY)
				mConfirmAck = true;
			else if (cats[i] == EMAIL_BCC_CATEGORY)
				mEmailBcc = true;
			else if (cats[i] == LATE_CANCEL_CATEGORY)
				mLateCancel = true;
			else if (cats[i] == ARCHIVE_CATEGORY)
				mArchive = true;
	}

	// Extract status from the event's alarms.
	// First set up defaults.
	mActionType     = T_MESSAGE;
	mRepeatAtLogin  = false;
	mDeferral       = false;
	mDisplaying     = false;
	mExpired        = true;
	mText           = "";
	mAudioFile      = "";
	mEmailSubject   = "";
	mEmailAddresses.clear();
	mEmailAttachments.clear();
	mDateTime       = event.dtStart();
	mEndDateTime    = event.dtEnd();
	mAnyTime        = event.doesFloat();
	initRecur(false);

	// Extract data from all the event's alarms and index the alarms by sequence number
	AlarmMap alarmMap;
	QPtrList<Alarm> alarms = event.alarms();
	for (QPtrListIterator<Alarm> ia(alarms);  ia.current();  ++ia)
	{
		// Parse the next alarm's text
		AlarmData data;
		readAlarm(*ia.current(), data);
		alarmMap.insert(data.type, data);
	}

	// Incorporate the alarms' details into the overall event
	AlarmMap::ConstIterator it = alarmMap.begin();
	mAlarmCount = 0;       // initialise as invalid
	bool set = false;
	for (  ;  it != alarmMap.end();  ++it)
	{
		const AlarmData& data = it.data();
		if (data.action == T_AUDIO)
			mAudioFile = data.cleanText;
		else
		{
			switch (data.type)
			{
				case KAlarmAlarm::MAIN_ALARM:
					mExpired = false;
					break;
				case KAlarmAlarm::AT_LOGIN_ALARM:
					mRepeatAtLogin   = true;
					mAtLoginDateTime = data.dateTime;
					break;
				case KAlarmAlarm::DEFERRAL_ALARM:
					mDeferral     = true;
					mDeferralTime = data.dateTime;
					break;
				case KAlarmAlarm::DISPLAYING_ALARM:
					mDisplaying      = true;
					mDisplayingTime  = data.dateTime;
					mDisplayingFlags = data.displayingFlags;
					break;
				default:
					break;
			}
		}

		// Ensure that the basic fields are set up even if there is no main
		// alarm in the event (which shouldn't ever happen!)
		if (!set)
		{
			if (data.action != T_AUDIO)
			{
				mActionType = data.action;
				mText = (mActionType == T_COMMAND) ? data.cleanText.stripWhiteSpace() : data.cleanText;
				if (data.action == T_EMAIL)
				{
					mEmailAddresses   = data.emailAddresses;
					mEmailSubject     = data.emailSubject;
					mEmailAttachments = data.emailAttachments;
				}
			}
			mDateTime = data.dateTime;
			if (mAnyTime)
				mDateTime.setTime(QTime());
			set = true;
		}
		if (data.action == T_FILE  &&  mActionType == T_MESSAGE)
			mActionType = T_FILE;
		++mAlarmCount;
	}

	Recurrence* recur = event.recurrence();
	if (recur  &&  recur->doesRecur() != Recurrence::rNone)
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
				mRemainingRecurrences = recur->duration();
				if (mRemainingRecurrences > 0)
					mRemainingRecurrences -= recur->durationTo(savedDT) - 1;
				break;
			default:
				mDateTime = savedDT;
				break;
		}
	}

	mUpdated = false;
}

/******************************************************************************
 * Parse a KCal::Alarm.
 * Reply = alarm ID (sequence number)
 */
void KAlarmEvent::readAlarm(const Alarm& alarm, AlarmData& data)
{
	// Parse the next alarm's text
	data.dateTime = alarm.time();
	data.displayingFlags = 0;
	if (!alarm.audioFile().isEmpty())
	{
		data.action    = T_AUDIO;
		data.cleanText = alarm.audioFile();
		data.type      = KAlarmAlarm::AUDIO_ALARM;
	}
	else
	{
		// It's a text message/file/command/email
		if (!alarm.programFile().isEmpty())
		{
			data.action    = T_COMMAND;
			data.cleanText = alarm.programFile();
		}
		else if (alarm.mailAddresses().count() > 0)
		{
			data.action = T_EMAIL;
			data.emailAddresses   = alarm.mailAddresses();
			data.emailSubject     = alarm.mailSubject();
			data.emailAttachments = alarm.mailAttachments();
			data.cleanText        = alarm.text();
		}
		else
		{
			data.action    = T_MESSAGE;
			data.cleanText = alarm.text();
		}

		bool atLogin = false;
		bool deferral = false;
		data.type = KAlarmAlarm::MAIN_ALARM;
		QString typelist = alarm.customProperty(APPNAME, TYPE_PROPERTY);
		QStringList types = QStringList::split(QChar(','), typelist);
		for (unsigned int i = 0;  i < types.count();  ++i)
		{
			// iCalendar puts a \ character before commas, so remove it if there is one
			QString type = types[i];
			int last = type.length() - 1;
			if (type[last] == QChar('\\'))
				type.truncate(last);

			if (type == AT_LOGIN_TYPE)
				atLogin = true;
			else if (type == FILE_TYPE  &&  data.action == T_MESSAGE)
				data.action = T_FILE;
			else if (type == DEFERRAL_TYPE)
				deferral = true;
			else if (type == DISPLAYING_TYPE)
				data.type = KAlarmAlarm::DISPLAYING_ALARM;
		}

		if (deferral)
		{
			if (data.type == KAlarmAlarm::MAIN_ALARM)
				data.type = KAlarmAlarm::DEFERRAL_ALARM;
			else if (data.type == KAlarmAlarm::DISPLAYING_ALARM)
				data.displayingFlags = DEFERRAL;
		}
		if (atLogin)
		{
			if (data.type == KAlarmAlarm::MAIN_ALARM)
				data.type = KAlarmAlarm::AT_LOGIN_ALARM;
			else if (data.type == KAlarmAlarm::DISPLAYING_ALARM)
				data.displayingFlags = REPEAT_AT_LOGIN;
		}
	}
//kdDebug()<<"ReadAlarm(): text="<<alarm.text()<<", time="<<alarm.time().toString()<<", valid time="<<alarm.time().isValid()<<endl;
}

/******************************************************************************
 * Initialise the KAlarmEvent with the specified parameters.
 */
void KAlarmEvent::set(const QDateTime& dateTime, const QString& text, const QColor& colour, Action action, int flags)
{
	initRecur(false);
	mDateTime = mEndDateTime = dateTime;
	switch (action)
	{
		case MESSAGE:
		case FILE:
		case COMMAND:
		case EMAIL:
			mActionType = (KAAlarmEventBase::Type)action;
			break;
		default:
			mActionType = T_MESSAGE;
			break;
	}
	mText       = (mActionType == T_COMMAND) ? text.stripWhiteSpace() : text;
	mAudioFile  = "";
	mColour     = colour;
	mAlarmCount = 1;
	set(flags);
	mDeferral   = false;
	mDisplaying = false;
	mExpired    = false;
	mArchive    = false;
	mUpdated    = false;
}

/******************************************************************************
 * Initialise an email KAlarmEvent.
 */
void KAlarmEvent::setEmail(const QDate& d, const EmailAddressList& addresses, const QString& subject,
			    const QString& message, const QStringList& attachments, int flags)
{
	set(d, message, QColor(), EMAIL, flags | ANY_TIME);
	mEmailAddresses   = addresses;
	mEmailSubject     = subject;
	mEmailAttachments = attachments;
}

void KAlarmEvent::setEmail(const QDateTime& dt, const EmailAddressList& addresses, const QString& subject,
			    const QString& message, const QStringList& attachments, int flags)
{
	set(dt, message, QColor(), EMAIL, flags);
	mEmailAddresses   = addresses;
	mEmailSubject     = subject;
	mEmailAttachments = attachments;
}

void KAlarmEvent::setEmail(const EmailAddressList& addresses, const QString& subject, const QStringList& attachments)
{
	mEmailAddresses   = addresses;
	mEmailSubject     = subject;
	mEmailAttachments = attachments;
}

/******************************************************************************
 * Convert a unique ID to indicate that the event is in a specified calendar file.
 */
QString KAlarmEvent::uid(const QString& id, Status status)
{
	QString result = id;
	Status oldStatus;
	int i, len;
	if ((i = result.find(EXPIRED_UID)) > 0)
	{
		oldStatus = EXPIRED;
		len = EXPIRED_UID.length();
	}
	else if ((i = result.find(DISPLAYING_UID)) > 0)
	{
		oldStatus = DISPLAYING;
		len = DISPLAYING_UID.length();
	}
	else
	{
		oldStatus = ACTIVE;
		i = result.findRev('-');
		len = 1;
	}
	if (status != oldStatus  &&  i > 0)
	{
		QString part;
		switch (status)
		{
			case ACTIVE:      part = "-";  break;
			case EXPIRED:     part = EXPIRED_UID;  break;
			case DISPLAYING:  part = DISPLAYING_UID;  break;
		}
		result.replace(i, len, part);
	}
	return result;
}

/******************************************************************************
 * Get the calendar type for a unique ID.
 */
KAlarmEvent::Status KAlarmEvent::uidStatus(const QString& uid)
{
	if (uid.find(EXPIRED_UID) > 0)
		return EXPIRED;
	if (uid.find(DISPLAYING_UID) > 0)
		return DISPLAYING;
	return ACTIVE;
}

void KAlarmEvent::set(int flags)
{
	KAAlarmEventBase::set(flags & ~(DEFERRAL | DISPLAYING_));    // DEFERRAL, DISPLAYING_ are read-only
	mAnyTime = flags & ANY_TIME;
}

int KAlarmEvent::flags() const
{
	return KAAlarmEventBase::flags() | (mAnyTime ? ANY_TIME : 0);
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
bool KAlarmEvent::updateEvent(Event& ev, bool checkUid) const
{
	if (checkUid  &&  !mEventID.isEmpty()  &&  mEventID != ev.uid()
	||  !mAlarmCount)
		return false;
	checkRecur();     // ensure recurrence/repetition data is consistent
	bool readOnly = ev.isReadOnly();
	ev.setReadOnly(false);

	// Set up event-specific data
	QStringList cats;
	cats.append(mColour.name());
	if (mBeep)
		cats.append(BEEP_CATEGORY);
	if (mConfirmAck)
		cats.append(CONFIRM_ACK_CATEGORY);
	if (mEmailBcc)
		cats.append(EMAIL_BCC_CATEGORY);
	if (mLateCancel)
		cats.append(LATE_CANCEL_CATEGORY);
	if (mArchive)
		cats.append(ARCHIVE_CATEGORY);
	ev.setCategories(cats);
	ev.setRevision(mRevision);
	ev.clearAlarms();

	QDateTime dtStart = mDateTime;
	QDateTime dtMain  = mDateTime;
	if (!mExpired)
	{
		// Add the main alarm
		if (mAnyTime)
			dtMain.setTime(theApp()->settings()->startOfDay());
		initKcalAlarm(ev, dtMain, QStringList());
	}

	// Add subsidiary alarms
	if (mRepeatAtLogin  &&  !mExpired)
	{
		QDateTime dtl = mAtLoginDateTime.isValid() ? mAtLoginDateTime
		                : QDateTime::currentDateTime();
		initKcalAlarm(ev, dtl, AT_LOGIN_TYPE);
		if (dtl < dtStart)
			dtStart = dtl;
	}
	if (mDeferral)
	{
		initKcalAlarm(ev, mDeferralTime, QStringList(DEFERRAL_TYPE));
		if (mDeferralTime < dtStart)
			dtStart = mDeferralTime;
	}
	if (mDisplaying)
	{
		QStringList list(DISPLAYING_TYPE);
		if (mDisplayingFlags & REPEAT_AT_LOGIN)
			list += AT_LOGIN_TYPE;
		else if (mDisplayingFlags & DEFERRAL)
			list += DEFERRAL_TYPE;
		initKcalAlarm(ev, mDisplayingTime, list);
		if (mDisplayingTime.isValid()  &&  mDisplayingTime < dtStart)
			dtStart = mDisplayingTime;
	}
	if (!mAudioFile.isEmpty())
	{
		Alarm* al = ev.newAlarm();
		al->setEnabled(true);        // enable the alarm
		al->setAudioFile(mAudioFile);
		al->setTime(dtMain);         // set it for the main alarm time
	}

	// Add recurrence data
	if (mRecurrence)
	{
		Recurrence* recur = ev.recurrence();
		int frequency = mRecurrence->frequency();
		int duration  = mRecurrence->duration();
		const QDateTime& endDateTime = mRecurrence->endDateTime();
		dtStart = mRecurrence->recurStart();
		recur->setRecurStart(dtStart);
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

	ev.setDtStart(dtStart);
	ev.setDtEnd(mEndDateTime);
	ev.setFloats(mAnyTime);
	ev.setReadOnly(readOnly);
	return true;
}

/******************************************************************************
 * Create a new alarm for a libkcal event, and initialise it according to the
 * alarm action. If 'type' is non-null, it is appended to the X-KDE-KALARM-TYPE
 * property value list.
 */
Alarm* KAlarmEvent::initKcalAlarm(Event& event, const QDateTime& dt, const QStringList& types) const
{
	QStringList alltypes;
	Alarm* alarm = event.newAlarm();
	alarm->setEnabled(true);
	alarm->setTime(dt);
	switch (mActionType)
	{
		case T_FILE:
			alltypes += FILE_TYPE;
			// fall through to T_MESSAGE
		case T_MESSAGE:
			alarm->setText(mText);
			break;
		case T_COMMAND:
			alarm->setProgramFile(mText);
			break;
		case T_EMAIL:
			alarm->setText(mText);
			if (mEmailAddresses.count() > 0)
				alarm->setMailAddresses(mEmailAddresses);
			alarm->setMailSubject(mEmailSubject);
			if (mEmailAttachments.count() > 0)
				alarm->setMailAttachments(mEmailAttachments);
			break;
		case T_AUDIO:     // never occurs in this context
			break;
	}
	alltypes += types;
	if (alltypes.count() > 0)
		alarm->setCustomProperty(APPNAME, TYPE_PROPERTY, alltypes.join(","));
	return alarm;
}

/******************************************************************************
 * Return the alarm of the specified type.
 */
KAlarmAlarm KAlarmEvent::alarm(KAlarmAlarm::Type type) const
{
	checkRecur();     // ensure recurrence/repetition data is consistent
	KAlarmAlarm al;
	if (mAlarmCount)
	{
		al.mEventID = mEventID;
		if (type == KAlarmAlarm::AUDIO_ALARM)
		{
			al.mType       = type;
			al.mActionType = T_AUDIO;
			al.mDateTime   = mDateTime;
			al.mText       = mAudioFile;
		}
		else
		{
			al.mType          = KAlarmAlarm::INVALID_ALARM;
			al.mActionType    = mActionType;
			al.mText          = mText;
			al.mColour        = mColour;
			al.mBeep          = mBeep;
			al.mConfirmAck    = mConfirmAck;
			al.mRepeatAtLogin = false;
			al.mDeferral      = false;
			al.mLateCancel    = mLateCancel;
			al.mEmailBcc      = mEmailBcc;
			if (mActionType == T_EMAIL)
			{
				al.mEmailAddresses   = mEmailAddresses;
				al.mEmailSubject     = mEmailSubject;
				al.mEmailAttachments = mEmailAttachments;
			}
			switch (type)
			{
				case KAlarmAlarm::MAIN_ALARM:
					if (!mExpired)
					{
						al.mType     = KAlarmAlarm::MAIN_ALARM;
						al.mDateTime = mDateTime;
					}
					break;
				case KAlarmAlarm::AT_LOGIN_ALARM:
					if (mRepeatAtLogin)
					{
						al.mType          = KAlarmAlarm::AT_LOGIN_ALARM;
						al.mDateTime      = mAtLoginDateTime;
						al.mRepeatAtLogin = true;
						al.mLateCancel    = false;
					}
					break;
				case KAlarmAlarm::DEFERRAL_ALARM:
					if (mDeferral)
					{
						al.mType     = KAlarmAlarm::DEFERRAL_ALARM;
						al.mDateTime = mDeferralTime;
						al.mDeferral = true;
					}
					break;
				case KAlarmAlarm::DISPLAYING_ALARM:
					if (mDisplaying)
					{
						al.mType       = KAlarmAlarm::DISPLAYING_ALARM;
						al.mDateTime   = mDisplayingTime;
						al.mDisplaying = true;
					}
					break;
				default:
					break;
			}
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
KAlarmAlarm KAlarmEvent::firstAlarm() const
{
	if (mAlarmCount)
	{
		if (!mExpired)
			return alarm(KAlarmAlarm::MAIN_ALARM);
		return nextAlarm(KAlarmAlarm::MAIN_ALARM);
	}
	return KAlarmAlarm();
}

/******************************************************************************
 * Return the next alarm for the event, after the specified alarm.
 * N.B. a repeat-at-login alarm can only be returned if it has been read from/
 * written to the calendar file.
 */
KAlarmAlarm KAlarmEvent::nextAlarm(KAlarmAlarm::Type prevType) const
{
	switch (prevType)
	{
		case KAlarmAlarm::MAIN_ALARM:
			if (mDeferral)
				return alarm(KAlarmAlarm::DEFERRAL_ALARM);
			// fall through to DEFERRAL_ALARM
		case KAlarmAlarm::DEFERRAL_ALARM:
			if (mRepeatAtLogin)
				return alarm(KAlarmAlarm::AT_LOGIN_ALARM);
			// fall through to AT_LOGIN_ALARM
		case KAlarmAlarm::AT_LOGIN_ALARM:
			if (mDisplaying)
				return alarm(KAlarmAlarm::DISPLAYING_ALARM);
			// fall through to DISPLAYING_ALARM
		case KAlarmAlarm::DISPLAYING_ALARM:
			if (!mAudioFile.isEmpty())
				return alarm(KAlarmAlarm::AUDIO_ALARM);
			// fall through to default
		default:
			break;
	}
	return KAlarmAlarm();
}

/******************************************************************************
 * Remove the alarm of the specified type from the event.
 */
void KAlarmEvent::removeAlarm(KAlarmAlarm::Type type)
{
	switch (type)
	{
		case KAlarmAlarm::MAIN_ALARM:
			mAlarmCount = 0;    // removing main alarm - also remove subsidiary alarms
			break;
		case KAlarmAlarm::AT_LOGIN_ALARM:
			if (mRepeatAtLogin)
			{
				mRepeatAtLogin = false;
				--mAlarmCount;
			}
			break;
		case KAlarmAlarm::DEFERRAL_ALARM:
			if (mDeferral)
			{
				mDeferral = false;
				--mAlarmCount;
			}
			break;
		case KAlarmAlarm::DISPLAYING_ALARM:
			if (mDisplaying)
			{
				mDisplaying = false;
				--mAlarmCount;
			}
			break;
		case KAlarmAlarm::AUDIO_ALARM:
			mAudioFile = "";
			--mAlarmCount;
			break;
		default:
			break;
	}
}

/******************************************************************************
 * Defer the event to the specified time.
 * Optionally ensure that the next scheduled recurrence is after the current time.
 */
void KAlarmEvent::defer(const QDateTime& dateTime, bool adjustRecurrence)
{
	if (checkRecur() == NO_RECUR)
		mDateTime = dateTime;
	else
	{
		addDefer(dateTime);
		if (adjustRecurrence)
		{
			QDateTime now = QDateTime::currentDateTime();
			if (mDateTime < now)
				setNextOccurrence(now);
		}
	}
}

/******************************************************************************
 * Add a deferral alarm with the specified trigger time.
 */
void KAlarmEvent::addDefer(const QDateTime& dateTime)
{
	mDeferralTime = dateTime;
	if (!mDeferral)
	{
		mDeferral = true;
		++mAlarmCount;
	}
}

/******************************************************************************
 * Cancel any deferral alarm.
 */
void KAlarmEvent::cancelDefer()
{
	if (mDeferral)
	{
		mDeferralTime = QDateTime();
		mDeferral     = false;
		--mAlarmCount;
	}
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
bool KAlarmEvent::setDisplaying(const KAlarmEvent& event, KAlarmAlarm::Type alarmType, const QDateTime& repeatAtLoginTime)
{
	if (!mDisplaying
	&&  (alarmType == KAlarmAlarm::MAIN_ALARM
	  || alarmType == KAlarmAlarm::DEFERRAL_ALARM
	  || alarmType == KAlarmAlarm::AT_LOGIN_ALARM))
	{
kdDebug()<<"KAlarmEvent::setDisplaying("<<event.id()<<", "<<(alarmType==KAlarmAlarm::MAIN_ALARM?"MAIN":alarmType==KAlarmAlarm::DEFERRAL_ALARM?"DEFERRAL":"LOGIN")<<"): time="<<repeatAtLoginTime.toString()<<endl;
		KAlarmAlarm al = event.alarm(alarmType);
		if (al.valid())
		{
			*this = event;
			setUid(DISPLAYING);
			mDisplaying      = true;
			mDisplayingTime  = (alarmType == KAlarmAlarm::AT_LOGIN_ALARM) ? repeatAtLoginTime : al.dateTime();
			mDisplayingFlags = (alarmType == KAlarmAlarm::AT_LOGIN_ALARM) ? REPEAT_AT_LOGIN
			                 : (alarmType == KAlarmAlarm::DEFERRAL_ALARM) ? DEFERRAL : 0;
			++mAlarmCount;
			return true;
		}
	}
	return false;
}

/******************************************************************************
 * Return the original alarm which the displaying alarm refers to.
 */
KAlarmAlarm KAlarmEvent::convertDisplayingAlarm() const
{
	KAlarmAlarm al;
	if (mDisplaying)
	{
		al = alarm(KAlarmAlarm::DISPLAYING_ALARM);
		if (mDisplayingFlags & REPEAT_AT_LOGIN)
		{
			al.mRepeatAtLogin = true;
			al.mType = KAlarmAlarm::AT_LOGIN_ALARM;
		}
		else if (mDisplayingFlags & DEFERRAL)
		{
			al.mDeferral = true;
			al.mType = KAlarmAlarm::DEFERRAL_ALARM;
		}
		else
			al.mType = KAlarmAlarm::MAIN_ALARM;
	}
	return al;
}

/******************************************************************************
 * Reinstate the original event from the 'displaying' event.
 */
void KAlarmEvent::reinstateFromDisplaying(const KAlarmEvent& dispEvent)
{
	if (dispEvent.mDisplaying)
	{
		*this = dispEvent;
		setUid(ACTIVE);
		mDisplaying = false;
		--mAlarmCount;
		mUpdated = true;
	}
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
	if (checkRecur() == NO_RECUR)
	{
		result = QDateTime();
		return NO_OCCURRENCE;
	}
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
				mRemainingRecurrences = remainingCount;
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
		mRemainingRecurrences = count;
		return true;
	}
	else
	{
		delete mRecurrence;
		mRecurrence = 0;
		mRemainingRecurrences = 0;
		return false;
	}
}

/******************************************************************************
 * Validate the event's recurrence and alarm repetition data, correcting any
 * inconsistencies (which should never occur!).
 * Reply = true if a recurrence (as opposed to a login repetition) exists.
 */
KAlarmEvent::RecurType KAlarmEvent::checkRecur() const
{
	if (mRecurrence)
	{
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
				if (mRecurrence)
				{
					delete mRecurrence;     // this shouldn't exist!!
					const_cast<KAlarmEvent*>(this)->mRecurrence = 0;
				}
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
				readAlarm(alarm, data);
				if (data.type == KAlarmAlarm::MAIN_ALARM)
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

/******************************************************************************
 * If the calendar was written by a previous version of KAlarm, do any
 * necessary format conversions on the events to ensure that when the calendar
 * is saved, no information is lost or corrupted.
 */
void KAlarmEvent::convertKCalEvents(AlarmCalendar& calendar)
{
	// KAlarm pre-0.9 codes held in the DESCRIPTION property
	static const QChar   SEPARATOR            = ';';
	static const QChar   LATE_CANCEL_CODE     = 'C';
	static const QChar   AT_LOGIN_CODE        = 'L';   // subsidiary alarm at every login
	static const QChar   DEFERRAL_CODE        = 'D';   // extra deferred alarm
	static const QString TEXT_PREFIX          = QString::fromLatin1("TEXT:");
	static const QString FILE_PREFIX          = QString::fromLatin1("FILE:");
	static const QString COMMAND_PREFIX       = QString::fromLatin1("CMD:");

	int version = calendar.KAlarmVersion();
	if (version >= AlarmCalendar::KAlarmVersion(0,9,0))
		return;

	kdDebug(5950) << "KAlarmEvent::convertKCalEvents(): adjusting\n";
	bool pre_0_7 = (version < AlarmCalendar::KAlarmVersion(0,7,0));
	bool adjustSummerTime = calendar.KAlarmVersion057_UTC();
	QDateTime dt0(QDate(1970,1,1), QTime(0,0,0));

	QPtrList<Event> events = calendar.events();
	for (Event* event = events.first();  event;  event = events.next())
	{
		if (pre_0_7  &&  event->doesFloat())
		{
			// It's a KAlarm pre-0.7 calendar file.
			// Ensure that when the calendar is saved, the alarm time isn't lost.
			event->setFloats(false);
		}

		QPtrList<Alarm> alarms = event->alarms();
		for (QPtrListIterator<Alarm> ia(alarms);  ia.current();  ++ia)
		{
			Alarm* alarm = ia.current();
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
			bool atLogin    = false;
			bool deferral   = false;
			bool lateCancel = false;
			KAAlarmEventBase::Type action = T_MESSAGE;
			QString txt = alarm->text();
			int length = txt.length();
			int i = 0;
			if (txt[0].isDigit())
			{
				while (++i < length  &&  txt[i].isDigit()) ;
				if (i < length  &&  txt[i++] == SEPARATOR)
				{
					while (i < length)
					{
						QChar ch = txt[i++];
						if (ch == SEPARATOR)
							break;
						if (ch == LATE_CANCEL_CODE)
							lateCancel = true;
						else if (ch == AT_LOGIN_CODE)
							atLogin = true;
						else if (ch == DEFERRAL_CODE)
							deferral = true;
					}
				}
				else
					i = 0;     // invalid prefix
			}
			if (txt.find(TEXT_PREFIX, i) == i)
				i += TEXT_PREFIX.length();
			else if (txt.find(FILE_PREFIX, i) == i)
			{
				action = T_FILE;
				i += FILE_PREFIX.length();
			}
			else if (txt.find(COMMAND_PREFIX, i) == i)
			{
				action = T_COMMAND;
				i += COMMAND_PREFIX.length();
			}
			else
				i = 0;
			txt = txt.mid(i);

			QStringList types;
			switch (action)
			{
				case T_FILE:
					types += FILE_TYPE;
					// fall through to T_MESSAGE
				case T_MESSAGE:
					alarm->setText(txt);
					break;
				case T_COMMAND:
					alarm->setProgramFile(txt);
					break;
				case T_EMAIL:     // email alarms were introduced in KAlarm 0.9
				case T_AUDIO:     // never occurs in this context
					break;
			}
			if (atLogin)
			{
				types += AT_LOGIN_TYPE;
				lateCancel = false;
			}
			else if (deferral)
				types += DEFERRAL_TYPE;
			if (lateCancel)
			{
				QStringList cats = event->categories();
				cats.append(LATE_CANCEL_CATEGORY);
				event->setCategories(cats);
			}
			if (types.count() > 0)
				alarm->setCustomProperty(APPNAME, TYPE_PROPERTY, types.join(","));

			if (pre_0_7  &&  alarm->repeatCount() > 0  &&  alarm->snoozeTime() > 0)
			{
				// It's a KAlarm pre-0.7 calendar file.
				// Minutely recurrences were stored differently.
				Recurrence* recur = event->recurrence();
				if (recur  &&  recur->doesRecur() == Recurrence::rNone)
				{
					recur->setMinutely(alarm->snoozeTime(), alarm->repeatCount() + 1);
					alarm->setRepeatCount(0);
					alarm->setSnoozeTime(0);
				}
			}

			if (adjustSummerTime)
			{
				// The calendar file was written by the KDE 3.0.0 version of KAlarm 0.5.7.
				// Summer time was ignored when converting to UTC.
				QDateTime dt = alarm->time();
				time_t t = dt0.secsTo(dt);
				struct tm* dtm = localtime(&t);
				if (dtm->tm_isdst)
				{
					dt = dt.addSecs(-3600);
					alarm->setTime(dt);
				}
			}
		}
	}
}

#ifndef NDEBUG
void KAlarmEvent::dumpDebug() const
{
	kdDebug(5950) << "KAlarmEvent dump:\n";
	KAAlarmEventBase::dumpDebug();
	kdDebug(5950) << "-- mAudioFile:" << mAudioFile << ":\n";
	kdDebug(5950) << "-- mEndDateTime:" << mEndDateTime.toString() << ":\n";
	if (mRepeatAtLogin)
		kdDebug(5950) << "-- mAtLoginDateTime:" << mAtLoginDateTime.toString() << ":\n";
	if (mDeferral)
		kdDebug(5950) << "-- mDeferralTime:" << mDeferralTime.toString() << ":\n";
	if (mDisplaying)
	{
		kdDebug(5950) << "-- mDisplayingTime:" << mDisplayingTime.toString() << ":\n";
		kdDebug(5950) << "-- mDisplayingFlags:" << mDisplayingFlags << ":\n";
	}
	kdDebug(5950) << "-- mRevision:" << mRevision << ":\n";
	kdDebug(5950) << "-- mRecurrence:" << (mRecurrence ? "true" : "false") << ":\n";
	if (mRecurrence)
		kdDebug(5950) << "-- mRemainingRecurrences:" << mRemainingRecurrences << ":\n";
	kdDebug(5950) << "-- mAlarmCount:" << mAlarmCount << ":\n";
	kdDebug(5950) << "-- mAnyTime:" << (mAnyTime ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mExpired:" << (mExpired ? "true" : "false") << ":\n";
	kdDebug(5950) << "KAlarmEvent dump end\n";
}
#endif


/*=============================================================================
= Class KAlarmAlarm
= Corresponds to a single KCal::Alarm instance.
=============================================================================*/

KAlarmAlarm::KAlarmAlarm(const KAlarmAlarm& alarm)
	: KAAlarmEventBase(alarm),
	  mType(alarm.mType),
	  mRecurs(alarm.mRecurs)
{ }

#ifndef NDEBUG
void KAlarmAlarm::dumpDebug() const
{
	kdDebug(5950) << "KAlarmAlarm dump:\n";
	KAAlarmEventBase::dumpDebug();
	kdDebug(5950) << "-- mType:" << (mType == MAIN_ALARM ? "MAIN" : mType == DEFERRAL_ALARM ? "DEFERAL" : mType == AT_LOGIN_ALARM ? "LOGIN" : mType == DISPLAYING_ALARM ? "DISPLAYING" : mType == AUDIO_ALARM ? "AUDIO" : "INVALID") << ":\n";
	kdDebug(5950) << "-- mRecurs:" << (mRecurs ? "true" : "false") << ":\n";
	kdDebug(5950) << "KAlarmAlarm dump end\n";
}
#endif


/*=============================================================================
= Class KAAlarmEventBase
=============================================================================*/

void KAAlarmEventBase::copy(const KAAlarmEventBase& rhs)
{
	mEventID          = rhs.mEventID;
	mText             = rhs.mText;
	mDateTime         = rhs.mDateTime;
	mColour           = rhs.mColour;
	mEmailAddresses   = rhs.mEmailAddresses;
	mEmailSubject     = rhs.mEmailSubject;
	mEmailAttachments = rhs.mEmailAttachments;
	mActionType       = rhs.mActionType;
	mBeep             = rhs.mBeep;
	mRepeatAtLogin    = rhs.mRepeatAtLogin;
	mDeferral         = rhs.mDeferral;
	mDisplaying       = rhs.mDisplaying;
	mLateCancel       = rhs.mLateCancel;
	mEmailBcc         = rhs.mEmailBcc;
	mConfirmAck       = rhs.mConfirmAck;
}

void KAAlarmEventBase::set(int flags)
{
	mBeep          = flags & KAlarmEvent::BEEP;
	mRepeatAtLogin = flags & KAlarmEvent::REPEAT_AT_LOGIN;
	mLateCancel    = flags & KAlarmEvent::LATE_CANCEL;
	mEmailBcc      = flags & KAlarmEvent::EMAIL_BCC;
	mConfirmAck    = flags & KAlarmEvent::CONFIRM_ACK;
	mDeferral      = flags & KAlarmEvent::DEFERRAL;
	mDisplaying    = flags & KAlarmEvent::DISPLAYING_;
}

int KAAlarmEventBase::flags() const
{
	return (mBeep          ? KAlarmEvent::BEEP : 0)
	     | (mRepeatAtLogin ? KAlarmEvent::REPEAT_AT_LOGIN : 0)
	     | (mLateCancel    ? KAlarmEvent::LATE_CANCEL : 0)
	     | (mEmailBcc      ? KAlarmEvent::EMAIL_BCC : 0)
	     | (mConfirmAck    ? KAlarmEvent::CONFIRM_ACK : 0)
	     | (mDeferral      ? KAlarmEvent::DEFERRAL : 0)
	     | (mDisplaying    ? KAlarmEvent::DISPLAYING_ : 0);

}

#ifndef NDEBUG
void KAAlarmEventBase::dumpDebug() const
{
	kdDebug(5950) << "-- mEventID:" << mEventID << ":\n";
	kdDebug(5950) << "-- mActionType:" << (mActionType == T_MESSAGE ? "MESSAGE" : mActionType == T_FILE ? "FILE" : mActionType == T_COMMAND ? "COMMAND" : mActionType == T_EMAIL ? "EMAIL" : mActionType == T_AUDIO ? "AUDIO" : "??") << ":\n";
	kdDebug(5950) << "-- mText:" << mText << ":\n";
	kdDebug(5950) << "-- mDateTime:" << mDateTime.toString() << ":\n";
	if (mActionType == T_EMAIL)
	{
		kdDebug(5950) << "-- mEmail: Addresses:" << mEmailAddresses.join(", ") << ":\n";
		kdDebug(5950) << "--         Subject:" << mEmailSubject << ":\n";
		kdDebug(5950) << "--         Attachments:" << mEmailAttachments.join(", ") << ":\n";
		kdDebug(5950) << "--         Bcc:" << (mEmailBcc ? "true" : "false") << ":\n";
	}
	kdDebug(5950) << "-- mColour:" << mColour.name() << ":\n";
	kdDebug(5950) << "-- mBeep:" << (mBeep ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mConfirmAck:" << (mConfirmAck ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mRepeatAtLogin:" << (mRepeatAtLogin ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mDeferral:" << (mDeferral ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mDisplaying:" << (mDisplaying ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mLateCancel:" << (mLateCancel ? "true" : "false") << ":\n";
}
#endif


/*=============================================================================
= Class EmailAddressList
=============================================================================*/

/******************************************************************************
 * Sets the list of email addresses, removing any empty addresses.
 * Reply = false if empty addresses were found.
 */
EmailAddressList& EmailAddressList::operator=(const QValueList<Person>& addresses)
{
	clear();
	for (QValueList<Person>::ConstIterator it = addresses.begin();  it != addresses.end();  ++it)
	{
		if (!(*it).email().isEmpty())
			append(*it);
	}
	return *this;
}

/******************************************************************************
 * Return the email address list as a string, each address being delimited by
 * the specified separator string.
 */
QString EmailAddressList::join(const QString& separator) const
{
	QString result;
	bool first = true;
	for (QValueList<Person>::ConstIterator it = begin();  it != end();  ++it)
	{
		if (first)
			first = false;
		else
			result += separator;

		bool quote = false;
		QString name = (*it).name();
		if (!name.isEmpty())
		{
			// Need to enclose the name in quotes if it has any special characters
			int len = name.length();
			for (int i = 0;  i < len;  ++i)
			{
				QChar ch = name[i];
				if (!ch.isLetterOrNumber() && !ch.isSpace())
				{
					quote = true;
					result += '\"';
					break;
				}
			}
			result += (*it).name();
			result += (quote ? "\" <" : " <");
			quote = true;    // need angle brackets round email address
		}

		result += (*it).email();
		if (quote)
			result += ">";
	}
	return result;
}
