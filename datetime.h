/*
 *  datetime.h  -  date and time spinboxes, and alarm time entry widget
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

#ifndef DATETIME_H
#define DATETIME_H

#include <qtimer.h>
#include <qdatetime.h>
#include "buttongroup.h"
#include "spinbox2.h"

class QRadioButton;
class QCheckBox;
class DateEdit;
class TimeSpinBox;


class AlarmTimeWidget : public ButtonGroup
{
		Q_OBJECT
	public:
		enum {       // 'mode' values for constructor. May be OR'ed together.
			AT_TIME      = 0x00,   // "At ..."
			DEFER_TIME   = 0x01,   // "Defer to ..."
			NARROW       = 0x02    // make a narrow widget
		};
		AlarmTimeWidget(const QString& groupBoxTitle, int mode, QWidget* parent = 0, const char* name = 0);
		AlarmTimeWidget(int mode, QWidget* parent = 0, const char* name = 0);
		QWidget*       getDateTime(QDateTime&, bool& anyTime, bool showErrorMessage = true) const;
		void           setDateTime(const QDate& d)                  { setDateTime(d, true); }
		void           setDateTime(const QDateTime&, bool anyTime = false);
		void           enableAnyTime(bool enable);
		QSize          sizeHint() const                             { return minimumSizeHint(); }
	protected slots:
		void           slotTimer();
		void           slotButtonSet(int id);
		void           slotButtonClicked(int id);
		void           slotDateChanged(QDate)     { dateTimeChanged(); }
		void           slotTimeChanged(int)       { dateTimeChanged(); }
		void           delayTimeChanged(int);
		void           anyTimeToggled(bool);
	private:
		void           init(int mode);
		void           dateTimeChanged();

		QRadioButton*  atTimeRadio;
		QRadioButton*  afterTimeRadio;
		DateEdit*      dateEdit;
		TimeSpinBox*   timeEdit;
		TimeSpinBox*   delayTime;
		QCheckBox*     anyTimeCheckBox;
		QTimer         timer;
		bool           timerSyncing;            // timer is not yet synchronised to the minute boundary
		bool           anyTimeAllowed;          // 'anyTimeCheckBox' is enabled
};


class TimeSpinBox : public SpinBox2
{
		Q_OBJECT
	public:
		TimeSpinBox(QWidget* parent = 0, const char* name = 0);
		TimeSpinBox(int minMinute, int maxMinute, QWidget* parent = 0, const char* name = 0);
		bool            valid() const;
		QTime           time() const;
		void            setValid(bool);
		static QString  shiftWhatsThis();
	public slots:
		virtual void    setValue(int value);
		virtual void    stepUp();
		virtual void    stepDown();
	protected:
		virtual QString mapValueToText(int v);
		virtual int     mapTextToValue(bool* ok);
	private:
		class TimeValidator;
		TimeValidator*  validator;
		int             minimumValue;
		bool            invalid;            // value is currently invalid (asterisks)
		bool            enteredSetValue;    // to prevent infinite recursion in setValue()
};

#endif // DATETIME_H
