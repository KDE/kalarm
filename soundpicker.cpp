/*
 *  soundpicker.cpp  -  widget to select a sound file or a beep
 *  Program:  kalarm
 *  Copyright (c) 2002-2006 by David Jarvie <software@astrojar.org.uk>
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
#include <QLabel>
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

#include "combobox.h"
#include "functions.h"
#include "kalarmapp.h"
#include "pushbutton.h"
#include "sounddlg.h"
#include "soundpicker.moc"


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString SoundPicker::i18n_Sound()       { return i18nc("An audio sound", "Sound"); }
QString SoundPicker::i18n_None()        { return i18n("None"); }
QString SoundPicker::i18n_Beep()        { return i18nc("(verb)", "Beep"); }
QString SoundPicker::i18n_Speak()       { return i18n("Speak"); }
QString SoundPicker::i18n_File()        { return i18nc("Play an audio file", "Play file"); }


SoundPicker::SoundPicker(QWidget* parent)
	: QFrame(parent),
	  mRevertType(false)
{
	QHBoxLayout* soundLayout = new QHBoxLayout(this);
	soundLayout->setMargin(0);
	soundLayout->setSpacing(2*KDialog::spacingHint());
	mTypeBox = new KHBox(this);    // this is to control the QWhatsThis text display area
	mTypeBox->setMargin(0);
	mTypeBox->setSpacing(KDialog::spacingHint());

	QLabel* label = new QLabel(i18nc("An audio sound", "&Sound:"), mTypeBox);
	label->setFixedSize(label->sizeHint());

	// Sound type combo box
	// The order of combo box entries must correspond with the 'Type' enum.
	mTypeCombo = new ComboBox(mTypeBox);
	mTypeCombo->addItem(i18n_None());     // index NONE
	mTypeCombo->addItem(i18n_Beep());     // index BEEP
	mTypeCombo->addItem(i18n_File());     // index PLAY_FILE
	mSpeakShowing = !theApp()->speechEnabled();
	showSpeak(!mSpeakShowing);            // index SPEAK (only displayed if appropriate)
	connect(mTypeCombo, SIGNAL(activated(int)), SLOT(slotSoundSelected(int)));
	label->setBuddy(mTypeCombo);
	soundLayout->addWidget(mTypeBox);

	// Sound file picker button
	mFilePicker = new PushButton(this);
	mFilePicker->setIcon(SmallIcon("playsound"));
	mFilePicker->setFixedSize(mFilePicker->sizeHint());
	connect(mFilePicker, SIGNAL(clicked()), SLOT(slotPickFile()));
	mFilePicker->setWhatsThis(i18n("Configure a sound file to play when the alarm is displayed."));
	soundLayout->addWidget(mFilePicker);

	// Initialise the file picker button state and tooltip
	mTypeCombo->setCurrentIndex(NONE);
	mFilePicker->setEnabled(false);
}

/******************************************************************************
* Set the read-only status of the widget.
*/
void SoundPicker::setReadOnly(bool readOnly)
{
	mTypeCombo->setReadOnly(readOnly);
#ifdef WITHOUT_ARTS
	mFilePicker->setReadOnly(readOnly);
#endif
	mReadOnly = readOnly;
}

/******************************************************************************
* Show or hide the Speak option.
*/
void SoundPicker::showSpeak(bool show)
{
	if (!theApp()->speechEnabled())
		show = false;    // speech capability is not installed
	if (show == mSpeakShowing)
		return;    // no change
	QString whatsThis = i18n("Choose a sound to play when the message is displayed.\n"
	                         "%1: the message is displayed silently.\n"
	                         "%2: a simple beep is sounded.\n"
	                         "%3: an audio file is played. You will be prompted to choose the file and select play options.",
	                         "<b>" + i18n_None() + "<\b>",
	                         "<b>" + i18n_Beep() + "<\b>",
	                         "<b>" + i18n_File() + "<\b>");
	if (!show  &&  mTypeCombo->currentIndex() == SPEAK)
		mTypeCombo->setCurrentIndex(NONE);
	mTypeCombo->removeItem(SPEAK);    // precaution in case of mix-ups
	if (show)
	{
		mTypeCombo->addItem(i18n_Speak());
		whatsThis += i18n("%3: the message text is spoken.", "\n<b>" + i18n_Speak() + "<\b>");
	}
	mTypeBox->setWhatsThis(whatsThis);
	mSpeakShowing = show;
}

/******************************************************************************
* Return the currently selected option.
*/
SoundPicker::Type SoundPicker::sound() const
{
	return static_cast<SoundPicker::Type>(mTypeCombo->currentIndex());
}

/******************************************************************************
* Return the selected sound file, if the File option is selected.
* Returns null string if File is not currently selected.
*/
QString SoundPicker::file() const
{
	return (mTypeCombo->currentIndex() == PLAY_FILE) ? mFile : QString();
}

/******************************************************************************
* Return the specified volumes (range 0 - 1).
* Returns < 0 if beep is currently selected, or if 'set volume' is not selected.
*/
float SoundPicker::volume(float& fadeVolume, int& fadeSeconds) const
{
	if (mTypeCombo->currentIndex() == PLAY_FILE  &&  !mFile.isEmpty())
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
	return mTypeCombo->currentIndex() == PLAY_FILE  &&  !mFile.isEmpty()  &&  mRepeat;
}

/******************************************************************************
* Initialise the widget's state.
*/
void SoundPicker::set(SoundPicker::Type type, const QString& f, float volume, float fadeVolume, int fadeSeconds, bool repeat)
{
	if (type == PLAY_FILE  &&  f.isEmpty())
		type = BEEP;
	mLastType    = NONE;
	mFile        = f;
	mVolume      = volume;
	mFadeVolume  = fadeVolume;
	mFadeSeconds = fadeSeconds;
	mRepeat      = repeat;
	mFilePicker->setToolTip(mFile);
	mTypeCombo->setCurrentIndex(type);
	mFilePicker->setEnabled(type == PLAY_FILE);
}

/******************************************************************************
* Called when the sound option is changed.
*/
void SoundPicker::slotTypeSelected(int id)
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
		mFilePicker->setEnabled(true);
	}
	mLastType = newType;
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
	SoundDlg dlg(mFile, mVolume, mFadeVolume, mFadeSeconds, mRepeat, i18n("Sound File"), this);
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
		// No audio file is selected, so revert to previously selected option
#if 0
// Remove mRevertType, setLastType(), #include QTimer
		// But wait a moment until setting the radio button, or it won't work.
		mRevertType = true;   // prevent sound dialogue popping up twice
		QTimer::singleShot(0, this, SLOT(setLastType()));
#else
		mTypeCombo->setCurrentIndex(mLastType);
#endif
	}
}

/******************************************************************************
* Select the previously selected sound type.
*/
void SoundPicker::setLastType()
{
	mTypeCombo->setCurrentIndex(mLastType);
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
	return KAlarm::browseFile(i18n("Choose Sound File"), defaultDir, initialFile, filter, KFile::ExistingOnly, 0);
}
