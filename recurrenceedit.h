/*
 *  recurrenceedit.h  -  widget to edit the event's recurrence definition
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
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

#include <qradiobutton.h>
#include <qbuttongroup.h>
#include <libkcal/event.h>

class QWidgetStack;
class QLabel;
class QSpinBox;
class QComboBox;
class QCheckBox;
class DateEdit;
class TimeSpinBox;
class ButtonGroup;
class KAlarmEvent;

using namespace KCal;


class RecurrenceEdit : public QObject
{
		Q_OBJECT
	public:
		enum RepeatType { SUBDAILY, DAILY, WEEKLY, MONTHLY, ANNUAL };

		RecurrenceEdit(QFrame* parent, const char* name = 0L);
		virtual ~RecurrenceEdit()  { }

		/** Set widgets to default values */
		void          setDefaults(const QDateTime& from);
		/** Initialise according to a specified event */
		void          set(const KAlarmEvent&);
		/** Write event settings to event object */
		void          updateEvent(KAlarmEvent&);
		bool          checkData(const QDateTime& startDateTime, bool& noTime) const;
		RepeatType    repeatType() const                    { return ruleButtonType; }

	public slots:
		void          setDateTime(const QDateTime& start)   { currStartDateTime = start; }

	signals:
		void          typeChanged(int recurType);   // returns a RepeatType value

	protected slots:
		void          periodClicked(int);
		void          monthlyClicked(int);
		void          yearlyClicked(int);
		void          disableRange(bool);
		void          rangeToggled(bool);
		void          enableDurationRange(bool);
		void          enableDateRange(bool);

	private:
		void          unsetAllCheckboxes();
		void          checkDay(int day);
		void          getCheckedDays(QBitArray& rDays);
		void          setCheckedDays(QBitArray& rDays);

		void          initNone();
		void          initSubdaily();
		void          initDaily();
		void          initWeekly();
		void          initMonthly();
		void          initYearly();

		// Main rule box and choices
		QGroupBox*    recurGroup;
		QFrame*       ruleFrame;
		QWidgetStack* ruleStack;

		ButtonGroup*  ruleButtonGroup;
		QLabel*       recurEveryLabel;
		QRadioButton* subdailyButton;
		QRadioButton* dailyButton;
		QRadioButton* weeklyButton;
		QRadioButton* monthlyButton;
		QRadioButton* yearlyButton;
		int           subdailyButtonId;
		int           dailyButtonId;
		int           weeklyButtonId;
		int           monthlyButtonId;
		int           yearlyButtonId;
		RepeatType    ruleButtonType;

		QWidgetStack* recurFrequencyStack;
		QSpinBox*     recurFrequency;
		TimeSpinBox*  recurHourMinFrequency;

		// Rules without choices
		QFrame*       noneFrame;

		// Weekly rule choices
		QFrame*       weeklyFrame;
		QCheckBox*    dayBox[7];

		// Monthly rule choices
		QFrame*       monthlyFrame;
		ButtonGroup*  monthlyButtonGroup;
		QRadioButton* onNthDayButton;
		QComboBox*    nthDayEntry;
		QRadioButton* onNthTypeOfDayButton;
		QComboBox*    nthNumberEntry;
		QComboBox*    nthTypeOfDayEntry;
		int           onNthDayButtonId;
		int           onNthTypeOfDayButtonId;

		// Yearly rule choices
		QFrame*       yearlyFrame;
		ButtonGroup*  yearlyButtonGroup;
		QRadioButton* yearMonthButton;
//		QRadioButton* yearDayButton;
		QRadioButton* yearlyOnNthTypeOfDayButton;
		QComboBox*    yearMonthDayEntry;
		QComboBox*    yearMonthComboBox;
//		QSpinBox*     yearDayEntry;
		QComboBox*    yearlyNthNumberEntry;
		QComboBox*    yearlyNthTypeOfDayEntry;
		QComboBox*    yeardayMonthComboBox;
		int           yearMonthButtonId;
//		int           yearDayButtonId;
		int           yearlyOnNthTypeOfDayButtonId;

		// Range
		QButtonGroup* rangeButtonGroup;
		QRadioButton* noEndDateButton;
		QRadioButton* repeatCountButton;
		QSpinBox*     repeatCountEntry;
		QLabel*       repeatCountLabel;
		QRadioButton* endDateButton;
		DateEdit*     endDateEdit;
		TimeSpinBox*  endTimeEdit;

		// Current start date and time
		QDateTime     currStartDateTime;
		bool          noEmitTypeChanged;    // suppress typeChanged() signal
};

#endif // RECURRENCEEDIT_H
