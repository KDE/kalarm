/*
 *  mainwindow.h  -  main application window
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "alarmevent.h"
#include "alarmtext.h"
#include "mainwindowbase.h"

class QListViewItem;
class KAction;
class KToggleAction;
class ActionAlarmsEnabled;
class AlarmListView;
class TemplateDlg;


class KAlarmMainWindow : public MainWindowBase
{
		Q_OBJECT

	public:
		static KAlarmMainWindow* create(bool restored = false);
		~KAlarmMainWindow();
		bool           isTrayParent() const;
		bool           isHiddenTrayParent() const   { return mHiddenTrayParent; }
		bool           showingExpired() const       { return mShowExpired; }
		void           selectEvent(const QString& eventID);

		static void    refresh();
		static void    updateExpired();
		static void    updateTimeColumns(bool oldTime, bool oldTimeTo);
		static void    addEvent(const KAEvent&, KAlarmMainWindow*);
		static void    executeNew(KAlarmMainWindow* = 0, KAEvent::Action = KAEvent::MESSAGE, const AlarmText& = AlarmText());
		static void    executeDragEnterEvent(QDragEnterEvent*);
		static void    executeDropEvent(KAlarmMainWindow*, QDropEvent*);
		static void    closeAll();
		static KAlarmMainWindow* toggleWindow(KAlarmMainWindow*);
		static KAlarmMainWindow* mainMainWindow();
		static KAlarmMainWindow* firstWindow()      { return mWindowList.first(); }
		static int               count()            { return mWindowList.count(); }

		static QString i18n_a_ShowAlarmTimes();     // text of 'Show Alarm Times' checkbox, with 'A' shortcut
		static QString i18n_t_ShowAlarmTime();      // text of 'Show alarm time' checkbox, with 'T' shortcut
		static QString i18n_m_ShowAlarmTime();      // text of 'Show alarm time' checkbox, with 'M' shortcut
		static QString i18n_o_ShowTimeToAlarms();   // text of 'Show Time to Alarms' checkbox, with 'O' shortcut
		static QString i18n_n_ShowTimeToAlarm();    // text of 'Show time until alarm' checkbox, with 'N' shortcut
		static QString i18n_l_ShowTimeToAlarm();    // text of 'Show time until alarm' checkbox, with 'L' shortcut
		static QString i18n_e_ShowExpiredAlarms();  // text of 'Show Expired Alarms' checkbox, with 'E' shortcut
		static QString i18n_s_ShowExpiredAlarms();  // text of 'Show expired alarms' checkbox, with 'S' shortcut

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
		void           slotNewTemplate();
		void           slotCopy();
		void           slotModify();
		void           slotDelete();
		void           slotUndelete();
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
		void           updateTrayIconAction();
		void           updateActionsMenu();

	private:
		KAlarmMainWindow(bool restored);
		void           createListView(bool recreate);
		void           initActions();
		void           setEnableText(bool enable);
		static KAEvent::Action  getDropAction(QDropEvent*, QString& text);
		static void    setUpdateTimer();
		static void    enableTemplateMenuItem(bool);
		static void    alarmWarnings(QWidget* parent, const KAEvent* = 0);
		static bool    findWindow(KAlarmMainWindow*);

		static QPtrList<KAlarmMainWindow> mWindowList;  // active main windows
		static TemplateDlg* mTemplateDlg;      // the one and only template dialogue
		AlarmListView* mListView;
		KAction*       mActionTemplates;
		KAction*       mActionNew;
		KAction*       mActionCreateTemplate;
		KAction*       mActionCopy;
		KAction*       mActionModify;
		KAction*       mActionView;
		KAction*       mActionDelete;
		KAction*       mActionUndelete;
		KAction*       mActionEnable;
		KToggleAction* mActionToggleTrayIcon;
		KToggleAction* mActionShowTime;
		KToggleAction* mActionShowTimeTo;
		KToggleAction* mActionShowExpired;
		KPopupMenu*    mActionsMenu;
		KPopupMenu*    mContextMenu;
		bool           mMinuteTimerActive;   // minute timer is active
		bool           mHiddenTrayParent;    // on session restoration, hide this window
		bool           mShowExpired;         // include expired alarms in the displayed list
		bool           mShowTime;            // show alarm times
		bool           mShowTimeTo;          // show time-to-alarms
		bool           mActionEnableEnable;  // Enable/Disable action is set to "Enable"
		bool           mMenuError;           // error occurred creating menus: need to show error message
};

#endif // MAINWINDOW_H

