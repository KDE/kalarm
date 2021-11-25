/*
 *  config.h  -  config functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2006-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

class QSize;

namespace Config
{

bool readWindowSize(const char* window, QSize&, int* splitterWidth = nullptr);
void writeWindowSize(const char* window, const QSize&, int splitterWidth = -1);

} // namespace Config

// vim: et sw=4:
