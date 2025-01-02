/*
 *  audioplugin_vlc.h  -  plugin to play audio using the VLC backend
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "audioplugin.h"

class AudioPlayerVlc;

class AudioPluginVlc : public AudioPlugin
{
    Q_OBJECT
public:
    explicit AudioPluginVlc(QObject* parent = nullptr, const QList<QVariant>& = {});

    /** Create a unique audio player using the VLC backend.
     *  The player must be deleted when finished with by calling deletePlayer().
     *  @return the new audio player which was created, or null if error or a player already exists.
     */
    bool createPlayer(SoundCategory, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr) override;

    /** Delete the plugin's audio player. */
    void deletePlayer() override;

    /** Return whether the plugin provides volume fade. */
    bool providesFade() const override;

    /** Fetch the last error message, and clear it. */
    QString popError() const override;

    /** Return the play status. */
    Status status() const override;

    /* Start playing the audio file. */
    bool play() override;

public Q_SLOTS:
    /** Stop playing the audio file. */
    void stop() override;

private:
    static AudioPlayerVlc* mPlayer;
};

// vim: et sw=4:
