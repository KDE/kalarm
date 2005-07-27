/*
 *  recurrenceeditprivate.h  -  private classes for recurrenceedit.cpp
 *  Program:  kalarm
 *  Copyright (C) 2003, 2005 by David Jarvie <software@astrojar.org.uk>
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
 *  51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef RECURRENCEEDITPRIVATE_H
#define RECURRENCEEDITPRIVATE_H

#include <qframe.h>

class QWidget;
class QVBoxLayout;
class ButtonGroup;
class RadioButton;
class ComboBox;
class CheckBox;
class SpinBox;
class TimeSpinBox;
class QString;


class NoRule : public QFrame
{
	public:
		NoRule(QWidget* parent, const char* name = 0) : QFrame(parent, name)
		                                                 { setFrameStyle(QFrame::NoFrame); }
		virtual int   frequency() const      { return 0; }
};

class Rule : public NoRule
{
		Q_OBJECT
	public:
		Rule(const QString& freqText, const QString& freqWhatsThis, bool time, bool readOnly,
		     QWidget* parent, const char* name = 0);
		int              frequency() const;
		void             setFrequency(int);
		virtual void     setFrequencyFocus()     { mSpinBox->setFocus(); }
		QVBoxLayout*     layout() const          { return mLayout; }
		virtual QWidget* validate(QString&)      { return 0; }
	signals:
		void         frequencyChanged();
	private:
		QWidget*     mSpinBox;
		SpinBox*     mIntSpinBox;
		TimeSpinBox* mTimeSpinBox;
		QVBoxLayout* mLayout;
};

// Subdaily rule choices
class SubDailyRule : public Rule
{
		Q_OBJECT
	public:
		SubDailyRule(bool readOnly, QWidget* parent, const char* name = 0);
};

// Daily rule choices
class DailyRule : public Rule
{
		Q_OBJECT
	public:
		DailyRule(bool readOnly, QWidget* parent, const char* name = 0);
};

// Weekly rule choices
class WeeklyRule : public Rule
{
		Q_OBJECT
	public:
		WeeklyRule(bool readOnly, QWidget* parent, const char* name = 0);
		bool         days(QBitArray& days) const;
		void         setDays(QBitArray& days);
		void         setDay(int dayOfWeek);
		virtual QWidget* validate(QString& errorMessage);
	private:
		CheckBox*    mDayBox[7];
};

// Monthly/yearly rule choices base class
class MonthYearRule : public Rule
{
		Q_OBJECT
	public:
		enum DayPosType { DATE, POS };

		MonthYearRule(const QString& freqText, const QString& freqWhatsThis, bool readOnly,
		              QWidget* parent, const char* name = 0);
		DayPosType   type() const;
		int          date() const;       // if date in month is selected
		int          week() const;       // if position is selected
		int          dayOfWeek() const;  // if position is selected
		void         setType(DayPosType);
		void         setDate(int dayOfMonth);
		void         setPosition(int week, int dayOfWeek);
		void         setDefaultValues(int dayOfMonth, int dayOfWeek);
	signals:
		void         typeChanged(DayPosType);
	protected:
		DayPosType   buttonType(int id) const  { return id == mDayButtonId ? DATE : POS; }
		virtual void daySelected(int /*day*/)  { }
	protected slots:
		virtual void clicked(int id);
	private slots:
		virtual void slotDaySelected(int index);
	private:
		void         enableSelection(DayPosType);

		ButtonGroup* mButtonGroup;
		RadioButton* mDayButton;
		RadioButton* mPosButton;
		ComboBox*    mDayCombo;
		ComboBox*    mWeekCombo;
		ComboBox*    mDayOfWeekCombo;
		int          mDayButtonId;
		int          mPosButtonId;
};

// Monthly rule choices
class MonthlyRule : public MonthYearRule
{
	public:
		MonthlyRule(bool readOnly, QWidget* parent, const char* name = 0);
};

// Yearly rule choices
class YearlyRule : public MonthYearRule
{
		Q_OBJECT
	public:
		YearlyRule(bool readOnly, QWidget* parent, const char* name = 0);
		bool         months(QValueList<int>& mnths) const;
		void         months(QBitArray& mnths) const;
		void         setMonths(const QValueList<int>& months);
		void         setDefaultValues(int dayOfMonth, int dayOfWeek, int month);
		virtual QWidget* validate(QString& errorMessage);
	protected slots:
		virtual void clicked(int id);
		virtual void daySelected(int day);
	private:
		CheckBox*    mMonthBox[12];
};

#endif // RECURRENCEEDITPRIVATE_H
