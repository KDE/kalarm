/*
 *  calendarfunctions.h  -  miscellaneous calendar access functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CALENDARFUNCTIONS_H
#define CALENDARFUNCTIONS_H

#include <KAlarmCal/KAEvent>

#include <QHash>
#include <QVector>

using namespace KAlarmCal;

class QUrl;
class QWidget;

namespace KAlarm
{

/** Read events from a calendar file. The events are converted to the current
 *  KAlarm format and are optionally given new unique event IDs.
 *
 *  @param url        URL of calendar file to read
 *  @param alarmTypes alarm types to read from calendar file; other types are ignored
 *  @param newId      whether to create new IDs for the events
 *  @param parent     parent widget for error messages
 *  @param events     imported alarms are appended to this list
 *  @return  true if the calendar file was read successfully.
 */
bool importCalendarFile(const QUrl& url, CalEvent::Types alarmTypes, bool newId, QWidget* parent, QHash<CalEvent::Type, QVector<KAEvent>>& events);

} // namespace KAlarm

#endif // CALENDARFUNCTIONS_H

// vim: et sw=4:
