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
 */

#ifndef DATETIME_H
#define DATETIME_H

#include <qtimer.h>
#include <qdatetime.h>
#include "spinbox2.h"

class QRadioButton;
class QCheckBox;
class DateSpinBox;
class TimeSpinBox;


class AlarmTimeWidget : public QWidget
{
		Q_OBJECT
	public:
		enum {       // 'mode' values for constructor. May be OR'ed together.
			AT_TIME      = 0x00,   // "At ..."
			DEFER_TIME   = 0x01,   // "Defer to ..."
			DEFER_BUTTON = 0x02,   // with Defer... button
		};
		AlarmTimeWidget(const QString& groupBoxTitle, int mode, int deferSpacing = 0, QWidget* parent = 0, const char* name = 0);
		AlarmTimeWidget(int mode, int deferSpacing = 0, QWidget* parent = 0, const char* name = 0);
		bool           getDateTime(QDateTime&, bool& anyTime) const;
		void           setDateTime(const QDate& d)                  { setDateTime(d, true); }
		void           setDateTime(const QDateTime&, bool anyTime = false);
		void           enableAnyTime(bool enable);
		QSize          sizeHint() const                             { return minimumSizeHint(); }
	signals:
		void           deferred();
	protected slots:
		void           slotDefer();
		void           slotTimer();
		void           slotAtTimeToggled(bool on);
		void           slotAfterTimeToggled(bool on);
		void           slotDateTimeChanged(int);
		void           slotDelayTimeChanged(int);
		void           anyTimeToggled(bool);
	private:
		void           init(const QString& groupBoxTitle, bool groupBox, int mode, int deferSpacing);
		QRadioButton*  atTimeRadio;
		QRadioButton*  afterTimeRadio;
		DateSpinBox*   dateEdit;
		TimeSpinBox*   timeEdit;
		TimeSpinBox*   delayTime;
		QCheckBox*     anyTimeCheckBox;
		QTimer         timer;
		bool           timerSyncing;            // timer is not yet synchronised to the minute boundary
		bool           anyTimeAllowed;          // 'anyTimeCheckBox' is enabled
		bool           enteredDateTimeChanged;  // prevent recursion through slotDelayTimeChanged()
};


class TimeSpinBox : public SpinBox2
{
		Q_OBJECT
	public:
		TimeSpinBox(QWidget* parent = 0, const char* name = 0);
		TimeSpinBox(int minMinute, int maxMinute, QWidget* parent = 0, const char* name = 0);
		bool            valid() const        { return !invalid; }
		QTime           getTime() const;
		void            setValid(bool);
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
		bool            invalid;
		bool            enteredSetValue;    // to prevent infinite recursion in setValue()
};


class DateSpinBox : public QSpinBox
{
	public:
		DateSpinBox(QWidget* parent = 0, const char* name = 0);
		void            setDate(const QDate& d)       { setValue(getDateValue(d)); }
		QDate           getDate();
		static int      getDateValue(const QDate&);
	protected:
		virtual QString mapValueToText(int v);
		virtual int     mapTextToValue(bool* ok);
	private:
		static QDate    baseDate;
};

#endif // DATETIME_H
