/*
 *  startdaytimer.h  -  timer triggered at the user-defined start-of-day time
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

#ifndef STARTDAYTIMER_H
#define STARTDAYTIMER_H

/* @file startdaytimer.h - timer triggered at the user-defined start-of-day time */

#include "synchtimer.h"


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

#endif // STARTDAYTIMER_H

// vim: et sw=4:
