/*
 *  msgevent.cpp  -  the event object for messages
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
static const QString REMINDER_TYPE    = QString::fromLatin1("REMINDER");
static const QString DEFERRAL_TYPE    = QString::fromLatin1("DEFERRAL");
static const QString DISPLAYING_TYPE  = QString::fromLatin1("DISPLAYING");   // used only in displaying calendar
static const QCString FONT_COLOUR_PROPERTY("FONTCOLOR");    // X-KDE-KALARM-FONTCOLOR property

// Event categories
static const QString EMAIL_BCC_CATEGORY            = QString::fromLatin1("BCC");
static const QString CONFIRM_ACK_CATEGORY          = QString::fromLatin1("ACKCONF");
static const QString LATE_CANCEL_CATEGORY          = QString::fromLatin1("LATECANCEL");
static const QString ARCHIVE_CATEGORY              = QString::fromLatin1("SAVE");
static const QRegExp ARCHIVE_REMINDER_CATEGORY     = QRegExp(QString::fromLatin1("^SAVE \\d+[MHD]$"));
static const QString ARCHIVE_CATEGORY_TEMPLATE     = QString::fromLatin1("SAVE %1%2");

static const QString EXPIRED_UID    = QString::fromLatin1("-exp-");
static const QString DISPLAYING_UID = QString::fromLatin1("-disp-");

struct AlarmData
{
	const Alarm*           alarm;
	QString                cleanText;       // text or audio file name
	EmailAddressList       emailAddresses;
	QString                emailSubject;
	QStringList            emailAttachments;
	QDateTime              dateTime;
	QFont                  font;
	QColor                 bgColour, fgColour;
	KAlarmAlarm::Type      type;
	KAAlarmEventBase::Type action;
	int                    displayingFlags;
	bool                   defaultFont;
};
typedef QMap<KAlarmAlarm::Type, AlarmData> AlarmMap;

static void setProcedureAlarm(Alarm*, const QString& commandLine);


/*=============================================================================
= Class KAlarmEvent
= Corresponds to a KCal::Event instance.
=============================================================================*/

void KAlarmEvent::copy(const KAlarmEvent& event)
{
	KAAlarmEventBase::copy(event);
	mAudioFile               = event.mAudioFile;
	mStartDateTime           = event.mStartDateTime;
	mEndDateTime             = event.mEndDateTime;
	mAtLoginDateTime         = event.mAtLoginDateTime;
	mDeferralTime            = event.mDeferralTime;
	mDisplayingTime          = event.mDisplayingTime;
	mDisplayingFlags         = event.mDisplayingFlags;
	mReminderMinutes         = event.mReminderMinutes;
	mReminderDeferralMinutes = event.mReminderDeferralMinutes;
	mReminderArchiveMinutes  = event.mReminderArchiveMinutes;
	mRevision                = event.mRevision;
	mRemainingRecurrences    = event.mRemainingRecurrences;
	mAlarmCount              = event.mAlarmCount;
	mRecursFeb29             = event.mRecursFeb29;
	mAnyTime                 = event.mAnyTime;
	mMainExpired             = event.mMainExpired;
	mArchive                 = event.mArchive;
	mUpdated                 = event.mUpdated;
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
	mBeep                   = false;
	mEmailBcc               = false;
	mConfirmAck             = false;
	mLateCancel             = false;
	mArchive                = false;
	mReminderArchiveMinutes = 0;
	mBgColour               = QColor(255, 255, 255);    // missing/invalid colour - return white
	mDefaultFont            = true;
	for (unsigned int i = 0;  i < cats.count();  ++i)
	{
		if (cats[i] == CONFIRM_ACK_CATEGORY)
			mConfirmAck = true;
		else if (cats[i] == EMAIL_BCC_CATEGORY)
			mEmailBcc = true;
		else if (cats[i] == LATE_CANCEL_CATEGORY)
			mLateCancel = true;
		else if (cats[i] == ARCHIVE_CATEGORY)
			mArchive = true;
		else if (cats[i].find(ARCHIVE_REMINDER_CATEGORY) == 0)
		{
			// It's the archive flag plus a reminder time
			mArchive = true;
			char ch;
			mReminderArchiveMinutes = 0;
			const char* cat = cats[i].latin1();
			while ((ch = *cat) != 0  &&  (ch < '0' || ch > '9'))
				++cat;
			if (ch)
			{
				mReminderArchiveMinutes = ch - '0';
				while ((ch = *++cat) >= '0'  &&  ch <= '9')
					mReminderArchiveMinutes = mReminderArchiveMinutes * 10 + ch - '0';
				switch (ch)
				{
					case 'M':  break;
					case 'H':  mReminderArchiveMinutes *= 60;    break;
					case 'D':  mReminderArchiveMinutes *= 1440;  break;
				}
			}
		}
	}

	// Extract status from the event's alarms.
	// First set up defaults.
	mActionType              = T_MESSAGE;
	mRecursFeb29             = false;
	mRepeatAtLogin           = false;
	mDeferral                = false;
	mDisplaying              = false;
	mMainExpired             = true;
	mReminderMinutes         = 0;
	mReminderDeferralMinutes = 0;
	mText                    = "";
	mAudioFile               = "";
	mEmailSubject            = "";
	mEmailAddresses.clear();
	mEmailAttachments.clear();
	mStartDateTime           = event.dtStart();
	mDateTime                = mStartDateTime;
	mEndDateTime             = event.dtEnd();
	mAnyTime                 = event.doesFloat();
	initRecur(false);

	// Extract data from all the event's alarms and index the alarms by sequence number
	AlarmMap alarmMap;
	readAlarms(event, &alarmMap);

	// Incorporate the alarms' details into the overall event
	AlarmMap::ConstIterator it = alarmMap.begin();
	mAlarmCount = 0;       // initialise as invalid
	QDateTime reminderTime, reminderDeferralTime;
	bool set = false;
	for (  ;  it != alarmMap.end();  ++it)
	{
		const AlarmData& data = it.data();
		switch (data.type)
		{
			case KAlarmAlarm::MAIN_ALARM:
				mMainExpired = false;
				break;
			case KAlarmAlarm::AT_LOGIN_ALARM:
				mRepeatAtLogin   = true;
				mAtLoginDateTime = data.dateTime;
				break;
			case KAlarmAlarm::REMINDER_ALARM:
				reminderTime = data.dateTime;
				break;
			case KAlarmAlarm::REMINDER_DEFERRAL_ALARM:
				mDeferral            = true;
				reminderDeferralTime = data.dateTime;
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
			case KAlarmAlarm::AUDIO_ALARM:
				mAudioFile = data.cleanText;
				mBeep      = mAudioFile.isEmpty();
				break;
			default:
				break;
		}

		// Ensure that the basic fields are set up even if there is no main
		// alarm in the event (if it has expired and then been deferred)
		if (!set)
		{
			if (data.action != T_AUDIO)
			{
				mActionType = data.action;
				mText = (mActionType == T_COMMAND) ? data.cleanText.stripWhiteSpace() : data.cleanText;
				if (data.action == T_MESSAGE)
				{
					mFont        = data.font;
					mDefaultFont = data.defaultFont;
					mBgColour    = data.bgColour;
				}
				else if (data.action == T_EMAIL)
				{
					mEmailAddresses   = data.emailAddresses;
					mEmailSubject     = data.emailSubject;
					mEmailAttachments = data.emailAttachments;
				}
			}
			mDateTime = data.dateTime;
			if (mAnyTime
			&&  ( data.type == KAlarmAlarm::MAIN_ALARM
			   || data.type == KAlarmAlarm::DISPLAYING_ALARM  && !(data.displayingFlags & DEFERRAL)))
#warning "Check whether time is shown in deferred any-time alarm message window"
				mDateTime.setTime(QTime());
			set = true;
		}
		if (data.action == T_FILE  &&  mActionType == T_MESSAGE)
			mActionType = T_FILE;
		++mAlarmCount;
	}
	if (reminderTime.isValid())
	{
		mReminderMinutes = reminderTime.secsTo(mDateTime) / 60;
		if (mReminderMinutes)
			mReminderArchiveMinutes = 0;
	}
	if (reminderDeferralTime.isValid())
		mReminderDeferralMinutes = reminderDeferralTime.secsTo(mDateTime) / 60;

	Recurrence* recur = event.recurrence();
	if (recur  &&  recur->doesRecur() != Recurrence::rNone)
	{
		// Copy the recurrence details.
		switch (recur->doesRecur())
		{
			case Recurrence::rYearlyMonth:
			{
				QDate start = recur->recurStart().date();
				mRecursFeb29 = (start.day() == 29  &&  start.month() == 2);
				// fall through to rMinutely
			}
			case Recurrence::rMinutely:
			case Recurrence::rHourly:
			case Recurrence::rDaily:
			case Recurrence::rWeekly:
			case Recurrence::rMonthlyDay:
			case Recurrence::rMonthlyPos:
			case Recurrence::rYearlyPos:
			case Recurrence::rYearlyDay:
				delete mRecurrence;
				mRecurrence = new Recurrence(*recur, 0);
				mRemainingRecurrences = recur->duration();
				if (mRemainingRecurrences > 0)
					mRemainingRecurrences -= recur->durationTo(mDateTime) - 1;
				break;
			default:
				break;
		}
	}

	mUpdated = false;
}

/******************************************************************************
 * Parse the alarms for a KCal::Event.
 * Reply = map of alarm data, indexed by KAlarmAlarm::Type
 */
void KAlarmEvent::readAlarms(const Event& event, void* almap)
{
	AlarmMap* alarmMap = (AlarmMap*)almap;
	QPtrList<Alarm> alarms = event.alarms();
	for (QPtrListIterator<Alarm> ia(alarms);  ia.current();  ++ia)
	{
		// Parse the next alarm's text
		AlarmData data;
		readAlarm(*ia.current(), data);
		if (data.type != KAlarmAlarm::INVALID_ALARM)
			alarmMap->insert(data.type, data);
	}
}

/******************************************************************************
 * Parse a KCal::Alarm.
 * Reply = alarm ID (sequence number)
 */
void KAlarmEvent::readAlarm(const Alarm& alarm, AlarmData& data)
{
	// Parse the next alarm's text
	data.alarm    = &alarm;
	data.dateTime = alarm.time();
	data.displayingFlags = 0;
	switch (alarm.type())
	{
		case Alarm::Procedure:
			data.action    = T_COMMAND;
			data.cleanText = alarm.programFile();
			if (!alarm.programArguments().isEmpty())
				data.cleanText += " " + alarm.programArguments();
			break;
		case Alarm::Email:
			data.action           = T_EMAIL;
			data.emailAddresses   = alarm.mailAddresses();
			data.emailSubject     = alarm.mailSubject();
			data.emailAttachments = alarm.mailAttachments();
			data.cleanText        = alarm.mailText();
			break;
		case Alarm::Display:
		{
			data.action    = T_MESSAGE;
			data.cleanText = alarm.text();
			QString property = alarm.customProperty(APPNAME, FONT_COLOUR_PROPERTY);
			QStringList list = QStringList::split(QChar(';'), property, true);
			data.bgColour = (list.count() > 0) ? list[0] : QColor(255, 255, 255);;
			data.fgColour = (list.count() > 1) ? list[1] : QColor(0, 0, 0);
			data.defaultFont = (list.count() <= 2 || list[2].isEmpty());
			if (!data.defaultFont)
				data.font.fromString(list[2]);
			break;
		}
		case Alarm::Audio:
			data.action    = T_AUDIO;
			data.cleanText = alarm.audioFile();
			data.type      = KAlarmAlarm::AUDIO_ALARM;
			return;
		case Alarm::Invalid:
			data.type = KAlarmAlarm::INVALID_ALARM;
			return;
	}

	bool atLogin = false;
	bool reminder = false;
	bool deferral = false;
	data.type = KAlarmAlarm::MAIN_ALARM;
	QString property = alarm.customProperty(APPNAME, TYPE_PROPERTY);
	QStringList types = QStringList::split(QChar(','), property);
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
		else if (type == REMINDER_TYPE)
			reminder = true;
		else if (type == DEFERRAL_TYPE)
			deferral = true;
		else if (type == DISPLAYING_TYPE)
			data.type = KAlarmAlarm::DISPLAYING_ALARM;
	}

	if (reminder)
	{
		if (data.type == KAlarmAlarm::MAIN_ALARM)
			data.type = deferral ? KAlarmAlarm::REMINDER_DEFERRAL_ALARM : KAlarmAlarm::REMINDER_ALARM;
		else if (data.type == KAlarmAlarm::DISPLAYING_ALARM)
			data.displayingFlags = deferral ? REMINDER | DEFERRAL : REMINDER;
	}
	else if (deferral)
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
//kdDebug()<<"ReadAlarm(): text="<<alarm.text()<<", time="<<alarm.time().toString()<<", valid time="<<alarm.time().isValid()<<endl;
}

/******************************************************************************
 * Initialise the KAlarmEvent with the specified parameters.
 */
void KAlarmEvent::set(const QDateTime& dateTime, const QString& text, const QColor& colour, const QFont& font, Action action, int flags)
{
	initRecur(false);
	mStartDateTime = mEndDateTime = mDateTime = dateTime;
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
	mText                    = (mActionType == T_COMMAND) ? text.stripWhiteSpace() : text;
	mAudioFile               = "";
	mBgColour                = colour;
	mFont                    = font;
	mAlarmCount              = 1;
	set(flags);
	mReminderMinutes         = 0;
	mReminderDeferralMinutes = 0;
	mReminderArchiveMinutes  = 0;
	mDeferral                = false;
	mDisplaying              = false;
	mMainExpired             = false;
	mArchive                 = false;
	mUpdated                 = false;
}

/******************************************************************************
 * Initialise an email KAlarmEvent.
 */
void KAlarmEvent::setEmail(const QDate& d, const EmailAddressList& addresses, const QString& subject,
			    const QString& message, const QStringList& attachments, int flags)
{
	set(d, message, QColor(), QFont(), EMAIL, flags | ANY_TIME);
	mEmailAddresses   = addresses;
	mEmailSubject     = subject;
	mEmailAttachments = attachments;
}

void KAlarmEvent::setEmail(const QDateTime& dt, const EmailAddressList& addresses, const QString& subject,
			    const QString& message, const QStringList& attachments, int flags)
{
	set(dt, message, QColor(), QFont(), EMAIL, flags);
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
 * Returns the time of the next scheduled occurrence of the event.
 */
QDateTime KAlarmEvent::nextDateTime() const
{
	int reminder = mReminderMinutes ? mReminderMinutes : mReminderDeferralMinutes;
	return reminder ? mDateTime.addSecs(-reminder * 60)
	     : mDeferral ? QMIN(mDeferralTime, mDateTime) : mDateTime;
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
	KAAlarmEventBase::set(flags & ~READ_ONLY_FLAGS);
	mAnyTime = flags & ANY_TIME;
	mUpdated = true;
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
	updateKCalEvent(*ev);
	return ev;
}

/******************************************************************************
 * Update an existing KCal::Event with the KAlarmEvent data.
 * If 'original' is true, the event start date/time is adjusted to its original
 * value instead of its next occurrence, and the expired main alarm is
 * reinstated.
 */
bool KAlarmEvent::updateKCalEvent(Event& ev, bool checkUid, bool original) const
{
	if (checkUid  &&  !mEventID.isEmpty()  &&  mEventID != ev.uid()
	||  !mAlarmCount  &&  (!original || !mMainExpired))
		return false;
	checkRecur();     // ensure recurrence/repetition data is consistent
	bool readOnly = ev.isReadOnly();
	ev.setReadOnly(false);

	// Set up event-specific data
	QStringList cats;
	if (mConfirmAck)
		cats.append(CONFIRM_ACK_CATEGORY);
	if (mEmailBcc)
		cats.append(EMAIL_BCC_CATEGORY);
	if (mLateCancel)
		cats.append(LATE_CANCEL_CATEGORY);
	if (mArchive  &&  !original)
	{
		if (mReminderArchiveMinutes)
		{
			char unit = 'M';
			int count = mReminderArchiveMinutes;
			if (count % 1440 == 0)
			{
				unit = 'D';
				count /= 1440;
			}
			else if (count % 60 == 0)
			{
				unit = 'H';
				count /= 60;
			}
			cats.append(ARCHIVE_CATEGORY_TEMPLATE.arg(count).arg(unit));
		}
		else
			cats.append(ARCHIVE_CATEGORY);
	}
	ev.setCategories(cats);
	ev.setRevision(mRevision);
	ev.clearAlarms();

	QDateTime dtMain = original ? mStartDateTime : mDateTime;
	if (!mMainExpired  ||  original)
	{
		// Add the main alarm
		if (mAnyTime)
			dtMain.setTime(theApp()->settings()->startOfDay());
		initKcalAlarm(ev, dtMain, QStringList());
	}

	// Add subsidiary alarms
	if (mRepeatAtLogin)
	{
		QDateTime dtl = mAtLoginDateTime.isValid() ? mAtLoginDateTime
		              : mAnyTime ? QDateTime(QDate::currentDate().addDays(-1), mStartDateTime.time())
		              : QDateTime::currentDateTime();
		initKcalAlarm(ev, dtl, AT_LOGIN_TYPE);
	}
	if (mReminderMinutes  ||  mReminderArchiveMinutes && original)
	{
		int minutes = mReminderMinutes ? mReminderMinutes : mReminderArchiveMinutes;
		QDateTime reminderTime = dtMain.addSecs(-minutes * 60);
		initKcalAlarm(ev, reminderTime, QStringList(REMINDER_TYPE));
	}
	if (mReminderDeferralMinutes)
	{
		QStringList list(REMINDER_TYPE);
		list += DEFERRAL_TYPE;
		QDateTime reminderTime = dtMain.addSecs(-mReminderDeferralMinutes * 60);
		initKcalAlarm(ev, reminderTime, list);
	}
	else if (mDeferral)
	{
		initKcalAlarm(ev, mDeferralTime, QStringList(DEFERRAL_TYPE));
	}
	if (mDisplaying)
	{
		QStringList list(DISPLAYING_TYPE);
		if (mDisplayingFlags & REPEAT_AT_LOGIN)
			list += AT_LOGIN_TYPE;
		else if (mDisplayingFlags & DEFERRAL)
			list += DEFERRAL_TYPE;
		initKcalAlarm(ev, mDisplayingTime, list);
	}
	if (mBeep  ||  !mAudioFile.isEmpty())
	{
		Alarm* al = ev.newAlarm();
		al->setEnabled(true);           // enable the alarm
		al->setAudioAlarm(mAudioFile);  // empty for a beep
		al->setTime(dtMain);            // set it for the main alarm time
	}

	// Add recurrence data
	if (mRecurrence)
	{
		Recurrence* recur = ev.recurrence();
		int frequency = mRecurrence->frequency();
		int duration  = mRecurrence->duration();
		const QDateTime& endDateTime = mRecurrence->endDateTime();
		recur->setRecurStart(mStartDateTime);
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

	ev.setDtStart(mStartDateTime);
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
	if (dt.isValid()  &&  dt < mStartDateTime)
		alarm->setOffset(mStartDateTime.secsTo(dt));
	else
		alarm->setTime(dt);
	switch (mActionType)
	{
		case T_FILE:
			alltypes += FILE_TYPE;
			// fall through to T_MESSAGE
		case T_MESSAGE:
			alarm->setDisplayAlarm(mText);
			alarm->setCustomProperty(APPNAME, FONT_COLOUR_PROPERTY,
			              QString::fromLatin1("%1;%2;%3").arg(mBgColour.name())
			                                             .arg(QString::null)
			                                             .arg(mDefaultFont ? QString::null : mFont.toString()));
			break;
		case T_COMMAND:
			setProcedureAlarm(alarm, mText);
			break;
		case T_EMAIL:
			alarm->setEmailAlarm(mEmailSubject, mText, mEmailAddresses, mEmailAttachments);
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
			al.mBgColour      = mBgColour;
			al.mFont          = mFont;
			al.mDefaultFont   = mDefaultFont;
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
					if (!mMainExpired)
					{
						al.mType     = KAlarmAlarm::MAIN_ALARM;
						al.mDateTime = mDateTime;
					}
					break;
				case KAlarmAlarm::REMINDER_ALARM:
					if (mReminderMinutes)
					{
						al.mType     = KAlarmAlarm::REMINDER_ALARM;
						al.mDateTime = mDateTime.addSecs(-mReminderMinutes * 60);
					}
					break;
				case KAlarmAlarm::DEFERRAL_ALARM:
					if (!mReminderDeferralMinutes)
					{
						if (mDeferral)
						{
							al.mType     = KAlarmAlarm::DEFERRAL_ALARM;
							al.mDateTime = mDeferralTime;
							al.mDeferral = true;
						}
						break;
					}
					// fall through to REMINDER_DEFERRAL_ALARM
				case KAlarmAlarm::REMINDER_DEFERRAL_ALARM:
					if (mReminderDeferralMinutes)
					{
						al.mType     = KAlarmAlarm::REMINDER_DEFERRAL_ALARM;
						al.mDateTime = mDateTime.addSecs(-mReminderDeferralMinutes * 60);
						al.mDeferral = true;
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
		if (!mMainExpired)
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
			if (mReminderMinutes)
				return alarm(KAlarmAlarm::REMINDER_ALARM);
			// fall through to REMINDER_ALARM
		case KAlarmAlarm::REMINDER_ALARM:
			// There can only be one deferral alarm
			if (mReminderDeferralMinutes)
				return alarm(KAlarmAlarm::REMINDER_DEFERRAL_ALARM);
			if (mDeferral)
				return alarm(KAlarmAlarm::DEFERRAL_ALARM);
			// fall through to DEFERRAL_ALARM
		case KAlarmAlarm::REMINDER_DEFERRAL_ALARM:
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
 * This should only be called to remove an alarm which has expired, not to
 * reconfigure the event.
 */
void KAlarmEvent::removeExpiredAlarm(KAlarmAlarm::Type type)
{
	int count = mAlarmCount;
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
		case KAlarmAlarm::REMINDER_ALARM:
			if (mReminderMinutes)
			{
				// Remove the reminder alarm, but keep a note of it for archiving purposes
				mReminderArchiveMinutes = mReminderMinutes;
				mReminderMinutes = 0;
				--mAlarmCount;
			}
			break;
		case KAlarmAlarm::REMINDER_DEFERRAL_ALARM:
		case KAlarmAlarm::DEFERRAL_ALARM:
			if (mDeferral)
			{
				mReminderDeferralMinutes = 0;
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
	if (mAlarmCount != count)
		mUpdated = true;
}

/******************************************************************************
 * Defer the event to the specified time.
 * If the main alarm time has passed, the main alarm is marked as expired.
 * Optionally ensure that the next scheduled recurrence is after the current time.
 */
void KAlarmEvent::defer(const QDateTime& dateTime, bool reminder, bool adjustRecurrence)
{
	if (checkRecur() == NO_RECUR)
	{
		if (mReminderMinutes)
		{
			// Remove the reminder alarm, but keep a note of it for archiving purposes
			mReminderArchiveMinutes = mReminderMinutes;
		}
		if (mReminderMinutes  ||  mReminderDeferralMinutes  ||  mReminderArchiveMinutes)
		{
			if (dateTime < mDateTime)
			{
				if (!mReminderMinutes  &&  !mReminderDeferralMinutes)
					++mAlarmCount;
				mReminderDeferralMinutes = dateTime.secsTo(mDateTime) / 60;   // defer reminder alarm
				mDeferral = true;
			}
			else
			{
				// Deferring past the main alarm time, so it no longer counts as a deferral
				if (mReminderMinutes  ||  mReminderDeferralMinutes)
				{
					mReminderDeferralMinutes = 0;
					mDeferral = false;
					--mAlarmCount;
				}
			}
			mReminderMinutes = 0;
		}
		if (!mReminderDeferralMinutes)
		{
			// Main alarm has now expired
			mDeferralTime = mDateTime = dateTime;
			if (!mDeferral)
			{
				mDeferral = true;
				++mAlarmCount;
			}
			mMainExpired = true;
			--mAlarmCount;
		}
	}
	else if (reminder)
	{
		// Deferring a reminder for a recurring alarm
		mReminderDeferralMinutes = dateTime.secsTo(mDateTime) / 60;
		if (mReminderDeferralMinutes <= 0)
			mReminderDeferralMinutes = 0;    // (error)
		else if (!mDeferral)
		{
			mDeferral = true;
			++mAlarmCount;
		}
	}
	else
	{
		mDeferralTime = dateTime;
		if (!mDeferral)
		{
			mDeferral = true;
			++mAlarmCount;
		}
		if (adjustRecurrence)
		{
			QDateTime now = QDateTime::currentDateTime();
			if (mDateTime < now)
			{
				if (setNextOccurrence(now) == NO_OCCURRENCE)
				{
					mMainExpired = true;
					--mAlarmCount;
				}
			}
		}
	}
	mUpdated = true;
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
		mUpdated = true;
	}
}

/******************************************************************************
 * Find the time of the deferred alarm.
 */
QDateTime KAlarmEvent::deferDateTime() const
{
	return mReminderDeferralMinutes ? mDateTime.addSecs(-mReminderDeferralMinutes * 60) : mDeferralTime;
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
	  || alarmType == KAlarmAlarm::REMINDER_ALARM
	  || alarmType == KAlarmAlarm::REMINDER_DEFERRAL_ALARM
	  || alarmType == KAlarmAlarm::DEFERRAL_ALARM
	  || alarmType == KAlarmAlarm::AT_LOGIN_ALARM))
	{
kdDebug()<<"KAlarmEvent::setDisplaying("<<event.id()<<", "<<(alarmType==KAlarmAlarm::MAIN_ALARM?"MAIN":alarmType==KAlarmAlarm::REMINDER_ALARM?"REMINDER":alarmType==KAlarmAlarm::REMINDER_DEFERRAL_ALARM?"REMINDER_DEFERRAL":alarmType==KAlarmAlarm::DEFERRAL_ALARM?"DEFERRAL":"LOGIN")<<"): time="<<repeatAtLoginTime.toString()<<endl;
		KAlarmAlarm al = event.alarm(alarmType);
		if (al.valid())
		{
			*this = event;
			setUid(DISPLAYING);
			mDisplaying      = true;
			mDisplayingTime  = (alarmType == KAlarmAlarm::AT_LOGIN_ALARM) ? repeatAtLoginTime : al.dateTime();
			mDisplayingFlags = (alarmType == KAlarmAlarm::AT_LOGIN_ALARM) ? REPEAT_AT_LOGIN
			                 : (alarmType == KAlarmAlarm::REMINDER_ALARM) ? REMINDER
			                 : (alarmType == KAlarmAlarm::REMINDER_DEFERRAL_ALARM) ? REMINDER | DEFERRAL
			                 : (alarmType == KAlarmAlarm::DEFERRAL_ALARM) ? DEFERRAL : 0;
			++mAlarmCount;
			mUpdated = true;
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
			al.mType = (mDisplayingFlags & REMINDER) ? KAlarmAlarm::REMINDER_DEFERRAL_ALARM : KAlarmAlarm::DEFERRAL_ALARM;
		}
		else if (mDisplayingFlags & REMINDER)
			al.mType = KAlarmAlarm::REMINDER_ALARM;
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
 * specified date/time. Any reminder alarm is adjusted accordingly.
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
		if (type != FIRST_OCCURRENCE  &&  type != NO_OCCURRENCE  &&  newTime != mDateTime)
		{
			mDateTime = newTime;
			if (mRecurrence->duration() > 0)
				mRemainingRecurrences = remainingCount;
			if (mReminderDeferralMinutes  ||  mReminderArchiveMinutes)
			{
				if (!mReminderMinutes)
					++mAlarmCount;
				mReminderMinutes = mReminderArchiveMinutes;
			}
			if (mReminderDeferralMinutes)
			{
				mReminderDeferralMinutes = 0;
				mDeferral = false;
				--mAlarmCount;
			}
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
 * Adjust the event date/time to the first recurrence of the event, on or after
 * start date/time. The event start date may not be a recurrence date, in which
 * case a later date will be set.
 */
void KAlarmEvent::setFirstRecurrence()
{
	if (checkRecur() != NO_RECUR)
	{
		int remainingCount;
		QDateTime next;
		QDateTime recurStart = mRecurrence->recurStart();
		QDateTime pre = mDateTime.addDays(-1);
		mRecurrence->setRecurStart(pre);
		nextRecurrence(pre, next, remainingCount);
		if (!next.isValid())
			mRecurrence->setRecurStart(recurStart);   // reinstate the old value
		else
		{
			mRecurrence->setRecurStart(next);
			mStartDateTime = mDateTime = next;
			mUpdated = true;
		}
	}
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
 * Set the event to recur annually, on the specified start date in each of the
 * specified months.
 * Parameters:
 *    freq   = how many years between recurrences.
 *    months = which months of the year alarms should occur on.
 *    feb29  = if start date is March 1st, recur on February 29th; otherwise ignored
 *    count  = number of occurrences, including first and last.
 *           = 0 to use 'end' instead.
 *    end    = end date (invalid to use 'count' instead).
 */
void KAlarmEvent::setRecurAnnualByDate(int freq, const QValueList<int>& months, bool feb29, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count, feb29))
	{
		if (count)
			mRecurrence->setYearly(Recurrence::rYearlyMonth, freq, count);
		else
			mRecurrence->setYearly(Recurrence::rYearlyMonth, freq, end);
		for (QValueListConstIterator<int> it = months.begin();  it != months.end();  ++it)
			mRecurrence->addYearlyNum(*it);
	}
}

void KAlarmEvent::setRecurAnnualByDate(int freq, const QPtrList<int>& months, bool feb29, int count, const QDate& end)
{
	if (initRecur(end.isValid(), count, feb29))
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
			mRecurrence->setYearly(Recurrence::rYearlyPos, freq, count);
		else
			mRecurrence->setYearly(Recurrence::rYearlyPos, freq, end);
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
bool KAlarmEvent::initRecur(bool endDate, int count, bool feb29)
{
	mRecursFeb29 = false;
	if (endDate || count)
	{
		if (!mRecurrence)
			mRecurrence = new Recurrence(0);
		mRecurrence->setRecurStart(mDateTime);
		mRemainingRecurrences = count;
		int year = mDateTime.date().year();
		if (feb29  &&  !QDate::leapYear(year)
		&&  mDateTime.date().month() == 3  &&  mDateTime.date().day() == 1)
		{
			// The event start date is March 1st, but it is a recurrence
			// on February 29th (recurring on March 1st in non-leap years)
			while (!QDate::leapYear(--year)) ;
			mRecurrence->setRecurStart(QDateTime(QDate(year, 2, 29), mDateTime.time()));
			mRecursFeb29 = true;
		}
		return true;
	}
	else
	{
		delete mRecurrence;
		mRecurrence = 0;
		mRemainingRecurrences = 0;
		return false;
	}
	mUpdated     = true;
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
		if (mRecurrence)
			const_cast<KAlarmEvent*>(this)->mStartDateTime = mRecurrence->recurStart();   // shouldn't be necessary, but just in case...
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
			Alarm* reminderAlarm         = 0;
			Alarm* reminderDeferralAlarm = 0;
			int adjustment = 0;
			for (QPtrListIterator<Alarm> it(alarms);  it.current();  ++it)
			{
				// Parse the next alarm's text
				Alarm& alarm = *it.current();
				AlarmData data;
				readAlarm(alarm, data);
				if (data.type == KAlarmAlarm::MAIN_ALARM)
				{
					QDateTime oldTime = alarm.time();
					alarm.setTime(QDateTime(alarm.time().date(), startOfDay));
					adjustment = oldTime.secsTo(alarm.time());
					changed = true;
					break;
				}
				else if (data.type == KAlarmAlarm::REMINDER_ALARM)
					reminderAlarm = &alarm;
				else if (data.type == KAlarmAlarm::REMINDER_DEFERRAL_ALARM)
					reminderDeferralAlarm = &alarm;
			}
			if (adjustment)
			{
				// Reminder alarms for untimed events are always
#warning "Check"
				if (reminderAlarm  &&  reminderAlarm->hasTime())
					reminderAlarm->setTime(reminderAlarm->time().addSecs(adjustment));
				if (reminderDeferralAlarm  &&  !reminderDeferralAlarm->hasTime())
					reminderDeferralAlarm->setOffset(reminderDeferralAlarm->offset().asSeconds() + adjustment);
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
	// KAlarm pre-0.9 codes held in the alarm's DESCRIPTION property
	static const QChar   SEPARATOR        = ';';
	static const QChar   LATE_CANCEL_CODE = 'C';
	static const QChar   AT_LOGIN_CODE    = 'L';   // subsidiary alarm at every login
	static const QChar   DEFERRAL_CODE    = 'D';   // extra deferred alarm
	static const QString TEXT_PREFIX      = QString::fromLatin1("TEXT:");
	static const QString FILE_PREFIX      = QString::fromLatin1("FILE:");
	static const QString COMMAND_PREFIX   = QString::fromLatin1("CMD:");

	// KAlarm pre-0.9.2 codes held in the event's CATEGORY property
	static const QString BEEP_CATEGORY    = QString::fromLatin1("BEEP");

	int version = calendar.KAlarmVersion();
	if (version >= AlarmCalendar::KAlarmVersion(0,9,2))
		return;

	kdDebug(5950) << "KAlarmEvent::convertKCalEvents(): adjusting\n";
	bool pre_0_7   = (version < AlarmCalendar::KAlarmVersion(0,7,0));
	bool pre_0_9   = (version < AlarmCalendar::KAlarmVersion(0,9,0));
	bool pre_0_9_2 = (version < AlarmCalendar::KAlarmVersion(0,9,2));
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

		if (pre_0_9)
		{
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
			QPtrList<Alarm> alarms = event->alarms();
			for (QPtrListIterator<Alarm> ia(alarms);  ia.current();  ++ia)
			{
				Alarm* alarm = ia.current();
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
						alarm->setDisplayAlarm(txt);
						break;
					case T_COMMAND:
						setProcedureAlarm(alarm, txt);
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

		if (pre_0_9_2)
		{
			/*
			 * It's a KAlarm pre-0.9.2 calendar file.
			 * For display alarms, convert the first unlabelled category to an
			 * X-KDE-KALARM-FONTCOLOUR property.
			 * Convert BEEP category into an audio alarm with no audio file.
			 */
			QStringList cats = event->categories();
			QString colour = (cats.count() > 0) ? cats[0] : QString::null;
			QPtrList<Alarm> alarms = event->alarms();
			for (QPtrListIterator<Alarm> ia(alarms);  ia.current();  ++ia)
			{
				Alarm* alarm = ia.current();
				if (alarm->type() == Alarm::Display)
					alarm->setCustomProperty(APPNAME, FONT_COLOUR_PROPERTY,
					                         QString::fromLatin1("%1;;").arg(colour));
			}

			for (QStringList::iterator it = cats.begin();  it != cats.end();  ++it)
			{
				if (*it == BEEP_CATEGORY)
				{
					cats.remove(it);

					Alarm* alarm = event->newAlarm();
					alarm->setEnabled(true);
					alarm->setAudioAlarm();
					alarm->setTime(event->dtStart());    // default

					// Parse and order the alarms to know which one's date/time to use
					AlarmMap alarmMap;
					readAlarms(*event, &alarmMap);
					AlarmMap::ConstIterator it = alarmMap.begin();
					if (it != alarmMap.end())
					{
						const Alarm* al = it.data().alarm;
						if (al->hasTime())
							alarm->setTime(al->time());
						else
							alarm->setOffset(al->offset());
					}
					break;
				}
			}

			event->setCategories(cats);
		}
	}
}

#ifndef NDEBUG
void KAlarmEvent::dumpDebug() const
{
	kdDebug(5950) << "KAlarmEvent dump:\n";
	KAAlarmEventBase::dumpDebug();
	kdDebug(5950) << "-- mAudioFile:" << mAudioFile << ":\n";
	kdDebug(5950) << "-- mStartDateTime:" << mStartDateTime.toString() << ":\n";
	kdDebug(5950) << "-- mEndDateTime:" << mEndDateTime.toString() << ":\n";
	if (mRepeatAtLogin)
		kdDebug(5950) << "-- mAtLoginDateTime:" << mAtLoginDateTime.toString() << ":\n";
	if (mReminderMinutes)
		kdDebug(5950) << "-- mReminderMinutes:" << mReminderMinutes << ":\n";
	if (mReminderDeferralMinutes)
		kdDebug(5950) << "-- mReminderDeferralMinutes:" << mReminderDeferralMinutes << ":\n";
	if (mReminderArchiveMinutes)
		kdDebug(5950) << "-- mReminderArchiveMinutes:" << mReminderArchiveMinutes << ":\n";
	else if (mDeferral)
		kdDebug(5950) << "-- mDeferralTime:" << mDeferralTime.toString() << ":\n";
	if (mDisplaying)
	{
		kdDebug(5950) << "-- mDisplayingTime:" << mDisplayingTime.toString() << ":\n";
		kdDebug(5950) << "-- mDisplayingFlags:" << mDisplayingFlags << ":\n";
	}
	kdDebug(5950) << "-- mRevision:" << mRevision << ":\n";
	kdDebug(5950) << "-- mRecurrence:" << (mRecurrence ? "true" : "false") << ":\n";
	if (mRecurrence)
	{
		kdDebug(5950) << "-- mRecursFeb29:" << (mRecursFeb29 ? "true" : "false") << ":\n";
		kdDebug(5950) << "-- mRemainingRecurrences:" << mRemainingRecurrences << ":\n";
	}
	kdDebug(5950) << "-- mAlarmCount:" << mAlarmCount << ":\n";
	kdDebug(5950) << "-- mAnyTime:" << (mAnyTime ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mMainExpired:" << (mMainExpired ? "true" : "false") << ":\n";
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
	kdDebug(5950) << "-- mType:" << (mType == MAIN_ALARM ? "MAIN" : mType == REMINDER_ALARM ? "REMINDER" : mType == REMINDER_DEFERRAL_ALARM ? "REMINDER_DEFERRAL" : mType == DEFERRAL_ALARM ? "DEFERRAL" : mType == AT_LOGIN_ALARM ? "LOGIN" : mType == DISPLAYING_ALARM ? "DISPLAYING" : mType == AUDIO_ALARM ? "AUDIO" : "INVALID") << ":\n";
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
	mBgColour         = rhs.mBgColour;
	mFont             = rhs.mFont;
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
	mDefaultFont      = rhs.mDefaultFont;
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
	mDefaultFont   = flags & KAlarmEvent::DEFAULT_FONT;
}

int KAAlarmEventBase::flags() const
{
	return (mBeep          ? KAlarmEvent::BEEP : 0)
	     | (mRepeatAtLogin ? KAlarmEvent::REPEAT_AT_LOGIN : 0)
	     | (mLateCancel    ? KAlarmEvent::LATE_CANCEL : 0)
	     | (mEmailBcc      ? KAlarmEvent::EMAIL_BCC : 0)
	     | (mConfirmAck    ? KAlarmEvent::CONFIRM_ACK : 0)
	     | (mDeferral      ? KAlarmEvent::DEFERRAL : 0)
	     | (mDisplaying    ? KAlarmEvent::DISPLAYING_ : 0)
	     | (mDefaultFont   ? KAlarmEvent::DEFAULT_FONT : 0);

}

const QFont& KAAlarmEventBase::font() const
{
	return mDefaultFont ? theApp()->settings()->messageFont() : mFont;
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
	kdDebug(5950) << "-- mBgColour:" << mBgColour.name() << ":\n";
	kdDebug(5950) << "-- mDefaultFont:" << (mDefaultFont ? "true" : "false") << ":\n";
	if (!mDefaultFont)
		kdDebug(5950) << "-- mFont:" << mFont.toString() << ":\n";
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


/*=============================================================================
= Static functions
=============================================================================*/

/******************************************************************************
 * Set the specified alarm to be a procedure alarm with the given command line.
 * The command line is first split into its program file and arguments before
 * initialising the alarm.
 */
static void setProcedureAlarm(Alarm* alarm, const QString& commandLine)
{
	QString command   = QString::null;
	QString arguments = QString::null;
	// This small state machine is used to parse "commandLine" in order
	// to cut arguments at spaces, but also treat "..." and '...'
	// as a single argument even if they contain spaces. Those simple
	// and double quotes are also removed.
	QChar quoteChar;
	bool quoted = false;
	uint posMax = commandLine.length();
	uint pos;
	for (pos = 0;  pos < posMax;  ++pos)
	{
		QChar ch = commandLine[pos];
		if (quoted)
		{
			if (ch == quoteChar)
			{
				++pos;    // omit the quote character
				break;
			}
			command += ch;
		}
		else
		{
			bool done = false;
			switch (ch)
			{
				case ' ':
				case ';':
				case '|':
				case '<':
					done = !command.isEmpty();
					break;
				case '\'':
				case '"':
					if (command.isEmpty())
					{
						// Start of a quoted string. Omit the quote character.
						quoted = true;
						quoteChar = ch;
						break;
					}
					// fall through to default
				default:
					command += ch;
					break;
			}
			if (done)
				break;
		}
	}

	// Skip any spaces after the command
	for ( ;  pos < posMax  &&  commandLine[pos] == ' ';  ++pos) ;
	arguments = commandLine.mid(pos);

	alarm->setProcedureAlarm(command, arguments);
}
