/*
 *  reminder.cpp  -  reminder setting widget
 *  Program:  kalarm
 *  Copyright © 2003-2005,2007,2008 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

#include <kglobal.h>
#include <klocale.h>
#include <kdialog.h>
#include <kdatetime.h>
#include <kdebug.h>
#include <kcal/duration.h>

#include "preferences.h"
#include "checkbox.h"
#include "timeselector.h"
#include "reminder.moc"


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString Reminder::i18n_chk_FirstRecurrenceOnly()   { return i18nc("@option:check", "Reminder for first recurrence only"); }


Reminder::Reminder(const QString& reminderWhatsThis, const QString& valueWhatsThis,
                   bool allowHourMinute, bool showOnceOnly, QWidget* parent)
	: QFrame(parent),
	  mReadOnly(false),
	  mOnceOnlyEnabled(showOnceOnly)
{
	QVBoxLayout* topLayout = new QVBoxLayout(this);
	topLayout->setMargin(0);
	topLayout->setSpacing(KDialog::spacingHint());

	mTime = new TimeSelector(i18nc("@option:check", "Reminder:"), i18nc("@label", "in advance"),
	                         reminderWhatsThis, valueWhatsThis, allowHourMinute, this);
	mTime->setFixedSize(mTime->sizeHint());
	connect(mTime, SIGNAL(toggled(bool)), SLOT(slotReminderToggled(bool)));
	topLayout->addWidget(mTime, 0, Qt::AlignLeft);

	if (showOnceOnly)
	{
		QHBoxLayout* layout = new QHBoxLayout();
		layout->setMargin(0);
		layout->addSpacing(3*KDialog::spacingHint());
		topLayout->addLayout(layout);
		mOnceOnly = new CheckBox(i18n_chk_FirstRecurrenceOnly(), this);
		mOnceOnly->setFixedSize(mOnceOnly->sizeHint());
		mOnceOnly->setWhatsThis(i18nc("@info:whatsthis", "Display the reminder only before the first time the alarm is scheduled"));
		layout->addWidget(mOnceOnly);
		layout->addStretch();
	}
	else
		mOnceOnly = 0;
}

/******************************************************************************
*  Set the read-only status.
*/
void Reminder::setReadOnly(bool ro)
{
	if ((int)ro != (int)mReadOnly)
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
*  Specify whether the once-only checkbox is allowed to be enabled.
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
 */
int Reminder::minutes() const
{
	return mTime->period().asSeconds() / 60;
}

/******************************************************************************
*  Initialise the controls with a specified reminder time.
*/
void Reminder::setMinutes(int minutes, bool dateOnly)
{
	KCal::Duration period;
	if (minutes % (24*60))
		period = KCal::Duration(minutes * 60, KCal::Duration::Seconds);
	else
		period = KCal::Duration(minutes / (24*60), KCal::Duration::Days);
	mTime->setPeriod(period, dateOnly, Preferences::defaultReminderUnits());
}

/******************************************************************************
*  Set the advance reminder units to days if "Any time" is checked.
*/
void Reminder::setDateOnly(bool dateOnly)
{
	mTime->setDateOnly(dateOnly);
}

/******************************************************************************
*  Set the input focus on the count field.
*/
void Reminder::setFocusOnCount()
{
	mTime->setFocusOnCount();
}

/******************************************************************************
*  Called when the Reminder checkbox is toggled.
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
void Reminder::setDefaultUnits(const KDateTime& dt)
{
	if (mTime->isChecked())
		return;   // don't change units if reminder is already set
	TimePeriod::Units units;
	TimePeriod::Units currentUnits = mTime->units();
	if (KDateTime::currentDateTime(dt.timeSpec()).daysTo(dt) < 7)
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
