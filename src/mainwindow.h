/*
 *  mainwindow.h  -  main application window
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/** @file mainwindow.h - main application window */

#include "editdlg.h"
#include "mainwindowbase.h"
#include "undo.h"

#include <KAlarmCal/KAEvent>

#include <KCalendarCore/Calendar>

#include <QList>
#include <QMap>

class QDragEnterEvent;
class QHideEvent;
class QShowEvent;
class QResizeEvent;
class QDropEvent;
class QCloseEvent;
class QSplitter;
class QMenu;
class QAction;
class KToggleAction;
class KToolBarPopupAction;
class AlarmListModel;
class AlarmListView;
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
        void               clearSelection();
        bool               eventFilter(QObject*, QEvent*) override;

        static void        refresh();
        static void        executeDragEnterEvent(QDragEnterEvent*);
        static void        executeDropEvent(MainWindow*, QDropEvent*);
        static void        closeAll();
        static MainWindow* toggleWindow(MainWindow*);
        static MainWindow* mainMainWindow();
        static MainWindow* firstWindow()      { return mWindowList.isEmpty() ? nullptr : mWindowList[0]; }
        static int         count()            { return mWindowList.count(); }

    public Q_SLOTS:
        virtual void   show();

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
        void           slotNew(EditAlarmDlg::Type);
        void           slotNewDisplay()   { slotNew(EditAlarmDlg::DISPLAY); }
        void           slotNewCommand()   { slotNew(EditAlarmDlg::COMMAND); }
        void           slotNewEmail()     { slotNew(EditAlarmDlg::EMAIL); }
        void           slotNewAudio()     { slotNew(EditAlarmDlg::AUDIO); }
        void           slotNewFromTemplate(const KAEvent*);
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
        void           slotShowMenubar();
        void           slotConfigureKeys();
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
        void           slotCalendarStatusChanged();
        void           slotAlarmListColumnsChanged();
        void           resourcesResized();
        void           showErrorMessage(const QString&);
        void           editAlarmOk();
        void           editAlarmDeleted(QObject*);

    private:
        typedef QList<MainWindow*> WindowList;

        explicit MainWindow(bool restored);
        void           createListView(bool recreate);
        void           initActions();
        void           initCalendarResources();
        void           selectionCleared();
        void           setEnableText(bool enable);
        void           initUndoMenu(QMenu*, Undo::Type);
        void           slotDelete(bool force);
        static KAEvent::SubAction  getDropAction(QDropEvent*, QString& text);
        static void    setUpdateTimer();
        static void    enableTemplateMenuItem(bool);

        static WindowList    mWindowList;   // active main windows
        static TemplateDlg*  mTemplateDlg;  // the one and only template dialog

        AlarmListModel*      mListFilterModel;
        AlarmListView*       mListView;
        ResourceSelector*    mResourceSelector;    // resource selector widget
        QSplitter*           mSplitter;            // splits window into list and resource selector
        QMap<EditAlarmDlg*, KAEvent> mEditAlarmMap; // edit alarm dialogs to be handled by this window
        KToggleAction*       mActionToggleResourceSel;
        QAction*             mActionImportAlarms;
        QAction*             mActionExportAlarms;
        QAction*             mActionExport;
        QAction*             mActionImportBirthdays;
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
        QMenu*               mActionsMenu;
        QMenu*               mContextMenu;
        QMap<QAction*, int>  mUndoMenuIds;         // items in the undo/redo menu, in order of appearance
        int                  mResourcesWidth {-1}; // width of resource selector widget
        bool                 mHiddenTrayParent {false}; // on session restoration, hide this window
        bool                 mShowResources;       // show resource selector
        bool                 mShowArchived;        // include archived alarms in the displayed list
        bool                 mShown {false};       // true once the window has been displayed
        bool                 mActionEnableEnable;  // Enable/Disable action is set to "Enable"
        bool                 mMenuError;           // error occurred creating menus: need to show error message
        bool                 mResizing {false};    // window resize is in progress
};

#endif // MAINWINDOW_H

// vim: et sw=4:
