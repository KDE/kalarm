/*
 *  audioplayer_mpv.h  -  play an audio file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "audioplayer.h"

struct mpv_handle;

class AudioPlayerMpv : public AudioPlayer
{
    Q_OBJECT
public:
    AudioPlayerMpv(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    ~AudioPlayerMpv() override;

public Q_SLOTS:
    bool    play() override;
    void    stop() override;

private Q_SLOTS:
    void    onMpvEvents();

protected:
    void internalSetVolume() override;

private:
    static void wakeup_callback(void* ctx);

    mpv_handle* mAudioInstance {nullptr};
};

// vim: et sw=4:
