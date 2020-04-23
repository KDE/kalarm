/*
 *  desktop.cpp  -  desktop functions
 *  Program:  kalarm
 *  Copyright © 2008-2020 David Jarvie <djarvie@kde.org>
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

#include "desktop.h"

#include "config-kalarm.h"

#if KDEPIM_HAVE_X11
#include <KWindowSystem>
#endif
#include <QGuiApplication>
#include <QScreen>
#include <QProcessEnvironment>

namespace Desktop
{

QWidget* (*mMainWindowFunc)() {nullptr};

/******************************************************************************
* Find the identity of the desktop we are running on.
*/
QString currentIdentityName()
{
    return QProcessEnvironment::systemEnvironment().value(QStringLiteral("XDG_CURRENT_DESKTOP"));
}

/******************************************************************************
* Find the identity of the desktop we are running on.
*/
Type currentIdentity()
{
    const QString desktop = currentIdentityName();
    if (desktop == QLatin1String("KDE"))    return Kde;
    if (desktop == QLatin1String("Unity"))  return Unity;
    return Other;
}

/******************************************************************************
* Return the size of the usable area of the desktop, optionally for a specific
* screen in a multi-head setup.
*/
QRect workArea(int screen)
{
#if KDEPIM_HAVE_X11
    if (screen < 0)
        return KWindowSystem::workArea();
#endif
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screen < 0  ||  screen >= screens.count())
        return QRect();
    return screens[screen]->availableGeometry();
}

/******************************************************************************
* Return the top level application window, for use as parent for dialogues etc.
*/
QWidget* mainWindow()
{
    return (*mMainWindowFunc)();
}

void setMainWindowFunc(QWidget* (*func)())
{
    mMainWindowFunc = func;
}

} // namespace Desktop

// vim: et sw=4:
