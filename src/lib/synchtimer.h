/*
 *  synchtimer.h  -  timers which synchronize to time boundaries
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SYNCHTIMER_H
#define SYNCHTIMER_H

/* @file synchtimer.h - timers which synchronize to time boundaries */

#include <QObject>
#include <QVector>
#include <QByteArray>
#include <QTime>
#include <QDate>
class QTimer;

/** SynchTimer is a virtual base class for application-wide timers synchronized
 *  to a time boundary.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class SynchTimer : public QObject
{
    Q_OBJECT
public:
    ~SynchTimer() override;
    // Prevent copying.
    SynchTimer(const SynchTimer&) = delete;
    SynchTimer& operator=(const SynchTimer&) = delete;

    struct Connection
    {
        Connection() { }
        Connection(QObject* r, const char* s) : receiver(r), slot(s) { }
        bool operator==(const Connection& c) const  { return receiver == c.receiver && slot == c.slot; }
        QObject*   receiver;
        QByteArray slot;
    };
protected:
    SynchTimer();
    virtual void        start() = 0;
    void                connecT(QObject* receiver, const char* member);
    virtual void        disconnecT(QObject* receiver, const char* member = nullptr);
    bool                hasConnections() const   { return !mConnections.isEmpty(); }

    QTimer*             mTimer;

protected Q_SLOTS:
    virtual void        slotTimer() = 0;

private Q_SLOTS:
    void                slotReceiverGone(QObject* r)  { disconnecT(r); }

private:
    QVector<Connection> mConnections;  // list of current clients
};


/** MinuteTimer is an application-wide timer synchronized to the minute boundary.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class MinuteTimer : public SynchTimer
{
    Q_OBJECT
public:
    ~MinuteTimer() override  { mInstance = nullptr; }

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
    MinuteTimer() : SynchTimer() { }
    static MinuteTimer* instance();
    void        start() override    { slotTimer(); }

protected Q_SLOTS:
    void        slotTimer() override;

private:
    static MinuteTimer* mInstance;     // the one and only instance
};


/** DailyTimer is an application-wide timer synchronized to a specified time of day, local time.
 *
 *  Daily timers come in two flavors: fixed, which can only be accessed through
 *  static methods, and variable, whose time can be adjusted and which are
 *  accessed through non-static methods.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class DailyTimer : public SynchTimer
{
    Q_OBJECT
public:
    ~DailyTimer() override;

    /** Connect to the timer signal which triggers at the given fixed time of day.
     *  A new timer is created if necessary.
     *  @param timeOfDay Time at which the timer is to trigger.
     *  @param receiver Receiving object.
     *  @param member Slot to activate.
     */
    static void connect(const QTime& timeOfDay, QObject* receiver, const char* member)
                       { fixedInstance(timeOfDay)->connecT(receiver, member); }

    /** Disconnect from the timer signal which triggers at the given fixed time of day.
     *  If there are no remaining connections to that timer, it is destroyed.
     *  @param timeOfDay Time at which the timer triggers.
     *  @param receiver Receiving object.
     *  @param member Slot to disconnect. If null, all slots belonging to
     *                @p receiver will be disconnected.
     */
    static void disconnect(const QTime& timeOfDay, QObject* receiver, const char* member = nullptr);

protected:
    /** Construct an instance.
     *  The constructor is protected to ensure that for variable timers, only
     *  derived classes can construct instances. This ensures that multiple
     *  timers are not created for the same use.
     */
    DailyTimer(const QTime&, bool fixed);

    void disconnecT(QObject* receiver, const char* member = nullptr) override;

    /** Change the time at which this variable timer triggers.
     *  @param newTimeOfDay New time at which the timer should trigger.
     *  @param triggerMissed If true, and if @p newTimeOfDay < @p oldTimeOfDay, and if the current
     *                       time is between @p newTimeOfDay and @p oldTimeOfDay, the timer will be
     *                       triggered immediately so as to avoid missing today's trigger.
     */
    void changeTime(const QTime& newTimeOfDay, bool triggerMissed = true);

    /** Return the current time of day at which this variable timer triggers. */
    QTime timeOfDay() const  { return mTime; }

    /** Return the instance which triggers at the specified fixed time of day,
     *  optionally creating a new instance if necessary.
     *  @param timeOfDay Time at which the timer triggers.
     *  @param create    If true, create a new instance if none already exists
     *                   for @p timeOfDay.
     *  @return The instance for @p timeOfDay, or 0 if it does not exist.
     */
    static DailyTimer* fixedInstance(const QTime& timeOfDay, bool create = true);

    void start() override;

protected Q_SLOTS:
    void slotTimer() override;

private:
    static QVector<DailyTimer*> mFixedTimers;  // list of timers whose trigger time is fixed
    QTime  mTime;
    QDate  mLastDate;  // the date on which the timer was last triggered
    bool   mFixed;     // the time at which the timer triggers cannot be changed
};


/** MidnightTimer is an application-wide timer synchronized to midnight, local time.
 *
 *  @author David Jarvie <djarvie@kde.org>
 */
class MidnightTimer
{
public:
    /** Connect to the timer signal.
     *  @param receiver Receiving object.
     *  @param member Slot to activate.
     */
    static void connect(QObject* receiver, const char* member)
                       { DailyTimer::connect(QTime(0,0), receiver, member); }

    /** Disconnect from the timer signal.
     *  @param receiver Receiving object.
     *  @param member Slot to disconnect. If null, all slots belonging to
     *                @p receiver will be disconnected.
     */
    static void disconnect(QObject* receiver, const char* member = nullptr)
                       { DailyTimer::disconnect(QTime(0,0), receiver, member); }
};

#endif // SYNCHTIMER_H

// vim: et sw=4:
