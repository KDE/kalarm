/*
 *  startdaytimer.cpp  -  timer triggered at the user-defined start-of-day time
 *  Program:  kalarm
 *  Copyright © 2004,2005,2009 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"

#include "preferences.h"
#include "startdaytimer.h"


StartOfDayTimer* StartOfDayTimer::mInstance = 0;

StartOfDayTimer::StartOfDayTimer()
    : DailyTimer(Preferences::startOfDay(), false)
{
    Preferences::connect(SIGNAL(startOfDayChanged(QTime)), this, SLOT(startOfDayChanged()));
}

StartOfDayTimer* StartOfDayTimer::instance()
{
    if (!mInstance)
        mInstance = new StartOfDayTimer;    // receive notifications of change of start-of-day time
    return mInstance;
}

/******************************************************************************
* Called when the start-of-day time has changed.
* The timer is adjusted and if appropriate timer events are triggered now.
*/
void StartOfDayTimer::startOfDayChanged()
{
    changeTime(Preferences::startOfDay(), true);
}

#include "moc_startdaytimer.cpp"
// vim: et sw=4:
