/*
 *  autostartmain.cpp - autostart an application when session restoration is complete
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

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kstddirs.h>
#include <kprocess.h>
#include <klocale.h>
#include <dcopclient.h>
#include <kdebug.h>

#include "autostart.h"
#include "autostart.moc"

const int LOGIN_DELAY = 5;

#undef VERSION
#define VERSION      "0.1"
#define PROGRAM_NAME "kalarmautostart"


static KCmdLineOptions options[] =
{
	{ "!+app", I18N_NOOP("Application to autostart"), 0L },
	{ "+[arg]", I18N_NOOP("Command line argument"), 0L },
	{ 0L, 0L, 0L }
};



int main(int argc, char *argv[])
{
	KAboutData aboutData(PROGRAM_NAME, I18N_NOOP("KAlarmAutostart"),
		VERSION, I18N_NOOP("       " PROGRAM_NAME "\n"
		"       " PROGRAM_NAME " [generic_options]\n\n"
		"KAlarm autostart at login"),
		KAboutData::License_GPL,
		"(c) 2002, David Jarvie", 0L, "http://www.astrojar.org.uk/linux",
		"software@astrojar.org.uk");
	aboutData.addAuthor("David Jarvie", 0L, "software@astrojar.org.uk");

	KCmdLineArgs::init(argc, argv, &aboutData);
	KCmdLineArgs::addCmdLineOptions(options);

	AutostartApp app;
	return app.exec();
}



AutostartApp::AutostartApp()
	: KApplication(false, false)       // initialise as non-GUI application
{
	// Login session is starting up - need to wait for it to complete
	// in order to prevent the daemon starting clients before they are
	// restored by the session (where applicable).
	// If ksplash can be detected as running, start a 1-second timer;
	// otherwise, wait a few seconds.
	bool splash = kapp->dcopClient()->isApplicationRegistered("ksplash");
kdDebug() << "AutostartApp: ksplash " << (splash ? "" : "not") << "running\n";
	mSessionStartTimer = new QTimer(this);
	connect(mSessionStartTimer, SIGNAL(timeout()), SLOT(checkIfSessionStarted()));
	mSessionStartTimer->start(splash ? 1000 : LOGIN_DELAY * 1000);
}

/*
 * Called by the timer to check whether session startup is complete.
 * (Ideally checking for session startup would be done using a signal
 * from ksmserver, but until such a signal is available, we can check
 * whether ksplash is still running.)
 */
void AutostartApp::checkIfSessionStarted()
{
	if (!kapp->dcopClient()->isApplicationRegistered("ksplash"))
	{
		// Session startup has now presumably completed.
		kdDebug() << "AutostartApp::checkIfSessionStarted(): startup complete\n";

		KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
		if (args->count() > 0)
		{
			KProcess proc;
			proc << locate("exe", QString::fromLatin1(args->arg(0)));
			for (int i = 1;  i < args->count();  ++i)
				proc << QString::fromLatin1(args->arg(i));
			proc.start(KProcess::DontCare);
		}
		exit();
	}
}
