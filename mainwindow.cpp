/*
 *  mainwindow.cpp  -  main application window
 *  Program:  kalarm
 *  Copyright © 2001-2006 by David Jarvie <software@astrojar.org.uk>
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

#include <QByteArray>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QSplitter>
#include <q3header.h>

#include <kmenubar.h>
#include <ktoolbar.h>
#include <kmenu.h>
#include <kaction.h>
#include <kactioncollection.h>

#include <kstandardaction.h>
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kurl.h>
#include <klocale.h>
#include <kglobalsettings.h>
#include <kconfig.h>
#include <kkeydialog.h>
#include <kedittoolbar.h>
#include <kxmlguifactory.h>
#include <kaboutdata.h>
#include <kdebug.h>
#include <ktoggleaction.h>
#include <ktoolbarpopupaction.h>
#include <libkdepim/maillistdrag.h>
#include <kmime/kmime_content.h>
#include <kcal/calendarlocal.h>
#include <kcal/icaldrag.h>
#include <kicon.h>

#include "alarmcalendar.h"
#include "alarmevent.h"
#include "alarmlistview.h"
#include "alarmresources.h"
#include "alarmtext.h"
#include "birthdaydlg.h"
#include "daemon.h"
#include "editdlg.h"
#include "functions.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "prefdlg.h"
#include "preferences.h"
#include "resourceselector.h"
#include "synchtimer.h"
#include "templatedlg.h"
#include "templatemenuaction.h"
#include "templatepickdlg.h"
#include "traywindow.h"
#include "mainwindow.moc"

using namespace KCal;

static const char* UI_FILE     = "kalarmui.rc";
static const char* WINDOW_NAME = "MainWindow";

static QString   undoText;
static QString   undoTextStripped;
static QIcon     undoIcon;
static KShortcut undoShortcut;
static QString   redoText;
static QString   redoTextStripped;
static QIcon     redoIcon;
static KShortcut redoShortcut;


/*=============================================================================
=  Class: MainWindow
=============================================================================*/

MainWindow::WindowList   MainWindow::mWindowList;
TemplateDlg*             MainWindow::mTemplateDlg = 0;

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString MainWindow::i18n_a_ShowAlarmTimes()     { return i18n("Show &Alarm Times"); }
QString MainWindow::i18n_t_ShowAlarmTime()      { return i18n("Show alarm &time"); }
QString MainWindow::i18n_m_ShowAlarmTime()      { return i18n("Show alarm ti&me"); }
QString MainWindow::i18n_o_ShowTimeToAlarms()   { return i18n("Show Time t&o Alarms"); }
QString MainWindow::i18n_n_ShowTimeToAlarm()    { return i18n("Show time u&ntil alarm"); }
QString MainWindow::i18n_l_ShowTimeToAlarm()    { return i18n("Show time unti&l alarm"); }
QString MainWindow::i18n_ShowArchivedAlarms()   { return i18n("Show Archived Alarms"); }
QString MainWindow::i18n_e_ShowArchivedAlarms() { return i18n("Show &Archived Alarms"); }
QString MainWindow::i18n_HideArchivedAlarms()   { return i18n("Hide Archived Alarms"); }
QString MainWindow::i18n_e_HideArchivedAlarms() { return i18n("Hide &Archived Alarms"); }
QString MainWindow::i18n_r_ShowResources()      { return i18n("Show &Resources");}


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
	: MainWindowBase(0, Qt::WindowContextHelpButtonHint),
	  mResourcesWidth(-1),
	  mMinuteTimerActive(false),
	  mHiddenTrayParent(false),
	  mShowResources(Preferences::showResources()),
	  mShowArchived(Preferences::showArchivedAlarms()),
	  mShowTime(Preferences::showAlarmTime()),
	  mShowTimeTo(Preferences::showTimeToAlarm())
{
	kDebug(5950) << "MainWindow::MainWindow()\n";
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowModality(Qt::WindowModal);
	setObjectName("MainWin");    // used by LikeBack
	setAutoSaveSettings(QLatin1String(WINDOW_NAME));    // save window sizes etc.
	setPlainCaption(kapp->aboutData()->programName());
	if (!restored)
	{
		QSize s;
		if (KAlarm::readConfigWindowSize(WINDOW_NAME, s, &mResourcesWidth))
			resize(s);
	}
	KConfig* config = KGlobal::config();
	config->setGroup(QString::fromLatin1(WINDOW_NAME));
	QList<int> order = config->readEntry("ColumnOrder", QList<int>());

	setAcceptDrops(true);         // allow drag-and-drop onto this window
	if (!mShowTimeTo)
		mShowTime = true;     // ensure at least one time column is visible

	mSplitter = new QSplitter(Qt::Horizontal, this);
	mSplitter->setChildrenCollapsible(false);
	setCentralWidget(mSplitter);

	// Create the calendar resource selector widget
	AlarmResources* resources = AlarmResources::instance();
	mResourceSelector = new ResourceSelector(resources, mSplitter);
	mSplitter->setStretchFactor(0, 0);   // don't resize resource selector when window is resized
	connect(resources, SIGNAL(signalErrorMessage(const QString&)), SLOT(showErrorMessage(const QString&)));

	// Create the alarm list widget
	mListView = new AlarmListView(order, mSplitter);
	mListView->selectTimeColumns(mShowTime, mShowTimeTo);
	mListView->showArchived(mShowArchived);
	mListView->refresh();          // populate the alarm list
	mListView->clearSelection();

	connect(mListView, SIGNAL(itemDeleted()), SLOT(slotDeletion()));
	connect(mListView, SIGNAL(selectionChanged()), SLOT(slotSelection()));
	connect(mListView, SIGNAL(mouseButtonClicked(int, Q3ListViewItem*, const QPoint&, int)),
	                   SLOT(slotMouseClicked(int, Q3ListViewItem*, const QPoint&, int)));
	connect(mListView, SIGNAL(executed(Q3ListViewItem*)), SLOT(slotDoubleClicked(Q3ListViewItem*)));
	connect(mListView->header(), SIGNAL(indexChange(int, int, int)), SLOT(columnsReordered()));
	connect(mResourceSelector, SIGNAL(resized(const QSize&, const QSize&)), SLOT(resourcesResized()));
	connect(mResourceSelector, SIGNAL(resourcesChanged()), mListView, SLOT(refresh()));
	connect(resources, SIGNAL(calendarChanged()), mListView, SLOT(refresh()));
	connect(resources, SIGNAL(resourceStatusChanged(AlarmResource*, AlarmResources::Change)),
	                   SLOT(slotResourceStatusChanged(AlarmResource*, AlarmResources::Change)));
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
	kDebug(5950) << "MainWindow::~MainWindow()\n";
	mWindowList.removeAt(mWindowList.indexOf(this));
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
		KAlarm::writeConfigWindowSize(WINDOW_NAME, main->size(), mResourcesWidth);
	KGlobal::config()->sync();    // save any new window size to disc
	KToolBar* tb = toolBar();
	if (tb)
		tb->saveSettings(KGlobal::config(), "Toolbars");
	theApp()->quitIf();
}

/******************************************************************************
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
void MainWindow::saveProperties(KConfig* config)
{
	config->writeEntry("HiddenTrayParent", isTrayParent() && isHidden());
	config->writeEntry("ShowArchived", mShowArchived);
	config->writeEntry("ShowTime", mShowTime);
	config->writeEntry("ShowTimeTo", mShowTimeTo);
	config->writeEntry("ResourcesWidth", mResourceSelector->isHidden() ? 0 : mResourceSelector->width());
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being
* restored. Read in whatever was saved in saveProperties().
*/
void MainWindow::readProperties(KConfig* config)
{
	mHiddenTrayParent = config->readEntry("HiddenTrayParent", true);
	mShowArchived     = config->readEntry("ShowArchived", false);
	mShowTime         = config->readEntry("ShowTime", true);
	mShowTimeTo       = config->readEntry("ShowTimeTo", false);
	mResourcesWidth   = config->readEntry("ResourcesWidth", (int)0);
	mShowResources    = (mResourcesWidth > 0);
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
	for (int i = 0, end = mWindowList.count();  i < end;  ++i)
		if (mWindowList[i]->isVisible())
			return mWindowList[i];
	if (tray)
		return tray;
	if (mWindowList.isEmpty())
		return 0;
	return mWindowList[0];
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
		delete mWindowList[0];    // N.B. the destructor removes the window from the list
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
		KAlarm::writeConfigWindowSize(WINDOW_NAME, re->size(), mResourcesWidth);
	MainWindowBase::resizeEvent(re);
}

void MainWindow::resourcesResized()
{
	QList<int> widths = mSplitter->sizes();
	if (widths.count() > 1)
	{
		mResourcesWidth = widths[0];
		// Width is reported as non-zero when resource selector is
		// actually invisible, so note a zero width in these circumstances.
		if (mResourcesWidth <= 5)
			mResourcesWidth = 0;
	}
}

/******************************************************************************
*  Called when the window is first displayed.
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*/
void MainWindow::showEvent(QShowEvent* se)
{
	if (mResourcesWidth > 0)
	{
		QList<int> widths;
		widths.append(mResourcesWidth);
		mSplitter->setSizes(widths);
	}
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
		KMessageBox::error(this, i18n("Failure to create menus\n(perhaps %1 missing or corrupted)", QLatin1String(UI_FILE)));
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
	config->setGroup(WINDOW_NAME);
	config->writeEntry("ColumnOrder", mListView->columnOrder());
	config->sync();
}

/******************************************************************************
*  Initialise the menu, toolbar and main window actions.
*/
void MainWindow::initActions()
{
	KActionCollection* actions = actionCollection();

	mActionTemplates  = new KAction(i18n("&Templates..."), this);
	actionCollection()->addAction(QLatin1String("templates"), mActionTemplates);
	connect(mActionTemplates, SIGNAL(triggered(bool)), SLOT(slotTemplates()));

	mActionNew = KAlarm::createNewAlarmAction(i18n("&New..."), actions, QLatin1String("new"));
	connect(mActionNew, SIGNAL(triggered(bool)), SLOT(slotNew()));

	mActionNewFromTemplate = KAlarm::createNewFromTemplateAction(i18n("New &From Template"), actions, QLatin1String("newFromTempl"));
	connect(mActionNewFromTemplate, SIGNAL(selected(const KAEvent&)), SLOT(slotNewFromTemplate(const KAEvent&)));

	mActionCreateTemplate  = new KAction(i18n("Create Tem&plate..."), this);
	actionCollection()->addAction(QLatin1String("createTemplate"), mActionCreateTemplate);
	connect(mActionCreateTemplate, SIGNAL(triggered(bool)), SLOT(slotNewTemplate()));

	mActionCopy  = new KAction(KIcon("editcopy"), i18n("&Copy..."), this);
	actionCollection()->addAction(QLatin1String("copy"), mActionCopy);
	mActionCopy->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Insert));
	connect(mActionCopy, SIGNAL(triggered(bool)), SLOT(slotCopy()));

	mActionModify  = new KAction(KIcon("edit"), i18n("&Edit..."), this);
	actionCollection()->addAction(QLatin1String("modify"), mActionModify);
	mActionModify->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
	connect(mActionModify, SIGNAL(triggered(bool)), SLOT(slotModify()));

	mActionDelete  = new KAction(KIcon("editdelete"), i18n("&Delete"), this);
	actionCollection()->addAction(QLatin1String("delete"), mActionDelete);
	mActionDelete->setShortcut(QKeySequence(Qt::Key_Delete));
	connect(mActionDelete, SIGNAL(triggered(bool)), SLOT(slotDelete()));

	mActionReactivate  = new KAction(i18n("Reac&tivate"), this);
	actionCollection()->addAction(QLatin1String("undelete"), mActionReactivate);
	mActionReactivate->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
	connect(mActionReactivate, SIGNAL(triggered(bool)), SLOT(slotReactivate()));

	mActionEnable = new KAction(this);
	actions->addAction(QLatin1String("disable"), mActionEnable);
	mActionEnable->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_B));
	connect(mActionEnable, SIGNAL(triggered(bool)), SLOT(slotEnable()));

	mActionView  = new KAction(KIcon("viewmag"), i18n("&View"), this);
	actionCollection()->addAction(QLatin1String("view"), mActionView);
	mActionView->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_W));
	connect(mActionView, SIGNAL(triggered(bool)), SLOT(slotView()));

	mActionShowTime = new KToggleAction(i18n_a_ShowAlarmTimes(), this);
	actions->addAction(QLatin1String("showAlarmTimes"), mActionShowTime);
	mActionShowTime->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_M));
	mActionShowTime->setCheckedState(KGuiItem(i18n("Hide &Alarm Times")));
	connect(mActionShowTime, SIGNAL(triggered(bool)), SLOT(slotShowTime()));

	mActionShowTimeTo = new KToggleAction(i18n_o_ShowTimeToAlarms(), this);
	actions->addAction(QLatin1String("showTimeToAlarms"), mActionShowTimeTo);
	mActionShowTimeTo->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_I));
	mActionShowTimeTo->setCheckedState(KGuiItem(i18n("Hide Time t&o Alarms")));
	connect(mActionShowTimeTo, SIGNAL(triggered(bool)), SLOT(slotShowTimeTo()));

	mActionShowArchived = new KToggleAction(KIcon("history"), i18n_e_ShowArchivedAlarms(), this);
	actions->addAction(QLatin1String("showArchivedAlarms"), mActionShowArchived);
	mActionShowArchived->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_P));
	mActionShowArchived->setCheckedState(KGuiItem(i18n_e_HideArchivedAlarms()));
	connect(mActionShowArchived, SIGNAL(triggered(bool)), SLOT(slotShowArchived()));

	mActionToggleTrayIcon  = new KToggleAction(i18n("Show in System &Tray"), this);
	actionCollection()->addAction(QLatin1String("showInSystemTray"), mActionToggleTrayIcon);
	mActionToggleTrayIcon->setCheckedState(KGuiItem(i18n("Hide From System &Tray")));
	connect(mActionToggleTrayIcon, SIGNAL(triggered(bool)), SLOT(slotToggleTrayIcon()));

	mActionToggleResourceSel = new KToggleAction(i18n_r_ShowResources(), this);
	actions->addAction(QLatin1String("showResources"), mActionToggleResourceSel);
	mActionToggleResourceSel->setCheckedState(KGuiItem(i18n("Hide &Resources")));
	connect(mActionToggleResourceSel, SIGNAL(triggered(bool)), SLOT(slotToggleResourceSelector()));

	mActionImportAlarms  = new KAction(i18n("Import &Alarms..."), this);
	actionCollection()->addAction(QLatin1String("importAlarms"), mActionImportAlarms);
	connect(mActionImportAlarms, SIGNAL(triggered(bool)), SLOT(slotImportAlarms()));

	mActionImportBirthdays  = new KAction(i18n("Import &Birthdays..."), this);
	actionCollection()->addAction(QLatin1String("importBirthdays"), mActionImportBirthdays);
	connect(mActionImportBirthdays, SIGNAL(triggered(bool)), SLOT(slotBirthdays()));

	QAction * action  = new KAction(KIcon("reload"), i18n("&Refresh Alarms"), this);
	actionCollection()->addAction(QLatin1String("refreshAlarms"), action);
	connect(action, SIGNAL(triggered(bool)), SLOT(slotResetDaemon()));

	Daemon::createAlarmEnableAction(actions);
	if (undoText.isNull())
	{
		// Get standard texts, etc., for Undo and Redo actions
		QAction * act = KStandardAction::undo(this, 0, actions);
//		undoIcon         = act->icon();
		undoShortcut     = KShortcut(act->shortcuts());
		undoText         = act->text();
		undoTextStripped = KAlarm::stripAccel(undoText);
		delete act;
		act = KStandardAction::redo(this, 0, actions);
//		redoIcon         = act->icon();
		redoShortcut     = KShortcut(act->shortcuts());
		redoText         = act->text();
		redoTextStripped = KAlarm::stripAccel(redoText);
		delete act;
	}
	mActionUndo = new KToolBarPopupAction(KIcon("undo"), undoText, this);
	actions->addAction(QLatin1String("edit_undo"), mActionUndo);
	mActionUndo->setShortcut(undoShortcut);
	connect(mActionUndo, SIGNAL(triggered(bool)), SLOT(slotUndo()));

	mActionRedo = new KToolBarPopupAction(KIcon("redo"), redoText, this);
	actions->addAction(QLatin1String("edit_redo"), mActionRedo);
	mActionRedo->setShortcut(redoShortcut);
	connect(mActionRedo, SIGNAL(triggered(bool)), SLOT(slotRedo()));

	KStandardAction::find(mListView, SLOT(slotFind()), actions);
	mActionFindNext = KStandardAction::findNext(mListView, SLOT(slotFindNext()), actions);
	mActionFindPrev = KStandardAction::findPrev(mListView, SLOT(slotFindPrev()), actions);
	KStandardAction::selectAll(mListView, SLOT(slotSelectAll()), actions);
	KStandardAction::deselect(mListView, SLOT(slotDeselect()), actions);
	KStandardAction::quit(this, SLOT(slotQuit()), actions);
	KStandardAction::keyBindings(this, SLOT(slotConfigureKeys()), actions);
	KStandardAction::configureToolbars(this, SLOT(slotConfigureToolbar()), actions);
	KStandardAction::preferences(this, SLOT(slotPreferences()), actions);
	mResourceSelector->initActions(actions);
	setStandardToolBarMenuEnabled(true);
	createGUI(UI_FILE);

	mContextMenu = static_cast<KMenu*>(factory()->container("listContext", this));
	mActionsMenu = static_cast<KMenu*>(factory()->container("actions", this));
	KMenu* resourceMenu = static_cast<KMenu*>(factory()->container("resourceContext", this));
	mResourceSelector->setContextMenu(resourceMenu);
	mMenuError = (!mContextMenu  ||  !mActionsMenu  ||  !resourceMenu);
	connect(mActionsMenu, SIGNAL(aboutToShow()), SLOT(updateActionsMenu()));
	connect(mActionUndo->menu(), SIGNAL(aboutToShow()), SLOT(slotInitUndoMenu()));
	connect(mActionUndo->menu(), SIGNAL(triggered(QAction*)), SLOT(slotUndoItem(QAction*)));
	connect(mActionRedo->menu(), SIGNAL(aboutToShow()), SLOT(slotInitRedoMenu()));
	connect(mActionRedo->menu(), SIGNAL(triggered(QAction*)), SLOT(slotRedoItem(QAction*)));
	connect(Undo::instance(), SIGNAL(changed(const QString&, const QString&)), SLOT(slotUndoStatus(const QString&, const QString&)));
	connect(mListView, SIGNAL(findActive(bool)), SLOT(slotFindActive(bool)));
	Preferences::connect(SIGNAL(preferencesChanged()), this, SLOT(updateTrayIconAction()));
	connect(theApp(), SIGNAL(trayIconToggled()), SLOT(updateTrayIconAction()));

	// Set menu item states
	setEnableText(true);
	mActionShowTime->setChecked(mShowTime);
	mActionShowTimeTo->setChecked(mShowTimeTo);
	mActionShowArchived->setChecked(mShowArchived);
	if (!Preferences::archivedKeepDays())
		mActionShowArchived->setEnabled(false);
	mActionToggleResourceSel->setChecked(mShowResources);
	slotToggleResourceSelector();
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
	for (int i = 0, end = mWindowList.count();  i < end;  ++i)
		mWindowList[i]->mActionTemplates->setEnabled(enable);
}

/******************************************************************************
* Refresh the alarm list in every main window instance.
*/
void MainWindow::refresh()
{
	kDebug(5950) << "MainWindow::refresh()\n";
	for (int i = 0, end = mWindowList.count();  i < end;  ++i)
		mWindowList[i]->mListView->refresh();
}

/******************************************************************************
* Refresh the alarm list in every main window instance which is displaying
* archived alarms.
* Called when an archived alarm setting changes in the user preferences.
*/
void MainWindow::updateArchived()
{
	kDebug(5950) << "MainWindow::updateArchived()\n";
	bool enableShowArchived = Preferences::archivedKeepDays();
	for (int i = 0, end = mWindowList.count();  i < end;  ++i)
	{
		MainWindow* w = mWindowList[i];
		if (w->mShowArchived)
		{
			if (!enableShowArchived)
				w->slotShowArchived();
			else
				w->mListView->refresh();
		}
		w->mActionShowArchived->setEnabled(enableShowArchived);
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
	kDebug(5950) << "MainWindow::updateShowAlarmTimes()\n";
	bool newTime   = Preferences::showAlarmTime();
	bool newTimeTo = Preferences::showTimeToAlarm();
	if (!newTime  &&  !newTimeTo)
		newTime = true;     // at least one time column must be displayed
	if (!oldTime  &&  !oldTimeTo)
		oldTime = true;     // at least one time column must have been displayed
	if (newTime != oldTime  ||  newTimeTo != oldTimeTo)
	{
		for (int i = 0, end = mWindowList.count();  i < end;  ++i)
		{
			MainWindow* w = mWindowList[i];
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
	for (int i = 0, end = mWindowList.count();  i < end;  ++i)
	{
		MainWindow* w = mWindowList[i];
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
		kDebug(5950) << "MainWindow::setUpdateTimer(): started timer" << endl;
	}
	else if (!needTimer  &&  timerWindow)
	{
		timerWindow->mMinuteTimerActive = false;
		MinuteTimer::disconnect(timerWindow);
		kDebug(5950) << "MainWindow::setUpdateTimer(): stopped timer" << endl;
	}
}
/******************************************************************************
* Update the time-to-alarm values for each main window which is displaying them.
*/
void MainWindow::slotUpdateTimeTo()
{
	kDebug(5950) << "MainWindow::slotUpdateTimeTo()" << endl;
	for (int i = 0, end = mWindowList.count();  i < end;  ++i)
	{
		MainWindow* w = mWindowList[i];
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
	EditAlarmDlg editDlg(false, i18n("New Alarm"), win, evnt);
	if (!text.isEmpty())
		editDlg.setAction(action, text);
	if (editDlg.exec() == QDialog::Accepted)
	{
		KAEvent event;
		AlarmResource* resource;
		editDlg.getEvent(event, resource);

		// Add the alarm to the displayed lists and to the calendar file
		if (KAlarm::addEvent(event, (win ? win->mListView : 0), resource, &editDlg) == KAlarm::UPDATE_KORG_ERR)
			KAlarm::displayKOrgUpdateError(&editDlg, KAlarm::ERR_ADD, 1);
		Undo::saveAdd(event, resource);

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
	EditAlarmDlg editDlg(false, i18n("Edit Alarm"), win, &event, EditAlarmDlg::RES_USE_EVENT_ID);
	if (editDlg.exec() == QDialog::Accepted)
	{
		KAEvent newEvent;
		AlarmResource* resource;
		bool changeDeferral = !editDlg.getEvent(newEvent, resource);

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
				KAlarm::displayKOrgUpdateError(&editDlg, KAlarm::ERR_MODIFY, 1);
		}
		Undo::saveEdit(event, newEvent, resource);

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
		EditAlarmDlg editDlg(false, (event.expired() ? i18n("Archived Alarm") + " [" + i18n("read-only") + ']'
		                                             : i18n("View Alarm")),
		                     this, &event, EditAlarmDlg::RES_PROMPT, true);
		editDlg.exec();
	}
}

/******************************************************************************
*  Called when the Delete button is clicked to delete the currently highlighted
*  alarms in the list.
*/
void MainWindow::slotDelete()
{
	QList<const KAEvent*> events = mListView->selectedEvents();
	// Copy the events to be deleted, in case any are deleted by being
	// triggered while the confirmation prompt is displayed.
	QList<KAEvent> eventCopies;
	Undo::EventList undos;
	AlarmResources* resources = AlarmResources::instance();
	for (int i = 0, end = events.count();  i < end;  ++i)
	{
		const KAEvent* event = events[i];
		eventCopies.append(*event);
		undos.append(*event, resources->resourceForIncidence(event->id()));
	}
	if (Preferences::confirmAlarmDeletion())
	{
		int n = events.count();
		if (KMessageBox::warningContinueCancel(this, i18np("Do you really want to delete the selected alarm?",
		                                                   "Do you really want to delete the %n selected alarms?", n),
		                                       i18np("Delete Alarm", "Delete Alarms", n),
		                                       KGuiItem(i18n("&Delete"), "editdelete"),
		                                       Preferences::CONFIRM_ALARM_DELETION)
		    != KMessageBox::Continue)
			return;
	}

	// Delete the events from the calendar and displays
	KAlarm::deleteEvents(eventCopies, true, this);
	Undo::saveDeletes(undos);
}

/******************************************************************************
*  Called when the Reactivate button is clicked to reinstate the currently
*  highlighted archived alarms in the list.
*/
void MainWindow::slotReactivate()
{
	int i, end;
	QList<const KAEvent*> events = mListView->selectedEvents();
	mListView->clearSelection();

	// Add the alarms to the displayed lists and to the calendar file
	QList<KAEvent> eventCopies;
	Undo::EventList undos;
	QStringList ineligibleIDs;
	AlarmResources* resources = AlarmResources::instance();
	for (i = 0, end = events.count();  i < end;  ++i)
		eventCopies.append(*events[i]);
	KAlarm::reactivateEvents(eventCopies, ineligibleIDs, mListView, 0, this);

	// Create the undo list, excluding ineligible events
	for (i = 0, end = eventCopies.count();  i < end;  ++i)
	{
		QString id = eventCopies[i].id();
		if (!ineligibleIDs.contains(id))
			undos.append(eventCopies[i], resources->resourceForIncidence(id));
	}
	Undo::saveReactivates(undos);
}

/******************************************************************************
*  Called when the Enable/Disable button is clicked to enable or disable the
*  currently highlighted alarms in the list.
*/
void MainWindow::slotEnable()
{
	bool enable = mActionEnableEnable;    // save since changed in response to KAlarm::enableEvent()
	QList<const KAEvent*> events = mListView->selectedEvents();
	QList<KAEvent> eventCopies;
	for (int i = 0, end = events.count();  i < end;  ++i)
		eventCopies += *events[i];
	KAlarm::enableEvents(eventCopies, mListView, enable, this);
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
*  Called when the Show Archived Alarms menu item is selected or deselected.
*/
void MainWindow::slotShowArchived()
{
	mShowArchived = !mShowArchived;
	mActionShowArchived->setChecked(mShowArchived);
	mActionShowArchived->setToolTip(mShowArchived ? i18n_HideArchivedAlarms() : i18n_ShowArchivedAlarms());
	mListView->showArchived(mShowArchived);
	mListView->refresh();
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
		QList<KAEvent> events = dlg.events();
		if (!events.isEmpty())
		{
			mListView->clearSelection();
			// Add alarm to the displayed lists and to the calendar file
			KAlarm::UpdateStatus status = KAlarm::addEvents(events, mListView, &dlg, true, true);

			Undo::EventList undos;
			AlarmResources* resources = AlarmResources::instance();
			for (int i = 0, end = events.count();  i < end;  ++i)
				undos.append(events[i], resources->resourceForIncidence(events[i].id()));
			Undo::saveAdds(undos, i18n("Import birthdays"));

			if (status != KAlarm::UPDATE_FAILED)
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
*  Called when the Show Resource Selector menu item is selected.
*/
void MainWindow::slotToggleResourceSelector()
{
	bool show = mActionToggleResourceSel->isChecked();
	if (show)
	{
		if (mResourcesWidth <= 0)
		{
			mResourcesWidth = mResourceSelector->sizeHint().width();
			mResourceSelector->resize(mResourcesWidth, mResourceSelector->height());
			QList<int> widths = mSplitter->sizes();
			if (widths.count() == 1)
			{
				int listwidth = widths[0] - mSplitter->handleWidth() - mResourcesWidth;
				mListView->resize(listwidth, mListView->height());
				widths.append(listwidth);
				widths[0] = mResourcesWidth;
			}
			mSplitter->setSizes(widths);
		}
		mResourceSelector->show();
	}
	else
		mResourceSelector->hide();
}

/******************************************************************************
* Called when an error occurs in the resource calendar, to display a message.
*/
void MainWindow::showErrorMessage(const QString& msg)
{
	KMessageBox::error(this, msg);
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
void MainWindow::slotUndoItem(QAction* action)
{
	int id = mUndoMenuIds[action];
	Undo::undo(id, this, Undo::actionText(Undo::UNDO, id));
}

/******************************************************************************
*  Called when a Redo item is selected.
*/
void MainWindow::slotRedoItem(QAction* action)
{
	int id = mUndoMenuIds[action];
	Undo::redo(id, this, Undo::actionText(Undo::REDO, id));
}

/******************************************************************************
*  Called when the Undo menu is about to show.
*  Populates the menu.
*/
void MainWindow::slotInitUndoMenu()
{
	initUndoMenu(mActionUndo->menu(), Undo::UNDO);
}

/******************************************************************************
*  Called when the Redo menu is about to show.
*  Populates the menu.
*/
void MainWindow::slotInitRedoMenu()
{
	initUndoMenu(mActionRedo->menu(), Undo::REDO);
}

/******************************************************************************
*  Populate the undo or redo menu.
*/
void MainWindow::initUndoMenu(QMenu* menu, Undo::Type type)
{
	menu->clear();
	mUndoMenuIds.clear();
	const QString& action = (type == Undo::UNDO) ? undoTextStripped : redoTextStripped;
	QList<int> ids = Undo::ids(type);
	for (int i = 0, end = ids.count();  i < end;  ++i)
	{
		int id = ids[i];
		QAction* act = menu->addAction(i18nc("Undo [action]: message", "%1 %2: %3",
		                               action, Undo::actionText(type, id), Undo::description(type, id)));
		mUndoMenuIds[act] = id;
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
	KKeyDialog::configure(actionCollection(), KKeyChooser::LetterShortcutsAllowed, this);
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
		kDebug(5950) << "MainWindow::slotDeletion(true)\n";
		selectionCleared();    // disable actions
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
	const QMimeData* data = e->mimeData();
	bool accept = KCal::ICalDrag::canDecode(data) ? !AlarmListView::dragging()   // don't accept "text/calendar" objects from KAlarm
	                                           :    data->hasText()
	                                             || KUrl::List::canDecode(data)
	                                             || KPIM::MailList::canDecode(data);
	if (accept)
		e->accept();
	else
		e->ignore();
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
	return hd ? hd->asUnicodeString() : QString();
}

/******************************************************************************
*  Called when an object is dropped on a main or system tray window, to
*  evaluate the action required and extract the text.
*/
void MainWindow::executeDropEvent(MainWindow* win, QDropEvent* e)
{
	const QMimeData* data = e->mimeData();
	KAEvent::Action action = KAEvent::MESSAGE;
	QByteArray      bytes;
	AlarmText       alarmText;
	KPIM::MailList  mailList;
	KUrl::List      files;
	KCal::CalendarLocal calendar(Preferences::timeZone(true));
#ifndef NDEBUG
	QString fmts = data->formats().join(", ");
	kDebug(5950) << "MainWindow::executeDropEvent(): " << fmts << endl;
#endif

	/* The order of the tests below matters, since some dropped objects
	 * provide more than one mime type.
	 * Don't change them without careful thought !!
	 */
	if (!(bytes = data->data("message/rfc822")).isEmpty())
	{
		// Email message(s). Ignore all but the first.
		kDebug(5950) << "MainWindow::executeDropEvent(email)" << endl;
		KMime::Content content;
		content.setContent(bytes);
		content.parse();
		QString body;
                if (content.textContent())
			body = content.textContent()->decodedText(true, true);    // strip trailing newlines & spaces
		unsigned long sernum = 0;
		if (KPIM::MailList::canDecode(data))
		{
			// Get its KMail serial number to allow the KMail message
			// to be called up from the alarm message window.
			mailList = KPIM::MailList::fromMimeData(data);
			if (!mailList.isEmpty())
				sernum = mailList[0].serialNumber();
		}
		alarmText.setEmail(getMailHeader("To", content),
		                   getMailHeader("From", content),
		                   getMailHeader("Cc", content),
		                   getMailHeader("Date", content),
		                   getMailHeader("Subject", content),
				   body, sernum);
	}
	else if (!(files = KUrl::List::fromMimeData(data)).isEmpty())
	{
		kDebug(5950) << "MainWindow::executeDropEvent(URL)" << endl;
		action = KAEvent::FILE;
		alarmText.setText(files[0].prettyUrl());
	}
	else if (KPIM::MailList::canDecode(data))
	{
		mailList = KPIM::MailList::fromMimeData(data);
		// KMail message(s). Ignore all but the first.
		kDebug(5950) << "MainWindow::executeDropEvent(KMail_list)" << endl;
		if (mailList.isEmpty())
			return;
		KPIM::MailSummary& summary = mailList[0];
		QDateTime dt;
		dt.setTime_t(summary.date());
		QString body = KAMail::getMailBody(summary.serialNumber());
		alarmText.setEmail(summary.to(), summary.from(), QString(),
		                   KGlobal::locale()->formatDateTime(dt), summary.subject(),
		                   body, summary.serialNumber());
	}
	else if (KCal::ICalDrag::fromMimeData(data, &calendar))
	{
		// iCalendar - ignore all but the first event
		kDebug(5950) << "MainWindow::executeDropEvent(iCalendar)" << endl;
		KCal::Event::List events = calendar.rawEvents();
		if (!events.isEmpty())
		{
			KAEvent ev(events[0]);
			executeNew(win, &ev);
		}
		return;
	}
	else if (data->hasText())
	{
		QString text = data->text();
		kDebug(5950) << "MainWindow::executeDropEvent(text)" << endl;
		alarmText.setText(text);
	}
	else
		return;

	if (!alarmText.isEmpty())
		executeNew(win, 0, action, alarmText);
}

/******************************************************************************
* Called when the status of a resource has changed.
* Enable or disable actions appropriately.
*/
void MainWindow::slotResourceStatusChanged(AlarmResource*, AlarmResources::Change change)
{
	// Find whether there are any writable resources
	AlarmResources* resources = AlarmResources::instance();
	bool active  = resources->activeCount(AlarmResource::ACTIVE, true);
	bool templat = resources->activeCount(AlarmResource::TEMPLATE, true);
	for (int i = 0, end = mWindowList.count();  i < end;  ++i)
	{
		MainWindow* w = mWindowList[i];
		w->mActionImportAlarms->setEnabled(active || templat);
		w->mActionImportBirthdays->setEnabled(active);
		w->mActionNew->setEnabled(active);
		w->mActionNewFromTemplate->setEnabled(active);
		w->mActionCreateTemplate->setEnabled(templat);
		w->slotSelection();
	}

	if (change == AlarmResources::Location)
		mListView->refresh();
}

/******************************************************************************
*  Called when the selected items in the ListView changes.
*  Selects the new current item, and enables the actions appropriately.
*/
void MainWindow::slotSelection()
{
	// Find which item has been selected, and whether more than one is selected
	QList<EventListViewItemBase*> items = mListView->selectedItems();
	int count = items.count();
	if (!count)
	{
		selectionCleared();    // disable actions
		return;
	}

	// Find whether there are any writable resources
	bool active = mActionNew->isEnabled();

	AlarmListViewItem* item = (AlarmListViewItem*)((count == 1) ? items.first() : 0);
	bool readOnly = false;
	bool allArchived = true;
	bool enableReactivate = true;
	bool enableEnableDisable = true;
	bool enableEnable = false;
	bool enableDisable = false;
	AlarmResources* resources = AlarmResources::instance();
	KDateTime now = KDateTime::currentUtcDateTime();
	for (int i = 0, end = items.count();  i < end;  ++i)
	{
		const KAEvent& event = ((AlarmListViewItem*)items[i])->event();
		bool expired = event.expired();
		if (!expired)
			allArchived = false;
		const Event* ev = resources->event(event.id());
		AlarmResource* resource = resources->resource(ev);
		if (!resource  ||  !resource->writable(ev))
			readOnly = true;
		if (enableReactivate
		&&  (!expired  ||  !event.occursAfter(now, true)))
			enableReactivate = false;
		if (enableEnableDisable)
		{
			if (expired)
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

	kDebug(5950) << "MainWindow::slotSelection(true)\n";
	mActionCreateTemplate->setEnabled((count == 1) && (resources->activeCount(AlarmResource::TEMPLATE, true) > 0));
	mActionCopy->setEnabled(active && count == 1);
	mActionModify->setEnabled(active && !readOnly && item && !mListView->archived(item));
	mActionView->setEnabled(count == 1);
	mActionDelete->setEnabled(!readOnly && (active || allArchived));
	mActionReactivate->setEnabled(active && enableReactivate);
	mActionEnable->setEnabled(active && !readOnly && (enableEnable || enableDisable));
	if (enableEnable || enableDisable)
		setEnableText(enableEnable);
}

/******************************************************************************
*  Called when the mouse is clicked on the ListView.
*  Deselects the current item and disables the actions if appropriate, or
*  displays a context menu to modify or delete the selected item.
*/
void MainWindow::slotMouseClicked(int button, Q3ListViewItem* item, const QPoint& pt, int)
{
	if (button == Qt::RightButton)
	{
		kDebug(5950) << "MainWindow::slotMouseClicked(right)\n";
		if (mContextMenu)
			mContextMenu->popup(pt);
	}
	else if (!item)
	{
		kDebug(5950) << "MainWindow::slotMouseClicked(left)\n";
		mListView->clearSelection();
		selectionCleared();    // disable actions
	}
}

/******************************************************************************
*  Disables actions when no item is selected.
*/
void MainWindow::selectionCleared()
{
	mActionCreateTemplate->setEnabled(false);
	mActionCopy->setEnabled(false);
	mActionModify->setEnabled(false);
	mActionView->setEnabled(false);
	mActionDelete->setEnabled(false);
	mActionReactivate->setEnabled(false);
	mActionEnable->setEnabled(false);
}

/******************************************************************************
*  Called when the mouse is double clicked on the ListView.
*  Displays the Edit Alarm dialog, for the clicked item if applicable.
*/
void MainWindow::slotDoubleClicked(Q3ListViewItem* item)
{
	kDebug(5950) << "MainWindow::slotDoubleClicked()\n";
	if (item)
	{
		if (mListView->archived((AlarmListViewItem*)item))
			slotView();
		else if (mActionModify->isEnabled())
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
	if (win  &&  mWindowList.indexOf(win) != -1)
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
			win->activateWindow();
			return win;
		}
	}

	// No window is specified, or the window doesn't exist. Open a new one.
	win = create();
	win->show();
	return win;
}
