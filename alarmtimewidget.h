/*
 *  alarmtimewidget.h  -  alarm date/time entry widget
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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

#ifndef ALARMTIMEWIDGET_H
#define ALARMTIMEWIDGET_H

#include <qtimer.h>
#include "datetime.h"
#include "buttongroup.h"

class RadioButton;
class CheckBox;
class DateEdit;
class TimeSpinBox;
class DateTime;


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
		DateTime       getDateTime(bool checkExpired = true, bool showErrorMessage = true, QWidget** errorWidget = 0) const;
		void           setDateTime(const DateTime&, bool setMinimum = true);
		void           setMinDateToday();
		void           setMinDate(bool);
		void           setReadOnly(bool);
		bool           anyTime() const               { return mAnyTime; }
		void           enableAnyTime(bool enable);
		QSize          sizeHint() const              { return minimumSizeHint(); }
	signals:
		void           anyTimeToggled(bool anyTime);
	protected slots:
		void           slotTimer();
		void           slotButtonSet(int id);
		void           dateTimeChanged();
		void           delayTimeChanged(int);
		void           slotAnyTimeToggled(bool);
	private:
		void           init(int mode);
		void           setAnyTime();

		RadioButton*   mAtTimeRadio;
		RadioButton*   mAfterTimeRadio;
		DateEdit*      mDateEdit;
		TimeSpinBox*   mTimeEdit;
		TimeSpinBox*   mDelayTimeEdit;
		CheckBox*      mAnyTimeCheckBox;
		QTimer         mTimer;
		int            mAnyTime;          // 0 = date/time is specified, 1 = only a date, -1 = uninitialised
		bool           mAnyTimeAllowed;   // 'mAnyTimeCheckBox' is enabled
		bool           mTimerSyncing;     // mTimer is not yet synchronised to the minute boundary
};

#endif // ALARMTIMEWIDGET_H
