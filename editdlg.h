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

#include <qspinbox.h>
#include <qcheckbox.h>
#include <qdatetime.h>

#include <kdialogbase.h>

#include <msgevent.h>
using namespace KCal;

#include "fontcolour.h"

class QMultiLineEdit;
class TimeSpinBox;
class DateSpinBox;

/**
 * EditAlarmDlg: A dialog for the input of an alarm message's details.
 */
class EditAlarmDlg : public KDialogBase
{
		Q_OBJECT
	public:
		EditAlarmDlg(const QString& caption, QWidget* parent = 0L, const char* name = 0L, const MessageEvent* = 0L);
		virtual ~EditAlarmDlg();

		void            getEvent(MessageEvent&);
		const QCString& getMessage() const      { return alarmMessage; }
		QDateTime       getDateTime() const     { return alarmDateTime; }
#ifdef SELECT_FONT
		const QColor    getBgColour() const       { return fontColour->bgColour(); }
		const QFont     getFont() const        { return fontColour->font(); }
#else
		const QColor    getBgColour() const       { return bgColourChoose->color(); }
#endif
		bool            getLateCancel() const   { return lateCancel->isChecked(); }
		bool            getBeep() const         { return beep->isChecked(); }

	protected:
		virtual void resizeEvent(QResizeEvent*);
	protected slots:
		void slotOk();
		void slotCancel();

	private:
		QMultiLineEdit* messageEdit;		// alarm message edit box
		TimeSpinBox*    timeEdit;
		DateSpinBox*    dateEdit;
		QCheckBox*      lateCancel;
		QCheckBox*      beep;
#ifdef SELECT_FONT
		FontColourChooser* fontColour;
#else
		ColourCombo*    bgColourChoose;
#endif
		QCString        alarmMessage;
		QDateTime       alarmDateTime;
//		QColor          bgColour;
};



class TimeSpinBox : public QSpinBox
{
	public:
		TimeSpinBox(QWidget* parent = 0L, const char* name = 0L);
	protected:
		virtual QString mapValueToText(int v);
		virtual int     mapTextToValue(bool* ok);
	private:
		class TimeValidator;
		TimeValidator*  validator;
};


class DateSpinBox : public QSpinBox
{
	public:
		DateSpinBox(QWidget* parent = 0L, const char* name = 0L);
		void            setDate(const QDate&);
		QDate           getDate();
	protected:
		virtual QString mapValueToText(int v);
		virtual int     mapTextToValue(bool* ok);
	private:
		static QDate    baseDate;
};

#endif // EDITDLG_H
