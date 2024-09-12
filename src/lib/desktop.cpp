/*
 *  desktop.cpp  -  desktop functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2008-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "desktop.h"

#include "config-kalarm.h"

#include <QApplication>
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
    if (desktop == QLatin1StringView("KDE"))    return Type::Kde;
    if (desktop == QLatin1StringView("Unity"))  return Type::Unity;
    return Type::Other;
}

/******************************************************************************
* Return the size of the usable area of the desktop, optionally for a specific
* screen in a multi-head setup.
*/
QRect workArea(int screen)
{
    QScreen* s;
    if (screen < 0)
        s = QApplication::primaryScreen();
    else
    {
        const QList<QScreen*> screens = QGuiApplication::screens();
        if (screen >= screens.count())
            return {};
        s = screens[screen];
    }
    return s->availableGeometry();
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
