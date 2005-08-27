/*
 *  reminder.cpp  -  reminder setting widget
 *  Program:  kalarm
 *  Copyright (C) 2003 - 2005 by David Jarvie <software@astrojar.org.uk>
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

#include <qlayout.h>
#include <qwhatsthis.h>

#include <kglobal.h>
#include <klocale.h>
#include <kdialog.h>
#include <kdebug.h>

#include "preferences.h"
#include "checkbox.h"
#include "timeselector.h"
#include "reminder.moc"


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString Reminder::i18n_first_recurrence_only()   { return i18n("Reminder for first recurrence only"); }
QString Reminder::i18n_u_first_recurrence_only() { return i18n("Reminder for first rec&urrence only"); }


Reminder::Reminder(const QString& caption, const QString& reminderWhatsThis, const QString& valueWhatsThis,
                   bool allowHourMinute, bool showOnceOnly, QWidget* parent, const char* name)
	: QFrame(parent, name),
	  mReadOnly(false),
	  mOnceOnlyEnabled(showOnceOnly)
{
	setFrameStyle(QFrame::NoFrame);
	QVBoxLayout* topLayout = new QVBoxLayout(this, 0, KDialog::spacingHint());

	mTime = new TimeSelector(caption, i18n("in advance"), reminderWhatsThis,
	                       valueWhatsThis, allowHourMinute, this, "timeOption");
	mTime->setFixedSize(mTime->sizeHint());
	connect(mTime, SIGNAL(toggled(bool)), SLOT(slotReminderToggled(bool)));
	topLayout->addWidget(mTime);

	if (showOnceOnly)
	{
		QBoxLayout* layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
		layout->addSpacing(3*KDialog::spacingHint());
		mOnceOnly = new CheckBox(i18n_u_first_recurrence_only(), this);
		mOnceOnly->setFixedSize(mOnceOnly->sizeHint());
		QWhatsThis::add(mOnceOnly, i18n("Display the reminder only before the first time the alarm is scheduled"));
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
	return mTime->minutes();
}

/******************************************************************************
*  Initialise the controls with a specified reminder time.
*/
void Reminder::setMinutes(int minutes, bool dateOnly)
{
	mTime->setMinutes(minutes, dateOnly, Preferences::defaultReminderUnits());
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
