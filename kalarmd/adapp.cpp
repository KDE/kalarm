/*
 *  adapp.cpp  -  kalarmd application
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  (C) 2001, 2004 by David Jarvie <software@astrojar.org.uk>
 *  Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
 *  Copyright (c) 1997-1999 Preston Brown <pbrown@kde.org>
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
 */

#include "kalarmd.h"

#include <kcmdlineargs.h>
#include <kdebug.h>

#include "alarmdaemon.h"
#include "adapp.moc"


AlarmDaemonApp::AlarmDaemonApp()
	: KUniqueApplication(false, false),
	  mAd(0)
{
	disableSessionManagement();
}

int AlarmDaemonApp::newInstance()
{
	kdDebug(5900) << "kalarmd:AlarmDaemonApp::newInstance()" << endl;

	/* Prevent the application being restored automatically by the session manager
	 * at session startup. Instead, the KDE autostart facility is used to start
	 * the application. This allows the user to configure whether or not it is to
	 * be started automatically, and also ensures that it is started in the correct
	 * phase of session startup, i.e. after clients have been restored by the
	 * session manager.
	 */
	disableSessionManagement();

	// Check if we already have a running alarm daemon widget
	if (mAd)
		return 0;

	// Check if we are starting up at session startup
	static bool restored = false;
	if (!restored  &&  isRestored())
		restored = true;       // make sure we restore only once
	else
	{
		KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
		args->clear();      // free up memory
	}

	mAd = new AlarmDaemon(0, DAEMON_DCOP_OBJECT);

	return 0;
}
