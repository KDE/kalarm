/*
 *  main.cpp
 *  Program:  kalarm
 *  Copyright © 2001-2012 by David Jarvie <djarvie@kde.org>
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

#include "kalarmapp.h"

#include <kalarmcal/version.h>

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kdebug.h>

#include <stdlib.h>

#define PROGRAM_NAME "kalarm"


int main(int argc, char *argv[])
{
    KAboutData aboutData(PROGRAM_NAME, 0, ki18n("KAlarm"), KALARM_VERSION,
        ki18n("Personal alarm message, command and email scheduler for KDE"),
        KAboutData::License_GPL,
        ki18n("Copyright 2001-2012, David Jarvie"), KLocalizedString(), "http://www.astrojar.org.uk/kalarm");
    aboutData.addAuthor(ki18n("David Jarvie"), KLocalizedString(), "djarvie@kde.org");
    aboutData.setOrganizationDomain("kde.org");

    KCmdLineArgs::init(argc, argv, &aboutData);

    KCmdLineOptions options;
    options.add("a");
    options.add("ack-confirm", ki18n("Prompt for confirmation when alarm is acknowledged"));
    options.add("A");
    options.add("attach <url>", ki18n("Attach file to email (repeat as needed)"));
    options.add("auto-close", ki18n("Auto-close alarm window after --late-cancel period"));
    options.add("bcc", ki18n("Blind copy email to self"));
    options.add("b");
    options.add("beep", ki18n("Beep when message is displayed"));
    options.add("colour");
    options.add("c");
    options.add("color <color>", ki18n("Message background color (name or hex 0xRRGGBB)"));
    options.add("colourfg");
    options.add("C");
    options.add("colorfg <color>", ki18n("Message foreground color (name or hex 0xRRGGBB)"));
    options.add("cancelEvent <eventID>", ki18n("Cancel alarm with the specified event ID"));
    options.add("d");
    options.add("disable", ki18n("Disable the alarm"));
    options.add("disable-all", ki18n("Disable monitoring of all alarms"));
    options.add("e");
    options.add("!exec <commandline>", ki18n("Execute a shell command line"));
    options.add("E");
    options.add("!exec-display <commandline>", ki18n("Command line to generate alarm message text"));
    options.add("edit <eventID>", ki18n("Display the alarm edit dialog to edit the specified alarm"));
    options.add("edit-new-display", ki18n("Display the alarm edit dialog to edit a new display alarm"));
    options.add("edit-new-command", ki18n("Display the alarm edit dialog to edit a new command alarm"));
    options.add("edit-new-email", ki18n("Display the alarm edit dialog to edit a new email alarm"));
    options.add("edit-new-audio", ki18n("Display the alarm edit dialog to edit a new audio alarm"));
    options.add("edit-new-preset <templateName>", ki18n("Display the alarm edit dialog, preset with a template"));
    options.add("f");
    options.add("file <url>", ki18n("File to display"));
    options.add("F");
    options.add("from-id <ID>", ki18n("KMail identity to use as sender of email"));
    options.add("i");
    options.add("interval <period>", ki18n("Interval between alarm repetitions"));
    options.add("k");
    options.add("korganizer", ki18n("Show alarm as an event in KOrganizer"));
    options.add("l");
    options.add("late-cancel <period>", ki18n("Cancel alarm if more than 'period' late when triggered"), "1");
    options.add("list", ki18n("Output list of scheduled alarms to stdout"));
    options.add("L");
    options.add("login", ki18n("Repeat alarm at every login"));
    options.add("m");
    options.add("mail <address>", ki18n("Send an email to the given address (repeat as needed)"));
    options.add("p");
    options.add("play <url>", ki18n("Audio file to play once"));
    options.add("P");
    options.add("play-repeat <url>", ki18n("Audio file to play repeatedly"));
    options.add("recurrence <spec>", ki18n("Specify alarm recurrence using iCalendar syntax"));
    options.add("R");
    options.add("reminder <period>", ki18n("Display reminder before or after alarm"));
    options.add("reminder-once <period>", ki18n("Display reminder once, before or after first alarm recurrence"));
    options.add("r");
    options.add("repeat <count>", ki18n("Number of times to repeat alarm (including initial occasion)"));
    options.add("s");
    options.add("speak", ki18n("Speak the message when it is displayed"));
    options.add("S");
    options.add("subject <text>", ki18n("Email subject line"));
#ifndef NDEBUG
    options.add("test-set-time <time>", ki18n("Simulate system time [[[yyyy-]mm-]dd-]hh:mm [TZ] (debug mode)"));
#endif
    options.add("t");
    options.add("time <time>", ki18n("Trigger alarm at time [[[yyyy-]mm-]dd-]hh:mm [TZ], or date yyyy-mm-dd [TZ]"));
    options.add("tray", ki18n("Display system tray icon"));
    options.add("triggerEvent <eventID>", ki18n("Trigger alarm with the specified event ID"));
    options.add("u");
    options.add("until <time>", ki18n("Repeat until time [[[yyyy-]mm-]dd-]hh:mm [TZ], or date yyyy-mm-dd [TZ]"));
    options.add("V");
    options.add("volume <percent>", ki18n("Volume to play audio file"));
    options.add("+[message]", ki18n("Message text to display"));
    KCmdLineArgs::addCmdLineOptions(options);
    KUniqueApplication::addCmdLineOptions();

    if (!KAlarmApp::start())
    {
        // An instance of the application is already running
        exit(0);
    }

    // This is the first time through
    kDebug() << "initialising";
    KAlarmApp* app = KAlarmApp::getInstance();
    app->restoreSession();
    return app->exec();
}

namespace KAlarm
{

/******************************************************************************
* Return the current KAlarm version number.
*/
int Version()
{
    static int version = 0;
    if (!version)
        version = KAlarmCal::getVersionNumber(QLatin1String(KALARM_VERSION));
    return version;
}

} // namespace KAlarm

// vim: et sw=4:
