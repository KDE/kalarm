/*
 *  alarmevent.cpp  -  represents calendar alarms and events
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

#include "kalarm.h"

#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <qcolor.h>
#include <qregexp.h>

#include <klocale.h>
#include <kdebug.h>

#include "alarmtext.h"
#include "functions.h"
#include "kalarmapp.h"
#include "preferences.h"
#include "alarmcalendar.h"
#include "alarmevent.h"
using namespace KCal;


const QCString APPNAME("KALARM");

// Custom calendar properties
static const QCString TYPE_PROPERTY("TYPE");    // X-KDE-KALARM-TYPE property
static const QString FILE_TYPE                  = QString::fromLatin1("FILE");
static const QString AT_LOGIN_TYPE              = QString::fromLatin1("LOGIN");
static const QString REMINDER_TYPE              = QString::fromLatin1("REMINDER");
static const QString REMINDER_ONCE_TYPE         = QString::fromLatin1("REMINDER_ONCE");
static const QString ARCHIVE_REMINDER_ONCE_TYPE = QString::fromLatin1("ONCE");
static const QString TIME_DEFERRAL_TYPE         = QString::fromLatin1("DEFERRAL");
static const QString DATE_DEFERRAL_TYPE         = QString::fromLatin1("DATE_DEFERRAL");
static const QString DISPLAYING_TYPE            = QString::fromLatin1("DISPLAYING");   // used only in displaying calendar
static const QString PRE_ACTION_TYPE            = QString::fromLatin1("PRE");
static const QString POST_ACTION_TYPE           = QString::fromLatin1("POST");
static const QCString FONT_COLOUR_PROPERTY("FONTCOLOR");    // X-KDE-KALARM-FONTCOLOR property
static const QCString VOLUME_PROPERTY("VOLUME");            // X-KDE-KALARM-VOLUME property
static const QCString KMAIL_ID_PROPERTY("KMAILID");         // X-KDE-KALARM-KMAILID property

// Event categories
static const QString DATE_ONLY_CATEGORY      = QString::fromLatin1("DATE");
static const QString EMAIL_BCC_CATEGORY      = QString::fromLatin1("BCC");
static const QString CONFIRM_ACK_CATEGORY    = QString::fromLatin1("ACKCONF");
static const QString LATE_CANCEL_CATEGORY    = QString::fromLatin1("LATECANCEL;");
static const QString AUTO_CLOSE_CATEGORY     = QString::fromLatin1("LATECLOSE;");
static const QString TEMPL_DEF_TIME_CATEGORY = QString::fromLatin1("TMPLDEFTIME");
static const QString ARCHIVE_CATEGORY        = QString::fromLatin1("SAVE");
static const QString ARCHIVE_CATEGORIES      = QString::fromLatin1("SAVE:");

// Event status strings
static const QString DISABLED_STATUS         = QString::fromLatin1("DISABLED");

static const QString EXPIRED_UID    = QString::fromLatin1("-exp-");
static const QString DISPLAYING_UID = QString::fromLatin1("-disp-");
static const QString TEMPLATE_UID   = QString::fromLatin1("-tmpl-");

struct AlarmData
{
	const Alarm*           alarm;
	QString                cleanText;       // text or audio file name
	QString                emailFromKMail;
	EmailAddressList       emailAddresses;
	QString                emailSubject;
	QStringList            emailAttachments;
	QDateTime              dateTime;
	QFont                  font;
	QColor                 bgColour, fgColour;
	float                  soundVolume;
	KAAlarm::SubType       type;
	KAAlarmEventBase::Type action;
	int                    displayingFlags;
	bool                   defaultFont;
	bool                   reminderOnceOnly;
	int                    repeatCount;
	int                    repeatInterval;
};
typedef QMap<KAAlarm::SubType, AlarmData> AlarmMap;

static void setProcedureAlarm(Alarm*, const QString& commandLine);


/*=============================================================================
= Class KAEvent
= Corresponds to a KCal::Event instance.
=============================================================================*/

inline void KAEvent::set_deferral(DeferType type)
{
	if (type)
	{
		if (!mDeferral)
			++mAlarmCount;
	}
	else
	{
		if (mDeferral)
			--mAlarmCount;
	}
	mDeferral = type;
}

inline void KAEvent::set_reminder(int minutes)
{
	if (!mReminderMinutes)
		++mAlarmCount;
	mReminderMinutes        = minutes;
	mArchiveReminderMinutes = 0;
}

inline void KAEvent::set_archiveReminder()
{
	if (mReminderMinutes)
		--mAlarmCount;
	mArchiveReminderMinutes = mReminderMinutes;
	mReminderMinutes        = 0;
}


void KAEvent::copy(const KAEvent& event)
{
	KAAlarmEventBase::copy(event);
	mTemplateName            = event.mTemplateName;
	mAudioFile               = event.mAudioFile;
	mPreAction               = event.mPreAction;
	mPostAction              = event.mPostAction;
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
	mDeferral                = event.mDeferral;
	mRecursFeb29             = event.mRecursFeb29;
	mReminderOnceOnly        = event.mReminderOnceOnly;
	mMainExpired             = event.mMainExpired;
	mArchiveRepeatAtLogin    = event.mArchiveRepeatAtLogin;
	mArchive                 = event.mArchive;
	mTemplateDefaultTime     = event.mTemplateDefaultTime;
	mEnabled                 = event.mEnabled;
	mUpdated                 = event.mUpdated;
	delete mRecurrence;
	if (event.mRecurrence)
		mRecurrence = new Recurrence(*event.mRecurrence, 0);
	else
		mRecurrence = 0;
}

/******************************************************************************
 * Initialise the KAEvent from a KCal::Event.
 */
void KAEvent::set(const Event& event)
{
	// Extract status from the event
	mEventID                = event.uid();
	mRevision               = event.revision();
	mTemplateName           = QString::null;
	mTemplateDefaultTime    = false;
	mBeep                   = false;
	mEmailBcc               = false;
	mConfirmAck             = false;
	mArchive                = false;
	mReminderOnceOnly       = false;
	mAutoClose              = false;
	mArchiveRepeatAtLogin   = false;
	mArchiveReminderMinutes = 0;
	mLateCancel             = 0;
	mBgColour               = QColor(255, 255, 255);    // missing/invalid colour - return white background
	mFgColour               = QColor(0, 0, 0);          // and black foreground
	mDefaultFont            = true;
	mEnabled                = true;
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
		else if (cats[i] == TEMPL_DEF_TIME_CATEGORY)
			mTemplateDefaultTime = true;
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
				else if (list[j] == ARCHIVE_REMINDER_ONCE_TYPE)
					mReminderOnceOnly = true;
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
		else if (cats[i].startsWith(LATE_CANCEL_CATEGORY))
		{
			bool ok;
			mLateCancel = static_cast<int>(cats[i].mid(LATE_CANCEL_CATEGORY.length()).toUInt(&ok));
			if (!ok  ||  !mLateCancel)
				mLateCancel = 1;    // invalid parameter defaults to 1 minute
		}
		else if (cats[i].startsWith(AUTO_CLOSE_CATEGORY))
		{
			bool ok;
			mLateCancel = static_cast<int>(cats[i].mid(AUTO_CLOSE_CATEGORY.length()).toUInt(&ok));
			if (!ok  ||  !mLateCancel)
				mLateCancel = 1;    // invalid parameter defaults to 1 minute
			mAutoClose = true;
		}
	}
	mStartDateTime.set(event.dtStart(), floats);
	mNextMainDateTime = mStartDateTime;
	mSaveDateTime     = event.created();
	if (uidStatus() == TEMPLATE)
		mTemplateName = event.summary();
	if (event.statusStr() == DISABLED_STATUS)
		mEnabled = false;

	// Extract status from the event's alarms.
	// First set up defaults.
	mActionType       = T_MESSAGE;
	mMainExpired      = true;
	mRecursFeb29      = false;
	mRepeatAtLogin    = false;
	mDisplaying       = false;
	mRepeatSound      = false;
	mDeferral         = NO_DEFERRAL;
	mSoundVolume      = -1;
	mReminderMinutes  = 0;
	mRepeatInterval   = 0;
	mRepeatCount      = 0;
	mText             = "";
	mAudioFile        = "";
	mPreAction        = "";
	mPostAction       = "";
	mEmailFromKMail   = "";
	mEmailSubject     = "";
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
			case KAAlarm::MAIN__ALARM:
				mMainExpired = false;
				alTime.set(data.dateTime, mStartDateTime.isDateOnly());
				if (data.repeatCount  &&  data.repeatInterval)
				{
					mRepeatInterval = data.repeatInterval;   // values may be adjusted in setRecurrence()
					mRepeatCount    = data.repeatCount;
				}
				break;
			case KAAlarm::AT_LOGIN__ALARM:
				mRepeatAtLogin   = true;
				mAtLoginDateTime = data.dateTime;
				alTime = mAtLoginDateTime;
				break;
			case KAAlarm::REMINDER__ALARM:
				reminderTime.set(data.dateTime, mStartDateTime.isDateOnly());
				alTime = reminderTime;
				break;
			case KAAlarm::DEFERRED_REMINDER_DATE__ALARM:
			case KAAlarm::DEFERRED_DATE__ALARM:
				mDeferral = (data.type == KAAlarm::DEFERRED_REMINDER_DATE__ALARM) ? REMINDER_DEFERRAL : NORMAL_DEFERRAL;
				mDeferralTime.set(data.dateTime, false);
				break;
			case KAAlarm::DEFERRED_REMINDER_TIME__ALARM:
			case KAAlarm::DEFERRED_TIME__ALARM:
				mDeferral = (data.type == KAAlarm::DEFERRED_REMINDER_TIME__ALARM) ? REMINDER_DEFERRAL : NORMAL_DEFERRAL;
				mDeferralTime.set(data.dateTime);
				break;
			case KAAlarm::DISPLAYING__ALARM:
			{
				mDisplaying      = true;
				mDisplayingFlags = data.displayingFlags;
				bool dateOnly = (mDisplayingFlags & DEFERRAL) ? !(mDisplayingFlags & TIMED_FLAG)
				              : mStartDateTime.isDateOnly();
				mDisplayingTime.set(data.dateTime, dateOnly);
				alTime = mDisplayingTime;
				break;
			}
			case KAAlarm::AUDIO__ALARM:
				mAudioFile   = data.cleanText;
				mBeep        = mAudioFile.isEmpty();
				mSoundVolume = !mBeep ? data.soundVolume : -1;
				mRepeatSound = !mBeep  &&  (data.repeatCount < 0);
				break;
			case KAAlarm::PRE_ACTION__ALARM:
				mPreAction = data.cleanText;
				break;
			case KAAlarm::POST_ACTION__ALARM:
				mPostAction = data.cleanText;
				break;
			case KAAlarm::INVALID__ALARM:
			default:
				break;
		}

		if (data.reminderOnceOnly)
			mReminderOnceOnly = true;
		switch (data.type)
		{
			case KAAlarm::DEFERRED_REMINDER_DATE__ALARM:
			case KAAlarm::DEFERRED_DATE__ALARM:
			case KAAlarm::DEFERRED_REMINDER_TIME__ALARM:
			case KAAlarm::DEFERRED_TIME__ALARM:
				alTime = mDeferralTime;
				if (mNextMainDateTime == mDeferralTime)
					mDeferral = CANCEL_DEFERRAL;     // it's a cancelled deferral
				// fall through to MAIN__ALARM
			case KAAlarm::MAIN__ALARM:
			case KAAlarm::AT_LOGIN__ALARM:
			case KAAlarm::REMINDER__ALARM:
			case KAAlarm::DISPLAYING__ALARM:
				// Ensure that the basic fields are set up even if there is no main
				// alarm in the event (if it has expired and then been deferred)
				if (!set)
				{
					mNextMainDateTime = alTime;
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
							mFgColour    = data.fgColour;
							break;
						case T_EMAIL:
							mEmailFromKMail   = data.emailFromKMail;
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
				break;
			case KAAlarm::AUDIO__ALARM:
			case KAAlarm::PRE_ACTION__ALARM:
			case KAAlarm::POST_ACTION__ALARM:
			case KAAlarm::INVALID__ALARM:
			default:
				break;
		}
	}
	if (reminderTime.isValid())
	{
		mReminderMinutes = reminderTime.secsTo(mNextMainDateTime) / 60;
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
 * Reply = map of alarm data, indexed by KAAlarm::Type
 */
void KAEvent::readAlarms(const Event& event, void* almap)
{
	AlarmMap* alarmMap = (AlarmMap*)almap;
	Alarm::List alarms = event.alarms();
	for (Alarm::List::ConstIterator it = alarms.begin();  it != alarms.end();  ++it)
	{
		// Parse the next alarm's text
		AlarmData data;
		readAlarm(**it, data);
		if (data.type != KAAlarm::INVALID__ALARM)
			alarmMap->insert(data.type, data);
	}
}

/******************************************************************************
 * Parse a KCal::Alarm.
 * Reply = alarm ID (sequence number)
 */
void KAEvent::readAlarm(const Alarm& alarm, AlarmData& data)
{
	// Parse the next alarm's text
	data.alarm           = &alarm;
	data.dateTime        = alarm.time();
	data.displayingFlags = 0;
	data.repeatCount     = alarm.repeatCount();
	data.repeatInterval  = alarm.snoozeTime();
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
			data.emailFromKMail   = alarm.customProperty(APPNAME, KMAIL_ID_PROPERTY);
			data.emailAddresses   = alarm.mailAddresses();
			data.emailSubject     = alarm.mailSubject();
			data.emailAttachments = alarm.mailAttachments();
			data.cleanText        = alarm.mailText();
			break;
		case Alarm::Display:
		{
			data.action    = T_MESSAGE;
			data.cleanText = AlarmText::fromCalendarText(alarm.text());
			QString property = alarm.customProperty(APPNAME, FONT_COLOUR_PROPERTY);
			QStringList list = QStringList::split(QChar(';'), property, true);
			data.bgColour = QColor(255, 255, 255);   // white
			data.fgColour = QColor(0, 0, 0);         // black
			int n = list.count();
			if (n > 0)
			{
				if (!list[0].isEmpty())
				{
					QColor c(list[0]);
					if (c.isValid())
						data.bgColour = c;
				}
				if (n > 1  &&  !list[1].isEmpty())
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
		{
			data.action    = T_AUDIO;
			data.cleanText = alarm.audioFile();
			data.type      = KAAlarm::AUDIO__ALARM;
			bool ok = false;
			QString property = alarm.customProperty(APPNAME, VOLUME_PROPERTY);
			if (!property.isEmpty())
				data.soundVolume = property.toFloat(&ok);
			if (!ok)
				data.soundVolume = -1;
			return;
		}
		case Alarm::Invalid:
			data.type = KAAlarm::INVALID__ALARM;
			return;
	}

	bool atLogin          = false;
	bool reminder         = false;
	bool deferral         = false;
	bool dateDeferral     = false;
	data.reminderOnceOnly = false;
	data.type = KAAlarm::MAIN__ALARM;
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
		else if (type == REMINDER_ONCE_TYPE)
			reminder = data.reminderOnceOnly = true;
		else if (type == TIME_DEFERRAL_TYPE)
			deferral = true;
		else if (type == DATE_DEFERRAL_TYPE)
			dateDeferral = deferral = true;
		else if (type == DISPLAYING_TYPE)
			data.type = KAAlarm::DISPLAYING__ALARM;
		else if (type == PRE_ACTION_TYPE  &&  data.action == T_COMMAND)
			data.type = KAAlarm::PRE_ACTION__ALARM;
		else if (type == POST_ACTION_TYPE  &&  data.action == T_COMMAND)
			data.type = KAAlarm::POST_ACTION__ALARM;
	}

	if (reminder)
	{
		if (data.type == KAAlarm::MAIN__ALARM)
			data.type = dateDeferral ? KAAlarm::DEFERRED_REMINDER_DATE__ALARM
			          : deferral ? KAAlarm::DEFERRED_REMINDER_TIME__ALARM : KAAlarm::REMINDER__ALARM;
		else if (data.type == KAAlarm::DISPLAYING__ALARM)
			data.displayingFlags = dateDeferral ? REMINDER | DATE_DEFERRAL
			                     : deferral ? REMINDER | TIME_DEFERRAL : REMINDER;
	}
	else if (deferral)
	{
		if (data.type == KAAlarm::MAIN__ALARM)
			data.type = dateDeferral ? KAAlarm::DEFERRED_DATE__ALARM : KAAlarm::DEFERRED_TIME__ALARM;
		else if (data.type == KAAlarm::DISPLAYING__ALARM)
			data.displayingFlags = dateDeferral ? DATE_DEFERRAL : TIME_DEFERRAL;
	}
	if (atLogin)
	{
		if (data.type == KAAlarm::MAIN__ALARM)
			data.type = KAAlarm::AT_LOGIN__ALARM;
		else if (data.type == KAAlarm::DISPLAYING__ALARM)
			data.displayingFlags = REPEAT_AT_LOGIN;
	}
//kdDebug(5950)<<"ReadAlarm(): text="<<alarm.text()<<", time="<<alarm.time().toString()<<", valid time="<<alarm.time().isValid()<<endl;
}

/******************************************************************************
 * Initialise the KAEvent with the specified parameters.
 */
void KAEvent::set(const QDateTime& dateTime, const QString& text, const QColor& bg, const QColor& fg,
                  const QFont& font, Action action, int lateCancel, int flags)
{
	initRecur();
	mStartDateTime.set(dateTime, flags & ANY_TIME);
	mNextMainDateTime = mStartDateTime;
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
	mEventID                = QString::null;
	mTemplateName           = QString::null;
	mPreAction              = QString::null;
	mPostAction             = QString::null;
	mAudioFile              = "";
	mSoundVolume            = -1;
	mBgColour               = bg;
	mFgColour               = fg;
	mFont                   = font;
	mAlarmCount             = 1;
	mLateCancel             = lateCancel;
	mDeferral               = NO_DEFERRAL;    // do this before set(flags)
	set(flags);
	mReminderMinutes        = 0;
	mArchiveReminderMinutes = 0;
	mRepeatInterval         = 0;
	mRepeatCount            = 0;
	mArchiveRepeatAtLogin   = false;
	mReminderOnceOnly       = false;
	mDisplaying             = false;
	mMainExpired            = false;
	mArchive                = false;
	mTemplateDefaultTime    = false;
	mUpdated                = false;
}

/******************************************************************************
 * Initialise an email KAEvent.
 */
void KAEvent::setEmail(const QDate& d, const QString& from, const EmailAddressList& addresses, const QString& subject,
			    const QString& message, const QStringList& attachments, int lateCancel, int flags)
{
	set(d, message, QColor(), QColor(), QFont(), EMAIL, lateCancel, flags | ANY_TIME);
	mEmailFromKMail   = from;
	mEmailAddresses   = addresses;
	mEmailSubject     = subject;
	mEmailAttachments = attachments;
}

void KAEvent::setEmail(const QDateTime& dt, const QString& from, const EmailAddressList& addresses, const QString& subject,
			    const QString& message, const QStringList& attachments, int lateCancel, int flags)
{
	set(dt, message, QColor(), QColor(), QFont(), EMAIL, lateCancel, flags);
	mEmailFromKMail   = from;
	mEmailAddresses   = addresses;
	mEmailSubject     = subject;
	mEmailAttachments = attachments;
}

void KAEvent::setEmail(const QString& from, const EmailAddressList& addresses, const QString& subject, const QStringList& attachments)
{
	mEmailFromKMail   = from;
	mEmailAddresses   = addresses;
	mEmailSubject     = subject;
	mEmailAttachments = attachments;
}

void KAEvent::setReminder(int minutes, bool onceOnly)
{
	set_reminder(minutes);
	mReminderOnceOnly = onceOnly;
	mUpdated          = true;
}

/******************************************************************************
 * Reinitialise the start date/time byt adjusting its date part, and setting
 * the next scheduled alarm to the new start date/time.
 */
void KAEvent::adjustStartDate(const QDate& d)
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
	mNextMainDateTime = mStartDateTime;
}

/******************************************************************************
 * Return the time of the next scheduled occurrence of the event.
 * Reminders and deferred reminders can optionally be ignored.
 */
DateTime KAEvent::nextDateTime(bool includeReminders) const
{
	if (includeReminders  &&  mReminderMinutes)
	{
		if (!mReminderOnceOnly  ||  mNextMainDateTime == mStartDateTime)
			return mNextMainDateTime.addSecs(-mReminderMinutes * 60);
	}
	DateTime dt = mNextMainDateTime;
	if (mRepeatCount)
	{
		// N.B. This is coded to avoid 32-bit integer overflow which occurs
		//      in QDateTime::secsTo() for large enough time differences.
		QDateTime now = QDateTime::currentDateTime();
		if (now > mNextMainDateTime)
		{
			dt = mainEndRepeatTime();    // get the last repetition time
			if (dt > now)
			{
				int repeatSecs = mRepeatInterval * 60;
				int repetition = (mNextMainDateTime.secsTo(now) + repeatSecs - 1) / repeatSecs;
				dt = mNextMainDateTime.addSecs(repetition * repeatSecs);
			}
		}
	}
	if (mDeferral > 0
	&&  (includeReminders  ||  mDeferral != REMINDER_DEFERRAL))
	{
		if (mMainExpired)
			return mDeferralTime;
		return QMIN(mDeferralTime, dt);
	}
	return dt;
}

/******************************************************************************
 * Convert a unique ID to indicate that the event is in a specified calendar file.
 */
QString KAEvent::uid(const QString& id, Status status)
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
	else if ((i = result.find(TEMPLATE_UID)) > 0)
	{
		oldStatus = TEMPLATE;
		len = TEMPLATE_UID.length();
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
			case TEMPLATE:    part = TEMPLATE_UID;  break;
		}
		result.replace(i, len, part);
	}
	return result;
}

/******************************************************************************
 * Get the calendar type for a unique ID.
 */
KAEvent::Status KAEvent::uidStatus(const QString& uid)
{
	if (uid.find(EXPIRED_UID) > 0)
		return EXPIRED;
	if (uid.find(DISPLAYING_UID) > 0)
		return DISPLAYING;
	if (uid.find(TEMPLATE_UID) > 0)
		return TEMPLATE;
	return ACTIVE;
}

void KAEvent::set(int flags)
{
	KAAlarmEventBase::set(flags & ~READ_ONLY_FLAGS);
	mStartDateTime.setDateOnly(flags & ANY_TIME);
	set_deferral((flags & KAEvent::DEFERRAL) ? NORMAL_DEFERRAL : NO_DEFERRAL);
	mEnabled  = !(flags & DISABLED);
	mUpdated = true;
}

int KAEvent::flags() const
{
	return KAAlarmEventBase::flags()
	     | (mStartDateTime.isDateOnly() ? ANY_TIME : 0)
	     | (mDeferral > 0               ? KAEvent::DEFERRAL : 0)
	     | (mEnabled                    ? 0 : DISABLED);
}

/******************************************************************************
 * Create a new Event from the KAEvent data.
 */
Event* KAEvent::event() const
{
	Event* ev = new KCal::Event;
	updateKCalEvent(*ev);
	return ev;
}

/******************************************************************************
 * Update an existing KCal::Event with the KAEvent data.
 * If 'original' is true, the event start date/time is adjusted to its original
 * value instead of its next occurrence, and the expired main alarm is
 * reinstated.
 */
bool KAEvent::updateKCalEvent(Event& ev, bool checkUid, bool original, bool cancelCancelledDefer) const
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
		cats.append(QString("%1%2").arg(mAutoClose ? AUTO_CLOSE_CATEGORY : LATE_CANCEL_CATEGORY).arg(mLateCancel));
	if (!mTemplateName.isEmpty()  &&  mTemplateDefaultTime)
		cats.append(TEMPL_DEF_TIME_CATEGORY);
	if (mArchive  &&  !original)
	{
		QStringList params;
		if (mArchiveReminderMinutes)
		{
			if (mReminderOnceOnly)
				params += ARCHIVE_REMINDER_ONCE_TYPE;
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
	ev.setCustomStatus(mEnabled ? QString::null : DISABLED_STATUS);
	ev.setRevision(mRevision);
	ev.clearAlarms();

	// Always set DTSTART as date/time, since alarm times can only be specified
	// in local time (instead of UTC) if they are relative to a DTSTART or DTEND
	// which is also specified in local time. Instead of calling setFloats() to
	// indicate a date-only event, the category "DATE" is included.
	ev.setDtStart(mStartDateTime.dateTime());
	ev.setFloats(false);
	ev.setHasEndDate(false);

	DateTime dtMain = original ? mStartDateTime : mNextMainDateTime;
	DateTime ancillaryTime;       // time for ancillary alarms (audio, pre-action, etc)
	if (!mMainExpired  ||  original)
	{
		// Add the main alarm
		initKcalAlarm(ev, dtMain, QStringList(), KAAlarm::MAIN_ALARM);
		ancillaryTime = dtMain;
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
		if (!ancillaryTime.isValid())
			ancillaryTime = dtl;
	}
	if (mReminderMinutes  ||  mArchiveReminderMinutes && original)
	{
		int minutes = mReminderMinutes ? mReminderMinutes : mArchiveReminderMinutes;
		DateTime reminderTime = dtMain.addSecs(-minutes * 60);
		initKcalAlarm(ev, reminderTime, QStringList(mReminderOnceOnly ? REMINDER_ONCE_TYPE : REMINDER_TYPE));
		if (!ancillaryTime.isValid())
			ancillaryTime = reminderTime;
	}
	if (mDeferral > 0  ||  mDeferral == CANCEL_DEFERRAL && !cancelCancelledDefer)
	{
		QStringList list;
		if (mDeferralTime.isDateOnly())
			list += DATE_DEFERRAL_TYPE;
		else
			list += TIME_DEFERRAL_TYPE;
		if (mDeferral == REMINDER_DEFERRAL)
			list += mReminderOnceOnly ? REMINDER_ONCE_TYPE : REMINDER_TYPE;
		initKcalAlarm(ev, mDeferralTime, list);
		if (!ancillaryTime.isValid())
			ancillaryTime = mDeferralTime;
	}
	if (!mTemplateName.isEmpty())
		ev.setSummary(mTemplateName);
	else if (mDisplaying)
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
			list += mReminderOnceOnly ? REMINDER_ONCE_TYPE : REMINDER_TYPE;
		initKcalAlarm(ev, mDisplayingTime, list);
		if (!ancillaryTime.isValid())
			ancillaryTime = mDisplayingTime;
	}
	if (mBeep  ||  !mAudioFile.isEmpty())
	{
		// A sound is specified
		initKcalAlarm(ev, ancillaryTime, QStringList(), KAAlarm::AUDIO_ALARM);
	}
	if (!mPreAction.isEmpty())
	{
		// A pre-display action is specified
		initKcalAlarm(ev, ancillaryTime, QStringList(PRE_ACTION_TYPE), KAAlarm::PRE_ACTION_ALARM);
	}
	if (!mPostAction.isEmpty())
	{
		// A post-display action is specified
		initKcalAlarm(ev, ancillaryTime, QStringList(POST_ACTION_TYPE), KAAlarm::POST_ACTION_ALARM);
	}

	// Add recurrence data
	ev.setExDates(DateList());
	ev.setExDateTimes(DateTimeList());
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
				setRecurAnnualByDate(*recur, frequency, mRecurrence->yearNums(),
				                     (mRecurrence->monthDays().count() ? *mRecurrence->monthDays().getFirst() : 0),
				                     duration, endDateTime.date());
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
		ev.setExDateTimes(mExceptionDateTimes);
	}

	if (mSaveDateTime.isValid())
		ev.setCreated(mSaveDateTime);
	ev.setReadOnly(readOnly);
	return true;
}

/******************************************************************************
 * Create a new alarm for a libkcal event, and initialise it according to the
 * alarm action. If 'types' is non-null, it is appended to the X-KDE-KALARM-TYPE
 * property value list.
 */
 Alarm* KAEvent::initKcalAlarm(Event& event, const DateTime& dt, const QStringList& types, KAAlarm::Type type) const
{
	QStringList alltypes;
	Alarm* alarm = event.newAlarm();
	alarm->setEnabled(true);
	// RFC2445 specifies that absolute alarm times must be stored as UTC.
	// So, in order to store local times, set the alarm time as an offset to DTSTART.
	alarm->setStartOffset(dt.isDateOnly() ? mStartDateTime.secsTo(dt)
	                                      : mStartDateTime.dateTime().secsTo(dt.dateTime()));

	switch (type)
	{
		case KAAlarm::AUDIO_ALARM:
			alarm->setAudioAlarm(mAudioFile);  // empty for a beep
			if (mRepeatSound)
			{
				alarm->setRepeatCount(-1);
				alarm->setSnoozeTime(0);
			}
			if (!mAudioFile.isEmpty()  &&  mSoundVolume >= 0)
				alarm->setCustomProperty(APPNAME, VOLUME_PROPERTY, QString::number(mSoundVolume, 'f', 2));
			break;
		case KAAlarm::PRE_ACTION_ALARM:
			setProcedureAlarm(alarm, mPreAction);
			break;
		case KAAlarm::POST_ACTION_ALARM:
			setProcedureAlarm(alarm, mPostAction);
			break;
		case KAAlarm::MAIN_ALARM:
			alarm->setSnoozeTime(mRepeatInterval);
			alarm->setRepeatCount(mRepeatCount);
			// fall through to INVALID_ALARM
		case KAAlarm::INVALID_ALARM:
			switch (mActionType)
			{
				case T_FILE:
					alltypes += FILE_TYPE;
					// fall through to T_MESSAGE
				case T_MESSAGE:
					alarm->setDisplayAlarm(AlarmText::toCalendarText(mText));
					alarm->setCustomProperty(APPNAME, FONT_COLOUR_PROPERTY,
						      QString::fromLatin1("%1;%2;%3").arg(mBgColour.name())
										     .arg(mFgColour.name())
										     .arg(mDefaultFont ? QString::null : mFont.toString()));
					break;
				case T_COMMAND:
					setProcedureAlarm(alarm, mText);
					break;
				case T_EMAIL:
					alarm->setEmailAlarm(mEmailSubject, mText, mEmailAddresses, mEmailAttachments);
					if (!mEmailFromKMail.isEmpty())
						alarm->setCustomProperty(APPNAME, KMAIL_ID_PROPERTY, mEmailFromKMail);
					break;
				case T_AUDIO:
					break;
			}
			break;
		case KAAlarm::REMINDER_ALARM:
		case KAAlarm::DEFERRED_ALARM:
		case KAAlarm::DEFERRED_REMINDER_ALARM:
		case KAAlarm::AT_LOGIN_ALARM:
		case KAAlarm::DISPLAYING_ALARM:
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
KAAlarm KAEvent::alarm(KAAlarm::Type type) const
{
	checkRecur();     // ensure recurrence/repetition data is consistent
	KAAlarm al;       // this sets type to INVALID_ALARM
	if (mAlarmCount)
	{
		al.mEventID       = mEventID;
		al.mActionType    = mActionType;
		al.mText          = mText;
		al.mBgColour      = mBgColour;
		al.mFgColour      = mFgColour;
		al.mFont          = mFont;
		al.mDefaultFont   = mDefaultFont;
		al.mBeep          = mBeep;
		al.mSoundVolume   = mSoundVolume;
		al.mRepeatSound   = mRepeatSound;
		al.mConfirmAck    = mConfirmAck;
		al.mRepeatAtLogin = false;
		al.mDeferred      = false;
		al.mLateCancel    = mLateCancel;
		al.mAutoClose     = mAutoClose;
		al.mEmailBcc      = mEmailBcc;
		if (mActionType == T_EMAIL)
		{
			al.mEmailFromKMail   = mEmailFromKMail;
			al.mEmailAddresses   = mEmailAddresses;
			al.mEmailSubject     = mEmailSubject;
			al.mEmailAttachments = mEmailAttachments;
		}
		switch (type)
		{
			case KAAlarm::MAIN_ALARM:
				if (!mMainExpired)
				{
					al.mType             = KAAlarm::MAIN__ALARM;
					al.mNextMainDateTime = mNextMainDateTime;
					al.mRepeatCount      = mRepeatCount;
					al.mRepeatInterval   = mRepeatInterval;
				}
				break;
			case KAAlarm::REMINDER_ALARM:
				if (mReminderMinutes)
				{
					al.mType = KAAlarm::REMINDER__ALARM;
					if (mReminderOnceOnly)
						al.mNextMainDateTime = mStartDateTime.addMins(-mReminderMinutes);
					else
						al.mNextMainDateTime = mNextMainDateTime.addMins(-mReminderMinutes);
				}
				break;
			case KAAlarm::DEFERRED_REMINDER_ALARM:
				if (mDeferral != REMINDER_DEFERRAL)
					break;
				// fall through to DEFERRED_ALARM
			case KAAlarm::DEFERRED_ALARM:
				if (mDeferral > 0)
				{
					al.mType = static_cast<KAAlarm::SubType>((mDeferral == REMINDER_DEFERRAL ? KAAlarm::DEFERRED_REMINDER_ALARM : KAAlarm::DEFERRED_ALARM)
					                                         | (mDeferralTime.isDateOnly() ? 0 : KAAlarm::TIMED_DEFERRAL_FLAG));
					al.mNextMainDateTime = mDeferralTime;
					al.mDeferred         = true;
				}
				break;
			case KAAlarm::AT_LOGIN_ALARM:
				if (mRepeatAtLogin)
				{
					al.mType             = KAAlarm::AT_LOGIN__ALARM;
					al.mNextMainDateTime = mAtLoginDateTime;
					al.mRepeatAtLogin    = true;
					al.mLateCancel       = 0;
					al.mAutoClose        = false;
				}
				break;
			case KAAlarm::DISPLAYING_ALARM:
				if (mDisplaying)
				{
					al.mType             = KAAlarm::DISPLAYING__ALARM;
					al.mNextMainDateTime = mDisplayingTime;
					al.mDisplaying       = true;
				}
				break;
			case KAAlarm::AUDIO_ALARM:
			case KAAlarm::PRE_ACTION_ALARM:
			case KAAlarm::POST_ACTION_ALARM:
			case KAAlarm::INVALID_ALARM:
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
KAAlarm KAEvent::firstAlarm() const
{
	if (mAlarmCount)
	{
		if (!mMainExpired)
			return alarm(KAAlarm::MAIN_ALARM);
		return nextAlarm(KAAlarm::MAIN_ALARM);
	}
	return KAAlarm();
}

/******************************************************************************
 * Return the next alarm for the event, after the specified alarm.
 * N.B. a repeat-at-login alarm can only be returned if it has been read from/
 * written to the calendar file.
 */
KAAlarm KAEvent::nextAlarm(KAAlarm::Type prevType) const
{
	switch (prevType)
	{
		case KAAlarm::MAIN_ALARM:
			if (mReminderMinutes)
				return alarm(KAAlarm::REMINDER_ALARM);
			// fall through to REMINDER_ALARM
		case KAAlarm::REMINDER_ALARM:
			// There can only be one deferral alarm
			if (mDeferral == REMINDER_DEFERRAL)
				return alarm(KAAlarm::DEFERRED_REMINDER_ALARM);
			if (mDeferral == NORMAL_DEFERRAL)
				return alarm(KAAlarm::DEFERRED_ALARM);
			// fall through to DEFERRED_ALARM
		case KAAlarm::DEFERRED_REMINDER_ALARM:
		case KAAlarm::DEFERRED_ALARM:
			if (mRepeatAtLogin)
				return alarm(KAAlarm::AT_LOGIN_ALARM);
			// fall through to AT_LOGIN_ALARM
		case KAAlarm::AT_LOGIN_ALARM:
			if (mDisplaying)
				return alarm(KAAlarm::DISPLAYING_ALARM);
			// fall through to DISPLAYING_ALARM
		case KAAlarm::DISPLAYING_ALARM:
			// fall through to default
		case KAAlarm::AUDIO_ALARM:
		case KAAlarm::PRE_ACTION_ALARM:
		case KAAlarm::POST_ACTION_ALARM:
		case KAAlarm::INVALID_ALARM:
		default:
			break;
	}
	return KAAlarm();
}

/******************************************************************************
 * Remove the alarm of the specified type from the event.
 * This must only be called to remove an alarm which has expired, not to
 * reconfigure the event.
 */
void KAEvent::removeExpiredAlarm(KAAlarm::Type type)
{
	int count = mAlarmCount;
	switch (type)
	{
		case KAAlarm::MAIN_ALARM:
			mAlarmCount = 0;    // removing main alarm - also remove subsidiary alarms
			break;
		case KAAlarm::AT_LOGIN_ALARM:
			if (mRepeatAtLogin)
			{
				// Remove the at-login alarm, but keep a note of it for archiving purposes
				mArchiveRepeatAtLogin = true;
				mRepeatAtLogin = false;
				--mAlarmCount;
			}
			break;
		case KAAlarm::REMINDER_ALARM:
			// Remove any reminder alarm, but keep a note of it for archiving purposes
			set_archiveReminder();
			break;
		case KAAlarm::DEFERRED_REMINDER_ALARM:
		case KAAlarm::DEFERRED_ALARM:
			set_deferral(NO_DEFERRAL);
			break;
		case KAAlarm::DISPLAYING_ALARM:
			if (mDisplaying)
			{
				mDisplaying = false;
				--mAlarmCount;
			}
			break;
		case KAAlarm::AUDIO_ALARM:
		case KAAlarm::PRE_ACTION_ALARM:
		case KAAlarm::POST_ACTION_ALARM:
		case KAAlarm::INVALID_ALARM:
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
 * Reply = true if a repetition has been deferred.
 */
bool KAEvent::defer(const DateTime& dateTime, bool reminder, bool adjustRecurrence)
{
	bool result = false;
	cancelCancelledDeferral();
	if (checkRecur() == NO_RECUR)
	{
		if (mReminderMinutes  ||  mDeferral == REMINDER_DEFERRAL  ||  mArchiveReminderMinutes)
		{
			if (dateTime < mNextMainDateTime.dateTime())
			{
				set_deferral(REMINDER_DEFERRAL);   // defer reminder alarm
				mDeferralTime = dateTime;
			}
			else
			{
				// Deferring past the main alarm time, so adjust any existing deferral
				if (mReminderMinutes  ||  mDeferral == REMINDER_DEFERRAL)
					set_deferral(NO_DEFERRAL);
			}
			// Remove any reminder alarm, but keep a note of it for archiving purposes
			set_archiveReminder();
		}
		if (mDeferral != REMINDER_DEFERRAL)
		{
			// We're deferring the main alarm, not a reminder
			if (mRepeatCount  &&  dateTime < mainEndRepeatTime())
			{
				// The alarm is repeated, and we're deferring to a time before the last repetition
				set_deferral(NORMAL_DEFERRAL);
				mDeferralTime = dateTime;
				result = true;
			}
			else
			{
				// Main alarm has now expired
				mNextMainDateTime = mDeferralTime = dateTime;
				set_deferral(NORMAL_DEFERRAL);
				if (!mMainExpired)
				{
					// Mark the alarm as expired now
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
	}
	else if (reminder)
	{
		// Deferring a reminder for a recurring alarm
		if (dateTime >= mNextMainDateTime.dateTime())
			set_deferral(NO_DEFERRAL);    // (error)
		else
		{
			set_deferral(REMINDER_DEFERRAL);
			mDeferralTime = dateTime;
		}
	}
	else
	{
		mDeferralTime = dateTime;
		if (mDeferral <= 0)
			set_deferral(NORMAL_DEFERRAL);
		if (adjustRecurrence)
		{
			QDateTime now = QDateTime::currentDateTime();
			if (mainEndRepeatTime() < now)
			{
				// The last repetition (if any) of the current recurrence has already passed.
				// Adjust to the next scheduled recurrence after now.
				if (!mMainExpired  &&  setNextOccurrence(now, true) == NO_OCCURRENCE)
				{
					mMainExpired = true;
					--mAlarmCount;
				}
			}
		}
	}
	mUpdated = true;
	return result;
}

/******************************************************************************
 * Cancel any deferral alarm.
 */
void KAEvent::cancelDefer()
{
	if (mDeferral > 0)
	{
		// Set the deferral time to be the same as the next recurrence/repetition.
		// This prevents an immediate retriggering of the alarm.
		if (mMainExpired
		||  nextOccurrence(QDateTime::currentDateTime(), mDeferralTime, RETURN_REPETITION) == NO_OCCURRENCE)
		{
			// The main alarm has expired, so simply delete the deferral
			mDeferralTime = DateTime();
			set_deferral(NO_DEFERRAL);
		}
		else
			set_deferral(CANCEL_DEFERRAL);
		mUpdated = true;
	}
}

/******************************************************************************
 * Cancel any cancelled deferral alarm.
 */
void KAEvent::cancelCancelledDeferral()
{
	if (mDeferral == CANCEL_DEFERRAL)
	{
		mDeferralTime = DateTime();
		set_deferral(NO_DEFERRAL);
	}
}

/******************************************************************************
*  Find the latest time which the alarm can currently be deferred to.
*/
DateTime KAEvent::deferralLimit(KAEvent::DeferLimitType* limitType) const
{
	DeferLimitType ltype;
	DateTime endTime;
	bool recurs = (checkRecur() != NO_RECUR);
	if (recurs  ||  mRepeatCount)
	{
		// It's a repeated alarm. Don't allow it to be deferred past its
		// next occurrence or repetition.
		DateTime reminderTime;
		QDateTime now = QDateTime::currentDateTime();
		OccurType type = nextOccurrence(now, endTime, RETURN_REPETITION);
		if (type & OCCURRENCE_REPEAT)
			ltype = LIMIT_REPETITION;
		else if (type == NO_OCCURRENCE)
			ltype = LIMIT_NONE;
		else if (mReminderMinutes  &&  (now < (reminderTime = endTime.addMins(-mReminderMinutes))))
		{
			endTime = reminderTime;
			ltype = LIMIT_REMINDER;
		}
		else if (type == FIRST_OCCURRENCE  &&  !recurs)
			ltype = LIMIT_REPETITION;
		else
			ltype = LIMIT_RECURRENCE;
	}
	else if ((mReminderMinutes  ||  mDeferral == REMINDER_DEFERRAL  ||  mArchiveReminderMinutes)
		 &&  QDateTime::currentDateTime() < mNextMainDateTime.dateTime())
	{
		// It's an reminder alarm. Don't allow it to be deferred past its main alarm time.
		endTime = mNextMainDateTime;
		ltype = LIMIT_REMINDER;
	}
	else
		ltype = LIMIT_NONE;
	if (ltype != LIMIT_NONE)
		endTime = endTime.addMins(-1);
	if (limitType)
		*limitType = ltype;
	return endTime;
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
bool KAEvent::setDisplaying(const KAEvent& event, KAAlarm::Type alarmType, const QDateTime& repeatAtLoginTime)
{
	if (!mDisplaying
	&&  (alarmType == KAAlarm::MAIN_ALARM
	  || alarmType == KAAlarm::REMINDER_ALARM
	  || alarmType == KAAlarm::DEFERRED_REMINDER_ALARM
	  || alarmType == KAAlarm::DEFERRED_ALARM
	  || alarmType == KAAlarm::AT_LOGIN_ALARM))
	{
//kdDebug(5950)<<"KAEvent::setDisplaying("<<event.id()<<", "<<(alarmType==KAAlarm::MAIN_ALARM?"MAIN":alarmType==KAAlarm::REMINDER_ALARM?"REMINDER":alarmType==KAAlarm::DEFERRED_REMINDER_ALARM?"REMINDER_DEFERRAL":alarmType==KAAlarm::DEFERRED_ALARM?"DEFERRAL":"LOGIN")<<"): time="<<repeatAtLoginTime.toString()<<endl;
		KAAlarm al = event.alarm(alarmType);
		if (al.valid())
		{
			*this = event;
			setUid(DISPLAYING);
			mDisplaying     = true;
			mDisplayingTime = (alarmType == KAAlarm::AT_LOGIN_ALARM) ? repeatAtLoginTime : al.dateTime();
			switch (al.type())
			{
				case KAAlarm::AT_LOGIN__ALARM:                mDisplayingFlags = REPEAT_AT_LOGIN;  break;
				case KAAlarm::REMINDER__ALARM:                mDisplayingFlags = REMINDER;  break;
				case KAAlarm::DEFERRED_REMINDER_TIME__ALARM:  mDisplayingFlags = REMINDER | TIME_DEFERRAL;  break;
				case KAAlarm::DEFERRED_REMINDER_DATE__ALARM:  mDisplayingFlags = REMINDER | DATE_DEFERRAL;  break;
				case KAAlarm::DEFERRED_TIME__ALARM:           mDisplayingFlags = TIME_DEFERRAL;  break;
				case KAAlarm::DEFERRED_DATE__ALARM:           mDisplayingFlags = DATE_DEFERRAL;  break;
				default:                                      mDisplayingFlags = 0;  break;
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
KAAlarm KAEvent::convertDisplayingAlarm() const
{
	KAAlarm al;
	if (mDisplaying)
	{
		al = alarm(KAAlarm::DISPLAYING_ALARM);
		if (mDisplayingFlags & REPEAT_AT_LOGIN)
		{
			al.mRepeatAtLogin = true;
			al.mType = KAAlarm::AT_LOGIN__ALARM;
		}
		else if (mDisplayingFlags & DEFERRAL)
		{
			al.mDeferred = true;
			al.mType = (mDisplayingFlags == REMINDER | DATE_DEFERRAL) ? KAAlarm::DEFERRED_REMINDER_DATE__ALARM
			         : (mDisplayingFlags == REMINDER | TIME_DEFERRAL) ? KAAlarm::DEFERRED_REMINDER_TIME__ALARM
			         : (mDisplayingFlags == DATE_DEFERRAL) ? KAAlarm::DEFERRED_DATE__ALARM
			         : KAAlarm::DEFERRED_TIME__ALARM;
		}
		else if (mDisplayingFlags & REMINDER)
			al.mType = KAAlarm::REMINDER__ALARM;
		else
			al.mType = KAAlarm::MAIN__ALARM;
	}
	return al;
}

/******************************************************************************
 * Reinstate the original event from the 'displaying' event.
 */
void KAEvent::reinstateFromDisplaying(const KAEvent& dispEvent)
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
 * Determine whether the event will occur after the specified date/time.
 * If 'includeRepetitions' is true and the alarm has a simple repetition, it
 * returns true if any repetitions occur after the specified date/time.
 */
bool KAEvent::occursAfter(const QDateTime& preDateTime, bool includeRepetitions) const
{
	QDateTime dt;
	if (checkRecur() != NO_RECUR)
	{
		if (mRecurrence->duration() < 0)
			return true;    // infinite recurrence
		dt = mRecurrence->endDateTime();
	}
	else
		dt = mNextMainDateTime.dateTime();
	if (mStartDateTime.isDateOnly())
	{
		QDate pre = preDateTime.date();
		if (preDateTime.time() < Preferences::instance()->startOfDay())
			pre = pre.addDays(-1);    // today's recurrence (if today recurs) is still to come
		if (pre < dt.date())
			return true;
	}
	else if (preDateTime < dt)
		return true;

	if (includeRepetitions  &&  mRepeatCount)
	{
		dt.addSecs(mRepeatCount * mRepeatInterval * 60);
		if (preDateTime < dt)
			return true;
	}
	return false;
}

/******************************************************************************
 * Get the date/time of the next occurrence of the event, after the specified
 * date/time.
 * 'result' = date/time of next occurrence, or invalid date/time if none.
 */
KAEvent::OccurType KAEvent::nextOccurrence(const QDateTime& preDateTime, DateTime& result,
                                           KAEvent::OccurOption includeRepetitions) const
{
	int repeatSecs = 0;
	QDateTime pre = preDateTime;
	if (includeRepetitions != IGNORE_REPETITION)
	{
		if (!mRepeatCount)
			includeRepetitions = IGNORE_REPETITION;
		else
		{
			repeatSecs = mRepeatInterval * 60;
			pre = preDateTime.addSecs(-mRepeatCount * repeatSecs);
		}
	}

	OccurType type;
	bool recurs = (checkRecur() != NO_RECUR);
	if (recurs)
	{
		int remainingCount;
		type = nextRecurrence(pre, result, remainingCount);
	}
	else if (pre < mNextMainDateTime.dateTime())
	{
		result = mNextMainDateTime;
		type = FIRST_OCCURRENCE;
	}
	else
	{
		result = DateTime();
		return NO_OCCURRENCE;
	}

	if (result <= preDateTime)
	{
		// The next occurrence is a simple repetition
		int repetition = result.secsTo(preDateTime) / repeatSecs + 1;
		DateTime repeatDT = result.addSecs(repetition * repeatSecs);
		if (recurs)
		{
			// We've found a recurrence before the specified date/time, which has
			// a simple repetition after the date/time.
			// However, if the intervals between recurrences vary, we could possibly
			// have missed a later recurrence, which fits the criterion, so check again.
			DateTime dt;
			OccurType newType = previousOccurrence(repeatDT.dateTime(), dt, false);
			if (dt > result)
			{
				type = newType;
				result = dt;
				if (includeRepetitions == RETURN_REPETITION  &&  result <= preDateTime)
				{
					// The next occurrence is a simple repetition
					int repetition = result.secsTo(preDateTime) / repeatSecs + 1;
					result = result.addSecs(repetition * repeatSecs);
					type = static_cast<OccurType>(type | OCCURRENCE_REPEAT);
				}
				return type;
			}
		}
		if (includeRepetitions == RETURN_REPETITION)
		{
			// The next occurrence is a simple repetition
			result = repeatDT;
			type = static_cast<OccurType>(type | OCCURRENCE_REPEAT);
		}
	}
	return type;
}

/******************************************************************************
 * Get the date/time of the last previous occurrence of the event, before the
 * specified date/time.
 * If 'includeRepetitions' is true and the alarm has a simple repetition, the
 * last previous repetition is returned if appropriate.
 * 'result' = date/time of previous occurrence, or invalid date/time if none.
 */
KAEvent::OccurType KAEvent::previousOccurrence(const QDateTime& afterDateTime, DateTime& result, bool includeRepetitions) const
{
	if (mStartDateTime >= afterDateTime)
	{
		result = QDateTime();
		return NO_OCCURRENCE;     // the event starts after the specified date/time
	}

	// Find the latest recurrence of the event
	OccurType type;
	if (checkRecur() == NO_RECUR)
	{
		result = mStartDateTime;
		type = FIRST_OCCURRENCE;
	}
	else
	{
		QDateTime recurStart = mRecurrence->recurStart();
		QDateTime after = afterDateTime;
		if (mStartDateTime.isDateOnly()  &&  afterDateTime.time() > Preferences::instance()->startOfDay())
			after = after.addDays(1);    // today's recurrence (if today recurs) has passed
		bool last;
		QDateTime dt = mRecurrence->getPreviousDateTime(after, &last);
		result.set(dt, mStartDateTime.isDateOnly());
		if (!dt.isValid())
			return NO_OCCURRENCE;
		if (dt == recurStart)
			type = FIRST_OCCURRENCE;
		else if (last)
			type = LAST_RECURRENCE;
		else
			type = result.isDateOnly() ? RECURRENCE_DATE : RECURRENCE_DATE_TIME;
	}

	if (includeRepetitions  &&  mRepeatCount)
	{
		// Find the latest repetition which is before the specified time.
		// N.B. This is coded to avoid 32-bit integer overflow which occurs
		//      in QDateTime::secsTo() for large enough time differences.
		int repeatSecs = mRepeatInterval * 60;
		DateTime lastRepetition = result.addSecs(mRepeatCount * repeatSecs);
		if (lastRepetition < afterDateTime)
		{
			result = lastRepetition;
			return static_cast<OccurType>(type | OCCURRENCE_REPEAT);
		}
		int repetition = (result.dateTime().secsTo(afterDateTime) - 1) / repeatSecs;
		if (repetition > 0)
		{
			result = result.addSecs(repetition * repeatSecs);
			return static_cast<OccurType>(type | OCCURRENCE_REPEAT);
		}
	}
	return type;
}

/******************************************************************************
 * Set the date/time of the event to the next scheduled occurrence after the
 * specified date/time, provided that this is later than its current date/time.
 * Any reminder alarm is adjusted accordingly.
 * If 'includeRepetitions' is true and the alarm has a simple repetition, and
 * a repetition of a previous recurrence occurs after the specified date/time,
 * that previous recurrence is returned instead.
 */
KAEvent::OccurType KAEvent::setNextOccurrence(const QDateTime& preDateTime, bool includeRepetitions)
{
	if (preDateTime < mNextMainDateTime.dateTime())
		return FIRST_OCCURRENCE;    // it might not be the first recurrence - tant pis
	QDateTime pre = preDateTime;
	if (includeRepetitions)
	{
		if (!mRepeatCount)
			includeRepetitions = false;
		else
			pre = preDateTime.addSecs(-mRepeatCount * mRepeatInterval * 60);
	}

	DateTime dt;
	OccurType type;
	if (pre < mNextMainDateTime.dateTime())
	{
		dt = mNextMainDateTime;
		type = FIRST_OCCURRENCE;   // may not actually be the first occurrence
	}
	else if (checkRecur() != NO_RECUR)
	{
		int remainingCount;
		type = nextRecurrence(pre, dt, remainingCount);
		if (type == NO_OCCURRENCE)
			return NO_OCCURRENCE;
		if (type != FIRST_OCCURRENCE  &&  dt != mNextMainDateTime)
		{
			// Need to reschedule the next trigger date/time
			mNextMainDateTime = dt;
			if (mRecurrence->duration() > 0)
				mRemainingRecurrences = remainingCount;
			// Reinstate the reminder (if any) for the rescheduled recurrence
			if (mDeferral == REMINDER_DEFERRAL  ||  mArchiveReminderMinutes)
			{
				if (mReminderOnceOnly)
				{
					if (mReminderMinutes)
						set_archiveReminder();
				}
				else
					set_reminder(mArchiveReminderMinutes);
			}
			if (mDeferral == REMINDER_DEFERRAL)
				set_deferral(NO_DEFERRAL);
			mUpdated = true;
		}
	}
	else
		return NO_OCCURRENCE;

	if (includeRepetitions  &&  dt.dateTime() <= preDateTime)
	{
		// The next occurrence is a simple repetition.
		type = static_cast<OccurType>(type | OCCURRENCE_REPEAT);
		// Repetitions can't have a reminder, so remove any.
		if (mReminderMinutes)
			set_archiveReminder();
		if (mDeferral == REMINDER_DEFERRAL)
			set_deferral(NO_DEFERRAL);
		mUpdated = true;
	}
	return type;
}

/******************************************************************************
 * Get the date/time of the next recurrence of the event, after the specified
 * date/time.
 * 'result' = date/time of next occurrence, or invalid date/time if none.
 * 'remainingCount' = number of repetitions due, including the next occurrence.
 */
KAEvent::OccurType KAEvent::nextRecurrence(const QDateTime& preDateTime, DateTime& result, int& remainingCount) const
{
	QDateTime recurStart = mRecurrence->recurStart();
	QDateTime pre = preDateTime;
	if (mStartDateTime.isDateOnly()  &&  preDateTime.time() < Preferences::instance()->startOfDay())
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
		return LAST_RECURRENCE;
	}
	remainingCount = mRecurrence->duration() - mRecurrence->durationTo(dt) + 1;
	return result.isDateOnly() ? RECURRENCE_DATE : RECURRENCE_DATE_TIME;
}

/******************************************************************************
 * Return the recurrence interval as text suitable for display.
 */
QString KAEvent::recurrenceText(bool brief) const
{
	if (mRepeatAtLogin)
		return brief ? i18n("Brief form of 'At Login'", "Login") : i18n("At login");
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
	return brief ? QString::null : i18n("None");
}

/******************************************************************************
 * Return the repetition interval as text suitable for display.
 */
QString KAEvent::repetitionText(bool brief) const
{
	if (mRepeatCount)
	{
		if (mRepeatInterval % 1440)
		{
			if (mRepeatInterval < 60)
				return i18n("1 Minute", "%n Minutes", mRepeatInterval);
			if (mRepeatInterval % 60 == 0)
				return i18n("1 Hour", "%n Hours", mRepeatInterval/60);
			QString mins;
			return i18n("Hours and Minutes", "%1H %2M").arg(QString::number(mRepeatInterval/60)).arg(mins.sprintf("%02d", mRepeatInterval%60));
		}
		if (mRepeatInterval % (7*1440))
			return i18n("1 Day", "%n Days", mRepeatInterval/1440);
		return i18n("1 Week", "%n Weeks", mRepeatInterval/(7*1440));
	}
	return brief ? QString::null : i18n("None");
}

/******************************************************************************
 * Adjust the event date/time to the first recurrence of the event, on or after
 * start date/time. The event start date may not be a recurrence date, in which
 * case a later date will be set.
 */
void KAEvent::setFirstRecurrence()
{
	switch (checkRecur())
	{
		case NO_RECUR:
		case MINUTELY:
		case DAILY:
			return;
		case ANNUAL_DATE:
		case ANNUAL_POS:
			if (!mRecurrence->yearNums().count())
				return;    // (presumably it's a template)
			break;
		case WEEKLY:
		case MONTHLY_POS:
		case MONTHLY_DAY:
		case ANNUAL_DAY:
			break;
	}
	QDateTime recurStart = mRecurrence->recurStart();
	if (mRecurrence->recursOnPure(recurStart.date()))
		return;           // it already recurs on the start date

	// Set the frequency to 1 to find the first possible occurrence
	int frequency = mRecurrence->frequency();
	mRecurrence->setFrequency(1);
	int remainingCount;
	DateTime next;
	nextRecurrence(mNextMainDateTime.dateTime(), next, remainingCount);
	if (!next.isValid())
		mRecurrence->setRecurStart(recurStart);   // reinstate the old value
	else
	{
		mRecurrence->setRecurStart(next.dateTime());
		mStartDateTime = mNextMainDateTime = next;
		mUpdated = true;
	}
	mRecurrence->setFrequency(frequency);    // restore the frequency
}

/******************************************************************************
*  Initialise the event's recurrence from a KCal::Recurrence.
*  The event's start date/time is not changed.
*/
void KAEvent::setRecurrence(const Recurrence& recurrence)
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
			if (mRemainingRecurrences > 0  &&  !isTemplate())
				mRemainingRecurrences -= mRecurrence->durationTo(mNextMainDateTime.dateTime()) - 1;
			break;

		default:
			mRecurrence = 0;
			mRemainingRecurrences = 0;
			break;
	}

	// Adjust simple repetition values to fit the recurrence
	setRepetition(mRepeatInterval, mRepeatCount);
}

/******************************************************************************
*  Initialise the event's simple repetition.
*  The repetition length is adjusted if necessary to fit any recurrence interval.
*  Reply = false if a non-daily interval was specified for a date-only recurrence.
*/
bool KAEvent::setRepetition(int interval, int count)
{
	mUpdated        = true;
	mRepeatInterval = 0;
	mRepeatCount    = 0;
	if (interval > 0  &&  count > 0  &&  !mRepeatAtLogin)
	{
		if (interval % 1440  &&  mStartDateTime.isDateOnly())
			return false;    // interval must be in units of days for date-only alarms
		if (checkRecur() != NO_RECUR)
		{
			int longestInterval = longestRecurrenceInterval() - 1;
			if (interval * count > longestInterval)
				count = longestInterval / interval;
		}
		mRepeatInterval = interval;
		mRepeatCount    = count;
	}
	return true;
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
bool KAEvent::setRecurMinutely(Recurrence& recurrence, int freq, int count, const QDateTime& end)
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
bool KAEvent::setRecurDaily(Recurrence& recurrence, int freq, int count, const QDate& end)
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
bool KAEvent::setRecurWeekly(Recurrence& recurrence, int freq, const QBitArray& days, int count, const QDate& end)
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
bool KAEvent::setRecurMonthlyByDate(Recurrence& recurrence, int freq, const QValueList<int>& days, int count, const QDate& end)
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

bool KAEvent::setRecurMonthlyByDate(Recurrence& recurrence, int freq, const QPtrList<int>& days, int count, const QDate& end)
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
bool KAEvent::setRecurMonthlyByPos(Recurrence& recurrence, int freq, const QValueList<MonthPos>& posns, int count, const QDate& end)
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

bool KAEvent::setRecurMonthlyByPos(Recurrence& recurrence, int freq, const QPtrList<Recurrence::rMonthPos>& posns, int count, const QDate& end)
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
 *    day    = day of month, or 0 to use start date
 *    feb29  = if start date is March 1st, recur on February 29th; otherwise ignored
 *    count  = number of occurrences, including first and last.
 *           = -1 to recur indefinitely.
 *           = 0 to use 'end' instead.
 *    end    = end date (invalid to use 'count' instead).
 */
bool KAEvent::setRecurAnnualByDate(Recurrence& recurrence, int freq, const QValueList<int>& months, int day, int count, const QDate& end)
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
	if (day)
		recurrence.addMonthlyDay(day);
	return true;
}

bool KAEvent::setRecurAnnualByDate(Recurrence& recurrence, int freq, const QPtrList<int>& months, int day, int count, const QDate& end)
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
	if (day)
		recurrence.addMonthlyDay(day);
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
bool KAEvent::setRecurAnnualByPos(Recurrence& recurrence, int freq, const QValueList<MonthPos>& posns, const QValueList<int>& months, int count, const QDate& end)
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

bool KAEvent::setRecurAnnualByPos(Recurrence& recurrence, int freq, const QPtrList<Recurrence::rMonthPos>& posns, const QPtrList<int>& months, int count, const QDate& end)
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
bool KAEvent::setRecurAnnualByDay(Recurrence& recurrence, int freq, const QValueList<int>& days, int count, const QDate& end)
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

bool KAEvent::setRecurAnnualByDay(Recurrence& recurrence, int freq, const QPtrList<int>& days, int count, const QDate& end)
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
bool KAEvent::setRecurrence(Recurrence& recurrence, RecurType recurType, int repeatInterval,
                            int repeatCount, const DateTime& start, const QDateTime& end)
{
	if (start.isDateOnly())
		recurrence.setRecurStart(start.date());
	else
		recurrence.setRecurStart(start.dateTime());
	switch (recurType)
	{
		case MINUTELY:
			setRecurMinutely(recurrence, repeatInterval, repeatCount, end);
			break;
		case DAILY:
			setRecurDaily(recurrence, repeatInterval, repeatCount, end.date());
			break;
		case WEEKLY:
		{
			QBitArray days(7);
			days.setBit(start.date().dayOfWeek() - 1);
			setRecurWeekly(recurrence, repeatInterval, days, repeatCount, end.date());
			break;
		}
		case MONTHLY_DAY:
		{
			QValueList<int> days;
			days.append(start.date().day());
			setRecurMonthlyByDate(recurrence, repeatInterval, days, repeatCount, end.date());
			break;
		}
		case ANNUAL_DATE:
		{
			QValueList<int> months;
			months.append(start.date().month());
			setRecurAnnualByDate(recurrence, repeatInterval, months, 0, repeatCount, end.date());
			break;
		}
		case NO_RECUR:
			recurrence.unsetRecurs();
			break;
		default:
			recurrence.unsetRecurs();
			return false;
	}

	// Adjust simple repetition values to fit the recurrence
	return true;
}

/******************************************************************************
 * Initialise the event's recurrence and alarm repetition data, and set the
 * recurrence start date and repetition count if applicable.
 */
bool KAEvent::initRecur(const QDate& endDate, int count, bool feb29)
{
	mExceptionDates.clear();
	mExceptionDateTimes.clear();
	mRecursFeb29 = false;
	mUpdated = true;
	if (endDate.isValid() || count > 0 || count == -1)
	{
		if (!mRecurrence)
			mRecurrence = new Recurrence(0);
		mRecurrence->setRecurStart(mNextMainDateTime.dateTime());
		mRemainingRecurrences = count;
		int year = mNextMainDateTime.date().year();
		if (feb29  &&  !QDate::leapYear(year)
		&&  mNextMainDateTime.date().month() == 3  &&  mNextMainDateTime.date().day() == 1)
		{
			// The event start date is March 1st, but it is a recurrence
			// on February 29th (recurring on March 1st in non-leap years)
			while (!QDate::leapYear(--year)) ;
			mRecurrence->setRecurStart(QDateTime(QDate(year, 2, 29), mNextMainDateTime.time()));
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
KAEvent::RecurType KAEvent::checkRecur() const
{
	if (mRecurrence)
	{
		RecurType type = static_cast<RecurType>(mRecurrence->doesRecur());
		switch (type)
		{
			case Recurrence::rMinutely:                          // minutely
			case Recurrence::rHourly:       return MINUTELY;     // hourly		
			case Recurrence::rDaily:        return DAILY;        // daily
			case Recurrence::rWeekly:       return WEEKLY;       // weekly on multiple days of week
			case Recurrence::rMonthlyDay:   return MONTHLY_DAY;  // monthly on multiple dates in month
			case Recurrence::rMonthlyPos:   return MONTHLY_POS;  // monthly on multiple nth day of week
			case Recurrence::rYearlyMonth:  return ANNUAL_DATE;  // annually on multiple months (day of month = start date)
			case Recurrence::rYearlyPos:    return ANNUAL_POS;   // annually on multiple nth day of week in multiple months
			case Recurrence::rYearlyDay:    return ANNUAL_DAY;   // annually on multiple day numbers in year
			case Recurrence::rNone:
			default:
				if (mRecurrence)
				{
					delete mRecurrence;     // this shouldn't exist!!
					const_cast<KAEvent*>(this)->mRecurrence = 0;
				}
				break;
		}
		if (mRecurrence)
			const_cast<KAEvent*>(this)->mStartDateTime.set(mRecurrence->recurStart(), mStartDateTime.isDateOnly());   // shouldn't be necessary, but just in case...
	}
	return NO_RECUR;
}


/******************************************************************************
 * Return the recurrence interval in units of the recurrence period type.
 */
int KAEvent::recurInterval() const
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
 * Return the longest interval (in minutes) between recurrences.
 */
int KAEvent::longestRecurrenceInterval(const Recurrence& recurrence)
{
	int freq = recurrence.frequency();
	switch (recurrence.doesRecur())
	{
		case Recurrence::rMinutely:
			return freq;
		case Recurrence::rHourly:
			return freq * 60;
		case Recurrence::rDaily:
			return freq * 1440;
		case Recurrence::rWeekly:
		{
			// Find which days of the week it recurs on, and if on more than
			// one, reduce the maximum interval accordingly.
			QBitArray days = recurrence.days();
			int first = -1;
			int last  = -1;
			int maxgap = 1;
			for (int i = 0;  i < 7;  ++i)
			{
				if (days.testBit(KAlarm::localeDayInWeek_to_weekDay(i) - 1))
				{
					if (first < 0)
						first = i;
					else if (i - last > maxgap)
						maxgap = i - last;
					last = i;
				}
			}
			if (first < 0)
				break;    // no days recur
			int span = last - first;
			if (freq > 1)
				return (freq*7 - span) * 1440;
			if (7 - span > maxgap)
				return (7 - span) * 1440;
			return maxgap * 1440;
		}
		case Recurrence::rMonthlyDay:
		case Recurrence::rMonthlyPos:
			return freq * 1440 * 31;
		case Recurrence::rYearlyMonth:
		case Recurrence::rYearlyPos:
		{
			// Find which months of the year it recurs on, and if on more than
			// one, reduce the maximum interval accordingly.
			QPtrList<int> months = recurrence.yearNums();  // month list is sorted
			if (!months.count())
				break;    // no months recur
			if (months.count() > 1)
			{
				int first = -1;
				int last  = -1;
				int maxgap = 0;
				for (int* it = months.first();  it;  it = months.next())
				{
					if (first < 0)
						first = *it;
					else
					{
						int span = QDate(2001, last, 1).daysTo(QDate(2001, *it, 1));
						if (span > maxgap)
							maxgap = span;
					}
					last = *it;
				}
				int span = QDate(2001, first, 1).daysTo(QDate(2001, last, 1));
				if (freq > 1)
					return (freq*365 - span) * 1440;
				if (365 - span > maxgap)
					return (365 - span) * 1440;
				return maxgap * 1440;
			}
			// fall through to rYearlyDay
		}
		case Recurrence::rYearlyDay:
			return freq * 1440 * 365;
		case Recurrence::rNone:
		default:
			break;
	}
	return 0;
}

/******************************************************************************
 * Find the alarm template with the specified name.
 * Reply = invalid event if not found.
 */
KAEvent KAEvent::findTemplateName(AlarmCalendar& calendar, const QString& name)
{
	KAEvent event;
	Event::List events = calendar.events();
	for (Event::List::ConstIterator evit = events.begin();  evit != events.end();  ++evit)
	{
		Event* ev = *evit;
		if (ev->summary() == name)
		{
			event.set(*ev);
			if (!event.isTemplate())
				return KAEvent();    // this shouldn't ever happen
			break;
		}
	}
	return event;
}

/******************************************************************************
 * Adjust the time at which date-only events will occur for each of the events
 * in a list. Events for which both date and time are specified are left
 * unchanged.
 * Reply = true if any events have been updated.
 */
bool KAEvent::adjustStartOfDay(const Event::List& events)
{
	bool changed = false;
	QTime startOfDay = Preferences::instance()->startOfDay();
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
					if (data.type & KAAlarm::TIMED_DEFERRAL_FLAG)
					{
						// Timed deferral alarm, so adjust the offset
						deferralOffset = alarm.startOffset().asSeconds();
						alarm.setStartOffset(deferralOffset - adjustment);
					}
					else if (data.type == KAAlarm::AUDIO__ALARM
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
				if ((data.type & KAAlarm::DEFERRED_ALARM)
				&&  !(data.type & KAAlarm::TIMED_DEFERRAL_FLAG))
				{
					// Date-only deferral alarm, so adjust its time
					QDateTime altime = data.alarm->time();
					altime.setTime(startOfDay);
					deferralOffset = data.alarm->startOffset().asSeconds();
					newDeferralOffset = event->dtStart().secsTo(altime);
					const_cast<Alarm*>(data.alarm)->setStartOffset(newDeferralOffset);
					changed = true;
				}
				else if (data.type == KAAlarm::AUDIO__ALARM
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
 * Set the yearly February 29th recurrence type according to the user's
 * preferences.
 */
void KAEvent::setFeb29RecurType()
{
	Preferences::Feb29Type pfeb29 = Preferences::instance()->feb29RecurType();
	switch (pfeb29)
	{
		case Preferences::FEB29_FEB28:
		case Preferences::FEB29_NONE:
		case Preferences::FEB29_MAR1:
			break;
		default:
			pfeb29 = Preferences::default_feb29RecurType;
			break;
	}
	Recurrence::Feb29Type feb29;
	switch (pfeb29)
	{
		case Preferences::FEB29_FEB28:  feb29 = Recurrence::rFeb28;  break;
		case Preferences::FEB29_NONE:   feb29 = Recurrence::rFeb29;  break;
		case Preferences::FEB29_MAR1:
		default:                        feb29 = Recurrence::rMar1;   break;
	}
	Recurrence::setFeb29YearlyTypeDefault(feb29);
}

/******************************************************************************
 * If the calendar was written by a previous version of KAlarm, do any
 * necessary format conversions on the events to ensure that when the calendar
 * is saved, no information is lost or corrupted.
 */
void KAEvent::convertKCalEvents(AlarmCalendar& calendar)
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

	// KAlarm pre-1.1.1 LATECANCEL category with no parameter
	static const QString LATE_CANCEL_CAT = QString::fromLatin1("LATECANCEL");

	int version = calendar.KAlarmVersion();
	if (version >= AlarmCalendar::KAlarmVersion(1,2,1))
		return;

	kdDebug(5950) << "KAEvent::convertKCalEvents(): adjusting\n";
	bool pre_0_7   = (version < AlarmCalendar::KAlarmVersion(0,7,0));
	bool pre_0_9   = (version < AlarmCalendar::KAlarmVersion(0,9,0));
	bool pre_0_9_2 = (version < AlarmCalendar::KAlarmVersion(0,9,2));
	bool pre_1_1_1 = (version < AlarmCalendar::KAlarmVersion(1,1,1));
	bool pre_1_2_1 = (version < AlarmCalendar::KAlarmVersion(1,2,1));
	bool adjustSummerTime = calendar.KAlarmVersion057_UTC();
	QDateTime dt0(QDate(1970,1,1), QTime(0,0,0));
	QTime startOfDay = Preferences::instance()->startOfDay();

	Event::List events = calendar.events();
	for (Event::List::ConstIterator evit = events.begin();  evit != events.end();  ++evit)
	{
		Event* event = *evit;
		QStringList cats = event->categories();
		bool addLateCancel = false;

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
					addLateCancel = true;
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

		}

		if (pre_1_1_1)
		{
			/*
			 * It's a KAlarm pre-1.1.1 calendar file.
			 * Convert simple LATECANCEL category to LATECANCEL:n where n = minutes late.
			 */
			QStringList::Iterator it;
			while ((it = cats.find(LATE_CANCEL_CAT)) != cats.end())
			{
				cats.remove(it);
				addLateCancel = true;
			}
		}

		if (pre_1_2_1)
		{
			/*
			 * It's a KAlarm pre-1.2.1 calendar file.
			 * Convert email display alarms from translated to untranslated header prefixes.
			 */
			Alarm::List alarms = event->alarms();
			for (Alarm::List::ConstIterator alit = alarms.begin();  alit != alarms.end();  ++alit)
			{
				Alarm* alarm = *alit;
				if (alarm->type() == Alarm::Display)
				{
					QString oldtext = alarm->text();
					QString newtext = AlarmText::toCalendarText(oldtext);
					if (oldtext != newtext)
						alarm->setDisplayAlarm(newtext);
				}
			}
		}

		if (addLateCancel)
			cats.append(QString("%1%2").arg(LATE_CANCEL_CATEGORY).arg(1));

		event->setCategories(cats);
	}
}

#ifndef NDEBUG
void KAEvent::dumpDebug() const
{
	kdDebug(5950) << "KAEvent dump:\n";
	KAAlarmEventBase::dumpDebug();
	if (!mTemplateName.isEmpty())
	{
		kdDebug(5950) << "-- mTemplateName:" << mTemplateName << ":\n";
		kdDebug(5950) << "-- mTemplateDefaultTime:" << (mTemplateDefaultTime ? "true" : "false") << ":\n";
	}
	if (mActionType == T_MESSAGE  ||  mActionType == T_FILE)
	{
		kdDebug(5950) << "-- mAudioFile:" << mAudioFile << ":\n";
		kdDebug(5950) << "-- mPreAction:" << mPreAction << ":\n";
		kdDebug(5950) << "-- mPostAction:" << mPostAction << ":\n";
	}
	kdDebug(5950) << "-- mStartDateTime:" << mStartDateTime.toString() << ":\n";
	kdDebug(5950) << "-- mSaveDateTime:" << mSaveDateTime.toString() << ":\n";
	if (mRepeatAtLogin)
		kdDebug(5950) << "-- mAtLoginDateTime:" << mAtLoginDateTime.toString() << ":\n";
	kdDebug(5950) << "-- mArchiveRepeatAtLogin:" << (mArchiveRepeatAtLogin ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mEnabled:" << (mEnabled ? "true" : "false") << ":\n";
	if (mReminderMinutes)
		kdDebug(5950) << "-- mReminderMinutes:" << mReminderMinutes << ":\n";
	if (mArchiveReminderMinutes)
		kdDebug(5950) << "-- mArchiveReminderMinutes:" << mArchiveReminderMinutes << ":\n";
	if (mReminderMinutes  ||  mArchiveReminderMinutes)
		kdDebug(5950) << "-- mReminderOnceOnly:" << mReminderOnceOnly << ":\n";
	else if (mDeferral > 0)
	{
		kdDebug(5950) << "-- mDeferral:" << (mDeferral == NORMAL_DEFERRAL ? "normal" : "reminder") << ":\n";
		kdDebug(5950) << "-- mDeferralTime:" << mDeferralTime.toString() << ":\n";
	}
	else if (mDeferral == CANCEL_DEFERRAL)
		kdDebug(5950) << "-- mDeferral:cancel:\n";
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
	kdDebug(5950) << "KAEvent dump end\n";
}
#endif


/*=============================================================================
= Class KAAlarm
= Corresponds to a single KCal::Alarm instance.
=============================================================================*/

KAAlarm::KAAlarm(const KAAlarm& alarm)
	: KAAlarmEventBase(alarm),
	  mType(alarm.mType),
	  mRecurs(alarm.mRecurs),
	  mDeferred(alarm.mDeferred)
{ }

void KAAlarm::set(int flags)
{
	mDeferred = flags & KAEvent::DEFERRAL;
}

int KAAlarm::flags() const
{
	return KAAlarmEventBase::flags()
	     | (mDeferred ? KAEvent::DEFERRAL : 0);

}

#ifndef NDEBUG
void KAAlarm::dumpDebug() const
{
	kdDebug(5950) << "KAAlarm dump:\n";
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
		case PRE_ACTION__ALARM:              altype = "PRE_ACTION";  break;
		case POST_ACTION__ALARM:             altype = "POST_ACTION";  break;
		default:                             altype = "INVALID";  break;
	}
	kdDebug(5950) << "-- mType:" << altype << ":\n";
	kdDebug(5950) << "-- mRecurs:" << (mRecurs ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mDeferred:" << (mDeferred ? "true" : "false") << ":\n";
	kdDebug(5950) << "KAAlarm dump end\n";
}

const char* KAAlarm::debugType(Type type)
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
		case PRE_ACTION_ALARM:         return "PRE_ACTION";
		case POST_ACTION_ALARM:        return "POST_ACTION";
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
	mNextMainDateTime = rhs.mNextMainDateTime;
	mBgColour         = rhs.mBgColour;
	mFgColour         = rhs.mFgColour;
	mFont             = rhs.mFont;
	mEmailFromKMail   = rhs.mEmailFromKMail;
	mEmailAddresses   = rhs.mEmailAddresses;
	mEmailSubject     = rhs.mEmailSubject;
	mEmailAttachments = rhs.mEmailAttachments;
	mSoundVolume      = rhs.mSoundVolume;
	mActionType       = rhs.mActionType;
	mRepeatCount      = rhs.mRepeatCount;
	mRepeatInterval   = rhs.mRepeatInterval;
	mBeep             = rhs.mBeep;
	mRepeatSound      = rhs.mRepeatSound;
	mRepeatAtLogin    = rhs.mRepeatAtLogin;
	mDisplaying       = rhs.mDisplaying;
	mLateCancel       = rhs.mLateCancel;
	mAutoClose        = rhs.mAutoClose;
	mEmailBcc         = rhs.mEmailBcc;
	mConfirmAck       = rhs.mConfirmAck;
	mDefaultFont      = rhs.mDefaultFont;
}

void KAAlarmEventBase::set(int flags)
{
	mBeep          = flags & KAEvent::BEEP;
	mRepeatSound   = flags & KAEvent::REPEAT_SOUND;
	mRepeatAtLogin = flags & KAEvent::REPEAT_AT_LOGIN;
	mAutoClose     = flags & KAEvent::AUTO_CLOSE;
	mEmailBcc      = flags & KAEvent::EMAIL_BCC;
	mConfirmAck    = flags & KAEvent::CONFIRM_ACK;
	mDisplaying    = flags & KAEvent::DISPLAYING_;
	mDefaultFont   = flags & KAEvent::DEFAULT_FONT;
}

int KAAlarmEventBase::flags() const
{
	return (mBeep          ? KAEvent::BEEP : 0)
	     | (mRepeatSound   ? KAEvent::REPEAT_SOUND : 0)
	     | (mRepeatAtLogin ? KAEvent::REPEAT_AT_LOGIN : 0)
		 | (mAutoClose     ? KAEvent::AUTO_CLOSE : 0)
	     | (mEmailBcc      ? KAEvent::EMAIL_BCC : 0)
	     | (mConfirmAck    ? KAEvent::CONFIRM_ACK : 0)
	     | (mDisplaying    ? KAEvent::DISPLAYING_ : 0)
	     | (mDefaultFont   ? KAEvent::DEFAULT_FONT : 0);

}

const QFont& KAAlarmEventBase::font() const
{
	return mDefaultFont ? Preferences::instance()->messageFont() : mFont;
}

#ifndef NDEBUG
void KAAlarmEventBase::dumpDebug() const
{
	kdDebug(5950) << "-- mEventID:" << mEventID << ":\n";
	kdDebug(5950) << "-- mActionType:" << (mActionType == T_MESSAGE ? "MESSAGE" : mActionType == T_FILE ? "FILE" : mActionType == T_COMMAND ? "COMMAND" : mActionType == T_EMAIL ? "EMAIL" : mActionType == T_AUDIO ? "AUDIO" : "??") << ":\n";
	kdDebug(5950) << "-- mText:" << mText << ":\n";
	kdDebug(5950) << "-- mNextMainDateTime:" << mNextMainDateTime.toString() << ":\n";
	if (mActionType == T_EMAIL)
	{
		kdDebug(5950) << "-- mEmail: FromKMail:" << mEmailFromKMail << ":\n";
		kdDebug(5950) << "--         Addresses:" << mEmailAddresses.join(", ") << ":\n";
		kdDebug(5950) << "--         Subject:" << mEmailSubject << ":\n";
		kdDebug(5950) << "--         Attachments:" << mEmailAttachments.join(", ") << ":\n";
		kdDebug(5950) << "--         Bcc:" << (mEmailBcc ? "true" : "false") << ":\n";
	}
	kdDebug(5950) << "-- mBgColour:" << mBgColour.name() << ":\n";
	kdDebug(5950) << "-- mFgColour:" << mFgColour.name() << ":\n";
	kdDebug(5950) << "-- mDefaultFont:" << (mDefaultFont ? "true" : "false") << ":\n";
	if (!mDefaultFont)
		kdDebug(5950) << "-- mFont:" << mFont.toString() << ":\n";
	kdDebug(5950) << "-- mBeep:" << (mBeep ? "true" : "false") << ":\n";
	if (mActionType == T_AUDIO)
	{
		if (mSoundVolume >= 0)
			kdDebug(5950) << "-- mSoundVolume:" << mSoundVolume << ":\n";
		else
			kdDebug(5950) << "-- mSoundVolume:-:\n";
		kdDebug(5950) << "-- mRepeatSound:" << (mRepeatSound ? "true" : "false") << ":\n";
	}
	kdDebug(5950) << "-- mConfirmAck:" << (mConfirmAck ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mRepeatAtLogin:" << (mRepeatAtLogin ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mRepeatCount:" << mRepeatCount << ":\n";
	kdDebug(5950) << "-- mRepeatInterval:" << mRepeatInterval << ":\n";
	kdDebug(5950) << "-- mDisplaying:" << (mDisplaying ? "true" : "false") << ":\n";
	kdDebug(5950) << "-- mLateCancel:" << mLateCancel << ":\n";
	kdDebug(5950) << "-- mAutoClose:" << (mAutoClose ? "true" : "false") << ":\n";
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
				if (!ch.isLetterOrNumber())
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
