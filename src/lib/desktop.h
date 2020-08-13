/*
 *  desktop.h  -  desktop functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2008-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DESKTOP_H
#define DESKTOP_H

#include <QRect>
class QWidget;

namespace Desktop
{

/** Desktop identity, obtained from XDG_CURRENT_DESKTOP. */
enum Type
{
    Kde,      //!< KDE (KDE 4 and Plasma both identify as "KDE")
    Unity,    //!< Unity
    Other
};

Type currentIdentity();
QString currentIdentityName();

QRect workArea(int screen = -1);

/** Return the top level application window, for use as parent for dialogues etc. */
QWidget* mainWindow();

/** Set the function to return the parent window for prompt and information messages. */
void setMainWindowFunc(QWidget* (*func)());

} // namespace Desktop

#endif // DESKTOP_H

// vim: et sw=4:
