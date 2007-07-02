/*
 *  admain.cpp  -  kalarmd main program
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  Copyright © 2001,2004,2005,2007 by David Jarvie <software@astrojar.org.uk>
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

int main(int argc, char** argv)
{
	KAboutData aboutData(DAEMON_APP_NAME, 0, ki18n("KAlarm Daemon"),
	                     DAEMON_VERSION, ki18n("KAlarm Alarm Daemon"), KAboutData::License_GPL,
	                     ki18n("Copyright 1997-1999 Preston Brown\nCopyright 2000-2001 Cornelius Schumacher\nCopyright 2001,2004-2007 David Jarvie"), KLocalizedString(),
	                     "http://www.astrojar.org.uk/kalarm");
	aboutData.addAuthor(ki18n("David Jarvie"), ki18n("Maintainer"), "software@astrojar.org.uk");
	aboutData.addAuthor(ki18n("Cornelius Schumacher"), ki18n("Author"), "schumacher@kde.org");
	aboutData.addAuthor(ki18n("Preston Brown"), ki18n("Original Author"), "pbrown@kde.org");
	aboutData.setOrganizationDomain("kalarm.kde.org");
	KCmdLineArgs::init(argc, argv, &aboutData);

	KCmdLineOptions options;
	options.add("autostart", ki18n("kalarmd is being autostarted"));
	KCmdLineArgs::addCmdLineOptions(options);
	KUniqueApplication::addCmdLineOptions();
	KStartupInfo::disableAutoAppStartedSending();

	if (!AlarmDaemonApp::start())
		exit(0);
	AlarmDaemonApp app;
	return app.exec();
}
