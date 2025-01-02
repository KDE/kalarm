/*
 *  pluginmanager.h  -  plugin manager
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2022-2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "pluginmanager.h"

#include "pluginbaseakonadi.h"
#include "pluginbaseaudio.h"
#include "kalarm_debug.h"

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
    mAkonadiPlugin  = nullptr;
    mAudioVlcPlugin = nullptr;
    mAudioMpvPlugin = nullptr;

    // Load plugins which are available
    const QList<KPluginMetaData> plugins = KPluginMetaData::findPlugins(
        QStringLiteral("pim6/kalarm"));

    for (const auto& metaData : plugins)
    {
        qCDebug(KALARM_LOG) << "PluginManager::loadPlugins: found " << metaData.pluginId();
        if (pluginVersion() != metaData.version())
            qCWarning(KALARM_LOG) << "Error! Plugin" << metaData.name() << "has wrong version";
        else
        {
            // Load the plugin
            if (metaData.pluginId() == QLatin1StringView("akonadiplugin"))
            {
                auto plugin = KPluginFactory::instantiatePlugin<PluginBaseAkonadi>(metaData, this).plugin;
                if (plugin)
                    mAkonadiPlugin = (AkonadiPlugin*)plugin;
            }
            else if (metaData.pluginId() == QLatin1StringView("audioplugin_vlc"))
            {
                auto plugin = KPluginFactory::instantiatePlugin<PluginBaseAudio>(metaData, this, {metaData.name()}).plugin;
                if (plugin)
                {
                    mAudioVlcPlugin = (AudioPlugin*)plugin;
                    mAudioPlugins += mAudioVlcPlugin;
                }
            }
            else if (metaData.pluginId() == QLatin1StringView("audioplugin_mpv"))
            {
                auto plugin = KPluginFactory::instantiatePlugin<PluginBaseAudio>(metaData, this, {metaData.name()}).plugin;
                if (plugin)
                {
                    mAudioMpvPlugin = (AudioPlugin*)plugin;
                    mAudioPlugins += mAudioMpvPlugin;
                }
            }
        }
    }
}

AkonadiPlugin* PluginManager::akonadiPlugin() const
{
    return mAkonadiPlugin;
}

QList<AudioPlugin*> PluginManager::audioPlugins() const
{
    return mAudioPlugins;
}

AudioPlugin* PluginManager::audioVlcPlugin() const
{
    return mAudioVlcPlugin;
}

AudioPlugin* PluginManager::audioMpvPlugin() const
{
    return mAudioMpvPlugin;
}

#include "moc_pluginmanager.cpp"

// vim: et sw=4:
