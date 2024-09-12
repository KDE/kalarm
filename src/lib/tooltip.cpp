/*
 *  tooltip.cpp  -  tooltip functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tooltip.h"

#include <QString>
#include <QWidget>

namespace
{
const QString tooltipWrapper = QString::fromLatin1("<qt>%1</qt>");
}

namespace KAlarm
{

/*****************************************************************************/
void setToolTip(QWidget* widget, const QString& tooltip)
{
    widget->setToolTip(tooltipWrapper.arg(tooltip.toHtmlEscaped()));
}

} // namespace KAlarm

// vim: et sw=4:
