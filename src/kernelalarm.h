/*
 *  kernelalarm.h  -  kernel alarm interface
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2023 one-d-wide <one-d-wide@protonmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <optional>
#include <QtGlobal>
#include <QDateTime>

/*==============================================================================
= Manages single alarm instance that wakes system upon expiring
=
= * Supported on:
=    * Linux (if `CAP_WAKE_ALARM` is set, see `capabilities(7)`)
=
= * Destroying the instance will disarm the alarm
==============================================================================*/
class KernelAlarm
{
public:
    KernelAlarm();
    ~KernelAlarm();
    bool isAvailable();
    bool arm(QDateTime triggerTime);
    void disArm();

    KernelAlarm(KernelAlarm const&);
    KernelAlarm& operator=(KernelAlarm const&);

private:
    std::optional<QDateTime> mTriggerTime;
#ifdef Q_OS_LINUX
    std::optional<int> mTimerFd;
#endif
};

// vim: et sw=4:
