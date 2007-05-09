/*
 *  calendarcompat.h  -  compatibility for old calendar file formats
 *  Program:  kalarm
 *  Copyright © 2005,2007 by David Jarvie <software@astrojar.org.uk>
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

/* @file calendarcompat.h - compatibility for old calendar file formats */

#include <qstring.h>
namespace KCal { class Calendar; }


class CalendarCompat
{
	public:
		static void fix(KCal::Calendar&, const QString& localFile = QString::null);

	private:
		static int  readKAlarmVersion(KCal::Calendar&, QString& subVersion);
		static bool isUTC(const QString& localFile);
};

#endif // CALENDARCOMPAT_H
