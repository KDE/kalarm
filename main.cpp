/*
 *  main.cpp
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#include "kalarm.h"

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kdebug.h>

#include "kalarmapp.h"


static KCmdLineOptions options[] =
{
	{ "b", 0, 0 },
	{ "beep", I18N_NOOP("Beep when message is displayed"), 0 },
	{ "c", 0, 0 },
	{ "color", 0, 0 },
	{ "colour <colour>", I18N_NOOP("Message background colour (name or hex 0xRRGGBB)"), 0 },
	{ "calendarURL <url>", I18N_NOOP("URL of calendar file"), 0 },
	{ "cancelEvent <eventID>", I18N_NOOP("Cancel message with the specified event ID"), 0 },
	{ "displayEvent <eventID>", I18N_NOOP("Display message with the specified event ID"), 0 },
	{ "handleEvent <eventID>", I18N_NOOP("Display or cancel message with the specified event ID"), 0 },
	{ "l", 0, 0 },
	{ "late-cancel", I18N_NOOP("Cancel message if it cannot be displayed on time"), 0 },
	{ "r", 0, 0 },
	{ "reset", I18N_NOOP("Reset the message scheduling daemon"), 0 },
	{ "s", 0, 0 },
	{ "stop", I18N_NOOP("Stop the message scheduling daemon"), 0 },
	{ "t", 0, 0 },
	{ "time <time>", I18N_NOOP("Display message at 'time' [[[yyyy-]mm-]dd-]hh:mm"), 0 },
	{ "+[message]", I18N_NOOP("Message text to display"), "Alarm" },
	{ 0, 0, 0 }
};


int main(int argc, char *argv[])
{
	KAboutData aboutData(PROGRAM_NAME, I18N_NOOP(PROGRAM_TITLE),
		VERSION, I18N_NOOP("       " PROGRAM_NAME "\n"
		"       " PROGRAM_NAME " -rs\n"
		"       " PROGRAM_NAME " [-bclt] message\n"
		"       " PROGRAM_NAME " --cancelEvent eventID [--calendarURL url]\n"
		"       " PROGRAM_NAME " --displayEvent eventID [--calendarURL url]\n"
		"       " PROGRAM_NAME " [generic_options]\n\n"
		"KDE alarm message scheduler"),
		KAboutData::License_GPL,
		"(c) 2001, David Jarvie", 0, 0, "software@astrojar.org.uk");
	aboutData.addAuthor("David Jarvie",0, "software@astrojar.org.uk");

	KCmdLineArgs::init(argc, argv, &aboutData);
	KCmdLineArgs::addCmdLineOptions(options);
	KUniqueApplication::addCmdLineOptions();

	if (!KAlarmApp::start())
	{
		// An instance of the application is already running
		exit(0);
	}

	// This is the child instance
	KAlarmApp* app = KAlarmApp::getInstance();
	return app->exec();
}
