/*
 *  mainwindow.h  -  main application window
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

/** @file mainwindow.h - main application window */

#include "editdlg.h"
#include "mainwindowbase.h"
#include "messagedisplay.h"
#include "undo.h"
#include "kalarmcalendar/kaevent.h"

#include <KCalendarCore/Calendar>

#include <QList>
#include <QMap>
#include <QPointer>

class QDragEnterEvent;
class QHideEvent;
class QShowEvent;
class QResizeEvent;
class QDropEvent;
class QCloseEvent;
class QSplitter;
class QMenu;
class QScrollArea;
class QFrame;
class QVBoxLayout;
class QAction;
class KToggleAction;
class KToolBarPopupAction;
class KHamburgerMenu;
class AlarmListModel;
class AlarmListView;
class DatePicker;
class DeferAlarmDlg;
class NewAlarmAction;
class TemplateDlg;
class ResourceSelector;


class MainWindow : public MainWindowBase, public KCalendarCore::Calendar::CalendarObserver
{
    Q_OBJECT

public:
    static MainWindow* create(bool restored = false);
    ~MainWindow() override;
    bool               isTrayParent() const;
    bool               isHiddenTrayParent() const   { return mHiddenTrayParent; }
    bool               showingArchived() const      { return mShowArchived; }
    void               selectEvent(const QString& eventId);
    KAEvent            selectedEvent() const;
    void               editAlarm(EditAlarmDlg*, const KAEvent&);
    void               showDeferAlarmDlg(MessageDisplay::DeferDlgData*);
    void               clearSelection();
    QMenu*             resourceContextMenu();
    bool               eventFilter(QObject*, QEvent*) override;

    static void        refresh();
    static void        executeDragEnterEvent(QDragEnterEvent*);
    static void        executeDropEvent(MainWindow*, QDropEvent*);
    static void        closeAll();
    static MainWindow* toggleWindow(MainWindow*);
    static MainWindow* mainMainWindow();
    static MainWindow* firstWindow()      { return mWindowList.isEmpty() ? nullptr : mWindowList[0]; }
    static int         count()            { return mWindowList.count(); }

Q_SIGNALS:
    void           selectionChanged();

protected:
    void           resizeEvent(QResizeEvent*) override;
    void           showEvent(QShowEvent*) override;
    void           hideEvent(QHideEvent*) override;
    void           closeEvent(QCloseEvent*) override;
    void           dragEnterEvent(QDragEnterEvent* e) override  { executeDragEnterEvent(e); }
    void           dropEvent(QDropEvent*) override;
    void           saveProperties(KConfigGroup&) override;
    void           readProperties(const KConfigGroup&) override;

private Q_SLOTS:
    void           slotInitHamburgerMenu();
    void           slotNew(EditAlarmDlg::Type);
    void           slotNewFromTemplate(const KAlarmCal::KAEvent&);
    void           slotNewTemplate();
    void           slotCopy();
    void           slotModify();
    void           slotDeleteIf()     { slotDelete(false); }
    void           slotDeleteForce()  { slotDelete(true); }
    void           slotReactivate();
    void           slotEnable();
    void           slotToggleTrayIcon();
    void           slotRefreshAlarms();
    void           slotImportAlarms();
    void           slotExportAlarms();
    void           slotBirthdays();
    void           slotTemplates();
    void           slotTemplatesEnd();
    void           slotPreferences();
    void           slotToggleMenubar(bool dontShowWarning);
    void           slotConfigureKeys();
    void           slotConfigureNotifications();
    void           slotConfigureToolbar();
    void           slotNewToolbarConfig();
    void           slotQuit();
    void           slotSelection();
    void           slotContextMenuRequested(const QPoint& globalPos);
    void           slotShowArchived();
    void           slotSpreadWindowsShortcut();
    void           slotWakeFromSuspend();
    void           updateKeepArchived(int days);
    void           slotUndo();
    void           slotUndoItem(QAction* id);
    void           slotRedo();
    void           slotRedoItem(QAction* id);
    void           slotInitUndoMenu();
    void           slotInitRedoMenu();
    void           slotUndoStatus(const QString&, const QString&);
    void           slotFindActive(bool);
    void           updateTrayIconAction();
    void           slotToggleResourceSelector();
    void           slotToggleDateNavigator();
    void           slotCalendarStatusChanged();
    void           slotAlarmListColumnsChanged();
    void           datesSelected(const QList<QDate>& dates, bool workChange);
    void           resourcesResized();
    void           showMenuErrorMessage();
    void           showErrorMessage(const QString&);
    void           editAlarmOk();
    void           editAlarmDeleted(QObject*);
    void           deferAlarmDlgDone(int result);
    void           deferAlarmDlgDeleted(QObject* obj)  { processDeferAlarmDlg(obj, QDialog::Rejected); }

private:
    using WindowList = QList<MainWindow*>;

    explicit MainWindow(bool restored);
    void            initActions();
    void            selectionCleared();
    void            setEnableText(bool enable);
    void            arrangePanel();
    void            setSplitterSizes();
    void            initUndoMenu(QMenu*, Undo::Type);
    void            slotDelete(bool force);
    void            processDeferAlarmDlg(QObject*, int result);
    static void     enableTemplateMenuItem(bool);

    static WindowList    mWindowList;    // active main windows
    static TemplateDlg*  mTemplateDlg;   // the one and only template dialog

    AlarmListModel*      mListFilterModel;
    AlarmListView*       mListView;
    ResourceSelector*    mResourceSelector;   // resource selector widget
    DatePicker*          mDatePicker;         // date navigator widget
    QSplitter*           mSplitter;           // splits window into list and resource selector
    QScrollArea*         mPanel;              // side panel containing resource selector & date navigator
    QVBoxLayout*         mPanelLayout;        // layout for side panel contents
    QFrame*              mPanelDivider{nullptr};
    QMap<EditAlarmDlg*, KAEvent> mEditAlarmMap; // edit alarm dialogs to be handled by this window
    QMap<DeferAlarmDlg*, MessageDisplay::DeferDlgData*> mDeferAlarmMap; // defer alarm dialogs to be handled by this window
    KToggleAction*       mActionShowMenuBar;
    KToggleAction*       mActionToggleResourceSel;
    KToggleAction*       mActionToggleDateNavigator;
    QAction*             mActionImportAlarms;
    QAction*             mActionExportAlarms;
    QAction*             mActionExport;
    QAction*             mActionImportBirthdays{nullptr};
    QAction*             mActionTemplates;
    NewAlarmAction*      mActionNew;
    QAction*             mActionCreateTemplate;
    QAction*             mActionCopy;
    QAction*             mActionModify;
    QAction*             mActionDelete;
    QAction*             mActionDeleteForce;
    QAction*             mActionReactivate;
    QAction*             mActionEnable;
    QAction*             mActionFindNext;
    QAction*             mActionFindPrev;
    KToolBarPopupAction* mActionUndo;
    KToolBarPopupAction* mActionRedo;
    KToggleAction*       mActionToggleTrayIcon;
    KToggleAction*       mActionShowArchived;
    KToggleAction*       mActionSpreadWindows;
    KHamburgerMenu*      mHamburgerMenu;
    QPointer<QMenu>      mContextMenu;
    QPointer<QMenu>      mResourceContextMenu;
    QMap<QAction*, int>  mUndoMenuIds;             // items in the undo/redo menu, in order of appearance
    int                  mResourcesWidth{-1};      // width of resource selector widget
    int                  mPanelScrollBarWidth{0};  // width of side panel vertical scroll bar
    bool                 mHiddenTrayParent{false}; // on session restoration, hide this window
    bool                 mShowResources;           // show resource selector
    bool                 mShowDateNavigator;       // show date navigator
    bool                 mDateNavigatorTop{true};  // date navigator is at top of panel
    bool                 mShowArchived;            // include archived alarms in the displayed list
    bool                 mShown{false};            // true once the window has been displayed
    bool                 mActionEnableEnable;      // Enable/Disable action is set to "Enable"
    bool                 mMenuError;               // error occurred creating menus: need to show error message
    bool                 mResizing{false};         // window resize is in progress
};

// vim: et sw=4:
