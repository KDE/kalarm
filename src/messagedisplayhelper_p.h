/*
 *  messagedisplayhelper_p.h  -  private declarations for MessageDisplayHelper
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009-2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QMutex>

// Canberra currently doesn't seem to allow the volume to be adjusted while playing.
//#define ENABLE_FADE

class QTimer;
class MessageDisplayHelper;
struct ca_context;
struct ca_proplist;

// Class to play an audio file, optionally repeated.
class AudioPlayer : public QObject
{
    Q_OBJECT
public:
    AudioPlayer(const QString& audioFile, float volume, float fadeVolume, int fadeSeconds, int repeatPause);
    ~AudioPlayer() override;
    void    execute();
    QString error() const;

public Q_SLOTS:
    void    stop();

Q_SIGNALS:
    void    readyToPlay();

private Q_SLOTS:
    void    checkAudioPlay();
    void    playFinished(uint32_t id, int errorCode);
#ifdef ENABLE_FADE
    void    fadeStep();
#endif

private:
    static void ca_finish_callback(ca_context*, uint32_t id, int error_code, void* userdata);

    static AudioPlayer*  mInstance;
    mutable QMutex       mMutex;
    QString              mFile;
    float                mVolume;        // configured end volume
    float                mFadeVolume;    // configured start volume
#ifdef ENABLE_FADE
    float                mFadeStep;
    float                mCurrentVolume;
    QTimer*              mFadeTimer {nullptr};
    time_t               mFadeStart {0};
#endif
    int                  mFadeSeconds;   // configured time to fade from mFadeVolume to mVolume
    int                  mRepeatPause;
    ca_context*          mAudioContext {nullptr};
    ca_proplist*         mAudioProperties {nullptr};
    QString              mError;
    bool                 mPlayedOnce;   // the sound file has started playing at least once
    bool                 mPausing;      // currently pausing between repeats
    bool                 mStopping {false};  // the player is about to be deleted
};

// vim: et sw=4:
