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
#include <KLocalizedString>
//#include <KIO/StatJob>
//#include <KIO/StoredTransferJob>

#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QIcon>
//#include <QLocale>
//#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

#include <canberra.h>
#ifdef ENABLE_AUDIO_FADE
#include <ctime>
#endif

namespace
{
const uint32_t ALARM_ID = 1;
const uint32_t SAMPLE_ID = 2;

inline const char* volumeToDbString(float volume)
{
    static QByteArray result;   // ensure that the result remains valid for use
    result = QByteArray::number(log(volume)*15, 'f', 2);
    return result.constData();
}

}

AudioPlayer::AudioPlayer(Type type, const QUrl& audioFile, QObject* parent)
    : AudioPlayer(type, audioFile, -1, -1, 0, parent)
{
}

/******************************************************************************
* Constructor for audio player.
*/
AudioPlayer::AudioPlayer(Type type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent)
    : QObject(parent)
    , mFile(audioFile.isLocalFile() ? audioFile.toLocalFile() : audioFile.toString())
    , mVolume(volume)
    , mFadeVolume(fadeVolume)
    , mFadeSeconds(fadeSeconds)
    , mId(type == Alarm ? ALARM_ID : type == Sample ? SAMPLE_ID : 0)
{
    qCDebug(KALARM_LOG) << "AudioPlayer:" << mFile;

    int result = ca_context_create(&mAudioContext);
    if (result != CA_SUCCESS)
    {
        mAudioContext = nullptr;
        mError = xi18nc("@info", "Cannot initialize audio player: %1", QString::fromUtf8(ca_strerror(result)));
        qCCritical(KALARM_LOG) << "AudioPlayer: Error initializing audio:" << ca_strerror(result);
        return;
    }
    result = ca_context_change_props(mAudioContext,
                                     CA_PROP_APPLICATION_NAME,
                                     qUtf8Printable(KAboutData::applicationData().displayName()),
                                     CA_PROP_APPLICATION_ID,
                                     qUtf8Printable(KAboutData::applicationData().componentName()),
                                     CA_PROP_APPLICATION_ICON_NAME,
                                     qUtf8Printable(qApp->windowIcon().name()),
                                     CA_PROP_EVENT_ID,
                                     KALARM_NAME,
                                     nullptr);
    if (result != CA_SUCCESS)
        qCWarning(KALARM_LOG) << "AudioPlayer: Failed to set application properties on canberra context for audio:" << ca_strerror(result);

    result = ca_proplist_create(&mAudioProperties);
    if (result != CA_SUCCESS)
    {
        mAudioProperties = nullptr;
        ca_context_destroy(mAudioContext);
        mAudioContext = nullptr;
        mError = xi18nc("@info", "Cannot set audio player properties: %1", QString::fromUtf8(ca_strerror(result)));
        qCCritical(KALARM_LOG) << "AudioPlayer: Error initializing audio properties:" << ca_strerror(result);
        return;
    }
//TODO Download remote file?
    ca_proplist_sets(mAudioProperties, CA_PROP_MEDIA_FILENAME, QFile::encodeName(mFile).constData());
    if (mVolume > 0)
    {
#ifdef ENABLE_AUDIO_FADE
        if (mFadeVolume >= 0  &&  mFadeSeconds > 0)
        {
            mFadeStep = (mVolume - mFadeVolume) / mFadeSeconds;
            mCurrentVolume = mFadeVolume;
            mFadeTimer = new QTimer(this);
            connect(mFadeTimer, &QTimer::timeout, this, &AudioPlayer::fadeStep);
        }
        else
            mCurrentVolume = mVolume;
        ca_proplist_sets(mAudioProperties, CA_PROP_CANBERRA_VOLUME, volumeToDbString(mCurrentVolume));
#else
        ca_proplist_sets(mAudioProperties, CA_PROP_CANBERRA_VOLUME, volumeToDbString(mVolume));
#endif
    }
    // We'll also want this cached for a time. volatile makes sure the cache is
    // dropped after some time or when the cache is under pressure.
    ca_proplist_sets(mAudioProperties, CA_PROP_CANBERRA_CACHE_CONTROL, "volatile");

    mStatus = Ready;
}

/******************************************************************************
* Destructor for audio player.
*/
AudioPlayer::~AudioPlayer()
{
    qCDebug(KALARM_LOG) << "AudioPlayer::~AudioPlayer";
    if (status() == Playing)
    {
        mNoFinishedSignal = true;
        stop();
    }
    if (mAudioContext)
    {
        ca_context_destroy(mAudioContext);
        mAudioContext = nullptr;
    }
    if (mAudioProperties)
    {
        ca_proplist_destroy(mAudioProperties);
        mAudioProperties = nullptr;
    }
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
* Play the audio file.
*/
bool AudioPlayer::play()
{
    if (!mAudioContext)
        return false;

    qCDebug(KALARM_LOG) << "AudioPlayer::play";
    const int result = ca_context_play_full(mAudioContext, mId, mAudioProperties, &ca_finish_callback, this);
    if (result != CA_SUCCESS)
    {
        mError = xi18nc("@info", "<para>Error playing audio file: <filename>%1</filename></para><para>%2</para>", mFile, QString::fromUtf8(ca_strerror(result)));
        qCWarning(KALARM_LOG) << "AudioPlayer::play: Failed to play sound with canberra:" << ca_strerror(result);
        return false;
    }
#ifdef ENABLE_AUDIO_FADE
    if (mVolume != mCurrentVolume)
    {
        mFadeStart = time(nullptr);
        mFadeTimer->start(1000);
    }
#endif
    mStatus = Playing;
    return true;
}

#ifdef ENABLE_AUDIO_FADE
// Canberra currently doesn't seem to allow the volume to be adjusted while playing.

/******************************************************************************
* Called every second to fade the volume.
*/
void AudioPlayer::fadeStep()
{
    qCDebug(KALARM_LOG) << "AudioPlayer::fadeStep";
    if (mFadeStart)
    {
        float volume;
        time_t elapsed = time(nullptr) - mFadeStart;
        if (elapsed >= mFadeSeconds)
        {
            volume = mVolume;
            mFadeStart = 0;
            mFadeTimer->stop();
        }
        else
            volume = mFadeVolume + (mVolume - mFadeVolume) * elapsed / mFadeSeconds;
volume = 1.0f;
        ca_context_change_props(mAudioContext,
                                CA_PROP_CANBERRA_VOLUME, volumeToDbString(volume),
                                nullptr);
    }
}
#endif

/******************************************************************************
* Called by Canberra to notify play completion or cancellation.
*/
void AudioPlayer::ca_finish_callback(ca_context*, uint32_t id, int errorCode, void* userdata)
{
    QMetaObject::invokeMethod(static_cast<AudioPlayer*>(userdata), "playFinished", Q_ARG(uint32_t, id), Q_ARG(int, errorCode));
}

/******************************************************************************
* Called to notify play completion or cancellation.
*/
void AudioPlayer::playFinished(uint32_t id, int errorCode)
{
    Q_UNUSED(id)
    qCDebug(KALARM_LOG) << "AudioPlayer::playFinished:" << mFile;
    mStatus = Ready;
#ifdef ENABLE_AUDIO_FADE
    mFadeStart = 0;
#endif
    bool result;
    switch (errorCode)
    {
        case CA_SUCCESS:
        case CA_ERROR_CANCELED:
            result = true;
            break;
        default:
        {
            qCCritical(KALARM_LOG) << "AudioPlayer::playFinished: Play failure:" << mFile << ":" << ca_strerror(errorCode);
            mError = xi18nc("@info", "<para>Error playing audio file: <filename>%1</filename></para><para>%2</para>", mFile, QString::fromUtf8(ca_strerror(errorCode)));
            result = false;
            break;
        }
    }

    if (!mNoFinishedSignal)
        Q_EMIT finished(result);
}

/******************************************************************************
* Called when play completes, the Silence button is clicked, or the display is
* closed, to terminate audio access.
*/
void AudioPlayer::stop()
{
    qCDebug(KALARM_LOG) << "AudioPlayer::stop";
    if (mAudioContext)
    {
        const int result = ca_context_cancel(mAudioContext, mId);
        if (result != CA_SUCCESS)
            qCWarning(KALARM_LOG) << "AudioPlayer::stop: Failed to cancel audio playing:" << ca_strerror(result);
    }
}

QString AudioPlayer::error() const
{
    return mError;
}

// vim: et sw=4:
