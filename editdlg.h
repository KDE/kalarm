/*
 *  editdlg.h  -  dialogue to create or modify an alarm or alarm template
 *  Program:  kalarm
 *  (C) 2001 - 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef EDITDLG_H
#define EDITDLG_H

#include <qdatetime.h>
#include <qlineedit.h>

#include <kdialogbase.h>

#include "alarmevent.h"
#include "alarmtext.h"
#include "datetime.h"

class QButton;
class QButtonGroup;
class QGroupBox;
class QComboBox;
class QTabWidget;
class QVBox;
class QHBox;
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
class SoundPicker;
class Reminder;
class SpecialActionsButton;
class RepetitionButton;
class LineEdit;
class TextEdit;


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

		static ColourCombo* createBgColourChooser(QHBox** box, QWidget* parent, const char* name = 0);
		static CheckBox*    createConfirmAckCheckbox(QWidget* parent, const char* name = 0);

		static QString  i18n_ConfirmAck();         // plain text of 'Confirm acknowledgement' checkbox
		static QString  i18n_k_ConfirmAck();       // text of 'Confirm acknowledgement' checkbox, with 'k' shortcut
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
		void            slotAlarmTypeClicked(int id);
		void            slotEditDeferral();
		void            slotBrowseFile();
		void            slotFontColourSelected();
		void            slotBgColourSelected(const QColor&);
		void            openAddressBook();
		void            slotAddAttachment();
		void            slotRemoveAttachment();
		void            slotShowMainPage();
		void            slotShowRecurrenceEdit();
		void            slotAnyTimeToggled(bool anyTime);
		void            slotTemplateTimeType(int id);
		void            slotSetSimpleRepetition();

	private:
		void            initialise(const KAEvent*);
		void            setReadOnly();
		void            setEvent(KAEvent&, const QString& text, bool trial);
		KAEvent::Action getAlarmType() const;
		int             getAlarmFlags() const;
		bool            checkText(QString& result, bool showErrorMessage = true) const;
		void            setSoundPicker();
		void            setRecurTabTitle(const KAEvent* = 0);
		bool            checkEmailData();

		void            initDisplayAlarms(QWidget* parent);
		void            initCommand(QWidget* parent);
		void            initEmail(QWidget* parent);
		void            saveState(const KAEvent*);
		bool            stateChanged() const;

		QTabWidget*         mTabs;                // the tabs in the dialog
		int                 mMainPageIndex;
		int                 mRecurPageIndex;
		bool                mMainPageShown;            // true once the main tab has been displayed
		bool                mRecurPageShown;           // true once the recurrence tab has been displayed
		bool                mRecurSetDefaultEndDate;   // adjust default end date/time when recurrence tab is displayed

		QButtonGroup*       mActionGroup;
		RadioButton*        mMessageRadio;
		RadioButton*        mCommandRadio;
		RadioButton*        mFileRadio;
		RadioButton*        mEmailRadio;
		QWidgetStack*       mAlarmTypeStack;

		// Templates
		QLineEdit*          mTemplateName;
		ButtonGroup*        mTemplateTimeGroup;
		RadioButton*        mTemplateDefaultTime; // no alarm time is specified
		RadioButton*        mTemplateAnyTime;     // alarms have date only, no time
		RadioButton*        mTemplateUseTime;     // an alarm time is specified
		TimeEdit*           mTemplateTime;        // the alarm time which is specified

		// Display alarm options widgets
		QFrame*             mDisplayAlarmsFrame;
		QHBox*              mFileBox;
		QHBox*              mFilePadding;
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
		LineEdit*           mCommandMessageEdit; // command edit box
		// Email alarm widgets
		QFrame*             mEmailFrame;
		EmailIdCombo*       mEmailFromList;
		LineEdit*           mEmailToEdit;
		QPushButton*        mEmailAddressButton; // email open address book button
		QLineEdit*          mEmailSubjectEdit;
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

		RepetitionButton*   mSimpleRepetition;
		QLabel*             mRecurrenceText;
		RecurrenceEdit*     mRecurrenceEdit;

		QString             mAlarmMessage;       // message text/file name/command/email message
		DateTime            mAlarmDateTime;
		DateTime            mDeferDateTime;
		EmailAddressList    mEmailAddresses;     // list of addresses to send email to
		QStringList         mEmailAttachments;   // list of email attachment file names
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
		QButton*            mSavedTemplateTimeType; // selected ID in mTemplateTimeGroup
		QTime               mSavedTemplateTime;   // mTemplateTime value
		QButton*            mSavedTypeRadio;      // mMessageRadio, etc
		bool                mSavedBeep;           // mSoundPicker beep status
		bool                mSavedRepeatSound;    // mSoundPicker repeat status
		QString             mSavedSoundFile;      // mSoundPicker sound file
		float               mSavedSoundVolume;    // mSoundPicker volume
		bool                mSavedConfirmAck;     // mConfirmAck status
		QFont               mSavedFont;           // mFontColourButton font
		QColor              mSavedBgColour;       // mBgColourChoose selection
		QColor              mSavedFgColour;       // mFontColourButton foreground colour
		QString             mSavedPreAction;      // mSpecialActionsButton pre-alarm action
		QString             mSavedPostAction;     // mSpecialActionsButton post-alarm action
		int                 mSavedReminder;       // mReminder value
		bool                mSavedOnceOnly;       // mReminder once-only status
		QString             mSavedTextFileCommandMessage;  // mTextMessageEdit/mFileMessageEdit/mCommandMessageEdit/mEmailMessageEdit value
		QString             mSavedEmailFrom;      // mEmailFromList current value
		QString             mSavedEmailTo;        // mEmailToEdit value
		QString             mSavedEmailSubject;   // mEmailSubjectEdit value
		QStringList         mSavedEmailAttach;    // mEmailAttachList values
		bool                mSavedEmailBcc;       // mEmailBcc status
		DateTime            mSavedDateTime;       // mTimeWidget value
		int                 mSavedRecurrenceType; // RecurrenceEdit::RepeatType value
		int                 mSavedRepeatInterval; // alarm repetition interval (via mSimpleRepetition button)
		int                 mSavedRepeatCount;    // alarm repetition count (via mSimpleRepetition button)
		int                 mSavedLateCancel;     // mLateCancel value
		bool                mSavedAutoClose;      // mLateCancel->isAutoClose() value
};

#endif // EDITDLG_H
