/*
 *  soundpicker.cpp  -  widget to select a sound file or a beep
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "soundpicker.h"

#include "sounddlg.h"
#include "lib/autoqpointer.h"
#include "lib/combobox.h"
#include "lib/file.h"
#include "lib/pushbutton.h"

#include <kpimtextedit/kpimtextedit-texttospeech.h>
#if KPIMTEXTEDIT_TEXT_TO_SPEECH
#include <KPIMTextEdit/TextToSpeech>
#endif

#include <KLocalizedString>
#include <phonon/backendcapabilities.h>

#include <QIcon>
#include <QFileInfo>
#include <QTimer>
#include <QLabel>
#include <QHBoxLayout>
#include <QStandardPaths>
#include <QMimeDatabase>

//clazy:excludeall=non-pod-global-static

namespace
{
const int NoneIndex = 0;
const int FileIndex = 2;
}

// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString SoundPicker::i18n_label_Sound()   { return i18nc("@label:listbox Listbox providing audio options", "Sound:"); }
QString SoundPicker::i18n_combo_None()    { return i18nc("@item:inlistbox No sound", "None"); }
QString SoundPicker::i18n_combo_Beep()    { return i18nc("@item:inlistbox", "Beep"); }
QString SoundPicker::i18n_combo_Speak()   { return i18nc("@item:inlistbox", "Speak"); }
QString SoundPicker::i18n_combo_File()    { return i18nc("@item:inlistbox", "Sound file"); }


SoundPicker::SoundPicker(QWidget* parent)
    : QFrame(parent)
{
    auto soundLayout = new QHBoxLayout(this);
    soundLayout->setContentsMargins(0, 0, 0, 0);
    mTypeBox = new QWidget(this);    // this is to control the QWhatsThis text display area
    auto typeBoxLayout = new QHBoxLayout(mTypeBox);
    typeBoxLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* label = new QLabel(i18n_label_Sound(), mTypeBox);
    typeBoxLayout->addWidget(label);

    // Sound type combo box
    mTypeCombo = new ComboBox(mTypeBox);
    typeBoxLayout->addWidget(mTypeCombo);
    mTypeCombo->addItem(i18n_combo_None(), Preferences::Sound_None);     // index None
    mTypeCombo->addItem(i18n_combo_Beep(), Preferences::Sound_Beep);     // index Beep
    mTypeCombo->addItem(i18n_combo_File(), Preferences::Sound_File);     // index PlayFile
    mTypeCombo->addItem(i18n_combo_Speak(), Preferences::Sound_Speak);   // index Speak (only displayed if appropriate)
    mFileShowing = true;
    mSpeakShowing = true;
    showSpeak(true);
    connect(mTypeCombo, &ComboBox::activated, this, &SoundPicker::slotTypeSelected);
    connect(mTypeCombo, static_cast<void (ComboBox::*)(int)>(&ComboBox::currentIndexChanged), this, &SoundPicker::changed);
    label->setBuddy(mTypeCombo);
    soundLayout->addWidget(mTypeBox);
    mTypeBox->setWhatsThis(xi18nc("@info:whatsthis",
                                  "<para>Choose a sound to play when the message is displayed."
                                  "<list><item><interface>%1</interface>: the message is displayed silently.</item>"
                                  "<item><interface>%2</interface>: a simple beep is sounded.</item>"
                                  "<item><interface>%3</interface>: an audio file is played. You will be prompted to choose the file and set play options. (Option not available if using notification.)</item>"
                                  "<item><interface>%4</interface>: the message text is spoken. (Option requires working Qt text-to-speech installation.)</item>"
                                  "</list></para>",
                                  i18n_combo_None(), i18n_combo_Beep(), i18n_combo_File(), i18n_combo_Speak()));

    // Sound file picker button
    mFilePicker = new PushButton(this);
    mFilePicker->setIcon(QIcon::fromTheme(QStringLiteral("audio-x-generic")));
    connect(mFilePicker, &PushButton::clicked, this, &SoundPicker::slotPickFile);
    mFilePicker->setToolTip(i18nc("@info:tooltip", "Configure sound file"));
    mFilePicker->setWhatsThis(i18nc("@info:whatsthis", "Configure a sound file to play when the alarm is displayed."));
    soundLayout->addWidget(mFilePicker);

    // Initialise the file picker button state and tooltip
    mTypeCombo->setCurrentIndex(NoneIndex);
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
* Show or hide the File option.
*/
void SoundPicker::showFile(bool show)
{
    if (show != mFileShowing)
    {
        if (show)
            mTypeCombo->insertItem(FileIndex, i18n_combo_File(), Preferences::Sound_File);
        else
        {
            if (mTypeCombo->currentData().toInt() == Preferences::Sound_File)
                mTypeCombo->setCurrentIndex(NoneIndex);
            mTypeCombo->removeItem(FileIndex);
        }
        mFilePicker->setVisible(show);
        mFileShowing = show;
    }
}

/******************************************************************************
* Show or hide the Speak option.
*/
void SoundPicker::showSpeak(bool show)
{
#if KPIMTEXTEDIT_TEXT_TO_SPEECH
    if (!KPIMTextEdit::TextToSpeech::self()->isReady())
#endif
        show = false;    // speech capability is not installed or configured
    if (show != mSpeakShowing)
    {
        // Note that 'Speak' is always the last option.
        if (show)
            mTypeCombo->addItem(i18n_combo_Speak(), Preferences::Sound_Speak);
        else
        {
            if (mTypeCombo->currentData().toInt() == Preferences::Sound_Speak)
                mTypeCombo->setCurrentIndex(NoneIndex);
            mTypeCombo->removeItem(mTypeCombo->count() - 1);
        }
        mSpeakShowing = show;
    }
}

/******************************************************************************
* Return the currently selected option.
*/
Preferences::SoundType SoundPicker::sound() const
{
    if (mTypeCombo->currentIndex() < 0)
        return Preferences::Sound_None;
#if !KPIMTEXTEDIT_TEXT_TO_SPEECH
    if (mTypeCombo->currentData().toInt() == Preferences::Sound_Speak)
        return Preferences::Sound_None;
#endif
    return static_cast<Preferences::SoundType>(mTypeCombo->currentData().toInt());
}

/******************************************************************************
* Return the selected sound file, if the File option is selected.
* Returns empty URL if File is not currently selected.
*/
QUrl SoundPicker::file() const
{
    return (mTypeCombo->currentData().toInt() == Preferences::Sound_File) ? mFile : QUrl();
}

/******************************************************************************
* Return the specified volumes (range 0 - 1).
* Returns < 0 if beep is currently selected, or if 'set volume' is not selected.
*/
float SoundPicker::volume(float& fadeVolume, int& fadeSeconds) const
{
    if (mTypeCombo->currentData().toInt() == Preferences::Sound_File  &&  !mFile.isEmpty())
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
    return mTypeCombo->currentData().toInt() == Preferences::Sound_File  &&  !mFile.isEmpty() ? mRepeatPause : -1;
}

/******************************************************************************
* Initialise the widget's state.
*/
void SoundPicker::set(Preferences::SoundType type, const QString& f, float volume, float fadeVolume, int fadeSeconds, int repeatPause)
{
    if (type == Preferences::Sound_File  &&  f.isEmpty())
        type = Preferences::Sound_Beep;
    mFile        = QUrl::fromUserInput(f, QString(), QUrl::AssumeLocalFile);
    mVolume      = volume;
    mFadeVolume  = fadeVolume;
    mFadeSeconds = fadeSeconds;
    mRepeatPause = repeatPause;
    selectType(type);  // this doesn't trigger slotTypeSelected()
    mFilePicker->setEnabled(type == Preferences::Sound_File);
    mTypeCombo->setToolTip(type == Preferences::Sound_File ? mFile.toDisplayString() : QString());
    mLastType = type;
}

/******************************************************************************
* Called when the sound option is changed.
*/
void SoundPicker::slotTypeSelected(int id)
{
    const Preferences::SoundType newType = (id >= 0) ? static_cast<Preferences::SoundType>(mTypeCombo->itemData(id).toInt()) : Preferences::Sound_None;
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
        mTypeCombo->setToolTip(mFile.toDisplayString());
    }
    mLastType = newType;
}

/******************************************************************************
* Called when the file picker button is clicked.
*/
void SoundPicker::slotPickFile()
{
    QUrl oldfile = mFile;
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of EditAlarmDlg, and on return from this function).
    AutoQPointer<SoundDlg> dlg = new SoundDlg(mFile.toDisplayString(), mVolume, mFadeVolume, mFadeSeconds, mRepeatPause, i18nc("@title:window", "Sound File"), this);
    dlg->setReadOnly(mReadOnly);
    bool accepted = (dlg->exec() == QDialog::Accepted);
    if (mReadOnly)
        return;
    if (accepted)
    {
        float volume, fadeVolume;
        int   fadeTime;
        dlg->getVolume(volume, fadeVolume, fadeTime);
        QUrl file    = dlg->getFile();
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
        QTimer::singleShot(0, this, &SoundPicker::setLastType);
#else
        selectType(mLastType);
#endif
        mTypeCombo->setToolTip(QString());
    }
    else
        mTypeCombo->setToolTip(mFile.toDisplayString());
    if (accepted  ||  mFile != oldfile)
        Q_EMIT changed();
}

/******************************************************************************
* Select the previously selected sound type.
*/
void SoundPicker::setLastType()
{
    selectType(mLastType);
    mRevertType = false;
}

/******************************************************************************
* Display a dialog to choose a sound file, initially highlighting any
* specified file. 'initialFile' must be a full path name or URL.
* 'defaultDir' is updated to the directory containing the chosen file.
* @param file  Updated to URL selected, or empty if none is selected.
* Reply = true if @p file value can be used,
*       = false if the dialog was deleted while visible.
*/
bool SoundPicker::browseFile(QString& file, QString& defaultDir, const QString& initialFile)
{
    static QString audioFilter;
    if (audioFilter.isEmpty())
    {
        audioFilter = QStringLiteral("%1 (").arg(i18nc("@item:inlistbox", "Audio files"));
        QMimeDatabase db;
        const QStringList mimeTypes = Phonon::BackendCapabilities::availableMimeTypes();
        for (const QString& mimeType : mimeTypes)
            if (mimeType.startsWith(QStringLiteral("audio/")))
            {
                const QMimeType mt = db.mimeTypeForName(mimeType);
                audioFilter += mt.globPatterns().join(QLatin1Char(' '));
                audioFilter += QLatin1Char(' ');
            }
        audioFilter[audioFilter.length() - 1] = QLatin1Char(')');
    }

    static QString kdeSoundDir;     // directory containing KDE sound files
    if (defaultDir.isEmpty())
    {
        if (kdeSoundDir.isNull())
        {
            kdeSoundDir = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("sounds"), QStandardPaths::LocateDirectory);
            kdeSoundDir = QFileInfo(kdeSoundDir).absoluteFilePath();
        }
        defaultDir = kdeSoundDir;
    }

    return File::browseFile(file, i18nc("@title:window", "Choose Sound File"), defaultDir,
                            audioFilter, initialFile, true, nullptr);
}

/******************************************************************************
* Select the item corresponding to a given sound type.
*/
void SoundPicker::selectType(Preferences::SoundType type)
{
    int i = mTypeCombo->findData(type);
    if (i >= 0)
        mTypeCombo->setCurrentIndex(i);
}

// vim: et sw=4:
