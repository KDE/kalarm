/*
 *  editdlg.h  -  dialogue to create or modify an alarm message
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef EDITDLG_H
#define EDITDLG_H

#include <qcheckbox.h>
#include <qdatetime.h>
#include <qradiobutton.h>

#include <kdialogbase.h>

#include "fontcolour.h"
#include "msgevent.h"
using namespace KCal;

class QMultiLineEdit;
class QSpinBox;
class TimeSpinBox;
class AlarmTimeWidget;

/**
 * EditAlarmDlg: A dialog for the input of an alarm message's details.
 */
class EditAlarmDlg : public KDialogBase
{
      Q_OBJECT
   public:
      enum MessageType { MESSAGE, FILE };

      EditAlarmDlg(const QString& caption, QWidget* parent = 0L, const char* name = 0L,
                   const KAlarmEvent* = 0L);
      virtual ~EditAlarmDlg();

      void            getEvent(KAlarmEvent&);
      const QString&  getMessage() const      { return alarmMessage; }
      MessageType     getMessageType() const  { return fileRadio->isOn() ? FILE : MESSAGE; }
      QDateTime       getDateTime() const     { return alarmDateTime; }
#ifdef SELECT_FONT
      const QColor    getBgColour() const     { return fontColour->bgColour(); }
      const QFont     getFont() const         { return fontColour->font(); }
#else
      const QColor    getBgColour() const     { return bgColourChoose->color(); }
#endif
      bool            getLateCancel() const   { return lateCancel->isChecked(); }
      bool            getBeep() const         { return beep->isChecked(); }

   protected:
      virtual void resizeEvent(QResizeEvent*);
   protected slots:
      void slotOk();
      void slotCancel();
      void slotMessageToggled(bool on);
      void slotFileToggled(bool on);
      void slotBrowse();
      void slotMessageTextChanged();
      void slotRepeatCountChanged(int);

   private:
      QRadioButton*    messageRadio;
      QRadioButton*    fileRadio;
      QPushButton*     browseButton;
      QMultiLineEdit*  messageEdit;     // alarm message edit box
      AlarmTimeWidget* timeWidget;
      QCheckBox*       lateCancel;
      QCheckBox*       beep;
      QSpinBox*        repeatCount;
      TimeSpinBox*     repeatInterval;
      QCheckBox*       repeatAtLogin;
#ifdef SELECT_FONT
      FontColourChooser* fontColour;
#else
      ColourCombo*     bgColourChoose;
#endif
      QString          alarmMessage;
      QDateTime        alarmDateTime;
//    QColor           bgColour;
      bool             timeDialog;      // the dialog shows date/time fields only
};

#endif // EDITDLG_H
