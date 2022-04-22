/*
 * migratekde4files.h - migrate KDE4 config and data file locations
 * Program:  kalarm
 * SPDX-FileCopyrightText: 2022 David Jarvie <djarvie@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)

namespace MigrateKde4Files
{

/** Migrate KAlarm config and data files from KDE4 locations to Qt5
 *  locations. The migration will be recorded by a [Migratekde4] entry
 *  in kalarmrc.
 */
void migrate();

}

#endif

// vim: et sw=4:
