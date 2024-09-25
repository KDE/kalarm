/*
 *  audioplayer_vlc.cpp  -  play an audio file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "audioplayer_vlc.h"

#include "kalarm.h"
#include "kalarm_debug.h"

#include <KLocalizedString>

#include <QFile>
#include <QTimer>
#include <QUrl>

#include <vlc/vlc.h>

/******************************************************************************
* Constructor for audio player.
*/
AudioPlayerVlc::AudioPlayerVlc(Type type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent)
    : AudioPlayer(type, audioFile, volume, fadeVolume, fadeSeconds, parent)
{
    Q_UNUSED(type)
    qCDebug(KALARM_LOG) << "AudioPlayerVlc:" << mFile;

    // Create the audio instance, and suppress video (which would cause havoc to KAlarm).
    const char* argv[] = { "--no-video" };
    mAudioInstance = libvlc_new(1, argv);
    if (!mAudioInstance)
    {
        setErrorStatus(i18nc("@info", "Cannot initialize audio system"));
        qCCritical(KALARM_LOG) << "AudioPlayer: Error initializing VLC audio";
        return;
    }

    mAudioMedia = audioFile.isLocalFile()
                ? libvlc_media_new_path(mAudioInstance, QFile::encodeName(mFile).constData())
                : libvlc_media_new_location(mAudioInstance, mFile.toLocal8Bit().constData());
    if (!mAudioMedia)
    {
        setErrorStatus(xi18nc("@info", "<para>Error opening audio file: <filename>%1</filename></para>", mFile));
        qCCritical(KALARM_LOG) << "AudioPlayer: Error opening audio file:" << mFile;
        return;
    }

    setOkStatus(Ready);
}

/******************************************************************************
* Destructor for audio player.
*/
AudioPlayerVlc::~AudioPlayerVlc()
{
    qCDebug(KALARM_LOG) << "AudioPlayerVlc::~AudioPlayerVlc";
    if (status() == Playing)
    {
        mNoFinishedSignal = true;
        stop();
    }
    if (mAudioPlayer)
    {
        libvlc_media_player_release(mAudioPlayer);
        mAudioPlayer = nullptr;
    }
    if (mAudioMedia)
    {
        libvlc_media_release(mAudioMedia);
        mAudioMedia = nullptr;
    }
    if (mAudioInstance)
    {
        libvlc_release(mAudioInstance);
        mAudioInstance = nullptr;
    }
    qCDebug(KALARM_LOG) << "AudioPlayerVlc::~AudioPlayerVlc exit";
}

/******************************************************************************
* Play the audio file.
*/
bool AudioPlayerVlc::play()
{
    if (mAudioPlayer)
        return false;

    qCDebug(KALARM_LOG) << "AudioPlayerVlc::play";

    // Note that libVLC has some issues which require workarounds to allow
    // audio files to be replayed.
    // There doesn't seem to be any way of replaying the audio file if the
    // media player is reused, so it's necessary to create a new media player
    // each time the audio file is played.
    // Using a media list player instead can allow replaying to work, but it
    // fails on some systems with a VLC "cache_read stream error".
    mAudioPlayer = libvlc_media_player_new_from_media(mAudioMedia);
    if (!mAudioPlayer)
    {
        setErrorStatus(i18nc("@info", "Cannot initialize audio player"));
        qCCritical(KALARM_LOG) << "AudioPlayer: Error initializing audio player";
        return false;
    }
    libvlc_media_player_set_role(mAudioPlayer, libvlc_role_Notification);

    if (mVolume > 0)
        internalSetVolume();

    libvlc_event_manager_t* eventManager = libvlc_media_player_event_manager(mAudioPlayer);
    if (libvlc_event_attach(eventManager, libvlc_MediaPlayerStopped, &finish_callback, this))
    {
        qCWarning(KALARM_LOG) << "AudioPlayerVlc: Error setting completion callback";
        if (!mCheckPlayTimer)
        {
            mCheckPlayTimer = new QTimer(this);
            connect(mCheckPlayTimer, &QTimer::timeout, this, &AudioPlayerVlc::checkPlay);
        }
    }
    // Does the Error event need to be watched??
    libvlc_event_attach(eventManager, libvlc_MediaPlayerEncounteredError, &finish_callback, this);

    if (libvlc_media_player_play(mAudioPlayer) < 0)
    {
        setErrorStatus(xi18nc("@info", "<para>Error playing audio file: <filename>%1</filename></para>", mFile));
        qCWarning(KALARM_LOG) << "AudioPlayerVlc::play: Failed to play sound with VLC:" << mFile;
        Q_EMIT finished(false);
        return false;
    }
    if (mFadeTimer  &&  mVolume != mCurrentVolume)
    {
        mFadeStart = time(nullptr);
        mFadeTimer->start(1000);
    }
    if (mCheckPlayTimer)
        mCheckPlayTimer->start(1000);
    setOkStatus(Playing);
    return true;
}

/******************************************************************************
* Called to set the volume.
*/
void AudioPlayerVlc::internalSetVolume()
{
    qCDebug(KALARM_LOG) << "AudioPlayerVlc::internalSetVolume" << mCurrentVolume;
    libvlc_audio_set_volume(mAudioPlayer, static_cast<int>(mCurrentVolume * 100));
}

/******************************************************************************
* Called on timer if attach to stop event failed, to check for completion.
*/
void AudioPlayerVlc::checkPlay()
{
    if (!libvlc_media_player_is_playing(mAudioPlayer))
        playFinished(libvlc_MediaPlayerStopped);
}

/******************************************************************************
* Called by VLC to notify play completion or cancellation.
*/
void AudioPlayerVlc::finish_callback(const libvlc_event_t* event, void* userdata)
{
    QMetaObject::invokeMethod(static_cast<AudioPlayerVlc*>(userdata), "playFinished", Q_ARG(uint32_t, event->type));
    if (event->type == libvlc_MediaPlayerEncounteredError)
        qCWarning(KALARM_LOG) << "AudioPlayerVlc: Error while playing";
}

/******************************************************************************
* Called to notify play completion.
*/
void AudioPlayerVlc::playFinished(uint32_t event)
{
    setOkStatus(Ready);
    mFadeStart = 0;
    if (mCheckPlayTimer)
        mCheckPlayTimer->stop();
    bool result;
    switch (event)
    {
        case libvlc_MediaPlayerStopped:
            qCDebug(KALARM_LOG) << "AudioPlayerVlc::playFinished:" << mFile;
            if (mAudioPlayer)
            {
                libvlc_media_player_release(mAudioPlayer);
                mAudioPlayer = nullptr;
            }
            result = true;
            break;
        default:
        {
            qCCritical(KALARM_LOG) << "AudioPlayerVlc::playFinished: Play failure:" << mFile;
            setErrorStatus(xi18nc("@info", "<para>Error playing audio file: <filename>%1</filename></para>", mFile));
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
void AudioPlayerVlc::stop()
{
    qCDebug(KALARM_LOG) << "AudioPlayerVlc::stop";
    if (mCheckPlayTimer)
        mCheckPlayTimer->stop();
    if (mAudioPlayer  &&  libvlc_media_player_is_playing(mAudioPlayer))
        libvlc_media_player_stop(mAudioPlayer);
}

#include "moc_audioplayer_vlc.cpp"

// vim: et sw=4:
