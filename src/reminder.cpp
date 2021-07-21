/*
 *  reminder.cpp  -  reminder setting widget
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2003-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "reminder.h"

#include "preferences.h"
#include "timeselector.h"
#include "lib/checkbox.h"
#include "lib/combobox.h"
#include "kalarm_debug.h"

#include <KAlarmCal/KADateTime>
using namespace KAlarmCal;
#include <KCalendarCore/Duration>
using namespace KCalendarCore;

#include <KLocalizedString>

#include <QVBoxLayout>
#include <QHBoxLayout>


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString Reminder::i18n_chk_FirstRecurrenceOnly()   { return i18nc("@option:check", "Reminder for first recurrence only"); }

#define i18n_in_advance i18nc("@item:inlistbox", "in advance")


Reminder::Reminder(const QString& reminderWhatsThis, const QString& valueWhatsThis,
                   const QString& beforeAfterWhatsThis, bool allowHourMinute,
                   bool showOnceOnly, QWidget* parent)
    : QFrame(parent)
    , mOnceOnlyEnabled(showOnceOnly)
{
    auto topLayout = new QVBoxLayout(this);
    topLayout->setContentsMargins(0, 0, 0, 0);

    mTime = new TimeSelector(i18nc("@option:check", "Reminder:"), reminderWhatsThis,
                             valueWhatsThis, allowHourMinute, this);
    mTimeSignCombo = mTime->createSignCombo();
    mTimeSignCombo->addItem(i18n_in_advance);
    mTimeSignCombo->addItem(i18nc("@item:inlistbox", "afterwards"));
    mTimeSignCombo->setWhatsThis(beforeAfterWhatsThis);
    mTimeSignCombo->setCurrentIndex(0);   // default to "in advance"
    mTime->setFixedSize(mTime->sizeHint());
    connect(mTime, &TimeSelector::toggled, this, &Reminder::slotReminderToggled);
    connect(mTime, &TimeSelector::valueChanged, this, &Reminder::changed);
    connect(mTimeSignCombo, static_cast<void (ComboBox::*)(int)>(&ComboBox::currentIndexChanged), this, &Reminder::changed);
    topLayout->addWidget(mTime, 0, Qt::AlignLeft);

    if (showOnceOnly)
    {
        auto layout = new QHBoxLayout();
        layout->setContentsMargins(0, 0, 0, 0);
        const int indent = CheckBox::textIndent(mTime);
        if (layoutDirection() == Qt::LeftToRight)
            layout->setContentsMargins(indent, 0, 0, 0);
        else
            layout->setContentsMargins(0, 0, indent, 0);
        topLayout->addLayout(layout);
        mOnceOnly = new CheckBox(i18n_chk_FirstRecurrenceOnly(), this);
        mOnceOnly->setFixedSize(mOnceOnly->sizeHint());
        connect(mOnceOnly, &CheckBox::toggled, this, &Reminder::changed);
        mOnceOnly->setWhatsThis(i18nc("@info:whatsthis", "Display the reminder only for the first time the alarm is scheduled"));
        layout->addWidget(mOnceOnly);
        layout->addStretch();
    }
    else
        mOnceOnly = nullptr;
}

/******************************************************************************
* Allow or disallow advance reminder selection.
*/
void Reminder::setAfterOnly(bool afterOnly)
{
    if (afterOnly  &&  mTimeSignCombo->count() == 2)
        mTimeSignCombo->removeItem(0);
    else if (!afterOnly  &&  mTimeSignCombo->count() == 1)
        mTimeSignCombo->insertItem(0, i18n_in_advance);
}

/******************************************************************************
* Set the read-only status.
*/
void Reminder::setReadOnly(bool ro)
{
    if (ro != mReadOnly)
    {
        mReadOnly = ro;
        mTime->setReadOnly(mReadOnly);
        if (mOnceOnly)
            mOnceOnly->setReadOnly(mReadOnly);
    }
}

bool Reminder::isReminder() const
{
    return mTime->isChecked();
}

bool Reminder::isOnceOnly() const
{
    return mOnceOnly  &&  mOnceOnly->isEnabled()  &&  mOnceOnly->isChecked();
}

void Reminder::setOnceOnly(bool onceOnly)
{
    if (mOnceOnly)
        mOnceOnly->setChecked(onceOnly);
}

/******************************************************************************
* Specify whether the once-only checkbox is allowed to be enabled.
*/
void Reminder::enableOnceOnly(bool enable)
{
    if (mOnceOnly)
    {
        mOnceOnlyEnabled = enable;
        mOnceOnly->setEnabled(enable && mTime->isChecked());
    }
}

void Reminder::setMaximum(int hourmin, int days)
{
    mTime->setMaximum(hourmin, days);
}

/******************************************************************************
* Get the specified number of minutes in advance of the main alarm the
* reminder is to be.
* Reply > 0 if advance reminder
*       < 0 if reminder after main alarm
*       = 0 if no reminder.
*/
int Reminder::minutes() const
{
    int index = mTimeSignCombo->currentIndex();
    if (mTimeSignCombo->count() == 1)
        ++index;    // "in advance" is not available
    int sign = index ? -1 : 1;
    return mTime->period().asSeconds() * sign / 60;
}

/******************************************************************************
* Initialise the controls with a specified reminder time.
*/
void Reminder::setMinutes(int minutes, bool dateOnly)
{
    bool neg = (minutes < 0);
    minutes = abs(minutes);
    Duration period;
    if (minutes % (24*60))
        period = Duration(minutes * 60, Duration::Seconds);
    else
        period = Duration(minutes / (24*60), Duration::Days);
    mTime->setPeriod(period, dateOnly, Preferences::defaultReminderUnits());
    mTimeSignCombo->setCurrentIndex(neg ? 1 : 0);
}

/******************************************************************************
* Set the reminder units to days if "Any time" is checked.
*/
void Reminder::setDateOnly(bool dateOnly)
{
    mTime->setDateOnly(dateOnly);
}

/******************************************************************************
* Set the input focus on the count field.
*/
void Reminder::setFocusOnCount()
{
    mTime->setFocusOnCount();
}

/******************************************************************************
* Called when the Reminder checkbox is toggled.
*/
void Reminder::slotReminderToggled(bool on)
{
    if (mOnceOnly)
        mOnceOnly->setEnabled(on && mOnceOnlyEnabled);
}

/******************************************************************************
* Called when the start time relating to the reminder has changed.
* Sets the default reminder time units appropriately, if no reminder time is
* currently set.
*/
void Reminder::setDefaultUnits(const KADateTime& dt)
{
    if (mTime->isChecked())
        return;   // don't change units if reminder is already set
    TimePeriod::Units units;
    TimePeriod::Units currentUnits = mTime->units();
    if (KADateTime::currentDateTime(dt.timeSpec()).daysTo(dt) < 7)
    {
        if (currentUnits == TimePeriod::Minutes  ||  currentUnits == TimePeriod::HoursMinutes)
            return;
        units = (Preferences::defaultReminderUnits() == TimePeriod::Minutes)
              ? TimePeriod::Minutes : TimePeriod::HoursMinutes;
    }
    else
    {
        if (currentUnits == TimePeriod::Days  ||  currentUnits == TimePeriod::Weeks)
            return;
        units = TimePeriod::Days;
    }
    mTime->setUnits(units);
}

// vim: et sw=4:
