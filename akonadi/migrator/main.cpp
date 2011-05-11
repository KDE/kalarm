/*
 *  main.cpp - main program for migrating KAlarm resources to Akonadi
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
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

#include "alarmmigrator.h"

#include <kaboutdata.h>
#include <kapplication.h>
#include <kcmdlineargs.h>
#include <kglobal.h>


#define PROGRAM_NAME "kalarm-migrator"


int main(int argc, char *argv[])
{
    KAboutData aboutData(PROGRAM_NAME, 0, ki18n("KAlarm Migration Tool"), "0.1",
        ki18n("Migration of KAlarm KResource settings to Akonadi"),
        KAboutData::License_GPL,
        ki18n("Copyright 2011, David Jarvie"), KLocalizedString(), "http://www.astrojar.org.uk/kalarm");
    aboutData.addAuthor(ki18n("David Jarvie"), KLocalizedString(), "djarvie@kde.org");
    aboutData.setOrganizationDomain("kde.org");

    KCmdLineArgs::init(argc, argv, &aboutData);

    KCmdLineOptions options;
    KCmdLineArgs::addCmdLineOptions(options);
    KCmdLineArgs* args = KCmdLineArgs::parsedArgs();

    KApplication app;
    app.setQuitOnLastWindowClosed(false);

    KGlobal::setAllowQuit(true);
    KGlobal::locale()->insertCatalog("libakonadi");

    new AlarmMigrator();

    args->clear();
    return app.exec();
}

// vim: et sw=4:
