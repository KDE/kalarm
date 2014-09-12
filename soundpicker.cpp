/*
 *  soundpicker.cpp  -  widget to select a sound file or a beep
 *  Program:  kalarm
 *  Copyright © 2002-2013 by David Jarvie <djarvie@kde.org>
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

#include "autoqpointer.h"
#include "combobox.h"
#include "functions.h"
#include "kalarmapp.h"
#include "pushbutton.h"
#include "sounddlg.h"
#include "soundpicker.h"

#include <KIcon>
#include <kglobal.h>
#include <KLocalizedString>
#include <kfiledialog.h>
#include <kiconloader.h>
#include <phonon/backendcapabilities.h>

#include <QTimer>
#include <QLabel>
#include <QHBoxLayout>
#include <QStandardPaths>
#include <qdebug.h>


static QMap<Preferences::SoundType, int> indexes;    // mapping from sound type to combo index


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString SoundPicker::i18n_label_Sound()   { return i18nc("@label:listbox Listbox providing audio options", "Sound:"); }
QString SoundPicker::i18n_combo_None()    { return i18nc("@item:inlistbox No sound", "None"); }
QString SoundPicker::i18n_combo_Beep()    { return i18nc("@item:inlistbox", "Beep"); }
QString SoundPicker::i18n_combo_Speak()   { return i18nc("@item:inlistbox", "Speak"); }
QString SoundPicker::i18n_combo_File()    { return i18nc("@item:inlistbox", "Sound file"); }


SoundPicker::SoundPicker(QWidget* parent)
    : QFrame(parent),
      mRevertType(false),
      mReadOnly(false)
{
    QHBoxLayout* soundLayout = new QHBoxLayout(this);
    soundLayout->setMargin(0);
    soundLayout->setSpacing(KDialog::spacingHint());
    mTypeBox = new QWidget(this);    // this is to control the QWhatsThis text display area
    QHBoxLayout* typeBoxLayout = new QHBoxLayout(mTypeBox);
    typeBoxLayout->setMargin(0);

    QLabel* label = new QLabel(i18n_label_Sound(), mTypeBox);
    typeBoxLayout->addWidget(label);
    label->setFixedSize(label->sizeHint());

    // Sound type combo box
    // The order of combo box entries must correspond with the 'Type' enum.
    if (indexes.isEmpty())
    {
        indexes[Preferences::Sound_None]     = 0;
        indexes[Preferences::Sound_Beep]     = 1;
        indexes[Preferences::Sound_File] = 2;
        indexes[Preferences::Sound_Speak]    = 3;
    }

    mTypeCombo = new ComboBox(mTypeBox);
    typeBoxLayout->addWidget(mTypeCombo);
    mTypeCombo->addItem(i18n_combo_None());     // index None
    mTypeCombo->addItem(i18n_combo_Beep());     // index Beep
    mTypeCombo->addItem(i18n_combo_File());     // index PlayFile
    mSpeakShowing = !theApp()->speechEnabled();
    showSpeak(!mSpeakShowing);            // index Speak (only displayed if appropriate)
    connect(mTypeCombo, static_cast<void (ComboBox::*)(int)>(&ComboBox::activated), this, &SoundPicker::slotTypeSelected);
    connect(mTypeCombo, static_cast<void (ComboBox::*)(int)>(&ComboBox::currentIndexChanged), this, &SoundPicker::changed);
    label->setBuddy(mTypeCombo);
    soundLayout->addWidget(mTypeBox);

    // Sound file picker button
    mFilePicker = new PushButton(this);
    mFilePicker->setIcon(KIcon(SmallIcon(QLatin1String("audio-x-generic"))));
    int size = mFilePicker->sizeHint().height();
    mFilePicker->setFixedSize(size, size);
    connect(mFilePicker, &PushButton::clicked, this, &SoundPicker::slotPickFile);
    mFilePicker->setToolTip(i18nc("@info:tooltip", "Configure sound file"));
    mFilePicker->setWhatsThis(i18nc("@info:whatsthis", "Configure a sound file to play when the alarm is displayed."));
    soundLayout->addWidget(mFilePicker);

    // Initialise the file picker button state and tooltip
    mTypeCombo->setCurrentIndex(indexes[Preferences::Sound_None]);
    mFilePicker->setEnabled(false);
}

/******************************************************************************
* Set the read-only status of the widget.
*/
void SoundPicker::setReadOnly(bool readOnly)
{
    // Don't set the sound file picker read-only since it still needs to
    // display the read-only SoundDlg.
    mTypeCombo->setReadOnly(readOnly);
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
    if (!show  &&  mTypeCombo->currentIndex() == indexes[Preferences::Sound_Speak])
        mTypeCombo->setCurrentIndex(indexes[Preferences::Sound_None]);
    if (mTypeCombo->count() == indexes[Preferences::Sound_Speak]+1)
        mTypeCombo->removeItem(indexes[Preferences::Sound_Speak]);    // precaution in case of mix-ups
    QString whatsThis;
    QString opt1 = xi18nc("@info:whatsthis", "<interface>%1</interface>: the message is displayed silently.", i18n_combo_None());
    QString opt2 = xi18nc("@info:whatsthis", "<interface>%1</interface>: a simple beep is sounded.", i18n_combo_Beep());
    QString opt3 = xi18nc("@info:whatsthis", "<interface>%1</interface>: an audio file is played. You will be prompted to choose the file and set play options.", i18n_combo_File());
    if (show)
    {
        mTypeCombo->addItem(i18n_combo_Speak());
        QString opt4 = xi18nc("@info:whatsthis", "<interface>%1</interface>: the message text is spoken.", i18n_combo_Speak());
        whatsThis = xi18nc("@info:whatsthis Combination of multiple whatsthis items",
                          "<para>Choose a sound to play when the message is displayed:"
                          "<list><item>%1</item>"
                          "<item>%2</item>"
                          "<item>%3</item>"
                          "<item>%4</item></list></para>", opt1, opt2, opt3, opt4);
    }
    else
        whatsThis = xi18nc("@info:whatsthis Combination of multiple whatsthis items",
                          "<para>Choose a sound to play when the message is displayed:"
                          "<list><item>%1</item>"
                          "<item>%2</item>"
                          "<item>%3</item></list></para>", opt1, opt2, opt3);
    mTypeBox->setWhatsThis(whatsThis);
    mSpeakShowing = show;
}

/******************************************************************************
* Return the currently selected option.
*/
Preferences::SoundType SoundPicker::sound() const
{
    int current = mTypeCombo->currentIndex();
    for (QMap<Preferences::SoundType, int>::ConstIterator it = indexes.constBegin();  it != indexes.constEnd();  ++it)
        if (it.value() == current)
            return it.key();
    return Preferences::Sound_None;
}

/******************************************************************************
* Return the selected sound file, if the File option is selected.
* Returns empty URL if File is not currently selected.
*/
KUrl SoundPicker::file() const
{
    return (mTypeCombo->currentIndex() == indexes[Preferences::Sound_File]) ? mFile : KUrl();
}

/******************************************************************************
* Return the specified volumes (range 0 - 1).
* Returns < 0 if beep is currently selected, or if 'set volume' is not selected.
*/
float SoundPicker::volume(float& fadeVolume, int& fadeSeconds) const
{
    if (mTypeCombo->currentIndex() == indexes[Preferences::Sound_File]  &&  !mFile.isEmpty())
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
* Return the pause between sound file repetitions is selected.
* Reply = pause in seconds, or -1 if repetition is not selected or beep is selected.
*/
int SoundPicker::repeatPause() const
{
    return mTypeCombo->currentIndex() == indexes[Preferences::Sound_File]  &&  !mFile.isEmpty() ? mRepeatPause : -1;
}

/******************************************************************************
* Initialise the widget's state.
*/
void SoundPicker::set(Preferences::SoundType type, const QString& f, float volume, float fadeVolume, int fadeSeconds, int repeatPause)
{
    if (type == Preferences::Sound_File  &&  f.isEmpty())
        type = Preferences::Sound_Beep;
    mFile        = KUrl(f);
    mVolume      = volume;
    mFadeVolume  = fadeVolume;
    mFadeSeconds = fadeSeconds;
    mRepeatPause = repeatPause;
    mTypeCombo->setCurrentIndex(indexes[type]);  // this doesn't trigger slotTypeSelected()
    mFilePicker->setEnabled(type == Preferences::Sound_File);
    mTypeCombo->setToolTip(type == Preferences::Sound_File ? mFile.prettyUrl() : QString());
    mLastType = type;
}

/******************************************************************************
* Called when the sound option is changed.
*/
void SoundPicker::slotTypeSelected(int id)
{
    Preferences::SoundType newType = Preferences::Sound_None;
    for (QMap<Preferences::SoundType, int>::ConstIterator it = indexes.constBegin();  it != indexes.constEnd();  ++it)
    {
        if (it.value() == id)
        {
            newType = it.key();
            break;
        }
    }
    if (newType == mLastType  ||  mRevertType)
        return;
    if (mLastType == Preferences::Sound_File)
    {
        mFilePicker->setEnabled(false);
        mTypeCombo->setToolTip(QString());
    }
    else if (newType == Preferences::Sound_File)
    {
        if (mFile.isEmpty())
        {
            slotPickFile();
            if (mFile.isEmpty())
                return;    // revert to previously selected type
        }
        mFilePicker->setEnabled(true);
        mTypeCombo->setToolTip(mFile.prettyUrl());
    }
    mLastType = newType;
}

/******************************************************************************
* Called when the file picker button is clicked.
*/
void SoundPicker::slotPickFile()
{
    KUrl oldfile = mFile;
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of EditAlarmDlg, and on return from this function).
    AutoQPointer<SoundDlg> dlg = new SoundDlg(mFile.prettyUrl(), mVolume, mFadeVolume, mFadeSeconds, mRepeatPause, i18nc("@title:window", "Sound File"), this);
    dlg->setReadOnly(mReadOnly);
    bool accepted = (dlg->exec() == QDialog::Accepted);
    if (mReadOnly)
        return;
    if (accepted)
    {
        float volume, fadeVolume;
        int   fadeTime;
        dlg->getVolume(volume, fadeVolume, fadeTime);
        KUrl file    = dlg->getFile();
        if (!file.isEmpty())
            mFile    = file;
        mRepeatPause = dlg->repeatPause();
        mVolume      = volume;
        mFadeVolume  = fadeVolume;
        mFadeSeconds = fadeTime;
    }
    if (mFile.isEmpty())
    {
        // No audio file is selected, so revert to previously selected option
#if 0
// Remove mRevertType, setLastType(), #include QTimer
        // But wait a moment until setting the radio button, or it won't work.
        mRevertType = true;   // prevent sound dialog popping up twice
        QTimer::singleShot(0, this, SLOT(setLastType()));
#else
        mTypeCombo->setCurrentIndex(indexes[mLastType]);
#endif
        mTypeCombo->setToolTip(QString());
    }
    else
        mTypeCombo->setToolTip(mFile.prettyUrl());
    if (accepted  ||  mFile != oldfile)
        emit changed();
}

/******************************************************************************
* Select the previously selected sound type.
*/
void SoundPicker::setLastType()
{
    mTypeCombo->setCurrentIndex(indexes[mLastType]);
    mRevertType = false;
}

/******************************************************************************
* Display a dialog to choose a sound file, initially highlighting any
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
        {
            kdeSoundDir = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("sound/KDE-Sys-Warning.ogg"));
            kdeSoundDir = QFileInfo(kdeSoundDir).absolutePath();
        }
        defaultDir = kdeSoundDir;
    }
    QString filter = Phonon::BackendCapabilities::availableMimeTypes().join(QLatin1String(" "));
    return KAlarm::browseFile(i18nc("@title:window", "Choose Sound File"), defaultDir, initialFile, filter, KFile::ExistingOnly, 0);
}

#include "moc_soundpicker.cpp"

// vim: et sw=4:
