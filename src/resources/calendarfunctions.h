/*
 *  calendarfunctions.h  -  miscellaneous calendar access functions
 *  Program:  kalarm
 *  Copyright © 2020 David Jarvie <djarvie@kde.org>
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
