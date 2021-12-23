/*
 *  main.cpp
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);
#endif

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
                         ki18n("Copyright © 2001-%1, David Jarvie").subs(QStringLiteral("2021")).toString(), QString());
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
        if (!outputText.isEmpty())
        {
            if (exitCode > 0)
                std::cerr << qPrintable(outputText) << std::endl;
            else
                std::cout << qPrintable(outputText) << std::endl;
        }
        exit(exitCode);
    }

    app->restoreSession();
    return app->exec();
}

// vim: et sw=4:
