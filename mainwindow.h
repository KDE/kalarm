/*
 *  mainwindow.h  -  main application window
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "alarmevent.h"
#include "alarmtext.h"
#include "mainwindowbase.h"
#include "undo.h"

class QListViewItem;
class KAction;
class KToggleAction;
class KToolBarPopupAction;
class KPopupMenu;
class ActionAlarmsEnabled;
class AlarmListView;
class TemplateDlg;
class TemplateMenuAction;


class MainWindow : public MainWindowBase
{
		Q_OBJECT

	public:
		static MainWindow* create(bool restored = false);
		~MainWindow();
		bool               isTrayParent() const;
		bool               isHiddenTrayParent() const   { return mHiddenTrayParent; }
		bool               showingExpired() const       { return mShowExpired; }
		void               selectEvent(const QString& eventID);

		static void        refresh();
		static void        updateExpired();
		static void        updateTimeColumns(bool oldTime, bool oldTimeTo);
		static void        addEvent(const KAEvent&, MainWindow*);
		static void        executeNew(MainWindow* w = 0, KAEvent::Action a = KAEvent::MESSAGE, const AlarmText& t = AlarmText())
		                                      { executeNew(w, 0, a, t); }
		static void        executeNew(const KAEvent& e, MainWindow* w = 0)
		                                      { executeNew(w, &e); }
		static void        executeDragEnterEvent(QDragEnterEvent*);
		static void        executeDropEvent(MainWindow*, QDropEvent*);
		static void        closeAll();
		static MainWindow* toggleWindow(MainWindow*);
		static MainWindow* mainMainWindow();
		static MainWindow* firstWindow()      { return mWindowList.first(); }
		static int         count()            { return mWindowList.count(); }

		static QString i18n_a_ShowAlarmTimes();     // text of 'Show Alarm Times' checkbox, with 'A' shortcut
		static QString i18n_t_ShowAlarmTime();      // text of 'Show alarm time' checkbox, with 'T' shortcut
		static QString i18n_m_ShowAlarmTime();      // text of 'Show alarm time' checkbox, with 'M' shortcut
		static QString i18n_o_ShowTimeToAlarms();   // text of 'Show Time to Alarms' checkbox, with 'O' shortcut
		static QString i18n_n_ShowTimeToAlarm();    // text of 'Show time until alarm' checkbox, with 'N' shortcut
		static QString i18n_l_ShowTimeToAlarm();    // text of 'Show time until alarm' checkbox, with 'L' shortcut
		static QString i18n_ShowExpiredAlarms();    // plain text of 'Show Expired Alarms' action
		static QString i18n_e_ShowExpiredAlarms();  // text of 'Show Expired Alarms' checkbox, with 'E' shortcut
		static QString i18n_HideExpiredAlarms();    // plain text of 'Hide Expired Alarms' action
		static QString i18n_e_HideExpiredAlarms();  // text of 'Hide Expired Alarms' action, with 'E' shortcut

	public slots:
		virtual void   show();

	protected:
		virtual void   resizeEvent(QResizeEvent*);
		virtual void   showEvent(QShowEvent*);
		virtual void   hideEvent(QHideEvent*);
		virtual void   closeEvent(QCloseEvent*);
		virtual void   dragEnterEvent(QDragEnterEvent*);
		virtual void   dropEvent(QDropEvent*);
		virtual void   saveProperties(KConfig*);
		virtual void   readProperties(KConfig*);

	private slots:
		void           slotNew();
		void           slotNewFromTemplate(const KAEvent&);
		void           slotNewTemplate();
		void           slotCopy();
		void           slotModify();
		void           slotDelete();
		void           slotReactivate();
		void           slotView();
		void           slotEnable();
		void           slotToggleTrayIcon();
		void           slotResetDaemon();
		void           slotBirthdays();
		void           slotTemplates();
		void           slotTemplatesEnd();
		void           slotPreferences();
		void           slotConfigureKeys();
		void           slotConfigureToolbar();
		void           slotQuit();
		void           slotDeletion();
		void           slotSelection();
		void           slotMouseClicked(int button, QListViewItem* item, const QPoint&, int);
		void           slotDoubleClicked(QListViewItem*);
		void           slotShowTime();
		void           slotShowTimeTo();
		void           slotShowExpired();
		void           slotUpdateTimeTo();
		void           slotUndo();
		void           slotUndoItem(int id);
		void           slotRedo();
		void           slotRedoItem(int id);
		void           slotInitUndoMenu();
		void           slotInitRedoMenu();
		void           slotUndoStatus(const QString&, const QString&);
		void           slotFindActive(bool);
		void           updateTrayIconAction();
		void           updateActionsMenu();

	private:
		MainWindow(bool restored);
		void           createListView(bool recreate);
		void           initActions();
		void           setEnableText(bool enable);
		static KAEvent::Action  getDropAction(QDropEvent*, QString& text);
		static void    executeNew(MainWindow*, const KAEvent*, KAEvent::Action = KAEvent::MESSAGE, const AlarmText& = AlarmText());
		static void    initUndoMenu(KPopupMenu*, Undo::Type);
		static void    setUpdateTimer();
		static void    enableTemplateMenuItem(bool);
		static void    alarmWarnings(QWidget* parent, const KAEvent* = 0);
		static bool    findWindow(MainWindow*);

		static QPtrList<MainWindow> mWindowList;   // active main windows
		static TemplateDlg*         mTemplateDlg;  // the one and only template dialogue

		AlarmListView*       mListView;
		KAction*             mActionTemplates;
		KAction*             mActionNew;
		TemplateMenuAction*  mActionNewFromTemplate;
		KAction*             mActionCreateTemplate;
		KAction*             mActionCopy;
		KAction*             mActionModify;
		KAction*             mActionView;
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
		KToggleAction*       mActionShowExpired;
		KPopupMenu*          mActionsMenu;
		KPopupMenu*          mContextMenu;
		bool                 mMinuteTimerActive;   // minute timer is active
		bool                 mHiddenTrayParent;    // on session restoration, hide this window
		bool                 mShowExpired;         // include expired alarms in the displayed list
		bool                 mShowTime;            // show alarm times
		bool                 mShowTimeTo;          // show time-to-alarms
		bool                 mActionEnableEnable;  // Enable/Disable action is set to "Enable"
		bool                 mMenuError;           // error occurred creating menus: need to show error message
};

#endif // MAINWINDOW_H

