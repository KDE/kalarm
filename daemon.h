/*
 *  daemon.h  -  controls the alarm daemon
 *  Program:  kalarm
 *  (C) 2001 - 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef DAEMON_H
#define DAEMON_H

#include <qobject.h>
#include <qdatetime.h>

class KAction;
class KActionCollection;
class AlarmCalendar;


extern const char* DAEMON_APP_NAME;
extern const char* DAEMON_DCOP_OBJECT;
extern const char* DCOP_OBJECT_NAME;

class Daemon : public QObject
{
		Q_OBJECT
	public:
		static void      initialise();
		static KAction*  createControlAction(KActionCollection*, const char* name);
		static bool      start();
		static bool      reregister()      { return registerWith(true); }
		static bool      reset();
		static bool      stop();
		static bool      isRunning(bool startDaemon = true);
		static int       maxTimeSinceCheck();
		static void      readCheckInterval();
		static bool      isRegistered()    { return mRegistered; }

	private slots:
		void             slotControl();
		void             slotCalendarSaved(AlarmCalendar*);
		void             checkIfStarted();

	private:
		explicit Daemon() { }
		static bool      registerWith(bool reregister);
		static void      reload();

		static Daemon*   mInstance;         // only one instance allowed
		static QTimer*   mStartTimer;       // timer to check daemon status after starting daemon
		static QDateTime mLastCheck;        // last time daemon checked alarms before check interval change
		static QDateTime mNextCheck;        // next time daemon will check alarms after check interval change
		static int       mCheckInterval;    // daemon check interval (seconds)
		static int       mStartTimeout;     // remaining number of times to check if alarm daemon has started
		static bool      mRegistered;       // true if we've registered with alarm daemon
};

#endif // DAEMON_H
