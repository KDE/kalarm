/*
 *  kernelalarm.cpp  -  kernel alarm interface
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2023 one-d-wide <one-d-wide@protonmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kernelalarm.h"
#include "kalarm_debug.h"

#include <QtGlobal>
#include <QDateTime>
#include <QLoggingCategory>

#ifdef Q_OS_LINUX

#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/timerfd.h>

KernelAlarm::KernelAlarm()
{
    // `timerfd_create(2)`
    int ret = timerfd_create(CLOCK_REALTIME_ALARM, 0);

    if (ret >= 0)
        mTimerFd = ret;
    else
    {
        qCWarning(KALARM_LOG) << "KernelAlarm::KernelAlarm:" << "Unable to create kernel timer:" << strerror(errno);
        switch errno
        {
            case EPERM:
                qCWarning(KALARM_LOG) << "KernelAlarm::KernelAlarm:" <<"CAP_WAKE_ALARM is not set";
                break;
            case EINVAL:
                qCWarning(KALARM_LOG) << "KernelAlarm::KernelAlarm:" << "CLOCK_REALTIME_ALARM is not supported";
                break;
            default:
                break;
        }
    }
}

KernelAlarm::~KernelAlarm()
{
    if (mTimerFd)
        close(mTimerFd.value());
}

bool KernelAlarm::arm(QDateTime triggerTime)
{
    if (!mTimerFd)
        return false;

    struct itimerspec time = {};
    time.it_value.tv_sec = triggerTime.toSecsSinceEpoch();

    // `timerfd_settime(2)`
    if (timerfd_settime(mTimerFd.value(), TFD_TIMER_ABSTIME, &time, nullptr) == 0)
    {
        if (triggerTime.toSecsSinceEpoch() != 0)
        {
            mTriggerTime = triggerTime;
            qCDebug(KALARM_LOG) << "KernelAlarm::arm:" << "Kernel timer has been set successfully!  time =" << triggerTime;
        }
        else
            mTriggerTime = {};
        return true;
    }
    else
        qCWarning(KALARM_LOG) << "KernelAlarm::arm:" << "Failed to set kernel timer:" << strerror(errno);

    return false;
}

void KernelAlarm::disArm()
{
    arm(QDateTime::fromSecsSinceEpoch(0));
}

bool KernelAlarm::isAvailable()
{
    return mTimerFd.has_value();
}

// endif Q_OS_LINUX
#else

KernelAlarm::KernelAlarm() {}
KernelAlarm::~KernelAlarm() {}
bool KernelAlarm::arm(QDateTime) { return false; }
void KernelAlarm::disArm() {}
bool KernelAlarm::isAvailable() { return false; }

#endif

KernelAlarm::KernelAlarm(KernelAlarm const& other) : KernelAlarm()
{
    if (other.mTriggerTime)
        arm(other.mTriggerTime.value());
}

KernelAlarm& KernelAlarm::operator=(KernelAlarm const& other)
{
    if (other.mTriggerTime)
        arm(other.mTriggerTime.value());
    return *this;
}

// vim: et sw=4:
