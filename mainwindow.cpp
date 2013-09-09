/*
 *  mainwindow.cpp  -  main application window
 *  Program:  kalarm
 *  Copyright © 2001-2012 by David Jarvie <djarvie@kde.org>
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
#include "mainwindow.moc"

#include "alarmcalendar.h"
#include "alarmlistdelegate.h"
#include "autoqpointer.h"
#include "alarmlistview.h"
#ifndef USE_AKONADI
#include "alarmlistfiltermodel.h"
#include "alarmresources.h"
#include "eventlistmodel.h"
#endif
#include "birthdaydlg.h"
#include "functions.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "messagebox.h"
#include "newalarmaction.h"
#include "prefdlg.h"
#include "preferences.h"
#include "resourceselector.h"
#include "synchtimer.h"
#include "templatedlg.h"
#include "templatemenuaction.h"
#include "templatepickdlg.h"
#include "traywindow.h"
#include "wakedlg.h"

#include <kalarmcal/alarmtext.h>
#include <kalarmcal/kaevent.h>

#include <libkdepim/misc/maillistdrag.h>
#include <kmime/kmime_content.h>
#ifdef USE_AKONADI
#include <kcalcore/memorycalendar.h>
#include <kcalutils/icaldrag.h>
#include <akonadi/control.h>
using namespace KCalCore;
using namespace KCalUtils;
#else
#include <kcal/calendarlocal.h>
#include <kcal/icaldrag.h>
using namespace KCal;
#endif

#include <kmenubar.h>
#include <ktoolbar.h>
#include <kmenu.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kinputdialog.h>
#include <ksystemtrayicon.h>
#include <kstandardaction.h>
#include <kiconloader.h>
#include <kurl.h>
#include <klocale.h>
#include <kglobalsettings.h>
#include <kconfiggroup.h>
#include <kshortcutsdialog.h>
#include <kedittoolbar.h>
#include <kxmlguifactory.h>
#include <kaboutdata.h>
#include <kdebug.h>
#include <ktoggleaction.h>
#include <ktoolbarpopupaction.h>
#include <kicon.h>

#include <QHeaderView>
#include <QSplitter>
#include <QByteArray>
#include <QDesktopWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QTimer>

using namespace KAlarmCal;

static const QString UI_FILE     = QLatin1String("kalarmui.rc");
static const char* WINDOW_NAME = "MainWindow";

static const char* VIEW_GROUP         = "View";
static const char* SHOW_TIME_KEY      = "ShowAlarmTime";
static const char* SHOW_TIME_TO_KEY   = "ShowTimeToAlarm";
static const char* SHOW_ARCHIVED_KEY  = "ShowArchivedAlarms";
static const char* SHOW_RESOURCES_KEY = "ShowResources";

static QString   undoText;
static QString   undoTextStripped;
static KShortcut undoShortcut;
static QString   redoText;
static QString   redoTextStripped;
static KShortcut redoShortcut;


/*=============================================================================
=  Class: MainWindow
=============================================================================*/

MainWindow::WindowList   MainWindow::mWindowList;
TemplateDlg*             MainWindow::mTemplateDlg = 0;

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString MainWindow::i18n_a_ShowAlarmTimes()        { return i18nc("@action", "Show &Alarm Times"); }
QString MainWindow::i18n_chk_ShowAlarmTime()       { return i18nc("@option:check", "Show alarm time"); }
QString MainWindow::i18n_o_ShowTimeToAlarms()      { return i18nc("@action", "Show Time t&o Alarms"); }
QString MainWindow::i18n_chk_ShowTimeToAlarm()     { return i18nc("@option:check", "Show time until alarm"); }


/******************************************************************************
* Construct an instance.
* To avoid resize() events occurring while still opening the calendar (and
* resultant crashes), the calendar is opened before constructing the instance.
*/
MainWindow* MainWindow::create(bool restored)
{
    theApp()->checkCalendar();    // ensure calendar is open
    return new MainWindow(restored);
}

MainWindow::MainWindow(bool restored)
    : MainWindowBase(0, Qt::WindowContextHelpButtonHint),
      mResourcesWidth(-1),
      mHiddenTrayParent(false),
      mShown(false),
      mResizing(false)
{
    kDebug();
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    setObjectName(QLatin1String("MainWin"));    // used by LikeBack
    setPlainCaption(KGlobal::mainComponent().aboutData()->programName());
    KConfigGroup config(KGlobal::config(), VIEW_GROUP);
    mShowResources = config.readEntry(SHOW_RESOURCES_KEY, false);
    mShowArchived  = config.readEntry(SHOW_ARCHIVED_KEY, false);
    mShowTime      = config.readEntry(SHOW_TIME_KEY, true);
    mShowTimeTo    = config.readEntry(SHOW_TIME_TO_KEY, false);
    if (!restored)
    {
        KConfigGroup wconfig(KGlobal::config(), WINDOW_NAME);
        mResourcesWidth = wconfig.readEntry(QString::fromLatin1("Splitter %1").arg(KApplication::desktop()->width()), (int)0);
    }

    setAcceptDrops(true);         // allow drag-and-drop onto this window
    if (!mShowTimeTo)
        mShowTime = true;     // ensure at least one time column is visible

    mSplitter = new QSplitter(Qt::Horizontal, this);
    mSplitter->setChildrenCollapsible(false);
    mSplitter->installEventFilter(this);
    setCentralWidget(mSplitter);

    // Create the calendar resource selector widget
#ifdef USE_AKONADI
    Akonadi::Control::widgetNeedsAkonadi(this);
    mResourceSelector = new ResourceSelector(mSplitter);
#else
    AlarmResources* resources = AlarmResources::instance();
    mResourceSelector = new ResourceSelector(resources, mSplitter);
#endif
    mSplitter->setStretchFactor(0, 0);   // don't resize resource selector when window is resized
    mSplitter->setStretchFactor(1, 1);
#ifndef USE_AKONADI
    connect(resources, SIGNAL(signalErrorMessage(QString)), SLOT(showErrorMessage(QString)));
#endif

    // Create the alarm list widget
#ifdef USE_AKONADI
    mListFilterModel = new AlarmListModel(this);
    mListFilterModel->setEventTypeFilter(mShowArchived ? CalEvent::ACTIVE | CalEvent::ARCHIVED : CalEvent::ACTIVE);
#else
    mListFilterModel = new AlarmListFilterModel(EventListModel::alarms(), this);
    mListFilterModel->setStatusFilter(mShowArchived ? CalEvent::ACTIVE | CalEvent::ARCHIVED : CalEvent::ACTIVE);
#endif
    mListView = new AlarmListView(WINDOW_NAME, mSplitter);
    mListView->setModel(mListFilterModel);
    mListView->selectTimeColumns(mShowTime, mShowTimeTo);
#ifdef USE_AKONADI
    mListView->sortByColumn(mShowTime ? AlarmListModel::TimeColumn : AlarmListModel::TimeToColumn, Qt::AscendingOrder);
#else
    mListView->sortByColumn(mShowTime ? EventListModel::TimeColumn : EventListModel::TimeToColumn, Qt::AscendingOrder);
#endif
    mListView->setItemDelegate(new AlarmListDelegate(mListView));
    connect(mListView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), SLOT(slotSelection()));
    connect(mListView, SIGNAL(contextMenuRequested(QPoint)), SLOT(slotContextMenuRequested(QPoint)));
#ifdef USE_AKONADI
    connect(AkonadiModel::instance(), SIGNAL(collectionStatusChanged(Akonadi::Collection,AkonadiModel::Change,QVariant,bool)),
                       SLOT(slotCalendarStatusChanged()));
#else
    connect(resources, SIGNAL(resourceStatusChanged(AlarmResource*,AlarmResources::Change)),
                       SLOT(slotCalendarStatusChanged()));
#endif
    connect(mResourceSelector, SIGNAL(resized(QSize,QSize)), SLOT(resourcesResized()));
    mListView->installEventFilter(this);
    initActions();

    setAutoSaveSettings(QLatin1String(WINDOW_NAME), true);    // save toolbars, window sizes etc.
    mWindowList.append(this);
    if (mWindowList.count() == 1)
    {
        // It's the first main window
        if (theApp()->wantShowInSystemTray())
            theApp()->displayTrayIcon(true, this);     // create system tray icon for run-in-system-tray mode
        else if (theApp()->trayWindow())
            theApp()->trayWindow()->setAssocMainWindow(this);    // associate this window with the system tray icon
    }
    slotCalendarStatusChanged();   // initialise action states now that window is registered
}

MainWindow::~MainWindow()
{
    kDebug();
    bool trayParent = isTrayParent();   // must call before removing from window list
    mWindowList.removeAt(mWindowList.indexOf(this));

    // Prevent view updates during window destruction
    delete mResourceSelector;
    mResourceSelector = 0;
    delete mListView;
    mListView = 0;

    if (theApp()->trayWindow())
    {
        if (trayParent)
            delete theApp()->trayWindow();
        else
            theApp()->trayWindow()->removeWindow(this);
    }
    KGlobal::config()->sync();    // save any new window size to disc
    theApp()->quitIf();
}

/******************************************************************************
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
void MainWindow::saveProperties(KConfigGroup& config)
{
    config.writeEntry("HiddenTrayParent", isTrayParent() && isHidden());
    config.writeEntry("ShowArchived", mShowArchived);
    config.writeEntry("ShowTime", mShowTime);
    config.writeEntry("ShowTimeTo", mShowTimeTo);
    config.writeEntry("ResourcesWidth", mResourceSelector->isHidden() ? 0 : mResourceSelector->width());
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being
* restored. Read in whatever was saved in saveProperties().
*/
void MainWindow::readProperties(const KConfigGroup& config)
{
    mHiddenTrayParent = config.readEntry("HiddenTrayParent", true);
    mShowArchived     = config.readEntry("ShowArchived", false);
    mShowTime         = config.readEntry("ShowTime", true);
    mShowTimeTo       = config.readEntry("ShowTimeTo", false);
    mResourcesWidth   = config.readEntry("ResourcesWidth", (int)0);
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
* Check whether this main window is effectively the parent of the system tray icon.
*/
bool MainWindow::isTrayParent() const
{
    TrayWindow* tray = theApp()->trayWindow();
    if (!tray  ||  !KSystemTrayIcon::isSystemTrayAvailable())
        return false;
    if (tray->assocMainWindow() == this)
        return true;
    return mWindowList.count() == 1;
}

/******************************************************************************
* Close all main windows.
*/
void MainWindow::closeAll()
{
    while (!mWindowList.isEmpty())
        delete mWindowList[0];    // N.B. the destructor removes the window from the list
}

/******************************************************************************
* Intercept events for the splitter widget.
*/
bool MainWindow::eventFilter(QObject* obj, QEvent* e)
{
    if (obj == mSplitter)
    {
        switch (e->type())
        {
            case QEvent::Resize:
                // Don't change resources size while WINDOW is being resized.
                // Resize event always occurs before Paint.
                mResizing = true;
                break;
            case QEvent::Paint:
                // Allow resources to be resized again
                mResizing = false;
                break;
            default:
                break;
        }
    }
    else if (obj == mListView)
    {
        switch (e->type())
        {
            case QEvent::KeyPress:
            {
                QKeyEvent* ke = static_cast<QKeyEvent*>(e);
                if (ke->key() == Qt::Key_Delete  &&  ke->modifiers() == Qt::ShiftModifier)
                {
                    // Prevent Shift-Delete being processed by EventListDelegate
                    mActionDeleteForce->trigger();
                    return true;
                }
                break;
            }
            default:
                break;
        }
    }
    return false;
}

/******************************************************************************
* Called when the window's size has changed (before it is painted).
* Sets the last column in the list view to extend at least to the right hand
* edge of the list view.
* Records the new size in the config file.
*/
void MainWindow::resizeEvent(QResizeEvent* re)
{
    // Save the window's new size only if it's the first main window
    MainWindowBase::resizeEvent(re);
    if (mResourcesWidth > 0)
    {
        QList<int> widths;
        widths.append(mResourcesWidth);
        widths.append(width() - mResourcesWidth - mSplitter->handleWidth());
        mSplitter->setSizes(widths);
    }
}

void MainWindow::resourcesResized()
{
    if (!mShown  ||  mResizing)
        return;
    QList<int> widths = mSplitter->sizes();
    if (widths.count() > 1)
    {
        mResourcesWidth = widths[0];
        // Width is reported as non-zero when resource selector is
        // actually invisible, so note a zero width in these circumstances.
        if (mResourcesWidth <= 5)
            mResourcesWidth = 0;
        else if (mainMainWindow() == this)
        {
            KConfigGroup config(KGlobal::config(), WINDOW_NAME);
            config.writeEntry(QString::fromLatin1("Splitter %1").arg(KApplication::desktop()->width()), mResourcesWidth);
        }
    }
}

/******************************************************************************
* Called when the window is first displayed.
* Sets the last column in the list view to extend at least to the right hand
* edge of the list view.
*/
void MainWindow::showEvent(QShowEvent* se)
{
    if (mResourcesWidth > 0)
    {
        QList<int> widths;
        widths.append(mResourcesWidth);
        widths.append(width() - mResourcesWidth - mSplitter->handleWidth());
        mSplitter->setSizes(widths);
    }
    MainWindowBase::showEvent(se);
    mShown = true;
}

/******************************************************************************
* Display the window.
*/
void MainWindow::show()
{
    MainWindowBase::show();
    if (mMenuError)
    {
        // Show error message now that the main window has been displayed.
        // Waiting until now lets the user easily associate the message with
        // the main window which is faulty.
        KAMessageBox::error(this, i18nc("@info", "Failure to create menus (perhaps <filename>%1</filename> missing or corrupted)", UI_FILE));
        mMenuError = false;
    }
}

/******************************************************************************
* Called after the window is hidden.
*/
void MainWindow::hideEvent(QHideEvent* he)
{
    MainWindowBase::hideEvent(he);
}

/******************************************************************************
* Initialise the menu, toolbar and main window actions.
*/
void MainWindow::initActions()
{
    KShortcut dummy;
    KActionCollection* actions = actionCollection();

    mActionTemplates = new KAction(i18nc("@action", "&Templates..."), this);
    actions->addAction(QLatin1String("templates"), mActionTemplates);
    connect(mActionTemplates, SIGNAL(triggered(bool)), SLOT(slotTemplates()));

    mActionNew = new NewAlarmAction(false, i18nc("@action", "&New"), this);
    actions->addAction(QLatin1String("new"), mActionNew);

    KAction* action = mActionNew->displayAlarmAction();
    actions->addAction(QLatin1String("newDisplay"), action);
    action->setGlobalShortcut(dummy);   // actions->addAction() must be called first!
    connect(action, SIGNAL(triggered(bool)), SLOT(slotNewDisplay()));

    action = mActionNew->commandAlarmAction();
    actions->addAction(QLatin1String("newCommand"), action);
    action->setGlobalShortcut(dummy);   // actions->addAction() must be called first!
    connect(action, SIGNAL(triggered(bool)), SLOT(slotNewCommand()));

    action = mActionNew->emailAlarmAction();
    actions->addAction(QLatin1String("newEmail"), action);
    action->setGlobalShortcut(dummy);   // actions->addAction() must be called first!
    connect(action, SIGNAL(triggered(bool)), SLOT(slotNewEmail()));

    action = mActionNew->audioAlarmAction();
    actions->addAction(QLatin1String("newAudio"), action);
    action->setGlobalShortcut(dummy);   // actions->addAction() must be called first!
    connect(action, SIGNAL(triggered(bool)), SLOT(slotNewAudio()));

    action = mActionNew->fromTemplateAlarmAction();
    actions->addAction(QLatin1String("newFromTemplate"), action);
    connect(action, SIGNAL(selected(const KAEvent*)), SLOT(slotNewFromTemplate(const KAEvent*)));

    mActionCreateTemplate = new KAction(i18nc("@action", "Create Tem&plate..."), this);
    actions->addAction(QLatin1String("createTemplate"), mActionCreateTemplate);
    connect(mActionCreateTemplate, SIGNAL(triggered(bool)), SLOT(slotNewTemplate()));

    mActionCopy = new KAction(KIcon(QLatin1String("edit-copy")), i18nc("@action", "&Copy..."), this);
    actions->addAction(QLatin1String("copy"), mActionCopy);
    mActionCopy->setShortcut(QKeySequence(Qt::SHIFT + Qt::Key_Insert));
    connect(mActionCopy, SIGNAL(triggered(bool)), SLOT(slotCopy()));

    mActionModify = new KAction(KIcon(QLatin1String("document-properties")), i18nc("@action", "&Edit..."), this);
    actions->addAction(QLatin1String("modify"), mActionModify);
    mActionModify->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
    connect(mActionModify, SIGNAL(triggered(bool)), SLOT(slotModify()));

    mActionDelete = new KAction(KIcon(QLatin1String("edit-delete")), i18nc("@action", "&Delete"), this);
    actions->addAction(QLatin1String("delete"), mActionDelete);
    mActionDelete->setShortcut(QKeySequence::Delete);
    connect(mActionDelete, SIGNAL(triggered(bool)), SLOT(slotDeleteIf()));

    // Set up Shift-Delete as a shortcut to delete without confirmation
    mActionDeleteForce = new KAction(i18nc("@action", "Delete Without Confirmation"), this);
    actions->addAction(QLatin1String("delete-force"), mActionDeleteForce);
    mActionDeleteForce->setShortcut(QKeySequence::Delete + Qt::SHIFT);
    connect(mActionDeleteForce, SIGNAL(triggered(bool)), SLOT(slotDeleteForce()));

    mActionReactivate = new KAction(i18nc("@action", "Reac&tivate"), this);
    actions->addAction(QLatin1String("undelete"), mActionReactivate);
    mActionReactivate->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
    connect(mActionReactivate, SIGNAL(triggered(bool)), SLOT(slotReactivate()));

    mActionEnable = new KAction(this);
    actions->addAction(QLatin1String("disable"), mActionEnable);
    mActionEnable->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_B));
    connect(mActionEnable, SIGNAL(triggered(bool)), SLOT(slotEnable()));

    action = new KAction(i18nc("@action", "Wake From Suspend..."), this);
    actions->addAction(QLatin1String("wakeSuspend"), action);
    connect(action, SIGNAL(triggered(bool)), SLOT(slotWakeFromSuspend()));

    action = KAlarm::createStopPlayAction(this);
    actions->addAction(QLatin1String("stopAudio"), action);
    action->setGlobalShortcut(dummy);   // actions->addAction() must be called first!

    mActionShowTime = new KToggleAction(i18n_a_ShowAlarmTimes(), this);
    actions->addAction(QLatin1String("showAlarmTimes"), mActionShowTime);
    connect(mActionShowTime, SIGNAL(triggered(bool)), SLOT(slotShowTime()));

    mActionShowTimeTo = new KToggleAction(i18n_o_ShowTimeToAlarms(), this);
    actions->addAction(QLatin1String("showTimeToAlarms"), mActionShowTimeTo);
    mActionShowTimeTo->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_I));
    connect(mActionShowTimeTo, SIGNAL(triggered(bool)), SLOT(slotShowTimeTo()));

    mActionShowArchived = new KToggleAction(i18nc("@action", "Show Archi&ved Alarms"), this);
    actions->addAction(QLatin1String("showArchivedAlarms"), mActionShowArchived);
    mActionShowArchived->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_P));
    connect(mActionShowArchived, SIGNAL(triggered(bool)), SLOT(slotShowArchived()));

    mActionToggleTrayIcon = new KToggleAction(i18nc("@action", "Show in System &Tray"), this);
    actions->addAction(QLatin1String("showInSystemTray"), mActionToggleTrayIcon);
    connect(mActionToggleTrayIcon, SIGNAL(triggered(bool)), SLOT(slotToggleTrayIcon()));

    mActionToggleResourceSel = new KToggleAction(KIcon(QLatin1String("view-choose")), i18nc("@action", "Show &Calendars"), this);
    actions->addAction(QLatin1String("showResources"), mActionToggleResourceSel);
    connect(mActionToggleResourceSel, SIGNAL(triggered(bool)), SLOT(slotToggleResourceSelector()));

    mActionSpreadWindows = KAlarm::createSpreadWindowsAction(this);
    actions->addAction(QLatin1String("spread"), mActionSpreadWindows);
    mActionSpreadWindows->setGlobalShortcut(dummy);   // actions->addAction() must be called first!

    mActionImportAlarms = new KAction(i18nc("@action", "Import &Alarms..."), this);
    actions->addAction(QLatin1String("importAlarms"), mActionImportAlarms);
    connect(mActionImportAlarms, SIGNAL(triggered(bool)), SLOT(slotImportAlarms()));

    mActionImportBirthdays = new KAction(i18nc("@action", "Import &Birthdays..."), this);
    actions->addAction(QLatin1String("importBirthdays"), mActionImportBirthdays);
    connect(mActionImportBirthdays, SIGNAL(triggered(bool)), SLOT(slotBirthdays()));

    mActionExportAlarms = new KAction(i18nc("@action", "E&xport Selected Alarms..."), this);
    actions->addAction(QLatin1String("exportAlarms"), mActionExportAlarms);
    connect(mActionExportAlarms, SIGNAL(triggered(bool)), SLOT(slotExportAlarms()));

    mActionExport = new KAction(i18nc("@action", "E&xport..."), this);
    actions->addAction(QLatin1String("export"), mActionExport);
    connect(mActionExport, SIGNAL(triggered(bool)), SLOT(slotExportAlarms()));

    action = new KAction(KIcon(QLatin1String("view-refresh")), i18nc("@action", "&Refresh Alarms"), this);
    actions->addAction(QLatin1String("refreshAlarms"), action);
    connect(action, SIGNAL(triggered(bool)), SLOT(slotRefreshAlarms()));

    action = KAlarm::createAlarmEnableAction(this);
    actions->addAction(QLatin1String("alarmsEnable"), action);
    if (undoText.isNull())
    {
        // Get standard texts, etc., for Undo and Redo actions
        QAction * act = KStandardAction::undo(this, 0, actions);
        undoShortcut     = KShortcut(act->shortcuts());
        undoText         = act->text();
        undoTextStripped = KGlobal::locale()->removeAcceleratorMarker(undoText);
        delete act;
        act = KStandardAction::redo(this, 0, actions);
        redoShortcut     = KShortcut(act->shortcuts());
        redoText         = act->text();
        redoTextStripped = KGlobal::locale()->removeAcceleratorMarker(redoText);
        delete act;
    }
    mActionUndo = new KToolBarPopupAction(KIcon(QLatin1String("edit-undo")), undoText, this);
    actions->addAction(QLatin1String("edit_undo"), mActionUndo);
    mActionUndo->setShortcut(undoShortcut);
    connect(mActionUndo, SIGNAL(triggered(bool)), SLOT(slotUndo()));

    mActionRedo = new KToolBarPopupAction(KIcon(QLatin1String("edit-redo")), redoText, this);
    actions->addAction(QLatin1String("edit_redo"), mActionRedo);
    mActionRedo->setShortcut(redoShortcut);
    connect(mActionRedo, SIGNAL(triggered(bool)), SLOT(slotRedo()));

    KStandardAction::find(mListView, SLOT(slotFind()), actions);
    mActionFindNext = KStandardAction::findNext(mListView, SLOT(slotFindNext()), actions);
    mActionFindPrev = KStandardAction::findPrev(mListView, SLOT(slotFindPrev()), actions);
    KStandardAction::selectAll(mListView, SLOT(selectAll()), actions);
    KStandardAction::deselect(mListView, SLOT(clearSelection()), actions);
    // Quit only once the event loop is called; otherwise, the parent window will
    // be deleted while still processing the action, resulting in a crash.
    KAction* act = KStandardAction::quit(0, 0, actions);
    connect(act, SIGNAL(triggered(bool)), SLOT(slotQuit()), Qt::QueuedConnection);
    KStandardAction::keyBindings(this, SLOT(slotConfigureKeys()), actions);
    KStandardAction::configureToolbars(this, SLOT(slotConfigureToolbar()), actions);
    KStandardAction::preferences(this, SLOT(slotPreferences()), actions);
    mResourceSelector->initActions(actions);
    setStandardToolBarMenuEnabled(true);
    createGUI(UI_FILE);
    // Load menu and toolbar settings
    applyMainWindowSettings(KGlobal::config()->group(WINDOW_NAME));

    mContextMenu = static_cast<KMenu*>(factory()->container(QLatin1String("listContext"), this));
    mActionsMenu = static_cast<KMenu*>(factory()->container(QLatin1String("actions"), this));
    KMenu* resourceMenu = static_cast<KMenu*>(factory()->container(QLatin1String("resourceContext"), this));
    mResourceSelector->setContextMenu(resourceMenu);
    mMenuError = (!mContextMenu  ||  !mActionsMenu  ||  !resourceMenu);
    connect(mActionUndo->menu(), SIGNAL(aboutToShow()), SLOT(slotInitUndoMenu()));
    connect(mActionUndo->menu(), SIGNAL(triggered(QAction*)), SLOT(slotUndoItem(QAction*)));
    connect(mActionRedo->menu(), SIGNAL(aboutToShow()), SLOT(slotInitRedoMenu()));
    connect(mActionRedo->menu(), SIGNAL(triggered(QAction*)), SLOT(slotRedoItem(QAction*)));
    connect(Undo::instance(), SIGNAL(changed(QString,QString)), SLOT(slotUndoStatus(QString,QString)));
    connect(mListView, SIGNAL(findActive(bool)), SLOT(slotFindActive(bool)));
    Preferences::connect(SIGNAL(archivedKeepDaysChanged(int)), this, SLOT(updateKeepArchived(int)));
    Preferences::connect(SIGNAL(showInSystemTrayChanged(bool)), this, SLOT(updateTrayIconAction()));
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
    mActionEnable->setEnabled(false);
    mActionCreateTemplate->setEnabled(false);
    mActionExport->setEnabled(false);

    Undo::emitChanged();     // set the Undo/Redo menu texts
//    Daemon::monitoringAlarms();
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
    kDebug();
#ifdef USE_AKONADI
    AkonadiModel::instance()->reload();
#else
    EventListModel::alarms()->reload();
#endif
}

/******************************************************************************
* Called when the keep archived alarm setting changes in the user preferences.
* Enable/disable Show archived alarms option.
*/
void MainWindow::updateKeepArchived(int days)
{
    kDebug() << (bool)days;
    if (mShowArchived  &&  !days)
        slotShowArchived();   // toggle Show Archived option setting
    mActionShowArchived->setEnabled(days);
}

/******************************************************************************
* Select an alarm in the displayed list.
*/
#ifdef USE_AKONADI
void MainWindow::selectEvent(Akonadi::Item::Id eventId)
#else
void MainWindow::selectEvent(const QString& eventId)
#endif
{
    mListView->clearSelection();
#ifdef USE_AKONADI
    QModelIndex index = mListFilterModel->eventIndex(eventId);
    if (index.isValid())
    {
        mListView->select(index);
        mListView->scrollTo(index);
    }
#else
    mListView->select(eventId, true);
#endif
}

/******************************************************************************
* Return the single selected alarm in the displayed list.
*/
#ifdef USE_AKONADI
KAEvent MainWindow::selectedEvent() const
#else
KAEvent* MainWindow::selectedEvent() const
#endif
{
    return mListView->selectedEvent();
}

/******************************************************************************
* Deselect all alarms in the displayed list.
*/
void MainWindow::clearSelection()
{
    mListView->clearSelection();
}

/******************************************************************************
* Called when the New button is clicked to edit a new alarm to add to the list.
*/
void MainWindow::slotNew(EditAlarmDlg::Type type)
{
    KAlarm::editNewAlarm(type, mListView);
}

/******************************************************************************
* Called when a template is selected from the New From Template popup menu.
* Executes a New Alarm dialog, preset from the selected template.
*/
void MainWindow::slotNewFromTemplate(const KAEvent* tmplate)
{
    KAlarm::editNewAlarm(tmplate, mListView);
}

/******************************************************************************
* Called when the New Template button is clicked to create a new template
* based on the currently selected alarm.
*/
void MainWindow::slotNewTemplate()
{
#ifdef USE_AKONADI
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editNewTemplate(&event, this);
#else
    KAEvent* event = mListView->selectedEvent();
    if (event)
        KAlarm::editNewTemplate(event, this);
#endif
}

/******************************************************************************
* Called when the Copy button is clicked to edit a copy of an existing alarm,
* to add to the list.
*/
void MainWindow::slotCopy()
{
#ifdef USE_AKONADI
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editNewAlarm(&event, this);
#else
    KAEvent* event = mListView->selectedEvent();
    if (event)
        KAlarm::editNewAlarm(event, this);
#endif
}

/******************************************************************************
* Called when the Modify button is clicked to edit the currently highlighted
* alarm in the list.
*/
void MainWindow::slotModify()
{
#ifdef USE_AKONADI
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editAlarm(&event, this);   // edit alarm (view-only mode if archived or read-only)
#else
    KAEvent* event = mListView->selectedEvent();
    if (event)
        KAlarm::editAlarm(event, this);   // edit alarm (view-only mode if archived or read-only)
#endif
}

/******************************************************************************
* Called when the Delete button is clicked to delete the currently highlighted
* alarms in the list.
*/
void MainWindow::slotDelete(bool force)
{
#ifdef USE_AKONADI
    QVector<KAEvent> events = mListView->selectedEvents();
#else
    KAEvent::List events = mListView->selectedEvents();
    // Save the IDs of the events to be deleted, in case any events are
    // deleted by being triggered while the confirmation prompt is displayed
    // (in which case their pointers will become invalid).
    QStringList ids;
    for (int i = 0, end = events.count();  i < end;  ++i)
        ids.append(events[i]->id());
#endif

    if (!force  &&  Preferences::confirmAlarmDeletion())
    {
        int n = events.count();
        if (KAMessageBox::warningContinueCancel(this, i18ncp("@info", "Do you really want to delete the selected alarm?",
                                                             "Do you really want to delete the %1 selected alarms?", n),
                                                i18ncp("@title:window", "Delete Alarm", "Delete Alarms", n),
                                                KGuiItem(i18nc("@action:button", "&Delete"), QLatin1String("edit-delete")),
                                                KStandardGuiItem::cancel(),
                                                QLatin1String(Preferences::CONFIRM_ALARM_DELETION))
            != KMessageBox::Continue)
            return;
    }

    // Remove any events which have just triggered, from the list to delete.
    Undo::EventList undos;
    AlarmCalendar* resources = AlarmCalendar::resources();
#ifdef USE_AKONADI
    for (int i = 0;  i < events.count();  )
    {
        Akonadi::Collection c = resources->collectionForEvent(events[i].itemId());
        if (!c.isValid())
            events.remove(i);
        else
            undos.append(events[i++], c);
    }
#else
    for (int i = 0, e = 0, end = ids.count();  i < end;  ++i)
    {
        AlarmResource* r = resources->resourceForEvent(ids[i]);
        if (!r)
            events.remove(e);
        else
            undos.append(*events[e++], r);
    }
#endif

    if (events.isEmpty())
        kDebug() << "No alarms left to delete";
    else
    {
        // Delete the events from the calendar and displays
        KAlarm::deleteEvents(events, true, this);
        Undo::saveDeletes(undos);
    }
}

/******************************************************************************
* Called when the Reactivate button is clicked to reinstate the currently
* highlighted archived alarms in the list.
*/
void MainWindow::slotReactivate()
{
#ifdef USE_AKONADI
    QVector<KAEvent> events = mListView->selectedEvents();
#else
    KAEvent::List events = mListView->selectedEvents();
#endif
    mListView->clearSelection();

    // Add the alarms to the displayed lists and to the calendar file
    Undo::EventList undos;
#ifdef USE_AKONADI
    QVector<EventId> ineligibleIDs;
    KAlarm::reactivateEvents(events, ineligibleIDs, 0, this);
#else
    QStringList ineligibleIDs;
    KAlarm::reactivateEvents(events, ineligibleIDs, 0, this);
#endif

    // Create the undo list, excluding ineligible events
    AlarmCalendar* resources = AlarmCalendar::resources();
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
#ifdef USE_AKONADI
        if (!ineligibleIDs.contains(EventId(events[i])))
        {
            undos.append(events[i], resources->collectionForEvent(events[i].itemId()));
        }
#else
        QString id = events[i]->id();
        if (!ineligibleIDs.contains(id))
            undos.append(*events[i], resources->resourceForEvent(id));
#endif
    }
    Undo::saveReactivates(undos);
}

/******************************************************************************
* Called when the Enable/Disable button is clicked to enable or disable the
* currently highlighted alarms in the list.
*/
void MainWindow::slotEnable()
{
    bool enable = mActionEnableEnable;    // save since changed in response to KAlarm::enableEvent()
#ifdef USE_AKONADI
    QVector<KAEvent> events = mListView->selectedEvents();
    QVector<KAEvent> eventCopies;
#else
    KAEvent::List events = mListView->selectedEvents();
    KAEvent::List eventCopies;
#endif
    for (int i = 0, end = events.count();  i < end;  ++i)
        eventCopies += events[i];
    KAlarm::enableEvents(eventCopies, enable, this);
    slotSelection();   // update Enable/Disable action text
}

/******************************************************************************
* Called when the Show Alarm Times menu item is selected or deselected.
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
        KConfigGroup config(KGlobal::config(), VIEW_GROUP);
        config.writeEntry(SHOW_TIME_KEY, mShowTime);
        config.writeEntry(SHOW_TIME_TO_KEY, mShowTimeTo);
    }
}

/******************************************************************************
* Called when the Show Time To Alarms menu item is selected or deselected.
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
        KConfigGroup config(KGlobal::config(), VIEW_GROUP);
        config.writeEntry(SHOW_TIME_KEY, mShowTime);
        config.writeEntry(SHOW_TIME_TO_KEY, mShowTimeTo);
    }
}

/******************************************************************************
* Called when the Show Archived Alarms menu item is selected or deselected.
*/
void MainWindow::slotShowArchived()
{
    mShowArchived = !mShowArchived;
    mActionShowArchived->setChecked(mShowArchived);
    mActionShowArchived->setToolTip(mShowArchived ? i18nc("@info:tooltip", "Hide Archived Alarms")
                                                  : i18nc("@info:tooltip", "Show Archived Alarms"));
#ifdef USE_AKONADI
    mListFilterModel->setEventTypeFilter(mShowArchived ? CalEvent::ACTIVE | CalEvent::ARCHIVED : CalEvent::ACTIVE);
#else
    mListFilterModel->setStatusFilter(mShowArchived ? CalEvent::ACTIVE | CalEvent::ARCHIVED : CalEvent::ACTIVE);
#endif
    mListView->reset();
    KConfigGroup config(KGlobal::config(), VIEW_GROUP);
    config.writeEntry(SHOW_ARCHIVED_KEY, mShowArchived);
}

/******************************************************************************
* Called when the Spread Windows global shortcut is selected, to spread alarm
* windows so that they are all visible.
*/
void MainWindow::slotSpreadWindowsShortcut()
{
    mActionSpreadWindows->trigger();
}

/******************************************************************************
* Called when the Wake From Suspend menu option is selected.
*/
void MainWindow::slotWakeFromSuspend()
{
    (WakeFromSuspendDlg::create(this))->show();
}

/******************************************************************************
* Called when the Import Alarms menu item is selected, to merge alarms from an
* external calendar into the current calendars.
*/
void MainWindow::slotImportAlarms()
{
    AlarmCalendar::importAlarms(this);
}

/******************************************************************************
* Called when the Export Alarms menu item is selected, to export the selected
* alarms to an external calendar.
*/
void MainWindow::slotExportAlarms()
{
#ifdef USE_AKONADI
    QVector<KAEvent> events = mListView->selectedEvents();
    if (!events.isEmpty())
    {
        KAEvent::List evts = KAEvent::ptrList(events);
        AlarmCalendar::exportAlarms(evts, this);
    }
#else
    KAEvent::List events = mListView->selectedEvents();
    if (!events.isEmpty())
        AlarmCalendar::exportAlarms(events, this);
#endif
}

/******************************************************************************
* Called when the Import Birthdays menu item is selected, to display birthdays
* from the address book for selection as alarms.
*/
void MainWindow::slotBirthdays()
{
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of MainWindow, and on return from this function).
    AutoQPointer<BirthdayDlg> dlg = new BirthdayDlg(this);
    if (dlg->exec() == QDialog::Accepted)
    {
        QVector<KAEvent> events = dlg->events();
        if (!events.isEmpty())
        {
            mListView->clearSelection();
            // Add alarm to the displayed lists and to the calendar file
            KAlarm::UpdateStatus status = KAlarm::addEvents(events, dlg, true, true);

            Undo::EventList undos;
            AlarmCalendar* resources = AlarmCalendar::resources();
            for (int i = 0, end = events.count();  i < end;  ++i)
#ifdef USE_AKONADI
            {
                Akonadi::Collection c = resources->collectionForEvent(events[i].itemId());
                undos.append(events[i], c);
            }
#else
                undos.append(events[i], resources->resourceForEvent(events[i].id()));
#endif
            Undo::saveAdds(undos, i18nc("@info", "Import birthdays"));

            if (status != KAlarm::UPDATE_FAILED)
                KAlarm::outputAlarmWarnings(dlg);
        }
    }
}

/******************************************************************************
* Called when the Templates menu item is selected, to display the alarm
* template editing dialog.
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
* Called when the alarm template editing dialog has exited.
*/
void MainWindow::slotTemplatesEnd()
{
    if (mTemplateDlg)
    {
        mTemplateDlg->delayedDestruct();   // this deletes the dialog once it is safe to do so
        mTemplateDlg = 0;
        enableTemplateMenuItem(true);      // re-enable menu item in all windows
    }
}

/******************************************************************************
* Called when the Display System Tray Icon menu item is selected.
*/
void MainWindow::slotToggleTrayIcon()
{
    theApp()->displayTrayIcon(!theApp()->trayIconDisplayed(), this);
}

/******************************************************************************
* Called when the Show Resource Selector menu item is selected.
*/
void MainWindow::slotToggleResourceSelector()
{
    mShowResources = mActionToggleResourceSel->isChecked();
    if (mShowResources)
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

    KConfigGroup config(KGlobal::config(), VIEW_GROUP);
    config.writeEntry(SHOW_RESOURCES_KEY, mShowResources);
}

/******************************************************************************
* Called when an error occurs in the resource calendar, to display a message.
*/
void MainWindow::showErrorMessage(const QString& msg)
{
    KAMessageBox::error(this, msg);
}

/******************************************************************************
* Called when the system tray icon is created or destroyed.
* Set the system tray icon menu text according to whether or not the system
* tray icon is currently visible.
*/
void MainWindow::updateTrayIconAction()
{
    mActionToggleTrayIcon->setEnabled(KSystemTrayIcon::isSystemTrayAvailable());
    mActionToggleTrayIcon->setChecked(theApp()->trayIconDisplayed());
}

/******************************************************************************
* Called when the active status of Find changes.
*/
void MainWindow::slotFindActive(bool active)
{
    mActionFindNext->setEnabled(active);
    mActionFindPrev->setEnabled(active);
}

/******************************************************************************
* Called when the Undo action is selected.
*/
void MainWindow::slotUndo()
{
    Undo::undo(this, KGlobal::locale()->removeAcceleratorMarker(mActionUndo->text()));
}

/******************************************************************************
* Called when the Redo action is selected.
*/
void MainWindow::slotRedo()
{
    Undo::redo(this, KGlobal::locale()->removeAcceleratorMarker(mActionRedo->text()));
}

/******************************************************************************
* Called when an Undo item is selected.
*/
void MainWindow::slotUndoItem(QAction* action)
{
    int id = mUndoMenuIds[action];
    Undo::undo(id, this, Undo::actionText(Undo::UNDO, id));
}

/******************************************************************************
* Called when a Redo item is selected.
*/
void MainWindow::slotRedoItem(QAction* action)
{
    int id = mUndoMenuIds[action];
    Undo::redo(id, this, Undo::actionText(Undo::REDO, id));
}

/******************************************************************************
* Called when the Undo menu is about to show.
* Populates the menu.
*/
void MainWindow::slotInitUndoMenu()
{
    initUndoMenu(mActionUndo->menu(), Undo::UNDO);
}

/******************************************************************************
* Called when the Redo menu is about to show.
* Populates the menu.
*/
void MainWindow::slotInitRedoMenu()
{
    initUndoMenu(mActionRedo->menu(), Undo::REDO);
}

/******************************************************************************
* Populate the undo or redo menu.
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
        QString actText = Undo::actionText(type, id);
        QString descrip = Undo::description(type, id);
        QString text = descrip.isEmpty()
                     ? i18nc("@action Undo/Redo [action]", "%1 %2", action, actText)
                     : i18nc("@action Undo [action]: message", "%1 %2: %3", action, actText, descrip);
        QAction* act = menu->addAction(text);
        mUndoMenuIds[act] = id;
    }
}

/******************************************************************************
* Called when the status of the Undo or Redo list changes.
* Change the Undo or Redo text to include the action which would be undone/redone.
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
        mActionUndo->setText(QString::fromLatin1("%1 %2").arg(undoText).arg(undo));
    }
    if (redo.isNull())
    {
        mActionRedo->setEnabled(false);
        mActionRedo->setText(redoText);
    }
    else
    {
        mActionRedo->setEnabled(true);
        mActionRedo->setText(QString::fromLatin1("%1 %2").arg(redoText).arg(redo));
    }
}

/******************************************************************************
* Called when the Refresh Alarms menu item is selected.
*/
void MainWindow::slotRefreshAlarms()
{
    KAlarm::refreshAlarms();
}

/******************************************************************************
* Called when the "Configure KAlarm" menu item is selected.
*/
void MainWindow::slotPreferences()
{
    KAlarmPrefDlg::display();
}

/******************************************************************************
* Called when the Configure Keys menu item is selected.
*/
void MainWindow::slotConfigureKeys()
{
    KShortcutsDialog::configure(actionCollection(), KShortcutsEditor::LetterShortcutsAllowed, this);
}

/******************************************************************************
* Called when the Configure Toolbars menu item is selected.
*/
void MainWindow::slotConfigureToolbar()
{
    saveMainWindowSettings(KGlobal::config()->group(WINDOW_NAME));
    KEditToolBar dlg(factory());
    connect(&dlg, SIGNAL(newToolBarConfig()), this, SLOT(slotNewToolbarConfig()));
    dlg.exec();
}

/******************************************************************************
* Called when OK or Apply is clicked in the Configure Toolbars dialog, to save
* the new configuration.
*/
void MainWindow::slotNewToolbarConfig()
{
    createGUI(UI_FILE);
    applyMainWindowSettings(KGlobal::config()->group(WINDOW_NAME));
}

/******************************************************************************
* Called when the Quit menu item is selected.
* Note that this must be called by the event loop, not directly from the menu
* item, since otherwise the window will be deleted while still processing the
* menu, resulting in a crash.
*/
void MainWindow::slotQuit()
{
    theApp()->doQuit(this);
}

/******************************************************************************
* Called when the user or the session manager attempts to close the window.
*/
void MainWindow::closeEvent(QCloseEvent* ce)
{
    if (!theApp()->sessionClosingDown())
    {
        // The user (not the session manager) wants to close the window.
        if (isTrayParent())
        {
            // It's the parent window of the system tray icon, so just hide
            // it to prevent the system tray icon closing.
            hide();
            theApp()->quitIf();
            ce->ignore();
            return;
        }
    }
    ce->accept();
}

/******************************************************************************
* Called when the drag cursor enters a main or system tray window, to accept
* or reject the dragged object.
*/
void MainWindow::executeDragEnterEvent(QDragEnterEvent* e)
{
    const QMimeData* data = e->mimeData();
    bool accept = ICalDrag::canDecode(data) ? !e->source()   // don't accept "text/calendar" objects from this application
                                               :    data->hasText()
                                                 || KUrl::List::canDecode(data)
                                                 || KPIM::MailList::canDecode(data);
    if (accept)
        e->acceptProposedAction();
}

/******************************************************************************
* Called when an object is dropped on the window.
* If the object is recognised, the edit alarm dialog is opened appropriately.
*/
void MainWindow::dropEvent(QDropEvent* e)
{
    executeDropEvent(this, e);
}

static QString getMailHeader(const char* header, KMime::Content& content)
{
    KMime::Headers::Base* hd = content.headerByType(header);
    return hd ? hd->asUnicodeString() : QString();
}

/******************************************************************************
* Called when an object is dropped on a main or system tray window, to
* evaluate the action required and extract the text.
*/
void MainWindow::executeDropEvent(MainWindow* win, QDropEvent* e)
{
    kDebug() << "Formats:" << e->mimeData()->formats();
    const QMimeData* data = e->mimeData();
    KAEvent::SubAction action = KAEvent::MESSAGE;
    QByteArray         bytes;
    AlarmText          alarmText;
    KPIM::MailList     mailList;
    KUrl::List         files;
#ifdef USE_AKONADI
    MemoryCalendar::Ptr calendar(new MemoryCalendar(Preferences::timeZone(true)));
#else
    CalendarLocal calendar(Preferences::timeZone(true));
#endif
#ifndef NDEBUG
    QString fmts = data->formats().join(QLatin1String(", "));
    kDebug() << fmts;
#endif

    /* The order of the tests below matters, since some dropped objects
     * provide more than one mime type.
     * Don't change them without careful thought !!
     */
    if (!(bytes = data->data(QLatin1String("message/rfc822"))).isEmpty())
    {
        // Email message(s). Ignore all but the first.
        kDebug() << "email";
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
#ifdef KMAIL_SUPPORTED
    else if (KPIM::MailList::canDecode(data))
    {
        mailList = KPIM::MailList::fromMimeData(data);
        // KMail message(s). Ignore all but the first.
        kDebug() << "KMail_list";
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
#endif
#ifdef USE_AKONADI
    else if (ICalDrag::fromMimeData(data, calendar))
#else
    else if (ICalDrag::fromMimeData(data, &calendar))
#endif
    {
        // iCalendar - If events are included, use the first event
        kDebug() << "iCalendar";
#ifdef USE_AKONADI
        Event::List events = calendar->rawEvents();
#else
        Event::List events = calendar.rawEvents();
#endif
        if (!events.isEmpty())
        {
            KAEvent ev(events[0]);
            KAlarm::editNewAlarm(&ev, win);
            return;
        }
        // If todos are included, use the first todo
#ifdef USE_AKONADI
        Todo::List todos = calendar->rawTodos();
#else
        Todo::List todos = calendar.rawTodos();
#endif
        if (todos.isEmpty())
            return;
#ifdef USE_AKONADI
        Todo::Ptr todo = todos[0];
#else
        Todo* todo = todos[0];
#endif
        alarmText.setTodo(todo);
        KDateTime start = todo->dtStart(true);
        if (!start.isValid()  &&  todo->hasDueDate())
            start = todo->dtDue(true);
        KAEvent::Flags flags = KAEvent::DEFAULT_FONT;
        if (start.isDateOnly())
            flags |= KAEvent::ANY_TIME;
        KAEvent ev(start, alarmText.displayText(), Preferences::defaultBgColour(), Preferences::defaultFgColour(),
                   QFont(), KAEvent::MESSAGE, 0, flags, true);
        if (todo->recurs())
        {
            ev.setRecurrence(*todo->recurrence());
            ev.setNextOccurrence(KDateTime::currentUtcDateTime());
        }
        ev.endChanges();
        KAlarm::editNewAlarm(&ev, win);
        return;
    }
    else if (!(files = KUrl::List::fromMimeData(data)).isEmpty())
    {
        kDebug() << "URL";
        // Try to find the mime type of the file, without downloading a remote file
        KMimeType::Ptr mimeType = KMimeType::findByUrl(files[0]);
        action = mimeType->name().startsWith(QLatin1String("audio/")) ? KAEvent::AUDIO : KAEvent::FILE;
        alarmText.setText(files[0].prettyUrl());
    }
    else if (data->hasText())
    {
        QString text = data->text();
        kDebug() << "text";
        alarmText.setText(text);
    }
    else
        return;

    if (!alarmText.isEmpty())
    {
        if (action == KAEvent::MESSAGE
        &&  (alarmText.isEmail() || alarmText.isScript()))
        {
            // If the alarm text could be interpreted as an email or command script,
            // prompt for which type of alarm to create.
            QStringList types;
            types += i18nc("@item:inlistbox", "Display Alarm");
            if (alarmText.isEmail())
                types += i18nc("@item:inlistbox", "Email Alarm");
            else if (alarmText.isScript())
                types += i18nc("@item:inlistbox", "Command Alarm");
            bool ok = false;
            QString type = KInputDialog::getItem(i18nc("@title:window", "Alarm Type"),
                                                 i18nc("@info", "Choose alarm type to create:"), types, 0, false, &ok, mainMainWindow());
            if (!ok)
                return;   // user didn't press OK
            int i = types.indexOf(type);
            if (i == 1)
                action = alarmText.isEmail() ? KAEvent::EMAIL : KAEvent::COMMAND;
        }
        KAlarm::editNewAlarm(action, win, &alarmText);
    }
}

/******************************************************************************
* Called when the status of a calendar has changed.
* Enable or disable actions appropriately.
*/
void MainWindow::slotCalendarStatusChanged()
{
    // Find whether there are any writable calendars
#ifdef USE_AKONADI
    bool active  = !CollectionControlModel::enabledCollections(CalEvent::ACTIVE, true).isEmpty();
    bool templat = !CollectionControlModel::enabledCollections(CalEvent::TEMPLATE, true).isEmpty();
#else
    AlarmResources* resources = AlarmResources::instance();
    bool active  = resources->activeCount(CalEvent::ACTIVE, true);
    bool templat = resources->activeCount(CalEvent::TEMPLATE, true);
#endif
    for (int i = 0, end = mWindowList.count();  i < end;  ++i)
    {
        MainWindow* w = mWindowList[i];
        w->mActionImportAlarms->setEnabled(active || templat);
        w->mActionImportBirthdays->setEnabled(active);
        w->mActionCreateTemplate->setEnabled(templat);
        // Note: w->mActionNew enabled status is set in the NewAlarmAction class.
        w->slotSelection();
    }
}

/******************************************************************************
* Called when the selected items in the ListView change.
* Enables the actions appropriately.
*/
void MainWindow::slotSelection()
{
    // Find which events have been selected
#ifdef USE_AKONADI
    QVector<KAEvent> events = mListView->selectedEvents();
#else
    KAEvent::List events = mListView->selectedEvents();
#endif
    int count = events.count();
    if (!count)
    {
        selectionCleared();    // disable actions
        emit selectionChanged();
        return;
    }

    // Find whether there are any writable resources
    bool active = mActionNew->isEnabled();

    bool readOnly = false;
    bool allArchived = true;
    bool enableReactivate = true;
    bool enableEnableDisable = true;
    bool enableEnable = false;
    bool enableDisable = false;
    AlarmCalendar* resources = AlarmCalendar::resources();
    KDateTime now = KDateTime::currentUtcDateTime();
    for (int i = 0;  i < count;  ++i)
    {
#ifdef USE_AKONADI
        KAEvent* ev = resources->event(EventId(events[i]));   // get up-to-date status
        KAEvent* event = ev ? ev : &events[i];
#else
        KAEvent* event = events[i];
#endif
        bool expired = event->expired();
        if (!expired)
            allArchived = false;
#ifdef USE_AKONADI
        if (resources->eventReadOnly(event->itemId()))
#else
        if (resources->eventReadOnly(event->id()))
#endif
            readOnly = true;
        if (enableReactivate
        &&  (!expired  ||  !event->occursAfter(now, true)))
            enableReactivate = false;
        if (enableEnableDisable)
        {
            if (expired)
                enableEnableDisable = enableEnable = enableDisable = false;
            else
            {
                if (!enableEnable  &&  !event->enabled())
                    enableEnable = true;
                if (!enableDisable  &&  event->enabled())
                    enableDisable = true;
            }
        }
    }

    kDebug() << "true";
#ifdef USE_AKONADI
    mActionCreateTemplate->setEnabled((count == 1) && !CollectionControlModel::enabledCollections(CalEvent::TEMPLATE, true).isEmpty());
#else
    mActionCreateTemplate->setEnabled((count == 1) && (AlarmResources::instance()->activeCount(CalEvent::TEMPLATE, true) > 0));
#endif
    mActionExportAlarms->setEnabled(true);
    mActionExport->setEnabled(true);
    mActionCopy->setEnabled(active && count == 1);
    mActionModify->setEnabled(count == 1);
    mActionDelete->setEnabled(!readOnly && (active || allArchived));
    mActionReactivate->setEnabled(active && enableReactivate);
    mActionEnable->setEnabled(active && !readOnly && (enableEnable || enableDisable));
    if (enableEnable || enableDisable)
        setEnableText(enableEnable);

    emit selectionChanged();
}

/******************************************************************************
* Called when a context menu is requested in the ListView.
* Displays a context menu to modify or delete the selected item.
*/
void MainWindow::slotContextMenuRequested(const QPoint& globalPos)
{
    kDebug();
    if (mContextMenu)
        mContextMenu->popup(globalPos);
}

/******************************************************************************
* Disables actions when no item is selected.
*/
void MainWindow::selectionCleared()
{
    mActionCreateTemplate->setEnabled(false);
    mActionExportAlarms->setEnabled(false);
    mActionExport->setEnabled(false);
    mActionCopy->setEnabled(false);
    mActionModify->setEnabled(false);
    mActionDelete->setEnabled(false);
    mActionReactivate->setEnabled(false);
    mActionEnable->setEnabled(false);
}

/******************************************************************************
* Set the text of the Enable/Disable menu action.
*/
void MainWindow::setEnableText(bool enable)
{
    mActionEnableEnable = enable;
    mActionEnable->setText(enable ? i18nc("@action", "Ena&ble") : i18nc("@action", "Disa&ble"));
}

/******************************************************************************
* Display or hide the specified main window.
* This should only be called when the application doesn't run in the system tray.
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
            win->setWindowState(win->windowState() & ~Qt::WindowMinimized);
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

/******************************************************************************
* Called when the Edit button is clicked in an alarm message window.
* This controls the alarm edit dialog created by the alarm window, and allows
* it to remain unaffected by the alarm window closing.
* See MessageWin::slotEdit() for more information.
*/
#ifdef USE_AKONADI
void MainWindow::editAlarm(EditAlarmDlg* dlg, const KAEvent& event)
#else
void MainWindow::editAlarm(EditAlarmDlg* dlg, const KAEvent& event, AlarmResource* resource)
#endif
{
#ifdef USE_AKONADI
    mEditAlarmMap[dlg] = event;
#else
    KAEvent ev = event;
    ev.setResource(resource);
    mEditAlarmMap[dlg] = ev;
#endif
    connect(dlg, SIGNAL(accepted()), SLOT(editAlarmOk()));
    connect(dlg, SIGNAL(destroyed(QObject*)), SLOT(editAlarmDeleted(QObject*)));
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);   // ensure no memory leaks
    dlg->show();
}

/******************************************************************************
* Called when OK is clicked in the alarm edit dialog shown by editAlarm().
* Updates the event which has been edited.
*/
void MainWindow::editAlarmOk()
{
    EditAlarmDlg* dlg = qobject_cast<EditAlarmDlg*>(sender());
    if (!dlg)
        return;
    QMap<EditAlarmDlg*, KAEvent>::Iterator it = mEditAlarmMap.find(dlg);
    if (it == mEditAlarmMap.end())
        return;
    KAEvent event = it.value();
    mEditAlarmMap.erase(it);
    if (!event.isValid())
        return;
    if (dlg->result() != QDialog::Accepted)
        return;
#ifdef USE_AKONADI
    Akonadi::Collection c = AkonadiModel::instance()->collection(event);
    KAlarm::updateEditedAlarm(dlg, event, c);
#else
    KAlarm::updateEditedAlarm(dlg, event, event.resource());
#endif
}

/******************************************************************************
* Called when the alarm edit dialog shown by editAlarm() is deleted.
* Removes the dialog from the pending list.
*/
void MainWindow::editAlarmDeleted(QObject* obj)
{
    mEditAlarmMap.remove(static_cast<EditAlarmDlg*>(obj));
}

// vim: et sw=4:
