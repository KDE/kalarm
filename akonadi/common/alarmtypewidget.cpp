/*
 *  alarmtypewidget.cpp  -  KAlarm Akonadi configuration alarm type selection widget
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

#include "alarmtypewidget.h"


AlarmTypeWidget::AlarmTypeWidget(QWidget* parent, QLayout* layout)
    : QWidget()
{
    ui.setupUi(parent);
    layout->addWidget(ui.groupBox);
    connect(ui.activeCheckBox, SIGNAL(toggled(bool)), SIGNAL(changed()));
    connect(ui.archivedCheckBox, SIGNAL(toggled(bool)), SIGNAL(changed()));
    connect(ui.templateCheckBox, SIGNAL(toggled(bool)), SIGNAL(changed()));
}

void AlarmTypeWidget::setAlarmTypes(CalEvent::Types types)
{
    if (types & CalEvent::ACTIVE)
        ui.activeCheckBox->setChecked(true);
    if (types & CalEvent::ARCHIVED)
        ui.archivedCheckBox->setChecked(true);
    if (types & CalEvent::TEMPLATE)
        ui.templateCheckBox->setChecked(true);
}

CalEvent::Types AlarmTypeWidget::alarmTypes() const
{
    CalEvent::Types types = CalEvent::EMPTY;
    if (ui.activeCheckBox->isChecked())
        types |= CalEvent::ACTIVE;
    if (ui.archivedCheckBox->isChecked())
        types |= CalEvent::ARCHIVED;
    if (ui.templateCheckBox->isChecked())
        types |= CalEvent::TEMPLATE;
    return types;
}

// vim: et sw=4:
