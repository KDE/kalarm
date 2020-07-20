/*
 * migratekde4files.cpp - migrate KDE4 config and data file locations
 * Program:  kalarm
 * Copyright (c) 2015-2020 Laurent Montel <montel@kde.org>
 * Copyright © 2019 David Jarvie <djarvie@kde.org>
 *
 * based on code from Sune Vuorela <sune@vuorela.dk> (Rawatar source code)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "migratekde4files.h"

#include "kalarm_debug.h"

#include <Kdelibs4ConfigMigrator>

MigrateKde4Files::MigrateKde4Files()
{
    initializeMigrator();
}

void MigrateKde4Files::migrate()
{
    // Migrate config and ui files to Qt5 locations.
    Kdelibs4ConfigMigrator migrate(QStringLiteral("kalarm"));
    migrate.setConfigFiles({QStringLiteral("kalarmrc")});
    migrate.setUiFiles({QStringLiteral("kalarmui.rc")});
    migrate.migrate();

    // Migrate data files to Qt5 locations.
    if (mMigrator.checkIfNecessary())
    {
        // When done, this will add a [Migratekde4] entry to kalarmrc.
        if (!mMigrator.start())
            qCCritical(KALARM_LOG) << "Error migrating config files";
    }
}

void MigrateKde4Files::initializeMigrator()
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
    migrateInfoIcs.setFilePatterns({QStringLiteral("*.ics")});
    mMigrator.insertMigrateInfo(migrateInfoIcs);
}

// vim: et sw=4: