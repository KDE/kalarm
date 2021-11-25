/*
 *  reminder.h  -  reminder setting widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2003-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QFrame>

namespace KAlarmCal { class KADateTime; }
class TimeSelector;
class CheckBox;
class ComboBox;


class Reminder : public QFrame
{
    Q_OBJECT
public:
    Reminder(const QString& reminderWhatsThis, const QString& valueWhatsThis,
             const QString& beforeAfterWhatsThis, bool allowHourMinute,
             bool showOnceOnly, QWidget* parent);
    void           setAfterOnly(bool after);
    bool           isReminder() const;
    bool           isOnceOnly() const;
    int            minutes() const;
    void           setMinutes(int minutes, bool dateOnly);
    void           setReadOnly(bool);
    void           setDateOnly(bool dateOnly);
    void           setMaximum(int hourmin, int days);
    void           setFocusOnCount();
    void           setOnceOnly(bool);
    void           enableOnceOnly(bool enable);

    static QString i18n_chk_FirstRecurrenceOnly();    // text of 'Reminder for first recurrence only' checkbox

public Q_SLOTS:
    void           setDefaultUnits(const KAlarmCal::KADateTime&);

Q_SIGNALS:
    void           changed();

private Q_SLOTS:
    void           slotReminderToggled(bool);

private:
    TimeSelector*  mTime;
    CheckBox*      mOnceOnly;
    ComboBox*      mTimeSignCombo;
    bool           mReadOnly {false};   // the widget is read only
    bool           mOnceOnlyEnabled;    // 'mOnceOnly' checkbox is allowed to be enabled
};

// vim: et sw=4:
