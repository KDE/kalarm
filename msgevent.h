/*
 *  msgevent.h  -  the event object for messages
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

#ifndef KALARMEVENT_H
#define KALARMEVENT_H

#include <event.h>

#include <kapp.h>
#if KDE_VERSION < 290
   #define Alarm KOAlarm
#endif

/*
 * KAlarm events are stored as alarms in the calendar file, as follows:
 * In the alarm object:
 *   next time/date - stored as the alarm time (alarm TRIGGER field)
 *   message text - stored as the alarm description, with prefix TEXT: (alarm DESCRIPTION field)
 *   file name to display text from - stored as the alarm description, with prefix FILE: (alarm DESCRIPTION field)
 *   late cancel, repeat at login - stored in prefix to the alarm description (alarm DESCRIPTION field)
 * In the event object:
 *   colour - stored as a hex string prefixed by #, as the first category (event CATEGORIES field)
 *   elapsed repeat count - stored as the revision number (event SEQUENCE field)
 *   beep - stored as a "BEEP" category (event CATEGORIES field)
 */

// KAlarmAlarm corresponds to a single KCal::Alarm instance
class KAlarmAlarm
{
	public:
		KAlarmAlarm()    : mAlarmSeq(-1), mRepeatCount(0), mBeep(false), mRepeatAtLogin(false), mLateCancel(false) { }
		~KAlarmAlarm()  { }
		void             set(int flags);
		bool             valid() const              { return mAlarmSeq > 0; }
		int              id() const                 { return mAlarmSeq; }
		int              sequence() const           { return mAlarmSeq; }
		const QString&   eventID() const            { return mEventID; }
		const QDateTime& dateTime() const           { return mDateTime; }
		QDate            date() const               { return mDateTime.date(); }
		QTime            time() const               { return mDateTime.time(); }
		QString          message() const            { return mFile ? QString::null : mMessageOrFile; }
		QString          fileName() const           { return mFile ? mMessageOrFile : QString::null; }
		const QColor&    colour() const             { return mColour; }
		int              repeatCount() const        { return mRepeatCount; }
		int              repeatMinutes() const      { return mRepeatMinutes; }
		QDateTime        lastDateTime() const       { return mDateTime.addSecs(mRepeatCount * mRepeatMinutes * 60); }
		bool             messageIsFileName() const  { return mFile; }
		bool             lateCancel() const         { return mLateCancel; }
		bool             repeatAtLogin() const      { return mRepeatAtLogin; }
		bool             beep() const               { return mBeep; }
		int              flags() const;

		QString          mEventID;          // KCal::Event unique ID
		QString          mMessageOrFile;    // message text or file URL
		QDateTime        mDateTime;         // next time to display the alarm
		QColor           mColour;           // background colour of alarm message
		int              mAlarmSeq;         // sequence number of main alarm
		int              mRepeatCount;      // number of times to repeat the alarm
		int              mRepeatMinutes;    // interval (minutes) between repeated alarms
		bool             mBeep;             // whether to beep when the alarm is displayed
		bool             mFile;             // mMessageOrFile is a file URL
		bool             mRepeatAtLogin;    // whether to repeat the alarm at every login
		bool             mLateCancel;       // whether to cancel the alarm if it can't be displayed on time
};


// KAlarmEvent corresponds to a KCal::Event instance
class KAlarmEvent
{
	public:
		enum            // used in DCOP calls, etc.
		{
			LATE_CANCEL     = 0x01,
			BEEP            = 0x02,
			REPEAT_AT_LOGIN = 0x04
		};
		KAlarmEvent()    : mRevision(0), mMainAlarmID(1), mRepeatCount(0) { }
		KAlarmEvent(const QDateTime& dt, const QString& message, const QColor& c, bool file, int flags, int repeatCount = 0, int repeatMinutes = 0)
		                                                                  { set(dt, message, c, file, flags, repeatCount, repeatMinutes); }
		explicit KAlarmEvent(const KCal::Event& e)  { set(e); }
		~KAlarmEvent()  { }
		void             set(const KCal::Event&);
		void             set(const QDateTime& dt, const QString& message, const QColor&, bool file, int flags, int repeatCount = 0, int repeatMinutes = 0);
		void             setMessage(const QDateTime& dt, const QString& message, const QColor& c, int flags, int repeatCount = 0, int repeatMinutes = 0)
											                 { set(dt, message, c, false, flags, repeatCount, repeatMinutes); }
		void             setFileName(const QDateTime& dt, const QString& filename, const QColor& c, int flags, int repeatCount = 0, int repeatMinutes = 0)
											                 { set(dt, filename, c, true, flags, repeatCount, repeatMinutes); }
		void             setRepetition(int count, int minutes)   { mRepeatCount = count;  mRepeatMinutes = minutes; }
		void             updateRepetition(const QDateTime& dt, int count)  { mRepeatCount = count;  mDateTime = dt; }
		void             setEventID(const QString& id)                     { mEventID = id; }
		void             setTime(const QDateTime& dt)                      { mDateTime = dt; }
		void             setLateCancel(bool lc)                            { mLateCancel = lc; }
		void             set(int flags);
		KCal::Event*     event() const;    // convert to new Event
		KAlarmAlarm      alarm(int alarmID) const;
		KAlarmAlarm      firstAlarm() const;
		KAlarmAlarm      nextAlarm(const KAlarmAlarm&) const;
		bool             updateEvent(KCal::Event&) const;
		void             removeAlarm(int alarmID);
		void             incrementRevision()        { ++mRevision; }
		void             setUpdated()               { mUpdated = true; }
		bool             updated() const            { return mUpdated; }

		bool             operator==(const KAlarmEvent&);
		bool             operator!=(const KAlarmEvent& e)  { return !operator==(e); }
		const QString&   id() const                 { return mEventID; }
		int              alarmCount() const         { return mAlarmCount; }
		const QDateTime& dateTime() const           { return mDateTime; }
		QDate            date() const               { return mDateTime.date(); }
		QTime            time() const               { return mDateTime.time(); }
		QString          message() const            { return mFile ? QString::null : mMessageOrFile; }
		QString          fileName() const           { return mFile ? mMessageOrFile : QString::null; }
		QString          messageOrFile() const      { return mMessageOrFile; }
		const QColor&    colour() const             { return mColour; }
		int              repeatCount() const        { return mRepeatCount; }
		int              repeatMinutes() const      { return mRepeatMinutes; }
		QDateTime        lastDateTime() const       { return mDateTime.addSecs(mRepeatCount * mRepeatMinutes * 60); }
		bool             messageIsFileName() const  { return mFile; }
		bool             lateCancel() const         { return mLateCancel; }
		bool             repeatAtLogin() const      { return mRepeatAtLogin; }
		bool             beep() const               { return mBeep; }
		int              flags() const;

		static const int REPEAT_AT_LOGIN_OFFSET = 1;
	private:

		QString          mEventID;          // KCal::Event unique ID
		QString          mMessageOrFile;    // message text or file URL
		QDateTime        mDateTime;         // next time to display the alarm
		QDateTime        mRepeatAtLoginDateTime;  // repeat at login time
		QColor           mColour;           // background colour of alarm message
		int              mRevision;         // revision number of the original alarm, or 0
		int              mMainAlarmID;      // sequence number of main alarm
		int              mRepeatAtLoginAlarmID; // sequence number of repeat-at-login alarm
		int              mAlarmCount;       // number of alarms
		int              mRepeatCount;      // number of times to repeat the alarm
		int              mRepeatMinutes;    // interval (minutes) between repeated alarms
		bool             mBeep;             // whether to beep when the alarm is displayed
		bool             mFile;             // mMessageOrFile is a file URL
		bool             mRepeatAtLogin;    // whether to repeat the alarm at every login
		bool             mLateCancel;       // whether to cancel the alarm if it can't be displayed on time
		bool             mUpdated;          // event has been updated but not written to calendar file
};

#endif // KALARMEVENT_H
