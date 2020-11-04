/*
 *  desktop.cpp  -  desktop functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2008-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
        return {};
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
