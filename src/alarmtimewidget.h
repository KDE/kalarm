/*
 *  alarmtimewidget.h  -  alarm date/time entry widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2024 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "kalarmcalendar/datetime.h"

#include <QFrame>

class QAbstractButton;
class KDateComboBox;
class ButtonGroup;
class ComboBox;
class RadioButton;
class CheckBox;
class PushButton;
class TimeEdit;
class TimePeriod;
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
    AlarmTimeWidget(const QString& groupBoxTitle, Mode, QWidget* parent = nullptr);
    explicit AlarmTimeWidget(Mode, QWidget* parent = nullptr);
    KADateTime        getDateTime(bool checkExpired = true, bool showErrorMessage = true, QWidget** errorWidget = nullptr) const
                      { return getDateTime(nullptr, checkExpired, showErrorMessage, errorWidget); }
    KADateTime        getDateTime(int* minsFromNow) const
                      { return getDateTime(minsFromNow, true, true, nullptr); }
    void              setDateTime(const DateTime&);
    void              setMinDateTimeIsCurrent();
    void              setMinDateTime(const KADateTime& = KADateTime());
    void              setMaxDateTime(const DateTime& = DateTime());
    const KADateTime& maxDateTime() const           { return mMaxDateTime; }
    KADateTime::Spec  timeSpec() const              { return mTimeSpec; }
    void              setReadOnly(bool);
    bool              anyTime() const               { return mAnyTime; }
    bool              anyTimeSelected() const;
    void              enableAnyTime(bool enable);

    /** Select/deselect 'Time from now' option.
     *  @param minutes  Value to set in 'Time from now', or
     *                  if < 0, select 'At date/time' option.
     */
    void              selectTimeFromNow(int minutes = 0);
    void              focusTimeFromNow();
    void              showMoreOptions(bool);
    QSize             sizeHint() const override              { return minimumSizeHint(); }

    static QString    i18n_TimeAfterPeriod();
    static const int  maxDelayTime;    // maximum time from now

Q_SIGNALS:
    void              changed(const KAlarmCal::KADateTime&);
    void              dateOnlyToggled(bool anyTime);
    void              pastMax();

private Q_SLOTS:
    void              updateTimes();
    void              slotButtonSet(QAbstractButton*, QAbstractButton*);
    void              dateTimeChanged();
    void              delayTimeChanged(int);
    void              slotPresetSelected(int);
    void              slotAnyTimeToggled(bool);
    void              slotTimeZoneChanged();
    void              showTimeZoneSelector();

private:
    void              init(Mode, const QString& groupBoxTitle = QString());
    KADateTime        getDateTime(int* minsFromNow, bool checkExpired, bool showErrorMessage, QWidget** errorWidget) const;
    void              setAnyTime();
    void              setMaxDelayTime(const KADateTime& now);
    void              setMaxMinTimeIf(const KADateTime& now);

    ButtonGroup*      mButtonGroup;
    RadioButton*      mAtTimeRadio;
    RadioButton*      mAfterTimeRadio;
    CheckBox*         mAnyTimeCheckBox;
    KDateComboBox*    mDateEdit;
    TimeEdit*         mTimeEdit;
    TimeSpinBox*      mDelayTimeEdit{nullptr};
    TimePeriod*       mDelayTimePeriod{nullptr};
    ComboBox*         mPresetsCombo{nullptr};
    PushButton*       mTimeZoneButton;
    QWidget*          mTimeZoneBox;           // contains label and time zone combo box
    TimeZoneCombo*    mTimeZone;
    KADateTime        mMinDateTime;           // earliest allowed date/time
    KADateTime        mMaxDateTime;           // latest allowed date/time
    KADateTime::Spec  mTimeSpec;              // time spec used
    int               mAnyTime;               // 0 = date/time is specified, 1 = only a date, -1 = uninitialised
    bool              mAnyTimeAllowed;        // 'mAnyTimeCheckBox' is enabled
    bool              mDeferring;             // being used to enter a deferral time
    bool              mMinDateTimeIsNow {false}; // earliest allowed date/time is the current time
    bool              mPastMax {false};       // current time is past the maximum date/time
    bool              mMinMaxTimeSet {false}; // limits have been set for the time edit control
};

// vim: et sw=4:
