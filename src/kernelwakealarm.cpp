/*
 *  kernelwakealarm.cpp  -  kernel alarm to wake from suspend
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2023 one-d-wide <one-d-wide@protonmail.com>
 *  SPDX-FileCopyrightText: 2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kernelwakealarm.h"

#include "kalarmcalendar/kadatetime.h"
#include "kalarm_debug.h"

#ifdef Q_OS_LINUX

#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/timerfd.h>

int KernelWakeAlarm::mAvailable = 0;  // 0 = unchecked, 1 = unavailable, 2 = available

KernelWakeAlarm::KernelWakeAlarm()
{
    // `timerfd_create(2)`
    int ret = timerfd_create(CLOCK_REALTIME_ALARM, 0);

    if (ret >= 0)
    {
        mTimerFd = ret;
        mAvailable = 2;
    }
    else
    {
        mAvailable = 1;
        switch (errno)
        {
            case EPERM:
                qCWarning(KALARM_LOG) << "KernelWakeAlarm: Error: CAP_WAKE_ALARM is not set";
                break;
            case EINVAL:
                qCWarning(KALARM_LOG) << "KernelWakeAlarm: Error: CLOCK_REALTIME_ALARM is not supported";
                break;
            default:
                qCWarning(KALARM_LOG) << "KernelWakeAlarm: Error creating kernel timer:" << strerror(errno);
                mAvailable = 2;
                break;
        }
    }
}

KernelWakeAlarm::KernelWakeAlarm(const KernelWakeAlarm& other)
    : KernelWakeAlarm()
{
    if (other.mTriggerTime > 0)
        arm(other.mTriggerTime);
}

KernelWakeAlarm::~KernelWakeAlarm()
{
    if (mTimerFd)
        close(mTimerFd.value());
}

KernelWakeAlarm& KernelWakeAlarm::operator=(const KernelWakeAlarm& other)
{
    if (other.mTriggerTime > 0)
        arm(other.mTriggerTime);
    return *this;
}

bool KernelWakeAlarm::isValid() const
{
    return mTimerFd.has_value();
}

bool KernelWakeAlarm::isAvailable()
{
    if (!mAvailable)
        KernelWakeAlarm a;
    return mAvailable == 2;
}

bool KernelWakeAlarm::arm(const KAlarmCal::KADateTime& triggerTime)
{
    if (triggerTime.isValid())
    {
        const time_t triggerSeconds = static_cast<time_t>(triggerTime.toSecsSinceEpoch());
        if (arm(triggerSeconds))
        {
            mTriggerTime = triggerSeconds;
            qCDebug(KALARM_LOG) << "KernelWakeAlarm::arm: Kernel timer set to:" << triggerTime.qDateTime();
            return true;
        }
    }
    return false;
}

bool KernelWakeAlarm::arm(time_t triggerSeconds)
{
    if (!mTimerFd)
        return false;
    if (triggerSeconds <= ::time(nullptr))
        return false;    // already expired
    struct itimerspec time = {};
    time.it_value.tv_sec = triggerSeconds;

    // `timerfd_settime(2)`
    if (timerfd_settime(mTimerFd.value(), TFD_TIMER_ABSTIME, &time, nullptr) < 0)
    {
        qCWarning(KALARM_LOG) << "KernelWakeAlarm::arm: Failed to set kernel timer:" << strerror(errno);

        return false;
    }
    return true;
}

void KernelWakeAlarm::disarm()
{
    if (arm(0))
        mTriggerTime = 0;
}

#else // not Q_OS_LINUX

KernelWakeAlarm::KernelWakeAlarm() {}
KernelWakeAlarm::KernelWakeAlarm(const KernelWakeAlarm& other) {}
KernelWakeAlarm::~KernelWakeAlarm() {}
KernelWakeAlarm& KernelWakeAlarm::operator=(const KernelWakeAlarm& other) { return *this; }
bool KernelWakeAlarm::arm(QDateTime)  { return false; }
void KernelWakeAlarm::disarm() {}
bool KernelWakeAlarm::isValid() const { return false; }
bool KernelWakeAlarm::isAvailable()   { return false; }

#endif // Q_OS_LINUX

// vim: et sw=4:
