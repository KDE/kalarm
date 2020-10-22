/*
 *  synchtimer.cpp  -  timers which synchronize to time boundaries
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "synchtimer.h"

#include "kalarm_debug.h"

#include <QTimer>
#include <QDateTime>

/*=============================================================================
=  Class: SynchTimer
=  Virtual base class for application-wide timer synchronized to a time boundary.
=============================================================================*/

SynchTimer::SynchTimer()
{
    mTimer = new QTimer(this);
    mTimer->setSingleShot(true);
}

SynchTimer::~SynchTimer()
{
    delete mTimer;
    mTimer = nullptr;
}

/******************************************************************************
* Connect to the timer. The timer is started if necessary.
*/
void SynchTimer::connecT(QObject* receiver, const char* member)
{
    Connection connection(receiver, member);
    if (mConnections.contains(connection))
        return;           // the slot is already connected, so ignore request
    connect(mTimer, SIGNAL(timeout()), receiver, member);
    mConnections.append(connection);
    connect(receiver, &QObject::destroyed, this, &SynchTimer::slotReceiverGone);
    if (!mTimer->isActive())
    {
        connect(mTimer, &QTimer::timeout, this, &SynchTimer::slotTimer);
        start();
    }
}

/******************************************************************************
* Disconnect from the timer. The timer is stopped if no longer needed.
*/
void SynchTimer::disconnecT(QObject* receiver, const char* member)
{
    if (mTimer)
    {
        mTimer->disconnect(receiver, member);
        if (member)
        {
            int i = mConnections.indexOf(Connection(receiver, member));
            if (i >= 0)
                mConnections.removeAt(i);
        }
        else
        {
            for (int i = 0;  i < mConnections.count();  )
            {
                if (mConnections.at(i).receiver == receiver)
                    mConnections.removeAt(i);
                else
                    ++i;
            }
        }
        if (mConnections.isEmpty())
        {
            mTimer->disconnect();
            mTimer->stop();
        }
    }
}


/*=============================================================================
=  Class: MinuteTimer
=  Application-wide timer synchronized to the minute boundary.
=============================================================================*/

MinuteTimer* MinuteTimer::mInstance = nullptr;

MinuteTimer* MinuteTimer::instance()
{
    if (!mInstance)
        mInstance = new MinuteTimer;
    return mInstance;
}

/******************************************************************************
* Called when the timer triggers, or to start the timer.
* Timers can under some circumstances wander off from the correct trigger time,
* so rather than setting a 1 minute interval, calculate the correct next
* interval each time it triggers.
*/
void MinuteTimer::slotTimer()
{
    qCDebug(KALARM_LOG) << "MinuteTimer::slotTimer";
    int interval = 62 - QTime::currentTime().second();
    mTimer->start(interval * 1000);     // execute a single shot
}


/*=============================================================================
=  Class: DailyTimer
=  Application-wide timer synchronized to midnight.
=============================================================================*/

QVector<DailyTimer*> DailyTimer::mFixedTimers;

DailyTimer::DailyTimer(const QTime& timeOfDay, bool fixed)
    : mTime(timeOfDay)
    , mFixed(fixed)
{
    if (fixed)
        mFixedTimers.append(this);
}

DailyTimer::~DailyTimer()
{
    if (mFixed)
        mFixedTimers.removeAt(mFixedTimers.indexOf(this));
}

DailyTimer* DailyTimer::fixedInstance(const QTime& timeOfDay, bool create)
{
    for (DailyTimer* timer : qAsConst(mFixedTimers))
        if (timer->mTime == timeOfDay)
            return timer;
    return create ? new DailyTimer(timeOfDay, true) : nullptr;
}

/******************************************************************************
* Disconnect from the timer signal which triggers at the given fixed time of day.
* If there are no remaining connections to that timer, it is destroyed.
*/
void DailyTimer::disconnect(const QTime& timeOfDay, QObject* receiver, const char* member)
{
    DailyTimer* timer = fixedInstance(timeOfDay, false);
    if (timer)
        timer->disconnecT(receiver, member);
}

/******************************************************************************
* Disconnect from the timer. The timer is deleted if no longer used.
*/
void DailyTimer::disconnecT(QObject* receiver, const char* member)
{
    SynchTimer::disconnecT(receiver, member);
    if (!hasConnections())
        deleteLater();
}

/******************************************************************************
* Change the time at which the variable timer triggers.
*/
void DailyTimer::changeTime(const QTime& newTimeOfDay, bool triggerMissed)
{
    if (mFixed)
        return;
    if (mTimer->isActive())
    {
        mTimer->stop();
        bool triggerNow = false;
        if (triggerMissed)
        {
            QTime now = QTime::currentTime();
            if (now >= newTimeOfDay  &&  now < mTime)
            {
                // The trigger time is now earlier and it has already arrived today.
                // Trigger a timer event immediately.
                triggerNow = true;
            }
        }
        mTime = newTimeOfDay;
        if (triggerNow)
            mTimer->start(0);    // trigger immediately
        else
            start();
    }
    else
        mTime = newTimeOfDay;
}

/******************************************************************************
* Initialise the timer to trigger at the specified time.
* This will either be today or tomorrow, depending on whether the trigger time
* has already passed.
*/
void DailyTimer::start()
{
    // TIMEZONE = local time
    const QDateTime now = QDateTime::currentDateTime();
    // Find out whether to trigger today or tomorrow.
    // In preference, use the last trigger date to determine this, since
    // that will avoid possible errors due to daylight savings time changes.
    bool today;
    if (mLastDate.isValid())
        today = (mLastDate < now.date());
    else
        today = (now.time() < mTime);
    QDateTime next;
    if (today)
        next = QDateTime(now.date(), mTime);
    else
        next = QDateTime(now.date().addDays(1), mTime);
    const qint64 interval = next.toSecsSinceEpoch() - now.toSecsSinceEpoch();
    mTimer->start(interval * 1000);    // execute a single shot
    qCDebug(KALARM_LOG) << "DailyTimer::start: at" << mTime.hour() << ":" << mTime.minute() << ": interval =" << interval/3600 << ":" << (interval/60)%60 << ":" << interval%60;
}

/******************************************************************************
* Called when the timer triggers.
* Set the timer to trigger again tomorrow at the specified time.
* Note that if daylight savings time changes occur, this will not be 24 hours
* from now.
*/
void DailyTimer::slotTimer()
{
    // TIMEZONE = local time
    const QDateTime now = QDateTime::currentDateTime();
    mLastDate = now.date();
    const QDateTime next = QDateTime(mLastDate.addDays(1), mTime);
    const qint64 interval = next.toSecsSinceEpoch() - now.toSecsSinceEpoch();
    mTimer->start(interval * 1000);    // execute a single shot
    qCDebug(KALARM_LOG) << "DailyTimer::slotTimer: at" << mTime.hour() << ":" << mTime.minute() << ": interval =" << interval/3600 << ":" << (interval/60)%60 << ":" << interval%60;
}

// vim: et sw=4:
