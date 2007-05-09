/*
 *  traywindow.h  -  the KDE system tray applet
 *  Program:  kalarm
 *  Copyright © 2002-2004,2007 by David Jarvie <software@astrojar.org.uk>
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

#include <qpixmap.h>
#include <ksystemtray.h>
class KPopupMenu;

class KAEvent;
class MainWindow;
class TrayTooltip;

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

	private slots:
		void         slotNewAlarm();
		void         slotNewFromTemplate(const KAEvent&);
		void         slotPreferences();
		void         setEnabledStatus(bool status);

	private:
		friend class TrayTooltip;

		MainWindow*  mAssocMainWindow;     // main window associated with this, or null
		QPixmap      mPixmapEnabled, mPixmapDisabled;
		TrayTooltip* mTooltip;
};

#endif // TRAYWINDOW_H
