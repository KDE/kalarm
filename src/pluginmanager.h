/*
 *  pluginmanager.cpp  -  plugin manager
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2022-2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "kalarmpluginlib_export.h"

#include <QObject>

class AkonadiPlugin;
class AudioPlugin;

class KALARMPLUGINLIB_EXPORT PluginManager : public QObject
{
    Q_OBJECT
public:
    ~PluginManager() override;

    static PluginManager* instance();

    void loadPlugins();
    AkonadiPlugin*      akonadiPlugin() const;
    QList<AudioPlugin*> audioPlugins() const;
    AudioPlugin*        audioMpvPlugin() const;
    AudioPlugin*        audioVlcPlugin() const;

private:
    explicit PluginManager(QObject* parent = nullptr);

    AkonadiPlugin*      mAkonadiPlugin {nullptr};
    AudioPlugin*        mAudioMpvPlugin {nullptr};
    AudioPlugin*        mAudioVlcPlugin {nullptr};
    QList<AudioPlugin*> mAudioPlugins;
};

// vim: et sw=4:
