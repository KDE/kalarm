/*
 *  mainwindow.cpp  -  main application window
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <qiconset.h>

#include <kmenubar.h>
#include <ktoolbar.h>
#include <kpopupmenu.h>
#include <kaccel.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <kdebug.h>

#include "kalarmapp.h"
#include "alarmcalendar.h"
#include "daemongui.h"
#include "traywindow.h"
#include "editdlg.h"
#include "prefdlg.h"
#include "prefsettings.h"
#include "alarmlistview.h"
#include "mainwindow.moc"


/*=============================================================================
=  Class: KAlarmMainWindow
=============================================================================*/

QPtrList<KAlarmMainWindow> KAlarmMainWindow::windowList;


KAlarmMainWindow::KAlarmMainWindow(bool restored)
	: MainWindowBase(0, 0, WGroupLeader | WStyle_ContextHelp | WDestructiveClose),
	  mHiddenTrayParent(false),
	  mShowExpired(false)
{
	kdDebug(5950) << "KAlarmMainWindow::KAlarmMainWindow()\n";
	setAutoSaveSettings(QString::fromLatin1("MainWindow"));    // save window sizes etc.
	setPlainCaption(kapp->aboutData()->programName());
	if (!restored)
		resize(theApp()->readConfigWindowSize("MainWindow", size()));

	theApp()->checkCalendar();    // ensure calendar is open
	listView = new AlarmListView(this, "listView");
	setCentralWidget(listView);
	listView->refresh();          // populate the alarm list
	listView->clearSelection();

	initActions();
	connect(listView, SIGNAL(itemDeleted()), SLOT(slotDeletion()));
	connect(listView, SIGNAL(selectionChanged(QListViewItem*)), SLOT(slotSelection(QListViewItem*)));
	connect(listView, SIGNAL(mouseButtonClicked(int, QListViewItem*, const QPoint&, int)),
	        SLOT(slotMouseClicked(int, QListViewItem*, const QPoint&, int)));
	connect(listView, SIGNAL(executed(QListViewItem*)), SLOT(slotDoubleClicked(QListViewItem*)));

	windowList.append(this);
	if (windowList.count() == 1  &&  theApp()->daemonGuiHandler())
	{
		// It's the first main window, and the DCOP handler is ready
		if (theApp()->wantRunInSystemTray())
			theApp()->displayTrayIcon(true, this);     // create system tray icon for run-in-system-tray mode
		else if (theApp()->trayWindow())
			theApp()->trayWindow()->setAssocMainWindow(this);    // associate this window with the system tray icon
	}
}

KAlarmMainWindow::~KAlarmMainWindow()
{
	kdDebug(5950) << "KAlarmMainWindow::~KAlarmMainWindow()\n";
	if (findWindow(this))
		windowList.remove();
	if (theApp()->trayWindow())
	{
		if (trayParent())
			delete theApp()->trayWindow();
		else
			theApp()->trayWindow()->removeWindow(this);
	}
	KAlarmMainWindow* main = mainMainWindow();
	if (main)
		theApp()->writeConfigWindowSize("MainWindow", main->size());
	KGlobal::config()->sync();    // save any new window size to disc
	theApp()->quitIf();
}

/******************************************************************************
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
void KAlarmMainWindow::saveProperties(KConfig* config)
{
	config->writeEntry(QString::fromLatin1("HiddenTrayParent"), trayParent() && isHidden());
	config->writeEntry(QString::fromLatin1("ShowExpired"), mShowExpired);
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being
* restored. Read in whatever was saved in saveProperties().
*/
void KAlarmMainWindow::readProperties(KConfig* config)
{
	mHiddenTrayParent = config->readBoolEntry(QString::fromLatin1("HiddenTrayParent"));
	mShowExpired      = config->readBoolEntry(QString::fromLatin1("ShowExpired"));
}

/******************************************************************************
* Get the main main window, i.e. the parent of the system tray icon, or if
* none, the first main window to be created. Visible windows take precedence
* over hidden ones.
*/
KAlarmMainWindow* KAlarmMainWindow::mainMainWindow()
{
	KAlarmMainWindow* tray = theApp()->trayWindow() ? theApp()->trayWindow()->assocMainWindow() : 0;
	if (tray  &&  tray->isVisible())
		return tray;
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w->isVisible())
				return w;
	if (tray)
		return tray;
	return windowList.first();
}

/******************************************************************************
* Check whether this main window is the parent of the system tray icon.
*/
bool KAlarmMainWindow::trayParent() const
{
	return theApp()->wantRunInSystemTray()  &&  theApp()->trayMainWindow() == this;
}

/******************************************************************************
*  Close all main windows.
*/
void KAlarmMainWindow::closeAll()
{
	while (windowList.first())
		delete windowList.first();
}

/******************************************************************************
*  Called when the window's size has changed (before it is painted).
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*  Records the new size in the config file.
*/
void KAlarmMainWindow::resizeEvent(QResizeEvent* re)
{
	listView->resizeLastColumn();
	// Save the window's new size only if it's the first main window
	if (mainMainWindow() == this)
		theApp()->writeConfigWindowSize("MainWindow", re->size());
	MainWindowBase::resizeEvent(re);
}

/******************************************************************************
*  Called when the window is first displayed.
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void KAlarmMainWindow::showEvent(QShowEvent* se)
{
	listView->resizeLastColumn();
	MainWindowBase::showEvent(se);
}

/******************************************************************************
*  Initialise the menu, toolbar and main window actions.
*/
void KAlarmMainWindow::initActions()
{
	KActionCollection* actions = actionCollection();
	actionQuit           = KStdAction::quit(this, SLOT(slotQuit()), actions);
#if KDE_VERSION >= 310
	actionNew            = new KAction(i18n("&New..."), "filenew2", Qt::Key_Insert, this, SLOT(slotNew()), actions, "new");
#else
	actionNew            = new KAction(i18n("&New..."), "filenew", Qt::Key_Insert, this, SLOT(slotNew()), actions, "new");
#endif
	actionCopy           = new KAction(i18n("&Copy..."), "editcopy", Qt::SHIFT+Qt::Key_Insert, this, SLOT(slotCopy()), actions, "copy");
	actionModify         = new KAction(i18n("&Modify..."), "edit", Qt::CTRL+Qt::Key_M, this, SLOT(slotModify()), actions, "modify");
	actionDelete         = new KAction(i18n("&Delete"), "editdelete", Qt::Key_Delete, this, SLOT(slotDelete()), actions, "delete");
	actionUndelete       = new KAction(i18n("&Undelete"), "undo", Qt::CTRL+Qt::Key_U, this, SLOT(slotUndelete()), actions, "undelete");
	actionShowExpired    = new KAction(i18n("&Show Expired Alarms"), Qt::CTRL+Qt::Key_S, this, SLOT(slotShowExpired()), actions, "expired");
	actionView           = new KAction(i18n("&View"), "viewmag", Qt::CTRL+Qt::Key_V, this, SLOT(slotView()), actions, "view");
	actionToggleTrayIcon = new KAction(i18n("Show in System &Tray"), Qt::CTRL+Qt::Key_T, this, SLOT(slotToggleTrayIcon()), actions, "tray");
	actionRefreshAlarms  = new KAction(i18n("&Refresh Alarms"), "reload", 0, this, SLOT(slotResetDaemon()), actions, "refresh");

	// Set up the menu bar

	KMenuBar* menu = menuBar();
	KPopupMenu* submenu = new KPopupMenu(this, "file");
	menu->insertItem(i18n("&File"), submenu);
	actionQuit->plug(submenu);

	mViewMenu = new KPopupMenu(this, "view");
	menu->insertItem(i18n("&View"), mViewMenu);
	actionShowExpired->plug(mViewMenu);
	mShowExpiredId = mViewMenu->idAt(0);
	mViewMenu->setItemChecked(mShowExpiredId, mShowExpired);
	actionToggleTrayIcon->plug(mViewMenu);
	mShowTrayId = mViewMenu->idAt(1);
	connect(theApp()->settings(), SIGNAL(settingsChanged()), SLOT(updateTrayIconAction()));
	connect(theApp(), SIGNAL(trayIconToggled()), SLOT(updateTrayIconAction()));
	updateTrayIconAction();         // set the correct text for this action

	mActionsMenu = new KPopupMenu(this, "actions");
	menu->insertItem(i18n("&Actions"), mActionsMenu);
	actionNew->plug(mActionsMenu);
	actionCopy->plug(mActionsMenu);
	actionModify->plug(mActionsMenu);
	actionDelete->plug(mActionsMenu);
	actionUndelete->plug(mActionsMenu);
	actionView->plug(mActionsMenu);
	mActionsMenu->insertSeparator(6);

	ActionAlarmsEnabled* a = theApp()->actionAlarmEnable();
	mAlarmsEnabledId = a->itemId(a->plug(mActionsMenu));
	connect(a, SIGNAL(alarmsEnabledChange(bool)), SLOT(setAlarmEnabledStatus(bool)));
	DaemonGuiHandler* daemonGui = theApp()->daemonGuiHandler();
	if (daemonGui)
	{
		daemonGui->checkStatus();
		setAlarmEnabledStatus(daemonGui->monitoringAlarms());
	}

	actionRefreshAlarms->plug(mActionsMenu);
	connect(mActionsMenu, SIGNAL(aboutToShow()), SLOT(updateActionsMenu()));

	submenu = new KPopupMenu(this, "settings");
	menu->insertItem(i18n("&Settings"), submenu);
	theApp()->actionPreferences()->plug(submenu);
	theApp()->actionDaemonControl()->plug(submenu);

	menu->insertItem(i18n("&Help"), helpMenu());

	// Set up the toolbar

	KToolBar* toolbar = toolBar();
	actionNew->plug(toolbar);
	actionCopy->plug(toolbar);
	actionModify->plug(toolbar);
	actionDelete->plug(toolbar);
	actionUndelete->plug(toolbar);
	actionView->plug(toolbar);

	actionCopy->setEnabled(false);
	actionModify->setEnabled(false);
	actionDelete->setEnabled(false);
	actionUndelete->setEnabled(false);
	actionView->setEnabled(false);
	if (!theApp()->settings()->expiredKeepDays())
		actionShowExpired->setEnabled(false);
	if (!theApp()->KDEDesktop())
		actionToggleTrayIcon->setEnabled(false);
}

/******************************************************************************
* Refresh the alarm list in every main window instance.
*/
void KAlarmMainWindow::refresh()
{
	kdDebug(5950) << "KAlarmMainWindow::refresh()\n";
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		w->listView->refresh();
}

/******************************************************************************
* Refresh the alarm list in every main window instance which is displaying
* expired alarms.
* Called when an expired alarm setting changes in the user preferences.
*/
void KAlarmMainWindow::updateExpired()
{
	kdDebug(5950) << "KAlarmMainWindow::updateExpired()\n";
	bool enableShowExpired = !!theApp()->settings()->expiredKeepDays();
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
	{
		if (w->mShowExpired)
		{
			if (!enableShowExpired)
				w->slotShowExpired();
			else
				w->listView->refresh();
		}
		w->actionShowExpired->setEnabled(enableShowExpired);
	}
}

/******************************************************************************
* Add a new alarm to every main window instance.
* 'win' = initiating main window instance (which has already been updated)
*/
void KAlarmMainWindow::addEvent(const KAlarmEvent& event, KAlarmMainWindow* win)
{
	kdDebug(5950) << "KAlarmMainWindow::addEvent(): " << event.id() << endl;
	bool expired = event.expired();
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w != win)
		{
			if (!expired  ||  w->mShowExpired)
				w->listView->addEntry(event, true);
		}
}

/******************************************************************************
* Modify an alarm in every main window instance.
* 'win' = initiating main window instance (which has already been updated)
*/
void KAlarmMainWindow::modifyEvent(const QString& oldEventID, const KAlarmEvent& newEvent, KAlarmMainWindow* win)
{
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w != win)
			w->modifyEvent(oldEventID, newEvent);
}

/******************************************************************************
* Modify an alarm in the displayed list.
*/
void KAlarmMainWindow::modifyEvent(const QString& oldEventID, const KAlarmEvent& newEvent)
{
	AlarmListViewItem* item = listView->getEntry(oldEventID);
	if (item)
	{
		listView->deleteEntry(item);
		listView->addEntry(newEvent, true);
	}
	else
		listView->refresh();
}

/******************************************************************************
* Delete an alarm from every main window instance.
* 'win' = initiating main window instance (which has already been updated)
*/
void KAlarmMainWindow::deleteEvent(const QString& eventID, KAlarmMainWindow* win)
{
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w != win)
			w->deleteEvent(eventID);
}

/******************************************************************************
* Delete an alarm from the displayed list.
*/
void KAlarmMainWindow::deleteEvent(const QString& eventID)
{
	AlarmListViewItem* item = listView->getEntry(eventID);
	if (item)
		listView->deleteEntry(item, true);
	else
		listView->refresh();
}

/******************************************************************************
* Undelete an alarm in the displayed list.
*/
void KAlarmMainWindow::undeleteEvent(const QString& oldEventID, const KAlarmEvent& event, KAlarmMainWindow* win)
{
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w != win)
			w->undeleteEvent(oldEventID, event);
}

/******************************************************************************
* Undelete an alarm in the displayed list.
*/
void KAlarmMainWindow::undeleteEvent(const QString& oldEventID, const KAlarmEvent& event)
{
	AlarmListViewItem* item = listView->getEntry(oldEventID);
	if (item)
		listView->deleteEntry(item, true);
	listView->addEntry(event, true);
}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////

/******************************************************************************
*  Called when the New button is clicked to edit a new alarm to add to the list.
*/
void KAlarmMainWindow::slotNew()
{
	EditAlarmDlg* editDlg = new EditAlarmDlg(i18n("New Alarm"), this, "editDlg");
	if (editDlg->exec() == QDialog::Accepted)
	{
		KAlarmEvent event;
		editDlg->getEvent(event);

		// Add the alarm to the displayed lists and to the calendar file
		theApp()->addEvent(event, this);
		AlarmListViewItem* item = listView->addEntry(event, true);
		listView->setSelected(item, true);
	}
}

/******************************************************************************
*  Called when the Copy button is clicked to edit a copy of an existing alarm,
*  to add to the list.
*/
void KAlarmMainWindow::slotCopy()
{
	AlarmListViewItem* item = listView->selectedItem();
	if (item)
	{
		KAlarmEvent event = listView->getEvent(item);
		EditAlarmDlg* editDlg = new EditAlarmDlg(i18n("New Alarm"), this, "editDlg", &event);
		if (editDlg->exec() == QDialog::Accepted)
		{
			KAlarmEvent event;
			editDlg->getEvent(event);

			// Add the alarm to the displayed lists and to the calendar file
			theApp()->addEvent(event, this);
			AlarmListViewItem* item = listView->addEntry(event, true);
			listView->setSelected(item, true);
		}
	}
}

/******************************************************************************
*  Called when the Modify button is clicked to edit the currently highlighted
*  alarm in the list.
*/
void KAlarmMainWindow::slotModify()
{
	AlarmListViewItem* item = listView->selectedItem();
	if (item)
	{
		KAlarmEvent event = listView->getEvent(item);
		EditAlarmDlg* editDlg = new EditAlarmDlg(i18n("Edit Alarm"), this, "editDlg", &event);
		if (editDlg->exec() == QDialog::Accepted)
		{
			KAlarmEvent newEvent;
			editDlg->getEvent(newEvent);

			// Update the event in the displays and in the calendar file
			theApp()->modifyEvent(event, newEvent, this);
			item = listView->updateEntry(item, newEvent, true);
			listView->setSelected(item, true);
		}
	}
}

/******************************************************************************
*  Called when the View button is clicked to view the currently highlighted
*  alarm in the list.
*/
void KAlarmMainWindow::slotView()
{
	AlarmListViewItem* item = listView->selectedItem();
	if (item)
	{
		KAlarmEvent event = listView->getEvent(item);
		EditAlarmDlg* editDlg = new EditAlarmDlg((event.expired() ? i18n("Expired Alarm") : i18n("View Alarm")),
		                                         this, "editDlg", &event, true);
		editDlg->exec();
	}
}

/******************************************************************************
*  Called when the Delete button is clicked to delete the currently highlighted
*  alarm in the list.
*/
void KAlarmMainWindow::slotDelete()
{
	AlarmListViewItem* item = listView->selectedItem();
	if (item)
	{
		if (theApp()->settings()->confirmAlarmDeletion())
		{
			int n = 1;
			if (KMessageBox::warningContinueCancel(this, i18n("Do you really want to delete the selected alarm?", "Do you really want to delete the %n selected alarms?", n),
			                                       i18n("Delete Alarm", "Delete Alarms", n), i18n("&Delete"))
			    != KMessageBox::Continue)
				return;
		}
		KAlarmEvent event = listView->getEvent(item);

		// Delete the event from the displays
		theApp()->deleteEvent(event, this);
		listView->deleteEntry(item, true);
	}
}

/******************************************************************************
*  Called when the Undelete button is clicked to reinstate the currently
*  highlighted expired alarm in the list.
*/
void KAlarmMainWindow::slotUndelete()
{
	AlarmListViewItem* item = listView->selectedItem();
	if (item)
	{
		KAlarmEvent event = listView->getEvent(item);

		// Add the alarm to the displayed lists and to the calendar file
		theApp()->undeleteEvent(event, this);
		item = listView->updateEntry(item, event, true);
		listView->setSelected(item, true);
	}
}

/******************************************************************************
*  Called when the Show Expired Alarms menu item is selected or deselected.
*/
void KAlarmMainWindow::slotShowExpired()
{
	mShowExpired = !mShowExpired;
	mViewMenu->setItemChecked(mShowExpiredId, mShowExpired);
	listView->showExpired(mShowExpired);
	listView->refresh();
}

/******************************************************************************
*  Called when the Display System Tray Icon menu item is selected.
*/
void KAlarmMainWindow::slotToggleTrayIcon()
{
	theApp()->displayTrayIcon(!theApp()->trayIconDisplayed(), this);
}

/******************************************************************************
* Called when the system tray icon is created or destroyed.
* Set the system tray icon menu text according to whether or not the system
* tray icon is currently visible.
*/
void KAlarmMainWindow::updateTrayIconAction()
{
	if (theApp()->wantRunInSystemTray())
;//		actionToggleTrayIcon->unplug(mViewMenu);
	else
	{
//		actionToggleTrayIcon->plug(mViewMenu);
//		mShowTrayId = mViewMenu->idAt(1);
	}
	actionToggleTrayIcon->setEnabled(!theApp()->wantRunInSystemTray());
	mViewMenu->setItemChecked(mShowTrayId, theApp()->trayIconDisplayed());
}

/******************************************************************************
* Called when the Actions menu is about to be displayed.
* Update the status of the Alarms Enabled menu item.
*/
void KAlarmMainWindow::updateActionsMenu()
{
	theApp()->daemonGuiHandler()->checkStatus();   // update the Alarms Enabled item status
}

/******************************************************************************
*  Called when the Reset Daemon menu item is selected.
*/
void KAlarmMainWindow::slotResetDaemon()
{
	theApp()->resetDaemon();
}

/******************************************************************************
*  Called when the Quit menu item is selected.
*/
void KAlarmMainWindow::slotQuit()
{
	if (trayParent())
	{
		hide();          // closing would also close the system tray icon
		theApp()->quitIf();
	}
	else
		close();
}

/******************************************************************************
*  Called when the user or the session manager attempts to close the window.
*/
void KAlarmMainWindow::closeEvent(QCloseEvent* ce)
{
	if (!theApp()->sessionClosingDown()  &&  trayParent())
	{
		// The user (not the session manager) wants to close the window.
		// It's the parent window of the system tray icon, so just hide
		// it to prevent the system tray icon closing.
		hide();
		theApp()->quitIf();
		ce->ignore();
	}
	else
		ce->accept();
}

/******************************************************************************
*  Called when an item is deleted from the ListView.
*  Disables the actions if no item is still selected.
*/
void KAlarmMainWindow::slotDeletion()
{
	if (!listView->selectedItem())
	{
		kdDebug(5950) << "KAlarmMainWindow::slotDeletion(true)\n";
		actionCopy->setEnabled(false);
		actionModify->setEnabled(false);
		actionView->setEnabled(false);
		actionDelete->setEnabled(false);
		actionUndelete->setEnabled(false);
	}
}

/******************************************************************************
*  Called when the selected item in the ListView changes.
*  Selects the new current item, and enables the actions appropriately.
*/
void KAlarmMainWindow::slotSelection(QListViewItem* item)
{
	if (item)
	{
		kdDebug(5950) << "KAlarmMainWindow::slotSelection(true)\n";
		listView->setSelected(item, true);
		actionCopy->setEnabled(true);
		actionModify->setEnabled(!listView->expired((AlarmListViewItem*)item));
		actionView->setEnabled(true);
		actionDelete->setEnabled(true);
		actionUndelete->setEnabled(listView->expired((AlarmListViewItem*)item));
	}
}

/******************************************************************************
*  Called when the mouse is clicked on the ListView.
*  Deselects the current item and disables the actions if appropriate, or
*  displays a context menu to modify or delete the selected item.
*/
void KAlarmMainWindow::slotMouseClicked(int button, QListViewItem* item, const QPoint& pt, int)
{
	if (!item)
	{
		kdDebug(5950) << "KAlarmMainWindow::slotMouseClicked(left)\n";
		listView->clearSelection();
		actionCopy->setEnabled(false);
		actionModify->setEnabled(false);
		actionView->setEnabled(false);
		actionDelete->setEnabled(false);
		actionUndelete->setEnabled(false);
	}
	else if (button == Qt::RightButton)
	{
		kdDebug(5950) << "KAlarmMainWindow::slotMouseClicked(right)\n";
		QPopupMenu* menu = new QPopupMenu(this, "ListContextMenu");
		actionCopy->plug(menu);
		if (!listView->expired((AlarmListViewItem*)item))
			actionModify->plug(menu);
		actionView->plug(menu);
		actionDelete->plug(menu);
		if (listView->expired((AlarmListViewItem*)item))
			actionUndelete->plug(menu);
		menu->exec(pt);
	}
}

/******************************************************************************
*  Called when the mouse is double clicked on the ListView.
*  If it is double clicked on an item, the modification dialog is displayed.
*/
void KAlarmMainWindow::slotDoubleClicked(QListViewItem* item)
{
	if (item)
	{
		kdDebug(5950) << "KAlarmMainWindow::slotDoubleClicked()\n";
		if (listView->expired((AlarmListViewItem*)item))
			slotView();
		else
			slotModify();
	}
}

/******************************************************************************
* Called when the Alarms Enabled action status has changed.
* Updates the alarms enabled menu item check state.
*/
void KAlarmMainWindow::setAlarmEnabledStatus(bool status)
{
	kdDebug(5950) << "KAlarmMainWindow::setAlarmEnabledStatus(" << (int)status << ")\n";
	mActionsMenu->setItemChecked(mAlarmsEnabledId, status);
}

/******************************************************************************
*  Display or hide the specified main window.
*  This should only be called when the application doesn't run in the system tray.
*/
KAlarmMainWindow* KAlarmMainWindow::toggleWindow(KAlarmMainWindow* win)
{
	if (win  &&  findWindow(win))
	{
		// A window is specified (and it exists)
		if (win->isVisible())
		{
			// The window is visible, so close it
			win->close();
			return 0;
		}
		else
		{
			// The window is hidden, so display it
			win->hide();        // in case it's on a different desktop
			win->showNormal();
			win->raise();
			win->setActiveWindow();
			return win;
		}
	}

	// No window is specified, or the window doesn't exist. Open a new one.
	win = new KAlarmMainWindow;
	win->show();
	return win;
}

/******************************************************************************
* Find the specified window in the main window list.
*/
bool KAlarmMainWindow::findWindow(KAlarmMainWindow* win)
{
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w == win)
			return true;
	return false;
}
