/*
 *  audioplugin.h  -  base class for plugin to play an audio file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "pluginbaseaudio.h"
#include "audioplayer.h"

class AudioPlugin : public PluginBaseAudio
{
    Q_OBJECT
public:
    explicit AudioPlugin(QObject* parent = nullptr, const QList<QVariant>& args = {})
        : PluginBaseAudio(parent, args)
        {}

protected:
    static AudioPlayer::Type playerType(SoundCategory);
    static Status pluginStatus(AudioPlayer::Status);
};

// vim: et sw=4:
