/*
 *  mainwindow.cpp  -  main application window
 *  Program:  kalarm
 *  (C) 2001 - 2005 by David Jarvie <software@astrojar.org.uk>
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
#include <kactionclasses.h>
#include <kstdaction.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kurldrag.h>
#include <klocale.h>
#include <kglobalsettings.h>
#include <kconfig.h>
#include <kkeydialog.h>
#include <kedittoolbar.h>
#include <kaboutdata.h>
#include <dcopclient.h>
#include <kdebug.h>
#include <kstdguiitem.h>

#include <libkdepim/maillistdrag.h>
#include <libkmime/kmime_content.h>
#include <libkcal/calendarlocal.h>
#include <libkcal/icaldrag.h>

#include "alarmcalendar.h"
#include "alarmevent.h"
#include "alarmlistview.h"
#include "alarmtext.h"
#include "birthdaydlg.h"
#include "daemon.h"
#include "editdlg.h"
#include "functions.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "prefdlg.h"
#include "preferences.h"
#include "synchtimer.h"
#include "templatepickdlg.h"
#include "templatedlg.h"
#include "traywindow.h"
#include "mainwindow.moc"

using namespace KCal;

static const char* UI_FILE = "kalarmui.rc";
QString   undoText;
QString   undoTextStripped;
QString   undoIcon;
KShortcut undoShortcut;
QString   redoText;
QString   redoTextStripped;
QString   redoIcon;
KShortcut redoShortcut;


/*=============================================================================
=  Class: MainWindow
=============================================================================*/

QPtrList<MainWindow> MainWindow::mWindowList;
TemplateDlg*         MainWindow::mTemplateDlg = 0;

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString MainWindow::i18n_a_ShowAlarmTimes()    { return i18n("Show &Alarm Times"); }
QString MainWindow::i18n_t_ShowAlarmTime()     { return i18n("Show alarm &time"); }
QString MainWindow::i18n_m_ShowAlarmTime()     { return i18n("Show alarm ti&me"); }
QString MainWindow::i18n_o_ShowTimeToAlarms()  { return i18n("Show Time t&o Alarms"); }
QString MainWindow::i18n_n_ShowTimeToAlarm()   { return i18n("Show time u&ntil alarm"); }
QString MainWindow::i18n_l_ShowTimeToAlarm()   { return i18n("Show time unti&l alarm"); }
QString MainWindow::i18n_ShowExpiredAlarms()   { return i18n("Show Expired Alarms"); }
QString MainWindow::i18n_e_ShowExpiredAlarms() { return i18n("Show &Expired Alarms"); }
QString MainWindow::i18n_HideExpiredAlarms()   { return i18n("Hide Expired Alarms"); }
QString MainWindow::i18n_e_HideExpiredAlarms() { return i18n("Hide &Expired Alarms"); }


/******************************************************************************
* Construct an instance.
* To avoid resize() events occurring while still opening the calendar (and
* resultant crashes), the calendar is opened before constructing the instance.
*/
MainWindow* MainWindow::create(bool restored)
{
	theApp()->checkCalendarDaemon();    // ensure calendar is open and daemon started
	return new MainWindow(restored);
}

MainWindow::MainWindow(bool restored)
	: MainWindowBase(0, 0, WGroupLeader | WStyle_ContextHelp | WDestructiveClose),
	  mMinuteTimerActive(false),
	  mHiddenTrayParent(false),
	  mShowExpired(Preferences::instance()->showExpiredAlarms()),
	  mShowTime(Preferences::instance()->showAlarmTime()),
	  mShowTimeTo(Preferences::instance()->showTimeToAlarm())
{
	kdDebug(5950) << "MainWindow::MainWindow()\n";
	setAutoSaveSettings(QString::fromLatin1("MainWindow"));    // save window sizes etc.
	setPlainCaption(kapp->aboutData()->programName());
	if (!restored)
	{
		QSize s;
		if (KAlarm::readConfigWindowSize("MainWindow", s))
			resize(s);
	}

	setAcceptDrops(true);         // allow drag-and-drop onto this window
	if (!mShowTimeTo)
		mShowTime = true;     // ensure at least one time column is visible
	mListView = new AlarmListView(this, "listView");
	mListView->selectTimeColumns(mShowTime, mShowTimeTo);
	mListView->showExpired(mShowExpired);
	setCentralWidget(mListView);
	mListView->refresh();          // populate the alarm list
	mListView->clearSelection();

	connect(mListView, SIGNAL(itemDeleted()), SLOT(slotDeletion()));
	connect(mListView, SIGNAL(selectionChanged()), SLOT(slotSelection()));
	connect(mListView, SIGNAL(mouseButtonClicked(int, QListViewItem*, const QPoint&, int)),
	        SLOT(slotMouseClicked(int, QListViewItem*, const QPoint&, int)));
	connect(mListView, SIGNAL(executed(QListViewItem*)), SLOT(slotDoubleClicked(QListViewItem*)));
	initActions();

	mWindowList.append(this);
	if (mWindowList.count() == 1  &&  Daemon::isDcopHandlerReady())
	{
		// It's the first main window, and the DCOP handler is ready
		if (theApp()->wantRunInSystemTray())
			theApp()->displayTrayIcon(true, this);     // create system tray icon for run-in-system-tray mode
		else if (theApp()->trayWindow())
			theApp()->trayWindow()->setAssocMainWindow(this);    // associate this window with the system tray icon
	}
	setUpdateTimer();
}

MainWindow::~MainWindow()
{
	kdDebug(5950) << "MainWindow::~MainWindow()\n";
	if (findWindow(this))
		mWindowList.remove();
	if (theApp()->trayWindow())
	{
		if (isTrayParent())
			delete theApp()->trayWindow();
		else
			theApp()->trayWindow()->removeWindow(this);
	}
	MinuteTimer::disconnect(this);
	mMinuteTimerActive = false;    // to ensure that setUpdateTimer() works correctly
	setUpdateTimer();
	MainWindow* main = mainMainWindow();
	if (main)
		KAlarm::writeConfigWindowSize("MainWindow", main->size());
	KGlobal::config()->sync();    // save any new window size to disc
	theApp()->quitIf();
}

/******************************************************************************
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
void MainWindow::saveProperties(KConfig* config)
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
void MainWindow::readProperties(KConfig* config)
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
MainWindow* MainWindow::mainMainWindow()
{
	MainWindow* tray = theApp()->trayWindow() ? theApp()->trayWindow()->assocMainWindow() : 0;
	if (tray  &&  tray->isVisible())
		return tray;
	for (MainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
		if (w->isVisible())
			return w;
	if (tray)
		return tray;
	return mWindowList.first();
}

/******************************************************************************
* Check whether this main window is the parent of the system tray icon.
*/
bool MainWindow::isTrayParent() const
{
	return theApp()->wantRunInSystemTray()  &&  theApp()->trayMainWindow() == this;
}

/******************************************************************************
*  Close all main windows.
*/
void MainWindow::closeAll()
{
	while (mWindowList.first())
		delete mWindowList.first();
}

/******************************************************************************
*  Called when the window's size has changed (before it is painted).
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*  Records the new size in the config file.
*/
void MainWindow::resizeEvent(QResizeEvent* re)
{
	// Save the window's new size only if it's the first main window
	if (mainMainWindow() == this)
		KAlarm::writeConfigWindowSize("MainWindow", re->size());
	MainWindowBase::resizeEvent(re);
}

/******************************************************************************
*  Called when the window is first displayed.
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void MainWindow::showEvent(QShowEvent* se)
{
	setUpdateTimer();
	slotUpdateTimeTo();
	MainWindowBase::showEvent(se);
}

/******************************************************************************
*  Display the window.
*/
void MainWindow::show()
{
	MainWindowBase::show();
	if (mMenuError)
	{
		// Show error message now that the main window has been displayed.
		// Waiting until now lets the user easily associate the message with
		// the main window which is faulty.
		KMessageBox::error(this, i18n("Failure to create menus\n(perhaps %1 missing or corrupted)").arg(QString::fromLatin1(UI_FILE)));
		mMenuError = false;
	}
}

/******************************************************************************
*  Called after the window is hidden.
*/
void MainWindow::hideEvent(QHideEvent* he)
{
	setUpdateTimer();
	MainWindowBase::hideEvent(he);
}

/******************************************************************************
*  Initialise the menu, toolbar and main window actions.
*/
void MainWindow::initActions()
{
	KActionCollection* actions = actionCollection();
	mActionTemplates       = new KAction(i18n("&Templates..."), 0, this, SLOT(slotTemplates()), actions, "templates");
	mActionNew             = KAlarm::createNewAlarmAction(i18n("&New..."), this, SLOT(slotNew()), actions, "new");
	mActionNewFromTemplate = KAlarm::createNewFromTemplateAction(i18n("New &From Template"), this, SLOT(slotNewFromTemplate(const KAEvent&)), actions, "newFromTempl");
	mActionCreateTemplate  = new KAction(i18n("Create Tem&plate..."), 0, this, SLOT(slotNewTemplate()), actions, "createTemplate");
	mActionCopy            = new KAction(i18n("&Copy..."), "editcopy", Qt::SHIFT+Qt::Key_Insert, this, SLOT(slotCopy()), actions, "copy");
	mActionModify          = new KAction(i18n("&Edit..."), "edit", Qt::CTRL+Qt::Key_E, this, SLOT(slotModify()), actions, "modify");
	mActionDelete          = new KAction(i18n("&Delete"), "editdelete", Qt::Key_Delete, this, SLOT(slotDelete()), actions, "delete");
	mActionReactivate      = new KAction(i18n("Reac&tivate"), 0, Qt::CTRL+Qt::Key_R, this, SLOT(slotReactivate()), actions, "undelete");
	mActionEnable          = new KAction(QString::null, 0, Qt::CTRL+Qt::Key_B, this, SLOT(slotEnable()), actions, "disable");
	mActionView            = new KAction(i18n("&View"), "viewmag", Qt::CTRL+Qt::Key_W, this, SLOT(slotView()), actions, "view");
	mActionShowTime        = new KToggleAction(i18n_a_ShowAlarmTimes(), Qt::CTRL+Qt::Key_M, this, SLOT(slotShowTime()), actions, "showAlarmTimes");
	mActionShowTime->setCheckedState(i18n("Hide &Alarm Times"));
	mActionShowTimeTo      = new KToggleAction(i18n_o_ShowTimeToAlarms(), Qt::CTRL+Qt::Key_I, this, SLOT(slotShowTimeTo()), actions, "showTimeToAlarms");
	mActionShowTimeTo->setCheckedState(i18n("Hide Time t&o Alarms"));
	mActionShowExpired     = new KToggleAction(i18n_e_ShowExpiredAlarms(), "history", Qt::CTRL+Qt::Key_P, this, SLOT(slotShowExpired()), actions, "showExpiredAlarms");
	mActionShowExpired->setCheckedState(i18n_e_HideExpiredAlarms());
	mActionToggleTrayIcon  = new KToggleAction(i18n("Show in System &Tray"), Qt::CTRL+Qt::Key_Y, this, SLOT(slotToggleTrayIcon()), actions, "showInSystemTray");
	mActionToggleTrayIcon->setCheckedState(i18n("Hide From System &Tray"));
	new KAction(i18n("Import &Birthdays..."), 0, this, SLOT(slotBirthdays()), actions, "importBirthdays");
	new KAction(i18n("&Refresh Alarms"), "reload", 0, this, SLOT(slotResetDaemon()), actions, "refreshAlarms");
	Daemon::createAlarmEnableAction(actions, "alarmEnable");
	if (undoText.isNull())
	{
		// Get standard texts, etc., for Undo and Redo actions
		KAction* act = KStdAction::undo(this, 0, actions);
		undoIcon         = act->icon();
		undoShortcut     = act->shortcut();
		undoText         = act->text();
		undoTextStripped = KAlarm::stripAccel(undoText);
		delete act;
		act = KStdAction::redo(this, 0, actions);
		redoIcon         = act->icon();
		redoShortcut     = act->shortcut();
		redoText         = act->text();
		redoTextStripped = KAlarm::stripAccel(redoText);
		delete act;
	}
	mActionUndo = new KToolBarPopupAction(undoText, undoIcon, undoShortcut, this, SLOT(slotUndo()), actions, "edit_undo");
	mActionRedo = new KToolBarPopupAction(redoText, redoIcon, redoShortcut, this, SLOT(slotRedo()), actions, "edit_redo");
	KStdAction::find(mListView, SLOT(slotFind()), actions);
	mActionFindNext = KStdAction::findNext(mListView, SLOT(slotFindNext()), actions);
	mActionFindPrev = KStdAction::findPrev(mListView, SLOT(slotFindPrev()), actions);
	KStdAction::quit(this, SLOT(slotQuit()), actions);
	KStdAction::keyBindings(this, SLOT(slotConfigureKeys()), actions);
	KStdAction::configureToolbars(this, SLOT(slotConfigureToolbar()), actions);
	KStdAction::preferences(this, SLOT(slotPreferences()), actions);
	setStandardToolBarMenuEnabled(true);
	createGUI(UI_FILE);

	mContextMenu = static_cast<KPopupMenu*>(factory()->container("listContext", this));
	mActionsMenu = static_cast<KPopupMenu*>(factory()->container("actions", this));
	mMenuError = (!mContextMenu  ||  !mActionsMenu);
	connect(mActionsMenu, SIGNAL(aboutToShow()), SLOT(updateActionsMenu()));
	connect(mActionUndo->popupMenu(), SIGNAL(aboutToShow()), SLOT(slotInitUndoMenu()));
	connect(mActionUndo->popupMenu(), SIGNAL(activated(int)), SLOT(slotUndoItem(int)));
	connect(mActionRedo->popupMenu(), SIGNAL(aboutToShow()), SLOT(slotInitRedoMenu()));
	connect(mActionRedo->popupMenu(), SIGNAL(activated(int)), SLOT(slotRedoItem(int)));
	connect(Undo::instance(), SIGNAL(changed(const QString&, const QString&)), SLOT(slotUndoStatus(const QString&, const QString&)));
	connect(mListView, SIGNAL(findActive(bool)), SLOT(slotFindActive(bool)));
	connect(Preferences::instance(), SIGNAL(preferencesChanged()), SLOT(updateTrayIconAction()));
	connect(theApp(), SIGNAL(trayIconToggled()), SLOT(updateTrayIconAction()));

	// Set menu item states
	setEnableText(true);
	mActionShowTime->setChecked(mShowTime);
	mActionShowTimeTo->setChecked(mShowTimeTo);
	mActionShowExpired->setChecked(mShowExpired);
	if (!Preferences::instance()->expiredKeepDays())
		mActionShowExpired->setEnabled(false);
	updateTrayIconAction();         // set the correct text for this action
	mActionUndo->setEnabled(Undo::haveUndo());
	mActionRedo->setEnabled(Undo::haveRedo());
	mActionFindNext->setEnabled(false);
	mActionFindPrev->setEnabled(false);

	mActionCopy->setEnabled(false);
	mActionModify->setEnabled(false);
	mActionDelete->setEnabled(false);
	mActionReactivate->setEnabled(false);
	mActionView->setEnabled(false);
	mActionEnable->setEnabled(false);
	mActionCreateTemplate->setEnabled(false);

	Undo::emitChanged();     // set the Undo/Redo menu texts
	Daemon::checkStatus();
	Daemon::monitoringAlarms();
}

/******************************************************************************
* Enable or disable the Templates menu item in every main window instance.
*/
void MainWindow::enableTemplateMenuItem(bool enable)
{
	for (MainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
		w->mActionTemplates->setEnabled(enable);
}

/******************************************************************************
* Refresh the alarm list in every main window instance.
*/
void MainWindow::refresh()
{
	kdDebug(5950) << "MainWindow::refresh()\n";
	for (MainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
		w->mListView->refresh();
}

/******************************************************************************
* Refresh the alarm list in every main window instance which is displaying
* expired alarms.
* Called when an expired alarm setting changes in the user preferences.
*/
void MainWindow::updateExpired()
{
	kdDebug(5950) << "MainWindow::updateExpired()\n";
	bool enableShowExpired = Preferences::instance()->expiredKeepDays();
	for (MainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
	{
		if (w->mShowExpired)
		{
			if (!enableShowExpired)
				w->slotShowExpired();
			else
				w->mListView->refresh();
		}
		w->mActionShowExpired->setEnabled(enableShowExpired);
	}
}

/******************************************************************************
* Called when the show-alarm-time or show-time-to-alarm setting changes in the
* user preferences.
* Update the alarm list in every main window instance to show the new default
* columns. No change is made if a window isn't using the old settings.
*/
void MainWindow::updateTimeColumns(bool oldTime, bool oldTimeTo)
{
	kdDebug(5950) << "MainWindow::updateShowAlarmTimes()\n";
	bool newTime   = Preferences::instance()->showAlarmTime();
	bool newTimeTo = Preferences::instance()->showTimeToAlarm();
	if (!newTime  &&  !newTimeTo)
		newTime = true;     // at least one time column must be displayed
	if (!oldTime  &&  !oldTimeTo)
		oldTime = true;     // at least one time column must have been displayed
	if (newTime != oldTime  ||  newTimeTo != oldTimeTo)
	{
		for (MainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
		{
			if (w->mShowTime   == oldTime
			&&  w->mShowTimeTo == oldTimeTo)
			{
				w->mShowTime   = newTime;
				w->mShowTimeTo = newTimeTo;
				w->mActionShowTime->setChecked(newTime);
				w->mActionShowTimeTo->setChecked(newTimeTo);
				w->mListView->selectTimeColumns(newTime, newTimeTo);
			}
		}
		setUpdateTimer();
	}
}

/******************************************************************************
* Start or stop the timer which updates the time-to-alarm values every minute.
* Should be called whenever a main window is created or destroyed, or shown or
* hidden.
*/
void MainWindow::setUpdateTimer()
{
	// Check whether any windows need to be updated
	MainWindow* needTimer = 0;
	MainWindow* timerWindow = 0;
	for (MainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
	{
		if (w->isVisible()  &&  w->mListView->showingTimeTo())
			needTimer = w;
		if (w->mMinuteTimerActive)
			timerWindow = w;
	}

	// Start or stop the update timer if necessary
	if (needTimer  &&  !timerWindow)
	{
		// Timeout every minute.
		needTimer->mMinuteTimerActive = true;
		MinuteTimer::connect(needTimer, SLOT(slotUpdateTimeTo()));
		kdDebug(5950) << "MainWindow::setUpdateTimer(): started timer" << endl;
	}
	else if (!needTimer  &&  timerWindow)
	{
		timerWindow->mMinuteTimerActive = false;
		MinuteTimer::disconnect(timerWindow);
		kdDebug(5950) << "MainWindow::setUpdateTimer(): stopped timer" << endl;
	}
}
/******************************************************************************
* Update the time-to-alarm values for each main window which is displaying them.
*/
void MainWindow::slotUpdateTimeTo()
{
	kdDebug(5950) << "MainWindow::slotUpdateTimeTo()" << endl;
	for (MainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
		if (w->isVisible()  &&  w->mListView->showingTimeTo())
			w->mListView->updateTimeToAlarms();
}

/******************************************************************************
* Select an alarm in the displayed list.
*/
void MainWindow::selectEvent(const QString& eventID)
{
	mListView->clearSelection();
	AlarmListViewItem* item = mListView->getEntry(eventID);
	if (item)
	{
		mListView->setSelected(item, true);
		mListView->setCurrentItem(item);
		mListView->ensureItemVisible(item);
	}
}

/******************************************************************************
*  Called when the New button is clicked to edit a new alarm to add to the list.
*/
void MainWindow::slotNew()
{
	executeNew(this);
}

/******************************************************************************
*  Execute a New Alarm dialog, optionally either presetting it to the supplied
*  event, or setting the action and text.
*/
void MainWindow::executeNew(MainWindow* win, const KAEvent* evnt, KAEvent::Action action, const AlarmText& text)
{
	EditAlarmDlg editDlg(false, i18n("New Alarm"), win, "editDlg", evnt);
	if (!text.isEmpty())
		editDlg.setAction(action, text);
	if (editDlg.exec() == QDialog::Accepted)
	{
		KAEvent event;
		editDlg.getEvent(event);

		// Add the alarm to the displayed lists and to the calendar file
		KAlarm::addEvent(event, (win ? win->mListView : 0));
		Undo::saveAdd(event);

		alarmWarnings(&editDlg, &event);
	}
}

/******************************************************************************
*  Called when a template is selected from the New From Template popup menu.
*  Executes a New Alarm dialog, preset from the selected template.
*/
void MainWindow::slotNewFromTemplate(const KAEvent& tmplate)
{
	executeNew(this, &tmplate);
}

/******************************************************************************
*  Called when the New Template button is clicked to create a new template
*  based on the currently selected alarm.
*/
void MainWindow::slotNewTemplate()
{
	AlarmListViewItem* item = mListView->selectedItem();
	if (item)
	{
		KAEvent event = item->event();
		TemplateDlg::createTemplate(&event, this);
	}
}

/******************************************************************************
*  Called when the Copy button is clicked to edit a copy of an existing alarm,
*  to add to the list.
*/
void MainWindow::slotCopy()
{
	AlarmListViewItem* item = mListView->selectedItem();
	if (item)
		executeNew(this, &item->event());
}

/******************************************************************************
*  Called when the Modify button is clicked to edit the currently highlighted
*  alarm in the list.
*/
void MainWindow::slotModify()
{
	AlarmListViewItem* item = mListView->selectedItem();
	if (item)
	{
		KAEvent event = item->event();
		EditAlarmDlg editDlg(false, i18n("Edit Alarm"), this, "editDlg", &event);
		if (editDlg.exec() == QDialog::Accepted)
		{
			KAEvent newEvent;
			bool changeDeferral = !editDlg.getEvent(newEvent);

			// Update the event in the displays and in the calendar file
			if (changeDeferral)
				KAlarm::updateEvent(newEvent, mListView, true, false);   // keep the same event ID
			else
				KAlarm::modifyEvent(event, newEvent, mListView);
			Undo::saveEdit(event, newEvent);

			alarmWarnings(&editDlg, &newEvent);
		}
	}
}

/******************************************************************************
*  Called when the View button is clicked to view the currently highlighted
*  alarm in the list.
*/
void MainWindow::slotView()
{
	AlarmListViewItem* item = mListView->selectedItem();
	if (item)
	{
		KAEvent event = item->event();
		EditAlarmDlg editDlg(false, (event.expired() ? i18n("Expired Alarm") : i18n("View Alarm")),
		                     this, "editDlg", &event, true);
		editDlg.exec();
	}
}

/******************************************************************************
*  Called when the Delete button is clicked to delete the currently highlighted
*  alarms in the list.
*/
void MainWindow::slotDelete()
{
	QValueList<EventListViewItemBase*> items = mListView->selectedItems();
	if (Preferences::instance()->confirmAlarmDeletion())
	{
		int n = items.count();
		if (KMessageBox::warningContinueCancel(this, i18n("Do you really want to delete the selected alarm?",
		                                                  "Do you really want to delete the %n selected alarms?", n),
		                                       i18n("Delete Alarm", "Delete Alarms", n),
		                                       KGuiItem(i18n("&Delete"), "editdelete"),
		                                       Preferences::CONFIRM_ALARM_DELETION)
		    != KMessageBox::Continue)
			return;
	}

	QValueList<KAEvent> events;
	AlarmCalendar::activeCalendar()->startUpdate();    // prevent multiple saves of the calendars until we're finished
	AlarmCalendar::expiredCalendar()->startUpdate();
	for (QValueList<EventListViewItemBase*>::Iterator it = items.begin();  it != items.end();  ++it)
	{
		AlarmListViewItem* item = (AlarmListViewItem*)(*it);
		KAEvent event = item->event();

		// Delete the event from the calendar and displays
		events.append(event);
		KAlarm::deleteEvent(event);
	}
	AlarmCalendar::activeCalendar()->endUpdate();      // save the calendars now
	AlarmCalendar::expiredCalendar()->endUpdate();
	Undo::saveDeletes(events);
}

/******************************************************************************
*  Called when the Reactivate button is clicked to reinstate the currently
*  highlighted expired alarms in the list.
*/
void MainWindow::slotReactivate()
{
	QValueList<KAEvent> events;
	QValueList<EventListViewItemBase*> items = mListView->selectedItems();
	mListView->clearSelection();
	AlarmCalendar::activeCalendar()->startUpdate();    // prevent multiple saves of the calendars until we're finished
	AlarmCalendar::expiredCalendar()->startUpdate();
	for (QValueList<EventListViewItemBase*>::Iterator it = items.begin();  it != items.end();  ++it)
	{
		// Add the alarm to the displayed lists and to the calendar file
		AlarmListViewItem* item = (AlarmListViewItem*)(*it);
		KAEvent event = item->event();
		events.append(event);
		KAlarm::reactivateEvent(event, mListView, true);
	}
	AlarmCalendar::activeCalendar()->endUpdate();      // save the calendars now
	AlarmCalendar::expiredCalendar()->endUpdate();
	Undo::saveReactivates(events);
}

/******************************************************************************
*  Called when the Enable/Disable button is clicked to enable or disable the
*  currently highlighted alarms in the list.
*/
void MainWindow::slotEnable()
{
	bool enable = mActionEnableEnable;    // save since changed in response to KAlarm::enableEvent()
	QValueList<EventListViewItemBase*> items = mListView->selectedItems();
	AlarmCalendar::activeCalendar()->startUpdate();    // prevent multiple saves of the calendars until we're finished
	for (QValueList<EventListViewItemBase*>::Iterator it = items.begin();  it != items.end();  ++it)
	{
		AlarmListViewItem* item = (AlarmListViewItem*)(*it);
		KAEvent event = item->event();

		// Enable the alarm in the displayed lists and in the calendar file
		KAlarm::enableEvent(event, mListView, enable);
	}
	AlarmCalendar::activeCalendar()->endUpdate();      // save the calendars now
}

/******************************************************************************
*  Called when the Show Alarm Times menu item is selected or deselected.
*/
void MainWindow::slotShowTime()
{
	mShowTime = !mShowTime;
	mActionShowTime->setChecked(mShowTime);
	if (!mShowTime  &&  !mShowTimeTo)
		slotShowTimeTo();    // at least one time column must be displayed
	else
		mListView->selectTimeColumns(mShowTime, mShowTimeTo);
}

/******************************************************************************
*  Called when the Show Time To Alarms menu item is selected or deselected.
*/
void MainWindow::slotShowTimeTo()
{
	mShowTimeTo = !mShowTimeTo;
	mActionShowTimeTo->setChecked(mShowTimeTo);
	if (!mShowTimeTo  &&  !mShowTime)
		slotShowTime();    // at least one time column must be displayed
	else
		mListView->selectTimeColumns(mShowTime, mShowTimeTo);
	setUpdateTimer();
}

/******************************************************************************
*  Called when the Show Expired Alarms menu item is selected or deselected.
*/
void MainWindow::slotShowExpired()
{
	mShowExpired = !mShowExpired;
	mActionShowExpired->setChecked(mShowExpired);
	mActionShowExpired->setToolTip(mShowExpired ? i18n_HideExpiredAlarms() : i18n_ShowExpiredAlarms());
	mListView->showExpired(mShowExpired);
	mListView->refresh();
}

/******************************************************************************
*  Called when the Import Birthdays menu item is selected, to display birthdays
*  from the address book for selection as alarms.
*/
void MainWindow::slotBirthdays()
{
	BirthdayDlg dlg(this);
	if (dlg.exec() == QDialog::Accepted)
	{
		QValueList<KAEvent> events = dlg.events();
		if (events.count())
		{
			mListView->clearSelection();
			for (QValueList<KAEvent>::Iterator ev = events.begin();  ev != events.end();  ++ev)
				KAlarm::addEvent(*ev, mListView);    // add alarm to the displayed lists and to the calendar file
			alarmWarnings(&dlg);
		}
	}
}

/******************************************************************************
*  Called when the Templates menu item is selected, to display the alarm
*  template editing dialogue.
*/
void MainWindow::slotTemplates()
{
	if (!mTemplateDlg)
	{
		mTemplateDlg = TemplateDlg::create(this);
		enableTemplateMenuItem(false);     // disable menu item in all windows
		connect(mTemplateDlg, SIGNAL(finished()), SLOT(slotTemplatesEnd()));
		mTemplateDlg->show();
	}
}

/******************************************************************************
*  Called when the alarm template editing dialogue has exited.
*/
void MainWindow::slotTemplatesEnd()
{
	if (mTemplateDlg)
	{
		mTemplateDlg->delayedDestruct();   // this deletes the dialogue once it is safe to do so
		mTemplateDlg = 0;
		enableTemplateMenuItem(true);      // re-enable menu item in all windows
	}
}

/******************************************************************************
*  Called when the Display System Tray Icon menu item is selected.
*/
void MainWindow::slotToggleTrayIcon()
{
	theApp()->displayTrayIcon(!theApp()->trayIconDisplayed(), this);
}

/******************************************************************************
* Called when the system tray icon is created or destroyed.
* Set the system tray icon menu text according to whether or not the system
* tray icon is currently visible.
*/
void MainWindow::updateTrayIconAction()
{
	mActionToggleTrayIcon->setEnabled(theApp()->KDEDesktop() && !theApp()->wantRunInSystemTray());
	mActionToggleTrayIcon->setChecked(theApp()->trayIconDisplayed());
}

/******************************************************************************
* Called when the Actions menu is about to be displayed.
* Update the status of the Alarms Enabled menu item.
*/
void MainWindow::updateActionsMenu()
{
	Daemon::checkStatus();   // update the Alarms Enabled item status
}

/******************************************************************************
*  Called when the active status of Find changes.
*/
void MainWindow::slotFindActive(bool active)
{
	mActionFindNext->setEnabled(active);
	mActionFindPrev->setEnabled(active);
}

/******************************************************************************
*  Called when the Undo action is selected.
*/
void MainWindow::slotUndo()
{
	QString action = KAlarm::stripAccel(mActionUndo->text());    // save menu text - it will change before undo() returns
	QString err = Undo::undo();
	if (!err.isNull())
		KMessageBox::sorry(this, i18n("Undo-action: message", "%1: %2").arg(action).arg(err));
}

/******************************************************************************
*  Called when the Redo action is selected.
*/
void MainWindow::slotRedo()
{
	QString action = KAlarm::stripAccel(mActionRedo->text());    // save menu text - it will change before redo() returns
	QString err = Undo::redo();
	if (!err.isNull())
		KMessageBox::sorry(this, i18n("Undo-action: message", "%1: %2").arg(action).arg(err));
}

/******************************************************************************
*  Called when an Undo item is selected.
*/
void MainWindow::slotUndoItem(int id)
{
	QString action = Undo::actionText(Undo::UNDO, id);    // save menu text - it will change before undo() returns
	QString err = Undo::undo(id);
	if (!err.isNull())
		KMessageBox::sorry(this, i18n("Undo-action: message", "%1: %2").arg(action).arg(err));
}

/******************************************************************************
*  Called when a Redo item is selected.
*/
void MainWindow::slotRedoItem(int id)
{
	QString action = Undo::actionText(Undo::REDO, id);    // save menu text - it will change before redo() returns
	QString err = Undo::redo(id);
	if (!err.isNull())
		KMessageBox::sorry(this, i18n("Undo-action: message", "%1: %2").arg(action).arg(err));
}

/******************************************************************************
*  Called when the Undo menu is about to show.
*  Populates the menu.
*/
void MainWindow::slotInitUndoMenu()
{
	initUndoMenu(mActionUndo->popupMenu(), Undo::UNDO);
}

/******************************************************************************
*  Called when the Redo menu is about to show.
*  Populates the menu.
*/
void MainWindow::slotInitRedoMenu()
{
	initUndoMenu(mActionRedo->popupMenu(), Undo::REDO);
}

/******************************************************************************
*  Populate the undo or redo menu.
*/
void MainWindow::initUndoMenu(KPopupMenu* menu, Undo::Type type)
{
	menu->clear();
	const QString& action = (type == Undo::UNDO) ? undoTextStripped : redoTextStripped;
	QValueList<int> ids = Undo::ids(type);
	for (QValueList<int>::ConstIterator it = ids.begin();  it != ids.end();  ++it)
	{
		int id = *it;
		menu->insertItem(i18n("Undo [action]: message", "%1 %2: %3")
		                       .arg(action).arg(Undo::actionText(type, id)).arg(Undo::description(type, id)), id);
	}
}

/******************************************************************************
*  Called when the status of the Undo or Redo list changes.
*  Change the Undo or Redo text to include the action which would be undone/redone.
*/
void MainWindow::slotUndoStatus(const QString& undo, const QString& redo)
{
	if (undo.isNull())
	{
		mActionUndo->setEnabled(false);
		mActionUndo->setText(undoText);
	}
	else
	{
		mActionUndo->setEnabled(true);
		mActionUndo->setText(QString("%1 %2").arg(undoText).arg(undo));
	}
	if (redo.isNull())
	{
		mActionRedo->setEnabled(false);
		mActionRedo->setText(redoText);
	}
	else
	{
		mActionRedo->setEnabled(true);
		mActionRedo->setText(QString("%1 %2").arg(redoText).arg(redo));
	}
}

/******************************************************************************
*  Called when the Reset Daemon menu item is selected.
*/
void MainWindow::slotResetDaemon()
{
	KAlarm::resetDaemon();
}

/******************************************************************************
*  Called when the "Configure KAlarm" menu item is selected.
*/
void MainWindow::slotPreferences()
{
	KAlarmPrefDlg prefDlg;
	prefDlg.exec();
}

/******************************************************************************
*  Called when the Configure Keys menu item is selected.
*/
void MainWindow::slotConfigureKeys()
{
	KKeyDialog::configure(actionCollection(), this);
}

/******************************************************************************
*  Called when the Configure Toolbars menu item is selected.
*/
void MainWindow::slotConfigureToolbar()
{
	saveMainWindowSettings(KGlobal::config(), "MainWindow");
	KEditToolbar dlg(factory());
	dlg.exec();
}

/******************************************************************************
*  Called when the Quit menu item is selected.
*/
void MainWindow::slotQuit()
{
	theApp()->doQuit(this);
}

/******************************************************************************
*  Called when the user or the session manager attempts to close the window.
*/
void MainWindow::closeEvent(QCloseEvent* ce)
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
void MainWindow::slotDeletion()
{
	if (!mListView->selectedCount())
	{
		kdDebug(5950) << "MainWindow::slotDeletion(true)\n";
		mActionCreateTemplate->setEnabled(false);
		mActionCopy->setEnabled(false);
		mActionModify->setEnabled(false);
		mActionView->setEnabled(false);
		mActionDelete->setEnabled(false);
		mActionReactivate->setEnabled(false);
		mActionEnable->setEnabled(false);
	}
}

/******************************************************************************
*  Called when the drag cursor enters the window.
*/
void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
	executeDragEnterEvent(e);
}

/******************************************************************************
*  Called when the drag cursor enters a main or system tray window, to accept
*  or reject the dragged object.
*/
void MainWindow::executeDragEnterEvent(QDragEnterEvent* e)
{
	if (KCal::ICalDrag::canDecode(e))
		e->accept(!AlarmListView::dragging());   // don't accept "text/calendar" objects from KAlarm
	else
		e->accept(QTextDrag::canDecode(e)
		       || KURLDrag::canDecode(e)
		       || KPIM::MailListDrag::canDecode(e));
}

/******************************************************************************
*  Called when an object is dropped on the window.
*  If the object is recognised, the edit alarm dialog is opened appropriately.
*/
void MainWindow::dropEvent(QDropEvent* e)
{
	executeDropEvent(this, e);
}

static QString getMailHeader(const char* header, KMime::Content& content)
{
	KMime::Headers::Base* hd = content.getHeaderByType(header);
	return hd ? hd->asUnicodeString() : QString::null;
}

/******************************************************************************
*  Called when an object is dropped on a main or system tray window, to
*  evaluate the action required and extract the text.
*/
void MainWindow::executeDropEvent(MainWindow* win, QDropEvent* e)
{
	KAEvent::Action action = KAEvent::MESSAGE;
	QString        text;
	QByteArray     bytes;
	AlarmText      alarmText;
	KPIM::MailList mailList;
	KURL::List     files;
	KCal::CalendarLocal calendar(QString::fromLatin1("UTC"));
	calendar.setLocalTime();    // default to local time (i.e. no time zone)

	if (KURLDrag::decode(e, files)  &&  files.count())
	{
		kdDebug(5950) << "MainWindow::executeDropEvent(URL)" << endl;
		action = KAEvent::FILE;
		alarmText.setText(files.first().prettyURL());
	}
	else if (e->provides("message/rfc822")
	&&       !(bytes = e->encodedData("message/rfc822")).isEmpty())
	{
		// Email message(s). Ignore all but the first.
		kdDebug(5950) << "MainWindow::executeDropEvent(email)" << endl;
		QCString mails(bytes.data(), bytes.size());
		KMime::Content content;
		content.setContent(mails);
		content.parse();
		QString body;
		content.textContent()->decodedText(body, true, true);    // strip trailing newlines & spaces
		alarmText.setEmail(getMailHeader("To", content),
		                   getMailHeader("From", content),
		                   getMailHeader("Cc", content),
		                   getMailHeader("Date", content),
		                   getMailHeader("Subject", content),
				   body);
	}
	else if (e->provides(KPIM::MailListDrag::format())
	&&       KPIM::MailListDrag::decode(e, mailList))
	{
		// KMail message(s). Ignore all but the first.
		kdDebug(5950) << "MainWindow::executeDropEvent(KMail_list)" << endl;
		if (!mailList.count())
			return;
		KPIM::MailSummary& summary = mailList.first();
		QDateTime dt;
		dt.setTime_t(summary.date());
		QString body = KAMail::getMailBody(summary.serialNumber());
		alarmText.setEmail(summary.to(), summary.from(), QString::null,
		                   KGlobal::locale()->formatDateTime(dt), summary.subject(), body);
	}
	else if (KCal::ICalDrag::decode(e, &calendar))
	{
		// iCalendar - ignore all but the first event
		kdDebug(5950) << "MainWindow::executeDropEvent(iCalendar)" << endl;
		KCal::Event::List events = calendar.rawEvents();
		if (!events.isEmpty())
		{
			KAEvent ev(*events.first());
			executeNew(win, &ev);
		}
		return;
	}
	else if (QTextDrag::decode(e, text))
	{
		kdDebug(5950) << "MainWindow::executeDropEvent(text)" << endl;
		alarmText.setText(text);
	}
	else
		return;

	if (!alarmText.isEmpty())
		executeNew(win, 0, action, alarmText);
}

/******************************************************************************
*  Called when the selected items in the ListView changes.
*  Selects the new current item, and enables the actions appropriately.
*/
void MainWindow::slotSelection()
{
	// Find which item has been selected, and whether more than one is selected
	QValueList<EventListViewItemBase*> items = mListView->selectedItems();
	int count = items.count();
	AlarmListViewItem* item = (AlarmListViewItem*)((count == 1) ? items.first() : 0);
	bool enableReactivate = true;
	bool enableEnableDisable = true;
	bool enableEnable = false;
	bool enableDisable = false;
	QDateTime now = QDateTime::currentDateTime();
	for (QValueList<EventListViewItemBase*>::Iterator it = items.begin();  it != items.end();  ++it)
	{
		const KAEvent& event = ((AlarmListViewItem*)(*it))->event();
		if (enableReactivate
		&&  (!event.expired()  ||  !event.occursAfter(now, true)))
			enableReactivate = false;
		if (enableEnableDisable)
		{
			if (event.expired())
				enableEnableDisable = enableEnable = enableDisable = false;
			else
			{
				if (!enableEnable  &&  !event.enabled())
					enableEnable = true;
				if (!enableDisable  &&  event.enabled())
					enableDisable = true;
			}
		}
	}

	kdDebug(5950) << "MainWindow::slotSelection(true)\n";
	mActionCreateTemplate->setEnabled(count == 1);
	mActionCopy->setEnabled(count == 1);
	mActionModify->setEnabled(item && !mListView->expired(item));
	mActionView->setEnabled(count == 1);
	mActionDelete->setEnabled(count);
	mActionReactivate->setEnabled(count && enableReactivate);
	mActionEnable->setEnabled(enableEnable || enableDisable);
	if (enableEnable || enableDisable)
		setEnableText(enableEnable);
}

/******************************************************************************
*  Called when the mouse is clicked on the ListView.
*  Deselects the current item and disables the actions if appropriate, or
*  displays a context menu to modify or delete the selected item.
*/
void MainWindow::slotMouseClicked(int button, QListViewItem* item, const QPoint& pt, int)
{
	if (button == Qt::RightButton)
	{
		kdDebug(5950) << "MainWindow::slotMouseClicked(right)\n";
		if (mContextMenu)
			mContextMenu->popup(pt);
	}
	else if (!item)
	{
		kdDebug(5950) << "MainWindow::slotMouseClicked(left)\n";
		mListView->clearSelection();
		mActionCreateTemplate->setEnabled(false);
		mActionCopy->setEnabled(false);
		mActionModify->setEnabled(false);
		mActionView->setEnabled(false);
		mActionDelete->setEnabled(false);
		mActionReactivate->setEnabled(false);
		mActionEnable->setEnabled(false);
	}
}

/******************************************************************************
*  Called when the mouse is double clicked on the ListView.
*  Displays the Edit Alarm dialog, for the clicked item if applicable.
*/
void MainWindow::slotDoubleClicked(QListViewItem* item)
{
	kdDebug(5950) << "MainWindow::slotDoubleClicked()\n";
	if (item)
	{
		if (mListView->expired((AlarmListViewItem*)item))
			slotView();
		else
			slotModify();
	}
	else
		slotNew();
}

/******************************************************************************
*  Set the text of the Enable/Disable menu action.
*/
void MainWindow::setEnableText(bool enable)
{
	mActionEnableEnable = enable;
	mActionEnable->setText(enable ? i18n("Ena&ble") : i18n("Disa&ble"));
}

/******************************************************************************
* Prompt the user to re-enable alarms if they are currently disabled, and if
* it's an email alarm, warn if no 'From' email address is configured.
*/
void MainWindow::alarmWarnings(QWidget* parent, const KAEvent* event)
{
	if (event  &&  event->action() == KAEvent::EMAIL
	&&  Preferences::instance()->emailAddress().isEmpty())
		KMessageBox::information(parent, i18n("Please set the 'From' email address...",
		                                      "%1\nPlease set it in the Preferences dialog.").arg(KAMail::i18n_NeedFromEmailAddress()));

	if (!Daemon::monitoringAlarms())
	{
		if (KMessageBox::warningYesNo(parent, i18n("Alarms are currently disabled.\nDo you want to enable alarms now?"),
		                              QString::null, KStdGuiItem::yes(), KStdGuiItem::no(),
		                              QString::fromLatin1("EditEnableAlarms"))
		                == KMessageBox::Yes)
			Daemon::setAlarmsEnabled();
	}
}

/******************************************************************************
*  Display or hide the specified main window.
*  This should only be called when the application doesn't run in the system tray.
*/
MainWindow* MainWindow::toggleWindow(MainWindow* win)
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
	win = create();
	win->show();
	return win;
}

/******************************************************************************
* Find the specified window in the main window list.
*/
bool MainWindow::findWindow(MainWindow* win)
{
	for (MainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
		if (w == win)
			return true;
	return false;
}
