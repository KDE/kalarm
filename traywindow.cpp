/*
 *  traywindow.cpp  -  the KDE system tray applet
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie  software@astrojar.org.uk
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
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */
#include "kalarm.h"
#include <stdlib.h>

#include <qtooltip.h>
#include <qlabel.h>

#include <kapplication.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kstdaction.h>
#include <kaboutdata.h>
#include <kpopupmenu.h>
#include <kmessagebox.h>
#include <kstandarddirs.h>
#include <kstdguiitem.h>
#include <kconfig.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "mainwindow.h"
#include "messagewin.h"
#include "daemongui.h"
#include "preferences.h"
#include "traywindow.moc"


/*=============================================================================
= Class: TrayWindow
= The KDE system tray window.
=============================================================================*/

TrayWindow::TrayWindow(KAlarmMainWindow* parent, const char* name)
	: KSystemTray((theApp()->wantRunInSystemTray() ? parent : 0), name),
	  mAssocMainWindow(parent),
	  mQuitReplaced(false),
	  mActionCollection(new KActionCollection(this))
{
	kdDebug(5950) << "TrayWindow::TrayWindow()\n";
	// Set up GUI icons
	mPixmapEnabled  = BarIcon("kalarm");
	mPixmapDisabled = BarIcon("kalarm_disabled");
	if (mPixmapEnabled.isNull() || mPixmapDisabled.isNull())
		KMessageBox::sorry(this, i18n("Can't load system tray icon!"),
		                         i18n("%1 Error").arg(kapp->aboutData()->programName()));

	mActionQuit = KStdAction::quit(this, SLOT(slotQuit()), mActionCollection);

	// Set up the context menu
	ActionAlarmsEnabled* a = theApp()->actionAlarmEnable();
	mAlarmsEnabledId = a->itemId(a->plug(contextMenu()));
	connect(a, SIGNAL(alarmsEnabledChange(bool)), this, SLOT(setEnabledStatus(bool)));
	theApp()->actionNewAlarm()->plug(contextMenu());
	theApp()->actionPreferences()->plug(contextMenu());
	theApp()->actionDaemonControl()->plug(contextMenu());

	// Set icon to correspond with the alarms enabled menu status
	DaemonGuiHandler* daemonGui = theApp()->daemonGuiHandler();
	daemonGui->checkStatus();
	setEnabledStatus(daemonGui->monitoringAlarms());

	QToolTip::add(this, kapp->aboutData()->programName());
}

TrayWindow::~TrayWindow()
{
	kdDebug(5950) << "TrayWindow::~TrayWindow()\n";
	theApp()->removeWindow(this);
	emit deleted();
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
		QString quitText = mActionQuit->text();
		for (unsigned n = 0;  n < menu->count();  ++n)
		{
			QString txt = menu->text(menu->idAt(n));
			if (txt.startsWith(quitText))
			{
				menu->removeItemAt(n);
				break;
			}
		}
		mActionQuit->plug(menu);
		mQuitReplaced = true;
	}

	// Update the Alarms Enabled item status
	theApp()->daemonGuiHandler()->checkStatus();
}

/******************************************************************************
* Called when the Quit context menu item is selected.
* Closes the system tray window, and if in 'run in system tray' mode closes all
* main windows, but does not exit the program if other windows are still open.
*/
void TrayWindow::slotQuit()
{
	kdDebug(5950)<<"TrayWindow::slotQuit()\n";
	if (theApp()->alarmsDisabledIfStopped()
#if KDE_VERSION < 290
	&&  quitWarning()
#endif
	&&  KMessageBox::warningYesNo(this, i18n("Quitting will disable alarms\n"
	                                         "(once any alarm message windows are closed)."),
	                              QString::null, mActionQuit->text(), KStdGuiItem::cancel(),
	                              QString::fromLatin1("QuitWarn")
	                             ) != KMessageBox::Yes)
		return;
	if (theApp()->wantRunInSystemTray())
	{
		if (!MessageWin::instanceCount())
			theApp()->quit();
		else
			KAlarmMainWindow::closeAll();
	}
	theApp()->displayTrayIcon(false);
}

/******************************************************************************
* Called to allow quit warning messages again.
*/
void TrayWindow::setQuitWarning(bool warn)
{
	KConfig* config = kapp->config();
	config->setGroup(QString::fromLatin1("Notification Messages"));
	config->writeEntry(QString::fromLatin1("QuitWarn"), QString::fromLatin1(warn ? "" : "Yes"));
	config->sync();
}

/******************************************************************************
* Return whether quit warning messages are output.
*/
bool TrayWindow::quitWarning()
{
	KConfig* config = kapp->config();
	config->setGroup(QString::fromLatin1("Notification Messages"));
	return config->readEntry(QString::fromLatin1("QuitWarn")) != QString::fromLatin1("Yes");
}

/******************************************************************************
* Called when the Alarms Enabled action status has changed.
* Updates the alarms enabled menu item check state, and the icon pixmap.
*/
void TrayWindow::setEnabledStatus(bool status)
{
	kdDebug(5950)<<"TrayWindow::setEnabledStatus(" << (int)status << ")\n";
	setPixmap(status ? mPixmapEnabled : mPixmapDisabled);
	contextMenu()->setItemChecked(mAlarmsEnabledId, status);
}

/******************************************************************************
*  Called when the mouse is clicked over the panel icon.
*  A left click displays the KAlarm main window.
*/
void TrayWindow::mousePressEvent(QMouseEvent* e)
{

	if (e->button() == LeftButton  &&  !theApp()->wantRunInSystemTray())
	{
		// Left click: display/hide the first main window
		mAssocMainWindow = KAlarmMainWindow::toggleWindow(mAssocMainWindow);
	}
	else
		KSystemTray::mousePressEvent(e);
}

/******************************************************************************
*  Called when the mouse is released over the panel icon.
*  The main window (if not hidden) is raised and made the active window.
*  If this is done in mousePressEvent(), it doesn't work.
*/
void TrayWindow::mouseReleaseEvent(QMouseEvent* e)
{

	if (e->button() == LeftButton  &&  mAssocMainWindow  &&  mAssocMainWindow->isVisible())
	{
		mAssocMainWindow->raise();
		mAssocMainWindow->setActiveWindow();
	}
	else
		KSystemTray::mouseReleaseEvent(e);
}

/******************************************************************************
* Called when the associated main window is closed.
*/
void TrayWindow::removeWindow(KAlarmMainWindow* win)
{
	if (win == mAssocMainWindow)
		mAssocMainWindow = 0;
}


#ifdef HAVE_X11_HEADERS
	#include <X11/X.h>
	#include <X11/Xlib.h>
	#include <X11/Xutil.h>
#endif

/******************************************************************************
* Check whether the widget is in the system tray.
* Note that it is only sometime AFTER the show event that the system tray
* becomes the widget's parent. So for a definitive status, call this method
* only after waiting a bit...
* Reply = true if the widget is in the system tray, or its status can't be determined.
*       = false if it is not currently in the system tray.
*/
bool TrayWindow::inSystemTray() const
{
#ifdef HAVE_X11_HEADERS
	Window  xParent;    // receives parent window
	Window  root;
	Window* children = 0;
	unsigned int nchildren;
  // Find the X parent window of the widget. This is not the same as the Qt parent widget.
	if (!XQueryTree(qt_xdisplay(), winId(), &root, &xParent, &children, &nchildren))
		return true;    // error determining its parent X window
	if (children)
		XFree(children);

	// If it is in the system tray, the system tray window will be its X parent.
	// Otherwise, the root window will be its X parent.
	return xParent != root;
#else
	return true;
#endif // HAVE_X11_HEADERS
}
