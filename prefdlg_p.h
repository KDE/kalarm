/*
 *  prefdlg_p.h  -  private classes for program preferences dialog
 *  Program:  kalarm
 *  Copyright © 2001-2012 by David Jarvie <djarvie@kde.org>
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

#ifndef PREFDLG_P_H
#define PREFDLG_P_H

#include "preferences.h"
#include "prefdlg.h"
#include "stackedwidgets.h"

class QCheckBox;
class QGroupBox;
class QAbstractButton;
class QRadioButton;
class QPushButton;
class QLabel;
class QSpinBox;
class KTimeZone;
class KLineEdit;
class KVBox;
class KComboBox;
class FontColourChooser;
class ColourButton;
class ButtonGroup;
class RadioButton;
class TimeEdit;
class TimePeriod;
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
        virtual void apply(bool syncToDisc) = 0;
        void         addAlignedLabel(QLabel*);
        KVBox*       topWidget() const  { return mTopWidget; }
        QVBoxLayout* topLayout() const  { return mTopLayout; }
        static int   indentWidth()      { return mIndentWidth; }

    protected:
        virtual void showEvent(QShowEvent*);

    private:
        static int   mIndentWidth;       // indent width for checkboxes etc.
        KVBox*       mTopWidget;
        QVBoxLayout* mTopLayout;
        QList<QLabel*> mLabels;          // labels to right-align
        bool           mLabelsAligned;   // labels have been aligned
};


// Miscellaneous tab of the Preferences dialog
class MiscPrefTab : public PrefsTabBase
{
        Q_OBJECT
    public:
        explicit MiscPrefTab(StackedScrollGroup*);

        virtual void restore(bool defaults, bool allTabs);
        virtual void apply(bool syncToDisc);

    private slots:
        void         slotAutostartClicked();
        void         slotOtherTerminalToggled(bool);

    private:
        void         setTimeZone(const KTimeZone&);

        QCheckBox*    mAutoStart;
        QCheckBox*    mQuitWarn;
        QCheckBox*    mConfirmAlarmDeletion;
        TimeSpinBox*  mDefaultDeferTime;
        ButtonGroup*  mXtermType;
        KLineEdit*    mXtermCommand;
        int           mXtermFirst;              // id of first terminal window radio button
        int           mXtermCount;              // number of terminal window types
};


// Date/time tab of the Preferences dialog
class TimePrefTab : public PrefsTabBase
{
        Q_OBJECT
    public:
        explicit TimePrefTab(StackedScrollGroup*);

        virtual void restore(bool defaults, bool allTabs);
        virtual void apply(bool syncToDisc);

    private:
        void         setWorkDays(const QBitArray& days);

        TimeZoneCombo* mTimeZone;
        KComboBox*    mHolidays;
        QMap<QString, QString> mHolidayNames;
        TimeEdit*     mStartOfDay;
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

        virtual void restore(bool defaults, bool allTabs);
        virtual void apply(bool syncToDisc);

    private slots:
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
        bool          mCheckKeepChanges;
};


// Email tab of the Preferences dialog
class EmailPrefTab : public PrefsTabBase
{
        Q_OBJECT
    public:
        explicit EmailPrefTab(StackedScrollGroup*);

        QString      validate();
        virtual void restore(bool defaults, bool allTabs);
        virtual void apply(bool syncToDisc);

    private slots:
        void         slotEmailClientChanged(QAbstractButton*);
        void         slotFromAddrChanged(QAbstractButton*);
        void         slotBccAddrChanged(QAbstractButton*);
        void         slotAddressChanged()    { mAddressChanged = true; }

    private:
        void         setEmailAddress(Preferences::MailFrom, const QString& address);
        void         setEmailBccAddress(bool useSystemSettings, const QString& address);
        QString      validateAddr(ButtonGroup*, KLineEdit* addr, const QString& msg);

        ButtonGroup* mEmailClient;
        RadioButton* mKMailButton;
        RadioButton* mSendmailButton;
        ButtonGroup* mFromAddressGroup;
        RadioButton* mFromAddrButton;
        RadioButton* mFromCCentreButton;
        RadioButton* mFromKMailButton;
        KLineEdit*   mEmailAddress;
        ButtonGroup* mBccAddressGroup;
        RadioButton* mBccAddrButton;
        RadioButton* mBccCCentreButton;
        KLineEdit*   mEmailBccAddress;
        QCheckBox*   mEmailQueuedNotify;
        QCheckBox*   mEmailCopyToKMail;
        bool         mAddressChanged;
        bool         mBccAddressChanged;
};


// Edit defaults tab of the Preferences dialog
class EditPrefTab : public PrefsTabBase
{
        Q_OBJECT
    public:
        explicit EditPrefTab(StackedScrollGroup*);

        QString      validate();
        virtual void restore(bool defaults, bool allTabs);
        virtual void apply(bool syncToDisc);

    private slots:
        void         slotBrowseSoundFile();

    private:
        KTabWidget*     mTabs;
        QCheckBox*      mAutoClose;
        QCheckBox*      mConfirmAck;
        KComboBox*      mReminderUnits;
        SpecialActionsButton* mSpecialActionsButton;
        QCheckBox*      mCmdScript;
        QCheckBox*      mCmdXterm;
        QCheckBox*      mEmailBcc;
        KComboBox*      mSound;
        QLabel*         mSoundFileLabel;
        KLineEdit*      mSoundFile;
        QPushButton*    mSoundFileBrowse;
        QCheckBox*      mSoundRepeat;
        QCheckBox*      mCopyToKOrganizer;
        QCheckBox*      mLateCancel;
        KComboBox*      mRecurPeriod;
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

        virtual void restore(bool defaults, bool allTabs);
        virtual void apply(bool syncToDisc);

    private slots:
        void         slotTooltipAlarmsToggled(bool);
        void         slotTooltipMaxToggled(bool);
        void         slotTooltipTimeToggled(bool);
        void         slotTooltipTimeToToggled(bool);
        void         slotAutoHideSysTrayChanged(QAbstractButton*);
        void         slotWindowPosChanged(QAbstractButton*);

    private:
        void         setTooltip(int maxAlarms, bool time, bool timeTo, const QString& prefix);

        KTabWidget*   mTabs;
        ColourButton* mDisabledColour;
        ColourButton* mArchivedColour;
        QGroupBox*    mShowInSystemTray;
        ButtonGroup*  mAutoHideSystemTray;
        TimePeriod*   mAutoHideSystemTrayPeriod;
        QCheckBox*    mTooltipShowAlarms;
        QCheckBox*    mTooltipMaxAlarms;
        SpinBox*      mTooltipMaxAlarmCount;
        QCheckBox*    mTooltipShowTime;
        QCheckBox*    mTooltipShowTimeTo;
        KLineEdit*    mTooltipTimeToPrefix;
        QLabel*       mTooltipTimeToPrefixLabel;
        ButtonGroup*  mWindowPosition;
        QSpinBox*     mWindowButtonDelay;
        QLabel*       mWindowButtonDelayLabel;
        QCheckBox*    mModalMessages;
        int           mTabGeneral;    // index of General tab
        int           mTabWindows;    // index of Alarm Windows tab
};

#endif // PREFDLG_P_H

// vim: et sw=4:
