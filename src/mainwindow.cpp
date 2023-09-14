/*
 *  mainwindow.cpp  -  main application window
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mainwindow.h"

#include "alarmlistdelegate.h"
#include "alarmlistview.h"
#include "birthdaydlg.h"
#include "datepicker.h"
#include "deferdlg.h"
#include "functions.h"
#include "kalarmapp.h"
#include "kernelwakealarm.h"
#include "newalarmaction.h"
#include "prefdlg.h"
#include "preferences.h"
#include "resourcescalendar.h"
#include "resourceselector.h"
#include "templatedlg.h"
#include "traywindow.h"
#include "wakedlg.h"
#include "resources/datamodel.h"
#include "resources/eventmodel.h"
#include "resources/resources.h"
#include "lib/autoqpointer.h"
#include "lib/dragdrop.h"
#include "lib/messagebox.h"
#include "kalarmcalendar/alarmtext.h"
#include "config-kalarm.h"
#include "kalarm_debug.h"

#include <KCalUtils/ICalDrag>
#include <KCalendarCore/MemoryCalendar>
using namespace KCalendarCore;
using namespace KCalUtils;

#include <KAboutData>
#include <KToolBar>
#include <KActionCollection>
#include <KGlobalAccel>
#include <KStandardAction>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KShortcutsDialog>
#include <KEditToolBar>
#include <KXMLGUIFactory>
#include <KToggleAction>
#include <KNotifyConfigWidget>
#include <KToolBarPopupAction>

#include <QAction>
#include <QSplitter>
#include <QVBoxLayout>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QMenu>
#include <QMimeDatabase>
#include <QInputDialog>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>
#include <QMenuBar>
#include <QSystemTrayIcon>
#include <QMimeData>

using namespace KAlarmCal;

//clazy:excludeall=non-pod-global-static

namespace
{
const QString UI_FILE(QStringLiteral("kalarmui.rc"));
const char*   WINDOW_NAME = "MainWindow";

const char*   VIEW_GROUP          = "View";
const char*   SHOW_COLUMNS        = "ShowColumns";
const char*   SHOW_ARCHIVED_KEY   = "ShowArchivedAlarms";
const char*   SHOW_RESOURCES_KEY  = "ShowResources";
const char*   RESOURCES_WIDTH_KEY = "ResourcesWidth";
const char*   SHOW_DATE_NAVIGATOR = "ShowDateNavigator";
const char*   DATE_NAVIGATOR_TOP  = "DateNavigatorTop";
const char*   HIDDEN_TRAY_PARENT  = "HiddenTrayParent";

QString             undoText;
QString             undoTextStripped;
QList<QKeySequence> undoShortcut;
QString             redoText;
QString             redoTextStripped;
QList<QKeySequence> redoShortcut;
}


/*=============================================================================
=  Class: MainWindow
=============================================================================*/

MainWindow::WindowList   MainWindow::mWindowList;
TemplateDlg*             MainWindow::mTemplateDlg = nullptr;


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
    : MainWindowBase(nullptr, Qt::WindowContextHelpButtonHint)
{
    Q_UNUSED(restored)
    qCDebug(KALARM_LOG) << "MainWindow:";
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    setObjectName(QStringLiteral("MainWin"));    // used by LikeBack
    setPlainCaption(KAboutData::applicationData().displayName());
    KConfigGroup config(KSharedConfig::openConfig(), VIEW_GROUP);
    mShowResources     = config.readEntry(SHOW_RESOURCES_KEY, false);
    mShowDateNavigator = config.readEntry(SHOW_DATE_NAVIGATOR, false);
    mDateNavigatorTop  = config.readEntry(DATE_NAVIGATOR_TOP, true);
    mShowArchived      = config.readEntry(SHOW_ARCHIVED_KEY, false);
    mResourcesWidth    = config.readEntry(RESOURCES_WIDTH_KEY, (int)0);
    const QList<bool> showColumns = config.readEntry(SHOW_COLUMNS, QList<bool>());

    setAcceptDrops(true);         // allow drag-and-drop onto this window

    mSplitter = new QSplitter(Qt::Horizontal, this);
    mSplitter->setChildrenCollapsible(false);
    mSplitter->installEventFilter(this);
    setCentralWidget(mSplitter);
    mSplitter->setStretchFactor(0, 0);   // don't resize side panel when window is resized
    mSplitter->setStretchFactor(1, 1);

    // Create the side panel, containing the calendar resource selector and
    // date picker widgets.
    mPanel = new QScrollArea(mSplitter);
    QWidget* panelContents = new QWidget(mPanel);
    mPanel->setWidget(panelContents);
    mPanel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mPanel->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mPanel->setWidgetResizable(true);
    mPanel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    mPanel->verticalScrollBar()->installEventFilter(this);
    mPanelLayout = new QVBoxLayout(panelContents);
    mPanelLayout->setContentsMargins(0, 0, 0, 0);

    DataModel::widgetNeedsDatabase(this);
    mResourceSelector = new ResourceSelector(this, panelContents);
    mResourceSelector->setResizeToList();
    mPanelLayout->addWidget(mResourceSelector);
    mPanelLayout->addStretch();

    mDatePicker = new DatePicker(nullptr);
    mPanelLayout->addWidget(mDatePicker);

    // Create the alarm list widget
    mListFilterModel = DataModel::createAlarmListModel(this);
    mListFilterModel->setEventTypeFilter(mShowArchived ? CalEvent::ACTIVE | CalEvent::ARCHIVED : CalEvent::ACTIVE);
    mListView = new AlarmListView(WINDOW_NAME, mSplitter);
    mListView->setModel(mListFilterModel);
    mListView->setColumnsVisible(showColumns);
    mListView->setItemDelegate(new AlarmListDelegate(mListView));
    connect(mListView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::slotSelection);
    connect(mListView, &AlarmListView::contextMenuRequested, this, &MainWindow::slotContextMenuRequested);
    connect(mListView, &AlarmListView::columnsVisibleChanged, this, &MainWindow::slotAlarmListColumnsChanged);
    connect(Resources::instance(), &Resources::settingsChanged,
                             this, &MainWindow::slotCalendarStatusChanged);
    connect(mResourceSelector, &ResourceSelector::resized, this, &MainWindow::resourcesResized);
    connect(mDatePicker, &DatePicker::datesSelected, this, &MainWindow::datesSelected);
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
    qCDebug(KALARM_LOG) << "~MainWindow";
    bool trayParent = isTrayParent();   // must call before removing from window list
    mWindowList.removeAt(mWindowList.indexOf(this));

    // Prevent view updates during window destruction
    delete mResourceSelector;
    mResourceSelector = nullptr;
    delete mDatePicker;
    mDatePicker = nullptr;
    delete mListView;
    mListView = nullptr;

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
* Called when the QApplication::saveStateRequest() signal has been emitted.
* Save settings to the session managed config file, for restoration
* when the program is restored.
*/
void MainWindow::saveProperties(KConfigGroup& config)
{
    config.writeEntry(HIDDEN_TRAY_PARENT, isTrayParent() && isHidden());
    config.writeEntry(SHOW_ARCHIVED_KEY, mShowArchived);
    config.writeEntry(SHOW_RESOURCES_KEY, mShowResources);
    config.writeEntry(SHOW_DATE_NAVIGATOR, mShowDateNavigator);
    config.writeEntry(DATE_NAVIGATOR_TOP, mDateNavigatorTop);
    config.writeEntry(SHOW_COLUMNS, mListView->columnsVisible());
    config.writeEntry(RESOURCES_WIDTH_KEY, mResourceSelector->isVisible() ? mResourceSelector->width() : 0);
}

/******************************************************************************
* Read settings from the session managed config file.
* This function is automatically called whenever the app is being
* restored. Read in whatever was saved in saveProperties().
*/
void MainWindow::readProperties(const KConfigGroup& config)
{
    mHiddenTrayParent  = config.readEntry(HIDDEN_TRAY_PARENT, true);
    mShowArchived      = config.readEntry(SHOW_ARCHIVED_KEY, false);
    mShowResources     = config.readEntry(SHOW_RESOURCES_KEY, false);
    mResourcesWidth    = config.readEntry(RESOURCES_WIDTH_KEY, (int)0);
    if (mResourcesWidth <= 0)
        mShowResources = false;
    mShowDateNavigator = config.readEntry(SHOW_DATE_NAVIGATOR, false);
    mDateNavigatorTop  = config.readEntry(DATE_NAVIGATOR_TOP, true);
    mListView->setColumnsVisible(config.readEntry(SHOW_COLUMNS, QList<bool>()));
}

/******************************************************************************
* Get the main main window, i.e. the parent of the system tray icon, or if
* none, the first main window to be created. Visible windows take precedence
* over hidden ones.
*/
MainWindow* MainWindow::mainMainWindow()
{
    MainWindow* tray = (theApp() && theApp()->trayWindow()) ? theApp()->trayWindow()->assocMainWindow() : nullptr;
    if (tray  &&  tray->isVisible())
        return tray;
    for (int i = 0, end = mWindowList.count();  i < end;  ++i)
        if (mWindowList.at(i)->isVisible())
            return mWindowList.at(i);
    if (tray)
        return tray;
    if (mWindowList.isEmpty())
        return nullptr;
    return mWindowList.at(0);
}

/******************************************************************************
* Check whether this main window is effectively the parent of the system tray icon.
*/
bool MainWindow::isTrayParent() const
{
    TrayWindow* tray = theApp()->trayWindow();
    if (!tray  ||  !QSystemTrayIcon::isSystemTrayAvailable())
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
                auto ke = static_cast<QKeyEvent*>(e);
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
    else if (obj == mPanel->verticalScrollBar())
    {
        // The scroll bar width can't be determined correctly until it has been shown.
        if (e->type() == QEvent::Show)
        {
            mPanelScrollBarWidth = mPanel->verticalScrollBar()->width();
            arrangePanel();
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
    setSplitterSizes();
}

/******************************************************************************
* Emitted when the date selection changes in the date picker.
*/
void MainWindow::datesSelected(const QList<QDate>& dates, bool workChange)
{
    mListFilterModel->setDateFilter(dates, workChange);
}

/******************************************************************************
* Called when the resources panel has been resized.
* Records the new size in the config file.
*/
void MainWindow::resourcesResized()
{
    if (!mShown  ||  mResizing)
        return;
    const QList<int> widths = mSplitter->sizes();
    if (widths.count() > 1  &&  mResourceSelector->isVisible())
    {
        mResourcesWidth = widths[0];
        // Width is reported as non-zero when resource selector is
        // actually invisible, so note a zero width in these circumstances.
        if (mResourcesWidth <= 5)
            mResourcesWidth = 0;
        else if (mainMainWindow() == this)
        {
            KConfigGroup config(KSharedConfig::openConfig(), VIEW_GROUP);
            config.writeEntry(SHOW_RESOURCES_KEY, mShowResources);
            if (mShowResources)
                config.writeEntry(RESOURCES_WIDTH_KEY, mResourcesWidth);
            config.sync();
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
    setSplitterSizes();
    MainWindowBase::showEvent(se);
    mShown = true;

    if (mMenuError)
        QTimer::singleShot(0, this, &MainWindow::showMenuErrorMessage);    //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
}

/******************************************************************************
* Set the sizes of the splitter panels.
*/
void MainWindow::setSplitterSizes()
{
    if (mShowResources  &&  mResourcesWidth > 0)
    {
        const QList<int> widths{ mResourcesWidth,
                                 width() - mResourcesWidth - mSplitter->handleWidth()
                               };
        mSplitter->setSizes(widths);
    }
}

/******************************************************************************
* Show the menu error message now that the main window has been displayed.
* Waiting until now lets the user easily associate the message with the main
* window which is faulty.
*/
void MainWindow::showMenuErrorMessage()
{
    if (mMenuError)
    {
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
    KActionCollection* actions = actionCollection();

    mActionTemplates = new QAction(i18nc("@action", "Templates..."), this);
    actions->addAction(QStringLiteral("templates"), mActionTemplates);
    connect(mActionTemplates, &QAction::triggered, this, &MainWindow::slotTemplates);

    mActionNew = new NewAlarmAction(false, i18nc("@action", "New"), this, actions);
    mActionNew->setActionNames(QStringLiteral("newDisplay"), QStringLiteral("newCommand"), QStringLiteral("newEmail"), QStringLiteral("newAudio"), QStringLiteral("newFromTemplate"));
    actions->addAction(QStringLiteral("new"), mActionNew);
    connect(mActionNew, &NewAlarmAction::selected, this, &MainWindow::slotNew);
    connect(mActionNew, &NewAlarmAction::selectedTemplate, this, &MainWindow::slotNewFromTemplate);

    mActionCreateTemplate = new QAction(i18nc("@action", "Create Template..."), this);
    actions->addAction(QStringLiteral("createTemplate"), mActionCreateTemplate);
    connect(mActionCreateTemplate, &QAction::triggered, this, &MainWindow::slotNewTemplate);

    mActionCopy = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18nc("@action", "Copy..."), this);
    actions->addAction(QStringLiteral("copy"), mActionCopy);
    actions->setDefaultShortcut(mActionCopy, QKeySequence(Qt::SHIFT | Qt::Key_Insert));
    connect(mActionCopy, &QAction::triggered, this, &MainWindow::slotCopy);

    mActionModify = new QAction(QIcon::fromTheme(QStringLiteral("document-properties")), i18nc("@action", "Edit..."), this);
    actions->addAction(QStringLiteral("modify"), mActionModify);
    actions->setDefaultShortcut(mActionModify, QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(mActionModify, &QAction::triggered, this, &MainWindow::slotModify);

    mActionDelete = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action", "Delete"), this);
    actions->addAction(QStringLiteral("delete"), mActionDelete);
    actions->setDefaultShortcut(mActionDelete, QKeySequence::Delete);
    connect(mActionDelete, &QAction::triggered, this, &MainWindow::slotDeleteIf);

    // Set up Shift-Delete as a shortcut to delete without confirmation
    mActionDeleteForce = new QAction(i18nc("@action", "Delete Without Confirmation"), this);
    actions->addAction(QStringLiteral("delete-force"), mActionDeleteForce);
    actions->setDefaultShortcut(mActionDeleteForce, QKeySequence::Delete | Qt::SHIFT);
    connect(mActionDeleteForce, &QAction::triggered, this, &MainWindow::slotDeleteForce);

    mActionReactivate = new QAction(i18nc("@action", "Reactivate"), this);
    actions->addAction(QStringLiteral("undelete"), mActionReactivate);
    actions->setDefaultShortcut(mActionReactivate, QKeySequence(Qt::CTRL | Qt::Key_R));
    connect(mActionReactivate, &QAction::triggered, this, &MainWindow::slotReactivate);

    mActionEnable = new QAction(this);
    actions->addAction(QStringLiteral("disable"), mActionEnable);
    actions->setDefaultShortcut(mActionEnable, QKeySequence(Qt::CTRL | Qt::Key_B));
    connect(mActionEnable, &QAction::triggered, this, &MainWindow::slotEnable);

#if ENABLE_RTC_WAKE_FROM_SUSPEND
    if (!KernelWakeAlarm::isAvailable())
    {
        // Only enable RTC wake from suspend if kernel wake alarms are not available
        QAction* waction = new QAction(i18nc("@action", "Wake From Suspend..."), this);
        actions->addAction(QStringLiteral("wakeSuspend"), waction);
        connect(waction, &QAction::triggered, this, &MainWindow::slotWakeFromSuspend);
    }
#endif

    QAction* action = KAlarm::createStopPlayAction(this);
    actions->addAction(QStringLiteral("stopAudio"), action);
    KGlobalAccel::setGlobalShortcut(action, QList<QKeySequence>());  // allow user to set a global shortcut

    mActionShowArchived = new KToggleAction(i18nc("@action", "Show Archived Alarms"), this);
    actions->addAction(QStringLiteral("showArchivedAlarms"), mActionShowArchived);
    actions->setDefaultShortcut(mActionShowArchived, QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(mActionShowArchived, &KToggleAction::triggered, this, &MainWindow::slotShowArchived);

    mActionToggleTrayIcon = new KToggleAction(i18nc("@action", "Show in System Tray"), this);
    actions->addAction(QStringLiteral("showInSystemTray"), mActionToggleTrayIcon);
    connect(mActionToggleTrayIcon, &KToggleAction::triggered, this, &MainWindow::slotToggleTrayIcon);

    mActionToggleResourceSel = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-choose")), i18nc("@action", "Show Calendars"), this);
    actions->addAction(QStringLiteral("showResources"), mActionToggleResourceSel);
    connect(mActionToggleResourceSel, &KToggleAction::triggered, this, &MainWindow::slotToggleResourceSelector);

    mActionToggleDateNavigator = new KToggleAction(QIcon::fromTheme(QStringLiteral("view-calendar-month")), i18nc("@action", "Show Date Selector"), this);
    actions->addAction(QStringLiteral("showDateSelector"), mActionToggleDateNavigator);
    connect(mActionToggleDateNavigator, &KToggleAction::triggered, this, &MainWindow::slotToggleDateNavigator);

    mActionSpreadWindows = KAlarm::createSpreadWindowsAction(this);
    actions->addAction(QStringLiteral("spread"), mActionSpreadWindows);
    KGlobalAccel::setGlobalShortcut(mActionSpreadWindows, QList<QKeySequence>());  // allow user to set a global shortcut

    mActionImportAlarms = new QAction(i18nc("@action", "Import Alarms..."), this);
    actions->addAction(QStringLiteral("importAlarms"), mActionImportAlarms);
    connect(mActionImportAlarms, &QAction::triggered, this, &MainWindow::slotImportAlarms);

    if (Preferences::useAkonadi())
    {
        mActionImportBirthdays = new QAction(i18nc("@action", "Import Birthdays..."), this);
        actions->addAction(QStringLiteral("importBirthdays"), mActionImportBirthdays);
        connect(mActionImportBirthdays, &QAction::triggered, this, &MainWindow::slotBirthdays);
    }

    mActionExportAlarms = new QAction(i18nc("@action", "Export Selected Alarms..."), this);
    actions->addAction(QStringLiteral("exportAlarms"), mActionExportAlarms);
    connect(mActionExportAlarms, &QAction::triggered, this, &MainWindow::slotExportAlarms);

    mActionExport = new QAction(i18nc("@action", "Export..."), this);
    actions->addAction(QStringLiteral("export"), mActionExport);
    connect(mActionExport, &QAction::triggered, this, &MainWindow::slotExportAlarms);

    action = new QAction(QIcon::fromTheme(QStringLiteral("view-refresh")), i18nc("@action", "Refresh Alarms"), this);
    actions->addAction(QStringLiteral("refreshAlarms"), action);
    connect(action, &QAction::triggered, this, &MainWindow::slotRefreshAlarms);

    KToggleAction* toggleAction = KAlarm::createAlarmEnableAction(this);
    actions->addAction(QStringLiteral("alarmsEnable"), toggleAction);
    if (undoText.isNull())
    {
        // Get standard texts, etc., for Undo and Redo actions
        QAction* act = KStandardAction::undo(this, nullptr, actions);
        undoShortcut     = act->shortcuts();
        undoText         = act->text();
        undoTextStripped = KLocalizedString::removeAcceleratorMarker(undoText);
        delete act;
        act = KStandardAction::redo(this, nullptr, actions);
        redoShortcut     = act->shortcuts();
        redoText         = act->text();
        redoTextStripped = KLocalizedString::removeAcceleratorMarker(redoText);
        delete act;
    }
    mActionUndo = new KToolBarPopupAction(QIcon::fromTheme(QStringLiteral("edit-undo")), undoText, this);
    actions->addAction(QStringLiteral("edit_undo"), mActionUndo);
    actions->setDefaultShortcuts(mActionUndo, undoShortcut);
    connect(mActionUndo, &KToolBarPopupAction::triggered, this, &MainWindow::slotUndo);

    mActionRedo = new KToolBarPopupAction(QIcon::fromTheme(QStringLiteral("edit-redo")), redoText, this);
    actions->addAction(QStringLiteral("edit_redo"), mActionRedo);
    actions->setDefaultShortcuts(mActionRedo, redoShortcut);
    connect(mActionRedo, &KToolBarPopupAction::triggered, this, &MainWindow::slotRedo);

    KStandardAction::find(mListView, &EventListView::slotFind, actions);
    mActionFindNext = KStandardAction::findNext(mListView, &EventListView::slotFindNext, actions);
    mActionFindPrev = KStandardAction::findPrev(mListView, &EventListView::slotFindPrev, actions);
    KStandardAction::selectAll(mListView, &QTreeView::selectAll, actions);
    KStandardAction::deselect(mListView, &QAbstractItemView::clearSelection, actions);
    // Quit only once the event loop is called; otherwise, the parent window will
    // be deleted while still processing the action, resulting in a crash.
    QAction* act = KStandardAction::quit(nullptr, nullptr, actions);
    connect(act, &QAction::triggered, this, &MainWindow::slotQuit, Qt::QueuedConnection);
    KStandardAction::keyBindings(this, &MainWindow::slotConfigureKeys, actions);
    KStandardAction::configureNotifications(this, &MainWindow::slotConfigureNotifications, actions);
    KStandardAction::configureToolbars(this, &MainWindow::slotConfigureToolbar, actions);
    KStandardAction::preferences(this, &MainWindow::slotPreferences, actions);
    mActionShowMenuBar = KStandardAction::showMenubar(this, &MainWindow::slotToggleMenubar, actions);
    mHamburgerMenu = KStandardAction::hamburgerMenu(nullptr, nullptr, actions);
    mHamburgerMenu->setShowMenuBarAction(mActionShowMenuBar);
    mHamburgerMenu->setMenuBar(menuBar());
    connect(mHamburgerMenu, &KHamburgerMenu::aboutToShowMenu, this, [this]() {
        slotInitHamburgerMenu();
        // Needs to be run on demand, but the contents won't change, so disconnect now.
        disconnect(mHamburgerMenu, &KHamburgerMenu::aboutToShowMenu, this, nullptr);
    });
    mResourceSelector->initActions(actions);
    setStandardToolBarMenuEnabled(true);
    createGUI(UI_FILE);
    // Load menu and toolbar settings
    applyMainWindowSettings(KSharedConfig::openConfig()->group(WINDOW_NAME));

    mContextMenu = static_cast<QMenu*>(factory()->container(QStringLiteral("listContext"), this));
    QMenu* actionsMenu = static_cast<QMenu*>(factory()->container(QStringLiteral("actions"), this));
    mMenuError = (!mContextMenu  ||  !actionsMenu  ||  !resourceContextMenu());
    connect(mActionUndo->menu(), &QMenu::aboutToShow, this, &MainWindow::slotInitUndoMenu);
    connect(mActionUndo->menu(), &QMenu::triggered, this, &MainWindow::slotUndoItem);
    connect(mActionRedo->menu(), &QMenu::aboutToShow, this, &MainWindow::slotInitRedoMenu);
    connect(mActionRedo->menu(), &QMenu::triggered, this, &MainWindow::slotRedoItem);
    connect(Undo::instance(), &Undo::changed, this, &MainWindow::slotUndoStatus);
    connect(mListView, &AlarmListView::findActive, this, &MainWindow::slotFindActive);
    Preferences::connect(&Preferences::archivedKeepDaysChanged, this, &MainWindow::updateKeepArchived);
    Preferences::connect(&Preferences::showInSystemTrayChanged, this, &MainWindow::updateTrayIconAction);
    connect(theApp(), &KAlarmApp::trayIconToggled, this, &MainWindow::updateTrayIconAction);

    // Set menu item states
    setEnableText(true);
    mActionShowArchived->setChecked(mShowArchived);
    if (!Preferences::archivedKeepDays())
        mActionShowArchived->setEnabled(false);
    mActionToggleResourceSel->setChecked(mShowResources);
    mActionToggleDateNavigator->setChecked(mShowDateNavigator);
    slotToggleResourceSelector();   // give priority to resource selector over date navigator
    slotToggleDateNavigator();
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

    mActionShowMenuBar->setChecked(Preferences::showMenuBar());
    slotToggleMenubar(true);

    Undo::emitChanged();     // set the Undo/Redo menu texts
}

/******************************************************************************
* Set up the Hamburger menu, which allows the menu bar to be easily reinstated
* after it has been hidden.
*/
void MainWindow::slotInitHamburgerMenu()
{
    QMenu* menu = new QMenu(this);
    KActionCollection* actions = actionCollection();
    menu->addAction(actions->action(QStringLiteral("new")));
    menu->addAction(actions->action(QStringLiteral("templates")));
    menu->addSeparator();
    menu->addAction(actions->action(QStringLiteral("edit_find")));
    menu->addSeparator();
    menu->addAction(actions->action(QStringLiteral("showArchivedAlarms")));
    menu->addAction(actions->action(QStringLiteral("showDateSelector")));
    menu->addSeparator();
    menu->addAction(actions->action(QLatin1String(KStandardAction::name(KStandardAction::Quit))));
    mHamburgerMenu->setMenu(menu);
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
    qCDebug(KALARM_LOG) << "MainWindow::refresh";
    DataModel::reload();
}

/******************************************************************************
* Called when the keep archived alarm setting changes in the user preferences.
* Enable/disable Show archived alarms option.
*/
void MainWindow::updateKeepArchived(int days)
{
    qCDebug(KALARM_LOG) << "MainWindow::updateKeepArchived:" << (bool)days;
    if (mShowArchived  &&  !days)
        slotShowArchived();   // toggle Show Archived option setting
    mActionShowArchived->setEnabled(days);
}

/******************************************************************************
* Select an alarm in the displayed list.
*/
void MainWindow::selectEvent(const QString& eventId)
{
    mListView->clearSelection();
    const QModelIndex index = mListFilterModel->eventIndex(eventId);
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
* Provide the context menu for the resource selector to use.
*/
QMenu* MainWindow::resourceContextMenu()
{
    // Recreate the resource selector context menu if it has been deleted
    // (which happens if the toolbar is edited).
    if (!mResourceContextMenu)
        mResourceContextMenu = static_cast<QMenu*>(factory()->container(QStringLiteral("resourceContext"), this));
    return mResourceContextMenu;
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
void MainWindow::slotNewFromTemplate(const KAEvent& tmplate)
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
        KAlarm::editNewTemplate(event, this);
}

/******************************************************************************
* Called when the Copy button is clicked to edit a copy of an existing alarm,
* to add to the list.
*/
void MainWindow::slotCopy()
{
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editNewAlarm(event, this);
}

/******************************************************************************
* Called when the Modify button is clicked to edit the currently highlighted
* alarm in the list.
*/
void MainWindow::slotModify()
{
    KAEvent event = mListView->selectedEvent();
    if (event.isValid())
        KAlarm::editAlarm(event, this);   // edit alarm (view-only mode if archived or read-only)
}

/******************************************************************************
* Called when the Delete button is clicked to delete the currently highlighted
* alarms in the list.
*/
void MainWindow::slotDelete(bool force)
{
    QList<KAEvent> events = mListView->selectedEvents();
    if (!force  &&  Preferences::confirmAlarmDeletion())
    {
        int n = events.count();
        if (KAMessageBox::warningContinueCancel(this, i18ncp("@info", "Do you really want to delete the selected alarm?",
                                                             "Do you really want to delete the %1 selected alarms?", n),
                                                i18ncp("@title:window", "Delete Alarm", "Delete Alarms", n),
                                                KGuiItem(i18nc("@action:button", "Delete"), QStringLiteral("edit-delete")),
                                                KStandardGuiItem::cancel(),
                                                Preferences::CONFIRM_ALARM_DELETION)
            != KMessageBox::Continue)
            return;
    }

    // Remove any events which have just triggered, from the list to delete.
    Undo::EventList undos;
    for (int i = 0;  i < events.count();  )
    {
        Resource res = Resources::resourceForEvent(events[i].id());
        if (!res.isValid())
            events.remove(i);
        else
            undos.append(events[i++], res);
    }

    if (events.isEmpty())
        qCDebug(KALARM_LOG) << "MainWindow::slotDelete: No alarms left to delete";
    else
    {
        // Delete the events from the calendar and displays
        Resource resource;
        const KAlarm::UpdateResult status = KAlarm::deleteEvents(events, resource, true, this);
        if (status.status < KAlarm::UPDATE_FAILED)
        {
            // Create the undo list
            for (int i = status.failed.count();  --i >= 0; )
                undos.removeAt(status.failed.at(i));
            Undo::saveDeletes(undos);
        }
    }
}

/******************************************************************************
* Called when the Reactivate button is clicked to reinstate the currently
* highlighted archived alarms in the list.
*/
void MainWindow::slotReactivate()
{
    QList<KAEvent> events = mListView->selectedEvents();
    mListView->clearSelection();

    // Add the alarms to the displayed lists and to the calendar file
    Resource resource;   // active alarms resource which alarms are restored to
    QList<int> ineligibleIndexes;
    const KAlarm::UpdateResult status = KAlarm::reactivateEvents(events, ineligibleIndexes, resource, this);
    if (status.status < KAlarm::UPDATE_FAILED)
    {
        // Create the undo list, excluding ineligible events
        Undo::EventList undos;
        for (int i = 0, end = events.count();  i < end;  ++i)
        {
            if (!ineligibleIndexes.contains(i)  &&  !status.failed.contains(i))
                undos.append(events[i], resource);
        }
        Undo::saveReactivates(undos);
    }
}

/******************************************************************************
* Called when the Enable/Disable button is clicked to enable or disable the
* currently highlighted alarms in the list.
*/
void MainWindow::slotEnable()
{
    bool enable = mActionEnableEnable;    // save since changed in response to KAlarm::enableEvent()
    const QList<KAEvent> events = mListView->selectedEvents();
    QList<KAEvent> eventCopies;
    eventCopies.reserve(events.count());
    for (const KAEvent& event : events)
        eventCopies += event;
    KAlarm::enableEvents(eventCopies, enable, this);
    slotSelection();   // update Enable/Disable action text
}

/******************************************************************************
* Called when the columns visible in the alarm list view have changed.
*/
void MainWindow::slotAlarmListColumnsChanged()
{
    KConfigGroup config(KSharedConfig::openConfig(), VIEW_GROUP);
    config.writeEntry(SHOW_COLUMNS, mListView->columnsVisible());
    config.sync();
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
    config.sync();
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
#if ENABLE_RTC_WAKE_FROM_SUSPEND
    (WakeFromSuspendDlg::create(this))->show();
#endif
}

/******************************************************************************
* Called when the Import Alarms menu item is selected, to merge alarms from an
* external calendar into the current calendars.
*/
void MainWindow::slotImportAlarms()
{
    Resource resource;
    KAlarm::importAlarms(resource, this);
}

/******************************************************************************
* Called when the Export Alarms menu item is selected, to export the selected
* alarms to an external calendar.
*/
void MainWindow::slotExportAlarms()
{
    const QList<KAEvent> events = mListView->selectedEvents();
    if (!events.isEmpty())
        KAlarm::exportAlarms(events, this);
}

/******************************************************************************
* Called when the Import Birthdays menu item is selected, to display birthdays
* from the address book for selection as alarms.
*/
void MainWindow::slotBirthdays()
{
    if (Preferences::useAkonadi())
    {
        // Use AutoQPointer to guard against crash on application exit while
        // the dialogue is still open. It prevents double deletion (both on
        // deletion of MainWindow, and on return from this function).
        AutoQPointer<BirthdayDlg> dlg = new BirthdayDlg(this);
        if (dlg->exec() == QDialog::Accepted)
        {
            QList<KAEvent> events = dlg->events();
            if (!events.isEmpty())
            {
                mListView->clearSelection();
                // Add alarm to the displayed lists and to the calendar file
                Resource resource;
                const KAlarm::UpdateResult status = KAlarm::addEvents(events, resource, dlg, true, true);
                if (status.status < KAlarm::UPDATE_FAILED)
                {
                    // Create the undo list
                    Undo::EventList undos;
                    for (int i = 0, end = events.count();  i < end;  ++i)
                        if (!status.failed.contains(i))
                            undos.append(events[i], resource);
                    Undo::saveAdds(undos, i18nc("@info", "Import birthdays"));

                    KAlarm::outputAlarmWarnings(dlg);
                }
            }
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
        connect(mTemplateDlg, &QDialog::finished, this, &MainWindow::slotTemplatesEnd);
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
        mTemplateDlg->deleteLater();   // this deletes the dialog once it is safe to do so
        mTemplateDlg = nullptr;
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
            mResourcesWidth = mResourceSelector->sizeHint().width();
        mResourceSelector->resize(mResourcesWidth, mResourceSelector->height());
        mDateNavigatorTop = mShowDateNavigator;
    }
    else
        mResourceSelector->hide();
    arrangePanel();

    KConfigGroup config(KSharedConfig::openConfig(), VIEW_GROUP);
    config.writeEntry(SHOW_RESOURCES_KEY, mShowResources);
    if (mShowResources)
        config.writeEntry(RESOURCES_WIDTH_KEY, mResourcesWidth);
    config.writeEntry(DATE_NAVIGATOR_TOP, mDateNavigatorTop);
    config.sync();
}

/******************************************************************************
* Called when the Show Date Navigator menu item is selected.
*/
void MainWindow::slotToggleDateNavigator()
{
    mShowDateNavigator = mActionToggleDateNavigator->isChecked();
    if (mShowDateNavigator)
    {
        const int panelWidth = mDatePicker->sizeHint().width();
        mDatePicker->resize(panelWidth, mDatePicker->height());
        mDateNavigatorTop = !mShowResources;
    }
    else
    {
        // When the date navigator is not visible, prevent it from filtering alarms.
        mDatePicker->clearSelection();
        mDatePicker->hide();
    }
    arrangePanel();

    KConfigGroup config(KSharedConfig::openConfig(), VIEW_GROUP);
    config.writeEntry(SHOW_DATE_NAVIGATOR, mShowDateNavigator);
    config.writeEntry(DATE_NAVIGATOR_TOP, mDateNavigatorTop);
    config.sync();
}

/******************************************************************************
* Arrange the contents and set the width of the side panel containing the
* resource selector or date navigator.
*/
void MainWindow::arrangePanel()
{
    // Clear the panel layout before recreating it.
    QLayoutItem* item;
    while ((item = mPanelLayout->takeAt(0)) != nullptr)
        delete item;

    QWidget* date = mShowDateNavigator ? mDatePicker : nullptr;
    QWidget* res  = mShowResources ? mResourceSelector : nullptr;
    QWidget* top    = mDateNavigatorTop ? date : res;
    QWidget* bottom = mDateNavigatorTop ? res : date;
    if (bottom  &&  !top)
    {
        top = bottom;
        bottom = nullptr;
    }
    if (!top)
        mPanel->hide();
    else
    {
        int minWidth = 0;
        top->show();
        mPanelLayout->addWidget(top);
        mPanelLayout->addStretch();
        mPanel->show();
        minWidth = top->minimumSizeHint().width();
        if (bottom)
        {
            bottom->show();
            if (!mPanelDivider)
            {
                mPanelDivider = new QFrame(mPanel);
                mPanelDivider->setFrameShape(QFrame::HLine);
            }
            mPanelDivider->show();
            mPanelLayout->addWidget(mPanelDivider);
            mPanelLayout->addWidget(bottom);
            minWidth = std::max(minWidth, bottom->minimumSizeHint().width());
        }
        else if (mPanelDivider)
            mPanelDivider->hide();
        // Warning: mPanel->vericalScrollBar()->width() returns a silly value
        //          until AFTER showEvent() has been executed.
        mPanel->setMinimumWidth(minWidth + mPanelScrollBarWidth);

        int panelWidth = 0;
        if (mShowDateNavigator)
            panelWidth = mDatePicker->sizeHint().width();
        if (mShowResources)
            panelWidth = std::max(panelWidth, mResourcesWidth);
        panelWidth += mPanelScrollBarWidth;

        const int listWidth = width() - mSplitter->handleWidth() - panelWidth;
        mListView->resize(listWidth, mListView->height());
        mSplitter->setSizes({ panelWidth, listWidth });
    }
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
    mActionToggleTrayIcon->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());
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
    int id = mUndoMenuIds.value(action);
    Undo::undo(id, this, Undo::actionText(Undo::UNDO, id));
}

/******************************************************************************
* Called when a Redo item is selected.
*/
void MainWindow::slotRedoItem(QAction* action)
{
    int id = mUndoMenuIds.value(action);
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
    const QList<int> ids = Undo::ids(type);
    for (const int id : ids)
    {
        const QString actText = Undo::actionText(type, id);
        const QString descrip = Undo::description(type, id);
        const QString text = descrip.isEmpty()
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
        mActionUndo->setText(QStringLiteral("%1 %2").arg(undoText, undo));
    }
    if (redo.isNull())
    {
        mActionRedo->setEnabled(false);
        mActionRedo->setText(redoText);
    }
    else
    {
        mActionRedo->setEnabled(true);
        mActionRedo->setText(QStringLiteral("%1 %2").arg(redoText, redo));
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
* Called when the Show Menubar menu item is selected.
*/
void MainWindow::slotToggleMenubar(bool dontShowWarning)
{
    if (menuBar())
    {
        if (mActionShowMenuBar->isChecked())
            menuBar()->show();
        else
        {
            if (!dontShowWarning
            &&  (!toolBar()->isVisible() || !toolBar()->actions().contains(mHamburgerMenu)))
            {
                const QString accel = mActionShowMenuBar->shortcut().toString();
                KMessageBox::information(this,
                                         i18n("<qt>This will hide the menu bar completely."
                                              " You can show it again by typing %1.</qt>", accel),
                                         i18n("Hide menu bar"), QStringLiteral("HideMenuBarWarning"));
            }
            menuBar()->hide();
        }
        Preferences::setShowMenuBar(mActionShowMenuBar->isChecked());
        Preferences::self()->save();
    }
}

/******************************************************************************
* Called when the Configure Keys menu item is selected.
*/
void MainWindow::slotConfigureKeys()
{
    KShortcutsDialog::showDialog(actionCollection(),  KShortcutsEditor::LetterShortcutsAllowed, this);
}

/******************************************************************************
* Called when the Configure Notifications menu item is selected.
*/
void MainWindow::slotConfigureNotifications()
{
    KNotifyConfigWidget::configure(this);
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
    if (!qApp->isSavingSession())
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
                                            : data->hasText() || data->hasUrls();
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

/******************************************************************************
* Called when an object is dropped on a main or system tray window, to
* evaluate the action required and extract the text.
*/
void MainWindow::executeDropEvent(MainWindow* win, QDropEvent* e)
{
    qCDebug(KALARM_LOG) << "MainWindow::executeDropEvent: Formats:" << e->mimeData()->formats();
    const QMimeData* data = e->mimeData();
    KAEvent::SubAction action = KAEvent::SubAction::Message;
    AlarmText          alarmText;
    MemoryCalendar::Ptr calendar(new MemoryCalendar(Preferences::timeSpecAsZone()));
#ifndef NDEBUG
    const QString fmts = data->formats().join(QLatin1String(", "));
    qCDebug(KALARM_LOG) << "MainWindow::executeDropEvent:" << fmts;
#endif

    /* The order of the tests below matters, since some dropped objects
     * provide more than one mime type.
     * Don't change them without careful thought !!
     */
    if (DragDrop::dropRFC822(data, alarmText))
    {
        // Email message(s). Ignore all but the first.
        qCDebug(KALARM_LOG) << "MainWindow::executeDropEvent: email";
//TODO: Fetch attachments if an email alarm is created below
    }
    else if (ICalDrag::fromMimeData(data, calendar))
    {
        // iCalendar - If events are included, use the first event
        qCDebug(KALARM_LOG) << "MainWindow::executeDropEvent: iCalendar";
        const Event::List events = calendar->rawEvents();
        if (!events.isEmpty())
        {
            Event::Ptr event = events[0];
            if (event->alarms().isEmpty())
            {
                Alarm::Ptr alarm = event->newAlarm();
                alarm->setEnabled(true);
                alarm->setTime(event->dtStart());
                alarm->setDisplayAlarm(event->summary().isEmpty() ? event->description() : event->summary());
                event->addAlarm(alarm);
            }
            KAEvent ev(event);
            KAlarm::editNewAlarm(ev, win);
            return;
        }
        // If todos are included, use the first todo
        const Todo::List todos = calendar->rawTodos();
        if (!todos.isEmpty())
        {
            Todo::Ptr todo = todos[0];
            alarmText.setTodo(todo);
            KADateTime start(todo->dtStart(true));
            KADateTime due(todo->dtDue(true));
            bool haveBothTimes = false;
            if (todo->hasDueDate())
            {
                if (start.isValid())
                    haveBothTimes = true;
                else
                    start = due;
            }
            if (todo->allDay())
                start.setDateOnly(true);
            KAEvent::Flags flags = KAEvent::DEFAULT_FONT;
            if (start.isDateOnly())
                flags |= KAEvent::ANY_TIME;
            KAEvent ev(start, todo->summary(), alarmText.displayText(), Preferences::defaultBgColour(), Preferences::defaultFgColour(),
                       QFont(), KAEvent::SubAction::Message, 0, flags, true);
            ev.startChanges();
            if (todo->recurs())
            {
                ev.setRecurrence(*todo->recurrence());
                ev.setNextOccurrence(KADateTime::currentUtcDateTime());
            }
            const Alarm::List alarms = todo->alarms();
            if (!alarms.isEmpty()  &&  alarms[0]->type() == Alarm::Display)
            {
                // A display alarm represents a reminder
                int offset = 0;
                if (alarms[0]->hasStartOffset())
                    offset = alarms[0]->startOffset().asSeconds();
                else if (alarms[0]->hasEndOffset())
                {
                    offset = alarms[0]->endOffset().asSeconds();
                    if (haveBothTimes)
                    {
                        // Get offset relative to start time instead of due time
                        offset += start.secsTo(due);
                    }
                }
                if (offset / 60)
                    ev.setReminder(-offset / 60, false);
            }
            ev.endChanges();
            KAlarm::editNewAlarm(ev, win);
        }
        return;
    }
    else
    {
        QUrl url;
        if (KAlarm::dropAkonadiEmail(data, url, alarmText))
        {
            // It's an email held in Akonadi
            qCDebug(KALARM_LOG) << "MainWindow::executeDropEvent: Akonadi email";
//TODO: Fetch attachments if an email alarm is created below
        }
        else if (!url.isEmpty())
        {
            // The data provides a URL, but it isn't an Akonadi email URL.
            qCDebug(KALARM_LOG) << "MainWindow::executeDropEvent: URL";
            // Try to find the mime type of the file, without downloading a remote file
            QMimeDatabase mimeDb;
            const QString mimeTypeName = mimeDb.mimeTypeForUrl(url).name();
            action = mimeTypeName.startsWith(QLatin1String("audio/")) ? KAEvent::SubAction::Audio : KAEvent::SubAction::File;
            alarmText.setText(url.toDisplayString());
        }
    }
    if (alarmText.isEmpty())
    {
        if (data->hasText())
        {
            const QString text = data->text();
            qCDebug(KALARM_LOG) << "MainWindow::executeDropEvent: text";
            alarmText.setText(text);
        }
        else
            return;
    }

    if (!alarmText.isEmpty())
    {
        if (action == KAEvent::SubAction::Message
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
            const QString type = QInputDialog::getItem(mainMainWindow(), i18nc("@title:window", "Alarm Type"),
                                                       i18nc("@info", "Choose alarm type to create:"), types, 0, false, &ok);
            if (!ok)
                return;   // user didn't press OK
            int i = types.indexOf(type);
            if (i == 1)
                action = alarmText.isEmail() ? KAEvent::SubAction::Email : KAEvent::SubAction::Command;
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
    bool active  = !Resources::enabledResources(CalEvent::ACTIVE, true).isEmpty();
    bool templat = !Resources::enabledResources(CalEvent::TEMPLATE, true).isEmpty();
    for (MainWindow* w : std::as_const(mWindowList))
    {
        w->mActionImportAlarms->setEnabled(active || templat);
        if (w->mActionImportBirthdays)
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
    QList<KAEvent> events = mListView->selectedEvents();
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
    const KADateTime now = KADateTime::currentUtcDateTime();
    for (int i = 0;  i < count;  ++i)
    {
        const KAEvent ev = ResourcesCalendar::event(EventId(events.at(i)));   // get up-to-date status
        const KAEvent& event = ev.isValid() ? ev : events[i];
        bool expired = event.expired();
        if (!expired)
            allArchived = false;
        if (KAlarm::eventReadOnly(event.id()))
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

    qCDebug(KALARM_LOG) << "MainWindow::slotSelection: true";
    mActionCreateTemplate->setEnabled((count == 1) && !Resources::enabledResources(CalEvent::TEMPLATE, true).isEmpty());
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
    qCDebug(KALARM_LOG) << "MainWindow::slotContextMenuRequested";
    // Recreate the context menu if it has been deleted (which happens if the
    // toolbar is edited).
    if (!mContextMenu)
        mContextMenu = static_cast<QMenu*>(factory()->container(QStringLiteral("listContext"), this));
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
    mActionEnable->setText(enable ? i18nc("@action", "Enable") : i18nc("@action", "Disable"));
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
            return nullptr;
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
* See MessageWindow::slotEdit() for more information.
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
    auto dlg = qobject_cast<EditAlarmDlg*>(sender());
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
    Resource res = Resources::resourceForEvent(event.id());
    KAlarm::updateEditedAlarm(dlg, event, res);
}

/******************************************************************************
* Called when the alarm edit dialog shown by editAlarm() is deleted.
* Removes the dialog from the pending list.
*/
void MainWindow::editAlarmDeleted(QObject* obj)
{
    mEditAlarmMap.remove(static_cast<EditAlarmDlg*>(obj));
}

/******************************************************************************
* Called when the Defer button is clicked in an alarm window or notification.
* This controls the defer dialog created by the alarm display, and allows it to
* remain unaffected by the alarm display closing.
* See MessageWindow::slotEdit() for more information.
*/
void MainWindow::showDeferAlarmDlg(MessageDisplay::DeferDlgData* data)
{
    DeferAlarmDlg* dlg = data->dlg;
    mDeferAlarmMap[dlg] = data;
    connect(dlg, &KEditToolBar::finished, this, &MainWindow::deferAlarmDlgDone);
    connect(dlg, &KEditToolBar::destroyed, this, &MainWindow::deferAlarmDlgDeleted);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);   // ensure no memory leaks
    dlg->show();
}

/******************************************************************************
* Called when OK is clicked in the defer alarm dialog shown by
* showDeferAlarmDlg().
*/
void MainWindow::deferAlarmDlgDone(int result)
{
    processDeferAlarmDlg(sender(), result);
}

/******************************************************************************
* Called when the defer alarm dialog shown by showDeferAlarmDlg() is complete.
* Removes the dialog from the pending list, and processes the result.
*/
void MainWindow::processDeferAlarmDlg(QObject* obj, int result)
{
    auto dlg = qobject_cast<DeferAlarmDlg*>(obj);
    if (!dlg)
        return;
    QMap<DeferAlarmDlg*, MessageDisplay::DeferDlgData*>::Iterator it = mDeferAlarmMap.find(dlg);
    if (it == mDeferAlarmMap.end())
        return;
    MessageDisplay::DeferDlgData* data = it.value();
    mDeferAlarmMap.erase(it);
    MessageDisplay::processDeferDlg(data, result);
}

#include "moc_mainwindow.cpp"

// vim: et sw=4:
