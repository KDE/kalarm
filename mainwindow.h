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

#include <qvariant.h>
class QListViewItem;

#include <kapp.h>
class KAction;

#include "mainwindowbase.h"

#include "msgevent.h"
using namespace KCal;

class AlarmListView;


class KAlarmMainWindow : public MainWindowBase
{
		Q_OBJECT

	public:
		KAlarmMainWindow();
		~KAlarmMainWindow();

		void  addMessage(const KAlarmEvent&);
		void  modifyMessage(const KAlarmEvent& event)    { modifyMessage(event.id(), event); }
		void  modifyMessage(const QString& oldEventID, const KAlarmEvent& newEvent);
		void  deleteMessage(const KAlarmEvent&);

	protected:
		virtual void resizeEvent(QResizeEvent*);
		virtual void showEvent(QShowEvent*);

	private:
		void         initActions();

		AlarmListView*  listView;
		KAction*        actionNew;
		KAction*        actionModify;
		KAction*        actionDelete;
		KAction*        actionToggleTrayIcon;
		KAction*        actionResetDaemon;
		KAction*        actionQuit;

	protected slots:
		virtual void slotDelete();
		virtual void slotNew();
		virtual void slotModify();
		virtual void slotToggleTrayIcon();
		virtual void slotResetDaemon();
//		virtual void slotPreferences();
		virtual void slotQuit();
		virtual void slotSelection();
		virtual void slotListRightClick(QListViewItem*, const QPoint&, int);
		virtual void setTrayIconActionText();
};

#endif // MAINWINDOW_H

