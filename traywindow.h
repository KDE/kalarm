/*
 *  traywindow.h  -  the KDE system tray applet
 *  Program:  kalarm
 *  Copyright © 2002-2010 by David Jarvie <djarvie@kde.org>
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

#ifndef TRAYWINDOW_H
#define TRAYWINDOW_H

#include "editdlg.h"
#include "kaevent.h"
#include <ksystemtrayicon.h>
#include <QIcon>

class QEvent;
class QDragEnterEvent;
class QDropEvent;
class KAction;
class KToggleAction;
class KAEvent;
class MainWindow;
class NewAlarmAction;
class TemplateMenuAction;

class TrayWindow : public KSystemTrayIcon
{
		Q_OBJECT
	public:
		explicit TrayWindow(MainWindow* parent);
		~TrayWindow();
		void         removeWindow(MainWindow*);
		MainWindow*  assocMainWindow() const               { return mAssocMainWindow; }
		void         setAssocMainWindow(MainWindow* win)   { mAssocMainWindow = win; }

	signals:
		void         deleted();

	protected:
		virtual void dragEnterEvent(QDragEnterEvent*);
		virtual void dropEvent(QDropEvent*);
		virtual bool event(QEvent*);

	private slots:
		void         slotActivated(QSystemTrayIcon::ActivationReason reason);
		void         slotNewAlarm(EditAlarmDlg::Type);
		void         slotNewFromTemplate(const KAEvent*);
		void         slotPreferences();
		void         setEnabledStatus(bool status);
		void         slotHaveDisabledAlarms(bool disabled);
		void         slotResourceStatusChanged();
		void         slotQuit();

	private:
		QString      tooltipAlarmText() const;

		MainWindow*     mAssocMainWindow;     // main window associated with this, or null
		QIcon           mIconEnabled;         // normal status icon
		QIcon           mIconDisabled;        // icon indicating all alarms disabled
		QIcon           mIconSomeDisabled;    // icon indicating individual alarms disabled
		KToggleAction*  mActionEnabled;
		NewAlarmAction* mActionNew;
		TemplateMenuAction* mActionNewFromTemplate;
		bool            mHaveDisabledAlarms;  // some individually disabled alarms exist
};

#endif // TRAYWINDOW_H
