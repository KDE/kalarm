/*
 *  timezonecombo.cpp  -  time zone selection combo box
 *  Program:  kalarm
 *  Copyright © 2006,2008 by David Jarvie <djarvie@kde.org>
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

#include <ksystemtimezone.h>
#include "timezonecombo.moc"


TimeZoneCombo::TimeZoneCombo(QWidget* parent)
	: ComboBox(parent)
{
	QString utc = KTimeZone::utc().name();
	addItem(utc);   // put UTC at start of list
	const KTimeZones::ZoneMap zones = KSystemTimeZones::zones();
	for (KTimeZones::ZoneMap::ConstIterator it = zones.constBegin();  it != zones.constEnd();  ++it)
		if (it.key() != utc)
			addItem(it.key());
}

KTimeZone TimeZoneCombo::timeZone() const
{
	return KSystemTimeZones::zone(currentText());
}

void TimeZoneCombo::setTimeZone(const KTimeZone& tz)
{
	if (!tz.isValid())
		return;
	QString zone = tz.name();
	int limit = count();
	for (int index = 0;  index < limit;  ++index)
	{
		if (itemText(index) == zone)
		{
			setCurrentIndex(index);
			break;
		}
	}
}
