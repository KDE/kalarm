/*
 *  mainwindow.h  -  main application window
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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
#include "mainwindowbase.h"

class QListViewItem;
class KAction;
class AlarmListView;


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
		void           modifyEvent(const KAlarmEvent& event)    { modifyEvent(event.id(), event); }
		void           modifyEvent(const QString& oldEventID, const KAlarmEvent& newEvent);
		void           deleteEvent(const QString& eventID);
		void           undeleteEvent(const QString& oldEventID, const KAlarmEvent& event);

		static void    refresh();
		static void    updateExpired();
		static void    addEvent(const KAlarmEvent&, KAlarmMainWindow*);
		static void    modifyEvent(const QString& oldEventID, const KAlarmEvent& newEvent, KAlarmMainWindow*);
		static void    modifyEvent(const KAlarmEvent& event, KAlarmMainWindow* w)   { modifyEvent(event.id(), event, w); }
		static void    deleteEvent(const QString& eventID, KAlarmMainWindow*);
		static void    undeleteEvent(const QString& oldEventID, const KAlarmEvent& event, KAlarmMainWindow*);
		static void    executeNew(KAlarmMainWindow* = 0, KAlarmEvent::Action = KAlarmEvent::MESSAGE, const QString& text = QString::null);
		static void    executeDragEnterEvent(QDragEnterEvent*);
		static void    executeDropEvent(KAlarmMainWindow*, QDropEvent*);
		static void              closeAll();
		static QString emailSubject(const QString&);
		static KAlarmMainWindow* toggleWindow(KAlarmMainWindow*);
		static KAlarmMainWindow* mainMainWindow();
		static KAlarmMainWindow* firstWindow()      { return windowList.first(); }
		static int               count()            { return windowList.count(); }

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
		void           slotCopy();
		void           slotModify();
		void           slotDelete();
		void           slotUndelete();
		void           slotView();
		void           slotToggleTrayIcon();
		void           slotResetDaemon();
		void           slotBirthdays();
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
		void           setAlarmEnabledStatus(bool status);

	private:
		KAlarmMainWindow(bool restored);
		void           createListView(bool recreate);
		void           initActions();
		static KAlarmEvent::Action  getDropAction(QDropEvent*, QString& text);
		static void    setUpdateTimer();
		static bool    findWindow(KAlarmMainWindow*);

		static QPtrList<KAlarmMainWindow> windowList;  // active main windows
		AlarmListView* listView;
		KAction*       actionNew;
		KAction*       actionCopy;
		KAction*       actionModify;
		KAction*       actionView;
		KAction*       actionDelete;
		KAction*       actionUndelete;
		KAction*       actionToggleTrayIcon;
		KAction*       actionRefreshAlarms;
		KAction*       actionShowTime;
		KAction*       actionShowTimeTo;
		KAction*       actionShowExpired;
		KAction*       actionQuit;
		int            mShowTimeId;
		int            mShowTimeToId;
		int            mShowExpiredId;
		int            mShowTrayId;
		KPopupMenu*    mActionsMenu;
		KPopupMenu*    mViewMenu;
		int            mAlarmsEnabledId;     // alarms enabled item in Actions menu
		QTimer*        mMinuteTimer;         // timer every minute
		bool           mMinuteTimerSyncing;  // minute timer hasn't synchronised to minute boundary
		bool           mHiddenTrayParent;    // on session restoration, hide this window
		bool           mShowTime;            // show alarm times
		bool           mShowTimeTo;          // show time-to-alarms
		bool           mShowExpired;         // include expired alarms in the displayed list
};

#endif // MAINWINDOW_H

