/*
 *  sounddlg.cpp  -  sound file selection and configuration dialog
 *  Program:  kalarm
 *  Copyright © 2005-2009 by David Jarvie <djarvie@kde.org>
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

#include <QLabel>
#include <QDir>
#include <QGroupBox>
#include <QApplication>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QShowEvent>
#include <QResizeEvent>

#include <klocale.h>
#include <kstandarddirs.h>
#include <kiconloader.h>
#include <khbox.h>
#include <kio/netaccess.h>
#include <Phonon/MediaObject>

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
QString SoundDlg::i18n_chk_Repeat()      { return i18nc("@option:check", "Repeat"); }

static const char SOUND_DIALOG_NAME[] = "SoundDialog";


SoundDlg::SoundDlg(const QString& file, float volume, float fadeVolume, int fadeSeconds, bool repeat,
                   const QString& caption, QWidget* parent)
	: KDialog(parent),
	  mPlayer(0),
	  mReadOnly(false)
{
	QWidget* page = new QWidget(this);
	setMainWidget(page);
	setCaption(caption);
	setButtons(Ok|Cancel);
	setDefaultButton(Ok);
	connect(this, SIGNAL(okClicked()), SLOT(slotOk()));

	QVBoxLayout* layout = new QVBoxLayout(page);
	layout->setMargin(0);
	layout->setSpacing(spacingHint());

	// File play button
	KHBox* box = new KHBox(page);
	box->setMargin(0);
	layout->addWidget(box);
	mFilePlay = new QPushButton(box);
	mFilePlay->setIcon(SmallIcon("media-playback-start"));
	connect(mFilePlay, SIGNAL(clicked()), SLOT(playSound()));
	mFilePlay->setToolTip(i18nc("@info:tooltip", "Test the sound"));
	mFilePlay->setWhatsThis(i18nc("@info:whatsthis", "Play the selected sound file."));

	// File name edit box
	mFileEdit = new LineEdit(LineEdit::Url, box);
	mFileEdit->setAcceptDrops(true);
	mFileEdit->setWhatsThis(i18nc("@info:whatsthis", "Enter the name or URL of a sound file to play."));

	// File browse button
	mFileBrowseButton = new PushButton(box);
	mFileBrowseButton->setIcon(SmallIcon("document-open"));
	connect(mFileBrowseButton, SIGNAL(clicked()), SLOT(slotPickFile()));
	mFileBrowseButton->setToolTip(i18nc("@info:tooltip", "Choose a file"));
	mFileBrowseButton->setWhatsThis(i18nc("@info:whatsthis", "Select a sound file to play."));

	int size = qMax(mFilePlay->sizeHint().height(), mFileBrowseButton->sizeHint().height());
	mFilePlay->setFixedSize(size, size);
	mFileBrowseButton->setFixedSize(size, size);


	// Sound repetition checkbox
	mRepeatCheckbox = new CheckBox(i18n_chk_Repeat(), page);
	mRepeatCheckbox->setFixedSize(mRepeatCheckbox->sizeHint());
	mRepeatCheckbox->setWhatsThis(i18nc("@info:whatsthis", "If checked, the sound file will be played repeatedly for as long as the message is displayed."));
	layout->addWidget(mRepeatCheckbox);

	// Volume
	QGroupBox* group = new QGroupBox(i18nc("@title:group Sound volume", "Volume"), page);
	layout->addWidget(group);
	QGridLayout* grid = new QGridLayout(group);
	grid->setMargin(marginHint());
	grid->setSpacing(spacingHint());
	grid->setColumnStretch(2, 1);
	int indentWidth = 3 * KDialog::spacingHint();
	grid->setColumnMinimumWidth(0, indentWidth);
	grid->setColumnMinimumWidth(1, indentWidth);

	// 'Set volume' checkbox
	box = new KHBox(group);
	box->setMargin(0);
	box->setSpacing(spacingHint());
	grid->addWidget(box, 1, 0, 1, 3);
	mVolumeCheckbox = new CheckBox(i18nc("@option:check", "Set volume"), box);
	mVolumeCheckbox->setFixedSize(mVolumeCheckbox->sizeHint());
	connect(mVolumeCheckbox, SIGNAL(toggled(bool)), SLOT(slotVolumeToggled(bool)));
	mVolumeCheckbox->setWhatsThis(i18nc("@info:whatsthis", "Select to choose the volume for playing the sound file."));

	// Volume slider
	mVolumeSlider = new Slider(0, 100, 10, Qt::Horizontal, box);
	mVolumeSlider->setTickPosition(QSlider::TicksBelow);
	mVolumeSlider->setTickInterval(10);
	mVolumeSlider->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
	mVolumeSlider->setWhatsThis(i18nc("@info:whatsthis", "Choose the volume for playing the sound file."));
	mVolumeCheckbox->setFocusWidget(mVolumeSlider);

	// Fade checkbox
	mFadeCheckbox = new CheckBox(i18nc("@option:check", "Fade"), group);
	mFadeCheckbox->setFixedSize(mFadeCheckbox->sizeHint());
	connect(mFadeCheckbox, SIGNAL(toggled(bool)), SLOT(slotFadeToggled(bool)));
	mFadeCheckbox->setWhatsThis(i18nc("@info:whatsthis", "Select to fade the volume when the sound file first starts to play."));
	grid->addWidget(mFadeCheckbox, 2, 1, 1, 2, Qt::AlignLeft);

	// Fade time
	mFadeBox = new KHBox(group);
	mFadeBox->setMargin(0);
	mFadeBox->setSpacing(spacingHint());
	grid->addWidget(mFadeBox, 3, 2, Qt::AlignLeft);
	QLabel* label = new QLabel(i18nc("@label:spinbox Time period over which to fade the sound", "Fade time:"), mFadeBox);
	label->setFixedSize(label->sizeHint());
	mFadeTime = new SpinBox(1, 999, mFadeBox);
	mFadeTime->setSingleShiftStep(10);
	mFadeTime->setFixedSize(mFadeTime->sizeHint());
	label->setBuddy(mFadeTime);
	label = new QLabel(i18nc("@label", "seconds"), mFadeBox);
	label->setFixedSize(label->sizeHint());
	mFadeBox->setWhatsThis(i18nc("@info:whatsthis", "Enter how many seconds to fade the sound before reaching the set volume."));

	// Fade slider
	mFadeVolumeBox = new KHBox(group);
	mFadeVolumeBox->setMargin(0);
	mFadeVolumeBox->setSpacing(spacingHint());
	grid->addWidget(mFadeVolumeBox, 4, 2);
	label = new QLabel(i18nc("@label:slider", "Initial volume:"), mFadeVolumeBox);
	label->setFixedSize(label->sizeHint());
	mFadeSlider = new Slider(0, 100, 10, Qt::Horizontal, mFadeVolumeBox);
	mFadeSlider->setTickPosition(QSlider::TicksBelow);
	mFadeSlider->setTickInterval(10);
	mFadeSlider->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
	label->setBuddy(mFadeSlider);
	mFadeVolumeBox->setWhatsThis(i18nc("@info:whatsthis", "Choose the initial volume for playing the sound file."));

	// Restore the dialog size from last time
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

SoundDlg::~SoundDlg()
{
	delete mPlayer;   // this stops playing if not already stopped
	mPlayer = 0;
}

/******************************************************************************
 * Set the read-only status of the dialog.
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
		if (readOnly)
		{
			setButtons(Cancel);
			setDefaultButton(Cancel);
		}
		else
		{
			setButtons(Ok|Cancel);
			setDefaultButton(Ok);
		}
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
* Called when the file play or stop button is clicked.
*/
void SoundDlg::playSound()
{
	if (mPlayer)
	{
		// The file is currently playing. Stop it.
		playFinished();
		return;
	}
	if (!checkFile())
		return;
	mPlayer = Phonon::createPlayer(Phonon::MusicCategory, mUrl);
	connect(mPlayer, SIGNAL(finished()), SLOT(playFinished()));
	mFilePlay->setIcon(SmallIcon("media-playback-stop"));   // change the play button to a stop button
	mFilePlay->setToolTip(i18nc("@info:tooltip", "Stop sound"));
	mFilePlay->setWhatsThis(i18nc("@info:whatsthis", "Stop playing the sound"));
	mPlayer->play();
}

/******************************************************************************
* Called when playing the file has completed, or to stop playing.
*/
void SoundDlg::playFinished()
{
	delete mPlayer;   // this stops playing if not already stopped
	mPlayer = 0;
	mFilePlay->setIcon(SmallIcon("media-playback-start"));
	mFilePlay->setToolTip(i18nc("@info:tooltip", "Test the sound"));
	mFilePlay->setWhatsThis(i18nc("@info:whatsthis", "Play the selected sound file."));
}

/******************************************************************************
* Check whether the specified sound file exists.
*/
bool SoundDlg::checkFile()
{
	QString file = mFileEdit->text();
	mUrl = KUrl(file);
	if (KIO::NetAccess::exists(mUrl, KIO::NetAccess::SourceSide, this))
		return true;
	if (mUrl.isLocalFile()  &&  !file.startsWith(QLatin1String("/")))
	{
		// It's a relative path.
		// Find the first sound resource that contains files.
		QStringList soundDirs = KGlobal::dirs()->resourceDirs("sound");
		if (!soundDirs.isEmpty())
		{
			QDir dir;
			dir.setFilter(QDir::Files | QDir::Readable);
			for (int i = 0, end = soundDirs.count();  i < end;  ++i)
			{
				dir = soundDirs[i];
				if (dir.isReadable() && dir.count() > 2)
				{
					mUrl.setPath(soundDirs[i]);
					mUrl.addPath(file);
					if (KIO::NetAccess::exists(mUrl, KIO::NetAccess::SourceSide, this))
						return true;
				}
			}
		}
		mUrl.setPath(QDir::homePath());
		mUrl.addPath(file);
		if (KIO::NetAccess::exists(mUrl, KIO::NetAccess::SourceSide, this))
			return true;
	}
	KMessageBox::sorry(this, i18nc("@info", "File not found"));
	mUrl.clear();
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
