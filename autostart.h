/*
 *  autostart.h - autostart an application when session restoration is complete
 *  Program:  kalarmautostart
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
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
#ifndef AUTOSTART_H
#define AUTOSTART_H

#include <qtimer.h>
#include <kapp.h>


class AutostartApp : public KApplication
{
		Q_OBJECT
	public:
		AutostartApp();
		~AutostartApp()  { }
	private slots:
		void checkIfSessionStarted();
	private:
		QTimer*   mSessionStartTimer;
};

#endif // AUTOSTART_H
