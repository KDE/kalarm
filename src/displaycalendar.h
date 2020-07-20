/*
 *  displaycalendar.h  -  KAlarm display calendar file access
 *  Program:  kalarm
 *  Copyright © 2001-2020 David Jarvie <djarvie@kde.org>
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

#ifndef DISPLAYCALENDAR_H
#define DISPLAYCALENDAR_H

#include <KAlarmCal/KAEvent>

#include <KCalendarCore/FileStorage>
#include <KCalendarCore/Event>

#include <QHash>

using namespace KAlarmCal;


/** Provides read and write access to the display calendar.
 *  This stores alarms currently being displayed, to enable them to be
 *  redisplayed if KAlarm is killed and restarted.
 */
class DisplayCalendar
{
public:
    static bool                       save();
    static KCalendarCore::Event::Ptr  kcalEvent(const QString& uniqueID);
    static KCalendarCore::Event::List kcalEvents(CalEvent::Type s = CalEvent::EMPTY);
    static bool                       addEvent(KAEvent&);
    static bool                       deleteEvent(const QString& eventID, bool save = false);
    static bool                       isOpen()               { return mInitialised && mOpen; }
    static void                       adjustStartOfDay();

    static void                       initialise();
    static bool                       open();
    static void                       terminate();

private:
    enum CalType { LOCAL_ICAL, LOCAL_VCAL };

    static int                        load();
    static void                       close();
    static bool                       saveCal(const QString& newFile = QString());
    static bool                       isValid()    { return mCalendarStorage; }
    static void                       updateKAEvents();

    static bool                       mInitialised;        // whether the calendar has been initialised
    static KAEvent::List              mEventList;
    static QHash<QString, KAEvent*>   mEventMap;           // lookup of all events by UID
    static KCalendarCore::FileStorage::Ptr mCalendarStorage;
    static QString                    mDisplayCalPath;     // path of display calendar file
    static QString                    mDisplayICalPath;    // path of display iCalendar file
    static CalType                    mCalType;            // mCalendar's type (ical/vcal)
    static bool                       mOpen;               // true if the calendar file is open
};

#endif // DISPLAYCALENDAR_H

// vim: et sw=4:
