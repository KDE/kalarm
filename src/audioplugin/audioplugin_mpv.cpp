/*
 *  audioplugin_mpv.cpp  -  plugin to play audio using the MPV backend
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "audioplugin_mpv.h"

#include "audioplayer_mpv.h"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(AudioPluginMpv, "audioplugin_mpv.json")

AudioPlayerMpv* AudioPluginMpv::mPlayer {nullptr};

AudioPluginMpv::AudioPluginMpv(QObject* parent, const QList<QVariant>& args)
    : AudioPlugin(parent, args)
{
    setName(args.isEmpty() ? QStringLiteral("MPV") : args[0].toString());
}

/******************************************************************************
* Create a unique instance of AudioPlayerMpv.
*/
bool AudioPluginMpv::createPlayer(SoundCategory cat, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent)
{
    if (mPlayer)
        return false;
    mPlayer = new AudioPlayerMpv(playerType(cat), audioFile, volume, fadeVolume, fadeSeconds, parent);
    connect(mPlayer, &AudioPlayerMpv::finished, this, &AudioPlugin::finished);
    return true;
}

/******************************************************************************
* Delete the plugin's audio player.
*/
void AudioPluginMpv::deletePlayer()
{
    if (mPlayer)
    {
        delete mPlayer;
        mPlayer = nullptr;
    }
}

/******************************************************************************
* Return whether the plugin provides volume fade.
*/
bool AudioPluginMpv::providesFade() const
{
    return AudioPlayerMpv::providesFade();
}

/******************************************************************************
* Fetch the last error message, and clear it.
*/
QString AudioPluginMpv::popError() const
{
    return AudioPlayerMpv::popError();
}

/******************************************************************************
* Return the player status.
*/
AudioPlugin::Status AudioPluginMpv::status() const
{
    if (!mPlayer)
        return Uninitialised;
    return pluginStatus(mPlayer->status());
}

/******************************************************************************
* Play the audio file.
*/
bool AudioPluginMpv::play()
{
    return mPlayer ? mPlayer->play() : false;
}

/******************************************************************************
* Stop playing the audio file.
*/
void AudioPluginMpv::stop()
{
    if (mPlayer)
        mPlayer->stop();
}

#include "audioplugin_mpv.moc"
#include "moc_audioplugin_mpv.cpp"

// vim: et sw=4:
