/*
    This file is part of the KDE alarm daemon.
    Copyright (c) 1997-1999 Preston Brown
    Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
    Copyright (c) 2001 David Jarvie <software@astrojar.org.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

    As a special exception, permission is given to link this program
    with any edition of Qt, and distribute the resulting executable,
    without including the source code for Qt in the source distribution.
*/

// $Id$
//
// KOrganizer/KAlarm alarm daemon main program
//

#include <stdlib.h>
#include <kdebug.h>

#include <klocale.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kstartupinfo.h>

#include "alarmapp.h"

static const char kalarmdVersion[] = "3.4";
static const KCmdLineOptions options[] =
{
   { "login", I18N_NOOP("Application is being auto-started at KDE session start"), 0L },
   { 0L, 0L, 0L }
};

int main(int argc, char **argv)
{
  KAboutData aboutData("kalarmd", I18N_NOOP("Alarm Daemon"),
      kalarmdVersion, I18N_NOOP("KOrganizer/KAlarm Alarm Daemon"), KAboutData::License_GPL,
      "(c) 1997-1999 Preston Brown\n(c) 2000-2001 Cornelius Schumacher\n(c) 2001 David Jarvie", 0L,
      "http://korganizer.kde.org");
  aboutData.addAuthor("Cornelius Schumacher",I18N_NOOP("Maintainer"),
                      "schumacher@kde.org");
  aboutData.addAuthor("David Jarvie",I18N_NOOP("KAlarm Author"),
                      "software@astrojar.org.uk");
  aboutData.addAuthor("Preston Brown",I18N_NOOP("Original Author"),
                      "pbrown@kde.org");

  KCmdLineArgs::init(argc,argv,&aboutData);
  KCmdLineArgs::addCmdLineOptions(options);
  KUniqueApplication::addCmdLineOptions();
  KStartupInfo::disableAutoAppStartedSending();

  if (!AlarmApp::start())
    exit(0);

  AlarmApp app;

  return app.exec();
}
