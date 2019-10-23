/*
 *  latecancel.h  -  widget to specify cancellation if late
 *  Program:  kalarm
 *  Copyright © 2004,2005,2007,2009 David Jarvie <djarvie@kde.org>
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

#ifndef LATECANCEL_H
#define LATECANCEL_H

#include "timeperiod.h"
#include "timeselector.h"

#include <QFrame>

class QStackedWidget;
class CheckBox;


class LateCancelSelector : public QFrame
{
        Q_OBJECT
    public:
        LateCancelSelector(bool allowHourMinute, QWidget* parent);
        int             minutes() const;
        void            setMinutes(int Minutes, bool dateOnly, TimePeriod::Units defaultUnits);
        void            setDateOnly(bool dateOnly);
        void            showAutoClose(bool show);
        bool            isAutoClose() const;
        void            setAutoClose(bool autoClose);
        bool            isReadOnly() const     { return mReadOnly; }
        void            setReadOnly(bool);

        static QString  i18n_chk_CancelIfLate();     // text of 'Cancel if late' checkbox
        static QString  i18n_chk_AutoCloseWin();     // text of 'Auto-close window after this time' checkbox
        static QString  i18n_chk_AutoCloseWinLC();   // text of 'Auto-close window after late-cancellation time' checkbox

    Q_SIGNALS:
        void            changed();          // emitted whenever any change occurs

    private Q_SLOTS:
        void            slotToggled(bool);

    private:
        QStackedWidget* mStack;             // contains mCheckboxFrame and mTimeSelectorFrame
        QFrame*         mCheckboxFrame;
        CheckBox*       mCheckbox;          // displayed when late cancellation is not selected
        QFrame*         mTimeSelectorFrame;
        TimeSelector*   mTimeSelector;      // displayed when late cancellation is selected
        CheckBox*       mAutoClose;
        bool            mDateOnly{false};   // hours/minutes units not allowed
        bool            mReadOnly{false};   // widget is read-only
        bool            mAutoCloseShown{false};  // auto-close checkbox is visible
};

#endif // LATECANCEL_H

// vim: et sw=4:
