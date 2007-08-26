/*
 *  admain.cpp  -  kalarmd main program
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  Copyright (c) 2001,2004,2005 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
	{ "autostart", "", 0 },
	KCmdLineLastOption
};

int main(int argc, char** argv)
{
	KAboutData aboutData(DAEMON_APP_NAME, I18N_NOOP("KAlarm Daemon"),
	                     DAEMON_VERSION, I18N_NOOP("KAlarm Alarm Daemon"), KAboutData::License_GPL,
	                     "Copyright 1997-1999 Preston Brown\nCopyright 2000-2001 Cornelius Schumacher\nCopyright 2001,2004-2007 David Jarvie", 0,
	                     "http://www.astrojar.org.uk/kalarm");
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
