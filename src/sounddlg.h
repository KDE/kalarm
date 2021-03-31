/*
 *  sounddlg.h  -  sound file selection and configuration dialog and widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2005-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QUrl>
#include <QDialog>
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
class QDialogButtonBox;
class QAbstractButton;


class SoundWidget : public QWidget
{
    Q_OBJECT
public:
    SoundWidget(bool showPlay, bool showRepeat, QWidget* parent);
    ~SoundWidget() override;
    void           set(const QString& file, float volume, float fadeVolume = -1, int fadeSeconds = 0, int repeatPause = -1);
    void           setReadOnly(bool);
    bool           isReadOnly() const    { return mReadOnly; }
    void           setAllowEmptyFile()   { mEmptyFileAllowed = true; }
    QString        fileName() const;
    bool           file(QUrl&, bool showErrorMessage = true) const;
    void           getVolume(float& volume, float& fadeVolume, int& fadeSeconds) const;
    int            repeatPause() const;   // -1 if none, else seconds between repeats
    QString        defaultDir() const    { return mDefaultDir; }
    bool           validate(bool showErrorMessage) const;

    static QString i18n_chk_Repeat();      // text of Repeat checkbox
    static QString i18n_chk_SetVolume();   // text of Set volume checkbox

Q_SIGNALS:
    void           changed();      // emitted whenever any contents change

protected:
    void           showEvent(QShowEvent*) override;
    void           resizeEvent(QResizeEvent*) override;

private Q_SLOTS:
    void           slotPickFile();
    void           slotVolumeToggled(bool on);
    void           slotFadeToggled(bool on);
    void           playSound();
    void           playFinished();

private:
    static QString       mDefaultDir;     // current default directory for mFileEdit
    QPushButton*         mFilePlay {nullptr};
    LineEdit*            mFileEdit;
    PushButton*          mFileBrowseButton;
    GroupBox*            mRepeatGroupBox {nullptr};
    SpinBox*             mRepeatPause {nullptr};
    CheckBox*            mVolumeCheckbox;
    Slider*              mVolumeSlider;
    CheckBox*            mFadeCheckbox;
    QWidget*             mFadeBox;
    SpinBox*             mFadeTime;
    QWidget*             mFadeVolumeBox;
    Slider*              mFadeSlider;
    mutable QUrl         mUrl;
    mutable QString      mValidatedFile;
    Phonon::MediaObject* mPlayer {nullptr};
    bool                 mReadOnly {false};
    bool                 mEmptyFileAllowed {false};
};


class SoundDlg : public QDialog
{
    Q_OBJECT
public:
    SoundDlg(const QString& file, float volume, float fadeVolume, int fadeSeconds, int repeatPause,
             const QString& caption, QWidget* parent);
    void    setReadOnly(bool);
    bool    isReadOnly() const    { return mReadOnly; }
    QUrl    getFile() const;
    void    getVolume(float& volume, float& fadeVolume, int& fadeSeconds) const
                                  { mSoundWidget->getVolume(volume, fadeVolume, fadeSeconds); }
    int     repeatPause() const   { return mSoundWidget->repeatPause(); }
    QString defaultDir() const    { return mSoundWidget->defaultDir(); }

protected:
    void    resizeEvent(QResizeEvent*) override;

protected Q_SLOTS:
    void    slotButtonClicked(QAbstractButton*);

private:
    SoundWidget*      mSoundWidget;
    QDialogButtonBox* mButtonBox;
    bool              mReadOnly {false};
};


// vim: et sw=4:
