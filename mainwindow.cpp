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


KAlarmMainWindow::KAlarmMainWindow()
	: MainWindowBase(0L, 0L, WGroupLeader | WStyle_ContextHelp | WDestructiveClose)
{
	kdDebug(5950) << "KAlarmMainWindow::KAlarmMainWindow()\n";
	setAutoSaveSettings(QString::fromLatin1("MainWindow"));    // save window sizes etc.
	setPlainCaption(kapp->aboutData()->programName());

	listView = new AlarmListView(this, "listView");
	setCentralWidget(listView);
	listView->refresh();          // populate the message list
	listView->clearSelection();

	initActions();
	connect(listView, SIGNAL(itemDeleted()), this, SLOT(slotDeletion()));
	connect(listView, SIGNAL(selectionChanged(QListViewItem*)), this, SLOT(slotSelection(QListViewItem*)));
	connect(listView, SIGNAL(mouseButtonClicked(int, QListViewItem*, const QPoint&, int)),
	        this, SLOT(slotMouseClicked(int, QListViewItem*, const QPoint&, int)));
	windowList.append(this);

	if (windowList.count() == 1  &&  theApp()->runInSystemTray())
		theApp()->displayTrayIcon(true, this);
}

KAlarmMainWindow::~KAlarmMainWindow()
{
	kdDebug(5950) << "KAlarmMainWindow::~KAlarmMainWindow()\n";
	if (findWindow(this))
		windowList.remove();
	if (theApp()->trayWindow())
	{
		if (theApp()->trayMainWindow() == this  &&  theApp()->runInSystemTray())
			delete theApp()->trayWindow();
		else
			theApp()->trayWindow()->removeWindow(this);
	}
	theApp()->quitIf();
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
*/
void KAlarmMainWindow::resizeEvent(QResizeEvent* re)
{
	listView->resizeLastColumn();
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
	actionQuit           = KStdAction::quit(this, SLOT(slotQuit()), this);
	actionNew            = new KAction(i18n("&New..."), "eventnew", Qt::Key_Insert, this, SLOT(slotNew()), this, "new");
	actionModify         = new KAction(i18n("&Modify..."), "pencil", Qt::CTRL+Qt::Key_M, this, SLOT(slotModify()), this, "modify");
	actionDelete         = new KAction(i18n("&Delete"), "eventdelete", Qt::Key_Delete, this, SLOT(slotDelete()), this, "delete");
	actionToggleTrayIcon = new KAction(QString(), "kalarm", Qt::CTRL+Qt::Key_T, this, SLOT(slotToggleTrayIcon()), this, "tray");
	actionResetDaemon    = new KAction(i18n("&Reset Daemon"), "reload", 0, this, SLOT(slotResetDaemon()), this, "reset");

	// Set up the menu bar

	KMenuBar* menu = menuBar();
	KPopupMenu* submenu = new KPopupMenu(this, "file");
	menu->insertItem(i18n("&File"), submenu);
	actionQuit->plug(submenu);

	mViewMenu = new KPopupMenu(this, "view");
	mViewMenuId = 0L;
	connect(theApp()->settings(), SIGNAL(settingsChanged()), this, SLOT(slotSettingsChanged()));
	actionToggleTrayIcon->plug(mViewMenu);
	connect(theApp(), SIGNAL(trayIconToggled()), this, SLOT(updateTrayIconAction()));
	updateTrayIconAction();         // set the correct text for this action
	slotSettingsChanged();          // display View menu if appropriate

	mActionsMenu = new KPopupMenu(this, "actions");
	menu->insertItem(i18n("&Actions"), mActionsMenu);
	actionNew->plug(mActionsMenu);
	actionModify->plug(mActionsMenu);
	actionDelete->plug(mActionsMenu);
	mActionsMenu->insertSeparator(3);

	ActionAlarmsEnabled* a = theApp()->actionAlarmEnable();
	mAlarmsEnabledId = a->itemId(a->plug(mActionsMenu));
	connect(a, SIGNAL(alarmsEnabledChange(bool)), this, SLOT(setAlarmEnabledStatus(bool)));
	DaemonGuiHandler* daemonGui = theApp()->daemonGuiHandler();
	if (daemonGui)
	{
		daemonGui->checkStatus();
		setAlarmEnabledStatus(daemonGui->monitoringAlarms());
	}

	actionResetDaemon->plug(mActionsMenu);
	connect(mActionsMenu, SIGNAL(aboutToShow()), this, SLOT(updateActionsMenu()));

	submenu = new KPopupMenu(this, "settings");
	menu->insertItem(i18n("&Settings"), submenu);
	theApp()->actionPreferences()->plug(submenu);
	theApp()->actionDaemonPreferences()->plug(submenu);

	menu->insertItem(i18n("&Help"), helpMenu());

	// Set up the toolbar

	KToolBar* toolbar = toolBar();
	actionNew->plug(toolbar);
	actionModify->plug(toolbar);
	actionDelete->plug(toolbar);


	actionModify->setEnabled(false);
	actionDelete->setEnabled(false);
	if (!theApp()->KDEDesktop())
		actionToggleTrayIcon->setEnabled(false);
}

/******************************************************************************
*  Called when KAlarm settings have changed.
*  Show or hide the View menu in the menu bar.
*/
void KAlarmMainWindow::slotSettingsChanged()
{
	bool systray = theApp()->runInSystemTray();
	if (systray  &&  mViewMenuId
	||  !systray  &&  !mViewMenuId)
	{
		KMenuBar* menu = menuBar();
		if (systray)
		{
			menu->removeItem(mViewMenuId);
			mViewMenuId = 0;
		}
		else
		{
			mViewMenuId = menu->insertItem(i18n("&View"), mViewMenu, -1, 1);
			updateTrayIconAction();
		}
	}
}

/******************************************************************************
* Add a new alarm message to every main window instance.
* 'win' = initiating main window instance (which has already been updated)
*/
void KAlarmMainWindow::addMessage(const KAlarmEvent& event, KAlarmMainWindow* win)
{
	kdDebug(5950) << "KAlarmMainWindow::addMessage(): " << event.id() << endl;
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w != win)
			w->listView->addEntry(event, true);
}

/******************************************************************************
* Modify a message in every main window instance.
* 'win' = initiating main window instance (which has already been updated)
*/
void KAlarmMainWindow::modifyMessage(const QString& oldEventID, const KAlarmEvent& newEvent, KAlarmMainWindow* win)
{
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w != win)
			w->modifyMessage(oldEventID, newEvent);
}

/******************************************************************************
* Modify a message in the displayed list.
*/
void KAlarmMainWindow::modifyMessage(const QString& oldEventID, const KAlarmEvent& newEvent)
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
* Delete a message from every main window instance.
* 'win' = initiating main window instance (which has already been updated)
*/
void KAlarmMainWindow::deleteMessage(const KAlarmEvent& event, KAlarmMainWindow* win)
{
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w != win)
			w->deleteMessage(event);
}

/******************************************************************************
* Delete a message from the displayed list.
*/
void KAlarmMainWindow::deleteMessage(const KAlarmEvent& event)
{
	AlarmListViewItem* item = listView->getEntry(event.id());
	if (item)
		listView->deleteEntry(item, true);
	else
		listView->refresh();
}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////

/******************************************************************************
*  Called when the New button is clicked to edit a new message to add to the
*  list.
*/
void KAlarmMainWindow::slotNew()
{
	EditAlarmDlg* editDlg = new EditAlarmDlg(i18n("New message"), this, "editDlg");
	if (editDlg->exec() == QDialog::Accepted)
	{
		KAlarmEvent event;
		editDlg->getEvent(event);

		// Add the message to the displayed lists and to the calendar file
		theApp()->addMessage(event, this);
		AlarmListViewItem* item = listView->addEntry(event, true);
		listView->setSelected(item, true);
	}
}

/******************************************************************************
*  Called when the Modify button is clicked to edit the currently highlighted
*  message in the list.
*/
void KAlarmMainWindow::slotModify()
{
	AlarmListViewItem* item = listView->selectedItem();
	if (item)
	{
		KAlarmEvent event = listView->getEntry(item);
		EditAlarmDlg* editDlg = new EditAlarmDlg(i18n("Edit message"), this, "editDlg", &event);
		if (editDlg->exec() == QDialog::Accepted)
		{
			KAlarmEvent newEvent;
			editDlg->getEvent(newEvent);

			// Update the event in the displays and in the calendar file
			theApp()->modifyMessage(event.id(), newEvent, this);
			item = listView->updateEntry(item, newEvent, true);
			listView->setSelected(item, true);
		}
	}
}

/******************************************************************************
*  Called when the Delete button is clicked to delete the currently highlighted
*  message in the list.
*/
void KAlarmMainWindow::slotDelete()
{
	AlarmListViewItem* item = listView->selectedItem();
	if (item)
	{
		KAlarmEvent event = listView->getEntry(item);

		// Delete the event from the displays
		theApp()->deleteMessage(event, this);
		listView->deleteEntry(item, true);
	}
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
	actionToggleTrayIcon->setEnabled(!theApp()->runInSystemTray());
	actionToggleTrayIcon->setText(theApp()->trayIconDisplayed() ? i18n("Hide System &Tray Icon") : i18n("Show System &Tray Icon"));
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
	if (theApp()->runInSystemTray()  &&  theApp()->trayMainWindow() == this)
		hide();          // closing would also close the system tray icon
	else
		close();
}

/******************************************************************************
*  Called when the user attempts to close the window.
*/
void KAlarmMainWindow::closeEvent(QCloseEvent* ce)
{
	if (theApp()->runInSystemTray()  &&  theApp()->trayMainWindow() == this)
	{
		ce->ignore();
		hide();
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
		actionModify->setEnabled(false);
		actionDelete->setEnabled(false);
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
		actionModify->setEnabled(true);
		actionDelete->setEnabled(true);
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
		actionModify->setEnabled(false);
		actionDelete->setEnabled(false);
	}
	else if (button == Qt::RightButton)
	{
		kdDebug(5950) << "KAlarmMainWindow::slotMouseClicked(right)\n";
		QPopupMenu* menu = new QPopupMenu(this, "ListContextMenu");
		actionModify->plug(menu);
		actionDelete->plug(menu);
		menu->exec(pt);
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
			return 0L;
		}
		else
		{
			// The window is hidden, so display it
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
