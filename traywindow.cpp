/*
 *  traywindow.cpp  -  the KDE system tray applet
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "kalarm.h"
#include <stdlib.h>

#include <qtooltip.h>
#include <qfile.h>
#include <qlabel.h>

#include <kapp.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kaboutdata.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <dcopclient.h>
#include <kdebug.h>

#include <kalarmd/alarmdaemoniface_stub.h>
#include "kalarmapp.h"
#include "alarmcalendar.h"
#include "prefdlg.h"
#include "prefsettings.h"
#include "traywindow.h"
#include "traywindow.moc"



/*=============================================================================
= Class: TrayWindow
= The KDE system tray window.
=============================================================================*/

TrayWindow::TrayWindow(const char* name)
	: KSystemTray(0, name),
	  mDaemonStatusTimer(this),
	  mDaemonStatusTimerCount(0),
	  mQuitReplaced(false),
	  mCalendarDisabled(false),
	  mEnableCalPending(false)
{
	kdDebug(5950) << "TrayWindow::TrayWindow()\n";
	// Set up GUI icons
//	KGlobal::iconLoader()->addAppDir(kapp->aboutData()->appName());
//	mPixmapEnabled  = KGlobal::iconLoader()->loadIcon("kalarm", KIcon::Panel);
//	mPixmapDisabled = KGlobal::iconLoader()->loadIcon("kalarm_disabled", KIcon::Panel);
	mPixmapEnabled  = BarIcon("kalarm");
	mPixmapDisabled = BarIcon("kalarm_disabled");
	if (mPixmapEnabled.isNull() || mPixmapDisabled.isNull())
		KMessageBox::sorry(this, i18n("Can't load system tray icon!"),
		                         i18n("%1 Error").arg(kapp->aboutData()->programName()));

	mActionQuit = KStdAction::quit(this, SLOT(slotQuit()));

	// Set up the context menu
	KAction* a = theApp()->actionAlarmEnable();
	mAlarmsEnabledId = a->itemId(a->plug(contextMenu()));
	connect(a, SIGNAL(activated()), this, SLOT(toggleAlarmsEnabled()));
	theApp()->actionPreferences()->plug(contextMenu());
	theApp()->actionDaemonPreferences()->plug(contextMenu());

	// Set icon to correspond with the alarms enabled menu status
	setEnabledStatus(true);

	setDaemonStatus(isDaemonRunning(false));

	mDaemonStatusTimerInterval = theApp()->settings()->daemonTrayCheckInterval();
	connect(theApp()->settings(), SIGNAL(settingsChanged()), this, SLOT(slotSettingsChanged()));
	connect(&mDaemonStatusTimer, SIGNAL(timeout()), SLOT(checkDaemonRunning()));
	mDaemonStatusTimer.start(mDaemonStatusTimerInterval * 1000);  // check regularly if daemon is running

	registerWithDaemon();

	QToolTip::add(this, kapp->aboutData()->programName());
}

TrayWindow::~TrayWindow()
{
	kdDebug(5950) << "TrayWindow::~TrayWindow()\n";
	theApp()->deleteWindow(this);
}

/******************************************************************************
* Called just before the context menu is displayed.
* Modify the Quit context menu item to only close the system tray widget.
*/
void TrayWindow::contextMenuAboutToShow(KPopupMenu* menu)
{
	if (!mQuitReplaced)
	{
		// Prevent Quit from quitting the program
		QString quitText = KStdAction::quit()->text();
		for (unsigned n = 0;  n < menu->count();  ++n)
		{
			QString txt = menu->text(menu->idAt(n));
			if (txt == quitText)
			{
				menu->removeItemAt(n);
				break;
			}
		}
		mActionQuit->plug(menu);
		mQuitReplaced = true;
	}

	// Update the Alarms Enabled item status
	setDaemonStatus(isDaemonRunning(false));
}

/******************************************************************************
* Update the context menu to display the alarm monitoring status.
*/
void TrayWindow::updateCalendarStatus(bool monitoring)
{
	mCalendarDisabled = !monitoring;
	if (!isDaemonRunning(false))
		monitoring = false;
	setEnabledStatus(monitoring);
}

/******************************************************************************
* Tell the alarm daemon to enable/disable monitoring of the calendar file.
*/
void TrayWindow::enableCalendar(bool enable)
{
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.enableCal(theApp()->getCalendar().urlString(), enable);
	mEnableCalPending = false;
}

/******************************************************************************
* Called when the Alarms Enabled context menu item is selected.
* The alarm daemon is told to stop or start monitoring the calendar
* file as appropriate.
*/
void TrayWindow::toggleAlarmsEnabled()
{
	bool newstate = !contextMenu()->isItemChecked(mAlarmsEnabledId);
	if (newstate  &&  !isDaemonRunning())
	{
		// The daemon is not running, so start it
		QString execStr = locate("exe", DAEMON_APP_NAME);
		if (execStr.isEmpty())
		{
			KMessageBox::error(this, i18n("Alarm Daemon not found"),
			                   i18n("%1 Error").arg(kapp->aboutData()->programName()));
			kdError() << "TrayWindow::toggleAlarmsEnabled(): kalarmd not found" << endl;
			return;
		}
		system(QFile::encodeName(execStr));
		mEnableCalPending = true;
		setFastDaemonCheck();
	}
	if (isDaemonRunning())
		enableCalendar(newstate);
}

/******************************************************************************
* Called when the Quit context menu item is selected.
* Closes the system tray window, but does not exit the program if other windows
* are still open.
*/
void TrayWindow::slotQuit()
{
	kdDebug(5950)<<"TrayWindow::slotQuit()\n";
	delete this;
}

/******************************************************************************
* Called by the timer after the Alarms Enabled context menu item is selected,
* to update the GUI status once the daemon has responded to the command.
* 'newstatus' is true of the daemon is running.
*/
void TrayWindow::setDaemonStatus(bool newstatus)
{
	bool oldstatus = contextMenu()->isItemChecked(mAlarmsEnabledId);
	kdDebug(5950) << "TrayWindow::setDaemonStatus(): "<<(int)oldstatus<<"->"<<(int)newstatus<<endl;
	if (mCalendarDisabled)
		newstatus = false;
	if (newstatus != oldstatus)
		setEnabledStatus(newstatus);
}

/******************************************************************************
* Update the alarms enabled menu item, and the icon pixmap.
*/
void TrayWindow::setEnabledStatus(bool status)
{
	setPixmap(status ? mPixmapEnabled : mPixmapDisabled);
	contextMenu()->setItemChecked(mAlarmsEnabledId, status);
	theApp()->setActionAlarmEnable(status);
}

/******************************************************************************
*  Called when the mouse is clicked over the panel icon.
*  A left click displays the KAlarm main window.
*/
void TrayWindow::mousePressEvent(QMouseEvent* e)
{
	if (e->button() == LeftButton)
		theApp()->slotKAlarm();      // left click: display the main window
	else
		KSystemTray::mousePressEvent(e);
}

/******************************************************************************
* Register as a GUI with the alarm daemon.
*/
void TrayWindow::registerWithDaemon()
{
	kdDebug(5950) << "TrayWindow::registerWithDaemon()\n";
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.registerGui(kapp->aboutData()->appName(), TRAY_DCOP_OBJECT_NAME);
}

/******************************************************************************
* Check whether the alarm daemon is currently running.
*/
bool TrayWindow::isDaemonRunning(bool updateTrayIcon)
{
	bool newstatus = kapp->dcopClient()->isApplicationRegistered(static_cast<const char*>(DAEMON_APP_NAME));
	if (!updateTrayIcon)
		return newstatus;
	if (newstatus != mDaemonRunning)
	{
		mDaemonRunning = newstatus;
		setDaemonStatus(newstatus);
		mDaemonStatusTimer.changeInterval(mDaemonStatusTimerInterval * 1000);   // exit from fast checking
		mDaemonStatusTimerCount = 0;
		if (newstatus)
		{
			registerWithDaemon();       // the alarm daemon has started up, so register with it
			if (mEnableCalPending)
				enableCalendar(true);   // and tell it to monitor the calendar, if appropriate
		}
	  }
	  return mDaemonRunning;
}

/******************************************************************************
* Called by the timer to check whether the daemon is running.
*/
void TrayWindow::checkDaemonRunning()
{
	isDaemonRunning();
	if (mDaemonStatusTimerCount > 0  &&  --mDaemonStatusTimerCount <= 0)   // limit how long we check at fast rate
		mDaemonStatusTimer.changeInterval(mDaemonStatusTimerInterval * 1000);
}

/******************************************************************************
* Starts checking at a faster rate whether the daemon is running.
*/
void TrayWindow::setFastDaemonCheck()
{
	mDaemonStatusTimer.start(500);     // check new status every half second
	mDaemonStatusTimerCount = 20;      // don't check at this rate for more than 10 seconds
}

/******************************************************************************
* Called when a program setting has changed.
* If the system tray icon update interval has changed, reset the timer.
*/
void TrayWindow::slotSettingsChanged()
{
	int newInterval = theApp()->settings()->daemonTrayCheckInterval();
	if (newInterval != mDaemonStatusTimerInterval)
	{
		// Daemon check interval has changed
		mDaemonStatusTimerInterval = newInterval;
		if (mDaemonStatusTimerCount <= 0)   // don't change if on fast rate
			mDaemonStatusTimer.changeInterval(mDaemonStatusTimerInterval * 1000);
	}
}
