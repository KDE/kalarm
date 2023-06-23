/*
 *  autostart.cpp - autostart KAlarm when session restoration is complete
 *  Program:  kalarmautostart
 *  SPDX-FileCopyrightText: 2001, 2008, 2018 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "autostart.h"
#include "kalarm.h"
#include "kalarm_autostart_debug.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QProcess>
#include <QTimer>
#include <QDBusConnectionInterface>
#include <QCommandLineParser>
#include <QStandardPaths>

// Number of seconds to wait before autostarting KAlarm.
// Allow plenty of time for session restoration to happen first.
static const int AUTOSTART_DELAY = 30;

#define PROGRAM_VERSION      "2.0"
#define PROGRAM_NAME "kalarmautostart"


int main(int argc, char *argv[])
{
    // Before using the command line, remove any options or arguments which are
    // for the application to be run.
    int ourArgc = argc;
    QStringList exeArgs;   // the command line for the application to run
    for (int i = 1; i < argc; ++i)
        if (*argv[i] != '-')
        {
            ourArgc = i + 1;
            while (i < argc)
                exeArgs.append(QString::fromLocal8Bit(argv[i++]));
            break;
        }

    AutostartApp app(ourArgc, argv);

    KLocalizedString::setApplicationDomain("kalarm");
    KAboutData aboutData(QStringLiteral(PROGRAM_NAME), i18n("KAlarm Autostart"),
                         QStringLiteral(PROGRAM_VERSION), i18n("KAlarm autostart at login"),
                         KAboutLicense::GPL,
                         ki18n("Copyright 2001-%1, David Jarvie").subs(QStringLiteral("2020")).toString(), QString());
    aboutData.addAuthor(i18n("David Jarvie"), i18n("Author"), QStringLiteral("djarvie@kde.org"));
    aboutData.setOrganizationDomain("kalarm.kde.org");
    KAboutData::setApplicationData(aboutData);

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.addPositionalArgument(QStringLiteral("application"), i18n("Application to run"));
    parser.addPositionalArgument(QStringLiteral("[arg...]"), i18n("Command line arguments to pass to application"));
    parser.process(app);
    aboutData.processCommandLine(&parser);

    if (exeArgs.isEmpty())
    {
        qCWarning(KALARMAUTOSTART_LOG) << "No command line";
        return 1;
    }
    const QString exe = QStandardPaths::findExecutable(exeArgs[0]);
    if (exe.isEmpty())
    {
        qCWarning(KALARMAUTOSTART_LOG) << "Executable not found:" << exeArgs[0];
        return 1;
    }

    app.setCommandLine(exe, exeArgs);
    return app.exec();
}



AutostartApp::AutostartApp(int& argc, char** argv)
    : QCoreApplication(argc, argv)
{
    // Note that this application does not require session management.
}

void AutostartApp::setCommandLine(const QString& exe, const QStringList& args)
{
    // Don't validate the arguments until exec() has been called, so that error
    // messages can be output to the correct log file instead of stderr.
    mExecutable = exe;
    mArgs = args;

    // Login session is starting up - need to wait for it to complete
    // in order to avoid starting the client before it is restored by
    // the session (where applicable).
    QTimer::singleShot(AUTOSTART_DELAY * 1000, this, &AutostartApp::slotAutostart);   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
}

void AutostartApp::slotAutostart()
{
    const QString prog = mArgs[0];
    if (prog == QLatin1String("kalarm"))
    {
        QDBusReply<bool> reply = QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral(KALARM_DBUS_SERVICE));
        if (reply.isValid()  &&  reply.value())
        {
            qCDebug(KALARMAUTOSTART_LOG) << "KAlarm already running";
            exit();
        }
    }

    qCDebug(KALARMAUTOSTART_LOG) << "Starting" << prog;
    QStringList args = mArgs;
    args.takeFirst();
    QProcess::startDetached(mExecutable, args);
    exit();
}

#include "moc_autostart.cpp"

// vim: et sw=4:
