/*
  Copyright (c) 2015-2016 Montel Laurent <montel@kde.org>

  based on code from Sune Vuorela <sune@vuorela.dk> (Rawatar source code)

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License, version 2, as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "kalarmmigrateapplication.h"
#include <Kdelibs4ConfigMigrator>

KAlarmMigrateApplication::KAlarmMigrateApplication()
{
    initializeMigrator();
}

void KAlarmMigrateApplication::migrate()
{
    // Migrate to xdg.
    Kdelibs4ConfigMigrator migrate(QStringLiteral("kalarm"));
    migrate.setConfigFiles(QStringList() << QStringLiteral("kalarmrc"));
    migrate.setUiFiles(QStringList() << QStringLiteral("kalarmui.rc"));
    migrate.migrate();

    // Migrate folders and files.
    if (mMigrator.checkIfNecessary())
        mMigrator.start();
}

void KAlarmMigrateApplication::initializeMigrator()
{
    const int currentVersion = 2;
    mMigrator.setApplicationName(QStringLiteral("kalarm"));
    mMigrator.setConfigFileName(QStringLiteral("kalarmrc"));

    // To migrate we need a version > currentVersion
    const int initialVersion = currentVersion + 1;

    // ics file
    PimCommon::MigrateFileInfo migrateInfoIcs;
    migrateInfoIcs.setFolder(false);
    migrateInfoIcs.setType(QStringLiteral("data"));
    migrateInfoIcs.setPath(QStringLiteral("kalarm/"));
    migrateInfoIcs.setVersion(initialVersion);
    migrateInfoIcs.setFilePatterns(QStringList() << QStringLiteral("*.ics"));
    mMigrator.insertMigrateInfo(migrateInfoIcs);
}

