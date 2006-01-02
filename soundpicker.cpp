/*
 *  soundpicker.cpp  -  widget to select a sound file or a beep
 *  Program:  kalarm
 *  Copyright (c) 2002, 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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

#include "kalarm.h"

#include <QTimer>
#include <QHBoxLayout>

#include <kglobal.h>
#include <klocale.h>
#include <kfiledialog.h>
#include <kstandarddirs.h>
#include <kiconloader.h>
#include <khbox.h>
#ifndef WITHOUT_ARTS
#include <arts/kplayobjectfactory.h>
#endif
#include <kdebug.h>

#include "buttongroup.h"
#include "checkbox.h"
#include "functions.h"
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


SoundPicker::SoundPicker(QWidget* parent)
	: QFrame(parent),
	  mRevertType(false)
{
	// Sound checkbox
	QHBoxLayout* soundLayout = new QHBoxLayout(this);
	soundLayout->setMargin(0);
	soundLayout->setSpacing(2*KDialog::spacingHint());
	soundLayout->setAlignment(Qt::AlignVCenter);
	mCheckbox = new CheckBox(i18n_s_Sound(), this);
	mCheckbox->setFixedSize(mCheckbox->sizeHint());
	connect(mCheckbox, SIGNAL(toggled(bool)), SLOT(slotSoundToggled(bool)));
	mCheckbox->setWhatsThis(i18n("Check to enable sound when the message is displayed. Select the type of sound from the displayed options."));
	soundLayout->addWidget(mCheckbox);

	// Sound type
	mTypeGroup = new ButtonGroup(this);
	connect(mTypeGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotTypeChanged(QAbstractButton*)));

	// Beep radio button
	mBeepRadio = new RadioButton(i18n_Beep(), this);
	mBeepRadio->setFixedSize(mBeepRadio->sizeHint());
	mBeepRadio->setWhatsThis(i18n("If checked, a beep will sound when the alarm is displayed."));
	mTypeGroup->addButton(mBeepRadio);
	soundLayout->addWidget(mBeepRadio);

	// File radio button
	KHBox* box = new KHBox(this);
	box->setMargin(0);
	mFileRadio = new RadioButton(i18n_File(), box);
	mFileRadio->setFixedSize(mFileRadio->sizeHint());
	mFileRadio->setWhatsThis(i18n("If checked, a sound file will be played when the alarm is displayed."));
	mTypeGroup->addButton(mFileRadio);

	// Sound file picker button
	mFilePicker = new PushButton(box);
	mFilePicker->setIcon(SmallIcon("playsound"));
	mFilePicker->setFixedSize(mFilePicker->sizeHint());
	connect(mFilePicker, SIGNAL(clicked()), SLOT(slotPickFile()));
	mFilePicker->setWhatsThis(i18n("Configure a sound file to play when the alarm is displayed."));
	box->setFixedSize(box->sizeHint());
	soundLayout->addWidget(box);
	box->setFocusProxy(mFileRadio);

	// Speak radio button
	mSpeakRadio = new RadioButton(i18n_p_Speak(), this);
	mSpeakRadio->setFixedSize(mSpeakRadio->sizeHint());
	mSpeakRadio->setWhatsThis(i18n("If checked, the message will be spoken when the alarm is displayed."));
	mTypeGroup->addButton(mSpeakRadio);
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
		if (mSpeakRadio->isChecked())
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
	QAbstractButton* button = mTypeGroup->checkedButton();
	return (button == mSpeakRadio) ? SPEAK : (button == mFileRadio) ? PLAY_FILE : BEEP;
}

/******************************************************************************
* Return whether beep is selected.
*/
bool SoundPicker::beep() const
{
	return mCheckbox->isChecked()  &&  mBeepRadio->isChecked();
}

/******************************************************************************
* Return whether speech is selected.
*/
bool SoundPicker::speak() const
{
	return mCheckbox->isChecked()  &&  !mSpeakRadio->isHidden()  &&  mSpeakRadio->isChecked();
}

/******************************************************************************
* Return the selected sound file, if the main checkbox is checked.
* Returns null string if beep is currently selected.
*/
QString SoundPicker::file() const
{
	return mCheckbox->isChecked() && mFileRadio->isChecked() ? mFile : QString();
}

/******************************************************************************
* Return the specified volumes (range 0 - 1).
* Returns < 0 if beep is currently selected, or if 'set volume' is not selected.
*/
float SoundPicker::volume(float& fadeVolume, int& fadeSeconds) const
{
	if (mCheckbox->isChecked() && mFileRadio->isChecked() && !mFile.isEmpty())
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
	return mCheckbox->isChecked() && mFileRadio->isChecked() && !mFile.isEmpty() && mRepeat;
}

/******************************************************************************
* Initialise the widget's state.
*/
void SoundPicker::set(bool sound, SoundPicker::Type defaultType, const QString& f, float volume, float fadeVolume, int fadeSeconds, bool repeat)
{
	if (defaultType == PLAY_FILE  &&  f.isEmpty())
		defaultType = BEEP;
	mLastButton  = 0;
	mFile        = f;
	mVolume      = volume;
	mFadeVolume  = fadeVolume;
	mFadeSeconds = fadeSeconds;
	mRepeat      = repeat;
	mFilePicker->setToolTip(mFile);
	mCheckbox->setChecked(sound);
	RadioButton* button;
	switch (defaultType)
	{
		case PLAY_FILE:  button = mFileRadio;  break;
		case SPEAK:      button = mSpeakRadio; break;
		case BEEP:
		default:         button = mBeepRadio;  break;
	}
	button->setChecked(true);
}

/******************************************************************************
* Called when the sound checkbox is toggled.
*/
void SoundPicker::slotSoundToggled(bool on)
{
	mBeepRadio->setEnabled(on);
	mSpeakRadio->setEnabled(on);
	mFileRadio->setEnabled(on);
	mFilePicker->setEnabled(on && mFileRadio->isChecked());
	if (on  &&  mSpeakRadio->isHidden()  &&  mSpeakRadio->isChecked())
		mBeepRadio->setChecked(true);
	if (on)
		mBeepRadio->setFocus();
}

/******************************************************************************
* Called when the sound option is changed.
*/
void SoundPicker::slotTypeChanged(QAbstractButton* button)
{
	if (button == mLastButton  ||  mRevertType)
		return;
	if (mLastButton == mFileRadio)
		mFilePicker->setEnabled(false);
	else if (button == mFileRadio)
	{
		if (mFile.isEmpty())
		{
			slotPickFile();
			if (mFile.isEmpty())
				return;    // revert to previously selected type
		}
		mFilePicker->setEnabled(mCheckbox->isChecked());
	}
	mLastButton = static_cast<RadioButton*>(button);
}

/******************************************************************************
* Called when the file picker button is clicked.
*/
void SoundPicker::slotPickFile()
{
#ifdef WITHOUT_ARTS
	QString url = browseFile(mDefaultDir, mFile);
	if (!url.isEmpty())
		mFile = url;
#else
	QString file = mFile;
	SoundDlg dlg(mFile, mVolume, mFadeVolume, mFadeSeconds, mRepeat, i18n("Sound File"), this, "soundDlg");
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
	mFilePicker->setToolTip(mFile);
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
	mLastButton->setChecked(true);
	mRevertType = false;
}

/******************************************************************************
* Display a dialogue to choose a sound file, initially highlighting any
* specified file. 'initialFile' must be a full path name or URL.
* 'defaultDir' is updated to the directory containing the chosen file.
* Reply = URL selected. If none is selected, URL.isEmpty() is true.
*/
QString SoundPicker::browseFile(QString& defaultDir, const QString& initialFile)
{
	static QString kdeSoundDir;     // directory containing KDE sound files
	if (defaultDir.isEmpty())
	{
		if (kdeSoundDir.isNull())
			kdeSoundDir = KGlobal::dirs()->findResourceDir("sound", "KDE_Notify.wav");
		defaultDir = kdeSoundDir;
	}
#ifdef WITHOUT_ARTS
	QString filter = QString::fromLatin1("*.wav *.mp3 *.ogg|%1\n*|%2").arg(i18n("Sound Files")).arg(i18n("All Files"));
#else
	QString filter = KDE::PlayObjectFactory::mimeTypes().join(" ");
#endif
	return KAlarm::browseFile(i18n("Choose Sound File"), defaultDir, initialFile, filter, KFile::ExistingOnly, 0, "pickSoundFile");
}
