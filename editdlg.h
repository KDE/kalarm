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

#include <qcheckbox.h>
#include <qdatetime.h>
#include <qradiobutton.h>

#include <kdialogbase.h>

#include "msgevent.h"
using namespace KCal;

class QButtonGroup;
class QGroupBox;
class QMultiLineEdit;
class QSpinBox;
class QHBox;
class ButtonGroup;
class FontColourChooser;
class ColourCombo;
class TimeSpinBox;
class AlarmTimeWidget;
class RecurrenceEdit;

/**
 * EditAlarmDlg: A dialog for the input of an alarm's details.
 */
class EditAlarmDlg : public KDialogBase
{
		Q_OBJECT
	public:
		enum MessageType { MESSAGE, FILE };

		EditAlarmDlg(const QString& caption, QWidget* parent = 0L, const char* name = 0L,
                             const KAlarmEvent* = 0L);
		virtual ~EditAlarmDlg();
		void         getEvent(KAlarmEvent&);
		QDateTime    getDateTime(bool* anyTime = 0L);

	protected:
		virtual void showEvent(QShowEvent*);
		virtual void resizeEvent(QResizeEvent*);
	protected slots:
		virtual void slotOk();
		virtual void slotCancel();
		virtual void slotTry();
		void         slotRepeatTypeChange(int repeatType);
		void         slotAlarmTypeClicked(int id);
		void         slotRepeatClicked(int id);
		void         slotEditDeferral();
		void         slotBrowseFile();
		void         slotSoundToggled(bool on);
		void         slotPickSound();

	private:
		KAlarmAlarm::Type getAlarmType() const;
		int               getAlarmFlags() const;
		bool              checkText(QString& result);
		void              setSoundPicker();

		void              initDisplayAlarms(QWidget* parent);
		void              initCommand(QWidget* parent);

		QFrame*          mainPage;
		QFrame*          recurPage;
		int              recurPageIndex;

		QRadioButton*    messageRadio;
		QRadioButton*    commandRadio;
		QRadioButton*    fileRadio;
		QWidgetStack*    alarmTypeStack;

		// Display alarm options widgets
		QFrame*          displayAlarmsFrame;
		QHBox*           fileBox;
		QHBox*           filePadding;
		QCheckBox*       sound;
		QPushButton*     soundPicker;
		QCheckBox*       confirmAck;
#ifdef SELECT_FONT
		FontColourChooser* fontColour;
#else
		ColourCombo*     bgColourChoose;
#endif
		// Text message alarm widgets
		QMultiLineEdit*  textMessageEdit;     // text message edit box
		// Text file alarm widgets
		QLineEdit*       fileMessageEdit;     // text file edit box
		QPushButton*     fileBrowseButton;
		QString          fileDefaultDir;      // default directory for browse button
		// Command alarm widgets
		QFrame*          commandFrame;
		QLineEdit*       commandMessageEdit;  // command edit box

		QGroupBox*       deferGroup;
		QLabel*          deferTimeLabel;
		AlarmTimeWidget* timeWidget;
		QCheckBox*       lateCancel;

		QRadioButton*    noRepeatRadio;
		QRadioButton*    repeatAtLoginRadio;
		QRadioButton*    recurRadio;
		RecurrenceEdit*  recurrenceEdit;

		QString          alarmMessage;     // message text/file name/command/email message
		QDateTime        alarmDateTime;
		QDateTime        deferDateTime;
		QString          soundFile;        // sound file to play when alarm is triggered, or null for beep
		QSize            basicSize;        // size without deferred time widget
		int              deferGroupHeight; // height added by deferred time widget
		bool             alarmAnyTime;     // alarmDateTime is only a date, not a time
		bool             timeDialog;       // the dialog shows date/time fields only
		bool             shown;            // the dialog has already been displayed
};

#endif // EDITDLG_H
