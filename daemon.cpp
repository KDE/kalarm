/*
 *  daemon.cpp  -  controls the alarm daemon
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

#include "kalarm.h"

#include <qtimer.h>
#include <qiconset.h>

#include <kstandarddirs.h>
#include <kconfig.h>
#include <kprocess.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kaboutdata.h>
#include <kmessagebox.h>
#include <dcopclient.h>
#include <kdebug.h>

#include <kalarmd/clientinfo.h>

#include "alarmcalendar.h"
#include "daemongui.h"
#include "preferences.h"
#include "kalarmapp.h"
#include "daemon.moc"


#define      DAEMON_APP_NAME_DEF   "kalarmd"    // DCOP name of alarm daemon application
const char*  DAEMON_APP_NAME     = DAEMON_APP_NAME_DEF;
const char*  DAEMON_DCOP_OBJECT  = "ad";        // DCOP name of kalarmd's DCOP interface
const char*  DCOP_OBJECT_NAME = "display";


Daemon*        Daemon::mInstance = 0;
QTimer*        Daemon::mStartTimer = 0;
QTimer*        Daemon::mRegisterTimer = 0;
QDateTime      Daemon::mLastCheck;
QDateTime      Daemon::mNextCheck;
int            Daemon::mCheckInterval = 0;
int            Daemon::mStartTimeout = 0;
Daemon::Status Daemon::mStatus = Daemon::STOPPED;
bool           Daemon::mRegisterFailMsg = false;

// How frequently to check the daemon's status after starting it.
// This is equal to the length of time we wait after the daemon is registered with DCOP
// before we assume that it is ready to accept DCOP calls.
static const int startCheckInterval = 500;     // 500 milliseconds


/******************************************************************************
* Initialise.
* A Daemon instance needs to be constructed only in order for slots to work.
* All external access is via static methods.
*/
void Daemon::initialise()
{
	if (!mInstance)
		mInstance = new Daemon();
	connect(AlarmCalendar::activeCalendar(), SIGNAL(calendarSaved(AlarmCalendar*)), mInstance, SLOT(slotCalendarSaved(AlarmCalendar*)));
	readCheckInterval();
}

/******************************************************************************
* Start the alarm daemon if necessary, and register this application with it.
* Reply = false if the daemon definitely couldn't be started or registered with.
*/
bool Daemon::start()
{
	kdDebug(5950) << "Daemon::start()\n";
	updateRegisteredStatus();
	switch (mStatus)
	{
		case STOPPED:
		{
			if (mStartTimer)
				return true;     // we're currently waiting for the daemon to start
			// Start the alarm daemon. It is a KUniqueApplication, which means that
			// there is automatically only one instance of the alarm daemon running.
			QString execStr = locate("exe", QString::fromLatin1(DAEMON_APP_NAME));
			if (execStr.isEmpty())
			{
				KMessageBox::error(0, i18n("Alarm Daemon not found"));
				kdError() << "Daemon::startApp(): " DAEMON_APP_NAME_DEF " not found" << endl;
				return false;
			}
			KApplication::kdeinitExec(execStr);
			kdDebug(5950) << "Daemon::start(): Alarm daemon started" << endl;
			mStartTimeout = 5000/startCheckInterval + 1;    // check daemon status for 5 seconds before giving up
			mStartTimer = new QTimer(mInstance);
			connect(mStartTimer, SIGNAL(timeout()), mInstance, SLOT(checkIfStarted()));
			mStartTimer->start(startCheckInterval);
			mInstance->checkIfStarted();
			return true;
		}
		case RUNNING:
			return true;     // we're waiting for the daemon to be completely ready
		case READY:
			// Daemon is ready. Register this application with it.
			if (!registerWith(false))
				return false;
			break;
		case REGISTERED:
			break;
	}
	return true;
}

/******************************************************************************
* Register this application with the alarm daemon.
*/
bool Daemon::registerWith(bool reregister)
{
	if (mRegisterTimer)
		return true;
	if (mStatus == STOPPED  ||  mStatus == RUNNING)
		return false;
	if (mStatus == REGISTERED  &&  !reregister)
		return true;

	bool disabledIfStopped = theApp()->alarmsDisabledIfStopped();
	kdDebug(5950) << (reregister ? "Daemon::reregisterWith(): " : "Daemon::registerWith(): ") << (disabledIfStopped ? "NO_START" : "COMMAND_LINE") << endl;
	QByteArray data;
	QDataStream arg(data, IO_WriteOnly);
	arg << QCString(kapp->aboutData()->appName()) << kapp->aboutData()->programName()
	    << QCString(DCOP_OBJECT_NAME)
	    << static_cast<int>(disabledIfStopped ? ClientInfo::NO_START_NOTIFY : ClientInfo::COMMAND_LINE_NOTIFY)
	    << (Q_INT8)0;
	const char* func = reregister ? "reregisterApp(QCString,QString,QCString,int,bool)" : "registerApp(QCString,QString,QCString,int,bool)";
	if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, func, data))
	{
		registrationResult(reregister, false);
		return false;
	}
	mRegisterTimer = new QTimer(mInstance);
	connect(mRegisterTimer, SIGNAL(timeout()), mInstance, SLOT(registerTimerExpired()));
	mRegisterTimer->start(10000);     // wait up to 10 seconds for the reply

	// Register as a GUI with the alarm daemon
	theApp()->daemonGuiHandler()->registerWith();
	return true;
}

/******************************************************************************
* Called when the daemon has notified us of the result of the register() DCOP call.
*/
void Daemon::registrationResult(bool reregister, bool success)
{
	kdDebug(5950) << "Daemon::registrationResult(" << reregister << ")\n";
	delete mRegisterTimer;
	mRegisterTimer = 0;
	if (!success)
	{
		kdError(5950) << "Daemon::registrationResult(" << reregister << "): registerApp dcop call failed" << endl;
		if (!reregister)
		{
			if (mStatus == REGISTERED)
				mStatus = READY;
			if (!mRegisterFailMsg)
			{
				mRegisterFailMsg = true;
				KMessageBox::error(0, i18n("Cannot enable alarms:\nFailed to register with Alarm Daemon (%1)").arg(QString::fromLatin1(DAEMON_APP_NAME)));
			}
		}
	}
	else if (!reregister)
	{
		// Tell alarm daemon to load the calendar
		QByteArray data;
		QDataStream arg(data, IO_WriteOnly);
		arg << QCString(kapp->aboutData()->appName()) << AlarmCalendar::activeCalendar()->urlString();
		if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "addMsgCal(QCString,QString)", data))
			kdError(5950) << "Daemon::start(): addMsgCal dcop send failed" << endl;
		else
		{
			mStatus = REGISTERED;
			mRegisterFailMsg = false;
			kdDebug(5950) << "Daemon::start(): daemon startup complete" << endl;
		}
	}
}

/******************************************************************************
* Check whether the alarm daemon has started yet, and if so, register with it.
*/
void Daemon::checkIfStarted()
{
	updateRegisteredStatus();
	bool err = false;
	switch (mStatus)
	{
		case STOPPED:
			if (--mStartTimeout > 0)
				return;     // wait a bit more to check again
			// Output error message, but delete timer first to prevent
			// multiple messages.
			err = true;
			break;
		case RUNNING:
		case READY:
		case REGISTERED:
			break;
	}
	delete mStartTimer;
	mStartTimer = 0;
	if (err)
	{
		kdError(5950) << "Daemon::checkIfStarted(): failed to start daemon" << endl;
		KMessageBox::error(0, i18n("Cannot enable alarms:\nFailed to start Alarm Daemon (%1)").arg(QString::fromLatin1(DAEMON_APP_NAME)));
	}
}

/******************************************************************************
* Check whether the alarm daemon has started yet, and if so, whether it is
* ready to accept DCOP calls.
*/
void Daemon::updateRegisteredStatus(bool timeout)
{
	if (!kapp->dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
	{
		mStatus = STOPPED;
		mRegisterFailMsg = false;
	}
	else
	{
		switch (mStatus)
		{
			case STOPPED:
				// The daemon has newly been detected as registered with DCOP.
				// Wait for a short time to ensure that it is ready for DCOP calls.
				mStatus = RUNNING;
				QTimer::singleShot(startCheckInterval, mInstance, SLOT(slotStarted()));
				break;
			case RUNNING:
				if (timeout)
				{
					mStatus = READY;
					start();
				}
				break;
			case READY:
			case REGISTERED:
				break;
		}
	}
	kdDebug(5950) << "Daemon::updateRegisteredStatus() -> " << mStatus << endl;
}

/******************************************************************************
* Stop the alarm daemon if it is running.
*/
bool Daemon::stop()
{
	kdDebug(5950) << "Daemon::stop()" << endl;
	if (kapp->dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
	{
		QByteArray data;
		if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "quit()", data))
		{
			kdError(5950) << "Daemon::stop(): quit dcop send failed" << endl;
			return false;
		}
	}
	return true;
}

/******************************************************************************
* Reset the alarm daemon.
* Reply = true if daemon was told to reset
*       = false if daemon is not running.
*/
bool Daemon::reset()
{
	kdDebug(5950) << "Daemon::reset()" << endl;
	if (!kapp->dcopClient()->isApplicationRegistered(DAEMON_APP_NAME))
		return false;
	QByteArray data;
	QDataStream arg(data, IO_WriteOnly);
	arg << QCString(kapp->aboutData()->appName()) << AlarmCalendar::activeCalendar()->urlString();
	if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "resetMsgCal(QCString,QString)", data))
		kdError(5950) << "Daemon::reset(): resetMsgCal dcop send failed" << endl;
	return true;
}

/******************************************************************************
* Tell the alarm daemon to reread the calendar file.
*/
void Daemon::reload()
{
	kdDebug(5950) << "Daemon::reload()\n";
	QByteArray data;
	QDataStream arg(data, IO_WriteOnly);
	arg << QCString(kapp->aboutData()->appName()) << AlarmCalendar::activeCalendar()->urlString();
	if (!kapp->dcopClient()->send(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT, "reloadMsgCal(QCString,QString)", data))
		kdError(5950) << "Daemon::reload(): reloadMsgCal dcop send failed" << endl;
}

/******************************************************************************
* Check whether the alarm daemon is currently running and available.
*/
bool Daemon::isRunning(bool startdaemon)
{
	static bool runState = false;
	updateRegisteredStatus();
	bool newRunState = (mStatus == READY  ||  mStatus == REGISTERED);
	if (newRunState != runState)
	{
		// Daemon's status has changed
		runState = newRunState;
		if (runState  &&  startdaemon)
			start();      // re-register with the daemon
	}
	return runState  &&  (mStatus == REGISTERED);
}

/******************************************************************************
* Create a "Control/Configure Alarm Daemon" action.
*/
KAction* Daemon::createControlAction(KActionCollection* actions, const char* name)
{
	if (!mInstance)
		return 0;
#if KDE_VERSION >= 308
	return new KAction(i18n("Control the Alarm Daemon", "Control Alarm &Daemon..."),
	                   0, mInstance, SLOT(slotControl()), actions, name);
#else
	KAction* prefs = KStdAction::preferences(mInstance, "x", actions);
	KAction* action = new KAction(i18n("Configure Alarm &Daemon..."), prefs->iconSet(),
	                              0, mInstance, SLOT(slotControl()), actions, name);
	delete prefs;
	return action;
#endif
}

/******************************************************************************
* Called when a Control Alarm Daemon menu item is selected.
* Displays the alarm daemon control dialog.
*/
void Daemon::slotControl()
{
	KProcess proc;
	proc << locate("exe", QString::fromLatin1("kcmshell"));
#if KDE_VERSION >= 308
	proc << QString::fromLatin1("kcmkded");
#else
	proc << QString::fromLatin1("alarmdaemonctrl");
#endif
	proc.start(KProcess::DontCare);
}

/******************************************************************************
* Called when a calendar has been saved.
* If it's the active alarm calendar, notify the alarm daemon.
*/
void Daemon::slotCalendarSaved(AlarmCalendar* cal)
{
	if (cal == AlarmCalendar::activeCalendar())
		reload();
}

/******************************************************************************
* Read the alarm daemon's alarm check interval from its config file. If it has
* reduced, any late-cancel alarms which are already due could potentially be
* cancelled erroneously. To avoid this, note the effective last time that it
* will have checked alarms.
*/
void Daemon::readCheckInterval()
{
	KConfig config(locate("config", DAEMON_APP_NAME_DEF"rc"));
	config.setGroup("General");
	int checkInterval = 60 * config.readNumEntry("CheckInterval", 1);
	if (checkInterval < mCheckInterval)
	{
		// The daemon check interval has reduced,
		// Note the effective last time that the daemon checked alarms.
		QDateTime now = QDateTime::currentDateTime();
		mLastCheck = now.addSecs(-mCheckInterval);
		mNextCheck = now.addSecs(checkInterval);
	}
	mCheckInterval = checkInterval;
}

/******************************************************************************
* Return the maximum time (in seconds) elapsed since the last time the alarm
* daemon must have checked alarms.
*/
int Daemon::maxTimeSinceCheck()
{
	readCheckInterval();
	if (mLastCheck.isValid())
	{
		QDateTime now = QDateTime::currentDateTime();
		if (mNextCheck > now)
		{
			// Daemon's check interval has just reduced, so allow extra time
			return mLastCheck.secsTo(now);
		}
		mLastCheck = QDateTime();
	}
	return mCheckInterval;
}
