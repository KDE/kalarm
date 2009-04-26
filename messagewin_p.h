/*
 *  messagewin_p.h  -  private declarations for MessageWin
 *  Program:  kalarm
 *  Copyright © 2009 by David Jarvie <djarvie@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef MESSAGEWIN_P_H
#define MESSAGEWIN_P_H

#include <phonon/phononnamespace.h>
#include <phonon/path.h>
#include <QThread>
#include <QMutex>

namespace Phonon { class MediaObject; }

class AudioThread : public QThread
{
		Q_OBJECT
	public:
		AudioThread(QObject* parent, const QString& audioFile, float volume, float fadeVolume, int fadeSeconds, bool repeat)
			: QThread(parent),
			  mFile(audioFile),
			  mVolume(volume),
			  mFadeVolume(fadeVolume),
			  mFadeSeconds(fadeSeconds),
			  mRepeat(repeat),
			  mAudioObject(0)
		{}
		~AudioThread();
		QString error() const;

	signals:
		void    readyToPlay();

	protected:
		virtual void run();

	private slots:
		void    checkAudioPlay();
		void    playStateChanged(Phonon::State);
		void    stopPlay();

	private:
		mutable QMutex       mMutex;
		QString              mFile;
		float                mVolume;
		float                mFadeVolume;
		int                  mFadeSeconds;
		bool                 mRepeat;
		Phonon::MediaObject* mAudioObject;
		Phonon::Path         mPath;
		QString              mError;
		bool                 mPlayedOnce;      // the sound file has started playing at least once
};

#endif // MESSAGEWIN_P_H
