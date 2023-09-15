/*
 *  desktop.h  -  desktop functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2008-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QRect>
class QWidget;

namespace Desktop
{

/** Desktop identity, obtained from XDG_CURRENT_DESKTOP. */
enum class Type
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

// vim: et sw=4:
