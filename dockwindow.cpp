/*
 *  dockwindow.cpp  -  the KDE system tray applet
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
#include <kprocess.h>
#include <kaboutdata.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <dcopclient.h>
#include <kdebug.h>

#include <kalarmd/alarmdaemoniface_stub.h>
#include "kalarmapp.h"
#include "prefdlg.h"
#include "prefsettings.h"
#include "dockwindow.h"
#include "dockwindow.moc"


/*=============================================================================
= Class: TrayMainWindow
= This class exists only to ensure that when other main windows are closed, the
= application does not terminate and close the system tray widget.
= (Note that when the last KMainWindow closes, the application terminates.)
=============================================================================*/

TrayMainWindow::TrayMainWindow()
	: KMainWindow(0L, 0L, WGroupLeader)
{
	kdDebug(5950) << "TrayMainWindow::TrayMainWindow()\n";
	theApp()->addWindow(this);
	QLabel* label = new QLabel(QString(""), this);
	setCentralWidget(label);
	mDockWindow = new DockWindow(this);
	mDockWindow->show();
}

TrayMainWindow::~TrayMainWindow()
{
	kdDebug(5950) << "TrayMainWindow::~TrayMainWindow()\n";
	delete mDockWindow;
	theApp()->deleteWindow(this);
}



/*=============================================================================
= Class: DockWindow
= The KDE system tray window.
=============================================================================*/

DockWindow::DockWindow(TrayMainWindow* parent, const char* name)
	: KSystemTray(0, name),
	  mTrayWindow(parent),
	  mDaemonStatusTimer(this),
	  mDaemonStatusTimerCount(0),
	  mQuitReplaced(false),
	  mEnableCalPending(false)
{
	kdDebug(5950) << "DockWindow::DockWindow()\n";
	// Set up GUI icons
//	KGlobal::iconLoader()->addAppDir(kapp->aboutData()->appName());
//	mPixmapEnabled  = KGlobal::iconLoader()->loadIcon("kalarm", KIcon::Panel);
//	mPixmapDisabled = KGlobal::iconLoader()->loadIcon("kalarm_disabled", KIcon::Panel);
	mPixmapEnabled  = BarIcon("kalarm");
	mPixmapDisabled = BarIcon("kalarm_disabled");
	if (mPixmapEnabled.isNull() || mPixmapDisabled.isNull())
		KMessageBox::sorry(this, i18n("Can't load system tray icon!"),
		                         i18n("%1 Error").arg(kapp->aboutData()->programName()));

	KAction* preferences = KStdAction::preferences(this, SLOT(slotConfigKAlarm()));
	KAction* daemonPreferences = new KAction(i18n("Configure Alarm &Daemon..."), preferences->iconSet(),
	                                         0, this, SLOT(slotConfigDaemon()));
	mActionQuit    = KStdAction::quit(this, SLOT(slotQuit()));

	// Set up the context menu
	mAlarmsEnabledId = contextMenu()->insertItem(i18n("Alarms Enabled"),
	                                             this, SLOT(toggleAlarmsEnabled()));
	preferences->plug(contextMenu());
	daemonPreferences->plug(contextMenu());

	// Set icon to correspond with the alarms enabled menu status
	setPixmap(mPixmapEnabled);
	contextMenu()->setItemChecked(mAlarmsEnabledId, true);

	setDaemonStatus(isDaemonRunning(false));

	mDaemonStatusTimerInterval = theApp()->settings()->daemonTrayCheckInterval();
	connect(theApp()->settings(), SIGNAL(settingsChanged()), this, SLOT(slotSettingsChanged()));
	connect(&mDaemonStatusTimer, SIGNAL(timeout()), SLOT(checkDaemonRunning()));
	mDaemonStatusTimer.start(mDaemonStatusTimerInterval * 1000);  // check regularly if daemon is running

	registerWithDaemon();

	QToolTip::add(this, kapp->aboutData()->programName());
}

/******************************************************************************
* Called just before the context menu is displayed.
* Modify the Quit context menu item to only close the system tray widget.
*/
void DockWindow::contextMenuAboutToShow(KPopupMenu* menu)
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
void DockWindow::updateCalendarStatus(bool monitoring)
{
	if (!isDaemonRunning(false))
		monitoring = false;
	KPopupMenu* menu = contextMenu();
	menu->setItemChecked(mAlarmsEnabledId, monitoring);
}

/******************************************************************************
* Tell the alarm daemon to enable/disable monitoring of the calendar file.
*/
void DockWindow::enableCalendar(bool enable)
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
void DockWindow::toggleAlarmsEnabled()
{
	bool newstate = !contextMenu()->isItemChecked(mAlarmsEnabledId);
	if (newstate  &&  !isDaemonRunning())
	{
		// The daemon is not running, so start it
		QString execStr = locate("exe", DAEMON_APP_NAME);
		if (execStr.isEmpty())
		{
			KMessageBox::error(this, i18n("Alarm Daemon not found"), i18n("%1 Error").arg(kapp->aboutData()->programName()));
			kdError() << "DockWindow::toggleAlarmsEnabled(): kalarmd not found" << endl;
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
* Called when the Configure KAlarm context menu item is selected.
* Displays the KAlarm configuration dialog.
*/
void DockWindow::slotConfigKAlarm()
{
	KAlarmPrefDlg* pref = new KAlarmPrefDlg(theApp()->settings());
	if (pref->exec())
		theApp()->settings()->saveSettings();
}

/******************************************************************************
* Called when the Configure Daemon context menu item is selected.
* Displays the alarm daemon configuration dialog.
*/
void DockWindow::slotConfigDaemon()
{
	KProcess proc;
	proc << QString::fromLatin1("kcmshell") << QString::fromLatin1("alarmdaemonctrl");
	proc.start(KProcess::DontCare);
}

/******************************************************************************
*  Called when the Activate KAlarm context menu item is selected.
*/
void DockWindow::slotKAlarm()
{
kdDebug()<<"DockWindow::slotKAlarm()\n";
	KProcess proc;
	proc << QString::fromLatin1(kapp->aboutData()->appName());
	proc.start(KProcess::DontCare);
}

/******************************************************************************
* Called when the Quit context menu item is selected.
* Closes the system tray window, but does not exit the program if other windows
* are still open.
*/
void DockWindow::slotQuit()
{
kdDebug()<<"DockWindow::slotQuit()\n";
	delete mTrayWindow;
}

/******************************************************************************
* Called by the timer after the Alarms Enabled context menu item is selected,
* to update the GUI status once the daemon has responded to the command.
*/
void DockWindow::setDaemonStatus(bool newstatus)
{
	bool oldstatus = contextMenu()->isItemChecked(mAlarmsEnabledId);
	kdDebug(5950) << "DockWindow::setDaemonStatus(): "<<(int)oldstatus<<"->"<<(int)newstatus<<endl;
	if (newstatus != oldstatus)
	{
		setPixmap(newstatus ? mPixmapEnabled : mPixmapDisabled);
		contextMenu()->setItemChecked(mAlarmsEnabledId, newstatus);
	}
}

/******************************************************************************
*  Called when the mouse is clicked over the panel icon.
*  A left click displays the KAlarm main window.
*/
void DockWindow::mousePressEvent(QMouseEvent* e)
{
	if (e->button() == LeftButton)
		slotKAlarm();      // left click: display the main window
	else
		KSystemTray::mousePressEvent(e);
}

/******************************************************************************
* Register as a GUI with the alarm daemon.
*/
void DockWindow::registerWithDaemon()
{
	kdDebug(5950) << "DockWindow::registerWithDaemon()\n";
	AlarmDaemonIface_stub s(DAEMON_APP_NAME, DAEMON_DCOP_OBJECT);
	s.registerGui(kapp->aboutData()->appName(), TRAY_DCOP_OBJECT_NAME);
}

/******************************************************************************
* Check whether the alarm daemon is currently running.
*/
bool DockWindow::isDaemonRunning(bool updateDockWindow)
{
	bool newstatus = kapp->dcopClient()->isApplicationRegistered(static_cast<const char*>(DAEMON_APP_NAME));
	if (!updateDockWindow)
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
void DockWindow::checkDaemonRunning()
{
	isDaemonRunning();
	if (mDaemonStatusTimerCount > 0  &&  --mDaemonStatusTimerCount <= 0)   // limit how long we check at fast rate
		mDaemonStatusTimer.changeInterval(mDaemonStatusTimerInterval * 1000);
}

/******************************************************************************
* Starts checking at a faster rate whether the daemon is running.
*/
void DockWindow::setFastDaemonCheck()
{
	mDaemonStatusTimer.start(500);     // check new status every half second
	mDaemonStatusTimerCount = 20;      // don't check at this rate for more than 10 seconds
}

/******************************************************************************
* Called when a program setting has changed.
* If the system tray icon update interval has changed, reset the timer.
*/
void DockWindow::slotSettingsChanged()
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
