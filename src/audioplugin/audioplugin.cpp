/*
 *  audioplugin.cpp  -  base class for plugin to play an audio file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "audioplugin.h"

/******************************************************************************
* Return the AudioPlayer::Type corresponding to a SoundCategory.
*/
AudioPlayer::Type AudioPlugin::playerType(SoundCategory cat)
{
    switch (cat)
    {
        case Sample:  return AudioPlayer::Sample;
        case Alarm:
        default:      return AudioPlayer::Alarm;
    }
}

/******************************************************************************
* Return the player status.
*/
AudioPlugin::Status AudioPlugin::pluginStatus(AudioPlayer::Status playerStatus)
{
    switch (playerStatus)
    {
        case AudioPlayer::Ready:    return Ready;
        case AudioPlayer::Playing:  return Playing;
        case AudioPlayer::Error:
        default:                    return Error;
    }
}

#include "moc_audioplugin.cpp"

// vim: et sw=4:
