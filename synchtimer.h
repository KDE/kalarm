/*
 *  minutetimer.h  -  timer which triggers on each minute boudary
 *  Program:  kalarm
 *  (C) 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef MINUTETIMER_H
#define MINUTETIMER_H

#include <qobject.h>
#include <qvaluelist.h>
#include <qcstring.h>
#include <qdatetime.h>
class QTimer;

/** SynchTimer is a virtual base class for application-wide timers synchronised
 *  to a time boundary.
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class SynchTimer : public QObject
{
		Q_OBJECT
	public:
		virtual ~SynchTimer();

		struct Connection
		{
			Connection() { }
			Connection(QObject* r, const char* s) : receiver(r), slot(s) { }
			bool operator==(const Connection& c) const  { return receiver == c.receiver && slot == c.slot; }
			QObject*       receiver;
			const QCString slot;
		};
	protected:
		SynchTimer();
		virtual void        start() = 0;
		void                connecT(QObject* receiver, const char* member);
		void                disconnecT(QObject* receiver, const char* member = 0);

		QTimer*             mTimer;

	protected slots:
		virtual void        slotSynchronise() = 0;

	private slots:
		void                slotReceiverGone(QObject* r)  { disconnecT(r); }

	private:
		SynchTimer(const SynchTimer&);   // prohibit copying
		QValueList<Connection> mConnections;  // list of current clients
};


/** MinuteTimer is an application-wide timer synchronised to the minute boundary.
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class MinuteTimer : public SynchTimer
{
		Q_OBJECT
	public:
		virtual ~MinuteTimer()  { mInstance = 0; }
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
		static void disconnect(QObject* receiver, const char* member = 0)
		                   { if (mInstance) mInstance->disconnecT(receiver, member); }

	protected:
		MinuteTimer() : SynchTimer(), mSynching(false) { }
		static MinuteTimer* instance();
		virtual void        start();

	protected slots:
		virtual void slotSynchronise();

	private:
		static MinuteTimer* mInstance;     // the one and only instance
		bool                mSynching;     // timer hasn't yet synchronised to a boundary
};


/** DailyTimer is an application-wide timer synchronised to midnight.
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class DailyTimer : public SynchTimer
{
		Q_OBJECT
	public:
		virtual ~DailyTimer()  { mInstance = 0; }
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
		static void disconnect(QObject* receiver, const char* member = 0)
		                   { if (mInstance) mInstance->disconnecT(receiver, member); }

	protected:
		DailyTimer() : SynchTimer() { }
		static DailyTimer*  instance();
		virtual void        start();

	protected slots:
		virtual void slotSynchronise()   { start(); }

	private:
		static DailyTimer*  mInstance;     // the one and only instance
};


/** StartOfDayTimer is an application-wide timer synchronised to the user-defined
 *  start-of-day time (set in KAlarm's Preferences dialog).
 *  It automatically adjusts to any changes in the start-of-day time.
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class StartOfDayTimer : public SynchTimer
{
		Q_OBJECT
	public:
		virtual ~StartOfDayTimer()  { mInstance = 0; }
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
		static void disconnect(QObject* receiver, const char* member = 0)
		                   { if (mInstance) mInstance->disconnecT(receiver, member); }

	protected:
		StartOfDayTimer();
		static StartOfDayTimer*  instance();
		virtual void        start();

	protected slots:
		virtual void slotSynchronise()   { start(); }
		void         startOfDayChanged(const QTime& oldTime);

	private:
		static StartOfDayTimer*  mInstance;     // the one and only instance
		QTime               mStartOfDay;   // the current start-of-day time
};

#endif // MINUTETIMER_H

