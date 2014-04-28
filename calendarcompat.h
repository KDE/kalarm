/*
 *  calendarcompat.h  -  compatibility for old calendar file formats
 *  Program:  kalarm
 *  Copyright © 2005-2011 by David Jarvie <djarvie@kde.org>
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

#ifndef CALENDARCOMPAT_H
#define CALENDARCOMPAT_H

#ifndef USE_AKONADI

#include "alarmresource.h"
#include <KAlarmCal/kacalendar.h>
namespace KCal { class CalendarLocal; }


namespace CalendarCompat
{
    KACalendar::Compat fix(KCal::CalendarLocal&, const QString& localFile,
                           AlarmResource* = 0, AlarmResource::FixFunc = AlarmResource::PROMPT, bool* wrongType = 0);
}

#endif

#endif // CALENDARCOMPAT_H

// vim: et sw=4:
