/*
 *  recurrenceedit.h  -  widget to edit the event's recurrence definition
 *  Program:  kalarm
 *  Copyright © 2002-2005,2008 by David Jarvie <djarvie@kde.org>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef RECURRENCEEDIT_H
#define RECURRENCEEDIT_H

#include <qframe.h>
#include <qdatetime.h>
#include <qvaluelist.h>

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
class RadioButton;
class DateEdit;
class TimeEdit;
class ButtonGroup;
class RepetitionButton;
class KAEvent;
class Rule;
class NoRule;
class SubDailyRule;
class DailyRule;
class WeeklyRule;
class MonthlyRule;
class YearlyRule;


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
		void          set(const KAEvent&, bool keepDuration);
		/** Write recurrence settings into an event */
		void          updateEvent(KAEvent&, bool adjustStart);
		QWidget*      checkData(const QDateTime& startDateTime, QString& errorMessage) const;
		RepeatType    repeatType() const                    { return mRuleButtonType; }
		bool          isTimedRepeatType() const             { return mRuleButtonType >= SUBDAILY; }
		int           subRepeatCount(int* subRepeatInterval = 0) const;
		void          setSubRepetition(int reminderMinutes, bool dateOnly);
		void          setStartDate(const QDate&, const QDate& today);
		void          setDefaultEndDate(const QDate&);
		void          setEndDateTime(const DateTime&);
		DateTime      endDateTime() const;
		bool          stateChanged() const;
		void          activateSubRepetition();

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
		void          repeatNeedsInitialisation();

	protected:
		virtual void  showEvent(QShowEvent*);

	private slots:
		void          periodClicked(int);
		void          rangeTypeClicked();
		void          repeatCountChanged(int value);
		void          slotAnyTimeToggled(bool);
		void          addException();
		void          changeException();
		void          deleteException();
		void          enableExceptionButtons();

	private:
		void          setRuleDefaults(const QDate& start);
		void          saveState();

		// Main rule box and choices
		QWidgetStack*     mRuleStack;
		Rule*             mRule;         // current rule widget, or 0 if NoRule
		NoRule*           mNoRule;
		SubDailyRule*     mSubDailyRule;
		DailyRule*        mDailyRule;
		WeeklyRule*       mWeeklyRule;
		MonthlyRule*      mMonthlyRule;
		YearlyRule*       mYearlyRule;

		ButtonGroup*      mRuleButtonGroup;
		RadioButton*      mNoneButton;
		RadioButton*      mAtLoginButton;
		RadioButton*      mSubDailyButton;
		RadioButton*      mDailyButton;
		RadioButton*      mWeeklyButton;
		RadioButton*      mMonthlyButton;
		RadioButton*      mYearlyButton;
		int               mNoneButtonId;
		int               mAtLoginButtonId;
		int               mSubDailyButtonId;
		int               mDailyButtonId;
		int               mWeeklyButtonId;
		int               mMonthlyButtonId;
		int               mYearlyButtonId;
		RepeatType        mRuleButtonType;
		bool              mDailyShown;       // daily rule has been displayed at some time or other
		bool              mWeeklyShown;      // weekly rule has been displayed at some time or other
		bool              mMonthlyShown;     // monthly rule has been displayed at some time or other
		bool              mYearlyShown;      // yearly rule has been displayed at some time or other

		// Range
		ButtonGroup*      mRangeButtonGroup;
		RadioButton*      mNoEndDateButton;
		RadioButton*      mRepeatCountButton;
		SpinBox*          mRepeatCountEntry;
		QLabel*           mRepeatCountLabel;
		RadioButton*      mEndDateButton;
		DateEdit*         mEndDateEdit;
		TimeEdit*         mEndTimeEdit;
		CheckBox*         mEndAnyTimeCheckBox;

		// Exceptions
		QGroupBox*        mExceptionGroup;
		QListBox*         mExceptionDateList;
		DateEdit*         mExceptionDateEdit;
		QPushButton*      mChangeExceptionButton;
		QPushButton*      mDeleteExceptionButton;
		QValueList<QDate> mExceptionDates;

		// Current start date and time
		QDateTime         mCurrStartDateTime;
		RepetitionButton* mSubRepetition;
		bool              mNoEmitTypeChanged;        // suppress typeChanged() signal
		bool              mReadOnly;

		// Initial state of non-rule controls
		QButton*          mSavedRuleButton;          // which rule button was selected
		QButton*          mSavedRangeButton;         // which range button was selected
		int               mSavedRecurCount;          // recurrence repeat count
		DateTime          mSavedEndDateTime;         // end date/time
		QValueList<QDate> mSavedExceptionDates;      // exception dates
		int               mSavedRepeatInterval;      // sub-repetition interval (via mSubRepetition button)
		int               mSavedRepeatCount;         // sub-repetition count (via mSubRepetition button)
};

#endif // RECURRENCEEDIT_H
