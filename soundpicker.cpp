/*
 *  soundpicker.cpp  -  widget to select a sound file or a beep
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

#include "kalarm.h"

#include <qlayout.h>
#include <qregexp.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

#include <kglobal.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kstandarddirs.h>
#include <kiconloader.h>
#ifndef WITHOUT_ARTS
#include <arts/kplayobjectfactory.h>
#endif

#include "checkbox.h"
#include "pushbutton.h"
#include "slider.h"
#include "soundpicker.moc"


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString SoundPicker::i18n_Sound()       { return i18n("An audio sound", "Sound"); }
QString SoundPicker::i18n_s_Sound()     { return i18n("An audio sound", "&Sound"); }
QString SoundPicker::i18n_SetVolume()   { return i18n("Set volume"); }
QString SoundPicker::i18n_v_SetVolume() { return i18n("Set &volume"); }
QString SoundPicker::i18n_Repeat()      { return i18n("Repeat"); }
QString SoundPicker::i18n_p_Repeat()    { return i18n("Re&peat"); }


SoundPicker::SoundPicker(QWidget* parent, const char* name)
	: QFrame(parent, name)
{
	// Sound checkbox
	setFrameStyle(QFrame::NoFrame);
	QHBoxLayout* soundLayout = new QHBoxLayout(this, 0, 2*KDialog::spacingHint());
	soundLayout->setAlignment(Qt::AlignVCenter);
	mCheckbox = new CheckBox(i18n_s_Sound(), this);
	mCheckbox->setFixedSize(mCheckbox->sizeHint());
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
	connect(mFilePicker, SIGNAL(clicked()), SLOT(slotPickFile()));
	QWhatsThis::add(mFilePicker,
	      i18n("Select a sound file to play when the message is displayed. If no sound file is "
	           "selected, a beep will sound."));
	soundLayout->addWidget(mFilePicker);

	// Sound repetition checkbox
	mRepeatCheckbox = new CheckBox(i18n_p_Repeat(), this);
	mRepeatCheckbox->setFixedSize(mRepeatCheckbox->sizeHint());
	QWhatsThis::add(mRepeatCheckbox,
	      i18n("If checked, the sound file will be played repeatedly for as long as the message is displayed."));
	soundLayout->addWidget(mRepeatCheckbox);

	// Set volume checkbox
	mVolumeCheckbox = new CheckBox(i18n_v_SetVolume(), this);
	mVolumeCheckbox->setFixedSize(mVolumeCheckbox->sizeHint());
	connect(mVolumeCheckbox, SIGNAL(toggled(bool)), SLOT(slotVolumeToggled(bool)));
	QWhatsThis::add(mVolumeCheckbox,
	      i18n("Select to choose the volume for playing the sound file."));
	soundLayout->addWidget(mVolumeCheckbox);

	// Volume slider
	mVolumeSlider = new Slider(0, 100, 10, 0, Qt::Horizontal, this);
	mVolumeSlider->setFixedSize(mVolumeSlider->sizeHint());
	QWhatsThis::add(mVolumeSlider,
	      i18n("Choose the volume for playing the sound file."));
	mVolumeCheckbox->setFocusWidget(mVolumeSlider);
	soundLayout->addWidget(mVolumeSlider);
	soundLayout->addStretch();

#ifdef WITHOUT_ARTS
	mRepeatCheckbox->hide();
	mVolumeCheckbox->hide();
	mVolumeSlider->hide();
#endif

	// Initialise the file picker button state and tooltip
	slotSoundToggled(false);
}

/******************************************************************************
 * Set the read-only status of the widget.
 */
void SoundPicker::setReadOnly(bool readOnly)
{
	mCheckbox->setReadOnly(readOnly);
	mFilePicker->setReadOnly(readOnly);
	mVolumeCheckbox->setReadOnly(readOnly);
	mRepeatCheckbox->setReadOnly(readOnly);
	mVolumeSlider->setReadOnly(readOnly);
}

/******************************************************************************
 * Return whether beep is selected.
 */
bool SoundPicker::beep() const
{
	return mCheckbox->isChecked()  &&  !mFilePicker->isOn();
}

/******************************************************************************
 * Return the selected sound file, if the main checkbox is checked.
 * Returns null string if beep is currently selected.
 */
QString SoundPicker::file() const
{
	return mCheckbox->isChecked() && mFilePicker->isOn() ? mFile : QString::null;
}

/******************************************************************************
 * Return the specified volume (range 0 - 1).
 * Returns < 0 if beep is currently selected, or if 'set volume' is not selected.
 */
float SoundPicker::volume() const
{
	return mVolumeSlider->isEnabled() ? (float)mVolumeSlider->value() / 100 : -1;
}

/******************************************************************************
 * Return whether sound file repetition is selected, if the main checkbox is checked.
 * Returns false if beep is currently selected.
 */
bool SoundPicker::repeat() const
{
	return mCheckbox->isChecked() && mFilePicker->isOn() && !mFile.isEmpty() && mRepeatCheckbox->isChecked();
}

/******************************************************************************
 * Return whether repetition is selected, regardless of the current beep and
 * file settings.
 */
bool SoundPicker::repeatSetting() const
{
	return mRepeatCheckbox->isChecked();
}

/******************************************************************************
 * Initialise the widget's state.
 */
void SoundPicker::set(bool beep, const QString& f, float volume, bool repeat)
{
	mCheckbox->setChecked(beep || !f.isEmpty());
	setFile(!beep  &&  !f.isEmpty(), f, volume, repeat);
}

/******************************************************************************
 * Set sound on or off.
 */
void SoundPicker::setChecked(bool on)
{
	mCheckbox->setChecked(on);
}

/******************************************************************************
 * Set the current beep status.
 */
void SoundPicker::setBeep(bool beep)
{
	mFilePicker->setOn(!beep);
	setFilePicker();
}

/******************************************************************************
 * Set the current sound file selection, volume and repetition status.
 */
void SoundPicker::setFile(bool on, const QString& f, float volume, bool repeat)
{
	mFile = f;
	mVolumeCheckbox->setChecked(volume >= 0);
	mVolumeSlider->setValue(volume >= 0 ? volume*100 : 100);
	mRepeatCheckbox->setChecked(repeat);
	mFilePicker->setOn(on);
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
 * Called when the Set Volume checkbox is toggled.
 */
void SoundPicker::slotVolumeToggled(bool on)
{
	mVolumeSlider->setEnabled(on && mVolumeCheckbox->isEnabled());
}

/******************************************************************************
 * Called when the file picker button is clicked.
 */
void SoundPicker::slotPickFile()
{
	if (mFilePicker->isOn())
	{
		KURL url = browseFile(mFile, mDefaultDir);
		if (!url.isEmpty())
		{
			mFile = url.prettyURL();
			mDefaultDir = url.path();
			setFilePicker();
		}
		else
			mFilePicker->setOn(false);
	}
	setFilePicker();
}

/******************************************************************************
 * Display a dialogue to choose a sound file, initially highlighting any
 * specified file. 'initialFile' must be a full path name or URL.
 * Reply = URL selected. If none is selected, URL.isEmpty() is true.
 */
KURL SoundPicker::browseFile(const QString& initialFile, const QString& defaultDir)
{
	QString initialDir = !initialFile.isEmpty() ? QString(initialFile).remove(QRegExp("/[^/]*$"))
	                   : !defaultDir.isEmpty()  ? defaultDir
	                   : KGlobal::dirs()->findResourceDir("sound", "KDE_Notify.wav");
#ifdef WITHOUT_ARTS
	KFileDialog fileDlg(initialDir, QString::fromLatin1("*.wav *.mp3 *.ogg|%1\n*|%2").arg(i18n("Sound Files")).arg(i18n("All Files")), 0, "soundFileDlg", true);
#else
	KFileDialog fileDlg(initialDir, KDE::PlayObjectFactory::mimeTypes().join(" "), 0, "soundFileDlg", true);
#endif
	fileDlg.setCaption(i18n("Choose Sound File"));
	fileDlg.setMode(KFile::File | KFile::ExistingOnly);
	if (!initialFile.isEmpty())
		fileDlg.setSelection(initialFile);
	if (fileDlg.exec() == QDialog::Accepted)
		return fileDlg.selectedURL();
	return KURL();
}

/******************************************************************************
 * Set the sound picker button according to whether a sound file is selected.
 */
void SoundPicker::setFilePicker()
{
	bool file = false;
	QToolTip::remove(mFilePicker);
	if (mFilePicker->isEnabled())
	{
		file = mFilePicker->isOn();
		if (mFilePicker->isOn())
			QToolTip::add(mFilePicker, i18n("Play '%1'").arg(mFile));
		else
			QToolTip::add(mFilePicker, i18n("Beep"));
	}
	mVolumeCheckbox->setEnabled(file);
	mRepeatCheckbox->setEnabled(file);
	mVolumeSlider->setEnabled(file && mVolumeCheckbox->isChecked());
}
