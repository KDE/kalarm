/*
 *  mainwindow.cpp  -  main application window
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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

#include "kalarm.h"

#include <qiconset.h>
#include <qdragobject.h>

#include <kmenubar.h>
#include <ktoolbar.h>
#include <kpopupmenu.h>
#include <kaccel.h>
#include <kaction.h>
#include <kstdaction.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kurldrag.h>
#include <klocale.h>
#include <kglobalsettings.h>
#include <kconfig.h>
#include <kaboutdata.h>
#include <kdebug.h>

#include <maillistdrag.h>

#include "kalarmapp.h"
#include "alarmcalendar.h"
#include "daemongui.h"
#include "traywindow.h"
#include "birthdaydlg.h"
#include "editdlg.h"
#include "prefdlg.h"
#include "preferences.h"
#include "alarmlistview.h"
#include "mainwindow.moc"

using namespace KCal;

static QString messageFromPrefix    = i18n("'From' email address", "From:");
static QString messageToPrefix      = i18n("Email addressee", "To:");
static QString messageDatePrefix    = i18n("Date:");
static QString messageSubjectPrefix = i18n("Email subject", "Subject:");


/*=============================================================================
=  Class: KAlarmMainWindow
=============================================================================*/

QPtrList<KAlarmMainWindow> KAlarmMainWindow::windowList;


KAlarmMainWindow::KAlarmMainWindow(bool restored)
	: MainWindowBase(0, 0, WGroupLeader | WStyle_ContextHelp | WDestructiveClose),
	  mMinuteTimer(0),
	  mMinuteTimerSyncing(false),
	  mHiddenTrayParent(false),
	  mShowTime(Preferences::instance()->showAlarmTime()),
	  mShowTimeTo(Preferences::instance()->showTimeToAlarm()),
	  mShowExpired(Preferences::instance()->showExpiredAlarms())
{
	kdDebug(5950) << "KAlarmMainWindow::KAlarmMainWindow()\n";
	setAutoSaveSettings(QString::fromLatin1("MainWindow"));    // save window sizes etc.
	setPlainCaption(kapp->aboutData()->programName());
	if (!restored)
		resize(theApp()->readConfigWindowSize("MainWindow", size()));

	setAcceptDrops(true);         // allow drag-and-drop onto this window
	theApp()->checkCalendar();    // ensure calendar is open
	listView = new AlarmListView(this, "listView");
	listView->selectTimeColumns(mShowTime, mShowTimeTo);
	listView->showExpired(mShowExpired);
	setCentralWidget(listView);
	listView->refresh();          // populate the alarm list
	listView->clearSelection();

	connect(listView, SIGNAL(itemDeleted()), SLOT(slotDeletion()));
	connect(listView, SIGNAL(selectionChanged()), SLOT(slotSelection()));
	connect(listView, SIGNAL(mouseButtonClicked(int, QListViewItem*, const QPoint&, int)),
	        SLOT(slotMouseClicked(int, QListViewItem*, const QPoint&, int)));
	connect(listView, SIGNAL(executed(QListViewItem*)), SLOT(slotDoubleClicked(QListViewItem*)));
	initActions();

	windowList.append(this);
	if (windowList.count() == 1  &&  theApp()->daemonGuiHandler())
	{
		// It's the first main window, and the DCOP handler is ready
		if (theApp()->wantRunInSystemTray())
			theApp()->displayTrayIcon(true, this);     // create system tray icon for run-in-system-tray mode
		else if (theApp()->trayWindow())
			theApp()->trayWindow()->setAssocMainWindow(this);    // associate this window with the system tray icon
	}
	setUpdateTimer();
}

KAlarmMainWindow::~KAlarmMainWindow()
{
	kdDebug(5950) << "KAlarmMainWindow::~KAlarmMainWindow()\n";
	if (findWindow(this))
		windowList.remove();
	if (theApp()->trayWindow())
	{
		if (isTrayParent())
			delete theApp()->trayWindow();
		else
			theApp()->trayWindow()->removeWindow(this);
	}
	mMinuteTimer = 0;    // to ensure that setUpdateTimer() works correctly
	setUpdateTimer();
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
	config->writeEntry(QString::fromLatin1("HiddenTrayParent"), isTrayParent() && isHidden());
	config->writeEntry(QString::fromLatin1("ShowExpired"), mShowExpired);
	config->writeEntry(QString::fromLatin1("ShowTime"), mShowTime);
	config->writeEntry(QString::fromLatin1("ShowTimeTo"), mShowTimeTo);
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
	mShowTime         = config->readBoolEntry(QString::fromLatin1("ShowTime"));
	mShowTimeTo       = config->readBoolEntry(QString::fromLatin1("ShowTimeTo"));
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
bool KAlarmMainWindow::isTrayParent() const
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
	setUpdateTimer();
	slotUpdateTimeTo();
	MainWindowBase::showEvent(se);
}

/******************************************************************************
*  Called after the window is hidden.
*/
void KAlarmMainWindow::hideEvent(QHideEvent* he)
{
	setUpdateTimer();
	MainWindowBase::hideEvent(he);
}

/******************************************************************************
*  Initialise the menu, toolbar and main window actions.
*/
void KAlarmMainWindow::initActions()
{
	KActionCollection* actions = actionCollection();
	actionQuit           = KStdAction::quit(this, SLOT(slotQuit()), actions);
	KAction* actBirthday = new KAction(i18n("Import &Birthdays..."), 0, this, SLOT(slotBirthdays()), actions, "birthdays");
	actionNew            = KAlarmApp::createNewAlarmAction(i18n("&New..."), this, SLOT(slotNew()), actions);
	actionCopy           = new KAction(i18n("&Copy..."), "editcopy", Qt::SHIFT+Qt::Key_Insert, this, SLOT(slotCopy()), actions, "copy");
	actionModify         = new KAction(i18n("&Modify..."), "edit", Qt::CTRL+Qt::Key_M, this, SLOT(slotModify()), actions, "modify");
	actionDelete         = new KAction(i18n("&Delete"), "editdelete", Qt::Key_Delete, this, SLOT(slotDelete()), actions, "delete");
	actionUndelete       = new KAction(i18n("&Undelete"), "undo", Qt::CTRL+Qt::Key_U, this, SLOT(slotUndelete()), actions, "undelete");
	actionView           = new KAction(i18n("&View"), "viewmag", Qt::CTRL+Qt::Key_V, this, SLOT(slotView()), actions, "view");
	actionShowTime       = new KAction(i18n("&Show Alarm Times"), 0, this, SLOT(slotShowTime()), actions, "time");
	actionShowTimeTo     = new KAction(i18n("&Show Time to Alarms"), 0, this, SLOT(slotShowTimeTo()), actions, "timeTo");
	actionShowExpired    = new KAction(i18n("&Show Expired Alarms"), Qt::CTRL+Qt::Key_S, this, SLOT(slotShowExpired()), actions, "expired");
	actionToggleTrayIcon = new KAction(i18n("Show in System &Tray"), Qt::CTRL+Qt::Key_T, this, SLOT(slotToggleTrayIcon()), actions, "tray");
	actionRefreshAlarms  = new KAction(i18n("&Refresh Alarms"), "reload", 0, this, SLOT(slotResetDaemon()), actions, "refresh");

	// Set up the menu bar

	KMenuBar* menu = menuBar();
	KPopupMenu* submenu = new KPopupMenu(this, "file");
	actBirthday->plug(submenu);
	menu->insertItem(i18n("&File"), submenu);
	actionQuit->plug(submenu);

	mViewMenu = new KPopupMenu(this, "view");
	menu->insertItem(i18n("&View"), mViewMenu);
	actionShowTime->plug(mViewMenu);
	mShowTimeId = mViewMenu->idAt(0);
	mViewMenu->setItemChecked(mShowTimeId, mShowTime);
	actionShowTimeTo->plug(mViewMenu);
	mShowTimeToId = mViewMenu->idAt(1);
	mViewMenu->setItemChecked(mShowTimeToId, mShowTimeTo);
	mViewMenu->insertSeparator(2);
	actionShowExpired->plug(mViewMenu);
	mShowExpiredId = mViewMenu->idAt(3);
	mViewMenu->setItemChecked(mShowExpiredId, mShowExpired);
	actionToggleTrayIcon->plug(mViewMenu);
	mShowTrayId = mViewMenu->idAt(4);
	connect(Preferences::instance(), SIGNAL(preferencesChanged()), SLOT(updateTrayIconAction()));
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
	theApp()->actionDaemonControl()->plug(submenu);
	theApp()->actionPreferences()->plug(submenu);

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
	if (!Preferences::instance()->expiredKeepDays())
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
	bool enableShowExpired = !!Preferences::instance()->expiredKeepDays();
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
* Start or stop the timer which updates the time-to-alarm values every minute.
* Should be called whenever a main window is created or destroyed, or shown or
* hidden.
*/
void KAlarmMainWindow::setUpdateTimer()
{
	// Check whether any windows need to be updated
	KAlarmMainWindow* needTimer = 0;
	KAlarmMainWindow* timerWindow = 0;
	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
	{
		if (w->isVisible()  &&  w->listView->showingTimeTo())
			needTimer = w;
		if (w->mMinuteTimer)
			timerWindow = w;
	}

	// Start or stop the update timer if necessary
	bool active = timerWindow && timerWindow->mMinuteTimer->isActive();
	if (needTimer  &&  !active)
	{
		// Timeout every minute.
		// But first synchronise to two seconds after the minute boundary.
		if (!timerWindow)
		{
			timerWindow = needTimer;
			timerWindow->mMinuteTimer = new QTimer(timerWindow);
		}
		int firstInterval = 62 - QTime::currentTime().second();
		timerWindow->mMinuteTimer->start(1000 * firstInterval);
		timerWindow->mMinuteTimerSyncing = (firstInterval != 60);
		connect(timerWindow->mMinuteTimer, SIGNAL(timeout()), timerWindow, SLOT(slotUpdateTimeTo()));
//		timerWindow->mMinuteTimerReceiver = true;
		kdDebug(5950) << "KAlarmMainWindow::setUpdateTimer(): started timer" << endl;
	}
	else if (!needTimer  &&  active)
	{
		timerWindow->mMinuteTimer->disconnect();
		timerWindow->mMinuteTimer->stop();
		kdDebug(5950) << "KAlarmMainWindow::setUpdateTimer(): stopped timer" << endl;
	}
}
/******************************************************************************
* Update the time-to-alarm values for each main window which is displaying them.
*/
void KAlarmMainWindow::slotUpdateTimeTo()
{
	kdDebug(5950) << "KAlarmMainWindow::slotUpdateTimeTo()" << endl;
	if (mMinuteTimerSyncing)
	{
		// We've synced to the minute boundary. Now set timer to 1 minute intervals.
		mMinuteTimer->changeInterval(60 * 1000);
		mMinuteTimerSyncing = false;
	}

	for (KAlarmMainWindow* w = windowList.first();  w;  w = windowList.next())
		if (w->isVisible()  &&  w->listView->showingTimeTo())
			w->listView->updateTimeToAlarms();
}

/******************************************************************************
* Select an alarm in the displayed list.
*/
void KAlarmMainWindow::selectEvent(const QString& eventID)
{
	listView->clearSelection();
	AlarmListViewItem* item = listView->getEntry(eventID);
	if (item)
	{
		listView->setSelected(item, true);
		listView->setCurrentItem(item);
		listView->ensureItemVisible(item);
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
		listView->deleteEntry(item);
	listView->addEntry(newEvent, true);
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
	executeNew(this);
}

/******************************************************************************
*  Execute a New Alarm dialog, optionally setting the action and text.
*/
void KAlarmMainWindow::executeNew(KAlarmMainWindow* win, KAlarmEvent::Action action, const QString& text)
{
	EditAlarmDlg* editDlg = new EditAlarmDlg(i18n("New Alarm"), win, "editDlg");
	if (!text.isNull())
		editDlg->setAction(action, text);
	if (editDlg->exec() == QDialog::Accepted)
	{
		KAlarmEvent event;
		editDlg->getEvent(event);

		// Add the alarm to the displayed lists and to the calendar file
		theApp()->addEvent(event, win);
		if (win)
		{
			AlarmListView* listView = win->listView;
			AlarmListViewItem* item = listView->addEntry(event, true);
			listView->clearSelection();
			listView->setSelected(item, true);
		}
	}
}

/******************************************************************************
*  Called when the Copy button is clicked to edit a copy of an existing alarm,
*  to add to the list.
*/
void KAlarmMainWindow::slotCopy()
{
	AlarmListViewItem* item = listView->singleSelectedItem();
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
			item = listView->addEntry(event, true);
			listView->clearSelection();
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
	AlarmListViewItem* item = listView->singleSelectedItem();
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
			item = listView->getEntry(event.id());   // in case item deleted since dialog was shown
			item = listView->updateEntry(item, newEvent, true);
			listView->clearSelection();
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
	AlarmListViewItem* item = listView->singleSelectedItem();
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
*  alarms in the list.
*/
void KAlarmMainWindow::slotDelete()
{
	QPtrList<AlarmListViewItem> items = listView->selectedItems();
	if (Preferences::instance()->confirmAlarmDeletion())
	{
		int n = items.count();
		if (KMessageBox::warningContinueCancel(this, i18n("Do you really want to delete the selected alarm?", "Do you really want to delete the %n selected alarms?", n),
						       i18n("Delete Alarm", "Delete Alarms", n), i18n("&Delete"))
		    != KMessageBox::Continue)
			return;
	}
	for (QPtrListIterator<AlarmListViewItem> it(items);  it.current();  ++it)
	{
		AlarmListViewItem* item = it.current();
		KAlarmEvent event = listView->getEvent(item);

		// Delete the event from the displays
		theApp()->deleteEvent(event, this);
		item = listView->getEntry(event.id());   // in case item deleted since dialog was shown
		listView->deleteEntry(item, true);
	}
}

/******************************************************************************
*  Called when the Undelete button is clicked to reinstate the currently
*  highlighted expired alarms in the list.
*/
void KAlarmMainWindow::slotUndelete()
{
	QPtrList<AlarmListViewItem> items = listView->selectedItems();
	listView->clearSelection();
	for (QPtrListIterator<AlarmListViewItem> it(items);  it.current();  ++it)
	{
		AlarmListViewItem* item = it.current();
		KAlarmEvent event = listView->getEvent(item);
		event.setArchive();    // ensure that it gets re-archived if it is deleted

		// Add the alarm to the displayed lists and to the calendar file
		theApp()->undeleteEvent(event, this);
		item = listView->updateEntry(item, event, true);
		listView->setSelected(item, true);
	}
}

/******************************************************************************
*  Called when the Show Alarm Times menu item is selected or deselected.
*/
void KAlarmMainWindow::slotShowTime()
{
	mShowTime = !mShowTime;
	mViewMenu->setItemChecked(mShowTimeId, mShowTime);
	if (!mShowTime  &&  !mShowTimeTo)
	{
		// At least one time column must be displayed
		mShowTimeTo = true;
		mViewMenu->setItemChecked(mShowTimeToId, mShowTimeTo);
	}
	listView->selectTimeColumns(mShowTime, mShowTimeTo);
}

/******************************************************************************
*  Called when the Show Time To Alarms menu item is selected or deselected.
*/
void KAlarmMainWindow::slotShowTimeTo()
{
	mShowTimeTo = !mShowTimeTo;
	mViewMenu->setItemChecked(mShowTimeToId, mShowTimeTo);
	if (!mShowTimeTo  &&  !mShowTime)
	{
		// At least one time column must be displayed
		mShowTime = true;
		mViewMenu->setItemChecked(mShowTimeId, mShowTime);
	}
	listView->selectTimeColumns(mShowTime, mShowTimeTo);
	setUpdateTimer();
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
*  Called when the Import Birthdays menu item is selected, to display birthdays
*  from the address book for selection as alarms.
*/
void KAlarmMainWindow::slotBirthdays()
{
	BirthdayDlg* dlg = new BirthdayDlg(this);
	if (dlg->exec() == QDialog::Accepted)
	{
		QValueList<KAlarmEvent> events = dlg->events();
		listView->clearSelection();
		for (QValueList<KAlarmEvent>::Iterator ev = events.begin();  ev != events.end();  ++ev)
		{
			// Add the alarm to the displayed lists and to the calendar file
			theApp()->addEvent(*ev, this);
			AlarmListViewItem* item = listView->addEntry(*ev, true);
			listView->setSelected(item, true);
		}
	}
	delete dlg;
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
	if (isTrayParent())
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
	if (!theApp()->sessionClosingDown()  &&  isTrayParent())
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
	if (!listView->selectedCount())
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
*  Called when the drag cursor enters the window.
*/
void KAlarmMainWindow::dragEnterEvent(QDragEnterEvent* e)
{
	executeDragEnterEvent(e);
}

/******************************************************************************
*  Called when the drag cursor enters a main or system tray window, to accept
*  or reject the dragged object.
*/
void KAlarmMainWindow::executeDragEnterEvent(QDragEnterEvent* e)
{
	e->accept(QTextDrag::canDecode(e)
	       || KURLDrag::canDecode(e)
	       || KPIM::MailListDrag::canDecode(e));
}

/******************************************************************************
*  Called when an object is dropped on the window.
*  If the object is recognised, the edit alarm dialog is opened appropriately.
*/
void KAlarmMainWindow::dropEvent(QDropEvent* e)
{
	executeDropEvent(this, e);
}

/******************************************************************************
*  Called when an object is dropped on a main or system tray window, to
*  evaluate the action required and extract the text.
*/
void KAlarmMainWindow::executeDropEvent(KAlarmMainWindow* win, QDropEvent* e)
{
	KAlarmEvent::Action action = KAlarmEvent::MESSAGE;
	QString text;
	KPIM::MailList mailList;
	KURL::List files;
	if (KURLDrag::decode(e, files)  &&  files.count())
	{
		action = KAlarmEvent::FILE;
		text = files.first().prettyURL();
	}
	else if (e->provides(KPIM::MailListDrag::format())
	&&  KPIM::MailListDrag::decode(e, mailList))
	{
		// KMail message(s). Ignore all but the first.
		if (!mailList.count())
			return;
		KPIM::MailSummary& summary = mailList.first();
		text = messageFromPrefix + '\t';
		text += summary.from();
		text += '\n';
		text += messageToPrefix + '\t';
		text += summary.to();
		text += '\n';
		text += messageDatePrefix + '\t';
		QDateTime dt;
		dt.setTime_t(summary.date());
		text += KGlobal::locale()->formatDateTime(dt);
		text += '\n';
		text += messageSubjectPrefix + '\t';
		text += summary.subject();
	}
	else if (QTextDrag::decode(e, text))
	{
	}
	else
		return;
	if (!text.isEmpty())
		executeNew(win, action, text);
}

/******************************************************************************
*  Check whether a text is an email, and if so return its subject line.
*  Reply = subject line, or QString::null if not the text of an email.
*/
QString KAlarmMainWindow::emailSubject(const QString& text)
{
	QStringList lines = QStringList::split('\n', text);
	if (lines.count() >= 4
	&&  lines[0].startsWith(messageFromPrefix)
	&&  lines[1].startsWith(messageToPrefix)
	&&  lines[2].startsWith(messageDatePrefix)
	&&  lines[3].startsWith(messageSubjectPrefix))
		return lines[3].mid(messageSubjectPrefix.length());
	return QString::null;
}

/******************************************************************************
*  Called when the selected items in the ListView changes.
*  Selects the new current item, and enables the actions appropriately.
*/
void KAlarmMainWindow::slotSelection()
{
	// Find which item has been selected, and whether more than one is selected
	QPtrList<AlarmListViewItem> items = listView->selectedItems();
	int count = items.count();
	AlarmListViewItem* item = (count == 1) ? items.first() : 0;
	bool allExpired = true;
	for (QPtrListIterator<AlarmListViewItem> it(items);  it.current();  ++it)
	{
		if (!listView->expired(it.current()))
			allExpired = false;
	}

	kdDebug(5950) << "KAlarmMainWindow::slotSelection(true)\n";
	actionCopy->setEnabled(count == 1);
	actionModify->setEnabled(item && !listView->expired(item));
	actionView->setEnabled(count == 1);
	actionDelete->setEnabled(count);
	actionUndelete->setEnabled(count && allExpired);
}

/******************************************************************************
*  Called when the mouse is clicked on the ListView.
*  Deselects the current item and disables the actions if appropriate, or
*  displays a context menu to modify or delete the selected item.
*/
void KAlarmMainWindow::slotMouseClicked(int button, QListViewItem* item, const QPoint& pt, int)
{
	if (button == Qt::RightButton)
	{
		kdDebug(5950) << "KAlarmMainWindow::slotMouseClicked(right)\n";
		QPopupMenu* menu = new QPopupMenu(this, "ListContextMenu");
		if (item)
		{
			actionCopy->plug(menu);
			actionModify->plug(menu);
			actionView->plug(menu);
			actionDelete->plug(menu);
			if (mShowExpired)
				actionUndelete->plug(menu);
		}
		else
			actionNew->plug(menu);
		menu->exec(pt);
	}
	else if (!item)
	{
		kdDebug(5950) << "KAlarmMainWindow::slotMouseClicked(left)\n";
		listView->clearSelection();
		actionCopy->setEnabled(false);
		actionModify->setEnabled(false);
		actionView->setEnabled(false);
		actionDelete->setEnabled(false);
		actionUndelete->setEnabled(false);
	}
}

/******************************************************************************
*  Called when the mouse is double clicked on the ListView.
*  Displays the Edit Alarm dialog, for the clicked item if applicable.
*/
void KAlarmMainWindow::slotDoubleClicked(QListViewItem* item)
{
	kdDebug(5950) << "KAlarmMainWindow::slotDoubleClicked()\n";
	if (item)
	{
		if (listView->expired((AlarmListViewItem*)item))
			slotView();
		else
			slotModify();
	}
	else
		slotNew();
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
