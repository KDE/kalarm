/*
 *  autostart.cpp - autostart KAlarm when session restoration is complete
 *  Program:  kalarmautostart
 *  Copyright © 2001,2008 by David Jarvie <djarvie@kde.org>
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
#include "autostart.h"

#include <kcmdlineargs.h>
#include <K4AboutData>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kprocess.h>
#include <kdebug.h>

#include <QTimer>
#include <QtDBus/QtDBus>

// Number of seconds to wait before autostarting KAlarm.
// Allow plenty of time for session restoration to happen first.
static const int AUTOSTART_DELAY = 30;

#define PROGRAM_VERSION      "1.0"
#define PROGRAM_NAME "kalarmautostart"


int main(int argc, char *argv[])
{
    K4AboutData aboutData(PROGRAM_NAME, "kalarm", ki18n("KAlarm Autostart"),
        PROGRAM_VERSION, ki18n("KAlarm autostart at login"), K4AboutData::License_GPL,
        ki18n("Copyright 2001,2008 David Jarvie"), KLocalizedString(),
        "http://www.astrojar.org.uk/kalarm");
    aboutData.addAuthor(ki18n("David Jarvie"), ki18n("Maintainer"), "djarvie@kde.org");
    aboutData.setOrganizationDomain("kalarm.kde.org");
    KCmdLineArgs::init(argc, argv, &aboutData);

    KCmdLineOptions options;
    options.add("!+app", ki18n("Application to autostart"));
    options.add("+[arg]", ki18n("Command line arguments"));
    KCmdLineArgs::addCmdLineOptions(options);

    AutostartApp app;
    KGlobal::locale()->insertCatalog(QLatin1String("kalarm"));
    return app.exec();
}



AutostartApp::AutostartApp()
    : KApplication(false)       // initialise as non-GUI application
{
    // Disable session management: there is no state to save, and
    // disabling prevents a crash on logout before this app exits.
    disableSessionManagement();

    // Login session is starting up - need to wait for it to complete
    // in order to avoid starting the client before it is restored by
    // the session (where applicable).
    QTimer::singleShot(AUTOSTART_DELAY * 1000, this, SLOT(slotAutostart()));
}

void AutostartApp::slotAutostart()
{
    QDBusReply<bool> reply = QDBusConnection::sessionBus().interface()->isServiceRegistered(QLatin1String(KALARM_DBUS_SERVICE));
    if (reply.isValid()  &&  reply.value())
        kDebug(5900) << "KAlarm already running";
    else
    {
        KCmdLineArgs* args = KCmdLineArgs::parsedArgs();
        if (args->count() <= 0)
            kWarning(5900) << "No command line";
        else
        {
            QString prog = args->arg(0);
            QString exe = KStandardDirs::locate("exe", prog);
            if (exe.isEmpty())
                kWarning(5900) << "Executable not found:" << prog;
            else
            {
                kDebug(5900) << "Starting" << prog;
                KProcess proc;
                proc << exe;
                for (int i = 1;  i < args->count();  ++i)
                    proc << args->arg(i);
                proc.startDetached();
            }
        }
    }
    exit();
}
#include "moc_autostart.cpp"
// vim: et sw=4:
