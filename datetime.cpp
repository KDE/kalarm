/*
 *  datetime.cpp  -  alarm date/time entry widget
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <qlayout.h>
#include <qgroupbox.h>
#include <qhbox.h>
#include <qpushbutton.h>
#include <qwhatsthis.h>

#include <kdialog.h>
#include <kmessagebox.h>
#include <klocale.h>

#include "dateedit.h"
#include "timespinbox.h"
#include "checkbox.h"
#include "radiobutton.h"
#include "datetime.moc"


/******************************************************************************
*  Construct a widget with a group box and title.
*/
AlarmTimeWidget::AlarmTimeWidget(const QString& groupBoxTitle, int mode, QWidget* parent, const char* name)
	: ButtonGroup(groupBoxTitle, parent, name)
{
	init(mode);
}

/******************************************************************************
*  Construct a widget without a group box or title.
*/
AlarmTimeWidget::AlarmTimeWidget(int mode, QWidget* parent, const char* name)
	: ButtonGroup(parent, name)
{
	setFrameStyle(QFrame::NoFrame);
	init(mode);
}

void AlarmTimeWidget::init(int mode)
{
	connect(this, SIGNAL(buttonSet(int)), SLOT(slotButtonSet(int)));
	connect(this, SIGNAL(clicked(int)), SLOT(slotButtonClicked(int)));
	QBoxLayout* topLayout = new QVBoxLayout(this, 0, KDialog::spacingHint());
	if (!title().isEmpty())
		topLayout->setMargin(marginKDE2 + KDialog::marginHint());
	topLayout->addSpacing(fontMetrics().lineSpacing()/2);

	// At time radio button/label
	mAtTimeRadio = new RadioButton(((mode & DEFER_TIME) ? i18n("&Defer to date/time:") : i18n("At &date/time:")), this, "atTimeRadio");
	mAtTimeRadio->setFixedSize(mAtTimeRadio->sizeHint());
	QWhatsThis::add(mAtTimeRadio,
	                ((mode & DEFER_TIME) ? i18n("Reschedule the alarm to the specified date and time.")
	                                     : i18n("Schedule the alarm at the specified date and time.")));

	// Date edit box
	mDateEdit = new DateEdit(this);
	mDateEdit->setFixedSize(mDateEdit->sizeHint());
	QWhatsThis::add(mDateEdit, i18n("Enter the date to schedule the alarm."));
	connect(mDateEdit, SIGNAL(dateChanged(QDate)), SLOT(slotDateChanged(QDate)));

	// Time edit box and Any time checkbox
	QHBox* timeBox = new QHBox(this);
	timeBox->setSpacing(2*KDialog::spacingHint());
	mTimeEdit = new TimeSpinBox(timeBox);
	mTimeEdit->setValue(1439);
	mTimeEdit->setFixedSize(mTimeEdit->sizeHint());
	QWhatsThis::add(mTimeEdit, i18n("Enter the time to schedule the alarm.\n%1").arg(TimeSpinBox::shiftWhatsThis()));
	connect(mTimeEdit, SIGNAL(valueChanged(int)), SLOT(slotTimeChanged(int)));

	if (mode & DEFER_TIME)
	{
		mAnyTimeAllowed = false;
		mAnyTimeCheckBox = 0;
	}
	else
	{
		mAnyTimeAllowed = true;
		mAnyTimeCheckBox = new CheckBox(i18n("An&y time"), timeBox);
		mAnyTimeCheckBox->setFixedSize(mAnyTimeCheckBox->sizeHint());
		QWhatsThis::add(mAnyTimeCheckBox, i18n("Schedule the alarm for any time during the day"));
		connect(mAnyTimeCheckBox, SIGNAL(toggled(bool)), SLOT(anyTimeToggled(bool)));
	}

	// 'Time from now' radio button/label
	mAfterTimeRadio = new RadioButton(((mode & DEFER_TIME) ? i18n("Defer for time &interval:") : i18n("Time from no&w:")),
	                                  this, "afterTimeRadio");
	mAfterTimeRadio->setFixedSize(mAfterTimeRadio->sizeHint());
	QWhatsThis::add(mAfterTimeRadio,
	                ((mode & DEFER_TIME) ? i18n("Reschedule the alarm for the specified time interval after now.")
	                                     : i18n("Schedule the alarm after the specified time interval from now.")));

	// Delay time spin box
	mDelayTimeEdit = new TimeSpinBox(1, 99*60+59, this);
	mDelayTimeEdit->setValue(1439);
	mDelayTimeEdit->setFixedSize(mDelayTimeEdit->sizeHint());
	QWhatsThis::add(mDelayTimeEdit,
	      i18n("Enter the length of time (in hours and minutes) after the current time to schedule the alarm.\n%1")
	           .arg(TimeSpinBox::shiftWhatsThis()));
	connect(mDelayTimeEdit, SIGNAL(valueChanged(int)), SLOT(delayTimeChanged(int)));

	// Set up the layout, either narrow or wide
	if (mode & NARROW)
	{
		QGridLayout* grid = new QGridLayout(topLayout, 2, 2, KDialog::spacingHint());
		grid->addWidget(mAtTimeRadio, 0, 0);
		grid->addWidget(mDateEdit, 0, 1, Qt::AlignLeft);
		grid->addWidget(timeBox, 1, 1, Qt::AlignLeft);
		grid->setColStretch(2, 1);
		topLayout->addStretch();
		QBoxLayout* layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
		layout->addWidget(mAfterTimeRadio);
		layout->addWidget(mDelayTimeEdit);
		layout->addStretch();
	}
	else
	{
		QGridLayout* grid = new QGridLayout(topLayout, 2, 3, KDialog::spacingHint());
		grid->addWidget(mAtTimeRadio, 0, 0, Qt::AlignLeft);
		grid->addWidget(mDateEdit, 0, 1, Qt::AlignLeft);
		grid->addWidget(timeBox, 0, 2, Qt::AlignLeft);
		grid->setRowStretch(0, 1);
		grid->addWidget(mAfterTimeRadio, 1, 0, Qt::AlignLeft);
		grid->addWidget(mDelayTimeEdit, 1, 1, Qt::AlignLeft);
		grid->setColStretch(3, 1);
		topLayout->addStretch();
	}

	// Initialise the radio button statuses
	setButton(id(mAtTimeRadio));

	// Timeout every minute to update alarm time fields.
	// But first synchronise to one second after the minute boundary.
	int firstInterval = 61 - QTime::currentTime().second();
	mTimer.start(1000 * firstInterval);
	mTimerSyncing = (firstInterval != 60);
	connect(&mTimer, SIGNAL(timeout()), SLOT(slotTimer()));
}

/******************************************************************************
* Set or clear read-only status for the controls
*/
void AlarmTimeWidget::setReadOnly(bool ro)
{
	mAtTimeRadio->setReadOnly(ro);
	mDateEdit->setReadOnly(ro);
	mTimeEdit->setReadOnly(ro);
	if (mAnyTimeCheckBox)
		mAnyTimeCheckBox->setReadOnly(ro);
	mAfterTimeRadio->setReadOnly(ro);
	mDelayTimeEdit->setReadOnly(ro);
}

/******************************************************************************
*  Fetch the entered date/time.
*  If <= current time, and 'showErrorMessage' is true, output an error message.
*  Output: 'anyTime' is set true if no time was entered.
*  Reply = widget with the invalid value, if it is after the current time.
*/
QWidget* AlarmTimeWidget::getDateTime(QDateTime& dateTime, bool& anyTime, bool showErrorMessage) const
{
	QDateTime now = QDateTime::currentDateTime();
	if (mAtTimeRadio->isOn())
	{
		dateTime.setDate(mDateEdit->date());
		anyTime = mAnyTimeAllowed && mAnyTimeCheckBox && mAnyTimeCheckBox->isChecked();
		if (anyTime)
		{
			dateTime.setTime(QTime());
			if (dateTime.date() < now.date())
			{
				if (showErrorMessage)
					KMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18n("Alarm date has already expired"));
				return mDateEdit;
			}
		}
		else
		{
			dateTime.setTime(mTimeEdit->time());
			int seconds = now.time().second();
			if (dateTime <= now.addSecs(1 - seconds))
			{
				if (showErrorMessage)
					KMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18n("Alarm time has already expired"));
				return mTimeEdit;
			}
		}
	}
	else
	{
		dateTime = now.addSecs(mDelayTimeEdit->value() * 60);
		dateTime = dateTime.addSecs(-dateTime.time().second());
		anyTime = false;
	}
	return 0;
}

/******************************************************************************
*  Set the date/time.
*/
void AlarmTimeWidget::setDateTime(const QDateTime& dt, bool anyTime)
{
	mTimeEdit->setValue(dt.time().hour()*60 + dt.time().minute());
	mDateEdit->setDate(dt.date());
	dateTimeChanged();     // update the delay time edit box
	QDate now = QDate::currentDate();
	mDateEdit->setMinDate(dt.date() < now ? dt.date() : now);
	if (mAnyTimeCheckBox)
	{
		if (anyTime)
			mAnyTimeAllowed = true;
		mAnyTimeCheckBox->setChecked(anyTime);
	}
}

/******************************************************************************
*  Enable/disable the "any time" checkbox.
*/
void AlarmTimeWidget::enableAnyTime(bool enable)
{
	if (mAnyTimeCheckBox)
	{
		mAnyTimeAllowed = enable;
		bool at = mAtTimeRadio->isOn();
		mAnyTimeCheckBox->setEnabled(enable && at);
		if (at)
			mTimeEdit->setEnabled(!enable || !mAnyTimeCheckBox->isChecked());
	}
}

/******************************************************************************
*  Called every minute to update the alarm time data entry fields.
*/
void AlarmTimeWidget::slotTimer()
{
	if (mTimerSyncing)
	{
		// We've synced to the minute boundary. Now set timer to 1 minute intervals.
		mTimer.changeInterval(1000 * 60);
		mTimerSyncing = false;
	}
	if (mAtTimeRadio->isOn())
		dateTimeChanged();
	else
		delayTimeChanged(mDelayTimeEdit->value());
}


/******************************************************************************
*  Called when the At or After time radio button states have been set.
*  Updates the appropriate edit box.
*/
void AlarmTimeWidget::slotButtonSet(int)
{
	bool at = mAtTimeRadio->isOn();
	mDateEdit->setEnabled(at);
	mTimeEdit->setEnabled(at && (!mAnyTimeAllowed || !mAnyTimeCheckBox || !mAnyTimeCheckBox->isChecked()));
	if (mAnyTimeCheckBox)
		mAnyTimeCheckBox->setEnabled(at && mAnyTimeAllowed);
	// Ensure that the value of the delay edit box is > 0.
	QDateTime dt(mDateEdit->date(), mTimeEdit->time());
	int minutes = (QDateTime::currentDateTime().secsTo(dt) + 59) / 60;
	if (minutes <= 0)
		mDelayTimeEdit->setValid(true);
	mDelayTimeEdit->setEnabled(!at);
}

/******************************************************************************
*  Called when the At or After time radio button has been clicked.
*  Moves the focus to the appropriate date or time edit field.
*/
void AlarmTimeWidget::slotButtonClicked(int)
{
	if (mAtTimeRadio->isOn())
		mDateEdit->setFocus();
	else
		mDelayTimeEdit->setFocus();
}
/******************************************************************************
*  Called after the mAnyTimeCheckBox checkbox has been toggled.
*/
void AlarmTimeWidget::anyTimeToggled(bool on)
{
	mTimeEdit->setEnabled((!mAnyTimeAllowed || !on) && mAtTimeRadio->isOn());
}

/******************************************************************************
*  Called when the date or time edit box values have changed.
*  Updates the time delay edit box accordingly.
*/
void AlarmTimeWidget::dateTimeChanged()
{
	QDateTime dt(mDateEdit->date(), mTimeEdit->time());
	int minutes = (QDateTime::currentDateTime().secsTo(dt) + 59) / 60;
	bool blocked = mDelayTimeEdit->signalsBlocked();
	mDelayTimeEdit->blockSignals(true);     // prevent infinite recursion between here and delayTimeChanged()
	if (minutes <= 0  ||  minutes > mDelayTimeEdit->maxValue())
		mDelayTimeEdit->setValid(false);
	else
		mDelayTimeEdit->setValue(minutes);
	mDelayTimeEdit->blockSignals(blocked);
}

/******************************************************************************
*  Called when the delay time edit box value has changed.
*  Updates the Date and Time edit boxes accordingly.
*/
void AlarmTimeWidget::delayTimeChanged(int minutes)
{
	if (mDelayTimeEdit->valid())
	{
		QDateTime dt = QDateTime::currentDateTime().addSecs(minutes * 60);
		bool blockedT = mTimeEdit->signalsBlocked();
		bool blockedD = mDateEdit->signalsBlocked();
		mTimeEdit->blockSignals(true);     // prevent infinite recursion between here and dateTimeChanged()
		mDateEdit->blockSignals(true);
		mTimeEdit->setValue(dt.time().hour()*60 + dt.time().minute());
		mDateEdit->setDate(dt.date());
		mTimeEdit->blockSignals(blockedT);
		mDateEdit->blockSignals(blockedD);
	}
}
