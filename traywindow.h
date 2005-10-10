/*
 *  traywindow.h  -  the KDE system tray applet
 *  Program:  kalarm
 *  Copyright (c) 2002 - 2004 by David Jarvie <software@astrojar.org.uk>
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

//Added by qt3to4:
#include <QPixmap>
#include <ksystemtray.h>

class QEvent;
class QMouseEvent;
class QDragEnterEvent;
class QDropEvent;
class KPopupMenu;
class KAEvent;
class MainWindow;

class TrayWindow : public KSystemTray
{
		Q_OBJECT
	public:
		TrayWindow(MainWindow* parent, const char* name = 0);
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
		virtual void contextMenuAboutToShow(KPopupMenu*);
		virtual void mousePressEvent(QMouseEvent*);
		virtual void mouseReleaseEvent(QMouseEvent*);
		virtual void dragEnterEvent(QDragEnterEvent*);
		virtual void dropEvent(QDropEvent*);
		virtual bool event(QEvent*);

	private slots:
		void         slotNewAlarm();
		void         slotNewFromTemplate(const KAEvent&);
		void         slotPreferences();
		void         setEnabledStatus(bool status);

	private:

		MainWindow*  mAssocMainWindow;     // main window associated with this, or null
		QPixmap      mPixmapEnabled, mPixmapDisabled;
};

#endif // TRAYWINDOW_H
