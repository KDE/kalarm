/*
 *  msgevent.h  -  the event object for messages
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  djarvie@lineone.net
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef MSGEVENT_H
#define MSGEVENT_H

#include <event.h>

/*
 * KAlarm events are stored as alarms in the calendar file, as follows:
 *   next time/date - stored as the alarm time (TRIGGER field)
 *   message text - stored as the alarm description, with prefix TEXT: (DESCRIPTION field)
 *   file name to display text from - stored as the alarm description, with prefix FILE: (DESCRIPTION field)
 *   colour - stored as a hex string prefixed by #, as the first category (CATEGORIES field)
 *   elapsed repeat count - stored as the revision number (SEQUENCE field)
 *   beep - stored as a "BEEP" category (CATEGORIES field)
 *   late cancel - DTEND set to be different from DTSTART.
 */
class MessageEvent : public KCal::Event
{
	public:
		enum
		{
			LATE_CANCEL = 0x01,
			BEEP        = 0x02
		};
		MessageEvent()  { }
		MessageEvent(const QDateTime& dt, int flags, const QColor& colour, const QString& message, bool file)
							  { setMessage(dt, flags, colour, message, file); }
		MessageEvent(const MessageEvent&);
		~MessageEvent()  { }
		void             set(const QDateTime&, bool lateCancel);
		void             setMessage(const QDateTime& dt, int flags, const QColor& c, const QString& message, bool file)
		                            { setMessage(dt, flags, c, message, (file ? 1 : 0)); }
		void             setFileName(const QDateTime& dt, int flags, const QColor& c, const QString& filename)
		                            { setMessage(dt, flags, c, filename, 1); }
		void             setRepetition(int minutes, int initialCount, int remainingCount = -1);
		void             updateRepetition(const QDateTime&, int remainingCount);
		const QDateTime& dateTime() const           { return alarm()->time(); }
		QDate            date() const               { return alarm()->time().date(); }
		QTime            time() const               { return alarm()->time().time(); }
		QString          cleanText() const;         // text with prefix cleaned off
		QString          message() const;           // message text, or null if not a text message
		QString          fileName() const;          // file name, or null if not a file name
		int              repeatCount() const        { return alarm()->repeatCount(); }
		int              repeatMinutes() const      { return alarm()->snoozeTime(); }
		int              initialRepeatCount() const { return alarm()->repeatCount() + revision(); }
		QDateTime        lastDateTime() const       { return alarm()->time().addSecs(alarm()->repeatCount() * repeatMinutes() * 60); }
		QColor           colour() const;
		int              flags() const;
		bool             messageIsFileName() const;
		bool             lateCancel() const         { return !isMultiDay(); }
		bool             beep() const               { return !!(flags() & BEEP); }
	private:
		void             setMessage(const QDateTime&, int flags, const QColor&, const QString& message, int type);
};

#endif // MSGEVENT_H
