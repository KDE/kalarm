/*
 *  minutetimer.cpp  -  timer which triggers on each minute boudary
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
 *
 *  In addition, as a special exception, the copyright holders give permission 
 *  to link the code of this program with any edition of the Qt library by 
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same 
 *  license as Qt), and distribute linked combinations including the two.  
 *  You must obey the GNU General Public License in all respects for all of 
 *  the code used other than Qt.  If you modify this file, you may extend 
 *  this exception to your version of the file, but you are not obligated to 
 *  do so. If you do not wish to do so, delete this exception statement from 
 *  your version.
 */

#include "kalarm.h"
#include <qtimer.h>
#include <kdebug.h>
#include "preferences.h"
#include "synchtimer.moc"


/*=============================================================================
=  Class: SynchTimer
*  Provides an application-wide timer synchronised to a time boundary.
=============================================================================*/

SynchTimer::SynchTimer()
{
	mTimer = new QTimer(this);
}

SynchTimer::~SynchTimer()
{
	delete mTimer;
	mTimer = 0;
}

/******************************************************************************
* Connect to the timer. The timer is started if necessary.
*/
void SynchTimer::connecT(QObject* receiver, const char* member)
{
	connect(mTimer, SIGNAL(timeout()), receiver, member);
	mConnections.append(Connection(receiver, member));
	if (!mTimer->isActive())
	{
		connect(mTimer, SIGNAL(timeout()), this, SLOT(slotSynchronise()));
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
			mConnections.remove(Connection(receiver, member));
		else
		{
			for (QValueList<Connection>::Iterator it = mConnections.begin();  it != mConnections.end();  )
			{
				if ((*it).receiver == receiver)
					it = mConnections.remove(it);
				else
					++it;
			}
		}
		if (mConnections.empty())
		{
			mTimer->disconnect();
			mTimer->stop();
		}
	}
}


/*=============================================================================
=  Class: MinuteTimer
*  Provides an application-wide timer synchronised to the minute boundary.
=============================================================================*/

MinuteTimer* MinuteTimer::mInstance = 0;

MinuteTimer::~MinuteTimer()
{
	mInstance = 0;
}

MinuteTimer* MinuteTimer::instance()
{
	if (!mInstance)
		mInstance = new MinuteTimer;
	return mInstance;
}

/******************************************************************************
* Start the timer.
*/
void MinuteTimer::start()
{
	int interval = 62 - QTime::currentTime().second();
	mTimer->start(1000 * interval);
	mSynching = (interval != 60);
	kdDebug(5950) << "MinuteTimer::start()\n";
}

/******************************************************************************
* Called when the timer triggers.
* If synchronised to the minute boundary, set the timer interval to 1 minute.
* If not yet synchronised, try again to synchronise.
*/
void MinuteTimer::slotSynchronise()
{
	kdDebug(5950) << "MinuteTimer::slotSynchronise()" << endl;
	int firstInterval = 62 - QTime::currentTime().second();
	mTimer->changeInterval(firstInterval * 1000);
	mSynching = (firstInterval != 60);
	if (!mSynching)
		mTimer->disconnect(this, SLOT(slotSynchronise()));
}


/*=============================================================================
=  Class: DailyTimer
*  Provides an application-wide timer synchronised to the user-defined
*  start-of-day time. It automatically adjusts to any changes in the
*  start-of-day time.
=============================================================================*/

DailyTimer* DailyTimer::mInstance = 0;

DailyTimer::DailyTimer()
{
	QObject::connect(Preferences::instance(), SIGNAL(startOfDayChanged(const QTime&)), SLOT(startOfDayChanged(const QTime&)));
}

DailyTimer::~DailyTimer()
{
	mInstance = 0;
}

DailyTimer* DailyTimer::instance()
{
	if (!mInstance)
		mInstance = new DailyTimer;
	return mInstance;
}

/******************************************************************************
* Start the timer to expire at the start of the next day (using the user-
* defined start-of-day time).
*/
void DailyTimer::start()
{
	mStartOfDay = Preferences::instance()->startOfDay();
	int interval = QTime::currentTime().secsTo(mStartOfDay);
	if (interval < 0)
		interval += 86400;
	mTimer->start(interval * 1000, true);
	kdDebug(5950) << "DailyTimer::start()\n";
}

/******************************************************************************
* Notify this instance of a change in the start-of-day time.
* The purge timer is adjusted and if necessary alarms are purged.
*/
void DailyTimer::startOfDayChanged(const QTime&)
{
	QTime sod = Preferences::instance()->startOfDay();
	QTime now = QTime::currentTime();
	if (sod <= now  &&  now < mStartOfDay)
	{
		// The start-of-day time is now earlier and it has arrived already.
		// Trigger a timer event immediately.
		mTimer->start(0, true);
	}
	else
		start();
}
