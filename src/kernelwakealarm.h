/*
 *  kernelwakealarm.h  -  kernel alarm to wake from suspend
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2023 one-d-wide <one-d-wide@protonmail.com>
 *  SPDX-FileCopyrightText: 2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtGlobal>

#include <optional>

namespace KAlarmCal
{
class KADateTime;
}

/*==============================================================================
= Manages single kernel alarm instance that wakes system from suspend on expiry.
=
= Supported on:
=    * Linux (if `CAP_WAKE_ALARM` is set, see `capabilities(7)`)
=
= Destroying the instance will disarm the alarm
==============================================================================*/
class KernelWakeAlarm
{
public:
    KernelWakeAlarm();
    KernelWakeAlarm(const KernelWakeAlarm&);
    ~KernelWakeAlarm();

    KernelWakeAlarm& operator=(const KernelWakeAlarm&);

    /** Return whether this instance was constructed successfully and can be used. */
    bool isValid() const;

    bool arm(const KAlarmCal::KADateTime& triggerTime);
    void disarm();

    /** Determine whether kernel alarms can be set. */
    static bool isAvailable();

private:
#ifdef Q_OS_LINUX
    bool arm(time_t triggerSeconds);

    time_t             mTriggerTime;
    std::optional<int> mTimerFd;
    static int         mAvailable;
#endif
};

// vim: et sw=4:
