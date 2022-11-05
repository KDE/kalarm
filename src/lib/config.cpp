/*
 *  config.cpp  -  config functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2006-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QApplication>
#include <QScreen>

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
    const QSize desktopSize = QApplication::primaryScreen()->virtualSize();
    const QSize s = QSize(config.readEntry(QStringLiteral("Width %1").arg(desktopSize.width()), (int)0),
                          config.readEntry(QStringLiteral("Height %1").arg(desktopSize.height()), (int)0));
    if (s.isEmpty())
        return false;
    result = s;
    if (splitterWidth)
        *splitterWidth = config.readEntry(QStringLiteral("Splitter %1").arg(desktopSize.width()), -1);
    return true;
}

/******************************************************************************
* Write the size for the specified window to the config file, for the
* current screen resolution.
*/
void writeWindowSize(const char* window, const QSize& size, int splitterWidth)
{
    KConfigGroup config(KSharedConfig::openConfig(), window);
    const QSize desktopSize = QApplication::primaryScreen()->virtualSize();
    config.writeEntry(QStringLiteral("Width %1").arg(desktopSize.width()), size.width());
    config.writeEntry(QStringLiteral("Height %1").arg(desktopSize.height()), size.height());
    if (splitterWidth >= 0)
        config.writeEntry(QStringLiteral("Splitter %1").arg(desktopSize.width()), splitterWidth);
    config.sync();
}

}

// vim: et sw=4:
