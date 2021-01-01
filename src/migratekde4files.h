/*
 * migratekde4files.h - migrate KDE4 config and data file locations
 * Program:  kalarm
 * SPDX-FileCopyrightText: 2015-2021 Laurent Montel <montel@kde.org>
 *
 * based on code from Sune Vuorela <sune@vuorela.dk> (Rawatar source code)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MIGRATEKDE4FILES_H
#define MIGRATEKDE4FILES_H

#include <PimCommon/MigrateApplicationFiles>

/** Class to migrate KAlarm config and data files from KDE4 locations to Qt5
 *  locations. The migration will be recorded by a [Migratekde4] entry in
 *  kalarmrc.
 */
class MigrateKde4Files
{
public:
    MigrateKde4Files();
    void migrate();

private:
    void initializeMigrator();

    PimCommon::MigrateApplicationFiles mMigrator;
};

#endif // MIGRATEKDE4FILES_H

// vim: et sw=4:
