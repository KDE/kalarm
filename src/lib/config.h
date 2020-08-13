/*
 *  config.h  -  config functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2006-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
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
