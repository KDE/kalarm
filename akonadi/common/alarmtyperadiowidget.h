/*
 *  alarmtyperadiowidget.h  -  KAlarm alarm type exclusive selection widget
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

#ifndef ALARMTYPERADIOWIDGET_H
#define ALARMTYPERADIOWIDGET_H

#include "singlefileresourceconfigdialogbase.h"
#include "ui_alarmtyperadiowidget.h"

#include <kalarmcal/kacalendar.h>

using namespace KAlarmCal;

class QButtonGroup;

class AlarmTypeRadioWidget : public Akonadi::SingleFileValidatingWidget
{
        Q_OBJECT
    public:
        explicit AlarmTypeRadioWidget(QWidget* parent = 0);
        void setAlarmType(CalEvent::Type);
        CalEvent::Type alarmType() const;
        virtual bool validate() const;

    private:
        Ui::AlarmTypeRadioWidget ui;
        QButtonGroup* mButtonGroup;
};

#endif // ALARMTYPERADIOWIDGET_H

// vim: et sw=4:
