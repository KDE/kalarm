/*
 *  messagedisplayhelper_p.h  -  private declarations for MessageDisplayHelper
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009-2013 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <phonon/phononnamespace.h>
#include <phonon/path.h>
#include <QThread>
#include <QMutex>

class MessageDisplayHelper;

namespace Phonon { class MediaObject; }

class AudioThread : public QThread
{
        Q_OBJECT
    public:
        AudioThread(MessageDisplayHelper* parent, const QString& audioFile, float volume, float fadeVolume, int fadeSeconds, int repeatPause);
        ~AudioThread() override;
        void    stop(bool wait = false);
        QString error() const;

        static MessageDisplayHelper* mAudioOwner;    // window which owns the unique AudioThread

    Q_SIGNALS:
        void    readyToPlay();

    protected:
        void    run() override;

    private Q_SLOTS:
        void    checkAudioPlay();
        void    playStateChanged(Phonon::State);
        void    stopPlay();

    private:
        mutable QMutex       mMutex;
        QString              mFile;
        float                mVolume;
        float                mFadeVolume;
        int                  mFadeSeconds;
        int                  mRepeatPause;
        Phonon::MediaObject* mAudioObject;
        Phonon::Path         mPath;
        QString              mError;
        bool                 mPlayedOnce;   // the sound file has started playing at least once
        bool                 mPausing;      // currently pausing between repeats
};


// vim: et sw=4:
