/*
 *  traywindow.h  -  the KDE system tray applet
 *  Program:  kalarm
 *  Copyright © 2002-2004,2006 by David Jarvie <software@astrojar.org.uk>
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

#include <QIcon>
#include <ksystemtrayicon.h>

class QEvent;
class QMouseEvent;
class QDragEnterEvent;
class QDropEvent;
class KMenu;
class KAEvent;
class MainWindow;

class TrayWindow : public KSystemTrayIcon
{
		Q_OBJECT
	public:
		TrayWindow(MainWindow* parent);
		~TrayWindow();
		void         removeWindow(MainWindow*);
		MainWindow*  assocMainWindow() const               { return mAssocMainWindow; }
		void         setAssocMainWindow(MainWindow* win)   { mAssocMainWindow = win; }
		bool         inSystemTray() const;
		void         tooltipAlarmText(QString& text) const;

	public slots:
		void         slotQuit();

	signals:
		void         deleted();

	protected:
		virtual void contextMenuAboutToShow(KMenu*);
		virtual void dragEnterEvent(QDragEnterEvent*);
		virtual void dropEvent(QDropEvent*);
		virtual bool event(QEvent*);

	private slots:
                void         slotActivated(QSystemTrayIcon::ActivationReason reason);
		void         slotNewAlarm();
		void         slotNewFromTemplate(const KAEvent&);
		void         slotPreferences();
		void         setEnabledStatus(bool status);
		void         slotResourceStatusChanged();

	private:

		MainWindow*  mAssocMainWindow;     // main window associated with this, or null
		QIcon        mIconEnabled, mIconDisabled;
		KAction*     mActionNew;
		KAction*     mActionNewFromTemplate;
};

#endif // TRAYWINDOW_H
