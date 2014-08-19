/*
 *  soundpicker.h  -  widget to select a sound file or a beep
 *  Program:  kalarm
 *  Copyright © 2002,2004-2012 by David Jarvie <djarvie@kde.org>
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

#include "preferences.h"

#include <kurl.h>
#include <QFrame>
#include <QString>

class ComboBox;
class PushButton;


class SoundPicker : public QFrame
{
        Q_OBJECT
    public:
        /** Constructor.
         *  @param parent The parent object of this widget.
         */
        explicit SoundPicker(QWidget* parent);
        /** Initialises the widget's state.
         *  @param type     The option to select.
         *  @param filename The full path or URL of the sound file to select.
         *                  If the 'file' option is not initially selected,
         *                  @p filename provides the default should 'file'
         *                  later be selected by the user.
         *  @param volume   The volume to play a sound file, or < 0 for no
         *                  volume setting. If the 'file' option is not
         *                  initially selected, @p volume provides the default
         *                  should 'file' later be selected by the user.
         *  @param fadeVolume  The initial volume to play a sound file if fading
         *                     is to be used, or < 0 for no fading. If the
         *                     'file' option is not initially selected, @p
         *                     fadeVolume provides the default should 'file'
         *                     later be selected by the user.
         *  @param fadeSeconds The number of seconds over which the sound file
         *                     volume should be faded, or 0 for no fading. If
         *                     the 'file' option is not initially selected,
         *                     @p fadeSeconds provides the default should
         *                     'file' later be selected by the user.
         *  @param repeatPause Number of seconds to pause between sound file
         *                     repetitions, or -1 for no repetition. If the
         *                     'file' option is not initially selected,
         *                     @p repeatPause provides the default should 'file'
         *                     later be selected by the user.
         */
        void           set(Preferences::SoundType type, const QString& filename, float volume, float fadeVolume, int fadeSeconds, int repeatPause);

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
        Preferences::SoundType sound() const;

        /** If the 'file' option is selected, returns the URL of the chosen file.
         *  Otherwise returns an empty URL.
         */
        KUrl           file() const;

        /** Returns the volume and fade characteristics for playing a sound file.
         *  @param fadeVolume  Receives the initial volume if the volume is to
         *                     be faded, else -1.
         *  @param fadeSeconds Receives the number of seconds over which the
         *                     volume is to be faded, else 0.
         *  @return            Volume to play the sound file, or < 0 if the
         *                     'file' option is not selected.
         */
        float          volume(float& fadeVolume, int& fadeSeconds) const;

        /** Returns pause in seconds between repetitions of the sound file,
         *  or -1 if no repeat or 'file' option is not selected.
         */
        int            repeatPause() const;

        /** Returns the current file URL regardless of whether the 'file' option is selected. */
        KUrl           fileSetting() const   { return mFile; }

        /** Returns the current file repetition setting regardless of whether
         *  the 'file' option is selected.
         */
        bool           repeatPauseSetting() const { return mRepeatPause; }

        /** Display a dialog to choose a sound file, initially highlighting
         *  @p initialFile if non-null.
         *  @param initialDir  Initial directory to display if @p initialFile
         *                     is null. If a file is chosen, this is updated to
         *                     the directory containing the chosen file.
         *  @param initialFile Full path name or URL of file to be highlighted
         *                     initially.  If null, no file will be highlighted.
         *  @return            URL selected, in human readable format. If none
         *                     is selected, URL.isEmpty() is true.
         */
        static QString browseFile(QString& initialDir, const QString& initialFile = QString());

        static QString i18n_label_Sound();  // text of Sound label
        static QString i18n_combo_None();   // text of None combo box item
        static QString i18n_combo_Beep();   // text of Beep combo box item
        static QString i18n_combo_Speak();  // text of Speak combo box item
        static QString i18n_combo_File();   // text of File combo box item

    signals:
        void           changed();     // emitted when any contents change

    private slots:
        void           slotTypeSelected(int id);
        void           slotPickFile();
        void           setLastType();

    private:
        ComboBox*      mTypeCombo;
        QWidget*       mTypeBox;
        PushButton*    mFilePicker;
        KUrl           mFile;         // sound file to play when alarm is triggered
        float          mVolume;       // volume for file, or < 0 to not set volume
        float          mFadeVolume;   // initial volume for file, or < 0 for no fading
        int            mFadeSeconds;  // fade interval in seconds
        int            mRepeatPause;  // seconds to pause between repetitions of the sound file, or -1 if no repeat
        Preferences::SoundType mLastType;     // last selected sound option
        bool           mSpeakShowing; // Speak option is shown in combo box
        bool           mRevertType;   // reverting to last selected sound option
        bool           mReadOnly;
};

#endif // SOUNDPICKER_H

// vim: et sw=4:
