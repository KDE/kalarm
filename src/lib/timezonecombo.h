/*
 *  timezonecombo.cpp  -  time zone selection combo box
 *  Program:  kalarm
 *  Copyright © 2006,2009,2011,2018 by David Jarvie <djarvie@kde.org>
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

#include "lib/combobox.h"

#include <QByteArray>

class QTimeZone;

/**
 *  @short A combo box for selecting a time zone, with a read-only option.
 *
 *  The first two entries are set to "System" and "UTC".
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
        explicit TimeZoneCombo(QWidget* parent = nullptr);

        /**
         * Returns the currently selected time zone.
         * @return The selected time zone, or invalid for the system time zone.
         */
        QTimeZone timeZone() const;

        /** Selects the specified time zone.
         *  @param tz The time zone to select, or invalid to select "System".
         */
        void setTimeZone(const QTimeZone& tz);

    private:
        QList<QByteArray> mZoneNames;
};

#endif // TIMEZONECOMBO_H

// vim: et sw=4:
