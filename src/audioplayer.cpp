/*
 *  audioplayer.cpp  -  play an audio file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "audioplayer.h"

#include "kalarm.h"
#include "kalarm_debug.h"

#include <KAboutData>
#include <KIO/Job>
#include <KIO/FileCopyJob>
#include <KLocalizedString>

#include <QApplication>
#include <QByteArray>
#include <QEventLoopLocker>
#include <QFile>
#include <QIcon>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

#include <ctime>

#ifdef HAVE_LIBVLC
#include "audioplayer_vlc.h"
#elif defined(HAVE_LIBMPV)
#include "audioplayer_mpv.h"
#endif

AudioPlayer* AudioPlayer::mInstance = nullptr;
QString      AudioPlayer::mError;

/******************************************************************************
* Create a unique instance of AudioPlayer.
*/
AudioPlayer* AudioPlayer::create(Type type, const QUrl& audioFile, QObject* parent)
{
    return create(type, audioFile, -1, -1, 0, parent);
}

AudioPlayer* AudioPlayer::create(Type type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent)
{
    if (mInstance)
        return nullptr;
    mInstance = new
#ifdef HAVE_LIBVLC
        AudioPlayerVlc
#elif defined(HAVE_LIBMPV)
        AudioPlayerMpv
#endif
            (type, audioFile, volume, fadeVolume, fadeSeconds, parent);
    return mInstance;
}

/******************************************************************************
* Constructor for audio player.
*/
AudioPlayer::AudioPlayer(Type type, const QUrl& audioFile, QObject* parent)
    : AudioPlayer(type, audioFile, -1, -1, 0, parent)
{
}

AudioPlayer::AudioPlayer(Type type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent)
    : QObject(parent)
    , mFile(audioFile.isLocalFile() ? audioFile.toLocalFile() : audioFile.toString())
    , mVolume(volume)
    , mFadeVolume(fadeVolume)
    , mFadeSeconds(fadeSeconds)
{
    Q_UNUSED(type)
    qCDebug(KALARM_LOG) << "AudioPlayer:" << mFile;

    mError.clear();

    if (mVolume > 0)
    {
        if (mFadeVolume >= 0  &&  mFadeSeconds > 0)
        {
            mFadeStep = (mVolume - mFadeVolume) / mFadeSeconds;
            mCurrentVolume = mFadeVolume;
            mFadeTimer = new QTimer(this);
            connect(mFadeTimer, &QTimer::timeout, this, &AudioPlayer::fadeStep);
        }
        else
            mCurrentVolume = mVolume;
    }
}

/******************************************************************************
* Destructor for audio player.
*/
AudioPlayer::~AudioPlayer()
{
    qCDebug(KALARM_LOG) << "AudioPlayer::~AudioPlayer";
    mInstance = nullptr;
    qCDebug(KALARM_LOG) << "AudioPlayer::~AudioPlayer exit";
}

/******************************************************************************
* Return the player status.
*/
AudioPlayer::Status AudioPlayer::status() const
{
    return mError.isEmpty() ? mStatus : Error;
}


/******************************************************************************
* Called every second to fade the volume.
*/
void AudioPlayer::fadeStep()
{
    qCDebug(KALARM_LOG) << "AudioPlayer::fadeStep";
    if (mFadeStart)
    {
        time_t elapsed = time(nullptr) - mFadeStart;
        if (elapsed >= mFadeSeconds)
        {
            mCurrentVolume = mVolume;
            mFadeStart = 0;
            mFadeTimer->stop();
        }
        else
            mCurrentVolume = mFadeVolume + (mVolume - mFadeVolume) * elapsed / mFadeSeconds;
        internalSetVolume();
    }
}

QString AudioPlayer::popError()
{
    const QString err = mError;
    mError.clear();
    return err;
}

// vim: et sw=4:

#include "moc_audioplayer.cpp"
