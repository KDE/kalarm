/*
 *  messagedisplayhelper_p.h  -  private declarations for MessageDisplayHelper
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009-2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QMutex>

class MessageDisplayHelper;
class AudioPlugin;

// Class to play an audio file, optionally repeated.
class AudioPlayerThread : public QObject
{
    Q_OBJECT
public:
    AudioPlayerThread(const QString& audioFile, float volume, float fadeVolume, int fadeSeconds, int repeatPause);
    ~AudioPlayerThread() override;
    void    execute();

public Q_SLOTS:
    void    stop();

Q_SIGNALS:
    void    readyToPlay();
    void    stopPlay();

private Q_SLOTS:
    void    checkAudioPlay();
    void    playFinished(bool ok);

private:
    static AudioPlayerThread* mInstance;
    mutable QMutex       mMutex;
    AudioPlugin*         mPlayer {nullptr};
    QString              mFile;
    float                mVolume;        // configured end volume
    float                mFadeVolume;    // configured start volume
    int                  mFadeSeconds;   // configured time to fade from mFadeVolume to mVolume
    int                  mRepeatPause;
    bool                 mPlayedOnce;   // the sound file has started playing at least once
    bool                 mPausing;      // currently pausing between repeats
    bool                 mStopping {false};  // the player is about to be deleted
};

// vim: et sw=4:
