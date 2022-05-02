/*
 *  messagedisplayhelper_p.h  -  private declarations for MessageDisplayHelper
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <phonon/phononnamespace.h>
#include <phonon/Path>
#include <QObject>
#include <QMutex>

class MessageDisplayHelper;

namespace Phonon
{
    class MediaObject;
    class VolumeFaderEffect;
}

// Class to play an audio file, optionally repeated.
class AudioPlayer : public QObject
{
    Q_OBJECT
public:
    AudioPlayer(const QString& audioFile, float volume, float fadeVolume, int fadeSeconds, int repeatPause);
    ~AudioPlayer() override;
    void    execute();
    QString error() const;

public Q_SLOTS:
    void    stop();

Q_SIGNALS:
    void    readyToPlay();

private Q_SLOTS:
    void    checkAudioPlay();
    void    playStateChanged(Phonon::State);

private:
    void    removeEffects();

    mutable QMutex       mMutex;
    QString              mFile;
    float                mVolume;
    float                mFadeVolume;
    int                  mFadeSeconds;
    int                  mRepeatPause;
    Phonon::MediaObject* mAudioObject {nullptr};
    Phonon::Path         mPath;
    Phonon::VolumeFaderEffect* mFader {nullptr};
    QString              mError;
    bool                 mPlayedOnce;   // the sound file has started playing at least once
    bool                 mPausing;      // currently pausing between repeats
    bool                 mStopping {false};  // the player is about to be deleted
};

// vim: et sw=4:
