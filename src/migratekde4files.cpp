/*
 * migratekde4files.cpp - migrate KDE4 config and data file locations
 * Program:  kalarm
 * SPDX-FileCopyrightText: 2015-2022 Laurent Montel <montel@kde.org>
 * SPDX-FileCopyrightText: 2019-2022 David Jarvie <djarvie@kde.org>
 *
 * based on code from Sune Vuorela <sune@vuorela.dk> (Rawatar source code)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "migratekde4files.h"
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include "kalarm_debug.h"

#include <Kdelibs4Migration>
#include <Kdelibs4ConfigMigrator>
#include <KSharedConfig>
#include <KConfigGroup>

#include <QFileInfo>
#include <QFile>
#include <QDir>

namespace MigrateKde4Files
{

void migrate()
{
    const QString application = QStringLiteral("kalarm");
    const QString configFile  = QStringLiteral("kalarmrc");
    const QString configGroup = QStringLiteral("Migratekde4");

    // Migrate config and ui files to Qt5 locations.
    Kdelibs4ConfigMigrator configMigrator(application);
    configMigrator.setConfigFiles({configFile});
    configMigrator.setUiFiles({QStringLiteral("kalarmui.rc")});
    if (!configMigrator.migrate())
    {
        qCWarning(KALARM_LOG) << "MigrateKde4Files::migrate: config file migration failed";
        return;
    }

    // Migrate data files to Qt5 locations.
    KSharedConfig::Ptr config = KSharedConfig::openConfig(configFile, KConfig::SimpleConfig);
    if (config->hasGroup(configGroup))
        return;   // already migrated

    Kdelibs4Migration migrator;
    QString oldPath = migrator.locateLocal("data", application);
    if (oldPath.isEmpty())
    {
        qCWarning(KALARM_LOG) << "MigrateKde4Files::migrate: Can't find KDE4 data directory";
        return;
    }
    const QString newPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Char('/') + application + QLatin1Char('/');
    if (!QDir().mkpath(QFileInfo(newPath).absolutePath()))
    {
        qCWarning(KALARM_LOG) << "MigrateKde4Files::migrate: Error creating data directory" << QFileInfo(newPath).absolutePath();
        return;
    }

    const QStringList files = QDir(oldPath).entryList({QStringLiteral("*.ics")}, QDir::Files);
    if (!oldPath.endsWith(QLatin1Char('/')))
        oldPath += QLatin1Char('/');
    for (const QString& file : files)
    {
        if (!QFile(oldPath + file).copy(newPath + file))
            qCWarning(KALARM_LOG) << "MigrateKde4Files::migrate: Error copying" << oldPath + file << "to" << newPath;
    }

    // Record that migration has been done.
    KConfigGroup group = config->group(configGroup);
    group.writeEntry(QStringLiteral("Version"), 1);
    group.sync();

    qCDebug(KALARM_LOG) << "MigrateKde4Files::migrate: done";
}

} // namespace MigrateKde4Files

#endif

// vim: et sw=4:
