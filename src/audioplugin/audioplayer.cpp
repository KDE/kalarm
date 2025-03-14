/*
 *  audioplayer.cpp  -  play an audio file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024-2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "audioplayer.h"

#include "audioplugin_debug.h"

#include <QTimer>
#include <QUrl>

QString AudioPlayer::mError;

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
    qCDebug(AUDIOPLUGIN_LOG) << "AudioPlayer:" << mFile;

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
* Return the player status.
*/
AudioPlayer::Status AudioPlayer::status() const
{
    return mError.isEmpty() ? mStatus : Error;
}

/******************************************************************************
* Reset fade to its initial status and value.
*/
void AudioPlayer::resetFade()
{
    if (mFadeTimer)
    {
        mFadeStart = 0;
        mCurrentVolume = mFadeVolume;
    }
}

/******************************************************************************
* Called every second to fade the volume.
*/
void AudioPlayer::fadeStep()
{
    qCDebug(AUDIOPLUGIN_LOG) << "AudioPlayer::fadeStep";
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
        setVolume();
    }
}

/******************************************************************************
* Set the status to a non-error value.
*/
void AudioPlayer::setOkStatus(Status stat)
{
    Q_ASSERT(stat != Error);
    mStatus = stat;
}

/******************************************************************************
* Set the status to Error, and set the error message to display to the user.
*/
void AudioPlayer::setErrorStatus(const QString& errorMessage)
{
    mError = errorMessage;
    mStatus = Error;
}

QString AudioPlayer::popError()
{
    const QString err = mError;
    mError.clear();
    return err;
}

#include "moc_audioplayer.cpp"

// vim: et sw=4:
