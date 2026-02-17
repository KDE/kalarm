/*
 *  audioplayer_vlc.h  -  play an audio file using the VLC backend
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024-2026 David Jarvie <djarvie@kde.org>
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
    void playStarted(uint32_t event);
    void playFinished(uint32_t event);
    void checkVolume() { setVolume(); }
    void checkPlay();

protected:
    void setVolume() override;

private:
    static void playing_callback(const libvlc_event_t* event, void* data);
    static void finish_callback(const libvlc_event_t* event, void* data);

    static AudioPlayerVlc* mInstance;
    libvlc_instance_t*     mAudioInstance {nullptr};
    libvlc_media_t*        mAudioMedia {nullptr};
    libvlc_media_player_t* mAudioPlayer {nullptr};
    QTimer*                mCheckPlayTimer {nullptr};
    QTimer*                mStartVolumeTimer {nullptr};
    int                    mStartVolumeCount {0};
    bool                   mStartVolumeWrong;   // a wrong volume has been found after play start
};

// vim: et sw=4:
