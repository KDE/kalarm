/*
 *  mainwindow.h  -  main application window
 *  Program:  kalarm
 *  Copyright © 2001-2008 by David Jarvie <djarvie@kde.org>
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/** @file mainwindow.h - main application window */

#include <QList>
#include <QMap>

#include <kcal/calendar.h>

#include "alarmevent.h"
#include "alarmresources.h"
#include "editdlg.h"
#include "mainwindowbase.h"
#include "undo.h"

class QDragEnterEvent;
class QHideEvent;
class QShowEvent;
class QResizeEvent;
class QDropEvent;
class QCloseEvent;
class QModelIndex;
class QSplitter;
class QMenu;
class KAction;
class KToggleAction;
class KToolBarPopupAction;
class AlarmListFilterModel;
class AlarmListDelegate;
class AlarmListView;
class NewAlarmAction;
class TemplateDlg;
class TemplateMenuAction;
class ResourceSelector;


class MainWindow : public MainWindowBase, public KCal::Calendar::CalendarObserver
{
		Q_OBJECT

	public:
		static MainWindow* create(bool restored = false);
		~MainWindow();
		bool               isTrayParent() const;
		bool               isHiddenTrayParent() const   { return mHiddenTrayParent; }
		bool               showingArchived() const      { return mShowArchived; }
		void               selectEvent(const QString& eventID);
		virtual bool       eventFilter(QObject*, QEvent*);

		static void        refresh();
		static void        executeDragEnterEvent(QDragEnterEvent*, QWidget* recipient);
		static void        executeDropEvent(MainWindow*, QDropEvent*);
		static void        closeAll();
		static MainWindow* toggleWindow(MainWindow*);
		static MainWindow* mainMainWindow();
		static MainWindow* firstWindow()      { return mWindowList.isEmpty() ? 0 : mWindowList[0]; }
		static int         count()            { return mWindowList.count(); }

		static QString i18n_a_ShowAlarmTimes();       // text of 'Show Alarm Times' action, with 'A' shortcut
		static QString i18n_chk_ShowAlarmTime();      // text of 'Show alarm time' checkbox
		static QString i18n_o_ShowTimeToAlarms();     // text of 'Show Time to Alarms' action, with 'O' shortcut
		static QString i18n_chk_ShowTimeToAlarm();    // text of 'Show time until alarm' checkbox
		static QString i18n_a_ShowArchivedAlarms();   // text of 'Show Archived Alarms' action, with 'A' shortcut
		static QString i18n_tip_ShowArchivedAlarms(); // text of 'Show Archived Alarms' tooltip
		static QString i18n_tip_HideArchivedAlarms(); // text of 'Hide Archived Alarms' tooltip

	public slots:
		virtual void   show();

	protected:
		virtual void   resizeEvent(QResizeEvent*);
		virtual void   showEvent(QShowEvent*);
		virtual void   hideEvent(QHideEvent*);
		virtual void   closeEvent(QCloseEvent*);
		virtual void   dragEnterEvent(QDragEnterEvent*);
		virtual void   dropEvent(QDropEvent*);
		virtual void   saveProperties(KConfigGroup&);
		virtual void   readProperties(const KConfigGroup&);

	private slots:
		void           slotNew(EditAlarmDlg::Type);
		void           slotNewFromTemplate(const KAEvent&);
		void           slotNewTemplate();
		void           slotCopy();
		void           slotModify();
		void           slotDelete();
		void           slotReactivate();
		void           slotEnable();
		void           slotToggleTrayIcon();
		void           slotResetDaemon();
		void           slotImportAlarms();
		void           slotBirthdays();
		void           slotTemplates();
		void           slotTemplatesEnd();
		void           slotPreferences();
		void           slotConfigureKeys();
		void           slotConfigureToolbar();
		void           slotNewToolbarConfig();
		void           slotQuit();
		void           slotSelection();
		void           slotContextMenuRequested(const QPoint& globalPos);
		void           slotDoubleClicked(const QModelIndex&);
		void           slotShowTime();
		void           slotShowTimeTo();
		void           slotShowArchived();
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
		void           updateActionsMenu();
		void           slotToggleResourceSelector();
		void           slotResourceStatusChanged();
		void           resourcesResized();
		void           showErrorMessage(const QString&);

	private:
		typedef QList<MainWindow*> WindowList;

		explicit MainWindow(bool restored);
		void           createListView(bool recreate);
		void           initActions();
		void           initCalendarResources();
		void           selectionCleared();
		void           setEnableText(bool enable);
		void           initUndoMenu(QMenu*, Undo::Type);
		static KAEvent::Action  getDropAction(QDropEvent*, QString& text);
		static void    setUpdateTimer();
		static void    enableTemplateMenuItem(bool);

		static WindowList    mWindowList;   // active main windows
		static TemplateDlg*  mTemplateDlg;  // the one and only template dialog

		AlarmListFilterModel* mListFilterModel;
		AlarmListView*       mListView;
		AlarmListDelegate*   mListDelegate;
		ResourceSelector*    mResourceSelector;    // resource selector widget
		QSplitter*           mSplitter;            // splits window into list and resource selector
		AlarmResources*      mAlarmResources;      // calendar resources to use for this window
		KToggleAction*       mActionToggleResourceSel;
		KAction*             mActionImportAlarms;
		KAction*             mActionImportBirthdays;
		KAction*             mActionTemplates;
		NewAlarmAction*      mActionNew;
		TemplateMenuAction*  mActionNewFromTemplate;
		KAction*             mActionCreateTemplate;
		KAction*             mActionCopy;
		KAction*             mActionModify;
		KAction*             mActionDelete;
		KAction*             mActionReactivate;
		KAction*             mActionEnable;
		KAction*             mActionFindNext;
		KAction*             mActionFindPrev;
		KToolBarPopupAction* mActionUndo;
		KToolBarPopupAction* mActionRedo;
		KToggleAction*       mActionToggleTrayIcon;
		KToggleAction*       mActionShowTime;
		KToggleAction*       mActionShowTimeTo;
		KToggleAction*       mActionShowArchived;
		KMenu*               mActionsMenu;
		KMenu*               mContextMenu;
		QMap<QAction*, int>  mUndoMenuIds;         // items in the undo/redo menu, in order of appearance
		int                  mResourcesWidth;      // width of resource selector widget
		bool                 mMinuteTimerActive;   // minute timer is active
		bool                 mHiddenTrayParent;    // on session restoration, hide this window
		bool                 mShowResources;       // show resource selector
		bool                 mShowArchived;        // include archived alarms in the displayed list
		bool                 mShowTime;            // show alarm times
		bool                 mShowTimeTo;          // show time-to-alarms
		bool                 mShown;               // true once the window has been displayed
		bool                 mActionEnableEnable;  // Enable/Disable action is set to "Enable"
		bool                 mMenuError;           // error occurred creating menus: need to show error message
		bool                 mResizing;            // window resize is in progress
};

#endif // MAINWINDOW_H

