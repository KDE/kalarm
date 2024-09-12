/*
 *  tooltip.h  -  tooltip functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

class QString;
class QWidget;

namespace KAlarm
{

/**
* Set a tooltip for a widget, enabling text wrapping.
* This is done by converting the text to rich text.
* @param widget   widget to set tooltip for
* @param tooltip  translated tooltip text
*/
void setToolTip(QWidget* widget, const QString& tooltip);

} // namespace KAlarm

// vim: et sw=4:
