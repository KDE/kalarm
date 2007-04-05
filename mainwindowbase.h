/*
 *  mainwindowbase.h  -  base class for main application windows
 *  Program:  kalarm
 *  Copyright © 2002 by David Jarvie  software@astrojar.org.uk
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

#ifndef MAINWINDOWBASE_H
#define MAINWINDOWBASE_H

#include <kmainwindow.h>


/**
 *  The MainWindowBase class is a base class for KAlarm's main window and message window.
 *  When the window is closed, it only allows the application to quit if there is no
 *  system tray icon displayed.
 *
 *  @author David Jarvie <software@astrojar.org.uk>
 */
class MainWindowBase : public KMainWindow
{
		Q_OBJECT

	public:
		explicit MainWindowBase(QWidget* parent = 0, const char* name = 0, WFlags f = WType_TopLevel | WDestructiveClose)
		                    : KMainWindow(parent, name, f), disableQuit(false) { }

	protected:
		virtual void closeEvent(QCloseEvent*);
		virtual bool queryExit();

	private:
		bool  disableQuit;       // allow the application to quit
};

#endif // MAINWINDOWBASE_H

