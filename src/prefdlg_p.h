/*
 *  prefdlg_p.h  -  private classes for program preferences dialog
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "preferences.h"
#include "prefdlg.h"
#include "lib/stackedwidgets.h"

class QCheckBox;
class QGroupBox;
class QAbstractButton;
class QRadioButton;
class QPushButton;
class QLabel;
class QSpinBox;
class QTimeZone;
class QLineEdit;
class QComboBox;
class QVBoxLayout;
class QTabWidget;
class FontColourChooser;
class CheckBox;
class ColourButton;
class ButtonGroup;
class RadioButton;
class TimeEdit;
class TimePeriod;
class Slider;
class SpinBox;
class TimeSpinBox;
class SpecialActionsButton;
class TimeZoneCombo;


// Base class for each tab in the Preferences dialog
class PrefsTabBase : public StackedScrollWidget
{
    Q_OBJECT
public:
    explicit PrefsTabBase(StackedScrollGroup*);

    void         setPreferences();
    virtual void restore(bool defaults, bool allTabs) = 0;
    virtual bool apply(bool syncToDisc) = 0;
    void         addAlignedLabel(QLabel*);
    QVBoxLayout* topLayout() const  { return mTopLayout; }
    static int   indentWidth()      { return mIndentWidth; }
    static int   gridIndentWidth()  { return mGridIndentWidth; }

protected:
    void showEvent(QShowEvent*) override;

private:
    static int       mIndentWidth;           // indent width for checkboxes etc.
    static int       mGridIndentWidth;       // minimum grid column indent width for checkboxes etc.
    QVBoxLayout*     mTopLayout;
    QList<QLabel*>   mLabels;                // labels to right-align
    bool             mLabelsAligned {false}; // labels have been aligned
};


// Miscellaneous tab of the Preferences dialog
class MiscPrefTab : public PrefsTabBase
{
    Q_OBJECT
public:
    explicit MiscPrefTab(StackedScrollGroup*);

    void restore(bool defaults, bool allTabs) override;
    bool apply(bool syncToDisc) override;

private Q_SLOTS:
    void         slotRunModeChanged(QAbstractButton* old, QAbstractButton* now);
    void         slotOtherTerminalToggled(bool);

private:
    void         setTimeZone(const QTimeZone&);

    ButtonGroup*  mRunMode;
    QRadioButton* mRunAuto;
    QRadioButton* mRunManual;
    QRadioButton* mRunNone;
    QCheckBox*    mQuitWarn;
    QCheckBox*    mUseAlarmNames;
    QCheckBox*    mConfirmAlarmDeletion;
    QCheckBox*    mUseAkonadi {nullptr};
    TimeSpinBox*  mDefaultDeferTime;
    ButtonGroup*  mXtermType;
    QLineEdit*    mXtermCommand;
};


// Date/time tab of the Preferences dialog
class TimePrefTab : public PrefsTabBase
{
    Q_OBJECT
public:
    explicit TimePrefTab(StackedScrollGroup*);

    void restore(bool defaults, bool allTabs) override;
    bool apply(bool syncToDisc) override;

private:
    void         setWorkDays(const QBitArray& days);

    TimeZoneCombo* mTimeZone;
    QComboBox*    mHolidays;
    QMap<QString, QString> mHolidayNames;
    TimeEdit*     mStartOfDay;
    SpinBox*      mPreWakeSuspend {nullptr};
    QCheckBox*    mWorkDays[7];
    TimeEdit*     mWorkStart;
    TimeEdit*     mWorkEnd;
    TimeSpinBox*  mKOrgEventDuration;
};


// Storage tab of the Preferences dialog
class StorePrefTab : public PrefsTabBase
{
    Q_OBJECT
public:
    explicit StorePrefTab(StackedScrollGroup*);

    void restore(bool defaults, bool allTabs) override;
    bool apply(bool syncToDisc) override;

private Q_SLOTS:
    void         slotArchivedToggled(bool);
    void         slotClearArchived();

private:
    void         setArchivedControls(int purgeDays);

    QRadioButton* mDefaultResource;
    QRadioButton* mAskResource;
    QCheckBox*    mKeepArchived;
    QCheckBox*    mPurgeArchived;
    SpinBox*      mPurgeAfter;
    QLabel*       mPurgeAfterLabel;
    QPushButton*  mClearArchived;
    bool          mOldKeepArchived;    // previous setting of keep-archived
    bool          mCheckKeepChanges {false};
};


// Email tab of the Preferences dialog
class EmailPrefTab : public PrefsTabBase
{
    Q_OBJECT
public:
    explicit EmailPrefTab(StackedScrollGroup*);

    QString      validate();
    void restore(bool defaults, bool allTabs) override;
    bool apply(bool syncToDisc) override;

protected:
    void showEvent(QShowEvent*) override;

private Q_SLOTS:
    void         slotEmailClientChanged(QAbstractButton*, QAbstractButton*);
    void         slotFromAddrChanged(QAbstractButton*, QAbstractButton*);
    void         slotBccAddrChanged(QAbstractButton*, QAbstractButton*);
    void         slotAddressChanged()    { mAddressChanged = true; }

private:
    void         setEmailAddress(Preferences::MailFrom, const QString& address);
    void         setEmailBccAddress(bool useSystemSettings, const QString& address);
    QString      validateAddr(ButtonGroup*, QLineEdit* addr, const QString& msg);

    ButtonGroup* mEmailClient;
    RadioButton* mKMailButton {nullptr};
    RadioButton* mSendmailButton;
    ButtonGroup* mFromAddressGroup;
    RadioButton* mFromAddrButton;
    RadioButton* mFromCCentreButton;
    RadioButton* mFromKMailButton;
    QLineEdit*   mEmailAddress;
    ButtonGroup* mBccAddressGroup;
    RadioButton* mBccAddrButton;
    RadioButton* mBccCCentreButton;
    QLineEdit*   mEmailBccAddress;
    QCheckBox*   mEmailQueuedNotify;
    QCheckBox*   mEmailCopyToKMail {nullptr};
    bool         mAddressChanged {false};
    bool         mBccAddressChanged {false};
};


// Edit defaults tab of the Preferences dialog
class EditPrefTab : public PrefsTabBase
{
    Q_OBJECT
public:
    explicit EditPrefTab(StackedScrollGroup*);

    QString      validate();
    void restore(bool defaults, bool allTabs) override;
    bool apply(bool syncToDisc) override;

private Q_SLOTS:
    void         slotBrowseSoundFile();

private:
    QTabWidget*     mTabs;
    QComboBox*      mDisplayMethod;
    QCheckBox*      mAutoClose;
    QCheckBox*      mConfirmAck;
    QComboBox*      mReminderUnits;
    SpecialActionsButton* mSpecialActionsButton;
    QCheckBox*      mCmdScript;
    QCheckBox*      mCmdXterm;
    QCheckBox*      mEmailBcc;
    QComboBox*      mSound;
    QLabel*         mSoundFileLabel;
    QLineEdit*      mSoundFile;
    QPushButton*    mSoundFileBrowse;
    CheckBox*       mSoundVolumeCheckbox;
    Slider*         mSoundVolumeSlider;
    QCheckBox*      mSoundRepeat;
    QCheckBox*      mCopyToKOrganizer;
    QCheckBox*      mLateCancel;
    ComboBox*       mRecurPeriod;
    ButtonGroup*    mFeb29;
    FontColourChooser* mFontChooser;
    int             mTabGeneral;     // index of General tab
    int             mTabTypes;       // index of Alarm Types tab
    int             mTabFontColour;  // index of Font & Color tab

    static int soundIndex(Preferences::SoundType);
};


// View tab of the Preferences dialog
class ViewPrefTab : public PrefsTabBase
{
    Q_OBJECT
public:
    explicit ViewPrefTab(StackedScrollGroup*);

    void restore(bool defaults, bool allTabs) override;
    bool apply(bool syncToDisc) override;

private Q_SLOTS:
    void         slotTooltipAlarmsToggled(bool);
    void         slotTooltipMaxToggled(bool);
    void         slotTooltipTimeToggled(bool);
    void         slotTooltipTimeToToggled(bool);
    void         slotAutoHideSysTrayChanged(QAbstractButton*, QAbstractButton*);
    void         slotWindowPosChanged(QAbstractButton*, QAbstractButton*);

private:
    void         setTooltip(int maxAlarms, bool time, bool timeTo, const QString& prefix);

    QTabWidget*   mTabs;
    ColourButton* mDisabledColour;
    ColourButton* mArchivedColour;
    QCheckBox*    mShowInSystemTrayCheck {nullptr};
    QGroupBox*    mShowInSystemTrayGroup {nullptr};
    ButtonGroup*  mAutoHideSystemTray {nullptr};
    TimePeriod*   mAutoHideSystemTrayPeriod {nullptr};
    QCheckBox*    mTooltipShowAlarms;
    QCheckBox*    mTooltipMaxAlarms;
    SpinBox*      mTooltipMaxAlarmCount;
    QCheckBox*    mTooltipShowTime;
    QCheckBox*    mTooltipShowTimeTo;
    QLineEdit*    mTooltipTimeToPrefix;
    QLabel*       mTooltipTimeToPrefixLabel;
    ButtonGroup*  mWindowPosition {nullptr};
    QSpinBox*     mWindowButtonDelay;
    QLabel*       mWindowButtonDelayLabel;
    QCheckBox*    mModalMessages {nullptr};
    int           mTabGeneral;    // index of General tab
    int           mTabWindows;    // index of Alarm Windows tab
};

// vim: et sw=4:
