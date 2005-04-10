/*
 *  soundpicker.cpp  -  widget to select a sound file or a beep
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

#include "kalarm.h"

#include <qlayout.h>
#include <qregexp.h>
#include <qtooltip.h>
#include <qtimer.h>
#include <qhbox.h>
#include <qwhatsthis.h>

#include <kglobal.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kstandarddirs.h>
#include <kiconloader.h>
#ifndef WITHOUT_ARTS
#include <arts/kplayobjectfactory.h>
#endif
#include <kdebug.h>

#include "buttongroup.h"
#include "checkbox.h"
#include "kalarmapp.h"
#include "pushbutton.h"
#include "radiobutton.h"
#include "sounddlg.h"
#include "soundpicker.moc"


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString SoundPicker::i18n_Sound()       { return i18n("An audio sound", "Sound"); }
QString SoundPicker::i18n_s_Sound()     { return i18n("An audio sound", "&Sound"); }
QString SoundPicker::i18n_Beep()        { return i18n("Beep"); }
QString SoundPicker::i18n_b_Beep()      { return i18n("&Beep"); }
QString SoundPicker::i18n_Speak()       { return i18n("Speak"); }
QString SoundPicker::i18n_p_Speak()     { return i18n("S&peak"); }
QString SoundPicker::i18n_File()        { return i18n("File"); }


SoundPicker::SoundPicker(QWidget* parent, const char* name)
	: QFrame(parent, name),
	  mRevertType(false)
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

	// Sound type
	mTypeGroup = new ButtonGroup(this);
	mTypeGroup->hide();
	connect(mTypeGroup, SIGNAL(buttonSet(int)), SLOT(slotTypeChanged(int)));

	// Beep radio button
	mBeepRadio = new RadioButton(i18n_Beep(), this, "beepButton");
	mBeepRadio->setFixedSize(mBeepRadio->sizeHint());
	QWhatsThis::add(mBeepRadio, i18n("If checked, a beep will sound when the alarm is displayed."));
	mTypeGroup->insert(mBeepRadio, BEEP);
	soundLayout->addWidget(mBeepRadio);

	// File radio button
	QHBox* box = new QHBox(this);
	mFileRadio = new RadioButton(i18n_File(), box, "audioFileButton");
	mFileRadio->setFixedSize(mFileRadio->sizeHint());
	QWhatsThis::add(mFileRadio, i18n("If checked, a sound file will be played when the alarm is displayed."));
	mTypeGroup->insert(mFileRadio, PLAY_FILE);

	// Sound file picker button
	mFilePicker = new PushButton(box);
	mFilePicker->setPixmap(SmallIcon("playsound"));
	mFilePicker->setFixedSize(mFilePicker->sizeHint());
	connect(mFilePicker, SIGNAL(clicked()), SLOT(slotPickFile()));
	QWhatsThis::add(mFilePicker, i18n("Configure a sound file to play when the alarm is displayed."));
	box->setFixedSize(box->sizeHint());
	soundLayout->addWidget(box);
	box->setFocusProxy(mFileRadio);

	// Speak radio button
	mSpeakRadio = new RadioButton(i18n_p_Speak(), this, "speakButton");
	mSpeakRadio->setFixedSize(mSpeakRadio->sizeHint());
	QWhatsThis::add(mSpeakRadio, i18n("If checked, the message will be spoken when the alarm is displayed."));
	mTypeGroup->insert(mSpeakRadio, SPEAK);
	soundLayout->addWidget(mSpeakRadio);

	if (!theApp()->speechEnabled())
		mSpeakRadio->hide();     // speech capability is not installed

	setTabOrder(mCheckbox, mBeepRadio);
	setTabOrder(mBeepRadio, mFileRadio);
	setTabOrder(mFileRadio, mFilePicker);
	setTabOrder(mFilePicker, mSpeakRadio);

	// Initialise the file picker button state and tooltip
	slotSoundToggled(false);
}

/******************************************************************************
* Set the read-only status of the widget.
*/
void SoundPicker::setReadOnly(bool readOnly)
{
	mCheckbox->setReadOnly(readOnly);
	mBeepRadio->setReadOnly(readOnly);
	mFileRadio->setReadOnly(readOnly);
#ifdef WITHOUT_ARTS
	mFilePicker->setReadOnly(readOnly);
#endif
	mSpeakRadio->setReadOnly(readOnly);
	mReadOnly = readOnly;
}

/******************************************************************************
* Show or hide the Speak option.
*/
void SoundPicker::showSpeak(bool show)
{
	if (!theApp()->speechEnabled())
		return;     // speech capability is not installed

	bool shown = !mSpeakRadio->isHidden();
	if (show  &&  !shown)
		mSpeakRadio->show();
	else if (!show  &&  shown)
	{
		if (mSpeakRadio->isOn())
			mCheckbox->setChecked(false);
		mSpeakRadio->hide();
	}
}

/******************************************************************************
* Return whether sound is selected.
*/
bool SoundPicker::sound() const
{
	return mCheckbox->isChecked();
}

/******************************************************************************
* Return the currently selected option.
*/
SoundPicker::Type SoundPicker::type() const
{
	return static_cast<SoundPicker::Type>(mTypeGroup->selectedId());
}

/******************************************************************************
* Return whether beep is selected.
*/
bool SoundPicker::beep() const
{
	return mCheckbox->isChecked()  &&  mBeepRadio->isOn();
}

/******************************************************************************
* Return whether speech is selected.
*/
bool SoundPicker::speak() const
{
	return mCheckbox->isChecked()  &&  !mSpeakRadio->isHidden()  &&  mSpeakRadio->isOn();
}

/******************************************************************************
* Return the selected sound file, if the main checkbox is checked.
* Returns null string if beep is currently selected.
*/
QString SoundPicker::file() const
{
	return mCheckbox->isChecked() && mFileRadio->isOn() ? mFile : QString::null;
}

/******************************************************************************
* Return the specified volumes (range 0 - 1).
* Returns < 0 if beep is currently selected, or if 'set volume' is not selected.
*/
float SoundPicker::volume(float& fadeVolume, int& fadeSeconds) const
{
	if (mCheckbox->isChecked() && mFileRadio->isOn() && !mFile.isEmpty())
	{
		fadeVolume  = mFadeVolume;
		fadeSeconds = mFadeSeconds;
		return mVolume;
	}
	else
	{
		fadeVolume  = -1;
		fadeSeconds = 0;
		return -1;
	}
}

/******************************************************************************
* Return whether sound file repetition is selected, if the main checkbox is checked.
* Returns false if beep is currently selected.
*/
bool SoundPicker::repeat() const
{
	return mCheckbox->isChecked() && mFileRadio->isOn() && !mFile.isEmpty() && mRepeat;
}

/******************************************************************************
* Initialise the widget's state.
*/
void SoundPicker::set(bool sound, SoundPicker::Type defaultType, const QString& f, float volume, float fadeVolume, int fadeSeconds, bool repeat)
{
	if (defaultType == PLAY_FILE  &&  f.isEmpty())
		defaultType = BEEP;
	mLastType    = static_cast<Type>(0);
	mFile        = f;
	mVolume      = volume;
	mFadeVolume  = fadeVolume;
	mFadeSeconds = fadeSeconds;
	mRepeat      = repeat;
	QToolTip::add(mFilePicker, mFile);
	mCheckbox->setChecked(sound);
	mTypeGroup->setButton(defaultType);
}

/******************************************************************************
* Called when the sound checkbox is toggled.
*/
void SoundPicker::slotSoundToggled(bool on)
{
	mBeepRadio->setEnabled(on);
	mSpeakRadio->setEnabled(on);
	mFileRadio->setEnabled(on);
	mFilePicker->setEnabled(on && mFileRadio->isOn());
	if (on  &&  mSpeakRadio->isHidden()  &&  mSpeakRadio->isOn())
		mBeepRadio->setChecked(true);
	if (on)
		mBeepRadio->setFocus();
}

/******************************************************************************
* Called when the sound option is changed.
*/
void SoundPicker::slotTypeChanged(int id)
{
	Type newType = static_cast<Type>(id);
	if (newType == mLastType  ||  mRevertType)
		return;
	if (mLastType == PLAY_FILE)
		mFilePicker->setEnabled(false);
	else if (newType == PLAY_FILE)
	{
		if (mFile.isEmpty())
		{
			slotPickFile();
			if (mFile.isEmpty())
				return;    // revert to previously selected type
		}
		mFilePicker->setEnabled(mCheckbox->isChecked());
	}
	mLastType = newType;
}

/******************************************************************************
* Called when the file picker button is clicked.
*/
void SoundPicker::slotPickFile()
{
#ifdef WITHOUT_ARTS
	KURL url = browseFile(mFile, mDefaultDir);
	if (!url.isEmpty())
	{
		mFile = url.prettyURL();
		mDefaultDir = url.path();
	}
#else
	QString file = mFile;
	SoundDlg dlg(mFile, mVolume, mFadeVolume, mFadeSeconds, mRepeat,
		     i18n("Sound File"), this, "soundDlg");
	dlg.setReadOnly(mReadOnly);
	bool accepted = (dlg.exec() == QDialog::Accepted);
	if (mReadOnly)
		return;
	if (accepted)
	{
		float volume, fadeVolume;
		int   fadeTime;
		file         = dlg.getFile();
		mRepeat      = dlg.getSettings(volume, fadeVolume, fadeTime);
		mVolume      = volume;
		mFadeVolume  = fadeVolume;
		mFadeSeconds = fadeTime;
	}
	if (!file.isEmpty())
	{
		mFile       = file;
		mDefaultDir = dlg.defaultDir();
	}
#endif
	QToolTip::add(mFilePicker, mFile);
	if (mFile.isEmpty())
	{
		// No audio file is selected, so revert to 'beep'.
		// But wait a moment until setting the radio button, or it won't work.
		mRevertType = true;   // prevent sound dialogue popping up twice
		QTimer::singleShot(0, this, SLOT(setLastType()));
	}
}

/******************************************************************************
* Select the previously selected sound type.
*/
void SoundPicker::setLastType()
{
	mTypeGroup->setButton(mLastType);
	mRevertType = false;
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
