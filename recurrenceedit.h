/*
 *  recurrenceedit.h  -  widget to edit the event's recurrence definition
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie  software@astrojar.org.uk
 *
 *  Based on KOrganizer module koeditorrecurrence.h,
    Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    As a special exception, permission is given to link this program
    with any edition of Qt, and distribute the resulting executable,
    without including the source code for Qt in the source distribution.
 */

#ifndef RECURRENCEEDIT_H
#define RECURRENCEEDIT_H

#include <qframe.h>
#include <qdatetime.h>
#include <libkcal/event.h>

#include "datetime.h"
class QWidgetStack;
class QGroupBox;
class QButtonGroup;
class QLabel;
class SpinBox;
class CheckBox;
class ComboBox;
class RadioButton;
class DateEdit;
class TimeSpinBox;
class TimePeriod;
class ButtonGroup;
class KAlarmEvent;


class RecurrenceEdit : public QFrame
{
		Q_OBJECT
	public:
		enum RepeatType { NO_RECUR, AT_LOGIN, SUBDAILY, DAILY, WEEKLY, MONTHLY, ANNUAL };

		RecurrenceEdit(bool readOnly, QWidget* parent, const char* name = 0);
		virtual ~RecurrenceEdit()  { }

		/** Set widgets to default values */
		void          setDefaults(const QDateTime& from);
		/** Initialise according to a specified event */
		void          set(const KAlarmEvent&);
		/** Write event settings to event object */
		void          updateEvent(KAlarmEvent&);
		QWidget*      checkData(const QDateTime& startDateTime, bool& noTime) const;
		RepeatType    repeatType() const                    { return mRuleButtonType; }
		void          setStartDate(const QDate&);
		void          setEndDate(const QDate&);
		void          setEndDateTime(const DateTime&);
		DateTime      endDateTime() const;

	public slots:
		void          setDateTime(const QDateTime& start)   { currStartDateTime = start; }

	signals:
		void          shown();
		void          typeChanged(int recurType);   // returns a RepeatType value
		void          frequencyChanged();

	protected slots:
		void          periodClicked(int);
		void          monthlyClicked(int);
		void          yearlyClicked(int);
		void          disableRange(bool);
		void          enableDurationRange(bool);
		void          enableDateRange(bool);
		void          repeatCountChanged(int value);
		void          slotAnyTimeToggled(bool);

	protected:
		virtual void  showEvent(QShowEvent*);

	private:
		void          unsetAllCheckboxes();
		void          checkDay(int day);
		void          getCheckedDays(QBitArray& rDays);
		void          setCheckedDays(QBitArray& rDays);

		void          initNone();
		void          initSubDaily();
		void          initDaily();
		void          initWeekly();
		void          initMonthly();
		void          initYearly();

		// Main rule box and choices
		QGroupBox*     recurGroup;
		QFrame*        ruleFrame;
		QWidgetStack*  ruleStack;

		ButtonGroup*   ruleButtonGroup;
		RadioButton*   mNoneButton;
		RadioButton*   mAtLoginButton;
		RadioButton*   mSubDailyButton;
		RadioButton*   mDailyButton;
		RadioButton*   mWeeklyButton;
		RadioButton*   mMonthlyButton;
		RadioButton*   mYearlyButton;
		int            mNoneButtonId;
		int            mAtLoginButtonId;
		int            mSubDailyButtonId;
		int            mDailyButtonId;
		int            mWeeklyButtonId;
		int            mMonthlyButtonId;
		int            mYearlyButtonId;
		RepeatType     mRuleButtonType;

		// Rules without choices
		QFrame*        mNoneRuleFrame;

		// Subdaily rule choices
		QFrame*        mSubDayRuleFrame;
		QLabel*        mSubDayRecurLabel;
		TimeSpinBox*   mSubDayRecurFrequency;

		// Daily rule choices
		QFrame*        mDayRuleFrame;
		QLabel*        mDayRecurLabel;
		SpinBox*       mDayRecurFrequency;

		// Weekly rule choices
		QFrame*        mWeekRuleFrame;
		QLabel*        mWeekRecurLabel;
		SpinBox*       mWeekRecurFrequency;
		CheckBox*      mWeekRuleDayBox[7];
		int            mWeekRuleFirstDay;

		// Monthly rule choices
		QFrame*        mMonthRuleFrame;
		QLabel*        mMonthRecurLabel;
		SpinBox*       mMonthRecurFrequency;
		ButtonGroup*   mMonthRuleButtonGroup;
		RadioButton*   mMonthRuleOnNthDayButton;
		ComboBox*      mMonthRuleNthDayEntry;
		RadioButton*   mMonthRuleOnNthTypeOfDayButton;
		ComboBox*      mMonthRuleNthNumberEntry;
		ComboBox*      mMonthRuleNthTypeOfDayEntry;
		int            mMonthRuleOnNthDayButtonId;
		int            mMonthRuleOnNthTypeOfDayButtonId;

		// Yearly rule choices
		QFrame*        mYearRuleFrame;
		QLabel*        mYearRecurLabel;
		SpinBox*       mYearRecurFrequency;
		ButtonGroup*   mYearRuleButtonGroup;
		RadioButton*   mYearRuleFeb29Button;
		RadioButton*   mYearRuleDayMonthButton;
//		RadioButton*   mYearRuleDayButton;
		RadioButton*   mYearRuleOnNthTypeOfDayButton;
//		QSpinBox*      mYearRuleDayEntry;
		ComboBox*      mYearRuleNthNumberEntry;
		ComboBox*      mYearRuleNthTypeOfDayEntry;
		ComboBox*      mYearRuleDayMonthComboBox;
		int            mYearRuleFeb29ButtonId;
		int            mYearRuleDayMonthButtonId;
//		int            mYearRuleDayButtonId;
		int            mYearRuleOnNthTypeOfDayButtonId;

		// Range
		QButtonGroup*  mRangeButtonGroup;
		RadioButton*   mNoEndDateButton;
		RadioButton*   mRepeatCountButton;
		SpinBox*       mRepeatCountEntry;
		QLabel*        mRepeatCountLabel;
		RadioButton*   mEndDateButton;
		DateEdit*      mEndDateEdit;
		TimeSpinBox*   mEndTimeEdit;
		CheckBox*      mEndAnyTimeCheckBox;

		// Current start date and time
		QDateTime      currStartDateTime;
		bool           noEmitTypeChanged;    // suppress typeChanged() signal
		bool           mReadOnly;
};

#endif // RECURRENCEEDIT_H
