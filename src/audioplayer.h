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

class QTemporaryFile;
class QTimer;
namespace KIO { class FileCopyJob; }
class KJob;
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
        Downloading,     //!< Downloading file
        Ready,           //!< Ready to play (newly initialised, or finished playing)
        Playing,         //!< Currently playing
        Error            //!< Something has gone wrong
    };

    static AudioPlayer* create(Type, const QUrl& audioFile, QObject* parent = nullptr);
    static AudioPlayer* create(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    ~AudioPlayer() override;
    Status  status() const;
    static QString popError();   // fetch last error message, and clear it

public Q_SLOTS:
    bool    play();
    void    stop();

Q_SIGNALS:
    void    downloaded(bool ok);
    void    finished(bool ok);

private Q_SLOTS:
    void    playFinished(uint32_t id, int errorCode);
    void    slotDownloadJobResult(KJob*);
#ifdef ENABLE_AUDIO_FADE
    void    fadeStep();
#endif

private:
    AudioPlayer(Type, const QUrl& audioFile, QObject* parent = nullptr);
    AudioPlayer(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    static void ca_finish_callback(ca_context*, uint32_t id, int error_code, void* userdata);

    static AudioPlayer*  mInstance;
    static QString       mError;
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
    KIO::FileCopyJob*    mDownloadJob {nullptr};
    QTemporaryFile*      mDownloadedFile {nullptr};  // downloaded copy of remote audio file
    ca_context*          mAudioContext {nullptr};
    ca_proplist*         mAudioProperties {nullptr};
    uint32_t             mId;
    Status               mStatus {Error};
    bool                 mPlayAfterDownload {false};
    bool                 mNoFinishedSignal {false};
};

// vim: et sw=4:
