/*
 *  soundpicker.h  -  widget to select a sound file or a beep
 *  Program:  kalarm
 *  (C) 2002, 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef SOUNDPICKER_H
#define SOUNDPICKER_H

#include <qframe.h>
#include <qstring.h>
#include <kurl.h>

class PushButton;
class CheckBox;


class SoundPicker : public QFrame
{
		Q_OBJECT
	public:
		SoundPicker(QWidget* parent, const char* name = 0);
		void           set(bool beep, const QString& filename, float volume, float fadeVolume, int fadeSeconds, bool repeat);
		void           setChecked(bool);
		void           setBeep(bool beep = true);
		void           setFile(const QString& f, float volume, float fadeVolume, int fadeSeconds, bool repeat)
		                                    { setFile(true, f, volume, fadeVolume, fadeSeconds, repeat); }
		void           setReadOnly(bool readOnly);
		bool           beep() const;
		QString        file() const;           // returns empty string if beep is selected
		float          volume(float& fadeVolume, int& fadeSeconds) const;   // returns < 0 if beep is selected or set-volume is not selected
		bool           repeat() const;         // returns false if beep is selected
		QString        fileSetting() const   { return mFile; }    // returns current file name regardless of beep setting
		bool           repeatSetting() const { return mRepeat; }  // returns current repeat status regardless of beep setting
		static KURL    browseFile(const QString& initialFile = QString::null, const QString& initialDir = QString::null);

		static QString i18n_Sound();       // plain text of Sound checkbox
		static QString i18n_s_Sound();     // text of Sound checkbox, with 'S' shortcut

	protected slots:
		void           slotSoundToggled(bool on);
		void           slotPickFile();

	private:
		void           setFile(bool on, const QString&, float volume, float fadeVolume, int fadeSeconds, bool repeat);
		void           setFilePicker();

		CheckBox*      mCheckbox;
		PushButton*    mFilePicker;
		QString        mDefaultDir;
		QString        mFile;         // sound file to play when alarm is triggered
		float          mVolume;       // volume for file, or < 0 to not set volume
		float          mFadeVolume;   // initial volume for file, or < 0 for no fading
		int            mFadeSeconds;  // fade interval in seconds
		bool           mRepeat;       // repeat the sound file
		bool           mReadOnly;
};

#endif // SOUNDPICKER_H
