/*
 *  timeselector.h  -  widget to optionally set a time period
 *  Program:  kalarm
 *  Copyright © 2004,2005,2007,2008,2010,2011 by David Jarvie <djarvie@kde.org>
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

#ifndef TIMESELECTOR_H
#define TIMESELECTOR_H

#include "timeperiod.h"
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
#ifdef USE_AKONADI
        KCalCore::Duration period() const;
        void         setPeriod(const KCalCore::Duration&, bool dateOnly, TimePeriod::Units defaultUnits);
#else
        KCal::Duration period() const;
        void         setPeriod(const KCal::Duration&, bool dateOnly, TimePeriod::Units defaultUnits);
#endif
        TimePeriod::Units units() const   { return mPeriod->units(); }
        void         setUnits(TimePeriod::Units units)  { mPeriod->setUnits(units); }
        void         setReadOnly(bool);
        bool         isDateOnly() const   { return mPeriod->isDateOnly(); }
        void         setDateOnly(bool dateOnly = true);
        void         setMaximum(int hourmin, int days);
        void         setFocusOnCount();

    signals:
        void         toggled(bool);             // selection checkbox has been toggled
#ifdef USE_AKONADI
        void         valueChanged(const KCalCore::Duration&); // value has changed
#else
        void         valueChanged(const KCal::Duration&); // value has changed
#endif

    protected slots:
        void         selectToggled(bool);
#ifdef USE_AKONADI
        void         periodChanged(const KCalCore::Duration&);
#else
        void         periodChanged(const KCal::Duration&);
#endif

    private:
        CheckBox*    mSelect;
        TimePeriod*  mPeriod;
        ComboBox*    mSignWidget;
        bool         mReadOnly;           // the widget is read only
};

#endif // TIMESELECTOR_H

// vim: et sw=4:
