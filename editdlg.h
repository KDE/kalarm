/*
 *  editdlg.h  -  dialogue to create or modify an alarm
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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

#include "msgevent.h"
using namespace KCal;

class QGroupBox;
class QMultiLineEdit;
class QComboBox;
class QRadioButton;
class QHBox;
class FontColourChooser;
class ColourCombo;
class TimeSpinBox;
class RadioButton;
class PushButton;
class CheckBox;
class AlarmTimeWidget;
class RecurrenceEdit;
class LineEdit;

/**
 * EditAlarmDlg: A dialog for the input of an alarm's details.
 */
class EditAlarmDlg : public KDialogBase
{
		Q_OBJECT
	public:
		enum MessageType { MESSAGE, FILE };

		EditAlarmDlg(const QString& caption, QWidget* parent = 0, const char* name = 0,
                             const KAlarmEvent* = 0, bool readOnly = false);
		virtual ~EditAlarmDlg();
		void         getEvent(KAlarmEvent&);
		QDateTime    getDateTime(bool* anyTime = 0);

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
		void         slotSoundToggled(bool on);
		void         slotPickSound();
		void         openAddressBook();
		void         slotAddAttachment();
		void         slotRemoveAttachment();
		void         slotShowMainPage();
		void         slotShowRecurrenceEdit();

	private:
		KAlarmEvent::Action getAlarmType() const;
		int                 getAlarmFlags() const;
		bool                checkText(QString& result);
		void                setSoundPicker();
		bool                checkEmailData();

		void                initDisplayAlarms(QWidget* parent);
		void                initCommand(QWidget* parent);
		void                initEmail(QWidget* parent);

		int              mainPageIndex;
		int              recurPageIndex;
		QWidgetStack*    recurTabStack;
		QLabel*          recurDisabled;
		bool             recurSetEndDate;    // adjust end date/time when recurrence tab is displayed

		RadioButton*     messageRadio;
		RadioButton*     commandRadio;
		RadioButton*     fileRadio;
		RadioButton*     emailRadio;
		QWidgetStack*    alarmTypeStack;

		// Display alarm options widgets
		QFrame*          displayAlarmsFrame;
		QHBox*           fileBox;
		QHBox*           filePadding;
		CheckBox*        sound;
		PushButton*      soundPicker;
		QString          soundDefaultDir;
		CheckBox*        confirmAck;
#ifdef SELECT_FONT
		FontColourChooser* fontColour;
#else
		ColourCombo*     bgColourChoose;
#endif
		// Text message alarm widgets
		QMultiLineEdit*  textMessageEdit;     // text message edit box
		// Text file alarm widgets
		LineEdit*        fileMessageEdit;     // text file edit box
		QString          fileDefaultDir;      // default directory for browse button
		// Command alarm widgets
		QFrame*          commandFrame;
		LineEdit*        commandMessageEdit;  // command edit box
		// Email alarm widgets
		QFrame*          emailFrame;
		LineEdit*        emailToEdit;
		QLineEdit*       emailSubjectEdit;
		QMultiLineEdit*  emailMessageEdit;    // email body edit box
		QComboBox*       emailAttachList;
		QPushButton*     emailRemoveButton;
		CheckBox*        emailBcc;
		QString          attachDefaultDir;

		QGroupBox*       deferGroup;
		QLabel*          deferTimeLabel;
		AlarmTimeWidget* timeWidget;
		CheckBox*        lateCancel;

		RadioButton*     noRepeatRadio;
		RadioButton*     repeatAtLoginRadio;
		RadioButton*     recurRadio;
		RecurrenceEdit*  recurrenceEdit;

		QString          alarmMessage;     // message text/file name/command/email message
		QDateTime        alarmDateTime;
		QDateTime        deferDateTime;
		QString          soundFile;        // sound file to play when alarm is triggered, or null for beep
		EmailAddressList emailAddresses;   // list of addresses to send email to
		QStringList      emailAttachments; // list of email attachment file names
		QSize            basicSize;        // size without deferred time widget
		int              deferGroupHeight; // height added by deferred time widget
		bool             alarmAnyTime;     // alarmDateTime is only a date, not a time
		bool             timeDialog;       // the dialog shows date/time fields only
		bool             mReadOnly;        // the dialog is read only
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
