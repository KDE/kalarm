/*
 *  timeselector.h  -  widget to optionally set a time period
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TIMESELECTOR_H
#define TIMESELECTOR_H

#include "lib/timeperiod.h"

#include <QFrame>

class CheckBox;
class ComboBox;


class TimeSelector : public QFrame
{
        Q_OBJECT
    public:
        TimeSelector(const QString& selectText, const QString& selectWhatsThis,
                     const QString& valueWhatsThis, bool allowHourMinute, QWidget* parent);
        ComboBox*    createSignCombo();
        bool         isChecked() const;
        void         setChecked(bool on);
        KCalendarCore::Duration period() const;
        void         setPeriod(const KCalendarCore::Duration&, bool dateOnly, TimePeriod::Units defaultUnits);
        TimePeriod::Units units() const   { return mPeriod->units(); }
        void         setUnits(TimePeriod::Units units)  { mPeriod->setUnits(units); }
        void         setReadOnly(bool);
        bool         isDateOnly() const   { return mPeriod->isDateOnly(); }
        void         setDateOnly(bool dateOnly = true);
        void         setMaximum(int hourmin, int days);
        void         setFocusOnCount();

    Q_SIGNALS:
        void         toggled(bool);             // selection checkbox has been toggled
        void         valueChanged(const KCalendarCore::Duration&); // value has changed

    protected Q_SLOTS:
        void         selectToggled(bool);
        void         periodChanged(const KCalendarCore::Duration&);

    private:
        CheckBox*    mSelect;
        TimePeriod*  mPeriod;
        ComboBox*    mSignWidget {nullptr};
        bool         mReadOnly {false};         // the widget is read only
};

#endif // TIMESELECTOR_H

// vim: et sw=4:
