/*
 *  audioplayer.h  -  play an audio file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

class QTimer;

// Class to play an audio file, optionally repeated.
class AudioPlayer : public QObject
{
    Q_OBJECT
public:
    enum Type { Alarm, Sample };
    enum Status
    {
        Ready,           //!< Ready to play (newly initialised, or finished playing)
        Playing,         //!< Currently playing
        Error            //!< Something has gone wrong
    };

    static AudioPlayer* create(Type, const QUrl& audioFile, QObject* parent = nullptr);
    static AudioPlayer* create(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    ~AudioPlayer() override;
    Status  status() const;
    static QString popError();   // fetch last error message, and clear it
    static bool providesFade();

public Q_SLOTS:
    virtual bool play() = 0;
    virtual void stop() = 0;

Q_SIGNALS:
    void finished(bool ok);

private Q_SLOTS:
    void fadeStep();

protected:
    AudioPlayer(Type, const QUrl& audioFile, QObject* parent = nullptr);
    AudioPlayer(Type, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr);
    void resetFade();
    virtual void internalSetVolume() = 0;

    /** Set the status to a non-error value. */
    void setOkStatus(Status status);

    /** Set the status to Error, and set the error message to display to the user. */
    void setErrorStatus(const QString& errorMessage);

    static AudioPlayer* mInstance;
    QString             mFile;
    float               mVolume;        // configured end volume
    float               mFadeVolume;    // configured start volume
    float               mFadeStep;
    float               mCurrentVolume;
    QTimer*             mFadeTimer {nullptr};
    time_t              mFadeStart {0};
    int                 mFadeSeconds;   // configured time to fade from mFadeVolume to mVolume
    bool                mNoFinishedSignal {false};

private:
    static QString      mError;
    Status              mStatus {Error};
};


// vim: et sw=4:
