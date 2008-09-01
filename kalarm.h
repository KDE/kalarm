/*
 *  kalarm.h  -  global header file
 *  Program:  kalarm
 *  Copyright © 2001-2008 by David Jarvie <djarvie@kde.org>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef KALARM_H
#define KALARM_H


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define KALARM_VERSION "1.5.5"
#define KALARM_NAME "KAlarm"

#include <kdeversion.h>

#define AUTOSTART_BY_KALARMD    // temporary fix for autostart before session restoration

#define OLD_DCOP    // retain DCOP pre-1.2 compatibility

#endif // KALARM_H

