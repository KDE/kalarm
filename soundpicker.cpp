/*
 *  soundpicker.cpp  -  widget to select a sound file or a beep
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
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

#include <kglobal.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kstandarddirs.h>
#include <kiconloader.h>

#include "checkbox.h"
#include "pushbutton.h"
#include "soundpicker.moc"


SoundPicker::SoundPicker(bool readOnly, QWidget* parent, const char* name)
	: QFrame(parent, name)
{
	// Sound checkbox
	setFrameStyle(QFrame::NoFrame);
	QHBoxLayout* soundLayout = new QHBoxLayout(this, 0, KDialog::spacingHint());
	mCheckbox = new CheckBox(i18n("&Sound"), this);
	mCheckbox->setFixedSize(mCheckbox->sizeHint());
	mCheckbox->setReadOnly(readOnly);
	connect(mCheckbox, SIGNAL(toggled(bool)), SLOT(slotSoundToggled(bool)));
	QWhatsThis::add(mCheckbox,
	      i18n("If checked, a sound will be played when the message is displayed. Click the "
	           "button on the right to select the sound."));
	soundLayout->addWidget(mCheckbox);

	// Sound file picker button
	mFilePicker = new PushButton(this);
	mFilePicker->setPixmap(SmallIcon("playsound"));
	mFilePicker->setFixedSize(mFilePicker->sizeHint());
	mFilePicker->setToggleButton(true);
	mFilePicker->setReadOnly(readOnly);
	connect(mFilePicker, SIGNAL(clicked()), SLOT(slotPickFile()));
	QWhatsThis::add(mFilePicker,
	      i18n("Select a sound file to play when the message is displayed. If no sound file is "
	           "selected, a beep will sound."));
	soundLayout->addWidget(mFilePicker);
	soundLayout->addStretch();

	// Initialise the file picker button state and tooltip
	slotSoundToggled(false);
}

/******************************************************************************
 * Return whether beep is selected.
 */
bool SoundPicker::beep() const
{
	return mCheckbox->isChecked() && mFile.isEmpty();
}

/******************************************************************************
 * Return the selected sound file, if the checkbox is checked.
 */
QString SoundPicker::file() const
{
	return mCheckbox->isChecked() ? mFile : QString();
}

/******************************************************************************
 * Set sound on or off.
 */
void SoundPicker::setChecked(bool on)
{
	mCheckbox->setChecked(on);
}

/******************************************************************************
 * Set the current sound file selection.
 */
void SoundPicker::setFile(const QString& f)
{
	mFile = f;
	setFilePicker();
}

/******************************************************************************
 * Called when the sound checkbox is toggled.
 */
void SoundPicker::slotSoundToggled(bool on)
{
	mFilePicker->setEnabled(on);
	setFilePicker();
}

/******************************************************************************
 * Called when the file picker button is clicked.
 */
void SoundPicker::slotPickFile()
{
	if (mFilePicker->isOn())
	{
		if (mDefaultDir.isEmpty())
			mDefaultDir = KGlobal::dirs()->findResourceDir("sound", "KDE_Notify.wav");
		KURL url = KFileDialog::getOpenURL(mDefaultDir, i18n("*.wav|Wav Files"), 0, i18n("Choose a Sound File"));
		if (!url.isEmpty())
		{
			mFile = url.prettyURL();
			mDefaultDir = url.path();
			setFilePicker();
		}
		else if (mFile.isEmpty())
			mFilePicker->setOn(false);
	}
	else
	{
		mFile = "";
		setFilePicker();
	}
}

/******************************************************************************
 * Set the sound picker button according to whether a sound file is selected.
 */
void SoundPicker::setFilePicker()
{
	QToolTip::remove(mFilePicker);
	if (mFilePicker->isEnabled())
	{
		bool beep = mFile.isEmpty();
		if (beep)
			QToolTip::add(mFilePicker, i18n("Beep"));
		else
			QToolTip::add(mFilePicker, i18n("Play '%1'").arg(mFile));
		mFilePicker->setOn(!beep);
	}
}
