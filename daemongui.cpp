/*
 *  daemongui.cpp  -  handler for the alarm daemon GUI interface
 *  Program:  kalarm
 *  (C) 2002 - 2004 by David Jarvie <software@astrojar.org.uk>
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

#include <stdlib.h>

#include <kaboutdata.h>
#include <klocale.h>
#include <kurl.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <dcopclient.h>
#include <kdebug.h>

#include <kalarmd/alarmdaemoniface_stub.h>
#include "kalarmapp.h"
#include "preferences.h"
#include "alarmcalendar.h"
#include "daemon.h"
#include "daemongui.h"
#include "daemongui.moc"


const char* GUI_DCOP_OBJECT_NAME = "tray";


DaemonGuiHandler::DaemonGuiHandler()
	: DCOPObject(GUI_DCOP_OBJECT_NAME), 
	  QObject(),
	  mDaemonStatusTimer(this),
	  mDaemonStatusTimerCount(0),
	  mCalendarDisabled(false),
	  mEnableCalPending(false)
{
	kdDebug(5950) << "DaemonGuiHandler::DaemonGuiHandler()\n";
	// Check if the alarm daemon is running, but don't start it yet, since
	// the program is still initialising.
	mDaemonRunning = Daemon::isRunning(false);

	mDaemonStatusTimerInterval = Preferences::instance()->daemonTrayCheckInterval();
	connect(Preferences::instance(), SIGNAL(preferencesChanged()), this, SLOT(slotPreferencesChanged()));
	connect(&mDaemonStatusTimer, SIGNAL(timeout()), SLOT(timerCheckDaemonRunning()));
	mDaemonStatusTimer.start(mDaemonStatusTimerInterval * 1000);  // check regularly if daemon is running
}

/******************************************************************************
 * DCOP call from the alarm daemon to notify a change.
 * The daemon notifies calendar statuses when we first register as a GUI, and whenever
 * a calendar status changes. So we don't need to read its config files.
 */
void DaemonGuiHandler::alarmDaemonUpdate(int alarmGuiChangeType,
                                        const QString& calendarURL, const QCString& /*appName*/)
{
	kdDebug(5950) << "DaemonGuiHandler::alarmDaemonUpdate(" << alarmGuiChangeType << ")\n";
	AlarmGuiChangeType changeType = AlarmGuiChangeType(alarmGuiChangeType);
	switch (changeType)
	{
		case CHANGE_STATUS:    // daemon status change
			Daemon::readCheckInterval();
			return;
		case CHANGE_CLIENT:    // change to daemon's client application list
			return;
		default:
		{
			// It must be a calendar-related change
			if (expandURL(calendarURL) != AlarmCalendar::activeCalendar()->urlString())
				return;     // it's not a notification about KAlarm's calendar
			switch (changeType)
			{
				case DELETE_CALENDAR:
					kdDebug(5950) << "DaemonGuiHandler::alarmDaemonUpdate(DELETE_CALENDAR)\n";
					mCalendarDisabled = true;
					break;
				case CALENDAR_UNAVAILABLE:
					// Calendar is not available for monitoring
					kdDebug(5950) << "DaemonGuiHandler::alarmDaemonUpdate(CALENDAR_UNAVAILABLE)\n";
					mCalendarDisabled = true;
					break;
				case DISABLE_CALENDAR:
					// Calendar is available for monitoring but is not currently being monitored
					kdDebug(5950) << "DaemonGuiHandler::alarmDaemonUpdate(DISABLE_CALENDAR)\n";
					mCalendarDisabled = true;
					break;
				case ENABLE_CALENDAR:
					// Calendar is currently being monitored
					kdDebug(5950) << "DaemonGuiHandler::alarmDaemonUpdate(ENABLE_CALENDAR)\n";
					mCalendarDisabled = false;
					break;
				case ADD_CALENDAR:        // add a KOrganizer-type calendar
				case ADD_MSG_CALENDAR:    // add a KAlarm-type calendar
				default:
					return;
			}
			emit daemonRunning(!mCalendarDisabled);
			break;
		}
	}
}

// DCOP functions which KAlarm either doesn't use or implements in
// the DcopHandler class.
void DaemonGuiHandler::handleEvent(const QString&, const QString&)
{ }
void DaemonGuiHandler::handleEvent(const QString&)
{ }
void DaemonGuiHandler::registered(bool, bool)
{ }

/******************************************************************************
* Register as a GUI with the alarm daemon.
*/
void DaemonGuiHandler::registerWith()
{
	kdDebug(5950) << "DaemonGuiHandler::registerWith()\n";
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.registerGui(kapp->aboutData()->appName(), GUI_DCOP_OBJECT_NAME);
}

/******************************************************************************
* Return whether the alarm daemon is monitoring alarms.
*/
bool DaemonGuiHandler::monitoringAlarms()
{
	bool ok = !mCalendarDisabled  &&  Daemon::isRunning();
	emit daemonRunning(ok);
	return ok;
}

/******************************************************************************
* Tell the alarm daemon to stop or start monitoring the calendar file as
* appropriate.
*/
void DaemonGuiHandler::setAlarmsEnabled(bool enable)
{
	kdDebug(5950) << "DaemonGuiHandler::setAlarmsEnabled(" << enable << ")\n";
	if (enable  &&  !checkIfDaemonRunning())
	{
		// The daemon is not running, so start it
		if (!Daemon::start())
		{
			emit daemonRunning(false);
			return;
		}
		mEnableCalPending = true;
		setFastDaemonCheck();
	}

	// If the daemon is now running, tell it to enable/disable the calendar
	if (checkIfDaemonRunning())
		daemonEnableCalendar(enable);
}

/******************************************************************************
* Tell the alarm daemon to enable/disable monitoring of the calendar file.
*/
void DaemonGuiHandler::daemonEnableCalendar(bool enable)
{
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.enableCal(AlarmCalendar::activeCalendar()->urlString(), enable);
	mEnableCalPending = false;
}

/******************************************************************************
* Called by the timer to check whether the daemon is running.
*/
void DaemonGuiHandler::timerCheckDaemonRunning()
{
	checkIfDaemonRunning();
	// Limit how long we check at the fast rate
	if (mDaemonStatusTimerCount > 0  &&  --mDaemonStatusTimerCount <= 0)
		mDaemonStatusTimer.changeInterval(mDaemonStatusTimerInterval * 1000);
}

/******************************************************************************
* Check whether the alarm daemon is currently running.
* If its status has changed, trigger GUI updates.
*/
bool DaemonGuiHandler::checkIfDaemonRunning()
{
	bool newstatus = Daemon::isRunning();
	if (newstatus != mDaemonRunning)
	{
		mDaemonRunning = newstatus;
		int status = mDaemonRunning  &&  !mCalendarDisabled;
		emit daemonRunning(status);
		mDaemonStatusTimer.changeInterval(mDaemonStatusTimerInterval * 1000);   // exit from fast checking
		mDaemonStatusTimerCount = 0;
		if (mDaemonRunning)
		{
			// The alarm daemon has started up
			if (mEnableCalPending)
				daemonEnableCalendar(true);  // tell it to monitor the calendar, if appropriate
		}
	}
	return mDaemonRunning;
}

/******************************************************************************
* Starts checking at a faster rate whether the daemon is running.
*/
void DaemonGuiHandler::setFastDaemonCheck()
{
	mDaemonStatusTimer.start(500);     // check new status every half second
	mDaemonStatusTimerCount = 20;      // don't check at this rate for more than 10 seconds
}

/******************************************************************************
* Called when a program setting has changed.
* If the system tray icon update interval has changed, reset the timer.
*/
void DaemonGuiHandler::slotPreferencesChanged()
{
	int newInterval = Preferences::instance()->daemonTrayCheckInterval();
	if (newInterval != mDaemonStatusTimerInterval)
	{
		// Daemon check interval has changed
		mDaemonStatusTimerInterval = newInterval;
		if (mDaemonStatusTimerCount <= 0)   // don't change if on fast rate
			mDaemonStatusTimer.changeInterval(mDaemonStatusTimerInterval * 1000);
	}
}

/******************************************************************************
* Create an "Alarms Enabled/Enable Alarms" action.
*/
AlarmEnableAction* DaemonGuiHandler::createAlarmEnableAction(KActionCollection* actions, const char* name)
{
	AlarmEnableAction* a = new AlarmEnableAction(Qt::CTRL+Qt::Key_A, actions, name);
	connect(a, SIGNAL(userClicked(bool)), SLOT(setAlarmsEnabled(bool)));
	connect(this, SIGNAL(daemonRunning(bool)), a, SLOT(setCheckedActual(bool)));
	return a;
}

/******************************************************************************
 * Expand a DCOP call parameter URL to a full URL.
 * (We must store full URLs in the calendar data since otherwise later calls to
 *  reload or remove calendars won't necessarily find a match.)
 */
QString DaemonGuiHandler::expandURL(const QString& urlString)
{
	if (urlString.isEmpty())
		return QString();
	return KURL(urlString).url();
}


/*=============================================================================
=  Class: AlarmEnableAction
=============================================================================*/

AlarmEnableAction::AlarmEnableAction(int accel, QObject* parent, const char* name)
	: KToggleAction(QString::null, accel, parent, name),
	  mInitialised(false)
{
	setCheckedActual(false);    // set the correct text
	mInitialised = true;
}

/******************************************************************************
*  Set the checked status and the correct text for the Alarms Enabled action.
*/
void AlarmEnableAction::setCheckedActual(bool running)
{
	kdDebug(5950) << "AlarmEnableAction::setCheckedActual(" << running << ")\n";
	if (running != isChecked()  ||  !mInitialised)
	{
		setText(running ? i18n("&Alarms Enabled") : i18n("Enable &Alarms"));
		KToggleAction::setChecked(running);
		emit switched(running);
	}
}

/******************************************************************************
*  Request a change in the checked status.
*  The status is only actually changed when the alarm daemon run state changes.
*/
void AlarmEnableAction::setChecked(bool check)
{
	kdDebug(5950) << "AlarmEnableAction::setChecked(" << check << ")\n";
	if (check != isChecked())
	{
		if (check)
			Daemon::allowRegisterFailMsg();
		emit userClicked(check);
	}
}
