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
 *   time/date - stored as the alarm time (TRIGGER field)
 *   message text - stored as the alarm description (DESCRIPTION field)
 *   colour - stored as a hex string prefixed by #, as the first category (CATEGORIES field)
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
    MessageEvent(const QDateTime& dt, int flags, const QColor& colour, const QString& message)
                                        { set(dt, flags, colour, message); }
    ~MessageEvent()  { }
    void             set(const QDateTime&, int flags, const QColor&, const QString& message);
    const QDateTime& dateTime() const  	{ return alarm()->time(); }
    QDate            date() const      	{ return alarm()->time().date(); }
    QTime            time() const      	{ return alarm()->time().time(); }
    const QString&   message() const    { return alarm()->text(); }
    QColor           colour() const;
    int              flags() const;
    bool             lateCancel() const { return !isMultiDay(); }
    bool             beep() const       { return !!(flags() & BEEP); }
};

#endif // MSGEVENT_H
