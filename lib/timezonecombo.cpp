/*
 *  timezonecombo.cpp  -  time zone selection combo box
 *  Program:  kalarm
 *  Copyright © 2006,2008,2009,2011 by David Jarvie <djarvie@kde.org>
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

#include "timezonecombo.h"
#include <ksystemtimezone.h>
#include <kglobal.h>
#include <KLocalizedString>


TimeZoneCombo::TimeZoneCombo(QWidget* parent)
    : ComboBox(parent)
{
    QString utc = KTimeZone::utc().name();
    addItem(utc);   // put UTC at start of list
    mZoneNames << utc;
    const KTimeZones::ZoneMap zones = KSystemTimeZones::zones();
    KTimeZones::ZoneMap::ConstIterator end = zones.constEnd();
    for (KTimeZones::ZoneMap::ConstIterator it = zones.constBegin();  it != end;  ++it)
        if (it.key() != utc)
        {
            mZoneNames << it.key();
            addItem(i18n(it.key().toUtf8()).replace(QLatin1Char('_'), QLatin1Char(' ')));
        }
}

KTimeZone TimeZoneCombo::timeZone() const
{
    return KSystemTimeZones::zone(mZoneNames[currentIndex()]);
}

void TimeZoneCombo::setTimeZone(const KTimeZone& tz)
{
    if (!tz.isValid())
        return;
    int index = mZoneNames.indexOf(tz.name());
    if (index >= 0)
        setCurrentIndex(index);
}

// vim: et sw=4:
