/*
 *  sounddlg.h  -  sound file selection and configuration dialog and widget
 *  Program:  kalarm
 *  Copyright © 2005-2007,2009-2012 by David Jarvie <djarvie@kde.org>
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

#ifndef SOUNDDLG_H
#define SOUNDDLG_H

#include <kurl.h>
#include <kdialog.h>
#include <QString>

class QPushButton;
class QShowEvent;
class QResizeEvent;

namespace Phonon { class MediaObject; }
class GroupBox;
class PushButton;
class CheckBox;
class SpinBox;
class Slider;
class LineEdit;


class SoundWidget : public QWidget
{
        Q_OBJECT
    public:
        SoundWidget(bool showPlay, bool showRepeat, QWidget* parent);
        ~SoundWidget();
        void           set(const QString& file, float volume, float fadeVolume = -1, int fadeSeconds = 0, int repeatPause = -1);
        void           setReadOnly(bool);
        bool           isReadOnly() const    { return mReadOnly; }
        void           setAllowEmptyFile()   { mEmptyFileAllowed = true; }
        QString        fileName() const;
        bool           file(KUrl&, bool showErrorMessage = true) const;
        void           getVolume(float& volume, float& fadeVolume, int& fadeSeconds) const;
        int            repeatPause() const;   // -1 if none, else seconds between repeats
        QString        defaultDir() const    { return mDefaultDir; }
        bool           validate(bool showErrorMessage) const;

        static QString i18n_chk_Repeat();      // text of Repeat checkbox

    signals:
        void           changed();      // emitted whenever any contents change

    protected:
        virtual void   showEvent(QShowEvent*);
        virtual void   resizeEvent(QResizeEvent*);

    private slots:
        void           slotPickFile();
        void           slotVolumeToggled(bool on);
        void           slotFadeToggled(bool on);
        void           playSound();
        void           playFinished();

    private:
        static QString mDefaultDir;     // current default directory for mFileEdit
        QPushButton*   mFilePlay;
        LineEdit*      mFileEdit;
        PushButton*    mFileBrowseButton;
        GroupBox*      mRepeatGroupBox;
        SpinBox*       mRepeatPause;
        CheckBox*      mVolumeCheckbox;
        Slider*        mVolumeSlider;
        CheckBox*      mFadeCheckbox;
        QWidget *         mFadeBox;
        SpinBox*       mFadeTime;
        QWidget *         mFadeVolumeBox;
        Slider*        mFadeSlider;
        mutable KUrl         mUrl;
        mutable QString      mValidatedFile;
        Phonon::MediaObject* mPlayer;
        bool                 mReadOnly;
        bool                 mEmptyFileAllowed;
};


class SoundDlg : public KDialog
{
        Q_OBJECT
    public:
        SoundDlg(const QString& file, float volume, float fadeVolume, int fadeSeconds, int repeatPause,
                 const QString& caption, QWidget* parent);
        void           setReadOnly(bool);
        bool           isReadOnly() const    { return mReadOnly; }
        KUrl           getFile() const;
        void           getVolume(float& volume, float& fadeVolume, int& fadeSeconds) const
                                             { mSoundWidget->getVolume(volume, fadeVolume, fadeSeconds); }
        int            repeatPause() const   { return mSoundWidget->repeatPause(); }
        QString        defaultDir() const    { return mSoundWidget->defaultDir(); }

    protected:
        virtual void   resizeEvent(QResizeEvent*);

    protected slots:
        virtual void   slotButtonClicked(int button);

    private:
        SoundWidget*   mSoundWidget;
        bool           mReadOnly;
};

#endif

// vim: et sw=4:
