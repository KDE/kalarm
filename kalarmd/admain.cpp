/*
 *  admain.cpp  -  kalarmd main program
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  (C) 2001, 2004 by David Jarvie <software@astrojar.org.uk>
 *  Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
 *  Copyright (c) 1997-1999 Preston Brown
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

#include <stdlib.h>

#include <klocale.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kstartupinfo.h>

#include "adapp.h"

static const KCmdLineOptions options[] =
{
	{ 0, 0, 0 }
};

int main(int argc, char** argv)
{
	KAboutData aboutData(DAEMON_APP_NAME, I18N_NOOP("KAlarm Daemon"),
	                     DAEMON_VERSION, I18N_NOOP("KAlarm Alarm Daemon"), KAboutData::License_GPL,
	                     "(c) 1997-1999 Preston Brown\n(c) 2000-2001 Cornelius Schumacher\n(c) 2001,2004 David Jarvie", 0,
	                     "http://www.astrojar.org.uk/linux/kalarm.html");
	aboutData.addAuthor("David Jarvie", I18N_NOOP("Maintainer"), "software@astrojar.org.uk");
	aboutData.addAuthor("Cornelius Schumacher", I18N_NOOP("Author"), "schumacher@kde.org");
	aboutData.addAuthor("Preston Brown", I18N_NOOP("Original Author"), "pbrown@kde.org");

	KCmdLineArgs::init(argc, argv, &aboutData);
	KCmdLineArgs::addCmdLineOptions(options);
	KUniqueApplication::addCmdLineOptions();
	KStartupInfo::disableAutoAppStartedSending();

	if (!AlarmDaemonApp::start())
		exit(0);
	AlarmDaemonApp app;
	return app.exec();
}
