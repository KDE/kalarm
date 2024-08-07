/*
 *  audioplayer.h  -  play an audio file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

class QTimer;

// Class to play an audio file, optionally repeated.
class AudioPlayer : public QObject
{
    Q_OBJECT
public:
    enum Type { Alarm, Sample };
    enum Status
    {
        Ready,           //!< Ready to play (newly initialised, or finished playing)
        Playing,         //!< Currently playing
        Error            //!< Something has gone wrong
    };

    static AudioPlayer* create(Type, const QUrl& audioFile, QObject* parent = nullptr);
    static AudioPlayer* create(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    ~AudioPlayer() override;
    Status  status() const;
    static QString popError();   // fetch last error message, and clear it
    static bool providesFade();

public Q_SLOTS:
    virtual bool    play() = 0;
    virtual void    stop() = 0;

Q_SIGNALS:
    void    finished(bool ok);

private Q_SLOTS:
    void    fadeStep();

protected:
    AudioPlayer(Type, const QUrl& audioFile, QObject* parent = nullptr);
    AudioPlayer(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    virtual void internalSetVolume() = 0;

    static AudioPlayer*    mInstance;
    static QString         mError;
    QString                mFile;
    float                  mVolume;        // configured end volume
    float                  mFadeVolume;    // configured start volume
    float                  mFadeStep;
    float                  mCurrentVolume;
    QTimer*                mFadeTimer {nullptr};
    time_t                 mFadeStart {0};
    int                    mFadeSeconds;   // configured time to fade from mFadeVolume to mVolume
    Status                 mStatus {Error};
    bool                   mNoFinishedSignal {false};
};


// vim: et sw=4:
