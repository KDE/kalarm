/*
 *  audioplayer.h  -  play an audio file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

// Canberra currently doesn't seem to allow the volume to be adjusted while playing.
//#define ENABLE_AUDIO_FADE

class QTimer;
struct ca_context;
struct ca_proplist;

// Class to play an audio file, optionally repeated.
class AudioPlayer : public QObject
{
    Q_OBJECT
public:
    enum Type { Alarm, Sample };
    enum Status
    {
        Ready,     //!< Ready to play (newly initialised, or finished playing)
        Playing,   //!< Currently playing
        Error      //!< Something has gone wrong
    };

    AudioPlayer(Type, const QUrl& audioFile, QObject* parent = nullptr);
    AudioPlayer(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    ~AudioPlayer() override;
    bool    play();
    Status  status() const;
    QString error() const;

public Q_SLOTS:
    void    stop();

Q_SIGNALS:
    void    finished(bool ok);

private Q_SLOTS:
    void    playFinished(uint32_t id, int errorCode);
#ifdef ENABLE_AUDIO_FADE
    void    fadeStep();
#endif

private:
    static void ca_finish_callback(ca_context*, uint32_t id, int error_code, void* userdata);

    QString              mFile;
    float                mVolume;        // configured end volume
    float                mFadeVolume;    // configured start volume
#ifdef ENABLE_AUDIO_FADE
    float                mFadeStep;
    float                mCurrentVolume;
    QTimer*              mFadeTimer {nullptr};
    time_t               mFadeStart {0};
#endif
    int                  mFadeSeconds;   // configured time to fade from mFadeVolume to mVolume
    ca_context*          mAudioContext {nullptr};
    ca_proplist*         mAudioProperties {nullptr};
    uint32_t             mId;
    QString              mError;
    Status               mStatus {Error};
    bool                 mNoFinishedSignal {false};
};

// vim: et sw=4:
