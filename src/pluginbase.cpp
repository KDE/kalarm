/*
 *  pluginbase.cpp  -  base class for plugin to provide features requiring Akonadi
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "pluginbase.h"

PluginBase::PluginBase(QObject* parent, const QList<QVariant>&)
    : QObject(parent)
{
}

PluginBase::~PluginBase() = default;

// vim: et sw=4:
