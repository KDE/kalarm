/*
 *  mainwindow.h  -  main application window
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "msgevent.h"
using namespace KCal;

#include "mainwindowbase.h"

class QListViewItem;
class KAction;
class AlarmListView;


class KAlarmMainWindow : public MainWindowBase
{
		Q_OBJECT

	public:
		KAlarmMainWindow();
		~KAlarmMainWindow();
		bool           trayParent() const;
		bool           hiddenTrayParent() const     { return mHiddenTrayParent; }

		void           modifyEvent(const KAlarmEvent& event)    { modifyEvent(event.id(), event); }
		void           modifyEvent(const QString& oldEventID, const KAlarmEvent& newEvent);
		void           deleteEvent(const KAlarmEvent&);

		static void    addEvent(const KAlarmEvent&, KAlarmMainWindow*);
		static void    modifyEvent(const QString& oldEventID, const KAlarmEvent& newEvent, KAlarmMainWindow*);
		static void    modifyEvent(const KAlarmEvent& event, KAlarmMainWindow* w)   { modifyEvent(event.id(), event, w); }
		static void    deleteEvent(const KAlarmEvent&, KAlarmMainWindow*);
		static void              closeAll();
		static KAlarmMainWindow* toggleWindow(KAlarmMainWindow*);
		static KAlarmMainWindow* firstWindow()      { return windowList.first(); }
		static int               count()            { return windowList.count(); }

	protected:
		virtual void   resizeEvent(QResizeEvent*);
		virtual void   showEvent(QShowEvent*);
		virtual void   closeEvent(QCloseEvent*);
		virtual void   saveProperties(KConfig*);
		virtual void   readProperties(KConfig*);

	private slots:
		void           slotDelete();
		void           slotNew();
		void           slotModify();
		void           slotToggleTrayIcon();
		void           slotResetDaemon();
		void           slotQuit();
		void           slotDeletion();
		void           slotSelection(QListViewItem*);
		void           slotMouseClicked(int button, QListViewItem* item, const QPoint&, int);
		void           slotSettingsChanged();
		void           updateTrayIconAction();
		void           updateActionsMenu();
		void           setAlarmEnabledStatus(bool status);

	private:
		void           initActions();
		static bool    findWindow(KAlarmMainWindow*);

		static QPtrList<KAlarmMainWindow> windowList;  // active main windows
		AlarmListView* listView;
		KAction*       actionNew;
		KAction*       actionModify;
		KAction*       actionDelete;
		KAction*       actionToggleTrayIcon;
		KAction*       actionResetDaemon;
		KAction*       actionQuit;
		int            mViewMenuId;
		KPopupMenu*    mViewMenu;
		KPopupMenu*    mActionsMenu;
		int            mAlarmsEnabledId;     // alarms enabled item in Actions menu
		bool           mHiddenTrayParent;    // on session restoration, hide this window
};

#endif // MAINWINDOW_H

