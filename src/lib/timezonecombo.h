/*
 *  timezonecombo.cpp  -  time zone selection combo box
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2006, 2009, 2011, 2018 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

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


// vim: et sw=4:
