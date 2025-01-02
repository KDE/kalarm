/*
 *  pluginbaseaudio.h  -  base class for plugin to provide an audio backend
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2025 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "audioplugin/audioplayer.h"
#include "kalarmpluginlib_export.h"

#include <QObject>
#include <QVariant>

class AudioPlayer;

class KALARMPLUGINLIB_EXPORT PluginBaseAudio : public QObject
{
    Q_OBJECT
public:
    enum SoundCategory { Alarm, Sample };
    enum Status
    {
        Uninitialised,   //!< No audio player has been created by createPlayer()
        Ready,           //!< Ready to play (newly initialised, or finished playing)
        Playing,         //!< Currently playing
        Error            //!< Something has gone wrong
    };

    explicit PluginBaseAudio(QObject* parent = nullptr, const QList<QVariant>& = {});
    ~PluginBaseAudio() override;

    QString name() const  { return mName; }

    /** Create a unique audio player using the plugin's backend.
     *  The player must be deleted when finished with by calling deletePlayer().
     *  @return true if a new audio player was created, false if error or a player already exists.
     */
    bool createPlayer(SoundCategory type, const QUrl& audioFile, QObject* parent = nullptr)
    { return createPlayer(type, audioFile, -1, -1, 0, parent); }

    /** Create a unique audio player using the plugin's backend.
     *  @return true if a new audio player was created, false if error or a player already exists.
     */
    virtual bool createPlayer(SoundCategory, const QUrl& audioFile, float volume, float fadeVolume, int fadeSeconds, QObject* parent = nullptr) = 0;

    /** Delete the plugin's audio player. */
    virtual void deletePlayer() = 0;

    /** Return whether the plugin provides volume fade. */
    virtual bool providesFade() const = 0;

    /** Fetch the last error message, and clear it. */
    virtual QString popError() const = 0;

    /** Return the play status. */
    virtual Status status() const = 0;

    /* Start playing the audio file. */
    virtual bool play() = 0;

public Q_SLOTS:
    /** Stop playing the audio file. */
    virtual void stop() = 0;

Q_SIGNALS:
    /** Emitted when play has finished. */
    void finished(bool ok);

protected:
    void setName(const QString& pluginName)  { mName = pluginName; }

private:
    QString mName;
};

// vim: et sw=4:
