/*
 *  sounddlg.h  -  sound file selection and configuration dialog
 *  Program:  kalarm
 *  Copyright (c) 2005-2007 by David Jarvie <software@astrojar.org.uk>
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

#ifndef SOUNDDLG_H
#define SOUNDDLG_H

#include <QString>
#include <kurl.h>
#include <kdialog.h>
#include <kmessagebox.h>

class QPushButton;
class QShowEvent;
class QResizeEvent;
class KHBox;
namespace Phonon { class AudioPlayer; }
class PushButton;
class CheckBox;
class SpinBox;
class Slider;
class LineEdit;


class SoundDlg : public KDialog
{
		Q_OBJECT
	public:
		SoundDlg(const QString& file, float volume, float fadeVolume, int fadeSeconds, bool repeat,
		         const QString& caption, QWidget* parent);
		~SoundDlg();
		void           setReadOnly(bool);
		bool           isReadOnly() const    { return mReadOnly; }
		KUrl           getFile() const       { return mUrl; }
		bool           getSettings(float& volume, float& fadeVolume, int& fadeSeconds) const;
		QString        defaultDir() const    { return mDefaultDir; }

		static QString i18n_SetVolume();   // plain text of Set volume checkbox
		static QString i18n_v_SetVolume(); // text of Set volume checkbox, with 'V' shortcut
		static QString i18n_Repeat();      // plain text of Repeat checkbox
		static QString i18n_p_Repeat();    // text of Repeat checkbox, with 'P' shortcut

	protected:
		virtual void   showEvent(QShowEvent*);
		virtual void   resizeEvent(QResizeEvent*);

	protected slots:
		virtual void   slotOk();

	private slots:
		void           slotPickFile();
		void           slotVolumeToggled(bool on);
		void           slotFadeToggled(bool on);
		void           playSound();
		void           playFinished();

	private:
		bool           checkFile();

		QPushButton*   mFilePlay;
		LineEdit*      mFileEdit;
		PushButton*    mFileBrowseButton;
		CheckBox*      mRepeatCheckbox;
		CheckBox*      mVolumeCheckbox;
		Slider*        mVolumeSlider;
		CheckBox*      mFadeCheckbox;
		KHBox*         mFadeBox;
		SpinBox*       mFadeTime;
		KHBox*         mFadeVolumeBox;
		Slider*        mFadeSlider;
		QString        mDefaultDir;     // current default directory for mFileEdit
		KUrl           mUrl;
		Phonon::AudioPlayer* mPlayer;
		bool           mReadOnly;
};

#endif
