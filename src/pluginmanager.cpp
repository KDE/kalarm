/*
 *  pluginmanager.h  -  plugin manager
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "pluginmanager.h"

#include "pluginbase.h"
#include "kalarmplugin_debug.h"

#include <KPluginFactory>
#include <KPluginMetaData>

namespace
{
QString pluginVersion()
{
    return QStringLiteral("1.0");
}
}


PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
{
    loadPlugins();
}

PluginManager::~PluginManager() = default;

PluginManager* PluginManager::instance()
{
    static PluginManager theInstance;
    return &theInstance;
}

void PluginManager::loadPlugins()
{
    // Reset existing plugin data
    mAkonadiPlugin = nullptr;

    // Load plugins which are available
    const QVector<KPluginMetaData> plugins = KPluginMetaData::findPlugins(QStringLiteral("kalarm"));

    for (const auto& metaData : plugins)
    {
        if (pluginVersion() != metaData.version())
            qCWarning(KALARMPLUGIN_LOG) << "Error! Plugin" << metaData.name() << "has wrong version";
       	else
       	{
            // Load the plugin
            auto plugin = KPluginFactory::instantiatePlugin<PluginBase>(metaData, this).plugin;
            if (plugin)
            {
                if (metaData.pluginId() == QStringLiteral("akonadi"))
                    mAkonadiPlugin = (AkonadiPlugin*)plugin;
            }
        }
    }
}

AkonadiPlugin* PluginManager::akonadiPlugin() const
{
    return mAkonadiPlugin;
}

// vim: et sw=4:
