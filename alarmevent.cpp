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

#include "alarmcalendar.h"
#include "alarmtext.h"
#include "functions.h"
#include "kalarmapp.h"
#include "preferences.h"

#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <QColor>
#include <QRegExp>
#include <QByteArray>

#include <ksystemtimezone.h>
#include <klocale.h>
#include <kdebug.h>

#include <kcal/calendarlocal.h>
#include <libkholidays/kholidays.h>

using namespace KCal;
using namespace LibKHolidays;


// KAlarm version which first used the current calendar/event format.
// If this changes, KAEvent::convertKCalEvents() must be changed correspondingly.
// The string version is the KAlarm version string used in the calendar file.
QString KAEvent::calVersionString()  { return QString::fromLatin1("1.9.10"); }
int     KAEvent::calVersion()        { return KAlarm::Version(1,9,10); }

// Custom calendar properties.
// Note that all custom property names are prefixed with X-KDE-KALARM- in the calendar file.

// Event properties
static const QByteArray FLAGS_PROPERTY("FLAGS");              // X-KDE-KALARM-FLAGS property
static const QString DATE_ONLY_FLAG        = QLatin1String("DATE");
static const QString EMAIL_BCC_FLAG        = QLatin1String("BCC");
static const QString CONFIRM_ACK_FLAG      = QLatin1String("ACKCONF");
static const QString KORGANIZER_FLAG       = QLatin1String("KORG");
static const QString EXCLUDE_HOLIDAYS_FLAG = QLatin1String("EXHOLIDAYS");
static const QString WORK_TIME_ONLY_FLAG   = QLatin1String("WORKTIME");
static const QString DEFER_FLAG            = QLatin1String("DEFER");
static const QString LATE_CANCEL_FLAG      = QLatin1String("LATECANCEL");
static const QString AUTO_CLOSE_FLAG       = QLatin1String("LATECLOSE");
static const QString TEMPL_AFTER_TIME_FLAG = QLatin1String("TMPLAFTTIME");
static const QString KMAIL_SERNUM_FLAG     = QLatin1String("KMAIL");

static const QByteArray NEXT_RECUR_PROPERTY("NEXTRECUR");     // X-KDE-KALARM-NEXTRECUR property
static const QByteArray REPEAT_PROPERTY("REPEAT");            // X-KDE-KALARM-REPEAT property
static const QByteArray ARCHIVE_PROPERTY("ARCHIVE");          // X-KDE-KALARM-ARCHIVE property
static const QString ARCHIVE_REMINDER_ONCE_TYPE = QLatin1String("ONCE");
static const QByteArray LOG_PROPERTY("LOG");                  // X-KDE-KALARM-LOG property
static const QString xtermURL = QLatin1String("xterm:");
static const QString displayURL = QLatin1String("display:");

// - General alarm properties
static const QByteArray TYPE_PROPERTY("TYPE");                // X-KDE-KALARM-TYPE property
static const QString FILE_TYPE                  = QLatin1String("FILE");
static const QString AT_LOGIN_TYPE              = QLatin1String("LOGIN");
static const QString REMINDER_TYPE              = QLatin1String("REMINDER");
static const QString REMINDER_ONCE_TYPE         = QLatin1String("REMINDER_ONCE");
static const QString TIME_DEFERRAL_TYPE         = QLatin1String("DEFERRAL");
static const QString DATE_DEFERRAL_TYPE         = QLatin1String("DATE_DEFERRAL");
static const QString DISPLAYING_TYPE            = QLatin1String("DISPLAYING");   // used only in displaying calendar
static const QString PRE_ACTION_TYPE            = QLatin1String("PRE");
static const QString POST_ACTION_TYPE           = QLatin1String("POST");
static const QByteArray NEXT_REPEAT_PROPERTY("NEXTREPEAT");   // X-KDE-KALARM-NEXTREPEAT property
// - Display alarm properties
static const QByteArray FONT_COLOUR_PROPERTY("FONTCOLOR");    // X-KDE-KALARM-FONTCOLOR property
// - Email alarm properties
static const QByteArray EMAIL_ID_PROPERTY("EMAILID");         // X-KDE-KALARM-EMAILID property
// - Audio alarm properties
static const QByteArray VOLUME_PROPERTY("VOLUME");            // X-KDE-KALARM-VOLUME property
static const QByteArray SPEAK_PROPERTY("SPEAK");              // X-KDE-KALARM-SPEAK property
// - Command alarm properties
static const QByteArray CANCEL_ON_ERROR_PROPERTY("ERRCANCEL");// X-KDE-KALARM-ERRCANCEL property

// Event status strings
static const QString DISABLED_STATUS            = QLatin1String("DISABLED");

// Displaying event ID identifier
static const QString DISP_DEFER = QLatin1String("DEFER");
static const QString DISP_EDIT  = QLatin1String("EDIT");

static const QString SC = QLatin1String(";");

struct AlarmData
{
	const Alarm*           alarm;
	QString                cleanText;       // text or audio file name
	uint                   emailFromId;
	QFont                  font;
	QColor                 bgColour, fgColour;
	float                  soundVolume;
	float                  fadeVolume;
	int                    fadeSeconds;
	int                    nextRepeat;
	bool                   speak;
	KAAlarm::SubType       type;
	KAAlarmEventBase::Type action;
	int                    displayingFlags;
	bool                   defaultFont;
	bool                   reminderOnceOnly;
	bool                   isEmailText;
	bool                   commandScript;
	bool                   cancelOnPreActErr;
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
	if (minutes  ||  !mReminderMinutes)
		++mAlarmCount;
	else if (!minutes  &&  mReminderMinutes)
		--mAlarmCount;
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
	mResource                = event.mResource;
	mResourceId              = event.mResourceId;
	mAudioFile               = event.mAudioFile;
	mPreAction               = event.mPreAction;
	mPostAction              = event.mPostAction;
	mStartDateTime           = event.mStartDateTime;
	mSaveDateTime            = event.mSaveDateTime;
	mAtLoginDateTime         = event.mAtLoginDateTime;
	mDeferralTime            = event.mDeferralTime;
	mAllTrigger              = event.mAllTrigger;
	mMainTrigger             = event.mMainTrigger;
	mAllWorkTrigger          = event.mAllWorkTrigger;
	mMainWorkTrigger         = event.mMainWorkTrigger;
	mDisplayingTime          = event.mDisplayingTime;
	mDisplayingFlags         = event.mDisplayingFlags;
	mReminderMinutes         = event.mReminderMinutes;
	mArchiveReminderMinutes  = event.mArchiveReminderMinutes;
	mDeferDefaultMinutes     = event.mDeferDefaultMinutes;
	mRevision                = event.mRevision;
	mAlarmCount              = event.mAlarmCount;
	mDeferral                = event.mDeferral;
	mLogFile                 = event.mLogFile;
	mCategory                = event.mCategory;
	mCancelOnPreActErr       = event.mCancelOnPreActErr;
	mCommandXterm            = event.mCommandXterm;
	mCommandDisplay          = event.mCommandDisplay;
	mSpeak                   = event.mSpeak;
	mKMailSerialNumber       = event.mKMailSerialNumber;
	mCopyToKOrganizer        = event.mCopyToKOrganizer;
	mExcludeHolidays         = event.mExcludeHolidays;
	mWorkTimeOnly            = event.mWorkTimeOnly;
	mReminderOnceOnly        = event.mReminderOnceOnly;
	mMainExpired             = event.mMainExpired;
	mArchiveRepeatAtLogin    = event.mArchiveRepeatAtLogin;
	mArchive                 = event.mArchive;
	mTemplateAfterTime       = event.mTemplateAfterTime;
	mDisplayingDefer         = event.mDisplayingDefer;
	mDisplayingEdit          = event.mDisplayingEdit;
	mEnabled                 = event.mEnabled;
	mUpdated                 = event.mUpdated;
	mChangeCount             = 0;
	mChanged                 = false;
	delete mRecurrence;
	if (event.mRecurrence)
		mRecurrence = new KARecurrence(*event.mRecurrence);
	else
		mRecurrence = 0;
	if (event.mChanged)
		calcTriggerTimes();
}

/******************************************************************************
* Initialise the KAEvent from a KCal::Event.
*/
void KAEvent::set(const Event* event)
{
	startChanges();
	// Extract status from the event
	mEventID                = event->uid();
	mRevision               = event->revision();
	mTemplateName.clear();
	mLogFile.clear();
	mResourceId.clear();
	mTemplateAfterTime      = -1;
	mBeep                   = false;
	mSpeak                  = false;
	mEmailBcc               = false;
	mCommandXterm           = false;
	mCommandDisplay         = false;
	mCopyToKOrganizer       = false;
	mExcludeHolidays        = false;
	mWorkTimeOnly           = false;
	mConfirmAck             = false;
	mArchive                = false;
	mReminderOnceOnly       = false;
	mAutoClose              = false;
	mArchiveRepeatAtLogin   = false;
	mDisplayingDefer        = false;
	mDisplayingEdit         = false;
	mResource               = 0;
	mArchiveReminderMinutes = 0;
	mDeferDefaultMinutes    = 0;
	mLateCancel             = 0;
	mKMailSerialNumber      = 0;
	mChangeCount            = 0;
	mChanged                = false;
	mBgColour               = QColor(255, 255, 255);    // missing/invalid colour - return white background
	mFgColour               = QColor(0, 0, 0);          // and black foreground
	mDefaultFont            = true;
	mEnabled                = true;
	clearRecur();
	QString param;
	mCategory               = KCalEvent::status(event, &param);
	if (mCategory == KCalEvent::DISPLAYING)
	{
		// It's a displaying calendar event - set values specific to displaying alarms
		QStringList params = param.split(SC, QString::KeepEmptyParts);
		int n = params.count();
		if (n)
		{
			mResourceId = params[0];
			for (int i = 1;  i < n;  ++i)
			{
				if (params[i] == DISP_DEFER)
					mDisplayingDefer = true;
				if (params[i] == DISP_EDIT)
					mDisplayingEdit = true;
			}
		}
	}
	bool ok;
	bool dateOnly = false;
	QStringList flags = event->customProperty(KCalendar::APPNAME, FLAGS_PROPERTY).split(SC, QString::SkipEmptyParts);
	flags += QString();    // to avoid having to check for end of list
	for (int i = 0, end = flags.count() - 1;  i < end;  ++i)
	{
		if (flags[i] == DATE_ONLY_FLAG)
			dateOnly = true;
		else if (flags[i] == CONFIRM_ACK_FLAG)
			mConfirmAck = true;
		else if (flags[i] == EMAIL_BCC_FLAG)
			mEmailBcc = true;
		else if (flags[i] == KORGANIZER_FLAG)
			mCopyToKOrganizer = true;
		else if (flags[i] == EXCLUDE_HOLIDAYS_FLAG)
			mExcludeHolidays = true;
		else if (flags[i] == WORK_TIME_ONLY_FLAG)
			mWorkTimeOnly = true;
		else if (flags[i]== KMAIL_SERNUM_FLAG)
		{
			unsigned long n = flags[i + 1].toULong(&ok);
			if (!ok)
				continue;
			mKMailSerialNumber = n;
			++i;
		}
		else if (flags[i] == DEFER_FLAG)
		{
			int n = static_cast<int>(flags[i + 1].toUInt(&ok));
			if (!ok)
				continue;
			mDeferDefaultMinutes = n;
			++i;
		}
		else if (flags[i] == TEMPL_AFTER_TIME_FLAG)
		{
			int n = static_cast<int>(flags[i + 1].toUInt(&ok));
			if (!ok)
				continue;
			mTemplateAfterTime = n;
			++i;
		}
		else if (flags[i] == LATE_CANCEL_FLAG)
		{
			mLateCancel = static_cast<int>(flags[i + 1].toUInt(&ok));
			if (ok)
				++i;
			if (!ok  ||  !mLateCancel)
				mLateCancel = 1;    // invalid parameter defaults to 1 minute
		}
		else if (flags[i] == AUTO_CLOSE_FLAG)
		{
			mLateCancel = static_cast<int>(flags[i + 1].toUInt(&ok));
			if (ok)
				++i;
			if (!ok  ||  !mLateCancel)
				mLateCancel = 1;    // invalid parameter defaults to 1 minute
			mAutoClose = true;
		}
	}

	QString prop = event->customProperty(KCalendar::APPNAME, LOG_PROPERTY);
	if (!prop.isEmpty())
	{
		if (prop == xtermURL)
			mCommandXterm = true;
		else if (prop == displayURL)
			mCommandDisplay = true;
		else
			mLogFile = prop;
	}
	prop = event->customProperty(KCalendar::APPNAME, REPEAT_PROPERTY);
	if (!prop.isEmpty())
	{
		// This property is used when the main alarm has expired
		QStringList list = prop.split(QLatin1Char(':'));
		if (list.count() >= 2)
		{
			int interval = static_cast<int>(list[0].toUInt());
			int count = static_cast<int>(list[1].toUInt());
			if (interval && count)
			{
				if (interval % (24*60))
					mRepeatInterval = Duration(interval * 60, Duration::Seconds);
				else
					mRepeatInterval = Duration(interval / (24*60), Duration::Days);
				mRepeatCount = count;
			}
		}
	}
	prop = event->customProperty(KCalendar::APPNAME, ARCHIVE_PROPERTY);
	if (!prop.isEmpty())
	{
		mArchive = true;
		if (prop != QLatin1String("0"))
		{
			// It's the archive property containing a reminder time and/or repeat-at-login flag
			QStringList list = prop.split(SC, QString::SkipEmptyParts);
			for (int j = 0;  j < list.count();  ++j)
			{
				if (list[j] == AT_LOGIN_TYPE)
					mArchiveRepeatAtLogin = true;
				else if (list[j] == ARCHIVE_REMINDER_ONCE_TYPE)
					mReminderOnceOnly = true;
				else
				{
					char ch;
					const char* cat = list[j].toLatin1().constData();
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
	mNextMainDateTime = readDateTime(event, dateOnly, mStartDateTime);
	mSaveDateTime = event->created();
	if (dateOnly  &&  !mRepeatInterval.isDaily())
		mRepeatInterval = Duration(mRepeatInterval.asDays(), Duration::Days);
	if (category() == KCalEvent::TEMPLATE)
		mTemplateName = event->summary();
	if (event->statusStr() == DISABLED_STATUS)
		mEnabled = false;

	// Extract status from the event's alarms.
	// First set up defaults.
	mActionType        = T_MESSAGE;
	mMainExpired       = true;
	mRepeatAtLogin     = false;
	mDisplaying        = false;
	mRepeatSound       = false;
	mCommandScript     = false;
	mCancelOnPreActErr = false;
	mDeferral          = NO_DEFERRAL;
	mSoundVolume       = -1;
	mFadeVolume        = -1;
	mFadeSeconds       = 0;
	mReminderMinutes   = 0;
	mEmailFromIdentity = 0;
	mText              = "";
	mAudioFile         = "";
	mPreAction         = "";
	mPostAction        = "";
	mEmailSubject      = "";
	mEmailAddresses.clear();
	mEmailAttachments.clear();

	// Extract data from all the event's alarms and index the alarms by sequence number
	AlarmMap alarmMap;
	readAlarms(event, &alarmMap, mCommandDisplay);

	// Incorporate the alarms' details into the overall event
	mAlarmCount = 0;       // initialise as invalid
	DateTime alTime;
	bool set = false;
	bool isEmailText = false;
	bool setDeferralTime = false;
	Duration deferralOffset;
	for (AlarmMap::ConstIterator it = alarmMap.constBegin();  it != alarmMap.constEnd();  ++it)
	{
		const AlarmData& data = it.value();
		DateTime dateTime = data.alarm->hasStartOffset() ? data.alarm->startOffset().end(mNextMainDateTime.effectiveKDateTime()) : data.alarm->time();
		switch (data.type)
		{
			case KAAlarm::MAIN__ALARM:
				mMainExpired = false;
				alTime = dateTime;
				alTime.setDateOnly(mStartDateTime.isDateOnly());
				if (data.alarm->repeatCount()  &&  data.alarm->snoozeTime())
				{
					mRepeatInterval = data.alarm->snoozeTime();   // values may be adjusted in setRecurrence()
					mRepeatCount    = data.alarm->repeatCount();
					mNextRepeat     = data.nextRepeat;
				}
				break;
			case KAAlarm::AT_LOGIN__ALARM:
				mRepeatAtLogin   = true;
				mAtLoginDateTime = dateTime.kDateTime();
				alTime = mAtLoginDateTime;
				break;
			case KAAlarm::REMINDER__ALARM:
				// N.B. there can be a start offset but no valid date/time (e.g. in template)
				mReminderMinutes = -(data.alarm->startOffset().asSeconds() / 60);
				if (mReminderMinutes)
					mArchiveReminderMinutes = 0;
				break;
			case KAAlarm::DEFERRED_REMINDER_DATE__ALARM:
			case KAAlarm::DEFERRED_DATE__ALARM:
				mDeferral = (data.type == KAAlarm::DEFERRED_REMINDER_DATE__ALARM) ? REMINDER_DEFERRAL : NORMAL_DEFERRAL;
				mDeferralTime = dateTime;
				mDeferralTime.setDateOnly(true);
				if (data.alarm->hasStartOffset())
					deferralOffset = data.alarm->startOffset();
				break;
			case KAAlarm::DEFERRED_REMINDER_TIME__ALARM:
			case KAAlarm::DEFERRED_TIME__ALARM:
				mDeferral = (data.type == KAAlarm::DEFERRED_REMINDER_TIME__ALARM) ? REMINDER_DEFERRAL : NORMAL_DEFERRAL;
				mDeferralTime = dateTime;
				if (data.alarm->hasStartOffset())
					deferralOffset = data.alarm->startOffset();
				break;
			case KAAlarm::DISPLAYING__ALARM:
			{
				mDisplaying      = true;
				mDisplayingFlags = data.displayingFlags;
				bool dateOnly = (mDisplayingFlags & DEFERRAL) ? !(mDisplayingFlags & TIMED_FLAG)
				              : mStartDateTime.isDateOnly();
				mDisplayingTime = dateTime;
				mDisplayingTime.setDateOnly(dateOnly);
				alTime = mDisplayingTime;
				break;
			}
			case KAAlarm::AUDIO__ALARM:
				mAudioFile   = data.cleanText;
				mSpeak       = data.speak  &&  mAudioFile.isEmpty();
				mBeep        = !mSpeak  &&  mAudioFile.isEmpty();
				mSoundVolume = (!mBeep && !mSpeak) ? data.soundVolume : -1;
				mFadeVolume  = (mSoundVolume >= 0  &&  data.fadeSeconds > 0) ? data.fadeVolume : -1;
				mFadeSeconds = (mFadeVolume >= 0) ? data.fadeSeconds : 0;
				mRepeatSound = (!mBeep && !mSpeak)  &&  (data.alarm->repeatCount() < 0);
				break;
			case KAAlarm::PRE_ACTION__ALARM:
				mPreAction         = data.cleanText;
				mCancelOnPreActErr = data.cancelOnPreActErr;
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
		bool noSetNextTime = false;
		switch (data.type)
		{
			case KAAlarm::DEFERRED_REMINDER_DATE__ALARM:
			case KAAlarm::DEFERRED_DATE__ALARM:
			case KAAlarm::DEFERRED_REMINDER_TIME__ALARM:
			case KAAlarm::DEFERRED_TIME__ALARM:
				if (!set)
				{
					// The recurrence has to be evaluated before we can
					// calculate the time of a deferral alarm.
					setDeferralTime = true;
					noSetNextTime = true;
				}
				// fall through to AT_LOGIN__ALARM etc.
			case KAAlarm::AT_LOGIN__ALARM:
			case KAAlarm::REMINDER__ALARM:
			case KAAlarm::DISPLAYING__ALARM:
				if (!set  &&  !noSetNextTime)
					mNextMainDateTime = alTime;
				// fall through to MAIN__ALARM
			case KAAlarm::MAIN__ALARM:
				// Ensure that the basic fields are set up even if there is no main
				// alarm in the event (if it has expired and then been deferred)
				if (!set)
				{
					mActionType = data.action;
					mText = (mActionType == T_COMMAND) ? data.cleanText.trimmed() : data.cleanText;
					switch (data.action)
					{
						case T_COMMAND:
							mCommandScript = data.commandScript;
							if (!mCommandDisplay)
								break;
							// fall through to T_MESSAGE
						case T_MESSAGE:
							mFont        = data.font;
							mDefaultFont = data.defaultFont;
							if (data.isEmailText)
								isEmailText = true;
							// fall through to T_FILE
						case T_FILE:
							mBgColour    = data.bgColour;
							mFgColour    = data.fgColour;
							break;
						case T_EMAIL:
							mEmailFromIdentity = data.emailFromId;
							mEmailAddresses    = data.alarm->mailAddresses();
							mEmailSubject      = data.alarm->mailSubject();
							mEmailAttachments  = data.alarm->mailAttachments();
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
	if (!isEmailText)
		mKMailSerialNumber = 0;
	if (mRepeatAtLogin)
		mArchiveRepeatAtLogin = false;

	Recurrence* recur = event->recurrence();
	if (recur  &&  recur->recurs())
	{
		int nextRepeat = mNextRepeat;    // setRecurrence() clears mNextRepeat
		setRecurrence(*recur);
		if (nextRepeat <= mRepeatCount)
			mNextRepeat = nextRepeat;
	}
	else
		checkRepetition();

	if (mMainExpired  &&  deferralOffset  &&  checkRecur() != KARecurrence::NO_RECUR)
	{
		// Adjust the deferral time for an expired recurrence, since the
		// offset is relative to the first actual occurrence.
		DateTime dt = mRecurrence->getNextDateTime(mStartDateTime.addDays(-1).kDateTime());
		dt.setDateOnly(mStartDateTime.isDateOnly());
		if (mDeferralTime.isDateOnly())
		{
			mDeferralTime = deferralOffset.end(dt.kDateTime());
			mDeferralTime.setDateOnly(true);
		}
		else
			mDeferralTime = deferralOffset.end(dt.effectiveKDateTime());
	}
	if (mDeferral)
	{
		if (setDeferralTime)
			mNextMainDateTime = mDeferralTime;
	}
	mChanged = true;
	endChanges();

	mUpdated = false;
}

/******************************************************************************
* Fetch the start and next date/time for a KCal::Event.
* Reply = next main date/time.
*/
DateTime KAEvent::readDateTime(const Event* event, bool dateOnly, DateTime& start)
{
	start = event->dtStart();
	if (dateOnly)
	{
		// A date-only event is indicated by the X-KDE-KALARM-FLAGS:DATE property, not
		// by a date-only start date/time (for the reasons given in updateKCalEvent()).
		start.setDateOnly(true);
	}
	DateTime next = start;
	QString prop = event->customProperty(KCalendar::APPNAME, NEXT_RECUR_PROPERTY);
	if (prop.length() >= 8)
	{
		// The next due recurrence time is specified
		QDate d(prop.left(4).toInt(), prop.mid(4,2).toInt(), prop.mid(6,2).toInt());
		if (d.isValid())
		{
			if (dateOnly  &&  prop.length() == 8)
				next.setDate(d);
			else if (!dateOnly  &&  prop.length() == 15  &&  prop[8] == QChar('T'))
			{
				QTime t(prop.mid(9,2).toInt(), prop.mid(11,2).toInt(), prop.mid(13,2).toInt());
				if (t.isValid())
				{
					next.setDate(d);
					next.setTime(t);
				}
			}
		}
	}
	return next;
}

/******************************************************************************
* Parse the alarms for a KCal::Event.
* Reply = map of alarm data, indexed by KAAlarm::Type
*/
void KAEvent::readAlarms(const Event* event, void* almap, bool cmdDisplay)
{
	AlarmMap* alarmMap = (AlarmMap*)almap;
	Alarm::List alarms = event->alarms();
	for (int i = 0, end = alarms.count();  i < end;  ++i)
	{
		// Parse the next alarm's text
		AlarmData data;
		readAlarm(alarms[i], data, cmdDisplay);
		if (data.type != KAAlarm::INVALID__ALARM)
			alarmMap->insert(data.type, data);
	}
}

/******************************************************************************
* Parse a KCal::Alarm.
* Reply = alarm ID (sequence number)
*/
void KAEvent::readAlarm(const Alarm* alarm, AlarmData& data, bool cmdDisplay)
{
	// Parse the next alarm's text
	data.alarm           = alarm;
	data.displayingFlags = 0;
	data.isEmailText     = false;
	data.nextRepeat      = 0;
	if (alarm->repeatCount())
	{
		bool ok;
		QString property = alarm->customProperty(KCalendar::APPNAME, NEXT_REPEAT_PROPERTY);
		int n = static_cast<int>(property.toUInt(&ok));
		if (ok)
			data.nextRepeat = n;
	}
	switch (alarm->type())
	{
		case Alarm::Procedure:
			data.action        = T_COMMAND;
			data.cleanText     = alarm->programFile();
			data.commandScript = data.cleanText.isEmpty();   // blank command indicates a script
			if (!alarm->programArguments().isEmpty())
			{
				if (!data.commandScript)
					data.cleanText += ' ';
				data.cleanText += alarm->programArguments();
			}
			data.cancelOnPreActErr = !alarm->customProperty(KCalendar::APPNAME, CANCEL_ON_ERROR_PROPERTY).isNull();
			if (!cmdDisplay)
				break;
			// fall through to Display
		case Alarm::Display:
		{
			if (alarm->type() == Alarm::Display)
			{
				data.action    = T_MESSAGE;
				data.cleanText = AlarmText::fromCalendarText(alarm->text(), data.isEmailText);
			}
			QString property = alarm->customProperty(KCalendar::APPNAME, FONT_COLOUR_PROPERTY);
			QStringList list = property.split(QLatin1Char(';'), QString::KeepEmptyParts);
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
		case Alarm::Email:
			data.action      = T_EMAIL;
			data.emailFromId = alarm->customProperty(KCalendar::APPNAME, EMAIL_ID_PROPERTY).toUInt();
			data.cleanText   = alarm->mailText();
			break;
		case Alarm::Audio:
		{
			data.action      = T_AUDIO;
			data.cleanText   = alarm->audioFile();
			data.type        = KAAlarm::AUDIO__ALARM;
			data.soundVolume = -1;
			data.fadeVolume  = -1;
			data.fadeSeconds = 0;
			data.speak       = !alarm->customProperty(KCalendar::APPNAME, SPEAK_PROPERTY).isNull();
			QString property = alarm->customProperty(KCalendar::APPNAME, VOLUME_PROPERTY);
			if (!property.isEmpty())
			{
				bool ok;
				float fadeVolume;
				int   fadeSecs = 0;
				QStringList list = property.split(QLatin1Char(';'), QString::KeepEmptyParts);
				data.soundVolume = list[0].toFloat(&ok);
				if (!ok)
					data.soundVolume = -1;
				if (data.soundVolume >= 0  &&  list.count() >= 3)
				{
					fadeVolume = list[1].toFloat(&ok);
					if (ok)
						fadeSecs = static_cast<int>(list[2].toUInt(&ok));
					if (ok  &&  fadeVolume >= 0  &&  fadeSecs > 0)
					{
						data.fadeVolume  = fadeVolume;
						data.fadeSeconds = fadeSecs;
					}
				}
			}
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
	QString property = alarm->customProperty(KCalendar::APPNAME, TYPE_PROPERTY);
	QStringList types = property.split(QLatin1Char(','), QString::SkipEmptyParts);
	for (int i = 0, end = types.count();  i < end;  ++i)
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
//kDebug()<<"text="<<alarm->text()<<", time="<<alarm->time().toString()<<", valid time="<<alarm->time().isValid();
}

/******************************************************************************
* Initialise the KAEvent with the specified parameters.
*/
void KAEvent::set(const KDateTime& dateTime, const QString& text, const QColor& bg, const QColor& fg,
                  const QFont& font, Action action, int lateCancel, int flags, bool changesPending)
{
	clearRecur();
	mStartDateTime = dateTime;
	mStartDateTime.setDateOnly(flags & ANY_TIME);
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
	mEventID.clear();
	mTemplateName.clear();
	mResourceId.clear();
	mPreAction.clear();
	mPostAction.clear();
	mResource               = 0;
	mText                   = (mActionType == T_COMMAND) ? text.trimmed() : text;
	mCategory               = KCalEvent::ACTIVE;
	mAudioFile              = "";
	mSoundVolume            = -1;
	mFadeVolume             = -1;
	mTemplateAfterTime      = -1;
	mFadeSeconds            = 0;
	mBgColour               = bg;
	mFgColour               = fg;
	mFont                   = font;
	mAlarmCount             = 1;
	mLateCancel             = lateCancel;     // do this before setting flags
	mDeferral               = NO_DEFERRAL;    // do this before setting flags

	KAAlarmEventBase::set(flags & ~READ_ONLY_FLAGS);
	mStartDateTime.setDateOnly(flags & ANY_TIME);
	set_deferral((flags & DEFERRAL) ? NORMAL_DEFERRAL : NO_DEFERRAL);
	mCommandXterm           = flags & EXEC_IN_XTERM;
	mCommandDisplay         = flags & DISPLAY_COMMAND;
	mCopyToKOrganizer       = flags & COPY_KORGANIZER;
	mExcludeHolidays        = flags & EXCL_HOLIDAYS;
	mWorkTimeOnly           = flags & WORK_TIME_ONLY;
	mEnabled                = !(flags & DISABLED);
	mSpeak                  = flags & SPEAK;
	if (mSpeak)
		mBeep           = false;

	mUpdated = true;
	mKMailSerialNumber      = 0;
	mReminderMinutes        = 0;
	mArchiveReminderMinutes = 0;
	mDeferDefaultMinutes    = 0;
	mArchiveRepeatAtLogin   = false;
	mReminderOnceOnly       = false;
	mDisplaying             = false;
	mMainExpired            = false;
	mDisplayingDefer        = false;
	mDisplayingEdit         = false;
	mArchive                = false;
	mCancelOnPreActErr      = false;
	mUpdated                = false;
	mChangeCount            = changesPending ? 1 : 0;
	mChanged                = true;
	calcTriggerTimes();
}

void KAEvent::setLogFile(const QString& logfile)
{
	mLogFile = logfile;
	if (!logfile.isEmpty())
		mCommandDisplay = mCommandXterm = false;
}

void KAEvent::setEmail(uint from, const EmailAddressList& addresses, const QString& subject, const QStringList& attachments)
{
	mEmailFromIdentity = from;
	mEmailAddresses    = addresses;
	mEmailSubject      = subject;
	mEmailAttachments  = attachments;
}

void KAEvent::setAudioFile(const QString& filename, float volume, float fadeVolume, int fadeSeconds)
{
	mAudioFile = filename;
	mSoundVolume = filename.isEmpty() ? -1 : volume;
	if (mSoundVolume >= 0)
	{
		mFadeVolume  = (fadeSeconds > 0) ? fadeVolume : -1;
		mFadeSeconds = (mFadeVolume >= 0) ? fadeSeconds : 0;
	}
	else
	{
		mFadeVolume  = -1;
		mFadeSeconds = 0;
	}
	mUpdated = true;
}

/******************************************************************************
* Change the type of an event.
* If it is being set to archived, set the archived indication in the event ID;
* otherwise, remove the archived indication from the event ID.
*/
void KAEvent::setCategory(KCalEvent::Status s)
{
	if (s == mCategory)
		return;
	mEventID = KCalEvent::uid(mEventID, s);
	mCategory = s;
	mUpdated = true;
}

/******************************************************************************
* Set the event to be an alarm template.
*/
void KAEvent::setTemplate(const QString& name, int afterTime)
{
	setCategory(KCalEvent::TEMPLATE);
	mTemplateName = name;
	mTemplateAfterTime = afterTime;
	mUpdated = true;
	// Templates don't need trigger times to be calculated
	mChangeCount = 0;
	calcTriggerTimes();
}

void KAEvent::setReminder(int minutes, bool onceOnly)
{
	if (minutes != mReminderMinutes)
	{
		set_reminder(minutes);
		mReminderOnceOnly = onceOnly;
		mUpdated          = true;
		calcTriggerTimes();
	}
}

DateTime KAEvent::nextTrigger(TriggerType type) const
{
	switch (type)
	{
		case ALL_TRIGGER:       return mAllTrigger;
		case MAIN_TRIGGER:      return mMainTrigger;
		case ALL_WORK_TRIGGER:  return mAllWorkTrigger;
		case WORK_TRIGGER:      return mMainWorkTrigger;
		case DISPLAY_TRIGGER:   return (mWorkTimeOnly || mExcludeHolidays) ? mMainWorkTrigger : mMainTrigger;
		default:                return DateTime();
	}
}

/******************************************************************************
* Indicate that changes to the instance are complete.
* Recalculate the trigger times if any changes have occurred.
*/
void KAEvent::endChanges()
{
	if (mChangeCount > 0)
		--mChangeCount;
	if (!mChangeCount  &&  mChanged)
		calcTriggerTimes();
}

/******************************************************************************
* Calculate the next trigger times of the alarm.
* This should only be called when changes have actually occurred which might
* affect the trigger times.
* mMainTrigger is set to the next scheduled recurrence/sub-repetition, or the
*              deferral time if a deferral is pending.
* mAllTrigger is the same as mMainTrigger, but takes account of reminders.
* mMainWorkTrigger is set to the next scheduled recurrence/sub-repetition
*                  which occurs in working hours, if working-time-only is set.
* mAllWorkTrigger is the same as mMainWorkTrigger, but takes account of reminders.
*/
void KAEvent::calcTriggerTimes() const
{
	if (mChangeCount)
	{
		mChanged = true;   // note that changes have actually occurred
		return;
	}
	mChanged = false;
	if (!mTemplateName.isEmpty())
	{
		// It's a template
		mAllTrigger = mMainTrigger = mAllWorkTrigger = mMainWorkTrigger = KDateTime();
	}
	else if (mDeferral > 0  &&  mDeferral != REMINDER_DEFERRAL)
	{
		// For a deferred alarm, working time setting is ignored
		mAllTrigger = mMainTrigger = mAllWorkTrigger = mMainWorkTrigger = mDeferralTime;
	}
	else
	{
		mMainTrigger = mainDateTime(true);   // next recurrence or sub-repetition
		// N.B. mReminderMinutes is only set when the reminder is pending -
		// it is cleared once the reminder has triggered.
		mAllTrigger = (mDeferral == REMINDER_DEFERRAL) ? mDeferralTime : mMainTrigger.addMins(-mReminderMinutes);
		// It's not deferred.
		// If only-during-working-time is set and it recurs, it won't actually trigger
		// unless it falls during working hours.
		if ((!mWorkTimeOnly && !mExcludeHolidays)
		||  ((!mRepeatCount || !mRepeatInterval) && checkRecur() == KARecurrence::NO_RECUR)
		||  KAlarm::isWorkingTime(mMainTrigger.kDateTime(), this))
		{
			mMainWorkTrigger = mMainTrigger;
			mAllWorkTrigger = mAllTrigger;
		}
		else if (mWorkTimeOnly)
		{
			// The alarm is restricted to working hours.
			// Finding the next occurrence during working hours can sometimes take a long time,
			// so mark the next actual trigger as invalid until the calculation completes.
			if (!mExcludeHolidays)
			{
				// There are no holiday restrictions.
				calcNextWorkingTime(mMainTrigger);
			}
			else
			{
				// Holidays are excluded.
				const KHolidays& holidays = Preferences::holidays();
				DateTime nextTrigger = mMainTrigger;
				KDateTime kdt;
				for (int i = 0;  i < 20;  ++i)
				{
					calcNextWorkingTime(nextTrigger);
					if (!holidays.isHoliday(mMainWorkTrigger.date()))
						return;   // found a non-holiday occurrence
					kdt = mMainWorkTrigger.effectiveKDateTime();
					kdt.setTime(QTime(23,59,59));
					OccurType type = nextOccurrence(kdt, nextTrigger, RETURN_REPETITION);
					if (!nextTrigger.isValid())
						break;
					if (KAlarm::isWorkingTime(nextTrigger.kDateTime(), this))
					{
						int reminder = mReminderMinutes ? mReminderMinutes : mArchiveReminderMinutes;
						mMainWorkTrigger = nextTrigger;
						mAllWorkTrigger = (type & OCCURRENCE_REPEAT) ? mMainWorkTrigger : mMainWorkTrigger.addMins(-reminder);
						return;   // found a non-holiday occurrence
					}
				}
				mMainWorkTrigger = mAllWorkTrigger = DateTime();
			}
		}
		else if (mExcludeHolidays)
		{
			// Holidays are excluded.
			const KHolidays& holidays = Preferences::holidays();
			DateTime nextTrigger = mMainTrigger;
			KDateTime kdt;
			for (int i = 0;  i < 20;  ++i)
			{
				kdt = nextTrigger.effectiveKDateTime();
				kdt.setTime(QTime(23,59,59));
				OccurType type = nextOccurrence(kdt, nextTrigger, RETURN_REPETITION);
				if (!nextTrigger.isValid())
					break;
				if (!holidays.isHoliday(nextTrigger.date()))
				{
					int reminder = mReminderMinutes ? mReminderMinutes : mArchiveReminderMinutes;
					mMainWorkTrigger = nextTrigger;
					mAllWorkTrigger = (type & OCCURRENCE_REPEAT) ? mMainWorkTrigger : mMainWorkTrigger.addMins(-reminder);
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
	KARecurrence::Type recurType = checkRecur();
	KDateTime kdt = nextTrigger.effectiveKDateTime();
	checkRepetition();   // ensure data consistency
	int reminder = mReminderMinutes ? mReminderMinutes : mArchiveReminderMinutes;
	// Check if it always falls on the same day(s) of the week.
	RecurrenceRule* rrule = mRecurrence->defaultRRuleConst();
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
		if (noWorkPos  &&  !mRepeatCount)
			return;   // never occurs on a working day
	}
	DateTime newdt;

	if (mStartDateTime.isDateOnly())
	{
		// It's a date-only alarm.
		// Sub-repetitions also have to be date-only.
		int repeatFreq = mRepeatInterval.asDays();
		bool weeklyRepeat = mRepeatCount && !(repeatFreq % 7);
		Duration interval = mRecurrence->regularInterval();
		if ((interval  &&  !(interval.asDays() % 7))
		||  nDayPos == 1)
		{
			// It recurs on the same day each week
			if (!mRepeatCount || weeklyRepeat)
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
			for (int repeatNum = mNextRepeat + 1;  ;  ++repeatNum)
			{
				if (repeatNum > mRepeatCount)
					repeatNum = 0;
				if (repeatNum == mNextRepeat)
					break;
				if (!repeatNum)
				{
					nextOccurrence(newdt.kDateTime(), newdt, IGNORE_REPETITION);
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
		if (!mRepeatCount  ||  weeklyRepeat)
		{
			// It's a date-only alarm with either no sub-repetition or a
			// sub-repetition which always falls on the same day of the week
			// as the recurrence (if any).
			unsigned days = 0;
			for ( ; ; )
			{
				kdt.setTime(QTime(23,59,59));
				nextOccurrence(kdt, newdt, IGNORE_REPETITION);
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
		for (int repeatNum = mNextRepeat;  ;  repeatNum = 0)
		{
			while (++repeatNum <= mRepeatCount)
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
			nextOccurrence(kdt, newdt, IGNORE_REPETITION);
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
	bool repeatTimeVaries = (mRepeatCount  &&  !mRepeatInterval.isDaily());

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
			OccurType type = nextOccurrence(kdt, newdt, RETURN_REPETITION);
			if (!newdt.isValid())
				return;
			repetition = (type & OCCURRENCE_REPEAT);
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
		if (mRepeatCount)
		{
			// It's a repetition inside a recurrence, each of which occurs
			// at different times of day (bearing in mind that the repetition
			// may occur at daily intervals after each recurrence).
			// Find the previous recurrence (as opposed to sub-repetition)
			repeatFreq = mRepeatInterval.asSeconds();
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
			if (mRepeatCount)
			{
				// Check the sub-repetitions for this recurrence
				for ( ; ; )
				{
					// Find the repeat count to the next start of the working day
					int inc = subdaily ? nextWorkRepetition(kdt) : 1;
					repeatNum += inc;
					if (repeatNum > mRepeatCount)
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
			nextOccurrence(kdtRecur, newdt, IGNORE_REPETITION);
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
				previousOccurrence(KDateTime(tzTransitions[transitionIndex].time(), KDateTime::UTC), newdt, IGNORE_REPETITION);
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
		int repeatFreq = mRepeatInterval.asSeconds();
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
			nextOccurrence(kdtRecur, newdt, IGNORE_REPETITION);
			KDateTime kdtNextRecur = newdt.effectiveKDateTime();

			int repeatsToCheck = mRepeatCount;
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
						bool pastEnd = (repeatNum > mRepeatCount);
						if (pastEnd)
							inc -= repeatNum - mRepeatCount;
						repeatsToCheck -= inc;
						kdt = kdt.addSecs(inc * repeatFreq);
						if (kdtNextRecur.isValid()  &&  kdt >= kdtNextRecur)
						{
							// This sub-repetition is past the next recurrence,
							// so start the check again from the next recurrence.
							repeatsToCheck = mRepeatCount;
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
				nextOccurrence(kdtRecur, newdt, IGNORE_REPETITION);
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
			previousOccurrence(kdt, newdt, IGNORE_REPETITION);
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
				return mRepeatCount + 1;  // should never happen
			if (Preferences::workDays().testBit((preDay + n) % 7))
			{
				nextWork = nextWork.addDays(n);
				nextWork.setTime(workStart);
				break;
			}
		}
	}
	return (pre.secsTo(nextWork) - 1) / mRepeatInterval.asSeconds() + 1;
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
	Duration interval = mRecurrence->regularInterval();
	if (interval  &&  interval.isDaily()  &&  !(interval.asDays() % 7))
	{
		// It recurs weekly
		if (!mRepeatCount  ||  (mRepeatInterval.isDaily() && !(mRepeatInterval.asDays() % 7)))
			return false;   // any repetitions are also weekly
		// Repetitions are daily. Check if any occur on working days
		// by checking the first recurrence and up to 6 repetitions.
		QBitArray workDays = Preferences::workDays();
		int day = mRecurrence->startDateTime().date().dayOfWeek() - 1;   // Monday = 0
		int repeatDays = mRepeatInterval.asDays();
		int maxRepeat = (mRepeatCount < 6) ? mRepeatCount : 6;
		for (int i = 0;  !workDays.testBit(day);  ++i, day = (day + repeatDays) % 7)
		{
			if (i >= maxRepeat)
				return false;  // no working day occurrences
		}
	}
	return true;
}

int KAEvent::flags() const
{
	if (mSpeak)
		const_cast<KAEvent*>(this)->mBeep = false;
	return KAAlarmEventBase::flags()
	     | (mStartDateTime.isDateOnly() ? ANY_TIME : 0)
	     | (mDeferral > 0               ? DEFERRAL : 0)
	     | (mSpeak                      ? SPEAK : 0)
	     | (mCommandXterm               ? EXEC_IN_XTERM : 0)
	     | (mCommandDisplay             ? DISPLAY_COMMAND : 0)
	     | (mCopyToKOrganizer           ? COPY_KORGANIZER : 0)
	     | (mExcludeHolidays            ? EXCL_HOLIDAYS : 0)
	     | (mWorkTimeOnly               ? WORK_TIME_ONLY : 0)
	     | (mEnabled                    ? 0 : DISABLED);
}

/******************************************************************************
 * Update an existing KCal::Event with the KAEvent data.
 * If 'original' is true, the event start date/time is adjusted to its original
 * value instead of its next occurrence, and the expired main alarm is
 * reinstated.
 */
bool KAEvent::updateKCalEvent(Event* ev, bool checkUid, bool original) const
{
	if (!ev
	||  (checkUid  &&  !mEventID.isEmpty()  &&  mEventID != ev->uid())
	||  (!mAlarmCount  &&  (!original || !mMainExpired)))
		return false;

	ev->startUpdates();   // prevent multiple update notifications
	checkRecur();         // ensure recurrence/repetition data is consistent
	bool readOnly = ev->isReadOnly();
	ev->setReadOnly(false);
	ev->setTransparency(Event::Transparent);

	// Set up event-specific data

	// Set up custom properties.
	ev->removeCustomProperty(KCalendar::APPNAME, FLAGS_PROPERTY);
	ev->removeCustomProperty(KCalendar::APPNAME, NEXT_RECUR_PROPERTY);
	ev->removeCustomProperty(KCalendar::APPNAME, REPEAT_PROPERTY);
	ev->removeCustomProperty(KCalendar::APPNAME, LOG_PROPERTY);
	ev->removeCustomProperty(KCalendar::APPNAME, ARCHIVE_PROPERTY);

	QString param;
	if (mCategory == KCalEvent::DISPLAYING)
	{
		param = mResourceId;
		if (mDisplayingDefer)
			param += SC + DISP_DEFER;
		if (mDisplayingEdit)
			param += SC + DISP_EDIT;
	}
	KCalEvent::setStatus(ev, mCategory, param);
	QStringList flags;
	if (mStartDateTime.isDateOnly())
		flags += DATE_ONLY_FLAG;
	if (mConfirmAck)
		flags += CONFIRM_ACK_FLAG;
	if (mEmailBcc)
		flags += EMAIL_BCC_FLAG;
	if (mCopyToKOrganizer)
		flags += KORGANIZER_FLAG;
	if (mExcludeHolidays)
		flags += EXCLUDE_HOLIDAYS_FLAG;
	if (mWorkTimeOnly)
		flags += WORK_TIME_ONLY_FLAG;
	if (mLateCancel)
		(flags += (mAutoClose ? AUTO_CLOSE_FLAG : LATE_CANCEL_FLAG)) += QString::number(mLateCancel);
	if (mDeferDefaultMinutes)
		(flags += DEFER_FLAG) += QString::number(mDeferDefaultMinutes);
	if (!mTemplateName.isEmpty()  &&  mTemplateAfterTime >= 0)
		(flags += TEMPL_AFTER_TIME_FLAG) += QString::number(mTemplateAfterTime);
	if (mKMailSerialNumber)
		(flags += KMAIL_SERNUM_FLAG) += QString::number(mKMailSerialNumber);
	if (!flags.isEmpty())
		ev->setCustomProperty(KCalendar::APPNAME, FLAGS_PROPERTY, flags.join(SC));

	if (mCommandXterm)
		ev->setCustomProperty(KCalendar::APPNAME, LOG_PROPERTY, xtermURL);
	else if (mCommandDisplay)
		ev->setCustomProperty(KCalendar::APPNAME, LOG_PROPERTY, displayURL);
	else if (!mLogFile.isEmpty())
		ev->setCustomProperty(KCalendar::APPNAME, LOG_PROPERTY, mLogFile);
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
		QString param;
		if (params.count() > 0)
			param = params.join(SC);
		else
			param = QLatin1String("0");
		ev->setCustomProperty(KCalendar::APPNAME, ARCHIVE_PROPERTY, param);
	}

	ev->setCustomStatus(mEnabled ? QString() : DISABLED_STATUS);
	ev->setRevision(mRevision);
	ev->clearAlarms();

	/* Always set DTSTART as date/time, and use the category "DATE" to indicate
	 * a date-only event, instead of calling setAllDay(). This is necessary to
	 * allow the alarm to float within the 24-hour period defined by the
	 * start-of-day time rather than midnight to midnight.
	 * RFC2445 states that alarm trigger times specified in absolute terms
	 * (rather than relative to DTSTART or DTEND) can only be specified as a
	 * UTC DATE-TIME value. So always use a time relative to DTSTART instead of
	 * an absolute time.
	 */
	ev->setDtStart(mStartDateTime.effectiveKDateTime());
	ev->setAllDay(false);
	ev->setHasEndDate(false);

	DateTime dtMain = original ? mStartDateTime : mNextMainDateTime;
	int      ancillaryType = 0;   // 0 = invalid, 1 = time, 2 = offset
	DateTime ancillaryTime;       // time for ancillary alarms (audio, pre-action, etc)
	int      ancillaryOffset = 0; // start offset for ancillary alarms
	if (!mMainExpired  ||  original)
	{
		/* The alarm offset must always be zero for the main alarm. To determine
		 * which recurrence is due, the property X-KDE-KALARM_NEXTRECUR is used.
		 * If the alarm offset was non-zero, exception dates and rules would not
		 * work since they apply to the event time, not the alarm time.
		 */
		if (!original  &&  checkRecur() != KARecurrence::NO_RECUR)
		{
			QDateTime dt = mNextMainDateTime.kDateTime().toTimeSpec(mStartDateTime.timeSpec()).dateTime();
			ev->setCustomProperty(KCalendar::APPNAME, NEXT_RECUR_PROPERTY,
			                      dt.toString(mNextMainDateTime.isDateOnly() ? "yyyyMMdd" : "yyyyMMddThhmmss"));
		}
		// Add the main alarm
		initKCalAlarm(ev, 0, QStringList(), KAAlarm::MAIN_ALARM);
		ancillaryOffset = 0;
		ancillaryType = dtMain.isValid() ? 2 : 0;
	}
	else if (mRepeatCount  &&  mRepeatInterval)
	{
		// Alarm repetition is normally held in the main alarm, but since
		// the main alarm has expired, store in a custom property.
		QString param = QString("%1:%2").arg(mRepeatInterval.asSeconds() / 60).arg(mRepeatCount);
		ev->setCustomProperty(KCalendar::APPNAME, REPEAT_PROPERTY, param);
	}

	// Add subsidiary alarms
	if (mRepeatAtLogin  ||  (mArchiveRepeatAtLogin && original))
	{
		DateTime dtl;
		if (mArchiveRepeatAtLogin)
			dtl = mStartDateTime.effectiveKDateTime().addDays(-1);
		else if (mAtLoginDateTime.isValid())
			dtl = mAtLoginDateTime;
		else if (mStartDateTime.isDateOnly())
			dtl = DateTime(QDate::currentDate().addDays(-1), mStartDateTime.timeSpec());
		else
			dtl = KDateTime::currentUtcDateTime();
		initKCalAlarm(ev, dtl, QStringList(AT_LOGIN_TYPE));
		if (!ancillaryType  &&  dtl.isValid())
		{
			ancillaryTime = dtl;
			ancillaryType = 1;
		}
	}
	if (mReminderMinutes  ||  (mArchiveReminderMinutes && original))
	{
		int minutes = mReminderMinutes ? mReminderMinutes : mArchiveReminderMinutes;
		initKCalAlarm(ev, -minutes * 60, QStringList(mReminderOnceOnly ? REMINDER_ONCE_TYPE : REMINDER_TYPE));
		if (!ancillaryType)
		{
			ancillaryOffset = -minutes * 60;
			ancillaryType = 2;
		}
	}
	if (mDeferral > 0)
	{
		DateTime nextDateTime = mNextMainDateTime;
		if (mMainExpired)
		{
			if (checkRecur() == KARecurrence::NO_RECUR)
				nextDateTime = mStartDateTime;
			else if (!original)
			{
				// It's a deferral of an expired recurrence.
				// Need to ensure that the alarm offset is to an occurrence
				// which isn't excluded by an exception - otherwise, it will
				// never be triggered. So choose the first recurrence which
				// isn't an exception.
				KDateTime dt = mRecurrence->getNextDateTime(mStartDateTime.addDays(-1).kDateTime());
				dt.setDateOnly(mStartDateTime.isDateOnly());
				nextDateTime = dt;
			}
		}
		int startOffset;
		QStringList list;
		if (mDeferralTime.isDateOnly())
		{
			startOffset = nextDateTime.secsTo(mDeferralTime.effectiveKDateTime());
			list += DATE_DEFERRAL_TYPE;
		}
		else
		{
			startOffset = nextDateTime.effectiveKDateTime().secsTo(mDeferralTime.effectiveKDateTime());
			list += TIME_DEFERRAL_TYPE;
		}
		if (mDeferral == REMINDER_DEFERRAL)
			list += mReminderOnceOnly ? REMINDER_ONCE_TYPE : REMINDER_TYPE;
		initKCalAlarm(ev, startOffset, list);
		if (!ancillaryType  &&  mDeferralTime.isValid())
		{
			ancillaryOffset = startOffset;
			ancillaryType = 2;
		}
	}
	if (!mTemplateName.isEmpty())
		ev->setSummary(mTemplateName);
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
		initKCalAlarm(ev, mDisplayingTime, list);
		if (!ancillaryType  &&  mDisplayingTime.isValid())
		{
			ancillaryTime = mDisplayingTime;
			ancillaryType = 1;
		}
	}
	if (mBeep  ||  mSpeak  ||  !mAudioFile.isEmpty())
	{
		// A sound is specified
		if (ancillaryType == 2)
			initKCalAlarm(ev, ancillaryOffset, QStringList(), KAAlarm::AUDIO_ALARM);
		else
			initKCalAlarm(ev, ancillaryTime, QStringList(), KAAlarm::AUDIO_ALARM);
	}
	if (!mPreAction.isEmpty())
	{
		// A pre-display action is specified
		if (ancillaryType == 2)
			initKCalAlarm(ev, ancillaryOffset, QStringList(PRE_ACTION_TYPE), KAAlarm::PRE_ACTION_ALARM);
		else
			initKCalAlarm(ev, ancillaryTime, QStringList(PRE_ACTION_TYPE), KAAlarm::PRE_ACTION_ALARM);
	}
	if (!mPostAction.isEmpty())
	{
		// A post-display action is specified
		if (ancillaryType == 2)
			initKCalAlarm(ev, ancillaryOffset, QStringList(POST_ACTION_TYPE), KAAlarm::POST_ACTION_ALARM);
		else
			initKCalAlarm(ev, ancillaryTime, QStringList(POST_ACTION_TYPE), KAAlarm::POST_ACTION_ALARM);
	}

	if (mRecurrence)
		mRecurrence->writeRecurrence(*ev->recurrence());
	else
		ev->clearRecurrence();
	if (mSaveDateTime.isValid())
		ev->setCreated(mSaveDateTime);
	ev->setReadOnly(readOnly);
	ev->endUpdates();     // finally issue an update notification
	return true;
}

/******************************************************************************
 * Create a new alarm for a libkcal event, and initialise it according to the
 * alarm action. If 'types' is non-null, it is appended to the X-KDE-KALARM-TYPE
 * property value list.
 */
Alarm* KAEvent::initKCalAlarm(Event* event, const DateTime& dt, const QStringList& types, KAAlarm::Type type) const
{
	int startOffset = dt.isDateOnly() ? mStartDateTime.secsTo(dt)
	                                  : mStartDateTime.effectiveKDateTime().secsTo(dt.effectiveKDateTime());
	return initKCalAlarm(event, startOffset, types, type);
}

Alarm* KAEvent::initKCalAlarm(Event* event, int startOffsetSecs, const QStringList& types, KAAlarm::Type type) const
{
	QStringList alltypes;
	Alarm* alarm = event->newAlarm();
	alarm->setEnabled(true);
	if (type != KAAlarm::MAIN_ALARM)
	{
		// RFC2445 specifies that absolute alarm times must be stored as a UTC DATE-TIME value.
		// Set the alarm time as an offset to DTSTART for the reasons described in updateKCalEvent().
		alarm->setStartOffset(startOffsetSecs);
	}

	switch (type)
	{
		case KAAlarm::AUDIO_ALARM:
			alarm->setAudioAlarm(mAudioFile);  // empty for a beep or for speaking
			if (mSpeak)
				alarm->setCustomProperty(KCalendar::APPNAME, SPEAK_PROPERTY, QLatin1String("Y"));
			if (mRepeatSound)
			{
				alarm->setRepeatCount(-1);
				alarm->setSnoozeTime(0);
			}
			if (!mAudioFile.isEmpty()  &&  mSoundVolume >= 0)
				alarm->setCustomProperty(KCalendar::APPNAME, VOLUME_PROPERTY,
				              QString::fromLatin1("%1;%2;%3").arg(QString::number(mSoundVolume, 'f', 2))
				                                             .arg(QString::number(mFadeVolume, 'f', 2))
				                                             .arg(mFadeSeconds));
			break;
		case KAAlarm::PRE_ACTION_ALARM:
			setProcedureAlarm(alarm, mPreAction);
			if (mCancelOnPreActErr)
				alarm->setCustomProperty(KCalendar::APPNAME, CANCEL_ON_ERROR_PROPERTY, QLatin1String("Y"));
			break;
		case KAAlarm::POST_ACTION_ALARM:
			setProcedureAlarm(alarm, mPostAction);
			break;
		case KAAlarm::MAIN_ALARM:
			alarm->setSnoozeTime(mRepeatInterval);
			alarm->setRepeatCount(mRepeatCount);
			if (mRepeatCount)
				alarm->setCustomProperty(KCalendar::APPNAME, NEXT_REPEAT_PROPERTY,
				                         QString::number(mNextRepeat));
			// fall through to INVALID_ALARM
		case KAAlarm::INVALID_ALARM:
		{
			bool display = false;
			switch (mActionType)
			{
				case T_FILE:
					alltypes += FILE_TYPE;
					// fall through to T_MESSAGE
				case T_MESSAGE:
					alarm->setDisplayAlarm(AlarmText::toCalendarText(mText));
					display = true;
					break;
				case T_COMMAND:
					if (mCommandScript)
						alarm->setProcedureAlarm("", mText);
					else
						setProcedureAlarm(alarm, mText);
					display = mCommandDisplay;
					break;
				case T_EMAIL:
					alarm->setEmailAlarm(mEmailSubject, mText, mEmailAddresses, mEmailAttachments);
					if (mEmailFromIdentity)
						alarm->setCustomProperty(KCalendar::APPNAME, EMAIL_ID_PROPERTY, QString::number(mEmailFromIdentity));
					break;
				case T_AUDIO:
					break;
			}
			if (display)
				alarm->setCustomProperty(KCalendar::APPNAME, FONT_COLOUR_PROPERTY,
				                         QString::fromLatin1("%1;%2;%3").arg(mBgColour.name())
				                                                        .arg(mFgColour.name())
				                                                        .arg(mDefaultFont ? QString() : mFont.toString()));
			break;
		}
		case KAAlarm::REMINDER_ALARM:
		case KAAlarm::DEFERRED_ALARM:
		case KAAlarm::DEFERRED_REMINDER_ALARM:
		case KAAlarm::AT_LOGIN_ALARM:
		case KAAlarm::DISPLAYING_ALARM:
			break;
	}
	alltypes += types;
	if (alltypes.count() > 0)
		alarm->setCustomProperty(KCalendar::APPNAME, TYPE_PROPERTY, alltypes.join(","));
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
		al.mEventID        = mEventID;
		al.mActionType     = mActionType;
		al.mText           = mText;
		al.mBgColour       = mBgColour;
		al.mFgColour       = mFgColour;
		al.mFont           = mFont;
		al.mDefaultFont    = mDefaultFont;
		al.mBeep           = mBeep;
		al.mSoundVolume    = mSoundVolume;
		al.mFadeVolume     = mFadeVolume;
		al.mFadeSeconds    = mFadeSeconds;
		al.mRepeatSound    = mRepeatSound;
		al.mConfirmAck     = mConfirmAck;
		al.mRepeatCount    = 0;
		al.mRepeatInterval = 0;
		al.mRepeatAtLogin  = false;
		al.mDeferred       = false;
		al.mLateCancel     = mLateCancel;
		al.mAutoClose      = mAutoClose;
		al.mEmailBcc       = mEmailBcc;
		al.mCommandScript  = mCommandScript;
		if (mActionType == T_EMAIL)
		{
			al.mEmailFromIdentity = mEmailFromIdentity;
			al.mEmailAddresses    = mEmailAddresses;
			al.mEmailSubject      = mEmailSubject;
			al.mEmailAttachments  = mEmailAttachments;
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
					al.mNextRepeat       = mNextRepeat;
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
			// and for restoration after the next recurrence.
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
	{
		mUpdated = true;
		calcTriggerTimes();
	}
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
	startChanges();   // prevent multiple trigger time evaluation here
	bool result = false;
	bool setNextRepetition = false;
	bool checkRepetition = false;
	if (checkRecur() == KARecurrence::NO_RECUR)
	{
		if (mReminderMinutes  ||  mDeferral == REMINDER_DEFERRAL  ||  mArchiveReminderMinutes)
		{
			if (dateTime < mNextMainDateTime.effectiveKDateTime())
			{
				set_deferral(REMINDER_DEFERRAL);   // defer reminder alarm
				mDeferralTime = dateTime;
				mChanged = true;
			}
			else
			{
				// Deferring past the main alarm time, so adjust any existing deferral
				if (mReminderMinutes  ||  mDeferral == REMINDER_DEFERRAL)
				{
					set_deferral(NO_DEFERRAL);
					mChanged = true;
				}
			}
			// Remove any reminder alarm, but keep a note of it for archiving purposes
			// and for restoration after the next recurrence.
			if (mReminderMinutes)
			{
				set_archiveReminder();
				mChanged = true;
			}
		}
		if (mDeferral != REMINDER_DEFERRAL)
		{
			// We're deferring the main alarm, not a reminder
			if (mRepeatCount && mRepeatInterval  &&  dateTime < mainEndRepeatTime())
			{
				// The alarm is repeated, and we're deferring to a time before the last repetition
				set_deferral(NORMAL_DEFERRAL);
				mDeferralTime = dateTime;
				result = true;
				mChanged = true;
				setNextRepetition = true;
			}
			else
			{
				// Main alarm has now expired
				mNextMainDateTime = mDeferralTime = dateTime;
				set_deferral(NORMAL_DEFERRAL);
				mChanged = true;
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
		if (dateTime >= mNextMainDateTime.effectiveKDateTime())
			set_deferral(NO_DEFERRAL);    // (error)
		else
		{
			set_deferral(REMINDER_DEFERRAL);
			mDeferralTime = dateTime;
			checkRepetition = true;
		}
		mChanged = true;
	}
	else
	{
		mDeferralTime = dateTime;
		mChanged = true;
		if (mDeferral <= 0)
			set_deferral(NORMAL_DEFERRAL);
		if (adjustRecurrence)
		{
			KDateTime now = KDateTime::currentUtcDateTime();
			if (mainEndRepeatTime() < now)
			{
				// The last repetition (if any) of the current recurrence has already passed.
				// Adjust to the next scheduled recurrence after now.
				if (!mMainExpired  &&  setNextOccurrence(now) == NO_OCCURRENCE)
				{
					mMainExpired = true;
					--mAlarmCount;
				}
			}
			else
				setNextRepetition = (mRepeatCount && mRepeatInterval);
		}
		else
			checkRepetition = true;
	}
	if (checkRepetition)
		setNextRepetition = (mRepeatCount && mRepeatInterval  &&  mDeferralTime < mainEndRepeatTime());
	if (setNextRepetition)
	{
		// The alarm is repeated, and we're deferring to a time before the last repetition.
		// Set the next scheduled repetition to the one after the deferral.
		if (mNextMainDateTime >= mDeferralTime)
			mNextRepeat = 0;
		else if (mRepeatInterval.isDaily())
			mNextRepeat = mNextMainDateTime.daysTo(mDeferralTime) / mRepeatInterval.asDays() + 1;
		else
			mNextRepeat = mNextMainDateTime.secsTo(mDeferralTime) / mRepeatInterval.asSeconds() + 1;
		mChanged = true;
	}
	mUpdated = true;
	endChanges();
	return result;
}

/******************************************************************************
 * Cancel any deferral alarm.
 */
void KAEvent::cancelDefer()
{
	if (mDeferral > 0)
	{
		mDeferralTime = DateTime();
		set_deferral(NO_DEFERRAL);
		mUpdated = true;
		calcTriggerTimes();
	}
}

/******************************************************************************
*  Find the latest time which the alarm can currently be deferred to.
*/
DateTime KAEvent::deferralLimit(KAEvent::DeferLimitType* limitType) const
{
	DeferLimitType ltype;
	DateTime endTime;
	bool recurs = (checkRecur() != KARecurrence::NO_RECUR);
	if (recurs  ||  mRepeatCount)
	{
		// It's a repeated alarm. Don't allow it to be deferred past its
		// next occurrence or repetition.
		DateTime reminderTime;
		KDateTime now = KDateTime::currentUtcDateTime();
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
		else if (type == FIRST_OR_ONLY_OCCURRENCE  &&  !recurs)
			ltype = LIMIT_REPETITION;
		else
			ltype = LIMIT_RECURRENCE;
	}
	else if ((mReminderMinutes  ||  mDeferral == REMINDER_DEFERRAL  ||  mArchiveReminderMinutes)
		 &&  KDateTime::currentUtcDateTime() < mNextMainDateTime.effectiveKDateTime())
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
bool KAEvent::setDisplaying(const KAEvent& event, KAAlarm::Type alarmType, const QString& resourceID, const KDateTime& repeatAtLoginTime,
                            bool showEdit, bool showDefer)
{
	if (!mDisplaying
	&&  (alarmType == KAAlarm::MAIN_ALARM
	  || alarmType == KAAlarm::REMINDER_ALARM
	  || alarmType == KAAlarm::DEFERRED_REMINDER_ALARM
	  || alarmType == KAAlarm::DEFERRED_ALARM
	  || alarmType == KAAlarm::AT_LOGIN_ALARM))
	{
//kDebug()<<event.id()<<","<<(alarmType==KAAlarm::MAIN_ALARM?"MAIN":alarmType==KAAlarm::REMINDER_ALARM?"REMINDER":alarmType==KAAlarm::DEFERRED_REMINDER_ALARM?"REMINDER_DEFERRAL":alarmType==KAAlarm::DEFERRED_ALARM?"DEFERRAL":"LOGIN")<<"): time="<<repeatAtLoginTime.toString();
		KAAlarm al = event.alarm(alarmType);
		if (al.valid())
		{
			*this = event;
			// Change the event ID to avoid duplicating the same unique ID as the original event
			setCategory(KCalEvent::DISPLAYING);
			mResourceId      = resourceID;
			mDisplayingDefer = showDefer;
			mDisplayingEdit  = showEdit;
			mDisplaying      = true;
			mDisplayingTime  = (alarmType == KAAlarm::AT_LOGIN_ALARM) ? repeatAtLoginTime : al.dateTime().kDateTime();
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
			al.mType = (mDisplayingFlags == (REMINDER | DATE_DEFERRAL)) ? KAAlarm::DEFERRED_REMINDER_DATE__ALARM
			         : (mDisplayingFlags == (REMINDER | TIME_DEFERRAL)) ? KAAlarm::DEFERRED_REMINDER_TIME__ALARM
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
void KAEvent::reinstateFromDisplaying(const Event* kcalEvent, QString& resourceID, bool& showEdit, bool& showDefer)
{
	set(kcalEvent);
	if (mDisplaying)
	{
		// Retrieve the original event's unique ID
		setCategory(KCalEvent::ACTIVE);
		resourceID  = mResourceId;
		showDefer   = mDisplayingDefer;
		showEdit    = mDisplayingEdit;
		mDisplaying = false;
		--mAlarmCount;
		mUpdated = true;
	}
}

/******************************************************************************
 * Determine whether the event will occur after the specified date/time.
 * If 'includeRepetitions' is true and the alarm has a sub-repetition, it
 * returns true if any repetitions occur after the specified date/time.
 */
bool KAEvent::occursAfter(const KDateTime& preDateTime, bool includeRepetitions) const
{
	KDateTime dt;
	if (checkRecur() != KARecurrence::NO_RECUR)
	{
		if (mRecurrence->duration() < 0)
			return true;    // infinite recurrence
		dt = mRecurrence->endDateTime();
	}
	else
		dt = mNextMainDateTime.effectiveKDateTime();
	if (mStartDateTime.isDateOnly())
	{
		QDate pre = preDateTime.date();
		if (preDateTime.time() < Preferences::startOfDay())
			pre = pre.addDays(-1);    // today's recurrence (if today recurs) is still to come
		if (pre < dt.date())
			return true;
	}
	else if (preDateTime < dt)
		return true;

	if (includeRepetitions  &&  mRepeatCount)
	{
		if (preDateTime < (mRepeatInterval * mRepeatCount).end(dt))
			return true;
	}
	return false;
}

/******************************************************************************
 * Get the date/time of the next occurrence of the event, after the specified
 * date/time.
 * 'result' = date/time of next occurrence, or invalid date/time if none.
 */
KAEvent::OccurType KAEvent::nextOccurrence(const KDateTime& preDateTime, DateTime& result,
                                           KAEvent::OccurOption includeRepetitions) const
{
	KDateTime pre = preDateTime;
	if (includeRepetitions != IGNORE_REPETITION)
	{                   // RETURN_REPETITION or ALLOW_FOR_REPETITION
		if (!mRepeatCount  ||  !mRepeatInterval)
			includeRepetitions = IGNORE_REPETITION;
		else
			pre = (mRepeatInterval * -mRepeatCount).end(preDateTime);
	}

	OccurType type;
	bool recurs = (checkRecur() != KARecurrence::NO_RECUR);
	if (recurs)
		type = nextRecurrence(pre, result);
	else if (pre < mNextMainDateTime.effectiveKDateTime())
	{
		result = mNextMainDateTime;
		type = FIRST_OR_ONLY_OCCURRENCE;
	}
	else
	{
		result = DateTime();
		type = NO_OCCURRENCE;
	}

	if (type != NO_OCCURRENCE  &&  result <= preDateTime  &&  includeRepetitions != IGNORE_REPETITION)
	{                   // RETURN_REPETITION or ALLOW_FOR_REPETITION
		// The next occurrence is a sub-repetition
		int repetition = mRepeatInterval.isDaily()
		               ? result.daysTo(preDateTime) / mRepeatInterval.asDays() + 1
		               : result.secsTo(preDateTime) / mRepeatInterval.asSeconds() + 1;
		DateTime repeatDT = (mRepeatInterval * repetition).end(result.kDateTime());
		if (recurs)
		{
			// We've found a recurrence before the specified date/time, which has
			// a sub-repetition after the date/time.
			// However, if the intervals between recurrences vary, we could possibly
			// have missed a later recurrence which fits the criterion, so check again.
			DateTime dt;
			OccurType newType = previousOccurrence(repeatDT.effectiveKDateTime(), dt, false);
			if (dt > result)
			{
				type = newType;
				result = dt;
				if (includeRepetitions == RETURN_REPETITION  &&  result <= preDateTime)
				{
					// The next occurrence is a sub-repetition
					int repetition = mRepeatInterval.isDaily()
					               ? result.daysTo(preDateTime) / mRepeatInterval.asDays() + 1
					               : result.secsTo(preDateTime) / mRepeatInterval.asSeconds() + 1;
					result = (mRepeatInterval * repetition).end(result.kDateTime());
					type = static_cast<OccurType>(type | OCCURRENCE_REPEAT);
				}
				return type;
			}
		}
		if (includeRepetitions == RETURN_REPETITION)
		{
			// The next occurrence is a sub-repetition
			result = repeatDT;
			type = static_cast<OccurType>(type | OCCURRENCE_REPEAT);
		}
	}
	return type;
}

/******************************************************************************
 * Get the date/time of the last previous occurrence of the event, before the
 * specified date/time.
 * If 'includeRepetitions' is true and the alarm has a sub-repetition, the
 * last previous repetition is returned if appropriate.
 * 'result' = date/time of previous occurrence, or invalid date/time if none.
 */
KAEvent::OccurType KAEvent::previousOccurrence(const KDateTime& afterDateTime, DateTime& result, bool includeRepetitions) const
{
	Q_ASSERT(!afterDateTime.isDateOnly());
	if (mStartDateTime >= afterDateTime)
	{
		result = KDateTime();
		return NO_OCCURRENCE;     // the event starts after the specified date/time
	}

	// Find the latest recurrence of the event
	OccurType type;
	if (checkRecur() == KARecurrence::NO_RECUR)
	{
		result = mStartDateTime;
		type = FIRST_OR_ONLY_OCCURRENCE;
	}
	else
	{
		KDateTime recurStart = mRecurrence->startDateTime();
		KDateTime after = afterDateTime;
		if (mStartDateTime.isDateOnly()  &&  afterDateTime.time() > Preferences::startOfDay())
			after = after.addDays(1);    // today's recurrence (if today recurs) has passed
		KDateTime dt = mRecurrence->getPreviousDateTime(after);
		result = dt;
		result.setDateOnly(mStartDateTime.isDateOnly());
		if (!dt.isValid())
			return NO_OCCURRENCE;
		if (dt == recurStart)
			type = FIRST_OR_ONLY_OCCURRENCE;
		else if (mRecurrence->getNextDateTime(dt).isValid())
			type = result.isDateOnly() ? RECURRENCE_DATE : RECURRENCE_DATE_TIME;
		else
			type = LAST_RECURRENCE;
	}

	if (includeRepetitions  &&  mRepeatCount)
	{
		// Find the latest repetition which is before the specified time.
		int repetition = mRepeatInterval.isDaily()
		               ? result.effectiveKDateTime().daysTo(afterDateTime.addSecs(-1)) / mRepeatInterval.asDays()
		               : static_cast<int>((result.effectiveKDateTime().secsTo_long(afterDateTime) - 1) / mRepeatInterval.asSeconds());
		if (repetition > 0)
		{
			result = (mRepeatInterval * qMin(repetition, mRepeatCount)).end(result.kDateTime());
			return static_cast<OccurType>(type | OCCURRENCE_REPEAT);
		}
	}
	return type;
}

/******************************************************************************
 * Set the date/time of the event to the next scheduled occurrence after the
 * specified date/time, provided that this is later than its current date/time.
 * Any reminder alarm is adjusted accordingly.
 * If the alarm has a sub-repetition, and a repetition of a previous recurrence
 * occurs after the specified date/time, that repetition is set as the next
 * occurrence.
 */
KAEvent::OccurType KAEvent::setNextOccurrence(const KDateTime& preDateTime)
{
	if (preDateTime < mNextMainDateTime.effectiveKDateTime())
		return FIRST_OR_ONLY_OCCURRENCE;    // it might not be the first recurrence - tant pis
	KDateTime pre = preDateTime;
	// If there are repetitions, adjust the comparison date/time so that
	// we find the earliest recurrence which has a repetition falling after
	// the specified preDateTime.
	if (mRepeatCount  &&  mRepeatInterval)
		pre = (mRepeatInterval * -mRepeatCount).end(preDateTime);

	DateTime dt;
	OccurType type;
	bool changed = false;
	if (pre < mNextMainDateTime.effectiveKDateTime())
	{
		dt = mNextMainDateTime;
		type = FIRST_OR_ONLY_OCCURRENCE;   // may not actually be the first occurrence
	}
	else if (checkRecur() != KARecurrence::NO_RECUR)
	{
		type = nextRecurrence(pre, dt);
		if (type == NO_OCCURRENCE)
			return NO_OCCURRENCE;
		if (type != FIRST_OR_ONLY_OCCURRENCE  &&  dt != mNextMainDateTime)
		{
			// Need to reschedule the next trigger date/time
			mNextMainDateTime = dt;
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
			changed = true;
		}
	}
	else
		return NO_OCCURRENCE;

	if (mRepeatCount  &&  mRepeatInterval)
	{
		if (dt <= preDateTime)
		{
			// The next occurrence is a sub-repetition.
			type = static_cast<OccurType>(type | OCCURRENCE_REPEAT);
			mNextRepeat = mRepeatInterval.isDaily()
			            ? dt.effectiveKDateTime().daysTo(preDateTime) / mRepeatInterval.asDays() + 1
			            : static_cast<int>(dt.effectiveKDateTime().secsTo_long(preDateTime) / mRepeatInterval.asSeconds()) + 1;
			// Repetitions can't have a reminder, so remove any.
			if (mReminderMinutes)
				set_archiveReminder();
			if (mDeferral == REMINDER_DEFERRAL)
				set_deferral(NO_DEFERRAL);
			changed = true;
		}
		else if (mNextRepeat)
		{
			// The next occurrence is the main occurrence, not a repetition
			mNextRepeat = 0;
			changed = true;
		}
	}
	if (changed)
	{
		mUpdated = true;
		calcTriggerTimes();
	}
	return type;
}

/******************************************************************************
 * Get the date/time of the next recurrence of the event, after the specified
 * date/time.
 * 'result' = date/time of next occurrence, or invalid date/time if none.
 */
KAEvent::OccurType KAEvent::nextRecurrence(const KDateTime& preDateTime, DateTime& result) const
{
	KDateTime recurStart = mRecurrence->startDateTime();
	KDateTime pre = preDateTime;
	if (mStartDateTime.isDateOnly()  &&  !pre.isDateOnly()  &&  pre.time() < Preferences::startOfDay())
	{
		pre = pre.addDays(-1);    // today's recurrence (if today recurs) is still to come
		pre.setTime(Preferences::startOfDay());
	}
	KDateTime dt = mRecurrence->getNextDateTime(pre);
	result = dt;
	result.setDateOnly(mStartDateTime.isDateOnly());
	if (!dt.isValid())
		return NO_OCCURRENCE;
	if (dt == recurStart)
		return FIRST_OR_ONLY_OCCURRENCE;
	if (mRecurrence->duration() >= 0  &&  dt == mRecurrence->endDateTime())
		return LAST_RECURRENCE;
	return result.isDateOnly() ? RECURRENCE_DATE : RECURRENCE_DATE_TIME;
}

/******************************************************************************
 * Return the recurrence interval as text suitable for display.
 */
QString KAEvent::recurrenceText(bool brief) const
{
	if (mRepeatAtLogin)
		return brief ? i18nc("@info/plain Brief form of 'At Login'", "Login") : i18nc("@info/plain", "At login");
	if (mRecurrence)
	{
		int frequency = mRecurrence->frequency();
		switch (mRecurrence->defaultRRuleConst()->recurrenceType())
		{
			case RecurrenceRule::rMinutely:
				if (frequency < 60)
					return i18ncp("@info/plain", "1 Minute", "%1 Minutes", frequency);
				else if (frequency % 60 == 0)
					return i18ncp("@info/plain", "1 Hour", "%1 Hours", frequency/60);
				else
				{
					QString mins;
					return i18nc("@info/plain Hours and minutes", "%1h %2m", frequency/60, mins.sprintf("%02d", frequency%60));
				}
			case RecurrenceRule::rDaily:
				return i18ncp("@info/plain", "1 Day", "%1 Days", frequency);
			case RecurrenceRule::rWeekly:
				return i18ncp("@info/plain", "1 Week", "%1 Weeks", frequency);
			case RecurrenceRule::rMonthly:
				return i18ncp("@info/plain", "1 Month", "%1 Months", frequency);
			case RecurrenceRule::rYearly:
				return i18ncp("@info/plain", "1 Year", "%1 Years", frequency);
			case RecurrenceRule::rNone:
			default:
				break;
		}
	}
	return brief ? QString() : i18nc("@info/plain No recurrence", "None");
}

/******************************************************************************
 * Return the repetition interval as text suitable for display.
 */
QString KAEvent::repetitionText(bool brief) const
{
	if (mRepeatCount)
	{
#ifdef __GNUC__
#warning Get 24 hours displayed with daily repetition
#endif
		if (!mRepeatInterval.isDaily())
		{
			int minutes = mRepeatInterval.asSeconds() / 60;
			if (minutes < 60)
				return i18ncp("@info/plain", "1 Minute", "%1 Minutes", minutes);
			if (minutes % 60 == 0)
				return i18ncp("@info/plain", "1 Hour", "%1 Hours", minutes/60);
			QString mins;
			return i18nc("@info/plain Hours and minutes", "%1h %2m", minutes/60, mins.sprintf("%02d", minutes%60));
		}
		int days = mRepeatInterval.asDays();
		if (days % 7)
			return i18ncp("@info/plain", "1 Day", "%1 Days", days);
		return i18ncp("@info/plain", "1 Week", "%1 Weeks", days / 7);
	}
	return brief ? QString() : i18nc("@info/plain No repetition", "None");
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
		case KARecurrence::NO_RECUR:
		case KARecurrence::MINUTELY:
			return;
		case KARecurrence::ANNUAL_DATE:
		case KARecurrence::ANNUAL_POS:
			if (mRecurrence->yearMonths().isEmpty())
				return;    // (presumably it's a template)
			break;
		case KARecurrence::DAILY:
		case KARecurrence::WEEKLY:
		case KARecurrence::MONTHLY_POS:
		case KARecurrence::MONTHLY_DAY:
			break;
	}
	KDateTime recurStart = mRecurrence->startDateTime();
	if (mRecurrence->recursOn(recurStart.date(), recurStart.timeSpec()))
		return;           // it already recurs on the start date

	// Set the frequency to 1 to find the first possible occurrence
	bool changed = false;
	int frequency = mRecurrence->frequency();
	mRecurrence->setFrequency(1);
	DateTime next;
	nextRecurrence(mNextMainDateTime.effectiveKDateTime(), next);
	if (!next.isValid())
		mRecurrence->setStartDateTime(recurStart);   // reinstate the old value
	else
	{
		mRecurrence->setStartDateTime(next.effectiveKDateTime());
		mStartDateTime = mNextMainDateTime = next;
		mUpdated = changed = true;
	}
	mRecurrence->setFrequency(frequency);    // restore the frequency
	if (changed)
		calcTriggerTimes();
}

/******************************************************************************
*  Initialise the event's recurrence from a KCal::Recurrence.
*  The event's start date/time is not changed.
*/
void KAEvent::setRecurrence(const KARecurrence& recurrence)
{
	startChanges();   // prevent multiple trigger time evaluation here
	mUpdated = true;
	delete mRecurrence;
	if (recurrence.recurs())
	{
		mRecurrence = new KARecurrence(recurrence);
		mRecurrence->setStartDateTime(mStartDateTime.effectiveKDateTime());
		mRecurrence->setAllDay(mStartDateTime.isDateOnly());
		mChanged = true;
	}
	else
	{
		if (mRecurrence)
			mChanged = true;
		mRecurrence = 0;
	}

	// Adjust sub-repetition values to fit the recurrence.
	// N.B. Need to make a copy of mRepeatInterval because it's passed to
	// setRepetition() as a reference, and setRepetition() sets it to zero
	// before using the parameter value.
	setRepetition(Duration(mRepeatInterval), mRepeatCount);

	endChanges();
}

/******************************************************************************
*  Initialise the event's sub-repetition.
*  The repetition length is adjusted if necessary to fit the recurrence interval.
*  Reply = false if a non-daily interval was specified for a date-only recurrence.
*/
bool KAEvent::setRepetition(const Duration& interval, int count)
{
	mUpdated        = true;
	mRepeatInterval = 0;
	mRepeatCount    = 0;
	mNextRepeat     = 0;
	if (interval.value() > 0  &&  count > 0  &&  !mRepeatAtLogin)
	{
		Q_ASSERT(checkRecur() != KARecurrence::NO_RECUR);
		if (!interval.isDaily()  &&  mStartDateTime.isDateOnly())
			return false;    // interval must be in units of days for date-only alarms
		Duration longestInterval = mRecurrence->longestInterval();
		if (interval * count >= longestInterval)
		{
			count = mStartDateTime.isDateOnly()
			      ? (longestInterval.asDays() - 1) / interval.asDays()
			      : (longestInterval.asSeconds() - 1) / interval.asSeconds();
		}
		mRepeatInterval = interval;
		mRepeatCount    = count;
		calcTriggerTimes();
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
 * Reply = false if no recurrence was set up.
 */
bool KAEvent::setRecurMinutely(int freq, int count, const KDateTime& end)
{
	bool success = setRecur(RecurrenceRule::rMinutely, freq, count, end);
	calcTriggerTimes();
	return success;
}

/******************************************************************************
 * Set the recurrence to recur daily.
 * Parameters:
 *    freq  = how many days between recurrences.
 *    days  = which days of the week alarms are allowed to occur on.
 *    count = number of occurrences, including first and last.
 *          = -1 to recur indefinitely.
 *          = 0 to use 'end' instead.
 *    end   = end date (invalid to use 'count' instead).
 * Reply = false if no recurrence was set up.
 */
bool KAEvent::setRecurDaily(int freq, const QBitArray& days, int count, const QDate& end)
{
	bool success = setRecur(RecurrenceRule::rDaily, freq, count, end);
	if (success)
	{
		int n = 0;
		for (int i = 0;  i < 7;  ++i)
		{
			if (days.testBit(i))
				++n;
		}
		if (n < 7)
			mRecurrence->addWeeklyDays(days);
	}
	calcTriggerTimes();
	return success;
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
 * Reply = false if no recurrence was set up.
 */
bool KAEvent::setRecurWeekly(int freq, const QBitArray& days, int count, const QDate& end)
{
	bool success = setRecur(RecurrenceRule::rWeekly, freq, count, end);
	if (success)
		mRecurrence->addWeeklyDays(days);
	calcTriggerTimes();
	return success;
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
 * Reply = false if no recurrence was set up.
 */
bool KAEvent::setRecurMonthlyByDate(int freq, const QList<int>& days, int count, const QDate& end)
{
	bool success = setRecur(RecurrenceRule::rMonthly, freq, count, end);
	if (success)
	{
		for (int i = 0, end = days.count();  i < end;  ++i)
			mRecurrence->addMonthlyDate(days[i]);
	}
	calcTriggerTimes();
	return success;
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
 * Reply = false if no recurrence was set up.
 */
bool KAEvent::setRecurMonthlyByPos(int freq, const QList<MonthPos>& posns, int count, const QDate& end)
{
	bool success = setRecur(RecurrenceRule::rMonthly, freq, count, end);
	if (success)
	{
		for (int i = 0, end = posns.count();  i < end;  ++i)
			mRecurrence->addMonthlyPos(posns[i].weeknum, posns[i].days);
	}
	calcTriggerTimes();
	return success;
}

/******************************************************************************
 * Set the recurrence to recur annually, on the specified start date in each
 * of the specified months.
 * Parameters:
 *    freq   = how many years between recurrences.
 *    months = which months of the year alarms should occur on.
 *    day    = day of month, or 0 to use start date
 *    feb29  = when February 29th should recur in non-leap years.
 *    count  = number of occurrences, including first and last.
 *           = -1 to recur indefinitely.
 *           = 0 to use 'end' instead.
 *    end    = end date (invalid to use 'count' instead).
 * Reply = false if no recurrence was set up.
 */
bool KAEvent::setRecurAnnualByDate(int freq, const QList<int>& months, int day, Preferences::Feb29Type feb29, int count, const QDate& end)
{
	bool success = setRecur(RecurrenceRule::rYearly, freq, count, end, feb29);
	if (success)
	{
		for (int i = 0, end = months.count();  i < end;  ++i)
			mRecurrence->addYearlyMonth(months[i]);
		if (day)
			mRecurrence->addMonthlyDate(day);
	}
	calcTriggerTimes();
	return success;
}

/******************************************************************************
 * Set the recurrence to recur annually, on the specified weekdays in the
 * specified weeks of the specified months.
 * Parameters:
 *    freq   = how many years between recurrences.
 *    posns  = which days of the week/weeks of the month alarms should occur on.
 *    months = which months of the year alarms should occur on.
 *    count  = number of occurrences, including first and last.
 *           = -1 to recur indefinitely.
 *           = 0 to use 'end' instead.
 *    end    = end date (invalid to use 'count' instead).
 * Reply = false if no recurrence was set up.
 */
bool KAEvent::setRecurAnnualByPos(int freq, const QList<MonthPos>& posns, const QList<int>& months, int count, const QDate& end)
{
	bool success = setRecur(RecurrenceRule::rYearly, freq, count, end);
	if (success)
	{
		int i = 0;
		int iend;
		for (iend = months.count();  i < iend;  ++i)
			mRecurrence->addYearlyMonth(months[i]);
		for (i = 0, iend = posns.count();  i < iend;  ++i)
			mRecurrence->addYearlyPos(posns[i].weeknum, posns[i].days);
	}
	calcTriggerTimes();
	return success;
}

/******************************************************************************
 * Initialise the event's recurrence data.
 * Parameters:
 *    freq  = how many intervals between recurrences.
 *    count = number of occurrences, including first and last.
 *          = -1 to recur indefinitely.
 *          = 0 to use 'end' instead.
 *    end   = end date/time (invalid to use 'count' instead).
 * Reply = false if no recurrence was set up.
 */
bool KAEvent::setRecur(RecurrenceRule::PeriodType recurType, int freq, int count, const QDate& end, Preferences::Feb29Type feb29)
{
	KDateTime edt = mNextMainDateTime.kDateTime();
	edt.setDate(end);
	return setRecur(recurType, freq, count, edt, feb29);
}
bool KAEvent::setRecur(RecurrenceRule::PeriodType recurType, int freq, int count, const KDateTime& end, Preferences::Feb29Type feb29)
{
	if (count >= -1  &&  (count || end.date().isValid()))
	{
		if (!mRecurrence)
			mRecurrence = new KARecurrence;
		if (mRecurrence->init(recurType, freq, count, mNextMainDateTime.kDateTime(), end, feb29))
		{
			mUpdated = true;
			return true;
		}
	}
	clearRecur();
	return false;
}

/******************************************************************************
 * Clear the event's recurrence and alarm repetition data.
 */
void KAEvent::clearRecur()
{
	delete mRecurrence;
	mRecurrence     = 0;
	mRepeatInterval = 0;
	mRepeatCount    = 0;
	mNextRepeat     = 0;
	mUpdated        = true;
}

/******************************************************************************
* Validate the event's recurrence data, correcting any inconsistencies (which
* should never occur!).
* Reply = true if a recurrence (as opposed to a login repetition) exists.
*/
KARecurrence::Type KAEvent::checkRecur() const
{
	if (mRecurrence)
	{
		KARecurrence::Type type = mRecurrence->type();
		switch (type)
		{
			case KARecurrence::MINUTELY:     // hourly
			case KARecurrence::DAILY:        // daily
			case KARecurrence::WEEKLY:       // weekly on multiple days of week
			case KARecurrence::MONTHLY_DAY:  // monthly on multiple dates in month
			case KARecurrence::MONTHLY_POS:  // monthly on multiple nth day of week
			case KARecurrence::ANNUAL_DATE:  // annually on multiple months (day of month = start date)
			case KARecurrence::ANNUAL_POS:   // annually on multiple nth day of week in multiple months
				return type;
			default:
				if (mRecurrence)
					const_cast<KAEvent*>(this)->clearRecur();  // this shouldn't exist!!
				break;
		}
	}
	return KARecurrence::NO_RECUR;
}


/******************************************************************************
 * Return the recurrence interval in units of the recurrence period type.
 */
int KAEvent::recurInterval() const
{
	if (mRecurrence)
	{
		switch (mRecurrence->type())
		{
			case KARecurrence::MINUTELY:
			case KARecurrence::DAILY:
			case KARecurrence::WEEKLY:
			case KARecurrence::MONTHLY_DAY:
			case KARecurrence::MONTHLY_POS:
			case KARecurrence::ANNUAL_DATE:
			case KARecurrence::ANNUAL_POS:
				return mRecurrence->frequency();
			default:
				break;
		}
	}
	return 0;
}

/******************************************************************************
* Validate the event's alarm sub-repetition data, correcting any
* inconsistencies (which should never occur!).
*/
void KAEvent::checkRepetition() const
{
	if (mRepeatCount  &&  !mRepeatInterval)
		const_cast<KAEvent*>(this)->mRepeatCount = 0;
	if (!mRepeatCount  &&  mRepeatInterval)
		const_cast<KAEvent*>(this)->mRepeatInterval = 0;
}

#if 0
/******************************************************************************
 * Convert a QList<WDayPos> to QList<MonthPos>.
 */
QList<KAEvent::MonthPos> KAEvent::convRecurPos(const QList<KCal::RecurrenceRule::WDayPos>& wdaypos)
{
	QList<MonthPos> mposns;
	for (int i = 0, end = wdaypos.count();  i < end;  ++i)
	{
		int daybit  = wdaypos[i].day() - 1;
		int weeknum = wdaypos[i].pos();
		bool found = false;
		for (int m = 0, mend = mposns.count();  m < mend;  ++m)
		{
			if (mposns[m].weeknum == weeknum)
			{
				mposns[m].days.setBit(daybit);
				found = true;
				break;
			}
		}
		if (!found)
		{
			MonthPos mpos;
			mpos.days.fill(false);
			mpos.days.setBit(daybit);
			mpos.weeknum = weeknum;
			mposns.append(mpos);
		}
	}
	return mposns;
}
#endif

/******************************************************************************
 * Adjust the time at which date-only events will occur for each of the events
 * in a list. Events for which both date and time are specified are left
 * unchanged.
 * Reply = true if any events have been updated.
 */
bool KAEvent::adjustStartOfDay(const Event::List& events)
{
	bool changed = false;
	QTime startOfDay = Preferences::startOfDay();
	for (int ei = 0, eend = events.count();  ei < eend;  ++ei)
	{
		Event* event = events[ei];
		QStringList flags = event->customProperty(KCalendar::APPNAME, FLAGS_PROPERTY).split(SC, QString::SkipEmptyParts);
		if (flags.indexOf(DATE_ONLY_FLAG) >= 0)
		{
			// It's an untimed event, so fix it
			QTime oldTime = event->dtStart().time();
			int adjustment = oldTime.secsTo(startOfDay);
			if (adjustment)
			{
				event->setDtStart(KDateTime(event->dtStart().date(), startOfDay, Preferences::timeZone()));
				Alarm::List alarms = event->alarms();
				int deferralOffset = 0;
				for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
				{
					// Parse the next alarm's text
					Alarm* alarm = alarms[ai];
					AlarmData data;
					readAlarm(alarm, data);
					if (data.type & KAAlarm::TIMED_DEFERRAL_FLAG)
					{
						// Timed deferral alarm, so adjust the offset
						deferralOffset = alarm->startOffset().asSeconds();
						alarm->setStartOffset(deferralOffset - adjustment);
					}
					else if (data.type == KAAlarm::AUDIO__ALARM
					&&       alarm->startOffset().asSeconds() == deferralOffset)
					{
						// Audio alarm is set for the same time as the deferral alarm
						alarm->setStartOffset(deferralOffset - adjustment);
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
			DateTime start;
			KDateTime nextMainDateTime = readDateTime(event, false, start).kDateTime();
			AlarmMap alarmMap;
			readAlarms(event, &alarmMap);
			for (AlarmMap::Iterator it = alarmMap.begin();  it != alarmMap.end();  ++it)
			{
				const AlarmData& data = it.value();
				if (!data.alarm->hasStartOffset())
					continue;
				if ((data.type & KAAlarm::DEFERRED_ALARM)
				&&  !(data.type & KAAlarm::TIMED_DEFERRAL_FLAG))
				{
					// Date-only deferral alarm, so adjust its time
					KDateTime altime = data.alarm->startOffset().end(nextMainDateTime);
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
 * If the calendar was written by a previous version of KAlarm, do any
 * necessary format conversions on the events to ensure that when the calendar
 * is saved, no information is lost or corrupted.
 * Reply = true if any conversions were done.
 */
bool KAEvent::convertKCalEvents(KCal::CalendarLocal& calendar, int version, bool adjustSummerTime)
{
	// KAlarm pre-0.9 codes held in the alarm's DESCRIPTION property
	static const QChar   SEPARATOR        = QLatin1Char(';');
	static const QChar   LATE_CANCEL_CODE = QLatin1Char('C');
	static const QChar   AT_LOGIN_CODE    = QLatin1Char('L');   // subsidiary alarm at every login
	static const QChar   DEFERRAL_CODE    = QLatin1Char('D');   // extra deferred alarm
	static const QString TEXT_PREFIX      = QLatin1String("TEXT:");
	static const QString FILE_PREFIX      = QLatin1String("FILE:");
	static const QString COMMAND_PREFIX   = QLatin1String("CMD:");

	// KAlarm pre-0.9.2 codes held in the event's CATEGORY property
	static const QString BEEP_CATEGORY    = QLatin1String("BEEP");

	// KAlarm pre-1.1.1 LATECANCEL category with no parameter
	static const QString LATE_CANCEL_CAT = QLatin1String("LATECANCEL");

	// KAlarm pre-1.3.0 TMPLDEFTIME category with no parameter
	static const QString TEMPL_DEF_TIME_CAT = QLatin1String("TMPLDEFTIME");

	// KAlarm pre-1.3.1 XTERM category
	static const QString EXEC_IN_XTERM_CAT  = QLatin1String("XTERM");

	// KAlarm pre-1.9.0 categories
	static const QString DATE_ONLY_CATEGORY        = QLatin1String("DATE");
	static const QString EMAIL_BCC_CATEGORY        = QLatin1String("BCC");
	static const QString CONFIRM_ACK_CATEGORY      = QLatin1String("ACKCONF");
	static const QString KORGANIZER_CATEGORY       = QLatin1String("KORG");
	static const QString DEFER_CATEGORY            = QLatin1String("DEFER;");
	static const QString ARCHIVE_CATEGORY          = QLatin1String("SAVE");
	static const QString ARCHIVE_CATEGORIES        = QLatin1String("SAVE:");
	static const QString LATE_CANCEL_CATEGORY      = QLatin1String("LATECANCEL;");
	static const QString AUTO_CLOSE_CATEGORY       = QLatin1String("LATECLOSE;");
	static const QString TEMPL_AFTER_TIME_CATEGORY = QLatin1String("TMPLAFTTIME;");
	static const QString KMAIL_SERNUM_CATEGORY     = QLatin1String("KMAIL:");
	static const QString LOG_CATEGORY              = QLatin1String("LOG:");

	// KAlarm pre-1.5.0/1.9.9 properties
	static const QByteArray KMAIL_ID_PROPERTY("KMAILID");    // X-KDE-KALARM-KMAILID property

	if (version >= calVersion())
		return false;

	kDebug() << "Adjusting version" << version;
	bool pre_0_7    = (version < KAlarm::Version(0,7,0));
	bool pre_0_9    = (version < KAlarm::Version(0,9,0));
	bool pre_0_9_2  = (version < KAlarm::Version(0,9,2));
	bool pre_1_1_1  = (version < KAlarm::Version(1,1,1));
	bool pre_1_2_1  = (version < KAlarm::Version(1,2,1));
	bool pre_1_3_0  = (version < KAlarm::Version(1,3,0));
	bool pre_1_3_1  = (version < KAlarm::Version(1,3,1));
	bool pre_1_4_14 = (version < KAlarm::Version(1,4,14));
	bool pre_1_5_0  = (version < KAlarm::Version(1,5,0));
	bool pre_1_9_0  = (version < KAlarm::Version(1,9,0));
	bool pre_1_9_2  = (version < KAlarm::Version(1,9,2));
	bool pre_1_9_7  = (version < KAlarm::Version(1,9,7));
	bool pre_1_9_9  = (version < KAlarm::Version(1,9,9));
	bool pre_1_9_10 = (version < KAlarm::Version(1,9,10));
	Q_ASSERT(calVersion() == KAlarm::Version(1,9,10));

	QTime startOfDay = Preferences::startOfDay();
	KTimeZone localZone;
	if (pre_1_9_2)
		localZone = KSystemTimeZones::local();

	bool converted = false;
	Event::List events = calendar.rawEvents();
	for (int ei = 0, eend = events.count();  ei < eend;  ++ei)
	{
		Event* event = events[ei];
		Alarm::List alarms = event->alarms();
		if (alarms.isEmpty())
			continue;    // KAlarm isn't interested in events without alarms
		event->startUpdates();   // prevent multiple update notifications
		bool readOnly = event->isReadOnly();
		if (readOnly)
			event->setReadOnly(false);
		QStringList cats = event->categories();
		bool addLateCancel = false;
		QStringList flags;

		if (pre_0_7  &&  event->allDay())
		{
			// It's a KAlarm pre-0.7 calendar file.
			// Ensure that when the calendar is saved, the alarm time isn't lost.
			event->setAllDay(false);
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
			for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
			{
				Alarm* alarm = alarms[ai];
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
				if (txt.indexOf(TEXT_PREFIX, i) == i)
					i += TEXT_PREFIX.length();
				else if (txt.indexOf(FILE_PREFIX, i) == i)
				{
					action = T_FILE;
					i += FILE_PREFIX.length();
				}
				else if (txt.indexOf(COMMAND_PREFIX, i) == i)
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
					alarm->setCustomProperty(KCalendar::APPNAME, TYPE_PROPERTY, types.join(","));

				if (pre_0_7  &&  alarm->repeatCount() > 0  &&  alarm->snoozeTime().value() > 0)
				{
					// It's a KAlarm pre-0.7 calendar file.
					// Minutely recurrences were stored differently.
					Recurrence* recur = event->recurrence();
					if (recur  &&  recur->recurs())
					{
						recur->setMinutely(alarm->snoozeTime().asSeconds() / 60);
						recur->setDuration(alarm->repeatCount() + 1);
						alarm->setRepeatCount(0);
						alarm->setSnoozeTime(0);
					}
				}

				if (adjustSummerTime)
				{
					// The calendar file was written by the KDE 3.0.0 version of KAlarm 0.5.7.
					// Summer time was ignored when converting to UTC.
					KDateTime dt = alarm->time();
					time_t t = dt.toTime_t();
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
			 * For the archive calendar, set the CREATED time to the DTEND value.
			 * Convert date-only DTSTART to date/time, and add category "DATE".
			 * Set the DTEND time to the DTSTART time.
			 * Convert all alarm times to DTSTART offsets.
			 * For display alarms, convert the first unlabelled category to an
			 * X-KDE-KALARM-FONTCOLOUR property.
			 * Convert BEEP category into an audio alarm with no audio file.
			 */
			if (KCalEvent::status(event) == KCalEvent::ARCHIVED)
				event->setCreated(event->dtEnd());
			KDateTime start = event->dtStart();
			if (event->allDay())
			{
				event->setAllDay(false);
				start.setTime(startOfDay);
				flags += DATE_ONLY_FLAG;
			}
			event->setHasEndDate(false);

			for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
			{
				Alarm* alarm = alarms[ai];
				KDateTime dt = alarm->time();
				alarm->setStartOffset(start.secsTo(dt));
			}

			if (!cats.isEmpty())
			{
				for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
				{
					Alarm* alarm = alarms[ai];
					if (alarm->type() == Alarm::Display)
						alarm->setCustomProperty(KCalendar::APPNAME, FONT_COLOUR_PROPERTY,
						                         QString::fromLatin1("%1;;").arg(cats[0]));
				}
				cats.removeAt(0);
			}

			for (int i = 0, end = cats.count();  i < end;  ++i)
			{
				if (cats[i] == BEEP_CATEGORY)
				{
					cats.removeAt(i);

					Alarm* alarm = event->newAlarm();
					alarm->setEnabled(true);
					alarm->setAudioAlarm();
					KDateTime dt = event->dtStart();    // default

					// Parse and order the alarms to know which one's date/time to use
					AlarmMap alarmMap;
					readAlarms(event, &alarmMap);
					AlarmMap::ConstIterator it = alarmMap.constBegin();
					if (it != alarmMap.constEnd())
					{
						dt = it.value().alarm->time();
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
			int i;
			while ((i = cats.indexOf(LATE_CANCEL_CAT)) >= 0)
			{
				cats.removeAt(i);
				addLateCancel = true;
			}
		}

		if (pre_1_2_1)
		{
			/*
			 * It's a KAlarm pre-1.2.1 calendar file.
			 * Convert email display alarms from translated to untranslated header prefixes.
			 */
			for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
			{
				Alarm* alarm = alarms[ai];
				if (alarm->type() == Alarm::Display)
				{
					QString oldtext = alarm->text();
					QString newtext = AlarmText::toCalendarText(oldtext);
					if (oldtext != newtext)
						alarm->setDisplayAlarm(newtext);
				}
			}
		}

		if (pre_1_3_0)
		{
			/*
			 * It's a KAlarm pre-1.3.0 calendar file.
			 * Convert simple TMPLDEFTIME category to TMPLAFTTIME:n where n = minutes after.
			 */
			int i;
			while ((i = cats.indexOf(TEMPL_DEF_TIME_CAT)) >= 0)
			{
				cats.removeAt(i);
				(flags += TEMPL_AFTER_TIME_FLAG) += QLatin1String("0");
			}
		}

		if (pre_1_3_1)
		{
			/*
			 * It's a KAlarm pre-1.3.1 calendar file.
			 * Convert simple XTERM category to LOG:xterm:
			 */
			int i;
			while ((i = cats.indexOf(EXEC_IN_XTERM_CAT)) >= 0)
			{
				cats.removeAt(i);
				event->setCustomProperty(KCalendar::APPNAME, LOG_PROPERTY, xtermURL);
			}
		}

		if (pre_1_9_0)
		{
			/*
			 * It's a KAlarm pre-1.9 calendar file.
			 * Add the X-KDE-KALARM-STATUS custom property.
			 * Convert KAlarm categories to custom fields.
			 */
			KCalEvent::setStatus(event, KCalEvent::status(event));
			for (int i = 0;  i < cats.count(); )
			{
				QString cat = cats[i];
				if (cat == DATE_ONLY_CATEGORY)
					flags += DATE_ONLY_FLAG;
				else if (cat == CONFIRM_ACK_CATEGORY)
					flags += CONFIRM_ACK_FLAG;
				else if (cat == EMAIL_BCC_CATEGORY)
					flags += EMAIL_BCC_FLAG;
				else if (cat == KORGANIZER_CATEGORY)
					flags += KORGANIZER_FLAG;
				else if (cat.startsWith(DEFER_CATEGORY))
					(flags += DEFER_FLAG) += cat.mid(DEFER_CATEGORY.length());
				else if (cat.startsWith(TEMPL_AFTER_TIME_CATEGORY))
					(flags += TEMPL_AFTER_TIME_FLAG) += cat.mid(TEMPL_AFTER_TIME_CATEGORY.length());
				else if (cat.startsWith(LATE_CANCEL_CATEGORY))
					(flags += LATE_CANCEL_FLAG) += cat.mid(LATE_CANCEL_CATEGORY.length());
				else if (cat.startsWith(AUTO_CLOSE_CATEGORY))
					(flags += AUTO_CLOSE_FLAG) += cat.mid(AUTO_CLOSE_CATEGORY.length());
				else if (cat.startsWith(KMAIL_SERNUM_CATEGORY))
					(flags += KMAIL_SERNUM_FLAG) += cat.mid(KMAIL_SERNUM_CATEGORY.length());
				else if (cat == ARCHIVE_CATEGORY)
					event->setCustomProperty(KCalendar::APPNAME, ARCHIVE_PROPERTY, QLatin1String("0"));
				else if (cat.startsWith(ARCHIVE_CATEGORIES))
					event->setCustomProperty(KCalendar::APPNAME, ARCHIVE_PROPERTY, cat.mid(ARCHIVE_CATEGORIES.length()));
				else if (cat.startsWith(LOG_CATEGORY))
					event->setCustomProperty(KCalendar::APPNAME, LOG_PROPERTY, cat.mid(LOG_CATEGORY.length()));
				else
				{
					++i;   // Not a KAlarm category, so leave it
					continue;
				}
				cats.removeAt(i);
			}
		}

		if (pre_1_9_2)
		{
			/*
			 * It's a KAlarm pre-1.9.2 calendar file.
			 * Convert from clock time to the local system time zone.
			 */
			event->shiftTimes(KDateTime::ClockTime, localZone);
			converted = true;
		}

		if (addLateCancel)
			(flags += LATE_CANCEL_FLAG) += QLatin1String("1");
		if (!flags.isEmpty())
			event->setCustomProperty(KCalendar::APPNAME, FLAGS_PROPERTY, flags.join(SC));
		event->setCategories(cats);


		if ((pre_1_4_14  ||  (pre_1_9_7 && !pre_1_9_0))
		&&  event->recurrence()  &&  event->recurrence()->recurs())
		{
			/*
			 * It's a KAlarm pre-1.4.14 or KAlarm 1.9 series pre-1.9.7 calendar file.
			 * For recurring events, convert the main alarm offset to an absolute
			 * time in the X-KDE-KALARM-NEXTRECUR property, and convert main
			 * alarm offsets to zero and deferral alarm offsets to be relative to
			 * the next recurrence.
			 */
			QStringList flags = event->customProperty(KCalendar::APPNAME, FLAGS_PROPERTY).split(SC, QString::SkipEmptyParts);
			bool dateOnly = flags.contains(DATE_ONLY_FLAG);
			KDateTime startDateTime = event->dtStart();
			if (dateOnly)
				startDateTime.setDateOnly(true);
			// Convert the main alarm and get the next main trigger time from it
			KDateTime nextMainDateTime;
			bool mainExpired = true;
			for (int i = 0, alend = alarms.count();  i < alend;  ++i)
			{
				Alarm* alarm = alarms[i];
				if (!alarm->hasStartOffset())
					continue;
				bool mainAlarm = true;
				QString property = alarm->customProperty(KCalendar::APPNAME, TYPE_PROPERTY);
				QStringList types = property.split(QChar(','), QString::SkipEmptyParts);
				for (int i = 0;  i < types.count();  ++i)
				{
					QString type = types[i];
					if (type == AT_LOGIN_TYPE
					||  type == TIME_DEFERRAL_TYPE
					||  type == DATE_DEFERRAL_TYPE
					||  type == REMINDER_TYPE
					||  type == REMINDER_ONCE_TYPE
					||  type == DISPLAYING_TYPE
					||  type == PRE_ACTION_TYPE
					||  type == POST_ACTION_TYPE)
						mainAlarm = false;
				}
				if (mainAlarm)
				{
					mainExpired = false;
					nextMainDateTime = alarm->time();
					nextMainDateTime.setDateOnly(dateOnly);
					nextMainDateTime = nextMainDateTime.toTimeSpec(startDateTime);
					if (nextMainDateTime != startDateTime)
					{
						QDateTime dt = nextMainDateTime.dateTime();
						event->setCustomProperty(KCalendar::APPNAME, NEXT_RECUR_PROPERTY,
						                         dt.toString(dateOnly ? "yyyyMMdd" : "yyyyMMddThhmmss"));
					}
					alarm->setStartOffset(0);
					converted = true;
				}
			}
			int adjustment;
			if (mainExpired)
			{
				// It's an expired recurrence.
				// Set the alarm offset relative to the first actual occurrence
				// (taking account of possible exceptions).
				KDateTime dt = event->recurrence()->getNextDateTime(startDateTime.addDays(-1));
				dt.setDateOnly(dateOnly);
				adjustment = startDateTime.secsTo(dt);
			}
			else
				adjustment = startDateTime.secsTo(nextMainDateTime);
			if (adjustment)
			{
				// Convert deferred alarms
				for (int i = 0, alend = alarms.count();  i < alend;  ++i)
				{
					Alarm* alarm = alarms[i];
					if (!alarm->hasStartOffset())
						continue;
					QString property = alarm->customProperty(KCalendar::APPNAME, TYPE_PROPERTY);
					QStringList types = property.split(QChar(','), QString::SkipEmptyParts);
					for (int i = 0;  i < types.count();  ++i)
					{
						QString type = types[i];
						if (type == TIME_DEFERRAL_TYPE
						||  type == DATE_DEFERRAL_TYPE)
						{
							alarm->setStartOffset(alarm->startOffset().asSeconds() - adjustment);
							converted = true;
							break;
						}
					}
				}
			}
		}

		if (pre_1_5_0  ||  (pre_1_9_9 && !pre_1_9_0))
		{
			/*
			 * It's a KAlarm pre-1.5.0 or KAlarm 1.9 series pre-1.9.9 calendar file.
			 * Convert email identity names to uoids.
			 */
			for (int i = 0, alend = alarms.count();  i < alend;  ++i)
			{
				Alarm* alarm = alarms[i];
				QString name = alarm->customProperty(KCalendar::APPNAME, KMAIL_ID_PROPERTY);
				if (name.isEmpty())
					continue;
				uint id = KAMail::identityUoid(name);
				if (id)
					alarm->setCustomProperty(KCalendar::APPNAME, EMAIL_ID_PROPERTY, QString::number(id));
				alarm->removeCustomProperty(KCalendar::APPNAME, KMAIL_ID_PROPERTY);
				converted = true;
			}
		}

		if (pre_1_9_10)
		{
			/*
			 * It's a KAlarm pre-1.9.10 calendar file.
			 * Convert simple repetitions without a recurrence, to a recurrence.
			 */
			if (convertRepetition(event))
				converted = true;
		}

		if (readOnly)
			event->setReadOnly(true);
		event->endUpdates();     // finally issue an update notification
	}
	return converted;
}

#if 0
/******************************************************************************
* If the calendar was written by a pre-1.9.10 version of KAlarm, or another
* program, convert simple repetitions in events without a recurrence, to a
* recurrence.
* Reply = true if any conversions were done.
*/
bool KAEvent::convertRepetitions(KCal::CalendarLocal& calendar)
{

	bool converted = false;
	Event::List events = calendar.rawEvents();
	for (int i = 0, end = events.count();  i < end;  ++i)
	{
		if (convertRepetition(events[i]))
			converted = true;
	}
	return converted;
}
#endif

/******************************************************************************
* Convert simple repetitions in an event without a recurrence, to a
* recurrence. Repetitions which are an exact multiple of 24 hours are converted
* to daily recurrences; else they are converted to minutely recurrences. Note
* that daily and minutely recurrences produce different results when they span
* a daylight saving time change.
* Reply = true if any conversions were done.
*/
bool KAEvent::convertRepetition(KCal::Event* event)
{
	Alarm::List alarms = event->alarms();
	if (alarms.isEmpty())
		return false;
	Recurrence* recur = event->recurrence();   // guaranteed to return non-null
	if (!recur->recurs())
		return false;
	bool converted = false;
	bool readOnly = event->isReadOnly();
	for (int ai = 0, aend = alarms.count();  ai < aend;  ++ai)
	{
		Alarm* alarm = alarms[ai];
		if (alarm->repeatCount() > 0  &&  alarm->snoozeTime().value() > 0)
		{
			if (!converted)
			{
				event->startUpdates();   // prevent multiple update notifications
				if (readOnly)
					event->setReadOnly(false);
				if ((alarm->snoozeTime().asSeconds() % (24*3600)) != 0)
					recur->setMinutely(alarm->snoozeTime());
				else
					recur->setDaily(alarm->snoozeTime() / (24*3600));
				recur->setDuration(alarm->repeatCount() + 1);
				converted = true;
			}
			alarm->setRepeatCount(0);
			alarm->setSnoozeTime(0);
		}
	}
	if (converted)
	{
		if (readOnly)
			event->setReadOnly(true);
		event->endUpdates();     // finally issue an update notification
	}
	return converted;
}

#ifndef NDEBUG
void KAEvent::dumpDebug() const
{
	kDebug() << "KAEvent dump:";
	kDebug() << "-- mCategory:" << mCategory;
	KAAlarmEventBase::dumpDebug();
	if (!mTemplateName.isEmpty())
	{
		kDebug() << "-- mTemplateName:" << mTemplateName;
		kDebug() << "-- mTemplateAfterTime:" << mTemplateAfterTime;
	}
	if (mResource)
		kDebug() << "-- mResource:" << mResource->resourceName();
	if (mActionType == T_MESSAGE  ||  mActionType == T_FILE)
	{
		kDebug() << "-- mSpeak:" << mSpeak;
		kDebug() << "-- mAudioFile:" << mAudioFile;
		kDebug() << "-- mPreAction:" << mPreAction;
		kDebug() << "-- mCancelOnPreActErr:" << mCancelOnPreActErr;
		kDebug() << "-- mPostAction:" << mPostAction;
	}
	else if (mActionType == T_COMMAND)
	{
		kDebug() << "-- mCommandXterm:" << mCommandXterm;
		kDebug() << "-- mCommandDisplay:" << mCommandDisplay;
		kDebug() << "-- mLogFile:" << mLogFile;
	}
	kDebug() << "-- mKMailSerialNumber:" << mKMailSerialNumber;
	kDebug() << "-- mCopyToKOrganizer:" << mCopyToKOrganizer;
	kDebug() << "-- mExcludeHolidays:" << mExcludeHolidays;
	kDebug() << "-- mWorkTimeOnly:" << mWorkTimeOnly;
	kDebug() << "-- mStartDateTime:" << mStartDateTime.toString();
	kDebug() << "-- mSaveDateTime:" << mSaveDateTime;
	if (mRepeatAtLogin)
		kDebug() << "-- mAtLoginDateTime:" << mAtLoginDateTime;
	kDebug() << "-- mArchiveRepeatAtLogin:" << mArchiveRepeatAtLogin;
	kDebug() << "-- mEnabled:" << mEnabled;
	if (mReminderMinutes)
		kDebug() << "-- mReminderMinutes:" << mReminderMinutes;
	if (mArchiveReminderMinutes)
		kDebug() << "-- mArchiveReminderMinutes:" << mArchiveReminderMinutes;
	if (mReminderMinutes  ||  mArchiveReminderMinutes)
		kDebug() << "-- mReminderOnceOnly:" << mReminderOnceOnly;
	else if (mDeferral > 0)
	{
		kDebug() << "-- mDeferral:" << (mDeferral == NORMAL_DEFERRAL ? "normal" : "reminder");
		kDebug() << "-- mDeferralTime:" << mDeferralTime.toString();
	}
	kDebug() << "-- mDeferDefaultMinutes:" << mDeferDefaultMinutes;
	kDebug() << "-- mAllTrigger:" << mAllTrigger.toString();
	kDebug() << "-- mMainTrigger:" << mMainTrigger.toString();
	kDebug() << "-- mAllWorkTrigger:" << mAllWorkTrigger.toString();
	kDebug() << "-- mMainWorkTrigger:" << mMainWorkTrigger.toString();
	if (mDisplaying)
	{
		kDebug() << "-- mDisplayingTime:" << mDisplayingTime.toString();
		kDebug() << "-- mDisplayingFlags:" << mDisplayingFlags;
		kDebug() << "-- mDisplayingDefer:" << mDisplayingDefer;
		kDebug() << "-- mDisplayingEdit:" << mDisplayingEdit;
	}
	kDebug() << "-- mRevision:" << mRevision;
	kDebug() << "-- mRecurrence:" << (mRecurrence ? "true" : "false");
	kDebug() << "-- mAlarmCount:" << mAlarmCount;
	kDebug() << "-- mMainExpired:" << mMainExpired;
	kDebug() << "KAEvent dump end";
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

int KAAlarm::flags() const
{
	return KAAlarmEventBase::flags()
	     | (mDeferred ? KAEvent::DEFERRAL : 0);

}

#ifndef NDEBUG
void KAAlarm::dumpDebug() const
{
	kDebug() << "KAAlarm dump:";
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
	kDebug() << "-- mType:" << altype;
	kDebug() << "-- mRecurs:" << (mRecurs ? "true" : "false");
	kDebug() << "-- mDeferred:" << (mDeferred ? "true" : "false");
	kDebug() << "KAAlarm dump end";
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
	mEventID           = rhs.mEventID;
	mText              = rhs.mText;
	mNextMainDateTime  = rhs.mNextMainDateTime;
	mBgColour          = rhs.mBgColour;
	mFgColour          = rhs.mFgColour;
	mFont              = rhs.mFont;
	mEmailFromIdentity = rhs.mEmailFromIdentity;
	mEmailAddresses    = rhs.mEmailAddresses;
	mEmailSubject      = rhs.mEmailSubject;
	mEmailAttachments  = rhs.mEmailAttachments;
	mSoundVolume       = rhs.mSoundVolume;
	mFadeVolume        = rhs.mFadeVolume;
	mFadeSeconds       = rhs.mFadeSeconds;
	mActionType        = rhs.mActionType;
	mCommandScript     = rhs.mCommandScript;
	mRepeatCount       = rhs.mRepeatCount;
	mRepeatInterval    = rhs.mRepeatInterval;
	mNextRepeat        = rhs.mNextRepeat;
	mBeep              = rhs.mBeep;
	mRepeatSound       = rhs.mRepeatSound;
	mRepeatAtLogin     = rhs.mRepeatAtLogin;
	mDisplaying        = rhs.mDisplaying;
	mLateCancel        = rhs.mLateCancel;
	mAutoClose         = rhs.mAutoClose;
	mEmailBcc          = rhs.mEmailBcc;
	mConfirmAck        = rhs.mConfirmAck;
	mDefaultFont       = rhs.mDefaultFont;
}

void KAAlarmEventBase::set(int flags)
{
	mBeep          = flags & KAEvent::BEEP;
	mRepeatSound   = flags & KAEvent::REPEAT_SOUND;
	mRepeatAtLogin = flags & KAEvent::REPEAT_AT_LOGIN;
	mAutoClose     = (flags & KAEvent::AUTO_CLOSE) && mLateCancel;
	mEmailBcc      = flags & KAEvent::EMAIL_BCC;
	mConfirmAck    = flags & KAEvent::CONFIRM_ACK;
	mDisplaying    = flags & KAEvent::DISPLAYING_;
	mDefaultFont   = flags & KAEvent::DEFAULT_FONT;
	mCommandScript = flags & KAEvent::SCRIPT;
}

int KAAlarmEventBase::flags() const
{
	return (mBeep            ? KAEvent::BEEP : 0)
	     | (mRepeatSound     ? KAEvent::REPEAT_SOUND : 0)
	     | (mRepeatAtLogin   ? KAEvent::REPEAT_AT_LOGIN : 0)
	     | (mAutoClose       ? KAEvent::AUTO_CLOSE : 0)
	     | (mEmailBcc        ? KAEvent::EMAIL_BCC : 0)
	     | (mConfirmAck      ? KAEvent::CONFIRM_ACK : 0)
	     | (mDisplaying      ? KAEvent::DISPLAYING_ : 0)
	     | (mDefaultFont     ? KAEvent::DEFAULT_FONT : 0)
	     | (mCommandScript   ? KAEvent::SCRIPT : 0);
}

QFont KAAlarmEventBase::font() const
{
	return mDefaultFont ? Preferences::messageFont() : mFont;
}

#ifndef NDEBUG
void KAAlarmEventBase::dumpDebug() const
{
	kDebug() << "-- mEventID:" << mEventID;
	kDebug() << "-- mActionType:" << (mActionType == T_MESSAGE ? "MESSAGE" : mActionType == T_FILE ? "FILE" : mActionType == T_COMMAND ? "COMMAND" : mActionType == T_EMAIL ? "EMAIL" : mActionType == T_AUDIO ? "AUDIO" : "??");
	kDebug() << "-- mText:" << mText;
	if (mActionType == T_COMMAND)
		kDebug() << "-- mCommandScript:" << (mCommandScript ? "true" : "false");
	kDebug() << "-- mNextMainDateTime:" << mNextMainDateTime.toString();
	if (mActionType == T_EMAIL)
	{
		kDebug() << "-- mEmail: FromKMail:" << mEmailFromIdentity;
		kDebug() << "--         Addresses:" << mEmailAddresses.join(",");
		kDebug() << "--         Subject:" << mEmailSubject;
		kDebug() << "--         Attachments:" << mEmailAttachments.join(",");
		kDebug() << "--         Bcc:" << (mEmailBcc ? "true" : "false");
	}
	kDebug() << "-- mBgColour:" << mBgColour.name();
	kDebug() << "-- mFgColour:" << mFgColour.name();
	kDebug() << "-- mDefaultFont:" << (mDefaultFont ? "true" : "false");
	if (!mDefaultFont)
		kDebug() << "-- mFont:" << mFont.toString();
	kDebug() << "-- mBeep:" << (mBeep ? "true" : "false");
	if (mActionType == T_AUDIO)
	{
		if (mSoundVolume >= 0)
		{
			kDebug() << "-- mSoundVolume:" << mSoundVolume;
			if (mFadeVolume >= 0)
			{
				kDebug() << "-- mFadeVolume:" << mFadeVolume;
				kDebug() << "-- mFadeSeconds:" << mFadeSeconds;
			}
			else
				kDebug() << "-- mFadeVolume:-:";
		}
		else
			kDebug() << "-- mSoundVolume:-:";
		kDebug() << "-- mRepeatSound:" << (mRepeatSound ? "true" : "false");
	}
	kDebug() << "-- mConfirmAck:" << (mConfirmAck ? "true" : "false");
	kDebug() << "-- mRepeatAtLogin:" << (mRepeatAtLogin ? "true" : "false");
	kDebug() << "-- mRepeatCount:" << mRepeatCount;
	if (mRepeatInterval.isDaily())
		kDebug() << "-- mRepeatInterval:" << mRepeatInterval.asDays() << "days";
	else
		kDebug() << "-- mRepeatInterval:" << mRepeatInterval.asSeconds()/60 << "minutes";
	kDebug() << "-- mNextRepeat:" << mNextRepeat;
	kDebug() << "-- mDisplaying:" << (mDisplaying ? "true" : "false");
	kDebug() << "-- mLateCancel:" << mLateCancel;
	kDebug() << "-- mAutoClose:" << (mAutoClose ? "true" : "false");
}
#endif


/*=============================================================================
= Class EmailAddressList
=============================================================================*/

/******************************************************************************
 * Sets the list of email addresses, removing any empty addresses.
 * Reply = false if empty addresses were found.
 */
EmailAddressList& EmailAddressList::operator=(const QList<Person>& addresses)
{
	clear();
	for (int p = 0, end = addresses.count();  p < end;  ++p)
	{
		if (!addresses[p].email().isEmpty())
			append(addresses[p]);
	}
	return *this;
}

/******************************************************************************
* Return the email address list as a string list of email addresses.
*/
EmailAddressList::operator QStringList() const
{
	QStringList list;
	for (int p = 0, end = count();  p < end;  ++p)
		list += address(p);
	return list;
}

/******************************************************************************
 * Return the email address list as a string, each address being delimited by
 * the specified separator string.
 */
QString EmailAddressList::join(const QString& separator) const
{
	QString result;
	bool first = true;
	for (int p = 0, end = count();  p < end;  ++p)
	{
		if (first)
			first = false;
		else
			result += separator;
		result += address(p);
	}
	return result;
}

/******************************************************************************
* Convert one item into an email address, including name.
*/
QString EmailAddressList::address(int index) const
{
	if (index < 0  ||  index > count())
		return QString();
	QString result;
	bool quote = false;
	KCal::Person person = (*this)[index];
	QString name = person.name();
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
		result += (*this)[index].name();
		result += (quote ? "\" <" : " <");
		quote = true;    // need angle brackets round email address
	}

	result += person.email();
	if (quote)
		result += '>';
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
	QString command;
	QString arguments;
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
			switch (ch.toAscii())
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
	for ( ;  pos < posMax  &&  commandLine[pos] == QLatin1Char(' ');  ++pos) ;
	arguments = commandLine.mid(pos);

	alarm->setProcedureAlarm(command, arguments);
}
