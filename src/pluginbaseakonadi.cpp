/*
 *  pluginbaseakonadi.cpp  -  base class for plugin to provide features requiring Akonadi
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2022-2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "pluginbaseakonadi.h"

PluginBaseAkonadi::PluginBaseAkonadi(QObject* parent, const QList<QVariant>&)
    : QObject(parent)
{
}

PluginBaseAkonadi::~PluginBaseAkonadi() = default;

#include "moc_pluginbaseakonadi.cpp"

// vim: et sw=4:
