/*
 *  mainwindowbase.cpp  -  base class for main application windows
 *  Program:  kalarm
 *  Copyright © 2002,2003,2007 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"
#include "kalarmapp.h"
#include "mainwindowbase.h"



MainWindowBase::MainWindowBase(QWidget* parent, Qt::WindowFlags f)
    : KXmlGuiWindow(parent, f)
{
    setWindowModality(Qt::WindowModal);
}

#include "moc_mainwindowbase.cpp"
// vim: et sw=4:
