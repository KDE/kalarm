/*
 *  traywindow.h  -  the KDE system tray applet
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
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

#ifndef DOCKWINDOW_H
#define DOCKWINDOW_H

#include <qtimer.h>

#include <ksystemtray.h>
#include <kpopupmenu.h>
class KAction;


class TrayWindow : public KSystemTray
{
		Q_OBJECT
	public:
		TrayWindow(const char* name = 0L);
		~TrayWindow();

		void         updateCalendarStatus(bool monitoring);

	public slots:
		void         toggleAlarmsEnabled();
		void         slotKAlarm();
		void         slotConfigKAlarm();
		void         slotConfigDaemon();
		void         slotQuit();

	protected:
		virtual void contextMenuAboutToShow(KPopupMenu*);
		void         mousePressEvent(QMouseEvent*);

	private slots:
		void         checkDaemonRunning();
		void         slotSettingsChanged();

	private:
		void         registerWithDaemon();
		void         enableCalendar(bool enable);
		void         setDaemonStatus(bool running);
		bool         isDaemonRunning(bool updateDockWindow = true);
		void         setFastDaemonCheck();

		QPixmap    mPixmapEnabled, mPixmapDisabled;
		KAction*   mActionQuit;          // quit action for system tray window
		int        mAlarmsEnabledId;     // alarms enabled item in menu
		QTimer     mDaemonStatusTimer;   // timer for checking daemon status
		int        mDaemonStatusTimerCount; // countdown for fast status checking
		bool       mDaemonRunning;       // whether the alarm daemon is currently running
		int        mDaemonStatusTimerInterval;  // timer interval (seconds) for checking daemon status
		bool       mQuitReplaced;        // the context menu Quit item has been replaced
		bool       mCalendarDisabled;    // monitoring of calendar is currently disabled
		bool       mEnableCalPending;    // waiting to tell daemon to enable calendar

};

#endif // DOCKWINDOW_H
