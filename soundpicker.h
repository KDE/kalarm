/*
 *  soundpicker.h  -  widget to select a sound file or a beep
 *  Program:  kalarm
 *  (C) 2002, 2004 by David Jarvie <software@astrojar.org.uk>
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
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
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
		void           set(bool beep, const QString& filename, bool repeat);
		void           setChecked(bool);
		void           setBeep(bool beep = true);
		void           setFile(const QString&, bool repeat);
		void           setReadOnly(bool readOnly);
		bool           beep() const;
		QString        file() const;          // returns empty string if beep is selected
		bool           repeat() const;        // returns false if beep is selected
		QString        fileSetting() const  { return mFile; }   // returns current file name regardless of beep setting
		bool           repeatSetting() const; // returns current repeat status regardless of beep setting
		static KURL    browseFile(const QString& initialFile = QString::null, const QString& initialDir = QString::null);

		static const QString i18n_Sound;     // plain text of Sound checkbox
		static const QString i18n_s_Sound;   // text of Sound checkbox, with 'S' shortcut
		static const QString i18n_Repeat;    // plain text of Repeat checkbox
		static const QString i18n_p_Repeat;  // text of Repeat checkbox, with 'P' shortcut

	protected slots:
		void           slotSoundToggled(bool on);
		void           slotPickFile();

	private:
		void           setFilePicker();

		CheckBox*      mCheckbox;
		CheckBox*      mRepeatCheckbox;
		PushButton*    mFilePicker;
		QString        mDefaultDir;
		QString        mFile;        // sound file to play when alarm is triggered
};

#endif // SOUNDPICKER_H
