/*
 *  mainwindow.cpp  -  main application window
 *  Program:  kalarm
 *  Copyright © 2001-2014 by David Jarvie <djarvie@kde.org>
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
#include "mainwindow.h"

#include "alarmcalendar.h"
#include "alarmlistdelegate.h"
#include "autoqpointer.h"
#include "alarmlistview.h"
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
#include <KCalCore/MemoryCalendar>
#include <KCalUtils/kcalutils/icaldrag.h>
#include <AkonadiWidgets/controlgui.h>
using namespace KCalCore;
using namespace KCalUtils;
#include <KLocale>
#include <K4AboutData>
#include <ktoolbar.h>
#include <kactioncollection.h>
#include <ksystemtrayicon.h>
#include <kstandardaction.h>
#include <kiconloader.h>
#include <kurl.h>
#include <KLocalizedString>
#include <KSharedConfig>
#include <kconfiggroup.h>
#include <kshortcutsdialog.h>
#include <kedittoolbar.h>
#include <kxmlguifactory.h>
#include <ktoggleaction.h>
#include <ktoolbarpopupaction.h>
#include <KTimeZone>

#include <QAction>
#include <QSplitter>
#include <QByteArray>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QMenu>
#include <qinputdialog.h>
#include "kalarm_debug.h"

using namespace KAlarmCal;

static const QString UI_FILE     = QLatin1String("kalarmui.rc");
static const char*   WINDOW_NAME = "MainWindow";

static const char*   VIEW_GROUP         = "View";
static const char*   SHOW_TIME_KEY      = "ShowAlarmTime";
static const char*   SHOW_TIME_TO_KEY   = "ShowTimeToAlarm";
static const char*   SHOW_ARCHIVED_KEY  = "ShowArchivedAlarms";
static const char*   SHOW_RESOURCES_KEY = "ShowResources";

static QString   undoText;
static QString   undoTextStripped;
static QList<QKeySequence>  undoShortcut;
static QString   redoText;
static QString   redoTextStripped;
static QList<QKeySequence> redoShortcut;


/*=============================================================================
=  Class: MainWindow
=============================================================================*/

MainWindow::WindowList   MainWindow::mWindowList;
TemplateDlg*             MainWindow::mTemplateDlg = Q_NULLPTR;

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
    : MainWindowBase(Q_NULLPTR, Qt::WindowContextHelpButtonHint),
      mResourcesWidth(-1),
      mHiddenTrayParent(false),
      mShown(false),
      mResizing(false)
{
    qCDebug(KALARM_LOG);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    setObjectName(QLatin1String("MainWin"));    // used by LikeBack
    setPlainCaption(KComponentData::mainComponent().aboutData()->programName());
    KConfigGroup config(KSharedConfig::openConfig(), VIEW_GROUP);
    mShowResources = config.readEntry(SHOW_RESOURCES_KEY, false);
    mShowArchived  = config.readEntry(SHOW_ARCHIVED_KEY, false);
    mShowTime      = config.readEntry(SHOW_TIME_KEY, true);
    mShowTimeTo    = config.readEntry(SHOW_TIME_TO_KEY, false);
    if (!restored)
    {
        KConfigGroup wconfig(KSharedConfig::openConfig(), WINDOW_NAME);
        mResourcesWidth = wconfig.readEntry(QStringLiteral("Splitter %1").arg(KApplication::desktop()->width()), (int)0);
    }

    setAcceptDrops(true);         // allow drag-and-drop onto this window
    if (!mShowTimeTo)
        mShowTime = true;     // ensure at least one time column is visible

    mSplitter = new QSplitter(Qt::Horizontal, this);
    mSplitter->setChildrenCollapsible(false);
    mSplitter->installEventFilter(this);
    setCentralWidget(mSplitter);

    // Create the calendar resource selector widget
    Akonadi::ControlGui::widgetNeedsAkonadi(this);
    mResourceSelector = new ResourceSelector(mSplitter);
    mSplitter->setStretchFactor(0, 0);   // don't resize resource selector when window is resized
    mSplitter->setStretchFactor(1, 1);

    // Create the alarm list widget
    mListFilterModel = new AlarmListModel(this);
    mListFilterModel->setEventTypeFilter(mShowArchived ? CalEvent::ACTIVE | CalEvent::ARCHIVED : CalEvent::ACTIVE);
    mListView = new AlarmListView(WINDOW_NAME, mSplitter);
    mListView->setModel(mListFilterModel);
    mListView->selectTimeColumns(mShowTime, mShowTimeTo);
    mListView->sortByColumn(mShowTime ? AlarmListModel::TimeColumn : AlarmListModel::TimeToColumn, Qt::AscendingOrder);
    mListView->setItemDelegate(new AlarmListDelegate(mListView));
    connect(mListView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), SLOT(slotSelection()));
    connect(mListView, &AlarmListView::contextMenuRequested, this, &MainWindow::slotContextMenuRequested);
    connect(AkonadiModel::instance(), SIGNAL(collectionStatusChanged(Akonadi::Collection,AkonadiModel::Change,QVariant,bool)),
                       SLOT(slotCalendarStatusChanged()));
    connect(mResourceSelector, &ResourceSelector::resized, this, &MainWindow::resourcesResized);
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
    qCDebug(KALARM_LOG);
    bool trayParent = isTrayParent();   // must call before removing from window list
    mWindowList.removeAt(mWindowList.indexOf(this));

    // Prevent view updates during window destruction
    delete mResourceSelector;
    mResourceSelector = Q_NULLPTR;
    delete mListView;
    mListView = Q_NULLPTR;

    if (theApp()->trayWindow())
    {
        if (trayParent)
            delete theApp()->trayWindow();
        else
            theApp()->trayWindow()->removeWindow(this);
    }
    KSharedConfig::openConfig()->sync();    // save any new window size to disc
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
    MainWindow* tray = theApp()->trayWindow() ? theApp()->trayWindow()->assocMainWindow() : Q_NULLPTR;
    if (tray  &&  tray->isVisible())
        return tray;
    for (int i = 0, end = mWindowList.count();  i < end;  ++i)
        if (mWindowList[i]->isVisible())
            return mWindowList[i];
    if (tray)
        return tray;
    if (mWindowList.isEmpty())
        return Q_NULLPTR;
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
            KConfigGroup config(KSharedConfig::openConfig(), WINDOW_NAME);
            config.writeEntry(QStringLiteral("Splitter %1").arg(KApplication::desktop()->width()), mResourcesWidth);
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
        KAMessageBox::error(this, xi18nc("@info", "Failure to create menus (perhaps <filename>%1</filename> missing or corrupted)", UI_FILE));
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
    //KShortcut dummy;
    KActionCollection* actions = actionCollection();

    mActionTemplates = new QAction(i18nc("@action", "&Templates..."), this);
    actions->addAction(QStringLiteral("templates"), mActionTemplates);
    connect(mActionTemplates, &QAction::triggered, this, &MainWindow::slotTemplates);

    mActionNew = new NewAlarmAction(false, i18nc("@action", "&New"), this);
    actions->addAction(QStringLiteral("new"), mActionNew);

    QAction* action = mActionNew->displayAlarmAction();
    actions->addAction(QStringLiteral("newDisplay"), action);
#pragma message("port QT5")
    //QT5 action->setGlobalShortcut(dummy);   // actions->addAction() must be called first!
    connect(action, &QAction::triggered, this, &MainWindow::slotNewDisplay);

    action = mActionNew->commandAlarmAction();
    actions->addAction(QStringLiteral("newCommand"), action);
#pragma message("port QT5")
    //QT5 action->setGlobalShortcut(dummy);   // actions->addAction() must be called first!
    connect(action, &QAction::triggered, this, &MainWindow::slotNewCommand);

    action = mActionNew->emailAlarmAction();
    actions->addAction(QStringLiteral("newEmail"), action);
#pragma message("port QT5")
    //QT5 action->setGlobalShortcut(dummy);   // actions->addAction() must be called first!
    connect(action, &QAction::triggered, this, &MainWindow::slotNewEmail);

    action = mActionNew->audioAlarmAction();
    actions->addAction(QStringLiteral("newAudio"), action);
#pragma message("port QT5")
    //QT5 action->setGlobalShortcut(dummy);   // actions->addAction() must be called first!
    connect(action, &QAction::triggered, this, &MainWindow::slotNewAudio);

    TemplateMenuAction *templateMenuAction = mActionNew->fromTemplateAlarmAction();
    actions->addAction(QStringLiteral("newFromTemplate"), templateMenuAction);
    connect(templateMenuAction, &TemplateMenuAction::selected, this, &MainWindow::slotNewFromTemplate);

    mActionCreateTemplate = new QAction(i18nc("@action", "Create Tem&plate..."), this);
    actions->addAction(QStringLiteral("createTemplate"), mActionCreateTemplate);
    connect(mActionCreateTemplate, &QAction::triggered, this, &MainWindow::slotNewTemplate);

    mActionCopy = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18nc("@action", "&Copy..."), this);
    actions->addAction(QStringLiteral("copy"), mActionCopy);
    actions->setDefaultShortcut(mActionCopy, QKeySequence(Qt::SHIFT + Qt::Key_Insert));
    connect(mActionCopy, &QAction::triggered, this, &MainWindow::slotCopy);

    mActionModify = new QAction(QIcon::fromTheme(QStringLiteral("document-properties")), i18nc("@action", "&Edit..."), this);
    actions->addAction(QStringLiteral("modify"), mActionModify);
    actions->setDefaultShortcut(mActionModify, QKeySequence(Qt::CTRL + Qt::Key_E));
    connect(mActionModify, &QAction::triggered, this, &MainWindow::slotModify);

    mActionDelete = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action", "&Delete"), this);
    actions->addAction(QStringLiteral("delete"), mActionDelete);
    actions->setDefaultShortcut(mActionDelete, QKeySequence::Delete);
    connect(mActionDelete, &QAction::triggered, this, &MainWindow::slotDeleteIf);

    // Set up Shift-Delete as a shortcut to delete without confirmation
    mActionDeleteForce = new QAction(i18nc("@action", "Delete Without Confirmation"), this);
    actions->addAction(QStringLiteral("delete-force"), mActionDeleteForce);
    actions->setDefaultShortcut(mActionDeleteForce, QKeySequence::Delete + Qt::SHIFT);
    connect(mActionDeleteForce, &QAction::triggered, this, &MainWindow::slotDeleteForce);

    mActionReactivate = new QAction(i18nc("@action", "Reac&tivate"), this);
    actions->addAction(QStringLiteral("undelete"), mActionReactivate);
    actions->setDefaultShortcut(mActionReactivate, QKeySequence(Qt::CTRL + Qt::Key_R));
    connect(mActionReactivate, &QAction::triggered, this, &MainWindow::slotReactivate);

    mActionEnable = new QAction(this);
    actions->addAction(QStringLiteral("disable"), mActionEnable);
    actions->setDefaultShortcut(mActionEnable, QKeySequence(Qt::CTRL + Qt::Key_B));
    connect(mActionEnable, &QAction::triggered, this, &MainWindow::slotEnable);

    action = new QAction(i18nc("@action", "Wake From Suspend..."), this);
    actions->addAction(QStringLiteral("wakeSuspend"), action);
    connect(action, &QAction::triggered, this, &MainWindow::slotWakeFromSuspend);

    action = KAlarm::createStopPlayAction(this);
    actions->addAction(QStringLiteral("stopAudio"), action);
    //QT5 action->setGlobalShortcut(dummy);   // actions->addAction() must be called first!

    mActionShowTime = new KToggleAction(i18n_a_ShowAlarmTimes(), this);
    actions->addAction(QStringLiteral("showAlarmTimes"), mActionShowTime);
    connect(mActionShowTime, &KToggleAction::triggered, this, &MainWindow::slotShowTime);

    mActionShowTimeTo = new KToggleAction(i18n_o_ShowTimeToAlarms(), this);
    actions->addAction(QStringLiteral("showTimeToAlarms"), mActionShowTimeTo);
    actions->setDefaultShortcut(mActionShowTimeTo, QKeySequence(Qt::CTRL + Qt::Key_I));
    connect(mActionShowTimeTo, &KToggleAction::triggered, this, &MainWindow::slotShowTimeTo);

    mActionShowArchived = new KToggleAction(i18nc("@action", "Show Archi&ved Alarms"), this);
    actions->addAction(QStringLiteral("showArchivedAlarms"), mActionShowArchived);
    actions->setDefaultShortcut(mActionShowArchived, QKeySequence(Qt::CTRL + Qt::Key_P));
    connect(mActionShowArchived, &KToggleAction::triggered, this, &MainWindow::slotShowArchived);

    mActionToggleTrayIcon = new KToggleAction(i18nc("@action", "Show in System &Tray"), this);
    actions->addAction(QStringLiteral("showInSystemTray"), mActionToggleTrayIcon);
    connect(mActionToggleTrayIcon, &KToggleAction::triggered, this, &MainWindow::slotToggleTrayIcon);

    mActionToggleResourceSel = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-choose")), i18nc("@action", "Show &Calendars"), this);
    actions->addAction(QStringLiteral("showResources"), mActionToggleResourceSel);
    connect(mActionToggleResourceSel, &KToggleAction::triggered, this, &MainWindow::slotToggleResourceSelector);

    mActionSpreadWindows = KAlarm::createSpreadWindowsAction(this);
    actions->addAction(QStringLiteral("spread"), mActionSpreadWindows);
#pragma message("port QT5")

    //QT5 mActionSpreadWindows->setGlobalShortcut(dummy);   // actions->addAction() must be called first!

    mActionImportAlarms = new QAction(i18nc("@action", "Import &Alarms..."), this);
    actions->addAction(QStringLiteral("importAlarms"), mActionImportAlarms);
    connect(mActionImportAlarms, &QAction::triggered, this, &MainWindow::slotImportAlarms);

    mActionImportBirthdays = new QAction(i18nc("@action", "Import &Birthdays..."), this);
    actions->addAction(QStringLiteral("importBirthdays"), mActionImportBirthdays);
    connect(mActionImportBirthdays, &QAction::triggered, this, &MainWindow::slotBirthdays);

    mActionExportAlarms = new QAction(i18nc("@action", "E&xport Selected Alarms..."), this);
    actions->addAction(QStringLiteral("exportAlarms"), mActionExportAlarms);
    connect(mActionExportAlarms, &QAction::triggered, this, &MainWindow::slotExportAlarms);

    mActionExport = new QAction(i18nc("@action", "E&xport..."), this);
    actions->addAction(QStringLiteral("export"), mActionExport);
    connect(mActionExport, &QAction::triggered, this, &MainWindow::slotExportAlarms);

    action = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18nc("@action", "&Refresh Alarms"), this);
    actions->addAction(QStringLiteral("refreshAlarms"), action);
    connect(action, &QAction::triggered, this, &MainWindow::slotRefreshAlarms);

    KToggleAction* toggleAction = KAlarm::createAlarmEnableAction(this);
    actions->addAction(QStringLiteral("alarmsEnable"), toggleAction);
    if (undoText.isNull())
    {
        // Get standard texts, etc., for Undo and Redo actions
        QAction* act = KStandardAction::undo(this, Q_NULLPTR, actions);
        undoShortcut     = act->shortcuts();
        undoText         = act->text();
        undoTextStripped = KLocalizedString::removeAcceleratorMarker(undoText);
        delete act;
        act = KStandardAction::redo(this, Q_NULLPTR, actions);
        redoShortcut     = act->shortcuts();
        redoText         = act->text();
        redoTextStripped = KLocalizedString::removeAcceleratorMarker(redoText);
        delete act;
    }
    mActionUndo = new KToolBarPopupAction(QIcon::fromTheme(QStringLiteral("edit-undo")), undoText, this);
    actions->addAction(QStringLiteral("edit_undo"), mActionUndo);
    mActionUndo->setShortcuts(undoShortcut);
    connect(mActionUndo, &KToolBarPopupAction::triggered, this, &MainWindow::slotUndo);

    mActionRedo = new KToolBarPopupAction(QIcon::fromTheme(QStringLiteral("edit-redo")), redoText, this);
    actions->addAction(QStringLiteral("edit_redo"), mActionRedo);
    mActionRedo->setShortcuts(redoShortcut);
    connect(mActionRedo, &KToolBarPopupAction::triggered, this, &MainWindow::slotRedo);

    KStandardAction::find(mListView, SLOT(slotFind()), actions);
    mActionFindNext = KStandardAction::findNext(mListView, SLOT(slotFindNext()), actions);
    mActionFindPrev = KStandardAction::findPrev(mListView, SLOT(slotFindPrev()), actions);
    KStandardAction::selectAll(mListView, SLOT(selectAll()), actions);
    KStandardAction::deselect(mListView, SLOT(clearSelection()), actions);
    // Quit only once the event loop is called; otherwise, the parent window will
    // be deleted while still processing the action, resulting in a crash.
    QAction* act = KStandardAction::quit(Q_NULLPTR, Q_NULLPTR, actions);
    connect(act, &QAction::triggered, this, &MainWindow::slotQuit, Qt::QueuedConnection);
    KStandardAction::keyBindings(this, SLOT(slotConfigureKeys()), actions);
    KStandardAction::configureToolbars(this, SLOT(slotConfigureToolbar()), actions);
    KStandardAction::preferences(this, SLOT(slotPreferences()), actions);
    mResourceSelector->initActions(actions);
    setStandardToolBarMenuEnabled(true);
    createGUI(UI_FILE);
    // Load menu and toolbar settings
    applyMainWindowSettings(KSharedConfig::openConfig()->group(WINDOW_NAME));

    mContextMenu = static_cast<QMenu*>(factory()->container(QLatin1String("listContext"), this));
    mActionsMenu = static_cast<QMenu*>(factory()->container(QLatin1String("actions"), this));
    QMenu* resourceMenu = static_cast<QMenu*>(factory()->container(QLatin1String("resourceContext"), this));
    mResourceSelector->setContextMenu(resourceMenu);
    mMenuError = (!mContextMenu  ||  !mActionsMenu  ||  !resourceMenu);
    connect(mActionUndo->menu(), SIGNAL(aboutToShow()), SLOT(slotInitUndoMenu()));
    connect(mActionUndo->menu(), SIGNAL(triggered(QAction*)), SLOT(slotUndoItem(QAction*)));
    connect(mActionRedo->menu(), SIGNAL(aboutToShow()), SLOT(slotInitRedoMenu()));
    connect(mActionRedo->menu(), SIGNAL(triggered(QAction*)), SLOT(slotRedoItem(QAction*)));
    connect(Undo::instance(), SIGNAL(changed(QString,QString)), SLOT(slotUndoStatus(QString,QString)));
    connect(mListView, &AlarmListView::findActive, this, &MainWindow::slotFindActive);
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
    qCDebug(KALARM_LOG);
    AkonadiModel::instance()->reload();
}

/******************************************************************************
* Called when the keep archived alarm setting changes in the user preferences.
* Enable/disable Show archived alarms option.
*/
void MainWindow::updateKeepArchived(int days)
{
    qCDebug(KALARM_LOG) << (bool)days;
    if (mShowArchived  &&  !days)
        slotShowArchived();   // toggle Show Archived option setting
    mActionShowArchived->setEnabled(days);
}

/******************************************************************************
* Select an alarm in the displayed list.
*/
void MainWindow::selectEvent(Akonadi::Item::Id eventId)
{
    mListView->clearSelection();
    QModelIndex index = mListFilterModel->eventIndex(eventId);
    if (index.isValid())
    {
        mListView->select(index);
        mListView->scrollTo(index);
    }
}

/******************************************************************************
* Return the single selected alarm in the displayed list.
*/
KAEvent MainWindow::selectedEvent() const
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
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editNewTemplate(&event, this);
}

/******************************************************************************
* Called when the Copy button is clicked to edit a copy of an existing alarm,
* to add to the list.
*/
void MainWindow::slotCopy()
{
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editNewAlarm(&event, this);
}

/******************************************************************************
* Called when the Modify button is clicked to edit the currently highlighted
* alarm in the list.
*/
void MainWindow::slotModify()
{
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editAlarm(&event, this);   // edit alarm (view-only mode if archived or read-only)
}

/******************************************************************************
* Called when the Delete button is clicked to delete the currently highlighted
* alarms in the list.
*/
void MainWindow::slotDelete(bool force)
{
    QVector<KAEvent> events = mListView->selectedEvents();
    if (!force  &&  Preferences::confirmAlarmDeletion())
    {
        int n = events.count();
        if (KAMessageBox::warningContinueCancel(this, i18ncp("@info", "Do you really want to delete the selected alarm?",
                                                             "Do you really want to delete the %1 selected alarms?", n),
                                                i18ncp("@title:window", "Delete Alarm", "Delete Alarms", n),
                                                KGuiItem(i18nc("@action:button", "&Delete"), QLatin1String("edit-delete")),
                                                KStandardGuiItem::cancel(),
                                                Preferences::CONFIRM_ALARM_DELETION)
            != KMessageBox::Continue)
            return;
    }

    // Remove any events which have just triggered, from the list to delete.
    Undo::EventList undos;
    AlarmCalendar* resources = AlarmCalendar::resources();
    for (int i = 0;  i < events.count();  )
    {
        Akonadi::Collection c = resources->collectionForEvent(events[i].itemId());
        if (!c.isValid())
            events.remove(i);
        else
            undos.append(events[i++], c);
    }

    if (events.isEmpty())
        qCDebug(KALARM_LOG) << "No alarms left to delete";
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
    QVector<KAEvent> events = mListView->selectedEvents();
    mListView->clearSelection();

    // Add the alarms to the displayed lists and to the calendar file
    Undo::EventList undos;
    QVector<EventId> ineligibleIDs;
    KAlarm::reactivateEvents(events, ineligibleIDs, Q_NULLPTR, this);

    // Create the undo list, excluding ineligible events
    AlarmCalendar* resources = AlarmCalendar::resources();
    for (int i = 0, end = events.count();  i < end;  ++i)
    {
        if (!ineligibleIDs.contains(EventId(events[i])))
        {
            undos.append(events[i], resources->collectionForEvent(events[i].itemId()));
        }
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
    QVector<KAEvent> events = mListView->selectedEvents();
    QVector<KAEvent> eventCopies;
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
        KConfigGroup config(KSharedConfig::openConfig(), VIEW_GROUP);
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
        KConfigGroup config(KSharedConfig::openConfig(), VIEW_GROUP);
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
    mListFilterModel->setEventTypeFilter(mShowArchived ? CalEvent::ACTIVE | CalEvent::ARCHIVED : CalEvent::ACTIVE);
    mListView->reset();
    KConfigGroup config(KSharedConfig::openConfig(), VIEW_GROUP);
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
    QVector<KAEvent> events = mListView->selectedEvents();
    if (!events.isEmpty())
    {
        KAEvent::List evts = KAEvent::ptrList(events);
        AlarmCalendar::exportAlarms(evts, this);
    }
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
            KAlarm::UpdateResult status = KAlarm::addEvents(events, dlg, true, true);

            Undo::EventList undos;
            AlarmCalendar* resources = AlarmCalendar::resources();
            for (int i = 0, end = events.count();  i < end;  ++i)
            {
                Akonadi::Collection c = resources->collectionForEvent(events[i].itemId());
                undos.append(events[i], c);
            }
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
        mTemplateDlg = Q_NULLPTR;
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

    KConfigGroup config(KSharedConfig::openConfig(), VIEW_GROUP);
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
    Undo::undo(this, KLocalizedString::removeAcceleratorMarker(mActionUndo->text()));
}

/******************************************************************************
* Called when the Redo action is selected.
*/
void MainWindow::slotRedo()
{
    Undo::redo(this, KLocalizedString::removeAcceleratorMarker(mActionRedo->text()));
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
        mActionUndo->setText(QStringLiteral("%1 %2").arg(undoText).arg(undo));
    }
    if (redo.isNull())
    {
        mActionRedo->setEnabled(false);
        mActionRedo->setText(redoText);
    }
    else
    {
        mActionRedo->setEnabled(true);
        mActionRedo->setText(QStringLiteral("%1 %2").arg(redoText).arg(redo));
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
    KConfigGroup grp(KSharedConfig::openConfig()->group(WINDOW_NAME));
    saveMainWindowSettings(grp);
    KEditToolBar dlg(factory());
    connect(&dlg, &KEditToolBar::newToolBarConfig, this, &MainWindow::slotNewToolbarConfig);
    dlg.exec();
}

/******************************************************************************
* Called when OK or Apply is clicked in the Configure Toolbars dialog, to save
* the new configuration.
*/
void MainWindow::slotNewToolbarConfig()
{
    createGUI(UI_FILE);
    applyMainWindowSettings(KSharedConfig::openConfig()->group(WINDOW_NAME));
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
    qCDebug(KALARM_LOG) << "Formats:" << e->mimeData()->formats();
    const QMimeData* data = e->mimeData();
    KAEvent::SubAction action = KAEvent::MESSAGE;
    QByteArray         bytes;
    AlarmText          alarmText;
    KPIM::MailList     mailList;
    KUrl::List         files;
    MemoryCalendar::Ptr calendar(new MemoryCalendar(Preferences::timeZone(true)));
#ifndef NDEBUG
    QString fmts = data->formats().join(QLatin1String(", "));
    qCDebug(KALARM_LOG) << fmts;
#endif

    /* The order of the tests below matters, since some dropped objects
     * provide more than one mime type.
     * Don't change them without careful thought !!
     */
    if (!(bytes = data->data(QLatin1String("message/rfc822"))).isEmpty())
    {
        // Email message(s). Ignore all but the first.
        qCDebug(KALARM_LOG) << "email";
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
    else if (KPIM::MailList::canDecode(data))
    {
        mailList = KPIM::MailList::fromMimeData(data);
        // KMail message(s). Ignore all but the first.
        qCDebug(KALARM_LOG) << "KMail_list";
        if (mailList.isEmpty())
            return;
        KPIM::MailSummary& summary = mailList[0];
        QDateTime dt;
        dt.setTime_t(summary.date());
        QString body = KAMail::getMailBody(summary.serialNumber());
        alarmText.setEmail(summary.to(), summary.from(), QString(),
                           KLocale::global()->formatDateTime(dt), summary.subject(),
                           body, summary.serialNumber());
    }
    else if (ICalDrag::fromMimeData(data, calendar))
    {
        // iCalendar - If events are included, use the first event
        qCDebug(KALARM_LOG) << "iCalendar";
        Event::List events = calendar->rawEvents();
        if (!events.isEmpty())
        {
            KAEvent ev(events[0]);
            KAlarm::editNewAlarm(&ev, win);
            return;
        }
        // If todos are included, use the first todo
        Todo::List todos = calendar->rawTodos();
        if (todos.isEmpty())
            return;
        Todo::Ptr todo = todos[0];
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
        qCDebug(KALARM_LOG) << "URL";
        // Try to find the mime type of the file, without downloading a remote file
        KMimeType::Ptr mimeType = KMimeType::findByUrl(files[0]);
        action = mimeType->name().startsWith(QStringLiteral("audio/")) ? KAEvent::AUDIO : KAEvent::FILE;
        alarmText.setText(files[0].prettyUrl());
    }
    else if (data->hasText())
    {
        QString text = data->text();
        qCDebug(KALARM_LOG) << "text";
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
            QString type = QInputDialog::getItem(mainMainWindow(), i18nc("@title:window", "Alarm Type"),
                                                 i18nc("@info", "Choose alarm type to create:"), types, 0, false, &ok);
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
    bool active  = !CollectionControlModel::enabledCollections(CalEvent::ACTIVE, true).isEmpty();
    bool templat = !CollectionControlModel::enabledCollections(CalEvent::TEMPLATE, true).isEmpty();
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
    QVector<KAEvent> events = mListView->selectedEvents();
    int count = events.count();
    if (!count)
    {
        selectionCleared();    // disable actions
        Q_EMIT selectionChanged();
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
        KAEvent* ev = resources->event(EventId(events[i]));   // get up-to-date status
        KAEvent* event = ev ? ev : &events[i];
        bool expired = event->expired();
        if (!expired)
            allArchived = false;
        if (resources->eventReadOnly(event->itemId()))
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

    qCDebug(KALARM_LOG) << "true";
    mActionCreateTemplate->setEnabled((count == 1) && !CollectionControlModel::enabledCollections(CalEvent::TEMPLATE, true).isEmpty());
    mActionExportAlarms->setEnabled(true);
    mActionExport->setEnabled(true);
    mActionCopy->setEnabled(active && count == 1);
    mActionModify->setEnabled(count == 1);
    mActionDelete->setEnabled(!readOnly && (active || allArchived));
    mActionReactivate->setEnabled(active && enableReactivate);
    mActionEnable->setEnabled(active && !readOnly && (enableEnable || enableDisable));
    if (enableEnable || enableDisable)
        setEnableText(enableEnable);

    Q_EMIT selectionChanged();
}

/******************************************************************************
* Called when a context menu is requested in the ListView.
* Displays a context menu to modify or delete the selected item.
*/
void MainWindow::slotContextMenuRequested(const QPoint& globalPos)
{
    qCDebug(KALARM_LOG);
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
            return Q_NULLPTR;
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
void MainWindow::editAlarm(EditAlarmDlg* dlg, const KAEvent& event)
{
    mEditAlarmMap[dlg] = event;
    connect(dlg, &KEditToolBar::accepted, this, &MainWindow::editAlarmOk);
    connect(dlg, &KEditToolBar::destroyed, this, &MainWindow::editAlarmDeleted);
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
    Akonadi::Collection c = AkonadiModel::instance()->collection(event);
    KAlarm::updateEditedAlarm(dlg, event, c);
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
