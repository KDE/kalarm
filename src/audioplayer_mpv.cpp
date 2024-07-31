/*
 *  audioplayer_mpv.cpp  -  play an audio file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024 Fabio Bas <ctrlaltca@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "audioplayer_mpv.h"

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

#include <mpv/client.h>
#include <ctime>

/******************************************************************************
* Constructor for audio player.
*/
AudioPlayerMpv::AudioPlayerMpv(Type type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent)
    : AudioPlayer(type, audioFile, volume, fadeVolume, fadeSeconds, parent)
{
    Q_UNUSED(type)
    qCDebug(KALARM_LOG) << "AudioPlayerMpv:" << mFile;

    int retval = 0;

    // Qt sets the locale in the QGuiApplication constructor, but libmpv
    // requires the LC_NUMERIC category to be set to "C", so change it back.
    std::setlocale(LC_NUMERIC, "C");

    // Create the audio instance
    mAudioInstance = mpv_create();
    if (!mAudioInstance)
    {
        mError = i18nc("@info", "Cannot initialize audio system");
        qCCritical(KALARM_LOG) << "AudioPlayerMpv: Error initializing MPV audio";
        return;
    }

    // Set playback options: Suppress video output
    if ((retval = mpv_set_option_string(mAudioInstance, "vo", "null"))  < 0)
    {
        mError = i18nc("@info", "Cannot initialize audio system: %1", QString::fromUtf8(mpv_error_string(retval)));
        qCCritical(KALARM_LOG) << "AudioPlayerMpv: Error initializing MPV audio:" << mpv_error_string(retval);
        return;
    }

    // Initialize mpv
    if ((retval = mpv_initialize(mAudioInstance))  < 0)
    {
        mError = i18nc("@info", "Cannot initialize audio system: %1", QString::fromUtf8(mpv_error_string(retval)));
        qCCritical(KALARM_LOG) << "AudioPlayerMpv: Error initializing MPV audio:" << mpv_error_string(retval);
        return;
    }

    // Register out event handler callback
    mpv_set_wakeup_callback(mAudioInstance, AudioPlayerMpv::wakeup_callback, this);

    if (mVolume > 0)
        internalSetVolume();

    mStatus = Ready;
}

/******************************************************************************
* Destructor for audio player.
*/
AudioPlayerMpv::~AudioPlayerMpv()
{
    qCDebug(KALARM_LOG) << "AudioPlayerMpv::~AudioPlayerMpv";
    if (status() == Playing)
    {
        mNoFinishedSignal = true;
        stop();
    }
    if (mAudioInstance)
    {
        mpv_set_wakeup_callback(mAudioInstance, nullptr, nullptr);
        mpv_terminate_destroy(mAudioInstance);
        mAudioInstance = nullptr;
    }
    qCDebug(KALARM_LOG) << "AudioPlayerMpv::~AudioPlayerMpv exit";
}


/******************************************************************************
* Play the audio file.
*/
bool AudioPlayerMpv::play()
{
    if (!mAudioInstance)
        return false;

    qCDebug(KALARM_LOG) << "AudioPlayerMpv::play";

    const char *cmd[] = {"loadfile", mFile.toUtf8().constData(), NULL};
    int retval = 0;
    if ((retval = mpv_command_async(mAudioInstance, 0, cmd)) < 0)
    {
        mError = xi18nc("@info", "<para>Error playing audio file <filename>%1</filename>: %2</para>",
            mFile,
            QString::fromUtf8(mpv_error_string(retval)));
        qCWarning(KALARM_LOG) << "AudioPlayerMpv::play: Failed to play sound with MPV:" << mFile << mpv_error_string(retval);
        Q_EMIT finished(false);
        return false;
    }

    if (mFadeTimer  &&  mVolume != mCurrentVolume)
    {
        mFadeStart = time(nullptr);
        mFadeTimer->start(1000);
    }
    mStatus = Playing;
    return true;
}

/******************************************************************************
* Called to set the volume.
*/
void AudioPlayerMpv::internalSetVolume()
{
    qCDebug(KALARM_LOG) << "AudioPlayerMpv::internalSetVolume" << mCurrentVolume;
    int retval = 0;
    const char* volumeLevel = QString::number(static_cast<int>(mCurrentVolume * 100)).toUtf8().constData();
    if ((retval = mpv_set_option_string(mAudioInstance, "volume", volumeLevel))  < 0)
    {
        mError = i18nc("@info", "Cannot set the audio volume: %1", QString::fromUtf8(mpv_error_string(retval)));
        qCCritical(KALARM_LOG) << "AudioPlayerMpv: Error setting MPV audio volume:" << mpv_error_string(retval);
    }
}

/******************************************************************************
* Called by MPV to notify that an event must be handled by our side.
*/
void AudioPlayerMpv::wakeup_callback(void* ctx)
{
    QMetaObject::invokeMethod(static_cast<AudioPlayerMpv*>(ctx), "onMpvEvents"); // , Qt::QueuedConnection
}

/******************************************************************************
* Called to notify play completion.
*/
void AudioPlayerMpv::onMpvEvents()
{
    qCDebug(KALARM_LOG) << "AudioPlayerMpv::onMpvEvents:" << mFile;
    // Process all events, until the event queue is empty.
    while (true)
    {
        mpv_event *event = mpv_wait_event(mAudioInstance, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;

        switch (event->event_id)
        {
            case MPV_EVENT_END_FILE:
            {
                bool result;
                mStatus = Ready;
                mFadeStart = 0;

                mpv_event_end_file* evt = static_cast<mpv_event_end_file*>(event->data);
                if (evt && evt->error != 0)
                {
                    qCCritical(KALARM_LOG) << "AudioPlayerMpv::onMpvEvents: Play failure:" << mFile << mpv_error_string(evt->error);
                    mError = xi18nc("@info", "<para>Error playing audio file <filename>%1</filename>: %2</para>",
                        mFile,
                        QString::fromUtf8(mpv_error_string(evt->error)));
                    result = false;
                }
                else
                    result = true;

                if (!mNoFinishedSignal)
                    Q_EMIT finished(result);
                break;
            }
            default:
                break;
        }
    }
}

/******************************************************************************
* Called when play completes, the Silence button is clicked, or the display is
* closed, to terminate audio access.
*/
void AudioPlayerMpv::stop()
{
    qCDebug(KALARM_LOG) << "AudioPlayerMpv::stop";
    if (!mAudioInstance)
        return;

    const char *cmd[] = {"stop"};
    mpv_command_async(mAudioInstance, 0, cmd);
}

// vim: et sw=4:

#include "moc_audioplayer_mpv.cpp"
