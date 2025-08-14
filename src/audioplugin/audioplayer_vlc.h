/*
 *  audioplayer_vlc.h  -  play an audio file using the VLC backend
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024-2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "audioplugin.h"

struct libvlc_instance_t;
struct libvlc_media_t;
struct libvlc_media_player_t;
struct libvlc_event_t;

class AudioPlayerVlc : public AudioPlayer
{
    Q_OBJECT
public:
    static AudioPlayerVlc* create(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    AudioPlayerVlc(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    ~AudioPlayerVlc() override;
    static bool providesFade()  { return true; }

public Q_SLOTS:
    bool play() override;
    void stop() override;

private Q_SLOTS:
    void playFinished(uint32_t event);
    void checkPlay();

protected:
    void setVolume() override;

private:
    static void finish_callback(const libvlc_event_t* event, void* data);

    static AudioPlayerVlc* mInstance;
    libvlc_instance_t*     mAudioInstance {nullptr};
    libvlc_media_t*        mAudioMedia {nullptr};
    libvlc_media_player_t* mAudioPlayer {nullptr};
    QTimer*                mCheckPlayTimer {nullptr};
};

// vim: et sw=4:
