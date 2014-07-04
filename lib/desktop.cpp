/*
 *  desktop.cpp  -  desktop functions
 *  Program:  kalarm
 *  Copyright © 2008,2009 by David Jarvie <djarvie@kde.org>
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
#include "config-kdepim.h"
#if KDEPIM_HAVE_X11
#include <kwindowsystem.h>
#endif
#include <QApplication>
#include <QDesktopWidget>

namespace KAlarm {

/******************************************************************************
* Return the size of the usable area of the desktop, optionally for a specific
* screen in a multi-head setup.
*/
QRect desktopWorkArea(int screen)
{
#if KDEPIM_HAVE_X11
    if (screen < 0)
        return KWindowSystem::workArea();
#endif
    return qApp->desktop()->availableGeometry(screen);
}

} // namespace KAlarm

// vim: et sw=4:
