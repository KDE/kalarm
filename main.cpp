/*
 *  main.cpp
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <software@astrojar.org.uk>
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

#include "kalarm.h"

#include <stdlib.h>

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kdebug.h>

#include "kalarmapp.h"

#define PROGRAM_NAME "kalarm"

static KCmdLineOptions options[] =
{
	{ "a", 0, 0 },
	{ "ack-confirm", I18N_NOOP("Prompt for confirmation when alarm is acknowledged"), 0 },
	{ "A", 0, 0 },
	{ "attach <url>", I18N_NOOP("Attach file to email (repeat as needed)"), 0 },
	{ "auto-close", I18N_NOOP("Auto-close alarm window after --late-cancel period"), 0 },
	{ "bcc", I18N_NOOP("Blind copy email to self"), 0 },
	{ "b", 0, 0 },
	{ "beep", I18N_NOOP("Beep when message is displayed"), 0 },
	{ "colour", 0, 0 },
	{ "c", 0, 0 },
	{ "color <color>", I18N_NOOP("Message background color (name or hex 0xRRGGBB)"), 0 },
	{ "colourfg", 0, 0 },
	{ "C", 0, 0 },
	{ "colorfg <color>", I18N_NOOP("Message foreground color (name or hex 0xRRGGBB)"), 0 },
	{ "calendarURL <url>", I18N_NOOP("URL of calendar file"), 0 },
	{ "cancelEvent <eventID>", I18N_NOOP("Cancel alarm with the specified event ID"), 0 },
	{ "d", 0, 0 },
	{ "disable", I18N_NOOP("Disable the alarm"), 0 },
	{ "e", 0, 0 },
	{ "!exec <commandline>", I18N_NOOP("Execute a shell command line"), 0 },
	{ "edit <eventID>", I18N_NOOP("Display the alarm edit dialog to edit the specified alarm"), 0 },
	{ "n", 0, 0 },
	{ "edit-new", I18N_NOOP("Display the alarm edit dialog to edit a new alarm"), 0 },
	{ "edit-new-preset <templateName>", I18N_NOOP("Display the alarm edit dialog, preset with a template"), 0 },
	{ "f", 0, 0 },
	{ "file <url>", I18N_NOOP("File to display"), 0 },
	{ "F", 0, 0 },
	{ "from-id <ID>", I18N_NOOP("KMail identity to use as sender of email"), 0 },
	{ "handleEvent <eventID>", I18N_NOOP("Trigger or cancel alarm with the specified event ID"), 0 },
	{ "i", 0, 0 },
	{ "interval <period>", I18N_NOOP("Interval between alarm repetitions"), 0 },
	{ "k", 0, 0 },
	{ "korganizer", I18N_NOOP("Show alarm as an event in KOrganizer"), 0 },
	{ "l", 0, 0 },
	{ "late-cancel <period>", I18N_NOOP("Cancel alarm if more than 'period' late when triggered"), "1" },
	{ "L", 0, 0 },
	{ "login", I18N_NOOP("Repeat alarm at every login"), 0 },
	{ "m", 0, 0 },
	{ "mail <address>", I18N_NOOP("Send an email to the given address (repeat as needed)"), 0 },
	{ "p", 0, 0 },
	{ "play <url>", I18N_NOOP("Audio file to play once"), 0 },
#ifndef WITHOUT_ARTS
	{ "P", 0, 0 },
	{ "play-repeat <url>", I18N_NOOP("Audio file to play repeatedly"), 0 },
#endif
	{ "recurrence <spec>", I18N_NOOP("Specify alarm recurrence using iCalendar syntax"), 0 },
	{ "R", 0, 0 },
	{ "reminder <period>", I18N_NOOP("Display reminder in advance of alarm"), 0 },
	{ "reminder-once <period>", I18N_NOOP("Display reminder once, before first alarm recurrence"), 0 },
	{ "r", 0, 0 },
	{ "repeat <count>", I18N_NOOP("Number of times to repeat alarm (including initial occasion)"), 0 },
	{ "reset", I18N_NOOP("Reset the alarm scheduling daemon"), 0 },
	{ "s", 0, 0 },
	{ "speak", I18N_NOOP("Speak the message when it is displayed"), 0 },
	{ "stop", I18N_NOOP("Stop the alarm scheduling daemon"), 0 },
	{ "S", 0, 0 },
	{ "subject", I18N_NOOP("Email subject line"), 0 },
	{ "t", 0, 0 },
	{ "time <time>", I18N_NOOP("Trigger alarm at time [[[yyyy-]mm-]dd-]hh:mm, or date yyyy-mm-dd"), 0 },
	{ "tray", I18N_NOOP("Display system tray icon"), 0 },
	{ "triggerEvent <eventID>", I18N_NOOP("Trigger alarm with the specified event ID"), 0 },
	{ "u", 0, 0 },
	{ "until <time>", I18N_NOOP("Repeat until time [[[yyyy-]mm-]dd-]hh:mm, or date yyyy-mm-dd"), 0 },
#ifndef WITHOUT_ARTS
	{ "V", 0, 0 },
	{ "volume <percent>", I18N_NOOP("Volume to play audio file"), 0 },
#endif
	{ "+[message]", I18N_NOOP("Message text to display"), 0 },
	KCmdLineLastOption
};


int main(int argc, char *argv[])
{
	KAboutData aboutData(PROGRAM_NAME, I18N_NOOP("KAlarm"), KALARM_VERSION,
		I18N_NOOP("Personal alarm message, command and email scheduler for KDE"),
		KAboutData::License_GPL,
		"Copyright 2001-2007, David Jarvie", 0, "http://www.astrojar.org.uk/kalarm");
	aboutData.addAuthor("David Jarvie", 0, "software@astrojar.org.uk");

	KCmdLineArgs::init(argc, argv, &aboutData);
	KCmdLineArgs::addCmdLineOptions(options);
	KUniqueApplication::addCmdLineOptions();

	if (!KAlarmApp::start())
	{
		// An instance of the application is already running
		exit(0);
	}

	// This is the first time through
	kdDebug(5950) << "main(): initialising\n";
	KAlarmApp* app = KAlarmApp::getInstance();
	app->restoreSession();
	return app->exec();
}
