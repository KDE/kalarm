/*
 *  recurrenceedit_p.h  -  private classes for recurrenceedit.cpp
 *  Program:  kalarm
 *  Copyright © 2003,2005,2007,2009-2011 by David Jarvie <djarvie@kde.org>
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

#ifndef RECURRENCEEDIT_P_H
#define RECURRENCEEDIT_P_H

#include "radiobutton.h"

#include <kalarmcal/karecurrence.h>

#include <QList>
#include <QVector>
#include <QFrame>
#include <QBitArray>
#include <QAbstractButton>

class QLabel;
class QWidget;
class QVBoxLayout;
class ButtonGroup;
class ComboBox;
class CheckBox;
class SpinBox;
class TimeSpinBox;
class QString;

using namespace KAlarmCal;


class NoRule : public QFrame
{
        Q_OBJECT
    public:
        explicit NoRule(QWidget* parent) : QFrame(parent) { }
        virtual int      frequency() const       { return 0; }
};

class Rule : public NoRule
{
        Q_OBJECT
    public:
        Rule(const QString& freqText, const QString& freqWhatsThis, bool time, bool readOnly,
             QWidget* parent);
        int              frequency() const;
        void             setFrequency(int);
        virtual void     setFrequencyFocus()     { mSpinBox->setFocus(); }
        QVBoxLayout*     layout() const          { return mLayout; }
        virtual QWidget* validate(QString&)      { return 0; }
        virtual void     saveState();
        virtual bool     stateChanged() const;

    signals:
        void             frequencyChanged();
        void             changed();          // emitted whenever any control changes

    private:
        QWidget*         mSpinBox;
        SpinBox*         mIntSpinBox;
        TimeSpinBox*     mTimeSpinBox;
        QVBoxLayout*     mLayout;
        // Saved state of all controls
        int              mSavedFrequency;    // frequency for the selected rule
};

// Subdaily rule choices
class SubDailyRule : public Rule
{
        Q_OBJECT
    public:
        SubDailyRule(bool readOnly, QWidget* parent);
};

// Daily/weekly rule choices base class
class DayWeekRule : public Rule
{
        Q_OBJECT
    public:
        DayWeekRule(const QString& freqText, const QString& freqWhatsThis, const QString& daysWhatsThis,
                    bool readOnly, QWidget* parent);
        QBitArray        days() const;
        void             setDays(bool);
        void             setDays(const QBitArray& days);
        void             setDay(int dayOfWeek);
        virtual QWidget* validate(QString& errorMessage);
        virtual void     saveState();
        virtual bool     stateChanged() const;

    private:
        CheckBox*        mDayBox[7];
        // Saved state of all controls
        QBitArray        mSavedDays;         // ticked days for weekly rule
};

// Daily rule choices
class DailyRule : public DayWeekRule
{
        Q_OBJECT
    public:
        DailyRule(bool readOnly, QWidget* parent);
};

// Weekly rule choices
class WeeklyRule : public DayWeekRule
{
        Q_OBJECT
    public:
        WeeklyRule(bool readOnly, QWidget* parent);
};

// Monthly/yearly rule choices base class
class MonthYearRule : public Rule
{
        Q_OBJECT
    public:
        enum DayPosType { DATE, POS };

        MonthYearRule(const QString& freqText, const QString& freqWhatsThis, bool allowEveryWeek,
                      bool readOnly, QWidget* parent);
        DayPosType       type() const;
        int              date() const;       // if date in month is selected
        int              week() const;       // if position is selected
        int              dayOfWeek() const;  // if position is selected
        void             setType(DayPosType);
        void             setDate(int dayOfMonth);
        void             setPosition(int week, int dayOfWeek);
        void             setDefaultValues(int dayOfMonth, int dayOfWeek);
        virtual void     saveState();
        virtual bool     stateChanged() const;

    signals:
        void             typeChanged(DayPosType);

    protected:
        DayPosType       buttonType(QAbstractButton* b) const  { return b == mDayButton ? DATE : POS; }
        virtual void     daySelected(int /*day*/)  { }

    protected slots:
        virtual void     clicked(QAbstractButton*);

    private slots:
        virtual void     slotDaySelected(int index);

    private:
        void             enableSelection(DayPosType);

        ButtonGroup*     mButtonGroup;
        RadioButton*     mDayButton;
        RadioButton*     mPosButton;
        ComboBox*        mDayCombo;
        ComboBox*        mWeekCombo;
        ComboBox*        mDayOfWeekCombo;
        bool             mEveryWeek;         // "Every" week is allowed
        // Saved state of all controls
        int              mSavedType;         // whether day-of-month or month position radio button was selected
        int              mSavedDay;          // chosen day of month selected item
        int              mSavedWeek;         // chosen month position: selected week item
        int              mSavedWeekDay;      // chosen month position: selected day of week
};

// Monthly rule choices
class MonthlyRule : public MonthYearRule
{
        Q_OBJECT
    public:
        MonthlyRule(bool readOnly, QWidget* parent);
};

// Yearly rule choices
class YearlyRule : public MonthYearRule
{
        Q_OBJECT
    public:
        YearlyRule(bool readOnly, QWidget* parent);
        QVector<int>     months() const;
        void             setMonths(const QList<int>& months);
        void             setDefaultValues(int dayOfMonth, int dayOfWeek, int month);
        KARecurrence::Feb29Type feb29Type() const;
        void             setFeb29Type(KARecurrence::Feb29Type);
        virtual QWidget* validate(QString& errorMessage);
        virtual void     saveState();
        virtual bool     stateChanged() const;

    protected:
        virtual void     daySelected(int day);

    protected slots:
        virtual void     clicked(QAbstractButton*);

    private slots:
        void             enableFeb29();

    private:
        CheckBox*        mMonthBox[12];
        QLabel*          mFeb29Label;
        ComboBox*        mFeb29Combo;
        // Saved state of all controls
        QVector<int>     mSavedMonths;       // ticked months for yearly rule
        int              mSavedFeb29Type;    // February 29th recurrence type
};

#endif // RECURRENCEEDIT_P_H

// vim: et sw=4:
