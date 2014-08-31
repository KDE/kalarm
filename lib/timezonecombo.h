/*
 *  timezonecombo.cpp  -  time zone selection combo box
 *  Program:  kalarm
 *  Copyright © 2006,2009,2011 by David Jarvie <djarvie@kde.org>
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

#ifndef TIMEZONECOMBO_H
#define TIMEZONECOMBO_H

#include <combobox.h>
#include <QStringList>

class KTimeZone;

/**
 *  @short A combo box for selecting a time zone, with a read-only option.
 *
 *  The widget may be set as read-only. This has the same effect as disabling it, except
 *  that its appearance is unchanged.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class TimeZoneCombo : public ComboBox
{
        Q_OBJECT
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         */
        explicit TimeZoneCombo(QWidget* parent = 0);
        /** Returns the currently selected time zone, or null if none. */
        KTimeZone timeZone() const;
        /** Selects the specified time zone. */
        void setTimeZone(const KTimeZone& tz);

    private:
        QStringList mZoneNames;
};

#endif // TIMEZONECOMBO_H

// vim: et sw=4:
