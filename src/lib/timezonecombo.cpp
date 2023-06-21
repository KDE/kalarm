/*
 *  timezonecombo.cpp  -  time zone selection combo box
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2006, 2008, 2009, 2011, 2018 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
            addItem(i18n(zone.constData()).replace(QLatin1Char('_'), QLatin1Char(' ')));
        }
}

QTimeZone TimeZoneCombo::timeZone() const
{
    if (!currentIndex())
        return {};
    return QTimeZone(mZoneNames[currentIndex()]);
}

void TimeZoneCombo::setTimeZone(const QTimeZone& tz)
{
    int index = tz.isValid() ? mZoneNames.indexOf(tz.id()) : 0;
    if (index >= 0)
        setCurrentIndex(index);
}

// vim: et sw=4:

#include "moc_timezonecombo.cpp"
