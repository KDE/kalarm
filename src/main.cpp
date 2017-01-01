/*
 *  main.cpp
 *  Program:  kalarm
 *  Copyright © 2001-2017 by David Jarvie <djarvie@kde.org>
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
#include "kalarm_debug.h"

#include <kalarmcal/version.h>

#include <KAboutData>
#include <KLocalizedString>

#include <QDir>
#include <QScopedPointer>

#include <stdlib.h>

#define PROGRAM_NAME "kalarm"

int main(int argc, char* argv[])
{
    // Use QScopedPointer to ensure the QCoreApplication instance is deleted
    // before libraries unload, to avoid crashes during clean-up.
    QScopedPointer<KAlarmApp> app(KAlarmApp::create(argc, argv));

    QStringList args = app->arguments();
    app->setAttribute(Qt::AA_UseHighDpiPixmaps, true);
    app->setAttribute(Qt::AA_EnableHighDpiScaling);

    KLocalizedString::setApplicationDomain("kalarm");
    KAboutData aboutData(QStringLiteral(PROGRAM_NAME), i18n("KAlarm"),
                         QStringLiteral(KALARM_VERSION),
                         i18n("Personal alarm message, command and email scheduler for KDE"),
                         KAboutLicense::GPL,
                         ki18n("Copyright 2001-%1, David Jarvie").subs(2017).toString(), QString(),
                         QStringLiteral("http://www.astrojar.org.uk/kalarm"));
    aboutData.addAuthor(i18n("David Jarvie"), i18n("Author"), QStringLiteral("djarvie@kde.org"));
    aboutData.setOrganizationDomain("kde.org");
    KAboutData::setApplicationData(aboutData);

    qCDebug(KALARM_LOG) << "initialising";

    app->activate(args, QDir::currentPath());
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
        version = KAlarmCal::getVersionNumber(QStringLiteral(KALARM_VERSION));
    return version;
}

} // namespace KAlarm

// vim: et sw=4:
