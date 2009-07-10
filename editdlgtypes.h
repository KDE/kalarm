/*
 *  editdlgtypes.h  -  dialogues to create or edit alarm or alarm template types
 *  Program:  kalarm
 *  Copyright © 2001-2009 by David Jarvie <djarvie@kde.org>
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

#ifndef EDITDLGTYPES_H
#define EDITDLGTYPES_H

#include <kurl.h>

#include "alarmevent.h"
#include "alarmtext.h"
#include "editdlg.h"

class QAbstractButton;
class QHBoxLayout;
class QGroupBox;
class KComboBox;
class KHBox;
class EmailIdCombo;
class CheckBox;
class ComboBox;
class FontColourButton;
class ColourButton;
class ButtonGroup;
class RadioButton;
class Reminder;
class SoundPicker;
class SpecialActionsButton;
class CommandEdit;
class LineEdit;
class TextEdit;


class EditDisplayAlarmDlg : public EditAlarmDlg
{
		Q_OBJECT
	public:
//		enum MessageType { TEXT, FILE, COMMAND };

		EditDisplayAlarmDlg(bool Template, bool newAlarm, QWidget* parent = 0,
		             GetResourceType = RES_PROMPT);
		EditDisplayAlarmDlg(bool Template, const KAEvent*, bool newAlarm, QWidget* parent = 0,
		             GetResourceType = RES_PROMPT, bool readOnly = false);

		// Methods to initialise values in the New Alarm dialogue.
		// N.B. setTime() must be called first to set the date-only characteristic,
		//      followed by setRecurrence().
		virtual void    setAction(KAEventData::Action, const AlarmText& = AlarmText());
		void            setBgColour(const QColor&);
		void            setFgColour(const QColor&);
		void            setConfirmAck(bool);
		void            setAutoClose(bool);
		void            setAudio(Preferences::SoundType, const QString& file = QString(), float volume = -1, bool repeat = false);
		void            setReminder(int minutes);   // minutes < 0 for once only option

		virtual Reminder* createReminder(QWidget* parent);
		static CheckBox*  createConfirmAckCheckbox(QWidget* parent);

		static QString  i18n_chk_ConfirmAck();    // text of 'Confirm acknowledgement' checkbox

	protected:
		virtual QString type_caption(bool newAlarm) const;
		virtual void    type_init(QWidget* parent, QVBoxLayout* frameLayout);
		virtual void    type_initValues(const KAEvent*);
		virtual void    type_showOptions(bool more);
		virtual void    setReadOnly(bool readOnly);
		virtual void    saveState(const KAEvent*);
		virtual bool    type_stateChanged() const;
		virtual void    type_setEvent(KAEvent&, const KDateTime&, const QString& text, int lateCancel, bool trial);
		virtual int     getAlarmFlags() const;
		virtual bool    type_validate(bool trial) { Q_UNUSED(trial); return true; }
		virtual void    type_trySuccessMessage(ShellProcess*, const QString&)  {}
		virtual CheckBox* type_createConfirmAckCheckbox(QWidget* parent)  { mConfirmAck = createConfirmAckCheckbox(parent); return mConfirmAck; }
		virtual bool    checkText(QString& result, bool showErrorMessage = true) const;

	private slots:
		void            slotAlarmTypeChanged(int index);
		void            slotPickFile();
		void            slotCmdScriptToggled(bool);
		void            setColours(const QColor& fg, const QColor& bg);

	private:
		void            setSoundPicker();

		// Display alarm options
		ComboBox*           mTypeCombo;
		KHBox*              mFileBox;
		KHBox*              mFilePadding;
		SoundPicker*        mSoundPicker;
		CheckBox*           mConfirmAck;
		FontColourButton*   mFontColourButton;
		SpecialActionsButton* mSpecialActionsButton;
		unsigned long       mKMailSerialNumber;  // if email text, message's KMail serial number, else 0
		bool                mReminderDeferral;
		bool                mReminderArchived;
		// Text message alarm widgets
		TextEdit*           mTextMessageEdit;    // text message edit box
		// Text file alarm widgets
		LineEdit*           mFileMessageEdit;    // text file URL edit box
		QPushButton*        mFileBrowseButton;   // text file browse button
		QString             mFileDefaultDir;     // default directory for browse button
		// Command output alarm widgets
		CommandEdit*        mCmdEdit;

		// Initial state of all controls
		int                 mSavedType;             // mTypeCombo index
		Preferences::SoundType mSavedSoundType;     // mSoundPicker sound type
		bool                mSavedSound;            // mSoundPicker sound status
		bool                mSavedRepeatSound;      // mSoundPicker repeat status
		KUrl                mSavedSoundFile;        // mSoundPicker sound file
		float               mSavedSoundVolume;      // mSoundPicker volume
		float               mSavedSoundFadeVolume;  // mSoundPicker fade volume
		int                 mSavedSoundFadeSeconds; // mSoundPicker fade time
		bool                mSavedCmdScript;        // mCmdEdit->isScript() status
		bool                mSavedConfirmAck;       // mConfirmAck status
		QFont               mSavedFont;             // mFontColourButton font
		QColor              mSavedBgColour;         // mBgColourChoose selection
		QColor              mSavedFgColour;         // mFontColourButton foreground colour
		QString             mSavedPreAction;        // mSpecialActionsButton pre-alarm action
		QString             mSavedPostAction;       // mSpecialActionsButton post-alarm action
		int                 mSavedReminder;         // mReminder value
		bool                mSavedAutoClose;        // mLateCancel->isAutoClose() value
		bool                mSavedOnceOnly;         // mReminder once-only status
		bool                mSavedPreActionCancel;  // mSpecialActionsButton cancel on pre-alarm action error
};


class EditCommandAlarmDlg : public EditAlarmDlg
{
		Q_OBJECT
	public:
		EditCommandAlarmDlg(bool Template, bool newAlarm, QWidget* parent = 0,
		                    GetResourceType = RES_PROMPT);
		EditCommandAlarmDlg(bool Template, const KAEvent*, bool newAlarm, QWidget* parent = 0,
		                    GetResourceType = RES_PROMPT, bool readOnly = false);

		// Methods to initialise values in the New Alarm dialogue.
		// N.B. setTime() must be called first to set the date-only characteristic,
		//      followed by setRecurrence().
		virtual void    setAction(KAEventData::Action, const AlarmText& = AlarmText());

		static QString  i18n_chk_EnterScript();        // text of 'Enter a script' checkbox
		static QString  i18n_radio_ExecInTermWindow(); // text of 'Execute in terminal window' radio button
		static QString  i18n_chk_ExecInTermWindow();   // text of 'Execute in terminal window' checkbox

	protected:
		virtual QString type_caption(bool newAlarm) const;
		virtual void    type_init(QWidget* parent, QVBoxLayout* frameLayout);
		virtual void    type_initValues(const KAEvent*);
		virtual void    type_showOptions(bool more);
		virtual void    setReadOnly(bool readOnly);
		virtual void    saveState(const KAEvent*);
		virtual bool    type_stateChanged() const;
		virtual void    type_setEvent(KAEvent&, const KDateTime&, const QString& text, int lateCancel, bool trial);
		virtual int     getAlarmFlags() const;
		virtual bool    type_validate(bool trial);
		virtual void    type_trySuccessMessage(ShellProcess*, const QString& text);
		virtual bool    checkText(QString& result, bool showErrorMessage = true) const;

	private slots:
		void            slotCmdScriptToggled(bool);

	private:
		// Command alarm options
		CommandEdit*        mCmdEdit;
		QGroupBox*          mCmdOutputBox;
		ButtonGroup*        mCmdOutputGroup;     // what to do with command output
		RadioButton*        mCmdExecInTerm;
		RadioButton*        mCmdLogToFile;
		RadioButton*        mCmdDiscardOutput;
		LineEdit*           mCmdLogFileEdit;     // log file URL edit box
		KHBox*              mCmdPadding;

		// Initial state of all controls
		bool                mSavedCmdScript;        // mCmdEdit->isScript() status
		QAbstractButton*    mSavedCmdOutputRadio;   // selected button in mCmdOutputGroup
		QString             mSavedCmdLogFile;       // mCmdLogFileEdit value
};


class EditEmailAlarmDlg : public EditAlarmDlg
{
		Q_OBJECT
	public:
		EditEmailAlarmDlg(bool Template, bool newAlarm, QWidget* parent = 0,
		                  GetResourceType = RES_PROMPT);
		EditEmailAlarmDlg(bool Template, const KAEvent*, bool newAlarm, QWidget* parent = 0,
		                  GetResourceType = RES_PROMPT, bool readOnly = false);

		// Methods to initialise values in the New Alarm dialogue.
		// N.B. setTime() must be called first to set the date-only characteristic,
		//      followed by setRecurrence().
		virtual void    setAction(KAEventData::Action, const AlarmText& = AlarmText());
		void            setEmailFields(uint fromID, const EmailAddressList&, const QString& subject,
		                               const QStringList& attachments);
		void            setBcc(bool);

		static QString  i18n_chk_CopyEmailToSelf();    // text of 'Copy email to self' checkbox

	protected:
		virtual QString type_caption(bool newAlarm) const;
		virtual void    type_init(QWidget* parent, QVBoxLayout* frameLayout);
		virtual void    type_initValues(const KAEvent*);
		virtual void    type_showOptions(bool)  {}
		virtual void    setReadOnly(bool readOnly);
		virtual void    saveState(const KAEvent*);
		virtual bool    type_stateChanged() const;
		virtual void    type_setEvent(KAEvent&, const KDateTime&, const QString& text, int lateCancel, bool trial);
		virtual int     getAlarmFlags() const;
		virtual bool    type_validate(bool trial);
		virtual void    type_trySuccessMessage(ShellProcess*, const QString& text);
		virtual bool    checkText(QString& result, bool showErrorMessage = true) const;

	private slots:
		void            openAddressBook();
		void            slotAddAttachment();
		void            slotRemoveAttachment();

	private:
		void            attachmentEnable();

		// Email alarm options
		EmailIdCombo*       mEmailFromList;
		LineEdit*           mEmailToEdit;
		QPushButton*        mEmailAddressButton; // email open address book button
		LineEdit*           mEmailSubjectEdit;
		TextEdit*           mEmailMessageEdit;   // email body edit box
		KComboBox*          mEmailAttachList;
		QPushButton*        mEmailAddAttachButton;
		QPushButton*        mEmailRemoveButton;
		CheckBox*           mEmailBcc;
		QString             mAttachDefaultDir;

		EmailAddressList    mEmailAddresses;     // list of addresses to send email to
		QStringList         mEmailAttachments;   // list of email attachment file names

		// Initial state of all controls
		QString             mSavedEmailFrom;        // mEmailFromList current value
		QString             mSavedEmailTo;          // mEmailToEdit value
		QString             mSavedEmailSubject;     // mEmailSubjectEdit value
		QStringList         mSavedEmailAttach;      // mEmailAttachList values
		bool                mSavedEmailBcc;         // mEmailBcc status
};

#endif // EDITDLGTYPES_H
