/*
 *  editdlg.h  -  dialogue to create or modify an alarm
 *  Program:  kalarm
 *  (C) 2001 - 2003 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#ifndef EDITDLG_H
#define EDITDLG_H

#include <qdatetime.h>
#include <qlineedit.h>

#include <kdialogbase.h>

#include "alarmevent.h"

class QGroupBox;
class QMultiLineEdit;
class QComboBox;
class QHBox;
class ColourCombo;
class FontColourButton;
class ComboBox;
class TimeSpinBox;
class RadioButton;
class PushButton;
class CheckBox;
class TimePeriod;
class AlarmTimeWidget;
class RecurrenceEdit;
class SoundPicker;
class LineEdit;

/**
 * EditAlarmDlg: A dialog for the input of an alarm's details.
 */
class EditAlarmDlg : public KDialogBase
{
		Q_OBJECT
	public:
		enum MessageType { MESSAGE, FILE };
		enum ReminderUnits { REMIND_HOURS_MINUTES, REMIND_DAYS, REMIND_WEEKS };

		EditAlarmDlg(const QString& caption, QWidget* parent = 0, const char* name = 0,
                             const KAlarmEvent* = 0, bool readOnly = false);
		virtual ~EditAlarmDlg();
		void         getEvent(KAlarmEvent&);
		QDateTime    getDateTime(bool* anyTime = 0);

		static ColourCombo* createBgColourChooser(bool readOnly, QWidget* parent, const char* name = 0);
		static CheckBox*    createConfirmAckCheckbox(bool readOnly, QWidget* parent, const char* name = 0);
		static CheckBox*    createLateCancelCheckbox(bool readOnly, QWidget* parent, const char* name = 0);

	protected:
		virtual void resizeEvent(QResizeEvent*);
	protected slots:
		virtual void slotOk();
		virtual void slotCancel();
		virtual void slotTry();
		void         slotRecurTypeChange(int repeatType);
		void         slotAlarmTypeClicked(int id);
		void         slotRepeatClicked(int id);
		void         slotEditDeferral();
		void         slotBrowseFile();
		void         slotFontColourSelected();
		void         slotBgColourSelected(const QColor&);
		void         openAddressBook();
		void         slotAddAttachment();
		void         slotRemoveAttachment();
		void         slotShowMainPage();
		void         slotShowRecurrenceEdit();
		void         slotAnyTimeToggled(bool anyTime);
		void         slotReminderToggled(bool);
		void         slotReminderUnitsSelected(int index);

	private:
		KAlarmEvent::Action getAlarmType() const;
		int                 getAlarmFlags() const;
		int                 getReminderMinutes() const;
		bool                checkText(QString& result);
		void                setReminder(int minutes);
		ReminderUnits       setReminderAnyTime(int minutes, bool anyTime);
		void                setSoundPicker();
		bool                checkEmailData();

		void                initDisplayAlarms(QWidget* parent);
		void                initCommand(QWidget* parent);
		void                initEmail(QWidget* parent);

		int               mMainPageIndex;
		int               mRecurPageIndex;
		QWidgetStack*     mRecurTabStack;
		QLabel*           mRecurDisabled;
		bool              mRecurSetEndDate;    // adjust end date/time when recurrence tab is displayed

		RadioButton*      mMessageRadio;
		RadioButton*      mCommandRadio;
		RadioButton*      mFileRadio;
		RadioButton*      mEmailRadio;
		QWidgetStack*     mAlarmTypeStack;

		// Display alarm options widgets
		QFrame*           mDisplayAlarmsFrame;
		QHBox*            mFileBox;
		QHBox*            mFilePadding;
		SoundPicker*      mSoundPicker;
		CheckBox*         mConfirmAck;
		FontColourButton* mFontColourButton;
		ColourCombo*      mBgColourChoose;
		CheckBox*         mReminder;
		TimePeriod*       mReminderCount;
		ComboBox*         mReminderUnits;
		// Text message alarm widgets
		QMultiLineEdit*   mTextMessageEdit;    // text message edit box
		// Text file alarm widgets
		LineEdit*         mFileMessageEdit;    // text file edit box
		QString           mFileDefaultDir;     // default directory for browse button
		// Command alarm widgets
		QFrame*           mCommandFrame;
		LineEdit*         mCommandMessageEdit; // command edit box
		// Email alarm widgets
		QFrame*           mEmailFrame;
		LineEdit*         mEmailToEdit;
		QLineEdit*        mEmailSubjectEdit;
		QMultiLineEdit*   mEmailMessageEdit;   // email body edit box
		QComboBox*        mEmailAttachList;
		QPushButton*      mEmailRemoveButton;
		CheckBox*         mEmailBcc;
		QString           mAttachDefaultDir;

		QGroupBox*        mDeferGroup;
		QLabel*           mDeferTimeLabel;
		AlarmTimeWidget*  mTimeWidget;
		CheckBox*         mLateCancel;

		RadioButton*      mNoRepeatRadio;
		RadioButton*      mRepeatAtLoginRadio;
		RadioButton*      mRecurRadio;
		RecurrenceEdit*   mRecurrenceEdit;

		QString           mAlarmMessage;       // message text/file name/command/email message
		QDateTime         mAlarmDateTime;
		QDateTime         mDeferDateTime;
		EmailAddressList  mEmailAddresses;     // list of addresses to send email to
		QStringList       mEmailAttachments;   // list of email attachment file names
		QSize             mBasicSize;          // size without deferred time widget
		int               mDeferGroupHeight;   // height added by deferred time widget
		bool              mAlarmAnyTime;       // mAlarmDateTime is only a date, not a time
		bool              mReadOnly;           // the dialog is read only
};


class PageFrame : public QFrame
{
		Q_OBJECT
	public:
		PageFrame(QWidget* parent = 0, const char* name = 0) : QFrame(parent, name) { }
	protected:
		virtual void     showEvent(QShowEvent*)    { emit shown(); }
	signals:
		void             shown();
};

class LineEdit : public QLineEdit
{
		Q_OBJECT
	public:
		LineEdit(QWidget* parent = 0, const char* name = 0)
		          : QLineEdit(parent, name), noSelect(false) { }
		void setNoSelect()   { noSelect = true; }
	protected:
		virtual void focusInEvent(QFocusEvent*);
	private:
		bool  noSelect;
};

#endif // EDITDLG_H
