/*
 *  alarmtimewidget.h  -  alarm date/time entry widget
 *  Program:  kalarm
 *  Copyright © 2001-2011 by David Jarvie <djarvie@kde.org>
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

#ifndef ALARMTIMEWIDGET_H
#define ALARMTIMEWIDGET_H

#include <kalarmcal/datetime.h>
#include <QFrame>

class QAbstractButton;
class KDateComboBox;
class KHBox;
class ButtonGroup;
class RadioButton;
class CheckBox;
class PushButton;
class TimeEdit;
class TimeSpinBox;
class TimeZoneCombo;

using namespace KAlarmCal;


class AlarmTimeWidget : public QFrame
{
        Q_OBJECT
    public:
        enum Mode {       // 'mode' values for constructor
            AT_TIME        = 0x01,  // "At ..."
            DEFER_TIME     = 0x02,  // "Defer to ..."
            DEFER_ANY_TIME = DEFER_TIME | 0x04  // "Defer to ..." with 'any time' option
        };
        AlarmTimeWidget(const QString& groupBoxTitle, Mode, QWidget* parent = 0);
        explicit AlarmTimeWidget(Mode, QWidget* parent = 0);
        KDateTime        getDateTime(int* minsFromNow = 0, bool checkExpired = true, bool showErrorMessage = true, QWidget** errorWidget = 0) const;
        void             setDateTime(const DateTime&);
        void             setMinDateTimeIsCurrent();
        void             setMinDateTime(const KDateTime& = KDateTime());
        void             setMaxDateTime(const DateTime& = DateTime());
        const KDateTime& maxDateTime() const           { return mMaxDateTime; }
        KDateTime::Spec  timeSpec() const              { return mTimeSpec; }
        void             setReadOnly(bool);
        bool             anyTime() const               { return mAnyTime; }
        void             enableAnyTime(bool enable);
        void             selectTimeFromNow(int minutes = 0);
        void             showMoreOptions(bool);
        QSize            sizeHint() const              { return minimumSizeHint(); }

        static QString   i18n_TimeAfterPeriod();
        static const int maxDelayTime;    // maximum time from now

    signals:
        void             changed(const KDateTime&);
        void             dateOnlyToggled(bool anyTime);
        void             pastMax();

    private slots:
        void             updateTimes();
        void             slotButtonSet(QAbstractButton*);
        void             dateTimeChanged();
        void             delayTimeChanged(int);
        void             slotAnyTimeToggled(bool);
        void             slotTimeZoneChanged();
        void             slotTimeZoneToggled(bool);
        void             showTimeZoneSelector();

    private:
        void             init(Mode, const QString& groupBoxTitle = QString());
        void             setAnyTime();
        void             setMaxDelayTime(const KDateTime& now);
        void             setMaxMinTimeIf(const KDateTime& now);

        ButtonGroup*     mButtonGroup;
        RadioButton*     mAtTimeRadio;
        RadioButton*     mAfterTimeRadio;
        CheckBox*        mAnyTimeCheckBox;
        KDateComboBox*   mDateEdit;
        TimeEdit*        mTimeEdit;
        TimeSpinBox*     mDelayTimeEdit;
        PushButton*      mTimeZoneButton;
        KHBox*           mTimeZoneBox;      // contains label and time zone combo box
        CheckBox*        mNoTimeZone;
        TimeZoneCombo*   mTimeZone;
        KDateTime        mMinDateTime;      // earliest allowed date/time
        KDateTime        mMaxDateTime;      // latest allowed date/time
        KDateTime::Spec  mTimeSpec;         // time spec used
        int              mAnyTime;          // 0 = date/time is specified, 1 = only a date, -1 = uninitialised
        bool             mAnyTimeAllowed;   // 'mAnyTimeCheckBox' is enabled
        bool             mDeferring;        // being used to enter a deferral time
        bool             mMinDateTimeIsNow; // earliest allowed date/time is the current time
        bool             mPastMax;          // current time is past the maximum date/time
        bool             mMinMaxTimeSet;    // limits have been set for the time edit control
        bool             mTimerSyncing;     // mTimer is not yet synchronized to the minute boundary
};

#endif // ALARMTIMEWIDGET_H

// vim: et sw=4:
