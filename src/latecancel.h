/*
 *  latecancel.h  -  widget to specify cancellation if late
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004, 2005, 2007, 2009 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "timeselector.h"
#include "lib/timeperiod.h"

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
        bool            mDateOnly {false};  // hours/minutes units not allowed
        bool            mReadOnly {false};  // widget is read-only
        bool            mAutoCloseShown {false};  // auto-close checkbox is visible
};


// vim: et sw=4:
