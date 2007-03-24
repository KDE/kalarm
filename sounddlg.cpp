/*
 *  sounddlg.cpp  -  sound file selection and configuration dialog
 *  Program:  kalarm
 *  Copyright © 2005,2007 by David Jarvie <software@astrojar.org.uk>
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

#ifndef WITHOUT_ARTS

#include "kalarm.h"

#include <qlabel.h>
#include <qhbox.h>
#include <qgroupbox.h>
#include <qlayout.h>
#include <qfile.h>
#include <qdir.h>
#include <qwhatsthis.h>
#include <qtooltip.h>

#include <klocale.h>
#include <kstandarddirs.h>
#include <kiconloader.h>
#include <kaudioplayer.h>
#include <kmessagebox.h>
#include <kio/netaccess.h>
#include <kdebug.h>

#include "checkbox.h"
#include "functions.h"
#include "lineedit.h"
#include "pushbutton.h"
#include "slider.h"
#include "soundpicker.h"
#include "spinbox.h"
#include "sounddlg.moc"


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString SoundDlg::i18n_SetVolume()   { return i18n("Set volume"); }
QString SoundDlg::i18n_v_SetVolume() { return i18n("Set &volume"); }
QString SoundDlg::i18n_Repeat()      { return i18n("Repeat"); }
QString SoundDlg::i18n_p_Repeat()    { return i18n("Re&peat"); }

static const char SOUND_DIALOG_NAME[] = "SoundDialog";


SoundDlg::SoundDlg(const QString& file, float volume, float fadeVolume, int fadeSeconds, bool repeat,
                   const QString& caption, QWidget* parent, const char* name)
	: KDialogBase(parent, name, true, caption, Ok|Cancel, Ok, false),
	  mReadOnly(false)
{
	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* layout = new QVBoxLayout(page, 0, spacingHint());

	// File play button
	QHBox* box = new QHBox(page);
	layout->addWidget(box);
	mFilePlay = new QPushButton(box);
	mFilePlay->setPixmap(SmallIcon("player_play"));
	mFilePlay->setFixedSize(mFilePlay->sizeHint());
	connect(mFilePlay, SIGNAL(clicked()), SLOT(playSound()));
	QToolTip::add(mFilePlay, i18n("Test the sound"));
	QWhatsThis::add(mFilePlay, i18n("Play the selected sound file."));

	// File name edit box
	mFileEdit = new LineEdit(LineEdit::Url, box);
	mFileEdit->setAcceptDrops(true);
	QWhatsThis::add(mFileEdit, i18n("Enter the name or URL of a sound file to play."));

	// File browse button
	mFileBrowseButton = new PushButton(box);
	mFileBrowseButton->setPixmap(SmallIcon("fileopen"));
	mFileBrowseButton->setFixedSize(mFileBrowseButton->sizeHint());
	connect(mFileBrowseButton, SIGNAL(clicked()), SLOT(slotPickFile()));
	QToolTip::add(mFileBrowseButton, i18n("Choose a file"));
	QWhatsThis::add(mFileBrowseButton, i18n("Select a sound file to play."));

	// Sound repetition checkbox
	mRepeatCheckbox = new CheckBox(i18n_p_Repeat(), page);
	mRepeatCheckbox->setFixedSize(mRepeatCheckbox->sizeHint());
	QWhatsThis::add(mRepeatCheckbox,
	      i18n("If checked, the sound file will be played repeatedly for as long as the message is displayed."));
	layout->addWidget(mRepeatCheckbox);

	// Volume
	QGroupBox* group = new QGroupBox(i18n("Volume"), page);
	layout->addWidget(group);
	QGridLayout* grid = new QGridLayout(group, 4, 3, marginHint(), spacingHint());
	grid->addRowSpacing(0, fontMetrics().lineSpacing()/2);
	grid->setColStretch(2, 1);
	int indentWidth = 3 * KDialog::spacingHint();
	grid->addColSpacing(0, indentWidth);
	grid->addColSpacing(1, indentWidth);
	// Get alignment to use in QGridLayout (AlignAuto doesn't work correctly there)
	int alignment = QApplication::reverseLayout() ? Qt::AlignRight : Qt::AlignLeft;

	// 'Set volume' checkbox
	box = new QHBox(group);
	box->setSpacing(spacingHint());
	grid->addMultiCellWidget(box, 1, 1, 0, 2);
	mVolumeCheckbox = new CheckBox(i18n_v_SetVolume(), box);
	mVolumeCheckbox->setFixedSize(mVolumeCheckbox->sizeHint());
	connect(mVolumeCheckbox, SIGNAL(toggled(bool)), SLOT(slotVolumeToggled(bool)));
	QWhatsThis::add(mVolumeCheckbox,
	      i18n("Select to choose the volume for playing the sound file."));

	// Volume slider
	mVolumeSlider = new Slider(0, 100, 10, 0, Qt::Horizontal, box);
	mVolumeSlider->setTickmarks(QSlider::Below);
	mVolumeSlider->setTickInterval(10);
	mVolumeSlider->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
	QWhatsThis::add(mVolumeSlider, i18n("Choose the volume for playing the sound file."));
	mVolumeCheckbox->setFocusWidget(mVolumeSlider);

	// Fade checkbox
	mFadeCheckbox = new CheckBox(i18n("Fade"), group);
	mFadeCheckbox->setFixedSize(mFadeCheckbox->sizeHint());
	connect(mFadeCheckbox, SIGNAL(toggled(bool)), SLOT(slotFadeToggled(bool)));
	QWhatsThis::add(mFadeCheckbox,
	      i18n("Select to fade the volume when the sound file first starts to play."));
	grid->addMultiCellWidget(mFadeCheckbox, 2, 2, 1, 2, alignment);

	// Fade time
	mFadeBox = new QHBox(group);
	mFadeBox->setSpacing(spacingHint());
	grid->addWidget(mFadeBox, 3, 2, alignment);
	QLabel* label = new QLabel(i18n("Time period over which to fade the sound", "Fade time:"), mFadeBox);
	label->setFixedSize(label->sizeHint());
	mFadeTime = new SpinBox(1, 999, 1, mFadeBox);
	mFadeTime->setLineShiftStep(10);
	mFadeTime->setFixedSize(mFadeTime->sizeHint());
	label->setBuddy(mFadeTime);
	label = new QLabel(i18n("seconds"), mFadeBox);
	label->setFixedSize(label->sizeHint());
	QWhatsThis::add(mFadeBox, i18n("Enter how many seconds to fade the sound before reaching the set volume."));

	// Fade slider
	mFadeVolumeBox = new QHBox(group);
	mFadeVolumeBox->setSpacing(spacingHint());
	grid->addWidget(mFadeVolumeBox, 4, 2);
	label = new QLabel(i18n("Initial volume:"), mFadeVolumeBox);
	label->setFixedSize(label->sizeHint());
	mFadeSlider = new Slider(0, 100, 10, 0, Qt::Horizontal, mFadeVolumeBox);
	mFadeSlider->setTickmarks(QSlider::Below);
	mFadeSlider->setTickInterval(10);
	mFadeSlider->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
	label->setBuddy(mFadeSlider);
	QWhatsThis::add(mFadeVolumeBox, i18n("Choose the initial volume for playing the sound file."));

	// Restore the dialogue size from last time
	QSize s;
	if (KAlarm::readConfigWindowSize(SOUND_DIALOG_NAME, s))
		resize(s);

	// Initialise the control values
	mFileEdit->setText(file);
	mRepeatCheckbox->setChecked(repeat);
	mVolumeCheckbox->setChecked(volume >= 0);
	mVolumeSlider->setValue(volume >= 0 ? static_cast<int>(volume*100) : 100);
	mFadeCheckbox->setChecked(fadeVolume >= 0);
	mFadeSlider->setValue(fadeVolume >= 0 ? static_cast<int>(fadeVolume*100) : 100);
	mFadeTime->setValue(fadeSeconds);
	slotVolumeToggled(volume >= 0);
}

/******************************************************************************
* Set the read-only status of the dialogue.
*/
void SoundDlg::setReadOnly(bool readOnly)
{
	if (readOnly != mReadOnly)
	{
		mFileEdit->setReadOnly(readOnly);
		mFileBrowseButton->setReadOnly(readOnly);
		mRepeatCheckbox->setReadOnly(readOnly);
		mVolumeCheckbox->setReadOnly(readOnly);
		mVolumeSlider->setReadOnly(readOnly);
		mFadeCheckbox->setReadOnly(readOnly);
		mFadeTime->setReadOnly(readOnly);
		mFadeSlider->setReadOnly(readOnly);
		mReadOnly = readOnly;
	}
}

/******************************************************************************
* Return the entered repetition and volume settings:
* 'volume' is in range 0 - 1, or < 0 if volume is not to be set.
* 'fadeVolume is similar, with 'fadeTime' set to the fade interval in seconds.
* Reply = whether to repeat or not.
*/
bool SoundDlg::getSettings(float& volume, float& fadeVolume, int& fadeSeconds) const
{
	volume = mVolumeCheckbox->isChecked() ? (float)mVolumeSlider->value() / 100 : -1;
	if (mFadeCheckbox->isChecked())
	{
		fadeVolume  = (float)mFadeSlider->value() / 100;
		fadeSeconds = mFadeTime->value();
	}
	else
	{
		fadeVolume  = -1;
		fadeSeconds = 0;
	}
	return mRepeatCheckbox->isChecked();
}

/******************************************************************************
*  Called when the dialog's size has changed.
*  Records the new size in the config file.
*/
void SoundDlg::resizeEvent(QResizeEvent* re)
{
	if (isVisible())
		KAlarm::writeConfigWindowSize(SOUND_DIALOG_NAME, re->size());
	mVolumeSlider->resize(mFadeSlider->size());
	KDialog::resizeEvent(re);
}

void SoundDlg::showEvent(QShowEvent* se)
{
	mVolumeSlider->resize(mFadeSlider->size());
	KDialog::showEvent(se);
}

/******************************************************************************
*  Called when the OK button is clicked.
*/
void SoundDlg::slotOk()
{
	if (mReadOnly)
		reject();
	if (!checkFile())
		return;
	accept();
}

/******************************************************************************
* Called when the file browser button is clicked.
*/
void SoundDlg::slotPickFile()
{
	QString url = SoundPicker::browseFile(mDefaultDir, mFileEdit->text());
	if (!url.isEmpty())
		mFileEdit->setText(url);
}

/******************************************************************************
* Called when the file play button is clicked.
*/
void SoundDlg::playSound()
{
	if (checkFile())
		KAudioPlayer::play(QFile::encodeName(mFileName));
}

/******************************************************************************
* Check whether the specified sound file exists.
* Note that KAudioPlayer::play() can only cope with local files.
*/
bool SoundDlg::checkFile()
{
	QString file = mFileEdit->text();
	KURL url;
	if (KURL::isRelativeURL(file))
	{
		// It's not an absolute URL, so check for an absolute path
		QFileInfo f(file);
		if (!f.isRelative())
			url.setPath(file);
	}
	else
		url = KURL::fromPathOrURL(file);   // it's an absolute URL
	if (!url.isEmpty())
	{
		// It's an absolute path or URL.
		// Only allow local files for KAudioPlayer.
		if (url.isLocalFile()  &&  KIO::NetAccess::exists(url, true, this))
		{
			mFileName = url.path();
			return true;
		}
	}
	else
	{
		// It's a relative path.
		// Find the first sound resource that contains files.
		QStringList soundDirs = KGlobal::dirs()->resourceDirs("sound");
		if (!soundDirs.isEmpty())
		{
			QDir dir;
			dir.setFilter(QDir::Files | QDir::Readable);
			for (QStringList::ConstIterator it = soundDirs.begin();  it != soundDirs.end();  ++it)
			{
				dir = *it;
				if (dir.isReadable() && dir.count() > 2)
				{
					url.setPath(*it);
					url.addPath(file);
					if (KIO::NetAccess::exists(url, true, this))
					{
						mFileName = url.path();
						return true;
					}
				}
			}
		}
		url.setPath(QDir::homeDirPath());
		url.addPath(file);
		if (KIO::NetAccess::exists(url, true, this))
		{
			mFileName = url.path();
			return true;
		}
	}
	KMessageBox::sorry(this, i18n("File not found"));
	mFileName = QString::null;
	return false;
}

/******************************************************************************
* Called when the Set Volume checkbox is toggled.
*/
void SoundDlg::slotVolumeToggled(bool on)
{
	mVolumeSlider->setEnabled(on);
	mFadeCheckbox->setEnabled(on);
	slotFadeToggled(on  &&  mFadeCheckbox->isChecked());
}

/******************************************************************************
* Called when the Fade checkbox is toggled.
*/
void SoundDlg::slotFadeToggled(bool on)
{
	mFadeBox->setEnabled(on);
	mFadeVolumeBox->setEnabled(on);
}

#endif // #ifndef WITHOUT_ARTS
