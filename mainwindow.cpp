/*
 *  mainwindow.cpp  -  main application window
 *  Program:  kalarm
 *  (C) 2001 - 2004 by David Jarvie <software@astrojar.org.uk>
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
#include <kkeydialog.h>
#include <kedittoolbar.h>
#include <kaboutdata.h>
#include <dcopclient.h>
#include <kdebug.h>
#include <kstdguiitem.h>

#include <maillistdrag.h>

#include "alarmcalendar.h"
#include "alarmlistview.h"
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

static QString messageFromPrefix    = i18n("'From' email address", "From:");
static QString messageToPrefix      = i18n("Email addressee", "To:");
static QString messageDatePrefix    = i18n("Date:");
static QString messageSubjectPrefix = i18n("Email subject", "Subject:");


/*=============================================================================
=  Class: KAlarmMainWindow
=============================================================================*/

QPtrList<KAlarmMainWindow> KAlarmMainWindow::mWindowList;
TemplateDlg*  KAlarmMainWindow::mTemplateDlg = 0;

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString KAlarmMainWindow::i18n_a_ShowAlarmTimes()    { return i18n("Show &Alarm Times"); }
QString KAlarmMainWindow::i18n_t_ShowAlarmTimes()    { return i18n("Show alarm &time"); }
QString KAlarmMainWindow::i18n_m_ShowAlarmTimes()    { return i18n("Show alarm ti&me"); }
QString KAlarmMainWindow::i18n_o_ShowTimeToAlarms()  { return i18n("Show Time t&o Alarms"); }
QString KAlarmMainWindow::i18n_n_ShowTimeToAlarms()  { return i18n("Show time u&ntil alarm"); }
QString KAlarmMainWindow::i18n_l_ShowTimeToAlarms()  { return i18n("Show time unti&l alarm"); }
QString KAlarmMainWindow::i18n_e_ShowExpiredAlarms() { return i18n("Show &Expired Alarms"); }
QString KAlarmMainWindow::i18n_s_ShowExpiredAlarms() { return i18n("&Show expired alarms"); }


/******************************************************************************
* Construct an instance.
* To avoid resize() events occurring while still opening the calendar (and
* resultant crashes), the calendar is opened before constructing the instance.
*/
KAlarmMainWindow* KAlarmMainWindow::create(bool restored)
{
	theApp()->checkCalendarDaemon();    // ensure calendar is open and daemon started
	return new KAlarmMainWindow(restored);
}

KAlarmMainWindow::KAlarmMainWindow(bool restored)
	: MainWindowBase(0, 0, WGroupLeader | WStyle_ContextHelp | WDestructiveClose),
	  mMinuteTimerActive(false),
	  mHiddenTrayParent(false),
	  mShowExpired(Preferences::instance()->showExpiredAlarms()),
	  mShowTime(Preferences::instance()->showAlarmTime()),
	  mShowTimeTo(Preferences::instance()->showTimeToAlarm())
{
	kdDebug(5950) << "KAlarmMainWindow::KAlarmMainWindow()\n";
	setAutoSaveSettings(QString::fromLatin1("MainWindow"));    // save window sizes etc.
	setPlainCaption(kapp->aboutData()->programName());
	if (!restored)
	{
		QSize s;
		if (KAlarm::readConfigWindowSize("MainWindow", s))
			resize(s);
	}

	setAcceptDrops(true);         // allow drag-and-drop onto this window
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

KAlarmMainWindow::~KAlarmMainWindow()
{
	kdDebug(5950) << "KAlarmMainWindow::~KAlarmMainWindow()\n";
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
	KAlarmMainWindow* main = mainMainWindow();
	if (main)
		KAlarm::writeConfigWindowSize("MainWindow", main->size());
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
	for (KAlarmMainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
		if (w->isVisible())
			return w;
	if (tray)
		return tray;
	return mWindowList.first();
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
	while (mWindowList.first())
		delete mWindowList.first();
}

/******************************************************************************
*  Called when the window's size has changed (before it is painted).
*  Sets the last column in the list view to extend at least to the right hand
*  edge of the list view.
*  Records the new size in the config file.
*/
void KAlarmMainWindow::resizeEvent(QResizeEvent* re)
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
void KAlarmMainWindow::showEvent(QShowEvent* se)
{
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
	mActionTemplates      = new KAction(i18n("&Templates..."), 0, this, SLOT(slotTemplates()), actions, "templates");
	mActionNew            = KAlarm::createNewAlarmAction(i18n("&New..."), this, SLOT(slotNew()), actions, "new");
	mActionCreateTemplate = new KAction(i18n("Create Tem&plate..."), 0, this, SLOT(slotNewTemplate()), actions, "createTemplate");
	mActionCopy           = new KAction(i18n("&Copy..."), "editcopy", Qt::SHIFT+Qt::Key_Insert, this, SLOT(slotCopy()), actions, "copy");
	mActionModify         = new KAction(i18n("&Edit..."), "edit", Qt::CTRL+Qt::Key_E, this, SLOT(slotModify()), actions, "modify");
	mActionDelete         = new KAction(i18n("&Delete"), "editdelete", Qt::Key_Delete, this, SLOT(slotDelete()), actions, "delete");
	mActionUndelete       = new KAction(i18n("&Undelete"), "undo", Qt::CTRL+Qt::Key_Z, this, SLOT(slotUndelete()), actions, "undelete");
	mActionEnable         = new KAction(QString::null, 0, Qt::CTRL+Qt::Key_B, this, SLOT(slotEnable()), actions, "disable");
	mActionView           = new KAction(i18n("&View"), "viewmag", Qt::CTRL+Qt::Key_W, this, SLOT(slotView()), actions, "view");
	mActionShowTime       = new KToggleAction(i18n_a_ShowAlarmTimes(), Qt::CTRL+Qt::Key_M, this, SLOT(slotShowTime()), actions, "showAlarmTimes");
	mActionShowTime->setCheckedState(i18n("Hide &Alarm Times"));
	mActionShowTimeTo     = new KToggleAction(i18n_o_ShowTimeToAlarms(), Qt::CTRL+Qt::Key_I, this, SLOT(slotShowTimeTo()), actions, "showTimeToAlarms");
	mActionShowTimeTo->setCheckedState(i18n("Hide Time t&o Alarms"));
	mActionShowExpired    = new KToggleAction(i18n_e_ShowExpiredAlarms(), Qt::CTRL+Qt::Key_P, this, SLOT(slotShowExpired()), actions, "showExpiredAlarms");
	mActionShowExpired->setCheckedState(i18n("Hide &Expired Alarms"));
	mActionToggleTrayIcon = new KToggleAction(i18n("Show in System &Tray"), Qt::CTRL+Qt::Key_Y, this, SLOT(slotToggleTrayIcon()), actions, "showInSystemTray");
	mActionToggleTrayIcon->setCheckedState(i18n("Hide From System &Tray"));
	new KAction(i18n("Import &Birthdays..."), 0, this, SLOT(slotBirthdays()), actions, "importBirthdays");
	new KAction(i18n("&Refresh Alarms"), "reload", 0, this, SLOT(slotResetDaemon()), actions, "refreshAlarms");
	Daemon::createAlarmEnableAction(actions, "alarmEnable");
	KStdAction::quit(this, SLOT(slotQuit()), actions);
	KStdAction::keyBindings(this, SLOT(slotConfigureKeys()), actions);
	KStdAction::configureToolbars(this, SLOT(slotConfigureToolbar()), actions);
	KStdAction::preferences(this, SLOT(slotPreferences()), actions);
	Daemon::createControlAction(actions, "controlDaemon");
	setStandardToolBarMenuEnabled(true);
	createGUI("kalarmui.rc");

	mContextMenu = static_cast<KPopupMenu*>(factory()->container("listContext", this));
	mActionsMenu = static_cast<KPopupMenu*>(factory()->container("actions", this));
	connect(mActionsMenu, SIGNAL(aboutToShow()), SLOT(updateActionsMenu()));
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

	mActionCopy->setEnabled(false);
	mActionModify->setEnabled(false);
	mActionDelete->setEnabled(false);
	mActionUndelete->setEnabled(false);
	mActionView->setEnabled(false);
	mActionEnable->setEnabled(false);
	mActionCreateTemplate->setEnabled(false);

	Daemon::checkStatus();
	Daemon::monitoringAlarms();
}

/******************************************************************************
* Enable or disable the Templates menu item in every main window instance.
*/
void KAlarmMainWindow::enableTemplateMenuItem(bool enable)
{
	for (KAlarmMainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
		w->mActionTemplates->setEnabled(enable);
}

/******************************************************************************
* Refresh the alarm list in every main window instance.
*/
void KAlarmMainWindow::refresh()
{
	kdDebug(5950) << "KAlarmMainWindow::refresh()\n";
	for (KAlarmMainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
		w->mListView->refresh();
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
	for (KAlarmMainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
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
void KAlarmMainWindow::updateTimeColumns(bool oldTime, bool oldTimeTo)
{
	kdDebug(5950) << "KAlarmMainWindow::updateShowAlarmTimes()\n";
	bool newTime   = Preferences::instance()->showAlarmTime();
	bool newTimeTo = Preferences::instance()->showTimeToAlarm();
	if (!newTime  &&  !newTimeTo)
		newTime = true;     // at least one time column must be displayed
	if (!oldTime  &&  !oldTimeTo)
		oldTime = true;     // at least one time column must have been displayed
	if (newTime != oldTime  ||  newTimeTo != oldTimeTo)
	{
		for (KAlarmMainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
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
void KAlarmMainWindow::setUpdateTimer()
{
	// Check whether any windows need to be updated
	KAlarmMainWindow* needTimer = 0;
	KAlarmMainWindow* timerWindow = 0;
	for (KAlarmMainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
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
		kdDebug(5950) << "KAlarmMainWindow::setUpdateTimer(): started timer" << endl;
	}
	else if (!needTimer  &&  timerWindow)
	{
		timerWindow->mMinuteTimerActive = false;
		MinuteTimer::disconnect(timerWindow);
		kdDebug(5950) << "KAlarmMainWindow::setUpdateTimer(): stopped timer" << endl;
	}
}
/******************************************************************************
* Update the time-to-alarm values for each main window which is displaying them.
*/
void KAlarmMainWindow::slotUpdateTimeTo()
{
	kdDebug(5950) << "KAlarmMainWindow::slotUpdateTimeTo()" << endl;
	for (KAlarmMainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
		if (w->isVisible()  &&  w->mListView->showingTimeTo())
			w->mListView->updateTimeToAlarms();
}

/******************************************************************************
* Select an alarm in the displayed list.
*/
void KAlarmMainWindow::selectEvent(const QString& eventID)
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
void KAlarmMainWindow::slotNew()
{
	executeNew(this);
}

/******************************************************************************
*  Execute a New Alarm dialog, optionally setting the action and text.
*/
void KAlarmMainWindow::executeNew(KAlarmMainWindow* win, KAEvent::Action action, const QString& text)
{
	EditAlarmDlg editDlg(false, i18n("New Alarm"), win, "editDlg");
	if (!text.isNull())
		editDlg.setAction(action, text);
	if (editDlg.exec() == QDialog::Accepted)
	{
		KAEvent event;
		editDlg.getEvent(event);

		// Add the alarm to the displayed lists and to the calendar file
		KAlarm::addEvent(event, (win ? win->mListView : 0));

		alarmWarnings(&editDlg, &event);
	}
}

/******************************************************************************
*  Called when the New Template button is clicked to create a new template
*  based on the currently selected alarm.
*/
void KAlarmMainWindow::slotNewTemplate()
{
	AlarmListViewItem* item = mListView->singleSelectedItem();
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
void KAlarmMainWindow::slotCopy()
{
	AlarmListViewItem* item = mListView->singleSelectedItem();
	if (item)
	{
		KAEvent event = item->event();
		EditAlarmDlg editDlg(false, i18n("New Alarm"), this, "editDlg", &event);
		if (editDlg.exec() == QDialog::Accepted)
		{
			KAEvent event;
			editDlg.getEvent(event);

			// Add the alarm to the displayed lists and to the calendar file
			KAlarm::addEvent(event, mListView);

			alarmWarnings(&editDlg, &event);
		}
	}
}

/******************************************************************************
*  Called when the Modify button is clicked to edit the currently highlighted
*  alarm in the list.
*/
void KAlarmMainWindow::slotModify()
{
	AlarmListViewItem* item = mListView->singleSelectedItem();
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

			alarmWarnings(&editDlg, &newEvent);
		}
	}
}

/******************************************************************************
*  Called when the View button is clicked to view the currently highlighted
*  alarm in the list.
*/
void KAlarmMainWindow::slotView()
{
	AlarmListViewItem* item = mListView->singleSelectedItem();
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
void KAlarmMainWindow::slotDelete()
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
	AlarmCalendar::activeCalendar()->startUpdate();    // prevent multiple saves of the calendars until we're finished
	AlarmCalendar::expiredCalendar()->startUpdate();
	for (QValueList<EventListViewItemBase*>::Iterator it = items.begin();  it != items.end();  ++it)
	{
		AlarmListViewItem* item = (AlarmListViewItem*)(*it);
		KAEvent event = item->event();

		// Delete the event from the calendar and displays
		KAlarm::deleteEvent(event);
	}
	AlarmCalendar::activeCalendar()->endUpdate();      // save the calendars now
	AlarmCalendar::expiredCalendar()->endUpdate();
}

/******************************************************************************
*  Called when the Undelete button is clicked to reinstate the currently
*  highlighted expired alarms in the list.
*/
void KAlarmMainWindow::slotUndelete()
{
	QValueList<EventListViewItemBase*> items = mListView->selectedItems();
	mListView->clearSelection();
	AlarmCalendar::activeCalendar()->startUpdate();    // prevent multiple saves of the calendars until we're finished
	AlarmCalendar::expiredCalendar()->startUpdate();
	for (QValueList<EventListViewItemBase*>::Iterator it = items.begin();  it != items.end();  ++it)
	{
		AlarmListViewItem* item = (AlarmListViewItem*)(*it);
		KAEvent event = item->event();
		event.setArchive();    // ensure that it gets re-archived if it is deleted

		// Add the alarm to the displayed lists and to the calendar file
		KAlarm::undeleteEvent(event, mListView);
	}
	AlarmCalendar::activeCalendar()->endUpdate();      // save the calendars now
	AlarmCalendar::expiredCalendar()->endUpdate();
}

/******************************************************************************
*  Called when the Enable/Disable button is clicked to enable or disable the
*  currently highlighted alarms in the list.
*/
void KAlarmMainWindow::slotEnable()
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
void KAlarmMainWindow::slotShowTime()
{
	mShowTime = !mShowTime;
	mActionShowTime->setChecked(mShowTime);
	if (!mShowTime  &&  !mShowTimeTo)
	{
		// At least one time column must be displayed
		mShowTimeTo = true;
		mActionShowTimeTo->setChecked(mShowTimeTo);
	}
	mListView->selectTimeColumns(mShowTime, mShowTimeTo);
}

/******************************************************************************
*  Called when the Show Time To Alarms menu item is selected or deselected.
*/
void KAlarmMainWindow::slotShowTimeTo()
{
	mShowTimeTo = !mShowTimeTo;
	mActionShowTimeTo->setChecked(mShowTimeTo);
	if (!mShowTimeTo  &&  !mShowTime)
	{
		// At least one time column must be displayed
		mShowTime = true;
		mActionShowTime->setChecked(mShowTime);
	}
	mListView->selectTimeColumns(mShowTime, mShowTimeTo);
	setUpdateTimer();
}

/******************************************************************************
*  Called when the Show Expired Alarms menu item is selected or deselected.
*/
void KAlarmMainWindow::slotShowExpired()
{
	mShowExpired = !mShowExpired;
	mActionShowExpired->setChecked(mShowExpired);
	mListView->showExpired(mShowExpired);
	mListView->refresh();
}

/******************************************************************************
*  Called when the Import Birthdays menu item is selected, to display birthdays
*  from the address book for selection as alarms.
*/
void KAlarmMainWindow::slotBirthdays()
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
void KAlarmMainWindow::slotTemplates()
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
void KAlarmMainWindow::slotTemplatesEnd()
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
	mActionToggleTrayIcon->setEnabled(theApp()->KDEDesktop() && !theApp()->wantRunInSystemTray());
	mActionToggleTrayIcon->setChecked(theApp()->trayIconDisplayed());
}

/******************************************************************************
* Called when the Actions menu is about to be displayed.
* Update the status of the Alarms Enabled menu item.
*/
void KAlarmMainWindow::updateActionsMenu()
{
	Daemon::checkStatus();   // update the Alarms Enabled item status
}

/******************************************************************************
*  Called when the Reset Daemon menu item is selected.
*/
void KAlarmMainWindow::slotResetDaemon()
{
	KAlarm::resetDaemon();
}

/******************************************************************************
*  Called when the "Configure KAlarm" menu item is selected.
*/
void KAlarmMainWindow::slotPreferences()
{
	KAlarmPrefDlg prefDlg;
	prefDlg.exec();
}

/******************************************************************************
*  Called when the Configure Keys menu item is selected.
*/
void KAlarmMainWindow::slotConfigureKeys()
{
	KKeyDialog::configure(actionCollection(), this);
}

/******************************************************************************
*  Called when the Configure Toolbars menu item is selected.
*/
void KAlarmMainWindow::slotConfigureToolbar()
{
	saveMainWindowSettings(KGlobal::config(), "MainWindow");
	KEditToolbar dlg(factory());
	dlg.exec();
}

/******************************************************************************
*  Called when the Quit menu item is selected.
*/
void KAlarmMainWindow::slotQuit()
{
	theApp()->doQuit(this);
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
	if (!mListView->selectedCount())
	{
		kdDebug(5950) << "KAlarmMainWindow::slotDeletion(true)\n";
		mActionCreateTemplate->setEnabled(false);
		mActionCopy->setEnabled(false);
		mActionModify->setEnabled(false);
		mActionView->setEnabled(false);
		mActionDelete->setEnabled(false);
		mActionUndelete->setEnabled(false);
		mActionEnable->setEnabled(false);
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
	KAEvent::Action action = KAEvent::MESSAGE;
	QString text;
	KPIM::MailList mailList;
	KURL::List files;
	if (KURLDrag::decode(e, files)  &&  files.count())
	{
		action = KAEvent::FILE;
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

		// Get the body of the email from KMail
		QCString    replyType;
		QByteArray  replyData;
		QByteArray  data;
		QDataStream arg(data, IO_WriteOnly);
		arg << summary.serialNumber();
		arg << (int)0;
		QCString body;
		if (kapp->dcopClient()->call("kmail", "KMailIface", "getDecodedBodyPart(Q_UINT32,int)", data, replyType, replyData)
		&&  replyType == "QString")
		{
			QDataStream reply_stream(replyData, IO_ReadOnly);
			reply_stream >> body;
			if (!body.isEmpty())
			{
				text += "\n\n";
				text += body;
			}
		}
		else
			kdDebug(5950) << "KAlarmMainWindow::executeDropEvent(): kmail getDecodedBodyPart() call failed\n";
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
*  Check whether a text is an email, and if so return its headers or optionally
*  only its subject line.
*  Reply = headers/subject line, or QString::null if not the text of an email.
*/
QString KAlarmMainWindow::emailHeaders(const QString& text, bool subjectOnly)
{
	QStringList lines = QStringList::split('\n', text);
	if (lines.count() >= 4
	&&  lines[0].startsWith(messageFromPrefix)
	&&  lines[1].startsWith(messageToPrefix)
	&&  lines[2].startsWith(messageDatePrefix)
	&&  lines[3].startsWith(messageSubjectPrefix))
	{
		if (subjectOnly)
			return lines[3].mid(messageSubjectPrefix.length());
		return lines[0] + '\n' + lines[1] + '\n' + lines[2] + '\n' + lines[3];
	}
	return QString::null;
}

/******************************************************************************
*  Called when the selected items in the ListView changes.
*  Selects the new current item, and enables the actions appropriately.
*/
void KAlarmMainWindow::slotSelection()
{
	// Find which item has been selected, and whether more than one is selected
	QValueList<EventListViewItemBase*> items = mListView->selectedItems();
	int count = items.count();
	AlarmListViewItem* item = (AlarmListViewItem*)((count == 1) ? items.first() : 0);
	bool enableUndelete = true;
	bool enableEnableDisable = true;
	bool enableEnable = false;
	bool enableDisable = false;
	QDateTime now = QDateTime::currentDateTime();
	for (QValueList<EventListViewItemBase*>::Iterator it = items.begin();  it != items.end();  ++it)
	{
		const KAEvent& event = ((AlarmListViewItem*)(*it))->event();
		if (enableUndelete
		&&  (!event.expired()  ||  !event.occursAfter(now, true)))
			enableUndelete = false;
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

	kdDebug(5950) << "KAlarmMainWindow::slotSelection(true)\n";
	mActionCreateTemplate->setEnabled(count == 1);
	mActionCopy->setEnabled(count == 1);
	mActionModify->setEnabled(item && !mListView->expired(item));
	mActionView->setEnabled(count == 1);
	mActionDelete->setEnabled(count);
	mActionUndelete->setEnabled(count && enableUndelete);
	mActionEnable->setEnabled(enableEnable || enableDisable);
	if (enableEnable || enableDisable)
		setEnableText(enableEnable);
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
		mContextMenu->popup(pt);
	}
	else if (!item)
	{
		kdDebug(5950) << "KAlarmMainWindow::slotMouseClicked(left)\n";
		mListView->clearSelection();
		mActionCreateTemplate->setEnabled(false);
		mActionCopy->setEnabled(false);
		mActionModify->setEnabled(false);
		mActionView->setEnabled(false);
		mActionDelete->setEnabled(false);
		mActionUndelete->setEnabled(false);
		mActionEnable->setEnabled(false);
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
void KAlarmMainWindow::setEnableText(bool enable)
{
	mActionEnableEnable = enable;
	mActionEnable->setText(enable ? i18n("Ena&ble") : i18n("Disa&ble"));
}

/******************************************************************************
* Prompt the user to re-enable alarms if they are currently disabled, and if
* it's an email alarm, warn if no 'From' email address is configured.
*/
void KAlarmMainWindow::alarmWarnings(QWidget* parent, const KAEvent* event)
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
	win = create();
	win->show();
	return win;
}

/******************************************************************************
* Find the specified window in the main window list.
*/
bool KAlarmMainWindow::findWindow(KAlarmMainWindow* win)
{
	for (KAlarmMainWindow* w = mWindowList.first();  w;  w = mWindowList.next())
		if (w == win)
			return true;
	return false;
}
