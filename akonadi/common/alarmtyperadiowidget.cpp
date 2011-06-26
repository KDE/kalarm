/*
 *  alarmtyperadiowidget.cpp  -  KAlarm alarm type exclusive selection widget
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "alarmtyperadiowidget.h"


AlarmTypeRadioWidget::AlarmTypeRadioWidget(QWidget* parent)
    : Akonadi::SingleFileValidatingWidget(parent)
{
    ui.setupUi(this);
    ui.mainLayout->setContentsMargins(0, 0, 0, 0);
    mButtonGroup = new QButtonGroup(ui.groupBox);
    mButtonGroup->addButton(ui.activeRadio);
    mButtonGroup->addButton(ui.archivedRadio);
    mButtonGroup->addButton(ui.templateRadio);
    connect(ui.activeRadio, SIGNAL(toggled(bool)), SIGNAL(changed()));
    connect(ui.archivedRadio, SIGNAL(toggled(bool)), SIGNAL(changed()));
    connect(ui.templateRadio, SIGNAL(toggled(bool)), SIGNAL(changed()));
}

void AlarmTypeRadioWidget::setAlarmType(KAlarm::CalEvent::Type type)
{
    switch (type)
    {
        case KAlarm::CalEvent::ACTIVE:
            ui.activeRadio->setChecked(true);
            break;
        case KAlarm::CalEvent::ARCHIVED:
            ui.archivedRadio->setChecked(true);
            break;
        case KAlarm::CalEvent::TEMPLATE:
            ui.templateRadio->setChecked(true);
            break;
        default:
            break;
    }
}

KAlarm::CalEvent::Type AlarmTypeRadioWidget::alarmType() const
{
    if (ui.activeRadio->isChecked())
        return KAlarm::CalEvent::ACTIVE;
    if (ui.archivedRadio->isChecked())
        return KAlarm::CalEvent::ARCHIVED;
    if (ui.templateRadio->isChecked())
        return KAlarm::CalEvent::TEMPLATE;
    return KAlarm::CalEvent::EMPTY;
}

bool AlarmTypeRadioWidget::validate() const
{
    return static_cast<bool>(mButtonGroup->checkedButton());
}

// vim: et sw=4:
