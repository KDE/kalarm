/*
 *  audioplayer_vlc.h  -  play an audio file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "audioplayer.h"

struct libvlc_instance_t;
struct libvlc_media_list_t;
struct libvlc_media_list_player_t;
struct libvlc_event_t;

class AudioPlayerVlc : public AudioPlayer
{
    Q_OBJECT
public:
    AudioPlayerVlc(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    ~AudioPlayerVlc() override;
    static bool backendProvidesFade() { return true; }

public Q_SLOTS:
    bool    play() override;
    void    stop() override;

private Q_SLOTS:
    void    playFinished(uint32_t event);
    void    checkPlay();

protected:
    void internalSetVolume() override;

private:
    static void finish_callback(const libvlc_event_t* event, void* data);

    libvlc_instance_t*          mAudioInstance {nullptr};
    libvlc_media_list_t*        mMediaList {nullptr};
    libvlc_media_list_player_t* mAudioPlayer {nullptr};
    QTimer*                     mCheckPlayTimer {nullptr};
    bool                        mPlayedAlready {false};
};

// vim: et sw=4:
