/*
 *  config.cpp  -  config functions
 *  Program:  kalarm
 *  Copyright © 2006-2019 David Jarvie <djarvie@kde.org>
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

#include "config.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QDesktopWidget>
#include <QApplication>

namespace Config
{

/******************************************************************************
* Read the size for the specified window from the config file, for the
* current screen resolution.
* Reply = true if size set in the config file, in which case 'result' is set
*       = false if no size is set, in which case 'result' is unchanged.
*/
bool readWindowSize(const char* window, QSize& result, int* splitterWidth)
{
    KConfigGroup config(KSharedConfig::openConfig(), window);
    const QWidget* desktop = QApplication::desktop();
    const QSize s = QSize(config.readEntry(QStringLiteral("Width %1").arg(desktop->width()), (int)0),
                          config.readEntry(QStringLiteral("Height %1").arg(desktop->height()), (int)0));
    if (s.isEmpty())
        return false;
    result = s;
    if (splitterWidth)
        *splitterWidth = config.readEntry(QStringLiteral("Splitter %1").arg(desktop->width()), -1);
    return true;
}

/******************************************************************************
* Write the size for the specified window to the config file, for the
* current screen resolution.
*/
void writeWindowSize(const char* window, const QSize& size, int splitterWidth)
{
    KConfigGroup config(KSharedConfig::openConfig(), window);
    const QWidget* desktop = QApplication::desktop();
    config.writeEntry(QStringLiteral("Width %1").arg(desktop->width()), size.width());
    config.writeEntry(QStringLiteral("Height %1").arg(desktop->height()), size.height());
    if (splitterWidth >= 0)
        config.writeEntry(QStringLiteral("Splitter %1").arg(desktop->width()), splitterWidth);
    config.sync();
}

}

// vim: et sw=4:
