/*
 *  alarmtimewidget.cpp  -  alarm date/time entry widget
 *  Program:  kalarm
 *  (C) 2001, 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include "datetime.h"
#include "dateedit.h"
#include "timespinbox.h"
#include "checkbox.h"
#include "radiobutton.h"
#include "alarmtimewidget.moc"


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
	static QString enterDateText = i18n("Enter the date to schedule the alarm.");
	static QString enterTimeText = i18n("Enter the time to schedule the alarm.");
	static QString recurText = i18n("If a recurrence is configured, the start date/time will be adjusted "
	                                    "to the first recurrence on or after the entered date/time."); 
	static QString lengthText = i18n("Enter the length of time (in hours and minutes) after the current "
	                                 "time to schedule the alarm.");

	connect(this, SIGNAL(buttonSet(int)), SLOT(slotButtonSet(int)));
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
	connect(mDateEdit, SIGNAL(dateChanged(QDate)), SLOT(dateTimeChanged()));
	QWhatsThis::add(mDateEdit, ((mode & DEFER_TIME) ? enterDateText
	                   : i18n("%1\n%2").arg(enterDateText).arg(recurText)));
	mAtTimeRadio->setFocusWidget(mDateEdit);

	// Time edit box and Any time checkbox
	QHBox* timeBox = new QHBox(this);
	timeBox->setSpacing(2*KDialog::spacingHint());
	mTimeEdit = new TimeSpinBox(timeBox);
	mTimeEdit->setValue(1439);
	mTimeEdit->setFixedSize(mTimeEdit->sizeHint());
	connect(mTimeEdit, SIGNAL(valueChanged(int)), SLOT(dateTimeChanged()));
	QWhatsThis::add(mTimeEdit,
	                ((mode & DEFER_TIME) ? i18n("%1\n%2").arg(enterTimeText).arg(TimeSpinBox::shiftWhatsThis())
	                   : i18n("%1\n%2\n%3").arg(enterTimeText).arg(recurText).arg(TimeSpinBox::shiftWhatsThis())));

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
		connect(mAnyTimeCheckBox, SIGNAL(toggled(bool)), SLOT(slotAnyTimeToggled(bool)));
		QWhatsThis::add(mAnyTimeCheckBox, i18n("Schedule the alarm for any time during the day"));
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
	connect(mDelayTimeEdit, SIGNAL(valueChanged(int)), SLOT(delayTimeChanged(int)));
	QWhatsThis::add(mDelayTimeEdit,
	                ((mode & DEFER_TIME) ? i18n("%1\n%2").arg(lengthText).arg(TimeSpinBox::shiftWhatsThis())
	                                     : i18n("%1\n%2\n%3").arg(lengthText).arg(recurText).arg(TimeSpinBox::shiftWhatsThis())));
	mAfterTimeRadio->setFocusWidget(mDelayTimeEdit);

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
*  If non-null, 'errorWidget' is set to point to the widget containing the error.
*  Reply = invalid date/time if error.
*/
DateTime AlarmTimeWidget::getDateTime(bool showErrorMessage, QWidget** errorWidget) const
{
	if (errorWidget)
		*errorWidget = 0;
	QTime nowt = QTime::currentTime();
	QDateTime now(QDate::currentDate(), QTime(nowt.hour(), nowt.minute()));
	if (mAtTimeRadio->isOn())
	{
		bool anyTime = mAnyTimeAllowed && mAnyTimeCheckBox && mAnyTimeCheckBox->isChecked();
		if (!mDateEdit->isValid()  ||  !mTimeEdit->isValid())
		{
			// The date and/or time is invalid
			if (!mDateEdit->isValid())
			{
				if (showErrorMessage)
					KMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18n("Invalid date"));
				if (errorWidget)
					*errorWidget = mDateEdit;
			}
			else
			{
				if (showErrorMessage)
					KMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18n("Invalid time"));
				if (errorWidget)
					*errorWidget = mTimeEdit;
			}
			return DateTime();
		}

		DateTime result;
		if (anyTime)
		{
			result = mDateEdit->date();
			if (result.date() < now.date())
			{
				if (showErrorMessage)
					KMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18n("Alarm date has already expired"));
				if (errorWidget)
					*errorWidget = mDateEdit;
				return DateTime();
			}
		}
		else
		{
			result.set(mDateEdit->date(), mTimeEdit->time());
			if (result <= now.addSecs(1))
			{
				if (showErrorMessage)
					KMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18n("Alarm time has already expired"));
				if (errorWidget)
					*errorWidget = mTimeEdit;
				return DateTime();
			}
		}
		return result;
	}
	else
	{
		if (!mDelayTimeEdit->isValid())
		{
			if (showErrorMessage)
				KMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18n("Invalid time"));
			if (errorWidget)
				*errorWidget = mDelayTimeEdit;
			return DateTime();
		}
		return now.addSecs(mDelayTimeEdit->value() * 60);
	}
}

/******************************************************************************
*  Set the date/time.
*/
void AlarmTimeWidget::setDateTime(const DateTime& dt)
{
	if (dt.date().isValid())
	{
		QTime t = dt.time();
		mTimeEdit->setValue(t.hour()*60 + t.minute());
		mDateEdit->setDate(dt.date());
		dateTimeChanged();     // update the delay time edit box
		QDate now = QDate::currentDate();
		mDateEdit->setMinDate(dt.date() < now ? dt.date() : now);
	}
	else
	{
		mTimeEdit->setValid(false);
		mDateEdit->setValid(false);
		mDelayTimeEdit->setValid(false);
	}
	if (mAnyTimeCheckBox)
	{
		bool anyTime = dt.isDateOnly();
		if (anyTime)
			mAnyTimeAllowed = true;
		mAnyTimeCheckBox->setChecked(anyTime);
		setAnyTime();
	}
}

/******************************************************************************
*  Set the status for whether a time is specified, or just a date.
*/
void AlarmTimeWidget::setAnyTime()
{
	bool old = mAnyTime;
	mAnyTime = mAtTimeRadio->isOn() && mAnyTimeAllowed && mAnyTimeCheckBox && mAnyTimeCheckBox->isChecked();
	if (mAnyTime != old)
		emit anyTimeToggled(mAnyTime);
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
		setAnyTime();
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
	setAnyTime();
}

/******************************************************************************
*  Called after the mAnyTimeCheckBox checkbox has been toggled.
*/
void AlarmTimeWidget::slotAnyTimeToggled(bool on)
{
	mTimeEdit->setEnabled((!mAnyTimeAllowed || !on) && mAtTimeRadio->isOn());
	setAnyTime();
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
	if (mDelayTimeEdit->isValid())
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
