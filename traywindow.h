/*
 *  traywindow.h  -  the KDE system tray applet
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#ifndef TRAYWINDOW_H
#define TRAYWINDOW_H

#include <ksystemtray.h>
class KPopupMenu;
class KAction;
class KActionCollection;

class KAlarmMainWindow;


class TrayWindow : public KSystemTray
{
		Q_OBJECT
	public:
		TrayWindow(KAlarmMainWindow* parent, const char* name = 0);
		~TrayWindow();
		void              removeWindow(KAlarmMainWindow*);
		KAlarmMainWindow* assocMainWindow() const  { return mAssocMainWindow; }
		void              setAssocMainWindow(KAlarmMainWindow* win)   { mAssocMainWindow = win; }
		bool              inSystemTray() const;
		static bool       quitWarning();
		static void       setQuitWarning(bool warn);

	public slots:
		void              slotQuit();

	signals:
		void              deleted();

	protected:
		virtual void      contextMenuAboutToShow(KPopupMenu*);
		virtual void      mousePressEvent(QMouseEvent*);
		virtual void      mouseReleaseEvent(QMouseEvent*);

	private slots:
		void              setEnabledStatus(bool status);

	private:
		KAlarmMainWindow*  mAssocMainWindow;     // main window associated with this, or null
		QPixmap            mPixmapEnabled, mPixmapDisabled;
		KAction*           mActionQuit;          // quit action for system tray window
		int                mAlarmsEnabledId;     // alarms enabled item in menu
		bool               mQuitReplaced;        // the context menu Quit item has been replaced
		KActionCollection* mActionCollection;
};

#endif // TRAYWINDOW_H
