/*
 *  resourcebase.cpp  -  base class for an alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2019 David Jarvie <djarvie@kde.org>
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

#include "resourcebase.h"
#include "preferences.h"
#include "kalarm_debug.h"

#include <KColorScheme>
#include <KColorUtils>
#include <KLocalizedString>

ResourceBase::~ResourceBase()
{
}

bool ResourceBase::isEnabled(CalEvent::Type type) const
{
    return (type == CalEvent::EMPTY) ? enabledTypes() : enabledTypes() & type;
}

bool ResourceBase::isWritable(CalEvent::Type type) const
{
    return writableStatus(type) == 1;
}

bool ResourceBase::isCompatible() const
{
    return compatibility() == KACalendar::Current;
}

/******************************************************************************
* Return the foreground colour for displaying a resource, based on the alarm
* types which it contains, and on whether it is fully writable.
*/
QColor ResourceBase::foregroundColour(CalEvent::Types types) const
{
    if (types == CalEvent::EMPTY)
        types = alarmTypes();
    else
        types &= alarmTypes();

//TODO: Should this look for the first writable alarm type?
    CalEvent::Type type;
    if (types & CalEvent::ACTIVE)
        type = CalEvent::ACTIVE;
    else if (types & CalEvent::ARCHIVED)
        type = CalEvent::ARCHIVED;
    else if (types & CalEvent::TEMPLATE)
        type = CalEvent::TEMPLATE;
    else
        type = CalEvent::EMPTY;

    QColor colour;
    switch (type)
    {
        case CalEvent::ACTIVE:
            colour = KColorScheme(QPalette::Active).foreground(KColorScheme::NormalText).color();
            break;
        case CalEvent::ARCHIVED:
            colour = Preferences::archivedColour();
            break;
        case CalEvent::TEMPLATE:
            colour = KColorScheme(QPalette::Active).foreground(KColorScheme::LinkText).color();
            break;
        default:
            break;
    }
    if (colour.isValid()  &&  !isWritable(type))
        return KColorUtils::lighten(colour, 0.2);
    return colour;
}

QString ResourceBase::storageTypeString(bool description, bool file, bool local) const
{
    if (description)
        return file ? i18nc("@info", "KAlarm Calendar File") : i18nc("@info", "KAlarm Calendar Directory");
    return (file && local)  ? i18nc("@info", "File")
         : (file && !local) ? i18nc("@info", "URL")
         : (!file && local) ? i18nc("@info Directory in filesystem", "Directory")
         : QString();
}

// vim: et sw=4:
