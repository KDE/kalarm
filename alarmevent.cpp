/*
 *  alarmevent.cpp  -  represents calendar alarms and events
 *  Program:  kalarm
 *  (C) 2001 - 2003 by David Jarvie <software@astrojar.org.uk>
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

#include <klocale.h>
#include <kdebug.h>

#include "kalarm.h"
#include "kalarmapp.h"
#include "preferences.h"
#include "alarmcalendar.h"
#include "alarmevent.h"
using namespace KCal;


const QCString APPNAME("KALARM");

// Custom calendar properties
static const QCString TYPE_PROPERTY("TYPE");    // X-KDE-KALARM-TYPE property
static const QString FILE_TYPE          = QString::fromLatin1("FILE");
static const QString AT_LOGIN_TYPE      = QString::fromLatin1("LOGIN");
static const QString REMINDER_TYPE      = QString::fromLatin1("REMINDER");
static const QString TIME_DEFERRAL_TYPE = QString::fromLatin1("DEFERRAL");
static const QString DATE_DEFERRAL_TYPE = QString::fromLatin1("DATE_DEFERRAL");
static const QString DISPLAYING_TYPE    = QString::fromLatin1("DISPLAYING");   // used only in displaying calendar
static const QCString FONT_COLOUR_PROPERTY("FONTCOLOR");    // X-KDE-KALARM-FONTCOLOR property

// Event categories
static const QString DATE_ONLY_CATEGORY            = QString::fromLatin1("DATE");
static const QString EMAIL_BCC_CATEGORY            = QString::fromLatin1("BCC");
static const QString CONFIRM_ACK_CATEGORY          = QString::fromLatin1("ACKCONF");
static const QString LATE_CANCEL_CATEGORY          = QString::fromLatin1("LATECANCEL");
static const QString ARCHIVE_CATEGORY              = QString::fromLatin1("SAVE");
static const QString ARCHIVE_CATEGORIES            = QString::fromLatin1("SAVE:");

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
	KAlarmAlarm::SubType   type;
	KAAlarmEventBase::Type action;
	int                    displayingFlags;
	bool                   defaultFont;
};
typedef QMap<KAlarmAlarm::SubType, AlarmData> AlarmMap;

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
	mSaveDateTime            = event.mSaveDateTime;
	mAtLoginDateTime         = event.mAtLoginDateTime;
	mDeferralTime            = event.mDeferralTime;
	mDisplayingTime          = event.mDisplayingTime;
	mDisplayingFlags         = event.mDisplayingFlags;
	mReminderMinutes         = event.mReminderMinutes;
	mArchiveReminderMinutes  = event.mArchiveReminderMinutes;
	mRevision                = event.mRevision;
	mRemainingRecurrences    = event.mRemainingRecurrences;
	mExceptionDates          = event.mExceptionDates;
	mExceptionDateTimes      = event.mExceptionDateTimes;
	mAlarmCount              = event.mAlarmCount;
	mRecursFeb29             = event.mRecursFeb29;
	mReminderDeferral        = event.mReminderDeferral;
	mMainExpired             = event.mMainExpired;
	mArchiveRepeatAtLogin    = event.mArchiveRepeatAtLogin;
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
	mEventID                = event.uid();
	mRevision               = event.revision();
	mBeep                   = false;
	mEmailBcc               = false;
	mConfirmAck             = false;
	mLateCancel             = false;
	mArchive                = false;
	mArchiveRepeatAtLogin   = false;
	mArchiveReminderMinutes = 0;
	mBgColour               = QColor(255, 255, 255);    // missing/invalid colour - return white
	mDefaultFont            = true;
	bool floats = false;
	const QStringList& cats = event.categories();
	for (unsigned int i = 0;  i < cats.count();  ++i)
	{
		if (cats[i] == DATE_ONLY_CATEGORY)
			floats = true;
		else if (cats[i] == CONFIRM_ACK_CATEGORY)
			mConfirmAck = true;
		else if (cats[i] == EMAIL_BCC_CATEGORY)
			mEmailBcc = true;
		else if (cats[i] == LATE_CANCEL_CATEGORY)
			mLateCancel = true;
		else if (cats[i] == ARCHIVE_CATEGORY)
			mArchive = true;
		else if (cats[i].startsWith(ARCHIVE_CATEGORIES))
		{
			// It's the archive flag plus a reminder time and/or repeat-at-login flag
			mArchive = true;
			QStringList list = QStringList::split(';', cats[i].mid(ARCHIVE_CATEGORIES.length()));
			for (unsigned int j = 0;  j < list.count();  ++j)
			{
				if (list[j] == AT_LOGIN_TYPE)
					mArchiveRepeatAtLogin = true;
				else
				{
					char ch;
					const char* cat = list[j].latin1();
					while ((ch = *cat) != 0  &&  (ch < '0' || ch > '9'))
						++cat;
					if (ch)
					{
						mArchiveReminderMinutes = ch - '0';
						while ((ch = *++cat) >= '0'  &&  ch <= '9')
							mArchiveReminderMinutes = mArchiveReminderMinutes * 10 + ch - '0';
						switch (ch)
						{
							case 'M':  break;
							case 'H':  mArchiveReminderMinutes *= 60;    break;
							case 'D':  mArchiveReminderMinutes *= 1440;  break;
						}
					}
				}
			}
		}
	}
	mStartDateTime.set(event.dtStart(), floats);
	mDateTime                = mStartDateTime;
	mSaveDateTime            = event.created();

	// Extract status from the event's alarms.
	// First set up defaults.
	mActionType              = T_MESSAGE;
	mRecursFeb29             = false;
	mRepeatAtLogin           = false;
	mDeferral                = false;
	mReminderDeferral        = false;
	mDisplaying              = false;
	mMainExpired             = true;
	mReminderMinutes         = 0;
	mText                    = "";
	mAudioFile               = "";
	mEmailSubject            = "";
	mEmailAddresses.clear();
	mEmailAttachments.clear();
	initRecur();

	// Extract data from all the event's alarms and index the alarms by sequence number
	AlarmMap alarmMap;
	readAlarms(event, &alarmMap);

	// Incorporate the alarms' details into the overall event
	AlarmMap::ConstIterator it = alarmMap.begin();
	mAlarmCount = 0;       // initialise as invalid
	DateTime reminderTime;
	DateTime alTime;
	bool set = false;
	for (  ;  it != alarmMap.end();  ++it)
	{
		const AlarmData& data = it.data();
		switch (data.type)
		{
			case KAlarmAlarm::MAIN__ALARM:
				mMainExpired = false;
				alTime.set(data.dateTime, mStartDateTime.isDateOnly());
				break;
			case KAlarmAlarm::AT_LOGIN__ALARM:
				mRepeatAtLogin   = true;
				mAtLoginDateTime = data.dateTime;
				alTime = mAtLoginDateTime;
				break;
			case KAlarmAlarm::REMINDER__ALARM:
				reminderTime.set(data.dateTime, mStartDateTime.isDateOnly());
				alTime = reminderTime;
				break;
			case KAlarmAlarm::DEFERRED_REMINDER_DATE__ALARM:
				mReminderDeferral = true;
				// fall through to DEFERRED_DATE__ALARM
			case KAlarmAlarm::DEFERRED_DATE__ALARM:
				mDeferral = true;
				mDeferralTime.set(data.dateTime, false);
				alTime = mDeferralTime;
				break;
			case KAlarmAlarm::DEFERRED_REMINDER_TIME__ALARM:
				mReminderDeferral = true;
				// fall through to DEFERRED_TIME__ALARM
			case KAlarmAlarm::DEFERRED_TIME__ALARM:
				mDeferral = true;
				mDeferralTime.set(data.dateTime);
				alTime = mDeferralTime;
				break;
			case KAlarmAlarm::DISPLAYING__ALARM:
			{
				mDisplaying      = true;
				mDisplayingFlags = data.displayingFlags;
				bool dateOnly = (mDisplayingFlags & DEFERRAL) ? !(mDisplayingFlags & TIMED_FLAG)
				              : mStartDateTime.isDateOnly();
				mDisplayingTime.set(data.dateTime, dateOnly);
				alTime = mDisplayingTime;
				break;
			}
			case KAlarmAlarm::AUDIO__ALARM:
				mAudioFile = data.cleanText;
				mBeep      = mAudioFile.isEmpty();
				break;
			case KAlarmAlarm::INVALID__ALARM:
			default:
				break;
		}

		if (data.action != T_AUDIO)
		{
			// Ensure that the basic fields are set up even if there is no main
			// alarm in the event (if it has expired and then been deferred)
			if (!set)
			{
				mDateTime   = alTime;
				mActionType = data.action;
				mText = (mActionType == T_COMMAND) ? data.cleanText.stripWhiteSpace() : data.cleanText;
				switch (data.action)
				{
					case T_MESSAGE:
						mFont        = data.font;
						mDefaultFont = data.defaultFont;
						// fall through to T_FILE
					case T_FILE:
						mBgColour    = data.bgColour;
						break;
					case T_EMAIL:
						mEmailAddresses   = data.emailAddresses;
						mEmailSubject     = data.emailSubject;
						mEmailAttachments = data.emailAttachments;
						break;
					default:
						break;
				}
				set = true;
			}
			if (data.action == T_FILE  &&  mActionType == T_MESSAGE)
				mActionType = T_FILE;
			++mAlarmCount;
		}
	}
	if (reminderTime.isValid())
	{
		mReminderMinutes = reminderTime.secsTo(mDateTime) / 60;
		if (mReminderMinutes)
			mArchiveReminderMinutes = 0;
	}
	if (mRepeatAtLogin)
		mArchiveRepeatAtLogin = false;

	Recurrence* recur = event.recurrence();
	if (recur  &&  recur->doesRecur() != Recurrence::rNone)
	{
		setRecurrence(*recur);
		mExceptionDates     = event.exDates();
		mExceptionDateTimes = event.exDateTimes();
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
	Alarm::List alarms = event.alarms();
        for (Alarm::List::ConstIterator it = alarms.begin();  it != alarms.end();  ++it)
	{
		// Parse the next alarm's text
		AlarmData data;
		readAlarm(**it, data);
		if (data.type != KAlarmAlarm::INVALID__ALARM)
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
			data.bgColour = QColor(255, 255, 255);   // white
			data.fgColour = QColor(0, 0, 0);         // black
			int n = list.count();
			if (n > 0)
			{
				QColor c(list[0]);
				if (c.isValid())
					data.bgColour = c;
				if (n > 1)
				{
					QColor c(list[1]);
					if (c.isValid())
						data.fgColour = c;
				}
			}
			data.defaultFont = (n <= 2 || list[2].isEmpty());
			if (!data.defaultFont)
				data.font.fromString(list[2]);
			break;
		}
		case Alarm::Audio:
			data.action    = T_AUDIO;
			data.cleanText = alarm.audioFile();
			data.type      = KAlarmAlarm::AUDIO__ALARM;
			return;
		case Alarm::Invalid:
			data.type = KAlarmAlarm::INVALID__ALARM;
			return;
	}

	bool atLogin      = false;
	bool reminder     = false;
	bool deferral     = false;
	bool dateDeferral = false;
	data.type = KAlarmAlarm::MAIN__ALARM;
	QString property = alarm.customProperty(APPNAME, TYPE_PROPERTY);
	QStringList types = QStringList::split(QChar(','), property);
	for (unsigned int i = 0;  i < types.count();  ++i)
	{
		QString type = types[i];
		if (type == AT_LOGIN_TYPE)
			atLogin = true;
		else if (type == FILE_TYPE  &&  data.action == T_MESSAGE)
			data.action = T_FILE;
		else if (type == REMINDER_TYPE)
			reminder = true;
		else if (type == TIME_DEFERRAL_TYPE)
			deferral = true;
		else if (type == DATE_DEFERRAL_TYPE)
			dateDeferral = deferral = true;
		else if (type == DISPLAYING_TYPE)
			data.type = KAlarmAlarm::DISPLAYING__ALARM;
	}

	if (reminder)
	{
		if (data.type == KAlarmAlarm::MAIN__ALARM)
			data.type = dateDeferral ? KAlarmAlarm::DEFERRED_REMINDER_DATE__ALARM
			          : deferral ? KAlarmAlarm::DEFERRED_REMINDER_TIME__ALARM : KAlarmAlarm::REMINDER__ALARM;
		else if (data.type == KAlarmAlarm::DISPLAYING__ALARM)
			data.displayingFlags = dateDeferral ? REMINDER | DATE_DEFERRAL
			                     : deferral ? REMINDER | TIME_DEFERRAL : REMINDER;
	}
	else if (deferral)
	{
		if (data.type == KAlarmAlarm::MAIN__ALARM)
			data.type = dateDeferral ? KAlarmAlarm::DEFERRED_DATE__ALARM : KAlarmAlarm::DEFERRED_TIME__ALARM;
		else if (data.type == KAlarmAlarm::DISPLAYING__ALARM)
			data.displayingFlags = dateDeferral ? DATE_DEFERRAL : TIME_DEFERRAL;
	}
	if (atLogin)
	{
		if (data.type == KAlarmAlarm::MAIN__ALARM)
			data.type = KAlarmAlarm::AT_LOGIN__ALARM;
		else if (data.type == KAlarmAlarm::DISPLAYING__ALARM)
			data.displayingFlags = REPEAT_AT_LOGIN;
	}
//kdDebug()<<"ReadAlarm(): text="<<alarm.text()<<", time="<<alarm.time().toString()<<", valid time="<<alarm.time().isValid()<<endl;
}

/******************************************************************************
 * Initialise the KAlarmEvent with the specified parameters.
 */
void KAlarmEvent::set(const QDateTime& dateTime, const QString& text, const QColor& colour, const QFont& font, Action action, int flags)
{
	initRecur();
	mStartDateTime.set(dateTime, flags & ANY_TIME);
	mDateTime = mStartDateTime;
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
	mText                   = (mActionType == T_COMMAND) ? text.stripWhiteSpace() : text;
	mAudioFile              = "";
	mBgColour               = colour;
	mFont                   = font;
	mAlarmCount             = 1;
	set(flags);
	mReminderMinutes        = 0;
	mArchiveReminderMinutes = 0;
	mArchiveRepeatAtLogin   = false;
	mDeferral               = false;
	mReminderDeferral       = false;
	mDisplaying             = false;
	mMainExpired            = false;
	mArchive                = false;
	mUpdated                = false;
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
 * Reinitialise the start date/time byt adjusting its date part, and setting
 * the next scheduled alarm to the new start date/time.
 */
void KAlarmEvent::adjustStartDate(const QDate& d)
{
	if (mStartDateTime.isDateOnly())
	{
		mStartDateTime = d;
		if (mRecurrence)
			mRecurrence->setRecurStart(d);
	}
	else
	{
		mStartDateTime.set(d, mStartDateTime.time());
		if (mRecurrence)
			mRecurrence->setRecurStart(mStartDateTime.dateTime());
	}
	mDateTime = mStartDateTime;
}

/******************************************************************************
 * Return the time of the next scheduled occurrence of the event.
 */
DateTime KAlarmEvent::nextDateTime() const
{
	return mReminderMinutes ? mDateTime.addSecs(-mReminderMinutes * 60)
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
	mStartDateTime.setDateOnly(flags & ANY_TIME);
	mUpdated = true;
}

int KAlarmEvent::flags() const
{
	return KAAlarmEventBase::flags() | (mStartDateTime.isDateOnly() ? ANY_TIME : 0);
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
	if (mStartDateTime.isDateOnly())
		cats.append(DATE_ONLY_CATEGORY);
	if (mConfirmAck)
		cats.append(CONFIRM_ACK_CATEGORY);
	if (mEmailBcc)
		cats.append(EMAIL_BCC_CATEGORY);
	if (mLateCancel)
		cats.append(LATE_CANCEL_CATEGORY);
	if (mArchive  &&  !original)
	{
		QStringList params;
		if (mArchiveReminderMinutes)
		{
			char unit = 'M';
			int count = mArchiveReminderMinutes;
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
			params += QString("%1%2").arg(count).arg(unit);
		}
		if (mArchiveRepeatAtLogin)
			params += AT_LOGIN_TYPE;
		if (params.count() > 0)
		{
			QString cat = ARCHIVE_CATEGORIES;
			cat += params.join(QString::fromLatin1(";"));
			cats.append(cat);
		}
		else
			cats.append(ARCHIVE_CATEGORY);
	}
	ev.setCategories(cats);
	ev.setRevision(mRevision);
	ev.clearAlarms();

	// Always set DTSTART as date/time, since alarm times can only be specified
	// in local time (instead of UTC) if they are relative to a DTSTART or DTEND
	// which is also specified in local time. Instead of calling setFloats() to
	// indicate a date-only event, the category "DATE" is included.
	ev.setDtStart(mStartDateTime.dateTime());
	ev.setFloats(false);
	ev.setHasEndDate(false);

	DateTime dtMain = original ? mStartDateTime : mDateTime;
	DateTime audioTime;
	if (!mMainExpired  ||  original)
	{
		// Add the main alarm
		initKcalAlarm(ev, dtMain, QStringList());
		audioTime = dtMain;
	}

	// Add subsidiary alarms
	if (mRepeatAtLogin  ||  mArchiveRepeatAtLogin && original)
	{
		DateTime dtl;
		if (mArchiveRepeatAtLogin)
			dtl = mStartDateTime.dateTime().addDays(-1);
		else if (mAtLoginDateTime.isValid())
			dtl = mAtLoginDateTime;
		else if (mStartDateTime.isDateOnly())
			dtl = QDate::currentDate().addDays(-1);
		else
			dtl = QDateTime::currentDateTime();
		initKcalAlarm(ev, dtl, AT_LOGIN_TYPE);
		if (!audioTime.isValid())
			audioTime = dtl;
	}
	if (mReminderMinutes  ||  mArchiveReminderMinutes && original)
	{
		int minutes = mReminderMinutes ? mReminderMinutes : mArchiveReminderMinutes;
		DateTime reminderTime = dtMain.addSecs(-minutes * 60);
		initKcalAlarm(ev, reminderTime, QStringList(REMINDER_TYPE));
		if (!audioTime.isValid())
			audioTime = reminderTime;
	}
	if (mDeferral)
	{
		QStringList list;
		if (mDeferralTime.isDateOnly())
			list += DATE_DEFERRAL_TYPE;
		else
			list += TIME_DEFERRAL_TYPE;
		if (mReminderDeferral)
			list += REMINDER_TYPE;
		initKcalAlarm(ev, mDeferralTime, list);
		if (!audioTime.isValid())
			audioTime = mDeferralTime;
	}
	if (mDisplaying)
	{
		QStringList list(DISPLAYING_TYPE);
		if (mDisplayingFlags & REPEAT_AT_LOGIN)
			list += AT_LOGIN_TYPE;
		else if (mDisplayingFlags & DEFERRAL)
		{
			if (mDisplayingFlags & TIMED_FLAG)
				list += TIME_DEFERRAL_TYPE;
			else
				list += DATE_DEFERRAL_TYPE;
		}
		if (mDisplayingFlags & REMINDER)
			list += REMINDER_TYPE;
		initKcalAlarm(ev, mDisplayingTime, list);
		if (!audioTime.isValid())
			audioTime = mDisplayingTime;
	}
	if (mBeep  ||  !mAudioFile.isEmpty())
	{
		KAAlarmEventBase::Type actType = mActionType;
		const_cast<KAlarmEvent*>(this)->mActionType = T_AUDIO;
		initKcalAlarm(ev, audioTime, QStringList());
		const_cast<KAlarmEvent*>(this)->mActionType = actType;
	}

	// Add recurrence data
	ev.setExDates(DateList());
	ev.setExDates(DateTimeList());
	if (mRecurrence)
	{
		Recurrence* recur = ev.recurrence();
		int frequency = mRecurrence->frequency();
		int duration  = mRecurrence->duration();
		const QDateTime& endDateTime = mRecurrence->endDateTime();
		recur->setRecurStart(mStartDateTime.dateTime());
		ushort rectype = mRecurrence->doesRecur();
		switch (rectype)
		{
			case Recurrence::rHourly:
				frequency *= 60;
				// fall through to Recurrence::rMinutely
			case Recurrence::rMinutely:
				setRecurMinutely(*recur, frequency, duration, endDateTime);
				break;
			case Recurrence::rDaily:
				setRecurDaily(*recur, frequency, duration, endDateTime.date());
				break;
			case Recurrence::rWeekly:
				setRecurWeekly(*recur, frequency, mRecurrence->days(), duration, endDateTime.date());
				break;
			case Recurrence::rMonthlyDay:
				setRecurMonthlyByDate(*recur, frequency, mRecurrence->monthDays(), duration, endDateTime.date());
				break;
			case Recurrence::rMonthlyPos:
				setRecurMonthlyByPos(*recur, frequency, mRecurrence->monthPositions(), duration, endDateTime.date());
				break;
			case Recurrence::rYearlyMonth:
				setRecurAnnualByDate(*recur, frequency, mRecurrence->yearNums(), duration, endDateTime.date());
				break;
			case Recurrence::rYearlyPos:
				setRecurAnnualByPos(*recur, frequency, mRecurrence->yearMonthPositions(), mRecurrence->yearNums(), duration, endDateTime.date());
				break;
			case Recurrence::rYearlyDay:
				setRecurAnnualByDay(*recur, frequency, mRecurrence->yearNums(), duration, endDateTime.date());
				break;
			default:
				break;
		}
		ev.setExDates(mExceptionDates);
		ev.setExDates(mExceptionDateTimes);
	}

	if (mSaveDateTime.isValid())
		ev.setCreated(mSaveDateTime);
	ev.setReadOnly(readOnly);
	return true;
}

/******************************************************************************
 * Create a new alarm for a libkcal event, and initialise it according to the
 * alarm action. If 'type' is non-null, it is appended to the X-KDE-KALARM-TYPE
 * property value list.
 */
Alarm* KAlarmEvent::initKcalAlarm(Event& event, const DateTime& dt, const QStringList& types) const
{
	QStringList alltypes;
	Alarm* alarm = event.newAlarm();
	alarm->setEnabled(true);
	// RFC2445 specifies that absolute alarm times must be stored as UTC.
	// So, in order to store local times, set the alarm time as an offset to DTSTART.
	alarm->setStartOffset(dt.isDateOnly() ? mStartDateTime.secsTo(dt)
	                                      : mStartDateTime.dateTime().secsTo(dt.dateTime()));

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
		case T_AUDIO:
			alarm->setAudioAlarm(mAudioFile);  // empty for a beep
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
	KAlarmAlarm al;   // this sets type to INVALID_ALARM
	if (mAlarmCount)
	{
		al.mEventID       = mEventID;
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
					al.mType     = KAlarmAlarm::MAIN__ALARM;
					al.mDateTime = mDateTime;
				}
				break;
			case KAlarmAlarm::REMINDER_ALARM:
				if (mReminderMinutes)
				{
					al.mType     = KAlarmAlarm::REMINDER__ALARM;
					al.mDateTime = mDateTime.addMins(-mReminderMinutes);
				}
				break;
			case KAlarmAlarm::DEFERRED_REMINDER_ALARM:
				if (!mReminderDeferral)
					break;
				// fall through to DEFERRED_ALARM
			case KAlarmAlarm::DEFERRED_ALARM:
				if (mDeferral)
				{
					al.mType = static_cast<KAlarmAlarm::SubType>((mReminderDeferral ? KAlarmAlarm::DEFERRED_REMINDER_ALARM : KAlarmAlarm::DEFERRED_ALARM)
					                                             | (mDeferralTime.isDateOnly() ? 0 : KAlarmAlarm::TIMED_DEFERRAL_FLAG));
					al.mDateTime = mDeferralTime;
					al.mDeferral = true;
				}
				break;
			case KAlarmAlarm::AT_LOGIN_ALARM:
				if (mRepeatAtLogin)
				{
					al.mType          = KAlarmAlarm::AT_LOGIN__ALARM;
					al.mDateTime      = mAtLoginDateTime;
					al.mRepeatAtLogin = true;
					al.mLateCancel    = false;
				}
				break;
			case KAlarmAlarm::DISPLAYING_ALARM:
				if (mDisplaying)
				{
					al.mType       = KAlarmAlarm::DISPLAYING__ALARM;
					al.mDateTime   = mDisplayingTime;
					al.mDisplaying = true;
				}
				break;
			case KAlarmAlarm::AUDIO_ALARM:
			case KAlarmAlarm::INVALID_ALARM:
			default:
				break;
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
			if (mReminderDeferral)
				return alarm(KAlarmAlarm::DEFERRED_REMINDER_ALARM);
			if (mDeferral)
				return alarm(KAlarmAlarm::DEFERRED_ALARM);
			// fall through to DEFERRED_ALARM
		case KAlarmAlarm::DEFERRED_REMINDER_ALARM:
		case KAlarmAlarm::DEFERRED_ALARM:
			if (mRepeatAtLogin)
				return alarm(KAlarmAlarm::AT_LOGIN_ALARM);
			// fall through to AT_LOGIN_ALARM
		case KAlarmAlarm::AT_LOGIN_ALARM:
			if (mDisplaying)
				return alarm(KAlarmAlarm::DISPLAYING_ALARM);
			// fall through to DISPLAYING_ALARM
		case KAlarmAlarm::DISPLAYING_ALARM:
			// fall through to default
		case KAlarmAlarm::AUDIO_ALARM:
		case KAlarmAlarm::INVALID_ALARM:
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
				// Remove the at-login alarm, but keep a note of it for archiving purposes
				mArchiveRepeatAtLogin = true;
				mRepeatAtLogin = false;
				--mAlarmCount;
			}
			break;
		case KAlarmAlarm::REMINDER_ALARM:
			if (mReminderMinutes)
			{
				// Remove the reminder alarm, but keep a note of it for archiving purposes
				mArchiveReminderMinutes = mReminderMinutes;
				mReminderMinutes = 0;
				--mAlarmCount;
			}
			break;
		case KAlarmAlarm::DEFERRED_REMINDER_ALARM:
		case KAlarmAlarm::DEFERRED_ALARM:
			if (mDeferral)
			{
				mReminderDeferral = false;
				mDeferral         = false;
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
		case KAlarmAlarm::INVALID_ALARM:
		default:
			break;
	}
	if (mAlarmCount != count)
		mUpdated = true;
}

/******************************************************************************
 * Defer the event to the specified time.
 * If the main alarm time has passed, the main alarm is marked as expired.
 * If 'adjustRecurrence' is true, ensure that the next scheduled recurrence is
 * after the current time.
 */
void KAlarmEvent::defer(const DateTime& dateTime, bool reminder, bool adjustRecurrence)
{
	if (checkRecur() == NO_RECUR)
	{
		if (mReminderMinutes)
		{
			// Remove the reminder alarm, but keep a note of it for archiving purposes
			mArchiveReminderMinutes = mReminderMinutes;
		}
		if (mReminderMinutes  ||  mReminderDeferral  ||  mArchiveReminderMinutes)
		{
			QDateTime dt = mDateTime.dateTime();
			if (dateTime < dt)
			{
				if (!mReminderMinutes  &&  !mReminderDeferral)
					++mAlarmCount;
				mDeferralTime     = dateTime;   // defer reminder alarm
				mReminderDeferral = true;
				mDeferral         = true;
			}
			else
			{
				// Deferring past the main alarm time, so it no longer counts as a deferral
				if (mReminderMinutes  ||  mReminderDeferral)
				{
					mReminderDeferral = false;
					mDeferral         = false;
					--mAlarmCount;
				}
			}
			mReminderMinutes = 0;
		}
		if (!mReminderDeferral)
		{
			// Main alarm has now expired
			mDateTime = mDeferralTime = dateTime;
			if (!mDeferral)
			{
				mDeferral = true;
				++mAlarmCount;
			}
			if (!mMainExpired)
			{
				mMainExpired = true;
				--mAlarmCount;
				if (mRepeatAtLogin)
				{
					// Remove the repeat-at-login alarm, but keep a note of it for archiving purposes
					mArchiveRepeatAtLogin = true;
					mRepeatAtLogin = false;
					--mAlarmCount;
				}
			}
		}
	}
	else if (reminder)
	{
		// Deferring a reminder for a recurring alarm
		if (dateTime >= mDateTime.dateTime())
		{
			mReminderDeferral = false;    // (error)
			if (mDeferral)
			{
				mDeferral = false;
				--mAlarmCount;
			}
		}
		else
		{
			mDeferralTime = dateTime;
			mReminderDeferral = true;
			if (!mDeferral)
			{
				mDeferral = true;
				++mAlarmCount;
			}
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
			if (mDateTime.dateTime() < now)
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
		mDeferralTime = DateTime();
		mDeferral     = false;
		--mAlarmCount;
		mUpdated = true;
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
	  || alarmType == KAlarmAlarm::REMINDER_ALARM
	  || alarmType == KAlarmAlarm::DEFERRED_REMINDER_ALARM
	  || alarmType == KAlarmAlarm::DEFERRED_ALARM
	  || alarmType == KAlarmAlarm::AT_LOGIN_ALARM))
	{
kdDebug()<<"KAlarmEvent::setDisplaying("<<event.id()<<", "<<(alarmType==KAlarmAlarm::MAIN_ALARM?"MAIN":alarmType==KAlarmAlarm::REMINDER_ALARM?"REMINDER":alarmType==KAlarmAlarm::DEFERRED_REMINDER_ALARM?"REMINDER_DEFERRAL":alarmType==KAlarmAlarm::DEFERRED_ALARM?"DEFERRAL":"LOGIN")<<"): time="<<repeatAtLoginTime.toString()<<endl;
		KAlarmAlarm al = event.alarm(alarmType);
		if (al.valid())
		{
			*this = event;
			setUid(DISPLAYING);
			mDisplaying     = true;
			mDisplayingTime = (alarmType == KAlarmAlarm::AT_LOGIN_ALARM) ? repeatAtLoginTime : al.dateTime();
			switch (al.type())
			{
				case KAlarmAlarm::AT_LOGIN__ALARM:                mDisplayingFlags = REPEAT_AT_LOGIN;  break;
				case KAlarmAlarm::REMINDER__ALARM:                mDisplayingFlags = REMINDER;  break;
				case KAlarmAlarm::DEFERRED_REMINDER_TIME__ALARM:  mDisplayingFlags = REMINDER | TIME_DEFERRAL;  break;
				case KAlarmAlarm::DEFERRED_REMINDER_DATE__ALARM:  mDisplayingFlags = REMINDER | DATE_DEFERRAL;  break;
				case KAlarmAlarm::DEFERRED_TIME__ALARM:           mDisplayingFlags = TIME_DEFERRAL;  break;
				case KAlarmAlarm::DEFERRED_DATE__ALARM:           mDisplayingFlags = DATE_DEFERRAL;  break;
				default:                                          mDisplayingFlags = 0;  break;
			}
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
			al.mType = KAlarmAlarm::AT_LOGIN__ALARM;
		}
		else if (mDisplayingFlags & DEFERRAL)
		{
			al.mDeferral = true;
			al.mType = (mDisplayingFlags == REMINDER | DATE_DEFERRAL) ? KAlarmAlarm::DEFERRED_REMINDER_DATE__ALARM
			         : (mDisplayingFlags == REMINDER | TIME_DEFERRAL) ? KAlarmAlarm::DEFERRED_REMINDER_TIME__ALARM
			         : (mDisplayingFlags == DATE_DEFERRAL) ? KAlarmAlarm::DEFERRED_DATE__ALARM
			         : KAlarmAlarm::DEFERRED_TIME__ALARM;
		}
		else if (mDisplayingFlags & REMINDER)
			al.mType = KAlarmAlarm::REMINDER__ALARM;
		else
			al.mType = KAlarmAlarm::MAIN__ALARM;
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
KAlarmEvent::OccurType KAlarmEvent::nextOccurrence(const QDateTime& preDateTime, DateTime& result) const
{
	if (checkRecur() != NO_RECUR)
	{
		int remainingCount;
		return nextRecurrence(preDateTime, result, remainingCount);
	}
	if (preDateTime < mDateTime.dateTime())
	{
		result = mDateTime;
		return FIRST_OCCURRENCE;
	}
	result = DateTime();
	return NO_OCCURRENCE;
}

/******************************************************************************
 * Get the date/time of the last previous occurrence of the event, before the
 * specified date/time.
 * 'result' = date/time of previous occurrence, or invalid date/time if none.
 */
KAlarmEvent::OccurType KAlarmEvent::previousOccurrence(const QDateTime& afterDateTime, DateTime& result) const
{
	if (checkRecur() == NO_RECUR)
	{
		result = QDateTime();
		return NO_OCCURRENCE;
	}
	QDateTime recurStart = mRecurrence->recurStart();
	QDateTime after = afterDateTime;
	if (mStartDateTime.isDateOnly()  &&  afterDateTime.time() > theApp()->preferences()->startOfDay())
		after = after.addDays(1);    // today's recurrence (if today recurs) has passed
	bool last;
	QDateTime dt = mRecurrence->getPreviousDateTime(after, &last);
	result.set(dt, mStartDateTime.isDateOnly());
	if (!dt.isValid())
		return NO_OCCURRENCE;
	if (dt == recurStart)
		return FIRST_OCCURRENCE;
	if (last)
		return LAST_OCCURRENCE;
	return result.isDateOnly() ? RECURRENCE_DATE : RECURRENCE_DATE_TIME;
}

/******************************************************************************
 * Set the date/time of the event to the next scheduled occurrence after the
 * specified date/time. Any reminder alarm is adjusted accordingly.
 */
KAlarmEvent::OccurType KAlarmEvent::setNextOccurrence(const QDateTime& preDateTime)
{
	if (preDateTime < mDateTime.dateTime())
		return FIRST_OCCURRENCE;
	OccurType type;
	if (checkRecur() != NO_RECUR)
	{
		int remainingCount;
		DateTime newTime;
		type = nextRecurrence(preDateTime, newTime, remainingCount);
		if (type != FIRST_OCCURRENCE  &&  type != NO_OCCURRENCE  &&  newTime != mDateTime)
		{
			mDateTime = newTime;
			if (mRecurrence->duration() > 0)
				mRemainingRecurrences = remainingCount;
			if (mReminderDeferral  ||  mArchiveReminderMinutes)
			{
				if (!mReminderMinutes)
					++mAlarmCount;
				mReminderMinutes = mArchiveReminderMinutes;
			}
			if (mReminderDeferral)
			{
				mReminderDeferral = false;
				mDeferral         = false;
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
KAlarmEvent::OccurType KAlarmEvent::nextRecurrence(const QDateTime& preDateTime, DateTime& result, int& remainingCount) const
{
	QDateTime recurStart = mRecurrence->recurStart();
	QDateTime pre = preDateTime;
	if (mStartDateTime.isDateOnly()  &&  preDateTime.time() < theApp()->preferences()->startOfDay())
		pre = pre.addDays(-1);    // today's recurrence (if today recurs) is still to come
	remainingCount = 0;
	bool last;
	QDateTime dt = mRecurrence->getNextDateTime(pre, &last);
	result.set(dt, mStartDateTime.isDateOnly());
	if (!dt.isValid())
		return NO_OCCURRENCE;
	if (dt == recurStart)
	{
		remainingCount = mRecurrence->duration();
		return FIRST_OCCURRENCE;
	}
	if (last)
	{
		remainingCount = 1;
		return LAST_OCCURRENCE;
	}
	remainingCount = mRecurrence->duration() - mRecurrence->durationTo(dt) + 1;
	return result.isDateOnly() ? RECURRENCE_DATE : RECURRENCE_DATE_TIME;
}

/******************************************************************************
 * Return the recurrence interval as text suitable for display.
 */
QString KAlarmEvent::recurrenceText(bool brief) const
{
	if (mRepeatAtLogin)
		return brief ? i18n("At Login", "Login") : i18n("At login");
	if (mRecurrence)
	{
		int frequency = mRecurrence->frequency();
		switch (mRecurrence->doesRecur())
		{
			case Recurrence::rHourly:
				frequency *= 60;
				// fall through to rMinutely
			case Recurrence::rMinutely:
				if (frequency < 60)
					return i18n("1 Minute", "%n Minutes", frequency);
				else if (frequency % 60 == 0)
					return i18n("1 Hour", "%n Hours", frequency/60);
				else
				{
					QString mins;
					return i18n("Hours and Minutes", "%1H %2M").arg(QString::number(frequency/60)).arg(mins.sprintf("%02d", frequency%60));
				}
			case Recurrence::rDaily:
				return i18n("1 Day", "%n Days", frequency);
			case Recurrence::rWeekly:
				return i18n("1 Week", "%n Weeks", frequency);
			case Recurrence::rMonthlyDay:
			case Recurrence::rMonthlyPos:
				return i18n("1 Month", "%n Months", frequency);
			case Recurrence::rYearlyMonth:
			case Recurrence::rYearlyPos:
			case Recurrence::rYearlyDay:
				return i18n("1 Year", "%n Years", frequency);
			case Recurrence::rNone:
			default:
				break;
		}
	}
	return QString::null;
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
		DateTime next;
		QDateTime recurStart = mRecurrence->recurStart();
		QDateTime pre = mDateTime.dateTime().addDays(-1);
		mRecurrence->setRecurStart(pre);
		nextRecurrence(pre, next, remainingCount);
		if (!next.isValid())
			mRecurrence->setRecurStart(recurStart);   // reinstate the old value
		else
		{
			mRecurrence->setRecurStart(next.dateTime());
			mStartDateTime = mDateTime = next;
			mUpdated = true;
		}
	}
}

/******************************************************************************
*  Initialise the event's recurrence from a KCal::Recurrence.
*  The event's start date/time is not changed.
*/
void KAlarmEvent::setRecurrence(const Recurrence& recurrence)
{
	mUpdated     = true;
	mRecursFeb29 = false;
	delete mRecurrence;
	// Copy the recurrence details.
	switch (recurrence.doesRecur())
	{
		case Recurrence::rYearlyMonth:
		{
			QDate start = recurrence.recurStart().date();
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
			mRecurrence = new Recurrence(recurrence, 0);
			mRecurrence->setRecurStart(mStartDateTime.dateTime());
			mRecurrence->setFloats(mStartDateTime.isDateOnly());
			mRemainingRecurrences = mRecurrence->duration();
			if (mRemainingRecurrences > 0)
				mRemainingRecurrences -= mRecurrence->durationTo(mDateTime.dateTime()) - 1;
			break;

		default:
			mRecurrence = 0;
			mRemainingRecurrences = 0;
			break;
	}
}

/******************************************************************************
 * Set the recurrence to recur at a minutes interval.
 * Parameters:
 *    freq  = how many minutes between recurrences.
 *    count = number of occurrences, including first and last.
 *          = -1 to recur indefinitely.
 *          = 0 to use 'end' instead.
 *    end   = end date/time (invalid to use 'count' instead).
 */
bool KAlarmEvent::setRecurMinutely(Recurrence& recurrence, int freq, int count, const QDateTime& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setMinutely(freq, count);
	else if (end.isValid())
		recurrence.setMinutely(freq, end);
	else
		return false;
	return true;
}

/******************************************************************************
 * Set the recurrence to recur daily.
 * Parameters:
 *    freq  = how many days between recurrences.
 *    count = number of occurrences, including first and last.
 *          = -1 to recur indefinitely.
 *          = 0 to use 'end' instead.
 *    end   = end date (invalid to use 'count' instead).
 */
bool KAlarmEvent::setRecurDaily(Recurrence& recurrence, int freq, int count, const QDate& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setDaily(freq, count);
	else if (end.isValid())
		recurrence.setDaily(freq, end);
	else
		return false;
	return true;
}

/******************************************************************************
 * Set the recurrence to recur weekly, on the specified weekdays.
 * Parameters:
 *    freq  = how many weeks between recurrences.
 *    days  = which days of the week alarms should occur on.
 *    count = number of occurrences, including first and last.
 *          = -1 to recur indefinitely.
 *          = 0 to use 'end' instead.
 *    end   = end date (invalid to use 'count' instead).
 */
bool KAlarmEvent::setRecurWeekly(Recurrence& recurrence, int freq, const QBitArray& days, int count, const QDate& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setWeekly(freq, days, count);
	else if (end.isValid())
		recurrence.setWeekly(freq, days, end);
	else
		return false;
	return true;
}

/******************************************************************************
 * Set the recurrence to recur monthly, on the specified days within the month.
 * Parameters:
 *    freq  = how many months between recurrences.
 *    days  = which days of the month alarms should occur on.
 *    count = number of occurrences, including first and last.
 *          = -1 to recur indefinitely.
 *          = 0 to use 'end' instead.
 *    end   = end date (invalid to use 'count' instead).
 */
bool KAlarmEvent::setRecurMonthlyByDate(Recurrence& recurrence, int freq, const QValueList<int>& days, int count, const QDate& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setMonthly(Recurrence::rMonthlyDay, freq, count);
	else if (end.isValid())
		recurrence.setMonthly(Recurrence::rMonthlyDay, freq, end);
	else
		return false;
	for (QValueListConstIterator<int> it = days.begin();  it != days.end();  ++it)
		recurrence.addMonthlyDay(*it);
	return true;
}

bool KAlarmEvent::setRecurMonthlyByDate(Recurrence& recurrence, int freq, const QPtrList<int>& days, int count, const QDate& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setMonthly(Recurrence::rMonthlyDay, freq, count);
	else if (end.isValid())
		recurrence.setMonthly(Recurrence::rMonthlyDay, freq, end);
	else
		return false;
	for (QPtrListIterator<int> it(days);  it.current();  ++it)
		recurrence.addMonthlyDay(*it.current());
	return true;
}

/******************************************************************************
 * Set the recurrence to recur monthly, on the specified weekdays in the
 * specified weeks of the month.
 * Parameters:
 *    freq  = how many months between recurrences.
 *    posns = which days of the week/weeks of the month alarms should occur on.
 *    count = number of occurrences, including first and last.
 *          = -1 to recur indefinitely.
 *          = 0 to use 'end' instead.
 *    end   = end date (invalid to use 'count' instead).
 */
bool KAlarmEvent::setRecurMonthlyByPos(Recurrence& recurrence, int freq, const QValueList<MonthPos>& posns, int count, const QDate& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setMonthly(Recurrence::rMonthlyPos, freq, count);
	else if (end.isValid())
		recurrence.setMonthly(Recurrence::rMonthlyPos, freq, end);
	else
		return false;
	for (QValueListConstIterator<MonthPos> it = posns.begin();  it != posns.end();  ++it)
		recurrence.addMonthlyPos((*it).weeknum, (*it).days);
	return true;
}

bool KAlarmEvent::setRecurMonthlyByPos(Recurrence& recurrence, int freq, const QPtrList<Recurrence::rMonthPos>& posns, int count, const QDate& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setMonthly(Recurrence::rMonthlyPos, freq, count);
	else if (end.isValid())
		recurrence.setMonthly(Recurrence::rMonthlyPos, freq, end);
	else
		return false;
	for (QPtrListIterator<Recurrence::rMonthPos> it(posns);  it.current();  ++it)
	{
		short weekno = it.current()->rPos;
		if (it.current()->negative)
			weekno = -weekno;
		recurrence.addMonthlyPos(weekno, it.current()->rDays);
	}
	return true;
}

/******************************************************************************
 * Set the recurrence to recur annually, on the specified start date in each
 * of the specified months.
 * Parameters:
 *    freq   = how many years between recurrences.
 *    months = which months of the year alarms should occur on.
 *    feb29  = if start date is March 1st, recur on February 29th; otherwise ignored
 *    count  = number of occurrences, including first and last.
 *           = -1 to recur indefinitely.
 *           = 0 to use 'end' instead.
 *    end    = end date (invalid to use 'count' instead).
 */
bool KAlarmEvent::setRecurAnnualByDate(Recurrence& recurrence, int freq, const QValueList<int>& months, int count, const QDate& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setYearly(Recurrence::rYearlyMonth, freq, count);
	else if (end.isValid())
		recurrence.setYearly(Recurrence::rYearlyMonth, freq, end);
	else
		return false;
	for (QValueListConstIterator<int> it = months.begin();  it != months.end();  ++it)
		recurrence.addYearlyNum(*it);
	return true;
}

bool KAlarmEvent::setRecurAnnualByDate(Recurrence& recurrence, int freq, const QPtrList<int>& months, int count, const QDate& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setYearly(Recurrence::rYearlyMonth, freq, count);
	else if (end.isValid())
		recurrence.setYearly(Recurrence::rYearlyMonth, freq, end);
	else
		return false;
	for (QPtrListIterator<int> it(months);  it.current();  ++it)
		recurrence.addYearlyNum(*it.current());
	return true;
}

/******************************************************************************
 * Set the recurrence to recur annually, on the specified weekdays in the
 * specified weeks of the specified month.
 * Parameters:
 *    freq   = how many years between recurrences.
 *    posns  = which days of the week/weeks of the month alarms should occur on.
 *    months = which months of the year alarms should occur on.
 *    count  = number of occurrences, including first and last.
 *           = -1 to recur indefinitely.
 *           = 0 to use 'end' instead.
 *    end    = end date (invalid to use 'count' instead).
 */
bool KAlarmEvent::setRecurAnnualByPos(Recurrence& recurrence, int freq, const QValueList<MonthPos>& posns, const QValueList<int>& months, int count, const QDate& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setYearly(Recurrence::rYearlyPos, freq, count);
	else if (end.isValid())
		recurrence.setYearly(Recurrence::rYearlyPos, freq, end);
	else
		return false;
	for (QValueListConstIterator<int> it = months.begin();  it != months.end();  ++it)
		recurrence.addYearlyNum(*it);
	for (QValueListConstIterator<MonthPos> it = posns.begin();  it != posns.end();  ++it)
		recurrence.addYearlyMonthPos((*it).weeknum, (*it).days);
	return true;
}

bool KAlarmEvent::setRecurAnnualByPos(Recurrence& recurrence, int freq, const QPtrList<Recurrence::rMonthPos>& posns, const QPtrList<int>& months, int count, const QDate& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setYearly(Recurrence::rYearlyPos, freq, count);
	else if (end.isValid())
		recurrence.setYearly(Recurrence::rYearlyPos, freq, end);
	else
		return false;
	for (QPtrListIterator<int> it(months);  it.current();  ++it)
		recurrence.addYearlyNum(*it.current());
	for (QPtrListIterator<Recurrence::rMonthPos> it(posns);  it.current();  ++it)
	{
		short weekno = it.current()->rPos;
		if (it.current()->negative)
		weekno = -weekno;
		recurrence.addYearlyMonthPos(weekno, it.current()->rDays);
	}
	return true;
}

/******************************************************************************
 * Set the recurrence to recur annually, on the specified day numbers.
 * Parameters:
 *    freq  = how many years between recurrences.
 *    days  = which days of the year alarms should occur on.
 *    count = number of occurrences, including first and last.
 *          = 0 to use 'end' instead.
 *    end   = end date (invalid to use 'count' instead).
 */
bool KAlarmEvent::setRecurAnnualByDay(Recurrence& recurrence, int freq, const QValueList<int>& days, int count, const QDate& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setYearly(Recurrence::rYearlyDay, freq, count);
	else if (end.isValid())
		recurrence.setYearly(Recurrence::rYearlyDay, freq, end);
	else
		return false;
	for (QValueListConstIterator<int> it = days.begin();  it != days.end();  ++it)
		recurrence.addYearlyNum(*it);
	return true;
}

bool KAlarmEvent::setRecurAnnualByDay(Recurrence& recurrence, int freq, const QPtrList<int>& days, int count, const QDate& end)
{
	if (count < -1)
		return false;
	if (count)
		recurrence.setYearly(Recurrence::rYearlyDay, freq, count);
	else if (end.isValid())
		recurrence.setYearly(Recurrence::rYearlyDay, freq, end);
	else
		return false;
	for (QPtrListIterator<int> it(days);  it.current();  ++it)
		recurrence.addYearlyNum(*it.current());
	return true;
}

/******************************************************************************
*  Initialise a KCal::Recurrence from recurrence parameters.
*  Reply = true if successful.
*/
bool KAlarmEvent::setRecurrence(Recurrence& recurrence, RecurType recurType, int repeatInterval,
                                int repeatCount, const QDateTime& endTime)
{
	switch (recurType)
	{
		case MINUTELY:
			setRecurMinutely(recurrence, repeatInterval, repeatCount, endTime);
			break;
		case DAILY:
			setRecurDaily(recurrence, repeatInterval, repeatCount, endTime.date());
			break;
		case WEEKLY:
		{
			QBitArray days(7);
			days.setBit(QDate::currentDate().dayOfWeek() - 1);
			setRecurWeekly(recurrence, repeatInterval, days, repeatCount, endTime.date());
			break;
		}
		case MONTHLY_DAY:
		{
			QValueList<int> days;
			days.append(QDate::currentDate().day());
			setRecurMonthlyByDate(recurrence, repeatInterval, days, repeatCount, endTime.date());
			break;
		}
		case ANNUAL_DATE:
		{
			QValueList<int> months;
			months.append(QDate::currentDate().month());
			setRecurAnnualByDate(recurrence, repeatInterval, months, repeatCount, endTime.date());
			break;
		}
		case NO_RECUR:
			recurrence.unsetRecurs();
			break;
		default:
			recurrence.unsetRecurs();
			return false;
	}
	return true;
}

/******************************************************************************
 * Initialise the event's recurrence and alarm repetition data, and set the
 * recurrence start date and repetition count if applicable.
 */
bool KAlarmEvent::initRecur(const QDate& endDate, int count, bool feb29)
{
	mExceptionDates.clear();
	mExceptionDateTimes.clear();
	mRecursFeb29 = false;
	mUpdated = true;
	if (endDate.isValid() || count > 0 || count == -1)
	{
		if (!mRecurrence)
			mRecurrence = new Recurrence(0);
		mRecurrence->setRecurStart(mDateTime.dateTime());
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
			const_cast<KAlarmEvent*>(this)->mStartDateTime.set(mRecurrence->recurStart(), mStartDateTime.isDateOnly());   // shouldn't be necessary, but just in case...
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
bool KAlarmEvent::adjustStartOfDay(const Event::List& events)
{
	bool changed = false;
	QTime startOfDay = theApp()->preferences()->startOfDay();
        for (Event::List::ConstIterator evit = events.begin();  evit != events.end();  ++evit)
	{
		Event* event = *evit;
		const QStringList& cats = event->categories();
		if (cats.find(DATE_ONLY_CATEGORY) != cats.end())
		{
			// It's an untimed event, so fix it
			QTime oldTime = event->dtStart().time();
			int adjustment = oldTime.secsTo(startOfDay);
			if (adjustment)
			{
				event->setDtStart(QDateTime(event->dtStart().date(), startOfDay));
				Alarm::List alarms = event->alarms();
				int deferralOffset = 0;
                                for (Alarm::List::ConstIterator alit = alarms.begin();  alit != alarms.end();  ++alit)
				{
					// Parse the next alarm's text
					Alarm& alarm = **alit;
					AlarmData data;
					readAlarm(alarm, data);
					if (data.type & KAlarmAlarm::TIMED_DEFERRAL_FLAG)
					{
						// Timed deferral alarm, so adjust the offset
						deferralOffset = alarm.startOffset().asSeconds();
						alarm.setStartOffset(deferralOffset - adjustment);
					}
					else if (data.type == KAlarmAlarm::AUDIO__ALARM
					&&       alarm.startOffset().asSeconds() == deferralOffset)
					{
						// Audio alarm is set for the same time as the deferral alarm
						alarm.setStartOffset(deferralOffset - adjustment);
					}
				}
				changed = true;
			}
		}
		else
		{
			// It's a timed event. Fix any untimed alarms.
			int deferralOffset = 0;
			int newDeferralOffset = 0;
			AlarmMap alarmMap;
			readAlarms(*event, &alarmMap);
			for (AlarmMap::Iterator it = alarmMap.begin();  it != alarmMap.end();  ++it)
			{
				const AlarmData& data = it.data();
				if ((data.type & KAlarmAlarm::DEFERRED_ALARM)
				&&  !(data.type & KAlarmAlarm::TIMED_DEFERRAL_FLAG))
				{
					// Date-only deferral alarm, so adjust its time
					QDateTime altime = data.alarm->time();
					altime.setTime(startOfDay);
					deferralOffset = data.alarm->startOffset().asSeconds();
					newDeferralOffset = event->dtStart().secsTo(altime);
					const_cast<Alarm*>(data.alarm)->setStartOffset(newDeferralOffset);
					changed = true;
				}
				else if (data.type == KAlarmAlarm::AUDIO__ALARM
				&&       data.alarm->startOffset().asSeconds() == deferralOffset)
				{
					// Audio alarm is set for the same time as the deferral alarm
					const_cast<Alarm*>(data.alarm)->setStartOffset(newDeferralOffset);
					changed = true;
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
	QTime startOfDay = theApp()->preferences()->startOfDay();

	Event::List events = calendar.events();
	for (Event::List::ConstIterator evit = events.begin();  evit != events.end();  ++evit)
	{
		Event* event = *evit;
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
			Alarm::List alarms = event->alarms();
			for (Alarm::List::ConstIterator alit = alarms.begin();  alit != alarms.end();  ++alit)
			{
				Alarm* alarm = *alit;
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
					types += TIME_DEFERRAL_TYPE;
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
			 * For the expired calendar, set the CREATED time to the DTEND value.
			 * Convert date-only DTSTART to date/time, and add category "DATE".
			 * Set the DTEND time to the DTSTART time.
			 * Convert all alarm times to DTSTART offsets.
			 * For display alarms, convert the first unlabelled category to an
			 * X-KDE-KALARM-FONTCOLOUR property.
			 * Convert BEEP category into an audio alarm with no audio file.
			 */
			QStringList cats = event->categories();

			if (calendar.type() == EXPIRED)
				event->setCreated(event->dtEnd());
			QDateTime start = event->dtStart();
			if (event->doesFloat())
			{
				event->setFloats(false);
				start.setTime(startOfDay);
				cats.append(DATE_ONLY_CATEGORY);
			}
			event->setHasEndDate(false);

			Alarm::List alarms = event->alarms();
			Alarm::List::ConstIterator alit;
                        for (alit = alarms.begin();  alit != alarms.end();  ++alit)
			{
				Alarm* alarm = *alit;
				QDateTime dt = alarm->time();
				alarm->setStartOffset(start.secsTo(dt));
			}

			if (cats.count() > 0)
			{
                                for (alit = alarms.begin();  alit != alarms.end();  ++alit)
				{
					Alarm* alarm = *alit;
					if (alarm->type() == Alarm::Display)
						alarm->setCustomProperty(APPNAME, FONT_COLOUR_PROPERTY,
						                         QString::fromLatin1("%1;;").arg(cats[0]));
				}
				cats.remove(cats.begin());
			}

			for (QStringList::Iterator it = cats.begin();  it != cats.end();  ++it)
			{
				if (*it == BEEP_CATEGORY)
				{
					cats.remove(it);

					Alarm* alarm = event->newAlarm();
					alarm->setEnabled(true);
					alarm->setAudioAlarm();
					QDateTime dt = event->dtStart();    // default

					// Parse and order the alarms to know which one's date/time to use
					AlarmMap alarmMap;
					readAlarms(*event, &alarmMap);
					AlarmMap::ConstIterator it = alarmMap.begin();
					if (it != alarmMap.end())
					{
						dt = it.data().alarm->time();
						break;
					}
					alarm->setStartOffset(start.secsTo(dt));
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
	kdDebug(5950) << "-- mSaveDateTime:" << mSaveDateTime.toString() << ":\n";
	if (mRepeatAtLogin)
		kdDebug(5950) << "-- mAtLoginDateTime:" << mAtLoginDateTime.toString() << ":\n";
	kdDebug(5950) << "-- mArchiveRepeatAtLogin:" << (mArchiveRepeatAtLogin ? "true" : "false") << ":\n";
	if (mReminderMinutes)
		kdDebug(5950) << "-- mReminderMinutes:" << mReminderMinutes << ":\n";
	if (mArchiveReminderMinutes)
		kdDebug(5950) << "-- mArchiveReminderMinutes:" << mArchiveReminderMinutes << ":\n";
	else if (mDeferral)
	{
		kdDebug(5950) << "-- mDeferralTime:" << mDeferralTime.toString() << ":\n";
		if (mReminderDeferral)
			kdDebug(5950) << "-- mReminderDeferral:" << (mReminderDeferral ? "true" : "false") << ":\n";
	}
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
	const char* altype = 0;
	switch (mType)
	{
		case MAIN__ALARM:                    altype = "MAIN";  break;
		case REMINDER__ALARM:                altype = "REMINDER";  break;
		case DEFERRED_DATE__ALARM:           altype = "DEFERRED(DATE)";  break;
		case DEFERRED_TIME__ALARM:           altype = "DEFERRED(TIME)";  break;
		case DEFERRED_REMINDER_DATE__ALARM:  altype = "DEFERRED_REMINDER(DATE)";  break;
		case DEFERRED_REMINDER_TIME__ALARM:  altype = "DEFERRED_REMINDER(TIME)";  break;
		case AT_LOGIN__ALARM:                altype = "LOGIN";  break;
		case DISPLAYING__ALARM:              altype = "DISPLAYING";  break;
		case AUDIO__ALARM:                   altype = "AUDIO";  break;
		default:                             altype = "INVALID";  break;
	}
	kdDebug(5950) << "-- mType:" << altype << ":\n";
	kdDebug(5950) << "-- mRecurs:" << (mRecurs ? "true" : "false") << ":\n";
	kdDebug(5950) << "KAlarmAlarm dump end\n";
}

const char* KAlarmAlarm::debugType(Type type)
{
	switch (type)
	{
		case MAIN_ALARM:               return "MAIN";
		case REMINDER_ALARM:           return "REMINDER";
		case DEFERRED_ALARM:           return "DEFERRED";
		case DEFERRED_REMINDER_ALARM:  return "DEFERRED_REMINDER";
		case AT_LOGIN_ALARM:           return "LOGIN";
		case DISPLAYING_ALARM:         return "DISPLAYING";
		case AUDIO_ALARM:              return "AUDIO";
		default:                       return "INVALID";
	}
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
	return mDefaultFont ? theApp()->preferences()->messageFont() : mFont;
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
				case '>':
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
