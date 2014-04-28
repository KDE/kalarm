/*
 *  recurrenceedit.h  -  widget to edit the event's recurrence definition
 *  Program:  kalarm
 *  Copyright © 2002-2011 by David Jarvie <djarvie@kde.org>
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

#include <KAlarmCal/repetition.h>

#include <kdatetime.h>

#include <QFrame>

class KDateComboBox;
class QDate;
class QShowEvent;
class QStackedWidget;
class QGroupBox;
class QLabel;
class QListWidget;
class QAbstractButton;
class QPushButton;
class SpinBox;
class CheckBox;
class RadioButton;
class TimeEdit;
class ButtonGroup;
class RepetitionButton;
class Rule;
class NoRule;
class SubDailyRule;
class DailyRule;
class WeeklyRule;
class MonthlyRule;
class YearlyRule;
namespace KAlarmCal { class KAEvent; }

using namespace KAlarmCal;

class RecurrenceEdit : public QFrame
{
        Q_OBJECT
    public:
        // Don't alter the order of these recurrence types
        enum RepeatType { INVALID_RECUR = -1, NO_RECUR, AT_LOGIN, SUBDAILY, DAILY, WEEKLY, MONTHLY, ANNUAL };

        explicit RecurrenceEdit(bool readOnly, QWidget* parent = 0);
        virtual ~RecurrenceEdit()  { }

        /** Set widgets to default values */
        void          setDefaults(const KDateTime& from);
        /** Initialise according to a specified event */
        void          set(const KAEvent&);
        /** Initialise with repeat-at-login selected, instead of calling set(). */
        void          setRepeatAtLogin();
        /** Write recurrence settings into an event */
        void          updateEvent(KAEvent&, bool adjustStart);
        QWidget*      checkData(const KDateTime& startDateTime, QString& errorMessage) const;
        RepeatType    repeatType() const                    { return mRuleButtonType; }
        bool          isTimedRepeatType() const             { return mRuleButtonType >= SUBDAILY; }
        Repetition    subRepetition() const;
        void          setSubRepetition(int reminderMinutes, bool dateOnly);
        void          setStartDate(const QDate&, const QDate& today);
        void          setDefaultEndDate(const QDate&);
        void          setEndDateTime(const KDateTime&);
        KDateTime     endDateTime() const;
        bool          stateChanged() const;
        void          activateSubRepetition();
        void          showMoreOptions(bool);

        static QString i18n_combo_NoRecur();           // text of 'No recurrence' selection
        static QString i18n_combo_AtLogin();           // text of 'At login' selection
        static QString i18n_combo_HourlyMinutely();    // text of 'Hourly/Minutely'
        static QString i18n_combo_Daily();             // text of 'Daily' selection
        static QString i18n_combo_Weekly();            // text of 'Weekly' selection
        static QString i18n_combo_Monthly();           // text of 'Monthly' selection
        static QString i18n_combo_Yearly();            // text of 'Yearly' selection

    public slots:
        void          setDateTime(const KDateTime& start)   { mCurrStartDateTime = start; }

    signals:
        void          shown();
        void          typeChanged(int recurType);   // returns a RepeatType value
        void          frequencyChanged();
        void          repeatNeedsInitialisation();
        void          contentsChanged();

    protected:
        virtual void  showEvent(QShowEvent*);

    private slots:
        void          periodClicked(QAbstractButton*);
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
        QStackedWidget*   mRuleStack;
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
        RepeatType        mRuleButtonType;
        bool              mDailyShown;       // daily rule has been displayed at some time or other
        bool              mWeeklyShown;      // weekly rule has been displayed at some time or other
        bool              mMonthlyShown;     // monthly rule has been displayed at some time or other
        bool              mYearlyShown;      // yearly rule has been displayed at some time or other

        // Range
        QGroupBox*        mRangeButtonBox;
        ButtonGroup*      mRangeButtonGroup;
        RadioButton*      mNoEndDateButton;
        RadioButton*      mRepeatCountButton;
        SpinBox*          mRepeatCountEntry;
        QLabel*           mRepeatCountLabel;
        RadioButton*      mEndDateButton;
        KDateComboBox*    mEndDateEdit;
        TimeEdit*         mEndTimeEdit;
        CheckBox*         mEndAnyTimeCheckBox;

        // Exceptions
        QGroupBox*        mExceptionGroup;
        QListWidget*      mExceptionDateList;
        KDateComboBox*    mExceptionDateEdit;
        QPushButton*      mChangeExceptionButton;
        QPushButton*      mDeleteExceptionButton;
        CheckBox*         mExcludeHolidays;
        CheckBox*         mWorkTimeOnly;
        QList<QDate>      mExceptionDates;

        // Current start date and time
        KDateTime         mCurrStartDateTime;
        RepetitionButton* mSubRepetition;
        bool              mNoEmitTypeChanged;        // suppress typeChanged() signal
        bool              mReadOnly;

        // Initial state of non-rule controls
        QAbstractButton*  mSavedRuleButton;          // which rule button was selected
        QAbstractButton*  mSavedRangeButton;         // which range button was selected
        int               mSavedRecurCount;          // recurrence repeat count
        KDateTime         mSavedEndDateTime;         // end date/time
        QList<QDate>      mSavedExceptionDates;      // exception dates
        Repetition        mSavedRepetition;          // sub-repetition interval & count (via mSubRepetition button)
        bool              mSavedExclHolidays;        // exclude holidays
        bool              mSavedWorkTimeOnly;        // only during working hours
};

#endif // RECURRENCEEDIT_H

// vim: et sw=4:
