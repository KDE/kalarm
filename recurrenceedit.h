/*
 *  recurrenceedit.h  -  widget to edit the event's recurrence definition
 *  Program:  kalarm
 *  (C) 2002 - 2004 by David Jarvie <software@astrojar.org.uk>
 *
 *  Based originally on KOrganizer module koeditorrecurrence.h,
 *  Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
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

#ifndef RECURRENCEEDIT_H
#define RECURRENCEEDIT_H

#include <qframe.h>
#include <qdatetime.h>
#include <qptrlist.h>
#include <libkcal/event.h>

#include "datetime.h"
class QWidgetStack;
class QGroupBox;
class QLabel;
class QListBox;
class QButton;
class QPushButton;
class QBoxLayout;
class SpinBox;
class CheckBox;
class ComboBox;
class RadioButton;
class DateEdit;
class TimeEdit;
class ButtonGroup;
class RecurFrequency;
class KAEvent;


class RecurrenceEdit : public QFrame
{
		Q_OBJECT
	public:
		// Don't alter the order of these recurrence types
		enum RepeatType { INVALID_RECUR = -1, NO_RECUR, AT_LOGIN, SUBDAILY, DAILY, WEEKLY, MONTHLY, ANNUAL };

		RecurrenceEdit(bool readOnly, QWidget* parent, const char* name = 0);
		virtual ~RecurrenceEdit()  { }

		/** Set widgets to default values */
		void          setDefaults(const QDateTime& from);
		/** Initialise according to a specified event */
		void          set(const KAEvent&);
		/** Write event settings to event object */
		void          updateEvent(KAEvent&, bool adjustStart);
		QWidget*      checkData(const QDateTime& startDateTime, QString& errorMessage) const;
		RepeatType    repeatType() const                    { return mRuleButtonType; }
		bool          isTimedRepeatType() const             { return mRuleButtonType >= SUBDAILY; }
		void          setStartDate(const QDate&, const QDate& today);
		void          setDefaultEndDate(const QDate&);
		void          setEndDateTime(const DateTime&);
		DateTime      endDateTime() const;
		bool          stateChanged() const;

		static QString i18n_Norecur();           // text of 'No recurrence' selection, lower case
		static QString i18n_NoRecur();           // text of 'No Recurrence' selection, initial capitals
		static QString i18n_AtLogin();           // text of 'At Login' selection
		static QString i18n_l_Atlogin();         // text of 'At &login' selection, with 'L' shortcut
		static QString i18n_HourlyMinutely();    // text of 'Hourly/Minutely'
		static QString i18n_u_HourlyMinutely();  // text of 'Ho&urly/Minutely' selection, with 'U' shortcut
		static QString i18n_Daily();             // text of 'Daily' selection
		static QString i18n_d_Daily();           // text of '&Daily' selection, with 'D' shortcut
		static QString i18n_Weekly();            // text of 'Weekly' selection
		static QString i18n_w_Weekly();          // text of '&Weekly' selection, with 'W' shortcut
		static QString i18n_Monthly();           // text of 'Monthly' selection
		static QString i18n_m_Monthly();         // text of '&Monthly' selection, with 'M' shortcut
		static QString i18n_Yearly();            // text of 'Yearly' selection
		static QString i18n_y_Yearly();          // text of '&Yearly' selection, with 'Y' shortcut

	public slots:
		void          setDateTime(const QDateTime& start)   { mCurrStartDateTime = start; }

	signals:
		void          shown();
		void          typeChanged(int recurType);   // returns a RepeatType value
		void          frequencyChanged();

	protected:
		virtual void  showEvent(QShowEvent*);

	private slots:
		void          periodClicked(int);
		void          monthlyClicked(int);
		void          yearlyClicked(int);
		void          rangeTypeClicked();
		void          repeatCountChanged(int value);
		void          slotAnyTimeToggled(bool);
		void          addException();
		void          changeException();
		void          deleteException();
		void          enableExceptionButtons();
		void          yearDayOfMonthSelected(int);

	private:
		void          setRuleDefaults(const QDate& start);
		bool          getCheckedDays(QBitArray& days) const;
		void          setCheckedDays(QBitArray& days);
		bool          getCheckedMonths(QValueList<int>& months) const;
		void          getCheckedMonths(QBitArray& months) const;
		void          saveState();
		void          initDayOfMonth(RadioButton**, ComboBox**, QWidget* parent, QBoxLayout*);
		void          initWeekOfMonth(RadioButton**, ComboBox** weekCombo, ComboBox** dayCombo, QWidget* parent, QBoxLayout*);

		void          initNone();
		void          initSubDaily();
		void          initDaily();
		void          initWeekly();
		void          initMonthly();
		void          initYearly();

		// Main rule box and choices
		QGroupBox*      recurGroup;
		QFrame*         ruleFrame;
		QWidgetStack*   ruleStack;

		ButtonGroup*    ruleButtonGroup;
		RadioButton*    mNoneButton;
		RadioButton*    mAtLoginButton;
		RadioButton*    mSubDailyButton;
		RadioButton*    mDailyButton;
		RadioButton*    mWeeklyButton;
		RadioButton*    mMonthlyButton;
		RadioButton*    mYearlyButton;
		int             mNoneButtonId;
		int             mAtLoginButtonId;
		int             mSubDailyButtonId;
		int             mDailyButtonId;
		int             mWeeklyButtonId;
		int             mMonthlyButtonId;
		int             mYearlyButtonId;
		RepeatType      mRuleButtonType;
		bool            mWeeklyShown;
		bool            mMonthlyShown;
		bool            mYearlyShown;

		// Rules without choices
		QFrame*         mNoneRuleFrame;

		// Subdaily rule choices
		QFrame*         mSubDayRuleFrame;
		RecurFrequency* mSubDayRecurFrequency;

		// Daily rule choices
		QFrame*         mDayRuleFrame;
		RecurFrequency* mDayRecurFrequency;

		// Weekly rule choices
		QFrame*         mWeekRuleFrame;
		RecurFrequency* mWeekRecurFrequency;
		CheckBox*       mWeekRuleDayBox[7];

		// Monthly rule choices
		QFrame*         mMonthRuleFrame;
		RecurFrequency* mMonthRecurFrequency;
		ButtonGroup*    mMonthRuleButtonGroup;
		RadioButton*    mMonthRuleOnNthDayButton;
		ComboBox*       mMonthRuleNthDayEntry;
		RadioButton*    mMonthRuleOnNthTypeOfDayButton;
		ComboBox*       mMonthRuleNthNumberEntry;
		ComboBox*       mMonthRuleNthTypeOfDayEntry;
		int             mMonthRuleOnNthDayButtonId;
		int             mMonthRuleOnNthTypeOfDayButtonId;

		// Yearly rule choices
		QFrame*         mYearRuleFrame;
		RecurFrequency* mYearRecurFrequency;
		ButtonGroup*    mYearRuleButtonGroup;
		RadioButton*    mYearRuleDayMonthButton;
		ComboBox*       mYearRuleNthDayEntry;
//		RadioButton*    mYearRuleDayButton;
		RadioButton*    mYearRuleOnNthTypeOfDayButton;
//		QSpinBox*       mYearRuleDayEntry;
		ComboBox*       mYearRuleNthNumberEntry;
		ComboBox*       mYearRuleNthTypeOfDayEntry;
		CheckBox*       mYearRuleMonthBox[12];
		int             mYearRuleDayMonthButtonId;
//		int             mYearRuleDayButtonId;
		int             mYearRuleOnNthTypeOfDayButtonId;

		// Range
		ButtonGroup*    mRangeButtonGroup;
		RadioButton*    mNoEndDateButton;
		RadioButton*    mRepeatCountButton;
		SpinBox*        mRepeatCountEntry;
		QLabel*         mRepeatCountLabel;
		RadioButton*    mEndDateButton;
		DateEdit*       mEndDateEdit;
		TimeEdit*    mEndTimeEdit;
		CheckBox*       mEndAnyTimeCheckBox;

		// Exceptions
		QGroupBox*      mExceptionGroup;
		QListBox*       mExceptionDateList;
		DateEdit*       mExceptionDateEdit;
		QPushButton*    mChangeExceptionButton;
		QPushButton*    mDeleteExceptionButton;
		QValueList<QDate> mExceptionDates;

		// Current start date and time
		QDateTime       mCurrStartDateTime;
		bool            noEmitTypeChanged;    // suppress typeChanged() signal
		bool            mReadOnly;

		// Initial state of all controls
		QButton*        mSavedRuleButton;          // which rule button was selected
		int             mSavedFrequency;           // frequency for the selected rule
		QBitArray       mSavedDays;                // ticked days for weekly rule
		bool            mSavedDayOfMonthSelected;  // day-of-month radio button was selected, false if month position
		int             mSavedDayOfMonth;          // chosen day of month selected item
		int             mSavedWeekOfMonth;         // chosen month position: selected week item
		int             mSavedWeekDayOfMonth;      // chosen month position: selected day of week
		QBitArray       mSavedMonths;              // ticked months for yearly rule
		QButton*        mSavedRangeButton;         // which range button was selected
		int             mSavedRepeatCount;         // repeat count
		DateTime        mSavedEndDateTime;         // end date/time
		QValueList<QDate> mSavedExceptionDates;    // exception dates
};

#endif // RECURRENCEEDIT_H
