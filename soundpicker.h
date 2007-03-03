/*
 *  soundpicker.h  -  widget to select a sound file or a beep
 *  Program:  kalarm
 *  Copyright © 2002,2004-2006 by David Jarvie <software@astrojar.org.uk>
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

#ifndef SOUNDPICKER_H
#define SOUNDPICKER_H

#include <qframe.h>
#include <qstring.h>
#include <kurl.h>

class QHBox;
class ComboBox;
class PushButton;


class SoundPicker : public QFrame
{
		Q_OBJECT
	public:
		/** Sound options which can be selected for when the alarm is displayed.
		 *  @li NONE      - silence.
		 *  @li BEEP      - a beep will be sounded.
		 *  @li PLAY_FILE - a sound file will be played.
		 *  @li SPEAK     - the message text will be spoken.
		 */
		enum Type { NONE = 0, BEEP, PLAY_FILE, SPEAK };
		/** Constructor.
		 *  @param parent The parent object of this widget.
		 *  @param name The name of this widget.
		 */
		explicit SoundPicker(QWidget* parent, const char* name = 0);
		/** Initialises the widget's state.
		 *  @param type     The option to select.
		 *  @param filename The full path or URL of the sound file to select. If the 'file' option is
		 *                  not initially selected, @p filename provides the default should 'file'
		 *                  later be selected by the user.
		 *  @param volume   The volume to play a sound file, or < 0 for no volume setting. If the
		 *                  'file' option is not initially selected, @p volume provides the default
		 *                  should 'file' later be selected by the user.
		 *  @param fadeVolume The initial volume to play a sound file if fading is to be used, or < 0
		 *                  for no fading. If the 'file' option is not initially selected, @p fadeVolume
		 *                  provides the default should 'file' later be selected by the user.
		 *  @param fadeSeconds The number of seconds over which the sound file volume should be faded,
		 *                  or 0 for no fading. If the 'file' option is not initially selected,
		 *                  @p fadeSeconds provides the default should 'file' later be selected by the user.
		 *  @param repeat   True to play the sound file repeatedly. If the 'file' option is not initially
		 *                  selected, @p repeat provides the default should 'file' later be selected by
		 *                  the user.
		 */
		void           set(Type type, const QString& filename, float volume, float fadeVolume, int fadeSeconds, bool repeat);
		/** Returns true if the widget is read only for the user. */
		bool           isReadOnly() const          { return mReadOnly; }
		/** Sets whether the widget can be changed the user.
		 *  @param readOnly True to set the widget read-only, false to set it read-write.
		 */
		void           setReadOnly(bool readOnly);
		/** Show or hide the 'speak' option.
		 *  If it is to be hidden and it is currently selected, sound is turned off.
		 */
		void           showSpeak(bool show);
		/** Returns the selected option. */
		Type           sound() const;
		/** If the 'file' option is selected, returns the URL of the chosen file.
		 *  Otherwise returns a null string.
		 */
		QString        file() const;
		/** Returns the volume and fade characteristics for playing a sound file.
		 *  @param fadeVolume Receives the initial volume if the volume is to be faded, else -1.
		 *  @param fadeSeconds Receives the number of seconds over which the volume is to be faded, else 0.
		 *  @return Volume to play the sound file, or < 0 if the 'file' option is not selected.
		 */
		float          volume(float& fadeVolume, int& fadeSeconds) const;
		/** Returns true if a sound file is to be played repeatedly.
		 *  If the 'file' option is not selected, returns false.
		 */
		bool           repeat() const;
		/** Returns the current file URL regardless of whether the 'file' option is selected. */
		QString        fileSetting() const   { return mFile; }
		/** Returns the current file repetition setting regardless of whether the 'file' option is selected. */
		bool           repeatSetting() const { return mRepeat; }
		/** Display a dialogue to choose a sound file, initially highlighting
		 *  @p initialFile if non-null.
		 *  @param initialDir  Initial directory to display if @p initialFile is null. If a file is
		 *                     chosen, this is updated to the directory containing the chosen file.
		 *  @param initialFile Full path name or URL of file to be highlighted initially.
		 *                     If null, no file will be highlighted.
		 *  @return URL selected, in human readable format. If none is selected, URL.isEmpty() is true.
		 */
		static QString browseFile(QString& initialDir, const QString& initialFile = QString::null);

		static QString i18n_Sound();       // plain text of Sound label
		static QString i18n_None();        // plain text of None combo box item
		static QString i18n_Beep();        // plain text of Beep combo box item
		static QString i18n_Speak();       // plain text of Speak combo box item
		static QString i18n_File();        // plain text of File combo box item


	private slots:
		void           slotTypeSelected(int id);
		void           slotPickFile();

	private:

		ComboBox*      mTypeCombo;
		QHBox*         mTypeBox;
		PushButton*    mFilePicker;
		QString        mDefaultDir;
		QString        mFile;         // sound file to play when alarm is triggered
		float          mVolume;       // volume for file, or < 0 to not set volume
		float          mFadeVolume;   // initial volume for file, or < 0 for no fading
		int            mFadeSeconds;  // fade interval in seconds
		Type           mLastType;     // last selected sound option
		bool           mSpeakShowing; // Speak option is shown in combo box
		bool           mRepeat;       // repeat the sound file
		bool           mReadOnly;
};

#endif // SOUNDPICKER_H
