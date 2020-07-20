/*
 * migratekde4files.h - migrate KDE4 config and data file locations
 * Program:  kalarm
 * Copyright (c) 2015-2020 Laurent Montel <montel@kde.org>
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