/*
 *  i18n.cpp  -  i18n related functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "i18n.h"

#include <QRegularExpression>

/******************************************************************************
* Remove enclosing <html> tags from an xi18n() string, to enable it to be
* cleanly incorporated into another xi18n() string as a substituted parameter.
*/
QString xi18nAsSubsParam(const QString& xi18nString)
{
    QRegularExpressionMatch m = QRegularExpression(QStringLiteral("^<html>(.*)</html>$")).match(xi18nString);
    return m.hasMatch() ? m.captured(1) : xi18nString;
}

// vim: et sw=4:
