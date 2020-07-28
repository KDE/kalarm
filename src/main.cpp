/*
 *  main.cpp
 *  Program:  kalarm
 *  Copyright © 2001-2020 David Jarvie <djarvie@kde.org>
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
#include "kalarm-version-string.h"
#include "kalarmapp.h"
#include "kalarm_debug.h"

#include <KAboutData>
#include <KLocalizedString>
#include <KDBusService>
#include <KCrash>

#include <QDir>
#include <QScopedPointer>

#include <iostream>
#include <stdlib.h>

#define PROGRAM_NAME "kalarm"

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    // Use QScopedPointer to ensure the QCoreApplication instance is deleted
    // before libraries unload, to avoid crashes during clean-up.
    QScopedPointer<KAlarmApp> app(KAlarmApp::create(argc, argv));

    const QStringList args = app->arguments();
    KCrash::initialize();

    KLocalizedString::setApplicationDomain("kalarm");
    KAboutData aboutData(QStringLiteral(PROGRAM_NAME), i18n(KALARM_NAME),
                         QStringLiteral(KALARM_FULL_VERSION),
                         i18n("Personal alarm message, command and email scheduler by KDE"),
                         KAboutLicense::GPL,
                         ki18n("Copyright © 2001-%1, David Jarvie").subs(QStringLiteral("2020")).toString(), QString());
    aboutData.addAuthor(i18n("David Jarvie"), i18n("Author"), QStringLiteral("djarvie@kde.org"));
    aboutData.setOrganizationDomain("kde.org");
    aboutData.setDesktopFileName(QStringLiteral(KALARM_DBUS_SERVICE));
    KAboutData::setApplicationData(aboutData);

    // Make this a unique application.
    KDBusService service(KDBusService::Unique);
    QObject::connect(&service, &KDBusService::activateRequested, app.data(), &KAlarmApp::activateByDBus);
    QObject::connect(app.data(), &KAlarmApp::setExitValue, &service, [&service](int ret) { service.setExitValue(ret); });

    qCDebug(KALARM_LOG) << "initialising";
    app->initialise();

    QString outputText;
    int exitCode = app->activateInstance(args, QDir::currentPath(), &outputText);
    if (exitCode >= 0)
    {
        if (exitCode > 0)
            std::cout << qPrintable(outputText) << std::endl;
        else
            std::cerr << qPrintable(outputText) << std::endl;
        exit(exitCode);
    }

    app->restoreSession();
    return app->exec();
}

// vim: et sw=4:
