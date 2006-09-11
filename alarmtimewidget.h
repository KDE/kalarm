/*
 *  alarmtimewidget.h  -  alarm date/time entry widget
 *  Program:  kalarm
 *  Copyright © 2001-2006 by David Jarvie <software@astrojar.org.uk>
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

#ifndef ALARMTIMEWIDGET_H
#define ALARMTIMEWIDGET_H

#include <QFrame>
#include "datetime.h"

class QAbstractButton;
class ButtonGroup;
class RadioButton;
class CheckBox;
class DateEdit;
class TimeEdit;
class TimeSpinBox;


class AlarmTimeWidget : public QFrame
{
		Q_OBJECT
	public:
		enum {       // 'mode' values for constructor. May be OR'ed together.
			AT_TIME      = 0x00,   // "At ..."
			DEFER_TIME   = 0x01,   // "Defer to ..."
			NARROW       = 0x02    // make a narrow widget
		};
		AlarmTimeWidget(const QString& groupBoxTitle, int mode, QWidget* parent = 0);
		AlarmTimeWidget(int mode, QWidget* parent = 0);
		DateTime         getDateTime(int* minsFromNow = 0, bool checkExpired = true, bool showErrorMessage = true, QWidget** errorWidget = 0) const;
		void             setDateTime(const DateTime&);
		void             setMinDateTimeIsCurrent();
		void             setMinDateTime(const KDateTime& = KDateTime());
		void             setMaxDateTime(const DateTime& = DateTime());
		const KDateTime& maxDateTime() const           { return mMaxDateTime; }
		void             setReadOnly(bool);
		bool             anyTime() const               { return mAnyTime; }
		void             enableAnyTime(bool enable);
		void             selectTimeFromNow(int minutes = 0);
		QSize            sizeHint() const              { return minimumSizeHint(); }

		static QString   i18n_w_TimeFromNow();     // text of 'Time from now:' radio button, with 'w' shortcut
		static QString   i18n_TimeAfterPeriod();
		static const int maxDelayTime;    // maximum time from now

	signals:
		void             anyTimeToggled(bool anyTime);
		void             pastMax();

	protected slots:
		void             slotTimer();
		void             slotButtonSet(QAbstractButton*);
		void             dateTimeChanged();
		void             delayTimeChanged(int);
		void             slotAnyTimeToggled(bool);

	private:
		void             init(QWidget* topWidget, int mode, bool hasTitle);
		KDateTime::Spec  timeSpec() const;
		void             setAnyTime();
		void             setMaxDelayTime(const KDateTime& now);
		void             setMaxMinTimeIf(const KDateTime& now);

		ButtonGroup*     mButtonGroup;
		RadioButton*     mAtTimeRadio;
		RadioButton*     mAfterTimeRadio;
		DateEdit*        mDateEdit;
		TimeEdit*        mTimeEdit;
		TimeSpinBox*     mDelayTimeEdit;
		CheckBox*        mAnyTimeCheckBox;
		KDateTime        mMinDateTime;      // earliest allowed date/time
		KDateTime        mMaxDateTime;      // latest allowed date/time
		int              mAnyTime;          // 0 = date/time is specified, 1 = only a date, -1 = uninitialised
		bool             mAnyTimeAllowed;   // 'mAnyTimeCheckBox' is enabled
		bool             mDeferring;        // being used to enter a deferral time
		bool             mMinDateTimeIsNow; // earliest allowed date/time is the current time
		bool             mPastMax;          // current time is past the maximum date/time
		bool             mMinMaxTimeSet;    // limits have been set for the time edit control
		bool             mTimerSyncing;     // mTimer is not yet synchronised to the minute boundary
};

#endif // ALARMTIMEWIDGET_H
