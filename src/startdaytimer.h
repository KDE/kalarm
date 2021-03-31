/*
 *  startdaytimer.h  -  timer triggered at the user-defined start-of-day time
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004, 2005, 2009 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

/* @file startdaytimer.h - timer triggered at the user-defined start-of-day time */

#include "lib/synchtimer.h"


/** StartOfDayTimer is an application-wide timer synchronized to the user-defined
 *  start-of-day time (set in KAlarm's Preferences dialog).
 *  It automatically adjusts to any changes in the start-of-day time.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class StartOfDayTimer : public DailyTimer
{
        Q_OBJECT
    public:
        virtual ~StartOfDayTimer()  { }
        /** Connect to the timer signal.
         *  @param receiver Receiving object.
         *  @param member Slot to activate.
         */
        static void connect(QObject* receiver, const char* member)
                           { instance()->connecT(receiver, member); }
        /** Disconnect from the timer signal.
         *  @param receiver Receiving object.
         *  @param member Slot to disconnect. If null, all slots belonging to
         *                @p receiver will be disconnected.
         */
        static void disconnect(QObject* receiver, const char* member = nullptr)
                           { if (mInstance) mInstance->disconnecT(receiver, member); }

    protected:
        StartOfDayTimer();
        static StartOfDayTimer* instance();

    private Q_SLOTS:
        void startOfDayChanged();

    private:
        static StartOfDayTimer* mInstance;    // exists solely to receive signals
};


// vim: et sw=4:
