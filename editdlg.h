/*
 *  editdlg.h  -  dialogue to create or modify an alarm or alarm template
 *  Program:  kalarm
 *  Copyright (c) 2001 - 2005 by David Jarvie <software@astrojar.org.uk>
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

#ifndef EDITDLG_H
#define EDITDLG_H

#include <qdatetime.h>

#include <kdialogbase.h>

#include "alarmevent.h"
#include "alarmtext.h"
#include "datetime.h"
#include "soundpicker.h"

class QStackedWidget;
class QFrame;
class QLabel;
class QShowEvent;
class QResizeEvent;
class QAbstractButton;
class QGroupBox;
class QLineEdit;
class QComboBox;
class QTabWidget;
class KHBox;
class EmailIdCombo;
class ColourCombo;
class FontColourButton;
class ButtonGroup;
class TimeEdit;
class RadioButton;
class CheckBox;
class LateCancelSelector;
class AlarmTimeWidget;
class RecurrenceEdit;
class Reminder;
class SpecialActionsButton;
class RepetitionButton;
class TimeSpinBox;
class LineEdit;
class TextEdit;
class PickAlarmFileRadio;


class EditAlarmDlg : public KDialogBase
{
		Q_OBJECT
	public:
		enum MessageType { MESSAGE, FILE };

		EditAlarmDlg(bool Template, const QString& caption, QWidget* parent = 0, const char* name = 0,
                     const KAEvent* = 0, bool readOnly = false);
		virtual ~EditAlarmDlg();
		bool            getEvent(KAEvent&);
		void            setAction(KAEvent::Action, const AlarmText& = AlarmText());

		static ColourCombo* createBgColourChooser(KHBox** box, QWidget* parent, const char* name = 0);
		static CheckBox*    createConfirmAckCheckbox(QWidget* parent);

		static QString  i18n_ConfirmAck();         // plain text of 'Confirm acknowledgement' checkbox
		static QString  i18n_k_ConfirmAck();       // text of 'Confirm acknowledgement' checkbox, with 'k' shortcut
		static QString  i18n_ShowInKOrganizer();   // plain text of 'Show in KOrganizer' checkbox
		static QString  i18n_g_ShowInKOrganizer(); // text of 'Show in KOrganizer' checkbox, with 'G' shortcut
		static QString  i18n_EnterScript();        // plain text of 'Enter a script' checkbox
		static QString  i18n_p_EnterScript();      // text of 'Enter a script' checkbox, with 'P' shortcut
		static QString  i18n_ExecInTermWindow();   // plain text of 'Execute in terminal window' checkbox
		static QString  i18n_w_ExecInTermWindow(); // text of 'Execute in terminal window' radio button, with 'W' shortcut
		static QString  i18n_u_ExecInTermWindow(); // text of 'Execute in terminal window' radio button, with 'U' shortcut
		static QString  i18n_g_LogToFile();        // text of 'Log to file' radio button, with 'G' shortcut
		static QString  i18n_CopyEmailToSelf();    // plain text of 'Copy email to self' checkbox
		static QString  i18n_e_CopyEmailToSelf();  // text of 'Copy email to self' checkbox, with 'E' shortcut
		static QString  i18n_s_CopyEmailToSelf();  // text of 'Copy email to self' checkbox, with 'S' shortcut
		static QString  i18n_EmailFrom();          // plain text of 'From:' (email address)
		static QString  i18n_f_EmailFrom();        // text of 'From:' (email address), with 'F' shortcut
		static QString  i18n_EmailTo();            // plain text of 'To:' (email addressee)
		static QString  i18n_EmailSubject();       // plain text of 'Subject:' (email)
		static QString  i18n_j_EmailSubject();     // text of 'Subject:' (email), with 'J' shortcut

	protected:
		virtual void    resizeEvent(QResizeEvent*);
		virtual void    showEvent(QShowEvent*);
	protected slots:
		virtual void    slotOk();
		virtual void    slotCancel();
		virtual void    slotTry();
		virtual void    slotDefault();   // Load Template
	private slots:
		void            slotRecurTypeChange(int repeatType);
		void            slotRecurFrequencyChange();
		void            slotAlarmTypeChanged(QAbstractButton*);
		void            slotEditDeferral();
		void            slotFontColourSelected();
		void            slotBgColourSelected(const QColor&);
		void            openAddressBook();
		void            slotAddAttachment();
		void            slotRemoveAttachment();
		void            slotShowMainPage();
		void            slotShowRecurrenceEdit();
		void            slotAnyTimeToggled(bool anyTime);
		void            slotTemplateTimeType(QAbstractButton*);
		void            slotSetSimpleRepetition();
		void            slotCmdScriptToggled(bool);

	private:
		void            initialise(const KAEvent*);
		void            setReadOnly();
		void            setEvent(KAEvent&, const QString& text, bool trial);
		KAEvent::Action getAlarmType() const;
		int             getAlarmFlags() const;
		bool            checkText(QString& result, bool showErrorMessage = true) const;
		void            setSoundPicker();
		void            setRecurTabTitle(const KAEvent* = 0);
		bool            checkCommandData();
		bool            checkEmailData();

		void            initDisplayAlarms();
		void            initCommand();
		void            initEmail();
		void            saveState(const KAEvent*);
		bool            stateChanged() const;

		QTabWidget*         mTabs;                // the tabs in the dialog
		int                 mMainPageIndex;
		int                 mRecurPageIndex;
		bool                mMainPageShown;            // true once the main tab has been displayed
		bool                mRecurPageShown;           // true once the recurrence tab has been displayed
		bool                mRecurSetDefaultEndDate;   // adjust default end date/time when recurrence tab is displayed

		ButtonGroup*        mActionGroup;
		RadioButton*        mMessageRadio;
		RadioButton*        mCommandRadio;
		PickAlarmFileRadio* mFileRadio;
		RadioButton*        mEmailRadio;
		QStackedWidget*     mAlarmTypeStack;

		// Templates
		QLineEdit*          mTemplateName;
		ButtonGroup*        mTemplateTimeGroup;
		RadioButton*        mTemplateDefaultTime; // no alarm time is specified
		RadioButton*        mTemplateUseTimeAfter;// alarm time is specified as an offset from current
		RadioButton*        mTemplateAnyTime;     // alarms have date only, no time
		RadioButton*        mTemplateUseTime;     // an alarm time is specified
		TimeSpinBox*        mTemplateTimeAfter;   // the specified offset from the current time
		TimeEdit*           mTemplateTime;        // the alarm time which is specified

		// Display alarm options widgets
		QFrame*             mDisplayAlarmsFrame;
		KHBox*              mFileBox;
		KHBox*              mFilePadding;
		SoundPicker*        mSoundPicker;
		CheckBox*           mConfirmAck;
		FontColourButton*   mFontColourButton;
		ColourCombo*        mBgColourChoose;
		SpecialActionsButton* mSpecialActionsButton;
		Reminder*           mReminder;
		bool                mReminderDeferral;
		bool                mReminderArchived;
		// Text message alarm widgets
		TextEdit*           mTextMessageEdit;    // text message edit box
		// Text file alarm widgets
		LineEdit*           mFileMessageEdit;    // text file URL edit box
		QPushButton*        mFileBrowseButton;   // text file browse button
		QString             mFileDefaultDir;     // default directory for browse button
		// Command alarm widgets
		QFrame*             mCommandFrame;
		CheckBox*           mCmdTypeScript;      // entering a script
		LineEdit*           mCmdCommandEdit;     // command line edit box
		TextEdit*           mCmdScriptEdit;      // script edit box
		ButtonGroup*        mCmdOutputGroup;     // what to do with command output
		RadioButton*        mCmdExecInTerm;
		RadioButton*        mCmdLogToFile;
		RadioButton*        mCmdDiscardOutput;
		LineEdit*           mCmdLogFileEdit;     // log file URL edit box
		KHBox*              mCmdPadding;
		// Email alarm widgets
		QFrame*             mEmailFrame;
		EmailIdCombo*       mEmailFromList;
		LineEdit*           mEmailToEdit;
		QPushButton*        mEmailAddressButton; // email open address book button
		LineEdit*           mEmailSubjectEdit;
		TextEdit*           mEmailMessageEdit;   // email body edit box
		QComboBox*          mEmailAttachList;
		QPushButton*        mEmailAddAttachButton;
		QPushButton*        mEmailRemoveButton;
		CheckBox*           mEmailBcc;
		QString             mAttachDefaultDir;

		QGroupBox*          mDeferGroup;
		QLabel*             mDeferTimeLabel;
		QPushButton*        mDeferChangeButton;

		AlarmTimeWidget*    mTimeWidget;
		LateCancelSelector* mLateCancel;
		CheckBox*           mShowInKorganizer;

		RepetitionButton*   mSimpleRepetition;
		QLabel*             mRecurrenceText;
		RecurrenceEdit*     mRecurrenceEdit;

		QString             mAlarmMessage;       // message text/file name/command/email message
		DateTime            mAlarmDateTime;
		DateTime            mDeferDateTime;
		EmailAddressList    mEmailAddresses;     // list of addresses to send email to
		QStringList         mEmailAttachments;   // list of email attachment file names
		unsigned long       mKMailSerialNumber;  // if email text, message's KMail serial number, else 0
		int                 mDeferGroupHeight;   // height added by deferred time widget
		int                 mDesktop;            // desktop to display the dialog in
		bool                mTemplate;           // editing an alarm template
		bool                mExpiredRecurrence;  // initially a recurrence which has expired
		mutable bool        mChanged;            // controls other than deferral have changed since dialog was displayed
		mutable bool        mOnlyDeferred;       // the only change made in the dialog was to the existing deferral
		bool                mDesiredReadOnly;    // the specified read-only status of the dialogue
		bool                mReadOnly;           // the actual read-only status of the dialogue

		// Initial state of all controls
		KAEvent*            mSavedEvent;
		QString             mSavedTemplateName;   // mTemplateName value
		QAbstractButton*    mSavedTemplateTimeType; // selected button in mTemplateTimeGroup
		QTime               mSavedTemplateTime;   // mTemplateTime value
		int                 mSavedTemplateAfterTime; // mTemplateAfterTime value
		QAbstractButton*    mSavedTypeRadio;      // mMessageRadio, etc
		SoundPicker::Type   mSavedSoundType;      // mSoundPicker sound type
		bool                mSavedSound;          // mSoundPicker sound status
		bool                mSavedRepeatSound;    // mSoundPicker repeat status
		QString             mSavedSoundFile;      // mSoundPicker sound file
		float               mSavedSoundVolume;    // mSoundPicker volume
		float               mSavedSoundFadeVolume;// mSoundPicker fade volume
		int                 mSavedSoundFadeSeconds;// mSoundPicker fade time
		bool                mSavedConfirmAck;     // mConfirmAck status
		QFont               mSavedFont;           // mFontColourButton font
		QColor              mSavedBgColour;       // mBgColourChoose selection
		QColor              mSavedFgColour;       // mFontColourButton foreground colour
		QString             mSavedPreAction;      // mSpecialActionsButton pre-alarm action
		QString             mSavedPostAction;     // mSpecialActionsButton post-alarm action
		int                 mSavedReminder;       // mReminder value
		bool                mSavedOnceOnly;       // mReminder once-only status
		QString             mSavedTextFileCommandMessage;  // mTextMessageEdit/mFileMessageEdit/mCmdCommandEdit/mEmailMessageEdit value
		QString             mSavedEmailFrom;      // mEmailFromList current value
		QString             mSavedEmailTo;        // mEmailToEdit value
		QString             mSavedEmailSubject;   // mEmailSubjectEdit value
		QStringList         mSavedEmailAttach;    // mEmailAttachList values
		bool                mSavedEmailBcc;       // mEmailBcc status
		bool                mSavedCmdScript;      // mCmdTypeScript status
		QAbstractButton*    mSavedCmdOutputRadio; // selected button in mCmdOutputGroup
		QString             mSavedCmdLogFile;     // mCmdLogFileEdit value
		DateTime            mSavedDateTime;       // mTimeWidget value
		int                 mSavedRecurrenceType; // RecurrenceEdit::RepeatType value
		int                 mSavedRepeatInterval; // alarm repetition interval (via mSimpleRepetition button)
		int                 mSavedRepeatCount;    // alarm repetition count (via mSimpleRepetition button)
		int                 mSavedLateCancel;     // mLateCancel value
		bool                mSavedAutoClose;      // mLateCancel->isAutoClose() value
		bool                mSavedShowInKorganizer; // mShowInKorganizer status
};

#endif // EDITDLG_H
