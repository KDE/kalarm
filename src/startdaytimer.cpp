/*
 *  startdaytimer.cpp  -  timer triggered at the user-defined start-of-day time
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "startdaytimer.h"

#include "preferences.h"


StartOfDayTimer* StartOfDayTimer::mInstance = nullptr;

StartOfDayTimer::StartOfDayTimer()
    : DailyTimer(Preferences::startOfDay(), false)
{
    Preferences::connect(&Preferences::startOfDayChanged, this, &StartOfDayTimer::startOfDayChanged);
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
