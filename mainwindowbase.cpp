/*
 *  mainwindowbase.cpp  -  base class for main application windows
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "kalarm.h"
#include <kdeversion.h>
#include "kalarmapp.h"
#include "mainwindowbase.moc"


/******************************************************************************
*  Called when a close event is received.
*  Only quits the application if there is no system tray icon displayed.
*/
void MainWindowBase::closeEvent(QCloseEvent* ce)
{
	disableQuit = theApp()->trayIconDisplayed();
	KMainWindow::closeEvent(ce);
	disableQuit = false;
	ce->accept();           // allow window to close even if it's the last main window
}

/******************************************************************************
*  Called when the window is being closed.
*  Returns true if the application should quit.
*/
bool MainWindowBase::queryExit()
{
#if KDE_IS_VERSION(3,1,90)
	if (kapp->sessionSaving())
		return true;
#endif
	return disableQuit ? false : KMainWindow::queryExit();
}
