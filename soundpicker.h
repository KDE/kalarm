/*
 *  soundpicker.h  -  widget to select a sound file or a beep
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
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

class PushButton;
class CheckBox;


class SoundPicker : public QFrame
{
		Q_OBJECT
	public:
		SoundPicker(bool readOnly, QWidget* parent, const char* name = 0);
		void           setFile(const QString&);
		void           setChecked(bool);
		bool           beep() const;
		QString        file() const;

	protected slots:
		void           slotSoundToggled(bool on);
		void           slotPickFile();

	private:
		void           setFilePicker();

		CheckBox*      mCheckbox;
		PushButton*    mFilePicker;
		QString        mDefaultDir;
		QString        mFile;        // sound file to play when alarm is triggered, or null for beep
};

#endif // SOUNDPICKER_H
