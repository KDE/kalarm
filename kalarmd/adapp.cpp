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

#include <qstring.h>

#include <kcmdlineargs.h>
#include <kdebug.h>
#include <kstartupinfo.h>

#include "alarmdaemon.h"

#include "alarmapp.h"
#include "alarmapp.moc"


AlarmApp::AlarmApp() :
  KUniqueApplication(false,false),
  mAd(0L)
{
  disableSessionManagement();
}

AlarmApp::~AlarmApp()
{
}

int AlarmApp::newInstance()
{
  kdDebug(5900) << "kalarmd:AlarmApp::newInstance()" << endl;

  //KStartupInfo::appStarted();

  /* Prevent the application being restored automatically by the session manager
   * at session startup. Instead, the KDE autostart facility is used to start
   * the application. This allows the user to configure whether or not it is to
   * be started automatically, and also ensures that it is started in the correct
   * phase of session startup, i.e. after clients have been restored by the
   * session manager.
   */
  disableSessionManagement();

  // Check if we already have a running alarm daemon widget
  if (mAd) return 0;

  // Check if we are starting up at session startup
  static bool restored = false;
  if (!restored  &&  isRestored()) {
    mStartedAtLogin = true;
    restored = true;       // make sure we restore only once
  }
  else {
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
    mStartedAtLogin = args->isSet("login");
    args->clear();      // free up memory
  }

  mAd = new AlarmDaemon(0L, "ad");

  return 0;
}
