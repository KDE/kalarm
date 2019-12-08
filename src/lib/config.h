/*
 *  config.h  -  config functions
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

#ifndef LIB_CONFIG_H
#define LIB_CONFIG_H

class QSize;

namespace Config
{

bool readWindowSize(const char* window, QSize&, int* splitterWidth = nullptr);
void writeWindowSize(const char* window, const QSize&, int splitterWidth = -1);

} // namespace Config

#endif // LIB_CONFIG_H

// vim: et sw=4:
