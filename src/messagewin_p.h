/*
 *  messagewin_p.h  -  private declarations for MessageWin
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2009-2013 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MESSAGEWIN_P_H
#define MESSAGEWIN_P_H

#include <phonon/phononnamespace.h>
#include <phonon/path.h>
#include <QThread>
#include <QMutex>

class MessageWin;

namespace Phonon { class MediaObject; }

class AudioThread : public QThread
{
        Q_OBJECT
    public:
        AudioThread(MessageWin* parent, const QString& audioFile, float volume, float fadeVolume, int fadeSeconds, int repeatPause);
        ~AudioThread();
        void    stop(bool wait = false);
        QString error() const;

        static MessageWin*   mAudioOwner;    // window which owns the unique AudioThread

    Q_SIGNALS:
        void    readyToPlay();

    protected:
        void run() override;

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

#endif // MESSAGEWIN_P_H

// vim: et sw=4:
