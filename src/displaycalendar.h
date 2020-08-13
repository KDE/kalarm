/*
 *  displaycalendar.h  -  KAlarm display calendar file access
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
