/*
 *  i18n.h  -  i18n related functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

class QString;

/** Remove enclosing <html> tags from an xi18n() string, to enable it to be
 *  cleanly incorporated into another xi18n() string as a substituted parameter.
 */
QString xi18nAsSubsParam(const QString& xi18nString);

// vim: et sw=4:
