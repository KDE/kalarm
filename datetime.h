/*
 *  datetime.h  -  alarm date/time entry widget
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

class RadioButton;
class CheckBox;
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
		void           setReadOnly(bool);
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

		RadioButton*   mAtTimeRadio;
		RadioButton*   mAfterTimeRadio;
		DateEdit*      mDateEdit;
		TimeSpinBox*   mTimeEdit;
		TimeSpinBox*   mDelayTimeEdit;
		CheckBox*      mAnyTimeCheckBox;
		QTimer         mTimer;
		bool           mTimerSyncing;            // mTimer is not yet synchronised to the minute boundary
		bool           mAnyTimeAllowed;          // 'mAnyTimeCheckBox' is enabled
};

#endif // DATETIME_H
