/*
 * migratekde4files.cpp - migrate KDE4 config and data file locations
 * Program:  kalarm
 * SPDX-FileCopyrightText: 2015-2021 Laurent Montel <montel@kde.org>
 * SPDX-FileCopyrightText: 2019 David Jarvie <djarvie@kde.org>
 *
 * based on code from Sune Vuorela <sune@vuorela.dk> (Rawatar source code)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "migratekde4files.h"
#if KCOREADDONS_VERSION < QT_VERSION_CHECK(6, 0, 0)
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
#endif
// vim: et sw=4:
