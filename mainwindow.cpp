/*
 *  mainwindow.cpp  -  main application window
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <djarvie@kde.org>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kalarm.h"

#include <qiconset.h>
#include <qdragobject.h>
#include <qheader.h>

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

static const char* UI_FILE     = "kalarmui.rc";
static const char* WINDOW_NAME = "MainWindow";

static const QString VIEW_GROUP         = QString::fromLatin1("View");
static const QString SHOW_TIME_KEY      = QString::fromLatin1("ShowAlarmTime");
static const QString SHOW_TIME_TO_KEY   = QString::fromLatin1("ShowTimeToAlarm");
static const QString SHOW_ARCHIVED_KEY  = QString::fromLatin1("ShowArchivedAlarms");
static const QString SHOW_RESOURCES_KEY = QString::fromLatin1("ShowResources");

static QString   undoText;
static QString   undoTextStripped;
static QString   undoIcon;
static KShortcut undoShortcut;
static QString   redoText;
static QString   redoTextStripped;
static QString   redoIcon;
static KShortcut redoShortcut;


/*=============================================================================
=  Class: MainWindow
=============================================================================*/

MainWindow::WindowList   MainWindow::mWindowList;
TemplateDlg*             MainWindow::mTemplateDlg = 0;

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString MainWindow::i18n_a_ShowAlarmTimes()    { return i18n("Show &Alarm Times"); }
QString MainWindow::i18n_m_ShowAlarmTime()     { return i18n("Show alarm ti&me"); }
QString MainWindow::i18n_o_ShowTimeToAlarms()  { return i18n("Show Time t&o Alarms"); }
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
	: MainWindowBase(0, "MainWin", WGroupLeader | WStyle_ContextHelp | WDestructiveClose),
	  mMinuteTimerActive(false),
	  mHiddenTrayParent(false)
{
	kdDebug(5950) << "MainWindow::MainWindow()\n";
	setAutoSaveSettings(QString::fromLatin1(WINDOW_NAME));    // save window sizes etc.
	setPlainCaption(kapp->aboutData()->programName());
	KConfig* config = KGlobal::config();
	config->setGroup(VIEW_GROUP);
	mShowExpired = config->readBoolEntry(SHOW_ARCHIVED_KEY, false);
	mShowTime    = config->readBoolEntry(SHOW_TIME_KEY, true);
	mShowTimeTo  = config->readBoolEntry(SHOW_TIME_TO_KEY, false);
	if (!restored)
	{
		QSize s;
		if (KAlarm::readConfigWindowSize(WINDOW_NAME, s))
			resize(s);
	}
	config->setGroup(QString::fromLatin1(WINDOW_NAME));
	QValueList<int> order = config->readIntListEntry(QString::fromLatin1("ColumnOrder"));

	setAcceptDrops(true);         // allow drag-and-drop onto this window
	if (!mShowTimeTo)
		mShowTime = true;     // ensure at least one time column is visible
	mListView = new AlarmListView(order, this, "listView");
	mListView->selectTimeColumns(mShowTime, mShowTimeTo);
	mListView->showExpired(mShowExpired);
	setCentralWidget(mListView);
	mListView->refresh();          // populate the alarm list
	mListView->clearSelection();

	connect(mListView, SIGNAL(itemDeleted()), SLOT(slotDeletion()));
	connect(mListView, SIGNAL(selectionChanged()), SLOT(slotSelection()));
	connect(mListView, SIGNAL(contextMenuRequested(QListViewItem*, const QPoint&, int)),
	        SLOT(slotContextMenuRequested(QListViewItem*, const QPoint&, int)));
	connect(mListView, SIGNAL(mouseButtonClicked(int, QListViewItem*, const QPoint&, int)),
	        SLOT(slotMouseClicked(int, QListViewItem*, const QPoint&, int)));
	connect(mListView, SIGNAL(executed(QListViewItem*)), SLOT(slotDoubleClicked(QListViewItem*)));
	connect(mListView->header(), SIGNAL(indexChange(int, int, int)), SLOT(columnsReordered()));
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
	mWindowList.remove(this);
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
		KAlarm::writeConfigWindowSize(WINDOW_NAME, main->size());
	KToolBar* tb = toolBar();
	if (tb)
		tb->saveSettings(KGlobal::config(), "Toolbars");
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
	for (WindowList::Iterator it = mWindowList.begin();  it != mWindowList.end();  ++it)
		if ((*it)->isVisible())
			return *it;
	if (tray)
		return tray;
	if (mWindowList.isEmpty())
		return 0;
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
	while (!mWindowList.isEmpty())
		delete mWindowList.first();    // N.B. the destructor removes the window from the list
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
		KAlarm::writeConfigWindowSize(WINDOW_NAME, re->size());
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
*  Called when the list's column order is changed.
*  Save the new column order as the default the next time the program is run.
*/
void MainWindow::columnsReordered()
{
	KConfig* config = KGlobal::config();
	config->setGroup(QString::fromLatin1(WINDOW_NAME));
	config->writeEntry(QString::fromLatin1("ColumnOrder"), mListView->columnOrder());
	config->sync();
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
	mActionToggleTrayIcon  = new KToggleAction(i18n("Show in System &Tray"), 0, this, SLOT(slotToggleTrayIcon()), actions, "showInSystemTray");
	mActionToggleTrayIcon->setCheckedState(i18n("Hide From System &Tray"));
	new KAction(i18n("Import &Alarms..."), 0, this, SLOT(slotImportAlarms()), actions, "importAlarms");
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
	KStdAction::selectAll(mListView, SLOT(slotSelectAll()), actions);
	KStdAction::deselect(mListView, SLOT(slotDeselect()), actions);
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
	Preferences::connect(SIGNAL(preferencesChanged()), this, SLOT(slotPrefsChanged()));
	connect(theApp(), SIGNAL(trayIconToggled()), SLOT(updateTrayIconAction()));

	// Set menu item states
	setEnableText(true);
	mActionShowTime->setChecked(mShowTime);
	mActionShowTimeTo->setChecked(mShowTimeTo);
	mActionShowExpired->setChecked(mShowExpired);
	slotPrefsChanged();         // set the correct text for this action
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

	KToolBar* tb = toolBar();
	if (tb)
		tb->applySettings(KGlobal::config(), "Toolbars");

	Undo::emitChanged();     // set the Undo/Redo menu texts
	Daemon::checkStatus();
	Daemon::monitoringAlarms();
}

/******************************************************************************
* Enable or disable the Templates menu item in every main window instance.
*/
void MainWindow::enableTemplateMenuItem(bool enable)
{
	for (WindowList::Iterator it = mWindowList.begin();  it != mWindowList.end();  ++it)
		(*it)->mActionTemplates->setEnabled(enable);
}

/******************************************************************************
* Refresh the alarm list in every main window instance.
*/
void MainWindow::refresh()
{
	kdDebug(5950) << "MainWindow::refresh()\n";
	for (WindowList::Iterator it = mWindowList.begin();  it != mWindowList.end();  ++it)
		(*it)->mListView->refresh();
}

/******************************************************************************
* Refresh the alarm list in every main window instance which is displaying
* expired alarms.
* Called when an expired alarm setting changes in the user preferences.
*/
void MainWindow::updateExpired()
{
	kdDebug(5950) << "MainWindow::updateExpired()\n";
	bool enableShowExpired = Preferences::expiredKeepDays();
	for (WindowList::Iterator it = mWindowList.begin();  it != mWindowList.end();  ++it)
	{
		MainWindow* w = *it;
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
* Start or stop the timer which updates the time-to-alarm values every minute.
* Should be called whenever a main window is created or destroyed, or shown or
* hidden.
*/
void MainWindow::setUpdateTimer()
{
	// Check whether any windows need to be updated
	MainWindow* needTimer = 0;
	MainWindow* timerWindow = 0;
	for (WindowList::Iterator it = mWindowList.begin();  it != mWindowList.end();  ++it)
	{
		MainWindow* w = *it;
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
	for (WindowList::Iterator it = mWindowList.begin();  it != mWindowList.end();  ++it)
	{
		MainWindow* w = *it;
		if (w->isVisible()  &&  w->mListView->showingTimeTo())
			w->mListView->updateTimeToAlarms();
	}
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
	EditAlarmDlg editDlg(false, i18n("New Alarm"), win, 0, evnt);
	if (!text.isEmpty())
		editDlg.setAction(action, text);
	if (editDlg.exec() == QDialog::Accepted)
	{
		KAEvent event;
		editDlg.getEvent(event);

		// Add the alarm to the displayed lists and to the calendar file
		if (KAlarm::addEvent(event, (win ? win->mListView : 0), &editDlg) == KAlarm::UPDATE_KORG_ERR)
			KAlarm::displayKOrgUpdateError(&editDlg, KAlarm::KORG_ERR_ADD, 1);
		Undo::saveAdd(event);

		KAlarm::outputAlarmWarnings(&editDlg, &event);
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
		executeEdit(event, this);
	}
}

/******************************************************************************
*  Open the Edit Alarm dialogue to edit the specified alarm.
*/
void MainWindow::executeEdit(KAEvent& event, MainWindow* win)
{
	EditAlarmDlg editDlg(false, i18n("Edit Alarm"), win, 0, &event);
	if (editDlg.exec() == QDialog::Accepted)
	{
		KAEvent newEvent;
		bool changeDeferral = !editDlg.getEvent(newEvent);

		// Update the event in the displays and in the calendar file
		AlarmListView* view = win ? win->mListView : 0;
		if (changeDeferral)
		{
			// The only change has been to an existing deferral
			if (KAlarm::updateEvent(newEvent, view, &editDlg, true, false) != KAlarm::UPDATE_OK)   // keep the same event ID
				return;   // failed to save event
		}
		else
		{
			if (KAlarm::modifyEvent(event, newEvent, view, &editDlg) == KAlarm::UPDATE_KORG_ERR)
				KAlarm::displayKOrgUpdateError(&editDlg, KAlarm::KORG_ERR_MODIFY, 1);
		}
		Undo::saveEdit(event, newEvent);

		KAlarm::outputAlarmWarnings(&editDlg, &newEvent);
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
		EditAlarmDlg editDlg(false, (event.expired() ? i18n("Expired Alarm") + " [" + i18n("read-only") + ']'
		                                             : i18n("View Alarm")),
		                     this, 0, &event, true);
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
	// Copy the events to be deleted, in case any are deleted by being
	// triggered while the confirmation prompt is displayed.
	QValueList<KAEvent> events;
	QValueList<KAEvent> origEvents;
	for (QValueList<EventListViewItemBase*>::Iterator iit = items.begin();  iit != items.end();  ++iit)
	{
		AlarmListViewItem* item = (AlarmListViewItem*)(*iit);
		events.append(item->event());
		origEvents.append(item->event());
	}
	if (Preferences::confirmAlarmDeletion())
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

	int warnErr = 0;
	int warnKOrg = 0;
	AlarmCalendar::activeCalendar()->startUpdate();    // prevent multiple saves of the calendars until we're finished
	AlarmCalendar::expiredCalendar()->startUpdate();
	for (QValueList<KAEvent>::Iterator it = events.begin();  it != events.end();  ++it)
	{
		// Delete the event from the calendar and displays
		switch (KAlarm::deleteEvent(*it))
		{
			case KAlarm::UPDATE_ERROR:
			case KAlarm::UPDATE_FAILED:
			case KAlarm::SAVE_FAILED:
				++warnErr;
				break;
			case KAlarm::UPDATE_KORG_ERR:
				++warnKOrg;
				break;
			default:
				break;
		}
	}
	if (!AlarmCalendar::activeCalendar()->endUpdate())      // save the calendars now
		warnErr = events.count();
	AlarmCalendar::expiredCalendar()->endUpdate();
	Undo::saveDeletes(origEvents);

	if (warnErr)
		KAlarm::displayUpdateError(this, KAlarm::UPDATE_FAILED, KAlarm::ERR_DELETE, warnErr);
	else if (warnKOrg)
		KAlarm::displayKOrgUpdateError(this, KAlarm::KORG_ERR_DELETE, warnKOrg);
}

/******************************************************************************
*  Called when the Reactivate button is clicked to reinstate the currently
*  highlighted expired alarms in the list.
*/
void MainWindow::slotReactivate()
{
	int warnErr = 0;
	int warnKOrg = 0;
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
		switch (KAlarm::reactivateEvent(event, mListView, true))
		{
			case KAlarm::UPDATE_ERROR:
			case KAlarm::UPDATE_FAILED:
			case KAlarm::SAVE_FAILED:
				++warnErr;
				break;
			case KAlarm::UPDATE_KORG_ERR:
				++warnKOrg;
				break;
			default:
				break;
		}
	}
	if (!AlarmCalendar::activeCalendar()->endUpdate())      // save the calendars now
		warnErr = items.count();
	AlarmCalendar::expiredCalendar()->endUpdate();
	Undo::saveReactivates(events);

	if (warnErr)
		KAlarm::displayUpdateError(this, KAlarm::UPDATE_FAILED, KAlarm::ERR_REACTIVATE, warnErr);
	else if (warnKOrg)
		KAlarm::displayKOrgUpdateError(this, KAlarm::KORG_ERR_ADD, warnKOrg);
}

/******************************************************************************
*  Called when the Enable/Disable button is clicked to enable or disable the
*  currently highlighted alarms in the list.
*/
void MainWindow::slotEnable()
{
	bool enable = mActionEnableEnable;    // save since changed in response to KAlarm::enableEvent()
	int warnErr = 0;
	QValueList<EventListViewItemBase*> items = mListView->selectedItems();
	AlarmCalendar::activeCalendar()->startUpdate();    // prevent multiple saves of the calendars until we're finished
	for (QValueList<EventListViewItemBase*>::Iterator it = items.begin();  it != items.end();  ++it)
	{
		AlarmListViewItem* item = (AlarmListViewItem*)(*it);
		KAEvent event = item->event();

		// Enable the alarm in the displayed lists and in the calendar file
		if (KAlarm::enableEvent(event, mListView, enable) != KAlarm::UPDATE_OK)
			++warnErr;
	}
	if (!AlarmCalendar::activeCalendar()->endUpdate())      // save the calendars now
		warnErr = items.count();
	if (warnErr)
		KAlarm::displayUpdateError(this, KAlarm::UPDATE_FAILED, KAlarm::ERR_ADD, warnErr);
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
	{
		mListView->selectTimeColumns(mShowTime, mShowTimeTo);
		KConfig* config = KGlobal::config();
		config->setGroup(VIEW_GROUP);
		config->writeEntry(SHOW_TIME_KEY, mShowTime);
		config->writeEntry(SHOW_TIME_TO_KEY, mShowTimeTo);
	}
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
	{
		mListView->selectTimeColumns(mShowTime, mShowTimeTo);
		KConfig* config = KGlobal::config();
		config->setGroup(VIEW_GROUP);
		config->writeEntry(SHOW_TIME_KEY, mShowTime);
		config->writeEntry(SHOW_TIME_TO_KEY, mShowTimeTo);
	}
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
	KConfig* config = KGlobal::config();
	config->setGroup(VIEW_GROUP);
	config->writeEntry(SHOW_ARCHIVED_KEY, mShowExpired);
}

/******************************************************************************
*  Called when the Import Alarms menu item is selected, to merge alarms from an
*  external calendar into the current calendars.
*/
void MainWindow::slotImportAlarms()
{
	if (AlarmCalendar::importAlarms(this))
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
			int warnErr = 0;
			int warnKOrg = 0;
			for (QValueList<KAEvent>::Iterator ev = events.begin();  ev != events.end();  ++ev)
			{
				// Add alarm to the displayed lists and to the calendar file
				switch (KAlarm::addEvent(*ev, mListView))
				{
					case KAlarm::UPDATE_ERROR:
					case KAlarm::UPDATE_FAILED:
					case KAlarm::SAVE_FAILED:
						++warnErr;
						break;
					case KAlarm::UPDATE_KORG_ERR:
						++warnKOrg;
						break;
					default:
						break;
				}
			}
			if (warnErr)
				KAlarm::displayUpdateError(this, KAlarm::UPDATE_FAILED, KAlarm::ERR_ADD, warnErr);
			else if (warnKOrg)
				KAlarm::displayKOrgUpdateError(this, KAlarm::KORG_ERR_ADD, warnKOrg);
			KAlarm::outputAlarmWarnings(&dlg);
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
* Called when the user preferences have changed.
*/
void MainWindow::slotPrefsChanged()
{
	mActionShowExpired->setEnabled(Preferences::expiredKeepDays());
	updateTrayIconAction();
}

/******************************************************************************
* Called when the system tray icon is created or destroyed.
* Set the system tray icon menu text according to whether or not the system
* tray icon is currently visible.
*/
void MainWindow::updateTrayIconAction()
{
	mActionToggleTrayIcon->setEnabled(theApp()->haveSystemTray() && !theApp()->wantRunInSystemTray());
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
	Undo::undo(this, KAlarm::stripAccel(mActionUndo->text()));
}

/******************************************************************************
*  Called when the Redo action is selected.
*/
void MainWindow::slotRedo()
{
	Undo::redo(this, KAlarm::stripAccel(mActionRedo->text()));
}

/******************************************************************************
*  Called when an Undo item is selected.
*/
void MainWindow::slotUndoItem(int id)
{
	Undo::undo(id, this, Undo::actionText(Undo::UNDO, id));
}

/******************************************************************************
*  Called when a Redo item is selected.
*/
void MainWindow::slotRedoItem(int id)
{
	Undo::redo(id, this, Undo::actionText(Undo::REDO, id));
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
		QString actText = Undo::actionText(type, id);
		QString descrip = Undo::description(type, id);
		QString text = descrip.isEmpty()
		             ? i18n("Undo/Redo [action]", "%1 %2").arg(action).arg(actText)
		             : i18n("Undo [action]: message", "%1 %2: %3").arg(action).arg(actText).arg(descrip);
		menu->insertItem(text, id);
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
	KAlarmPrefDlg::display();
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
	saveMainWindowSettings(KGlobal::config(), WINDOW_NAME);
	KEditToolbar dlg(factory());
	connect(&dlg, SIGNAL(newToolbarConfig()), this, SLOT(slotNewToolbarConfig()));
	dlg.exec();
}

/******************************************************************************
*  Called when OK or Apply is clicked in the Configure Toolbars dialog, to save
*  the new configuration.
*/
void MainWindow::slotNewToolbarConfig()
{
	createGUI(UI_FILE);
	applyMainWindowSettings(KGlobal::config(), WINDOW_NAME);
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
#ifndef NDEBUG
	QCString fmts;
	for (int idbg = 0;  e->format(idbg);  ++idbg)
	{
		if (idbg) fmts += ", ";
		fmts += e->format(idbg);
	}
	kdDebug(5950) << "MainWindow::executeDropEvent(): " << fmts << endl;
#endif

	/* The order of the tests below matters, since some dropped objects
	 * provide more than one mime type.
	 * Don't change them without careful thought !!
	 */
	if (e->provides("message/rfc822")
	&&  !(bytes = e->encodedData("message/rfc822")).isEmpty())
	{
		// Email message(s). Ignore all but the first.
		kdDebug(5950) << "MainWindow::executeDropEvent(email)" << endl;
		QCString mails(bytes.data(), bytes.size());
		KMime::Content content;
		content.setContent(mails);
		content.parse();
		QString body;
		if (content.textContent())
			content.textContent()->decodedText(body, true, true);    // strip trailing newlines & spaces
		unsigned long sernum = 0;
		if (e->provides(KPIM::MailListDrag::format())
		&&  KPIM::MailListDrag::decode(e, mailList)
		&&  mailList.count())
		{
			// Get its KMail serial number to allow the KMail message
			// to be called up from the alarm message window.
			sernum = mailList.first().serialNumber();
		}
		alarmText.setEmail(getMailHeader("To", content),
		                   getMailHeader("From", content),
		                   getMailHeader("Cc", content),
		                   getMailHeader("Date", content),
		                   getMailHeader("Subject", content),
				   body, sernum);
	}
	else if (KURLDrag::decode(e, files)  &&  files.count())
	{
		kdDebug(5950) << "MainWindow::executeDropEvent(URL)" << endl;
		action = KAEvent::FILE;
		alarmText.setText(files.first().prettyURL());
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
		                   KGlobal::locale()->formatDateTime(dt), summary.subject(),
		                   body, summary.serialNumber());
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
* Called when a context menu is requested either by a mouse click or by a
* key press.
*/
void MainWindow::slotContextMenuRequested(QListViewItem* item, const QPoint& pt, int)
{
	kdDebug(5950) << "MainWindow::slotContextMenuRequested()" << endl;
	if (mContextMenu)
		mContextMenu->popup(pt);
}

/******************************************************************************
* Called when the mouse is clicked on the ListView.
* Deselects the current item and disables the actions if appropriate.
* Note that if a right button click is handled by slotContextMenuRequested().
*/
void MainWindow::slotMouseClicked(int button, QListViewItem* item, const QPoint& pt, int)
{
	if (button != Qt::RightButton  &&  !item)
	{
		kdDebug(5950) << "MainWindow::slotMouseClicked(left)" << endl;
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
*  Display or hide the specified main window.
*  This should only be called when the application doesn't run in the system tray.
*/
MainWindow* MainWindow::toggleWindow(MainWindow* win)
{
	if (win  &&  mWindowList.find(win) != mWindowList.end())
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
