/*
 *  audioplugin_vlc.cpp  -  plugin to play audio using the VLC backend
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "audioplugin_vlc.h"

#include "audioplayer_vlc.h"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(AudioPluginVlc, "audioplugin_vlc.json")

AudioPlayerVlc* AudioPluginVlc::mPlayer {nullptr};

AudioPluginVlc::AudioPluginVlc(QObject* parent, const QList<QVariant>& args)
    : AudioPlugin(parent, args)
{
    setName(args.isEmpty() ? QStringLiteral("VLC") : args[0].toString());
}

/******************************************************************************
* Create a unique instance of AudioPlayerVlc.
*/
bool AudioPluginVlc::createPlayer(SoundCategory cat, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent)
{
    if (mPlayer)
        return false;
    mPlayer = new AudioPlayerVlc(playerType(cat), audioFile, volume, fadeVolume, fadeSeconds, parent);
    connect(mPlayer, &AudioPlayerVlc::finished, this, &AudioPlugin::finished);
    return true;
}

/******************************************************************************
* Delete the plugin's audio player.
*/
void AudioPluginVlc::deletePlayer()
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
bool AudioPluginVlc::providesFade() const
{
    return AudioPlayerVlc::providesFade();
}

/******************************************************************************
* Fetch the last error message, and clear it.
*/
QString AudioPluginVlc::popError() const
{
    return AudioPlayerVlc::popError();
}

/******************************************************************************
* Return the player status.
*/
AudioPlugin::Status AudioPluginVlc::status() const
{
    if (!mPlayer)
        return Uninitialised;
    return pluginStatus(mPlayer->status());
}

/******************************************************************************
* Play the audio file.
*/
bool AudioPluginVlc::play()
{
    return mPlayer ? mPlayer->play() : false;
}

/******************************************************************************
* Stop playing the audio file.
*/
void AudioPluginVlc::stop()
{
    if (mPlayer)
        mPlayer->stop();
}

#include "audioplugin_vlc.moc"
#include "moc_audioplugin_vlc.cpp"

// vim: et sw=4:
