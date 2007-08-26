/*
 *  soundpicker.cpp  -  widget to select a sound file or a beep
 *  Program:  kalarm
 *  Copyright © 2002,2004-2007 by David Jarvie <software@astrojar.org.uk>
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

#include <qlayout.h>
#include <qregexp.h>
#include <qtooltip.h>
#include <qtimer.h>
#include <qlabel.h>
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

#include "combobox.h"
#include "functions.h"
#include "kalarmapp.h"
#include "pushbutton.h"
#include "sounddlg.h"
#include "soundpicker.moc"


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString SoundPicker::i18n_Sound()       { return i18n("An audio sound", "Sound"); }
QString SoundPicker::i18n_None()        { return i18n("None"); }
QString SoundPicker::i18n_Beep()        { return i18n("Beep"); }
QString SoundPicker::i18n_Speak()       { return i18n("Speak"); }
QString SoundPicker::i18n_File()        { return i18n("Sound file"); }


SoundPicker::SoundPicker(QWidget* parent, const char* name)
	: QFrame(parent, name)
{
	setFrameStyle(QFrame::NoFrame);
	QHBoxLayout* soundLayout = new QHBoxLayout(this, 0, KDialog::spacingHint());
	mTypeBox = new QHBox(this);    // this is to control the QWhatsThis text display area
	mTypeBox->setSpacing(KDialog::spacingHint());

	QLabel* label = new QLabel(i18n("An audio sound", "&Sound:"), mTypeBox);
	label->setFixedSize(label->sizeHint());

	// Sound type combo box
	// The order of combo box entries must correspond with the 'Type' enum.
	mTypeCombo = new ComboBox(false, mTypeBox);
	mTypeCombo->insertItem(i18n_None());     // index NONE
	mTypeCombo->insertItem(i18n_Beep());     // index BEEP
	mTypeCombo->insertItem(i18n_File());     // index PLAY_FILE
	mSpeakShowing = !theApp()->speechEnabled();
	showSpeak(!mSpeakShowing);               // index SPEAK (only displayed if appropriate)
	connect(mTypeCombo, SIGNAL(activated(int)), SLOT(slotTypeSelected(int)));
	label->setBuddy(mTypeCombo);
	soundLayout->addWidget(mTypeBox);

	// Sound file picker button
	mFilePicker = new PushButton(this);
	mFilePicker->setPixmap(SmallIcon("playsound"));
	mFilePicker->setFixedSize(mFilePicker->sizeHint());
	connect(mFilePicker, SIGNAL(clicked()), SLOT(slotPickFile()));
	QToolTip::add(mFilePicker, i18n("Configure sound file"));
	QWhatsThis::add(mFilePicker, i18n("Configure a sound file to play when the alarm is displayed."));
	soundLayout->addWidget(mFilePicker);

	// Initialise the file picker button state and tooltip
	mTypeCombo->setCurrentItem(NONE);
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
	QString whatsThis = "<p>" + i18n("Choose a sound to play when the message is displayed.")
	                  + "<br>" + i18n("%1: the message is displayed silently.").arg("<b>" + i18n_None() + "</b>")
	                  + "<br>" + i18n("%1: a simple beep is sounded.").arg("<b>" + i18n_Beep() + "</b>")
	                  + "<br>" + i18n("%1: an audio file is played. You will be prompted to choose the file and set play options.").arg("<b>" + i18n_File() + "</b>");
	if (!show  &&  mTypeCombo->currentItem() == SPEAK)
		mTypeCombo->setCurrentItem(NONE);
	if (mTypeCombo->count() == SPEAK+1)
		mTypeCombo->removeItem(SPEAK);    // precaution in case of mix-ups
	if (show)
	{
		mTypeCombo->insertItem(i18n_Speak());
		whatsThis += "<br>" + i18n("%1: the message text is spoken.").arg("<b>" + i18n_Speak() + "</b>") + "</p>";
	}
	QWhatsThis::add(mTypeBox, whatsThis + "</p>");
	mSpeakShowing = show;
}

/******************************************************************************
* Return the currently selected option.
*/
SoundPicker::Type SoundPicker::sound() const
{
	return static_cast<SoundPicker::Type>(mTypeCombo->currentItem());
}

/******************************************************************************
* Return the selected sound file, if the File option is selected.
* Returns null string if File is not currently selected.
*/
QString SoundPicker::file() const
{
	return (mTypeCombo->currentItem() == PLAY_FILE) ? mFile : QString::null;
}

/******************************************************************************
* Return the specified volumes (range 0 - 1).
* Returns < 0 if beep is currently selected, or if 'set volume' is not selected.
*/
float SoundPicker::volume(float& fadeVolume, int& fadeSeconds) const
{
	if (mTypeCombo->currentItem() == PLAY_FILE  &&  !mFile.isEmpty())
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
	return mTypeCombo->currentItem() == PLAY_FILE  &&  !mFile.isEmpty()  &&  mRepeat;
}

/******************************************************************************
* Initialise the widget's state.
*/
void SoundPicker::set(SoundPicker::Type type, const QString& f, float volume, float fadeVolume, int fadeSeconds, bool repeat)
{
	if (type == PLAY_FILE  &&  f.isEmpty())
		type = BEEP;
	mFile        = f;
	mVolume      = volume;
	mFadeVolume  = fadeVolume;
	mFadeSeconds = fadeSeconds;
	mRepeat      = repeat;
	mTypeCombo->setCurrentItem(type);  // this doesn't trigger slotTypeSelected()
	mFilePicker->setEnabled(type == PLAY_FILE);
	if (type == PLAY_FILE)
		QToolTip::add(mTypeCombo, mFile);
	else
		QToolTip::remove(mTypeCombo);
	mLastType = type;
}

/******************************************************************************
* Called when the sound option is changed.
*/
void SoundPicker::slotTypeSelected(int id)
{
	Type newType = static_cast<Type>(id);
	if (newType == mLastType)
		return;
	QString tooltip;
	if (mLastType == PLAY_FILE)
	{
		mFilePicker->setEnabled(false);
		QToolTip::remove(mTypeCombo);
	}
	else if (newType == PLAY_FILE)
	{
		if (mFile.isEmpty())
		{
			slotPickFile();
			if (mFile.isEmpty())
				return;    // revert to previously selected type
		}
		mFilePicker->setEnabled(true);
		QToolTip::add(mTypeCombo, mFile);
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
	if (mFile.isEmpty())
	{
		// No audio file is selected, so revert to previously selected option
		mTypeCombo->setCurrentItem(mLastType);
		QToolTip::remove(mTypeCombo);
	}
	else
		QToolTip::add(mTypeCombo, mFile);
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
	QStringList filters = KDE::PlayObjectFactory::mimeTypes();
	QString filter = filters.join(" ");
#endif
	return KAlarm::browseFile(i18n("Choose Sound File"), defaultDir, initialFile, filter, KFile::ExistingOnly, 0, "pickSoundFile");
}
