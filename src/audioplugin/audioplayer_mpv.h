/*
 *  audioplayer_mpv.h  -  play an audio file using the MPV backend
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 *  SPDX-FileCopyrightText: 2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "audioplugin.h"

struct mpv_handle;

class AudioPlayerMpv : public AudioPlayer
{
    Q_OBJECT
public:
    AudioPlayerMpv* create(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    AudioPlayerMpv(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    ~AudioPlayerMpv() override;
    static bool providesFade()  { return true; }

public Q_SLOTS:
    bool    play() override;
    void    stop() override;

private Q_SLOTS:
    void    onMpvEvents();

protected:
    void setVolume() override;

private:
    static void wakeup_callback(void* ctx);

    static AudioPlayerMpv* mInstance;
    mpv_handle* mAudioInstance {nullptr};
};

// vim: et sw=4:
