/*
 *  traywindow.h  -  the KDE system tray applet
 *  Program:  kalarm
 *  (C) 2002 - 2004 by David Jarvie <software@astrojar.org.uk>
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
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
 */

#ifndef TRAYWINDOW_H
#define TRAYWINDOW_H

#include <ksystemtray.h>
class KPopupMenu;

class KAlarmMainWindow;
class TrayTooltip;

class TrayWindow : public KSystemTray
{
		Q_OBJECT
	public:
		TrayWindow(KAlarmMainWindow* parent, const char* name = 0);
		~TrayWindow();
		void               removeWindow(KAlarmMainWindow*);
		KAlarmMainWindow*  assocMainWindow() const  { return mAssocMainWindow; }
		void               setAssocMainWindow(KAlarmMainWindow* win)   { mAssocMainWindow = win; }
		bool               inSystemTray() const;
		void               tooltipAlarmText(QString& text) const;

	public slots:
		void               slotQuit();

	signals:
		void               deleted();

	protected:
		virtual void       contextMenuAboutToShow(KPopupMenu*);
		virtual void       mousePressEvent(QMouseEvent*);
		virtual void       mouseReleaseEvent(QMouseEvent*);
		virtual void       dragEnterEvent(QDragEnterEvent*);
		virtual void       dropEvent(QDropEvent*);

	private slots:
		void               slotNewAlarm();
		void               slotPreferences();
		void               setEnabledStatus(bool status);

	private:
		friend class TrayTooltip;

		KAlarmMainWindow*  mAssocMainWindow;     // main window associated with this, or null
		QPixmap            mPixmapEnabled, mPixmapDisabled;
		TrayTooltip*       mTooltip;
};

#endif // TRAYWINDOW_H
