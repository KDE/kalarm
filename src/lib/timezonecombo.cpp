/*
 *  timezonecombo.cpp  -  time zone selection combo box
 *  Program:  kalarm
 *  Copyright © 2006,2008,2009,2011,2018 by David Jarvie <djarvie@kde.org>
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
#include <KLocalizedString>
#include <QTimeZone>


TimeZoneCombo::TimeZoneCombo(QWidget* parent)
    : ComboBox(parent)
{
    addItem(i18n("System time zone"));   // put System at start of list
    mZoneNames << "System";
    const QByteArray utc = QTimeZone::utc().id();
    addItem(QString::fromLatin1(utc));            // put UTC second in list
    mZoneNames << utc;
    const QList<QByteArray> zones = QTimeZone::availableTimeZoneIds();
    for (const QByteArray& zone : zones)
        if (zone != utc)
        {
            mZoneNames << zone;
            addItem(i18n(zone).replace(QLatin1Char('_'), QLatin1Char(' ')));
        }
}

QTimeZone TimeZoneCombo::timeZone() const
{
    if (!currentIndex())
        return QTimeZone();
    return QTimeZone(mZoneNames[currentIndex()]);
}

void TimeZoneCombo::setTimeZone(const QTimeZone& tz)
{
    int index = tz.isValid() ? mZoneNames.indexOf(tz.id()) : 0;
    if (index >= 0)
        setCurrentIndex(index);
}

// vim: et sw=4:
