/*
 *  msgevent.cpp  -  the event object for messages
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
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
 *   TYPE = TEXT or FILE
 *   TEXT = message text or file name/URL
 */
static const QChar   SEPARATOR        = QChar(';');
static const QString TEXT_PREFIX      = QString::fromLatin1("TEXT:");
static const QString FILE_PREFIX      = QString::fromLatin1("FILE:");
static const QString LATE_CANCEL_CODE = QString::fromLatin1("C");
static const QString AT_LOGIN_CODE    = QString::fromLatin1("L");   // subsidiary alarm at every login
static const QString BEEP_CATEGORY    = QString::fromLatin1("BEEP");

struct AlarmData
{
	AlarmData() : file(false), lateCancel(false), repeatAtLogin(false) { }
	QString   messageOrFile;
	QDateTime dateTime;
	int       repeatCount;
	int       repeatMinutes;
	bool      file;
	bool      lateCancel;
	bool      repeatAtLogin;
};
typedef QMap<int, AlarmData> AlarmMap;


void KAlarmEvent::set(const Event& event)
{
	// Extract status from the event
	mEventID  = event.VUID();
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
	mFile          = false;
	mLateCancel    = false;
	mRepeatAtLogin = false;
	mMessageOrFile = "";
	mDateTime      = event.dtStart();

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
		if (txt.find(FILE_PREFIX, i) == i)
		{
			data.file = true;
			i += FILE_PREFIX.length();
		}
		else if (txt.find(TEXT_PREFIX, i) == i)
			i += TEXT_PREFIX.length();
		else
			i = 0;
		data.messageOrFile = txt.mid(i);
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
			mMessageOrFile = data.messageOrFile;
			mFile          = data.file;
			mDateTime      = data.dateTime;
			mRepeatCount   = data.repeatCount;
			mRepeatMinutes = data.repeatMinutes;
			mLateCancel    = data.lateCancel;
			set = true;
		}
		++mAlarmCount;
	}
	mUpdated = false;
}

void KAlarmEvent::set(const QDateTime& dateTime, const QString& text, const QColor& colour, bool file, int flags, int repeatCount, int repeatInterval)
{
	mDateTime      = dateTime;
	mMessageOrFile = text;
	mFile          = file;
	mColour        = colour;
	mRepeatCount   = repeatCount;
	mRepeatMinutes = repeatInterval;
	set(flags);
	mUpdated = false;
}

void KAlarmEvent::set(int flags)
{
	mBeep          = flags & BEEP;
	mRepeatAtLogin = flags & REPEAT_AT_LOGIN;
	mLateCancel    = flags & LATE_CANCEL;
}

bool KAlarmEvent::operator==(const KAlarmEvent& event)
{
	return mMessageOrFile        == event.mMessageOrFile
	&&     mDateTime             == event.mDateTime
	&&     mColour               == event.mColour
	&&     mRevision             == event.mRevision
	&&     mMainAlarmID          == event.mMainAlarmID
	&&     mRepeatAtLoginAlarmID == event.mRepeatAtLoginAlarmID
	&&     mRepeatCount          == event.mRepeatCount
	&&     mRepeatMinutes        == event.mRepeatMinutes
	&&     mBeep                 == event.mBeep
	&&     mFile                 == event.mFile
	&&     mRepeatAtLogin        == event.mRepeatAtLogin
	&&     mLateCancel           == event.mLateCancel;
}

int KAlarmEvent::flags() const
{
	return (mBeep          ? BEEP : 0)
	     | (mRepeatAtLogin ? REPEAT_AT_LOGIN : 0)
	     | (mLateCancel    ? LATE_CANCEL : 0);
}

// Create a new Event from the KAlarmEvent data
Event* KAlarmEvent::event() const
{
	Event* ev = new KCal::Event;
	updateEvent(*ev);
	return ev;
}

// Update an existing Event with the KAlarmEvent data
bool KAlarmEvent::updateEvent(Event& ev) const
{
	if (!mEventID.isEmpty()  &&  mEventID != ev.VUID())
		return false;
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
	suffix += SEPARATOR + (mFile ? FILE_PREFIX : TEXT_PREFIX) + mMessageOrFile;
	al->setText(QString::number(sequence) + SEPARATOR + suffix);
	al->setTime(mDateTime);
	al->setRepeatCount(mRepeatCount);
	al->setSnoozeTime(mRepeatMinutes);
	QDateTime dt = mDateTime;
	if (mRepeatAtLogin)
	{
		al = ev.newAlarm();
		al->setEnabled(true);        // enable the alarm
		al->setText(QString::number(sequence + REPEAT_AT_LOGIN_OFFSET)
						+ SEPARATOR + AT_LOGIN_CODE + suffix);
		QDateTime dtl = QDateTime::currentDateTime().addSecs(-KAlarmApp::MAX_LATENESS - 1);
		al->setTime(dtl);
		if (dtl < dt)
			dt = dtl;
	}
	ev.setDtStart(dt);
	ev.setDtEnd(dt);
	ev.setReadOnly(readOnly);
	return true;
}

KAlarmAlarm KAlarmEvent::alarm(int alarmID) const
{
	KAlarmAlarm al;
	al.mEventID       = mEventID;
	al.mMessageOrFile = mMessageOrFile;
	al.mFile          = mFile;
	al.mColour        = mColour;
	al.mBeep          = mBeep;
	if (alarmID == mMainAlarmID)
	{
		al.mAlarmSeq      = mMainAlarmID;
		al.mDateTime      = mDateTime;
		al.mRepeatCount   = mRepeatCount;
		al.mRepeatMinutes = mRepeatMinutes;
		al.mLateCancel    = mLateCancel;
	}
	else if (alarmID == mRepeatAtLoginAlarmID  &&  mRepeatAtLogin)
	{
		al.mAlarmSeq      = mRepeatAtLoginAlarmID;
		al.mDateTime      = mRepeatAtLoginDateTime;
//		al.mDateTime      = QDateTime::currentDateTime().addSecs(-KAlarmApp::MAX_LATENESS - 1);
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
