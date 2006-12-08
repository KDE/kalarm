/*
 *  alarmtimewidget.cpp  -  alarm date/time entry widget
 *  Program:  kalarm
 *  Copyright © 2001-2006 by David Jarvie <software@astrojar.org.uk>
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

#include <QGroupBox>
#include <QGridLayout>

#include <kdialog.h>
#include <kmessagebox.h>
#include <klocale.h>

#include "buttongroup.h"
#include "checkbox.h"
#include "dateedit.h"
#include "datetime.h"
#include "preferences.h"
#include "radiobutton.h"
#include "synchtimer.h"
#include "timeedit.h"
#include "timespinbox.h"
#include "timezonecombo.h"
#include "alarmtimewidget.moc"

static const QTime time_23_59(23, 59);


const int AlarmTimeWidget::maxDelayTime = 99*60 + 59;    // < 100 hours

QString AlarmTimeWidget::i18n_w_TimeFromNow()     { return i18n("Time from no&w:"); }
QString AlarmTimeWidget::i18n_TimeAfterPeriod()
{
	return i18n("Enter the length of time (in hours and minutes) after "
	            "the current time to schedule the alarm.");
}


/******************************************************************************
* Construct a widget with a group box and title.
*/
AlarmTimeWidget::AlarmTimeWidget(const QString& groupBoxTitle, int mode, QWidget* parent)
	: QFrame(parent),
	  mMinDateTimeIsNow(false),
	  mPastMax(false),
	  mMinMaxTimeSet(false)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setMargin(0);
	QGroupBox* box = new QGroupBox(groupBoxTitle, this);
	layout->addWidget(box);
	init(mode, true);
}

/******************************************************************************
* Construct a widget without a group box or title.
*/
AlarmTimeWidget::AlarmTimeWidget(int mode, QWidget* parent)
	: QFrame(parent),
	  mMinDateTimeIsNow(false),
	  mPastMax(false),
	  mMinMaxTimeSet(false)
{
	init(mode, false);
}

void AlarmTimeWidget::init(int mode, bool hasTitle)
{
	static const QString recurText = i18n("For a simple repetition, enter the date/time of the first occurrence.\n"
	                                      "If a recurrence is configured, the start date/time will be adjusted "
	                                      "to the first recurrence on or after the entered date/time."); 
	static const QString tzText = i18n("This uses KAlarm's default time zone, set in the Preferences dialog.");

	mDeferring = mode & DEFER_TIME;
	mButtonGroup = new ButtonGroup(this);
	connect(mButtonGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(slotButtonSet(QAbstractButton*)));
	QGridLayout* topLayout = new QGridLayout(this);
	topLayout->setSpacing(KDialog::spacingHint());
	topLayout->setMargin(hasTitle ? KDialog::marginHint() : 0);

	// At time radio button
	mDateTimeRadio = new RadioButton((mDeferring ? i18n("&Defer to date/time:") : i18n("&Date/time")), this);
	mDateTimeRadio->setFixedSize(mDateTimeRadio->sizeHint());
	mDateTimeRadio->setWhatsThis(mDeferring ? i18n("Reschedule the alarm to the specified date and time.")
	                                        : i18n("Schedule the alarm at the specified date and time."));
	mButtonGroup->addButton(mDateTimeRadio);
	topLayout->addWidget(mDateTimeRadio, 1, 0, Qt::AlignLeft);

	int row = 1;
	mAnyTime = -1;    // last status is uninitialised
	if (mDeferring)
	{
		mDateRadio  = 0;
		mTimeZone   = 0;
		mNoTimeZone = 0;
		mAnyTimeAllowed = false;
	}
	else
	{
		// Date radio button
		mDateRadio = new RadioButton(i18n("Date"), this);
		mDateRadio->setFixedSize(mDateRadio->sizeHint());
		mDateRadio->setWhatsThis(i18n("Schedule the alarm on the specified date."));
		mButtonGroup->addButton(mDateRadio);
		topLayout->addWidget(mDateRadio, 2, 0, Qt::AlignLeft);
		++row;
		mAnyTimeAllowed = true;
	}

	// Date edit box
	mDateEdit = new DateEdit(this);
	mDateEdit->setFixedSize(mDateEdit->sizeHint());
	connect(mDateEdit, SIGNAL(dateChanged(const QDate&)), SLOT(dateTimeChanged()));
	static const QString enterDateText = i18n("Enter the date to schedule the alarm.");
	mDateEdit->setWhatsThis(QString("%1\n%2").arg(enterDateText).arg(mDeferring ? tzText : recurText));
	topLayout->addWidget(mDateEdit, 1, 1, Qt::AlignLeft);
	mDateTimeRadio->setFocusWidget(mDateEdit);
	if (mDateRadio)
		mDateRadio->setFocusWidget(mDateEdit);

	// Time edit box
	mTimeEdit = new TimeEdit(this);
	mTimeEdit->setFixedSize(mTimeEdit->sizeHint());
	connect(mTimeEdit, SIGNAL(valueChanged(int)), SLOT(dateTimeChanged()));
	static const QString enterTimeText = i18n("Enter the time to schedule the alarm.");
	mTimeEdit->setWhatsThis(QString("%1\n%2\n\n%3").arg(enterTimeText).arg(mDeferring ? tzText : recurText).arg(TimeSpinBox::shiftWhatsThis()));
	topLayout->addWidget(mTimeEdit, row, (mDateRadio ? 1 : 2), Qt::AlignLeft);
	++row;

	if (!mDeferring)
	{
		// Time zone selector
		mTimeZone = new TimeZoneCombo(this);
		mTimeZone->setMaxVisibleItems(15);
		mTimeZone->setWhatsThis(i18n("Select the time zone to use for this alarm."));
		topLayout->addWidget(mTimeZone, 1, 3, Qt::AlignLeft);

		// Time zone checkbox
		mNoTimeZone = new CheckBox(i18n("Ignore time &zone"), this);
		connect(mNoTimeZone, SIGNAL(toggled(bool)), SLOT(slotTimeZoneToggled(bool)));
		mNoTimeZone->setWhatsThis("<qt>" +
		      i18n("Check to use the local computer time, ignoring time zones.")
		    + "<p>"
		    + i18n("You are recommended not to use this option if the alarm has a "
		           "recurrence specified in hours/minutes. If you do, the alarm may "
		           "occur at unexpected times after daylight saving time shifts.")
		    + "</qt>");
		topLayout->addWidget(mNoTimeZone, 2, 3, Qt::AlignLeft);
	}

	// 'Time from now' radio button/label
	mAfterTimeRadio = new RadioButton((mDeferring ? i18n("Defer for time &interval:") : i18n_w_TimeFromNow()), this);
	mAfterTimeRadio->setFixedSize(mAfterTimeRadio->sizeHint());
	mAfterTimeRadio->setWhatsThis(mDeferring ? i18n("Reschedule the alarm for the specified time interval after now.")
	                                         : i18n("Schedule the alarm after the specified time interval from now."));
	mButtonGroup->addButton(mAfterTimeRadio);
	topLayout->addWidget(mAfterTimeRadio, row, 0, Qt::AlignLeft);

	// Delay time spin box
	mDelayTimeEdit = new TimeSpinBox(1, maxDelayTime, this);
	mDelayTimeEdit->setValue(1439);
	mDelayTimeEdit->setFixedSize(mDelayTimeEdit->sizeHint());
	connect(mDelayTimeEdit, SIGNAL(valueChanged(int)), SLOT(delayTimeChanged(int)));
	mDelayTimeEdit->setWhatsThis(mDeferring ? QString("%1\n\n%2").arg(i18n_TimeAfterPeriod()).arg(TimeSpinBox::shiftWhatsThis())
	                                        : QString("%1\n%2\n\n%3").arg(i18n_TimeAfterPeriod()).arg(recurText).arg(TimeSpinBox::shiftWhatsThis()));
	mAfterTimeRadio->setFocusWidget(mDelayTimeEdit);

	topLayout->setColumnStretch(2, 1);

	// Initialise the radio button statuses
	mDateTimeRadio->setChecked(true);

	// Timeout every minute to update alarm time fields.
	MinuteTimer::connect(this, SLOT(slotTimer()));
}

/******************************************************************************
* Set or clear read-only status for the controls
*/
void AlarmTimeWidget::setReadOnly(bool ro)
{
	if (mDateRadio)
		mDateRadio->setReadOnly(ro);
	mDateTimeRadio->setReadOnly(ro);
	mDateEdit->setReadOnly(ro);
	mTimeEdit->setReadOnly(ro);
	if (!mDeferring)
	{
		mTimeZone->setReadOnly(ro);
		mNoTimeZone->setReadOnly(ro);
	}
	mAfterTimeRadio->setReadOnly(ro);
	mDelayTimeEdit->setReadOnly(ro);
}

/******************************************************************************
* Select the "Time from now" radio button.
*/
void AlarmTimeWidget::selectTimeFromNow(int minutes)
{
	mAfterTimeRadio->setChecked(true);
//	mButtonGroup->slotButtonSet(mAfterTimeRadio);
	if (minutes > 0)
		mDelayTimeEdit->setValue(minutes);
}

/******************************************************************************
* Fetch the entered date/time.
* If 'checkExpired' is true and the entered value <= current time, an error occurs.
* If 'minsFromNow' is non-null, it is set to the number of minutes' delay selected,
* or to zero if a date/time was entered.
* In this case, if 'showErrorMessage' is true, output an error message.
* 'errorWidget' if non-null, is set to point to the widget containing the error.
* Reply = invalid date/time if error.
*/
KDateTime AlarmTimeWidget::getDateTime(int* minsFromNow, bool checkExpired, bool showErrorMessage, QWidget** errorWidget) const
{
	if (minsFromNow)
		*minsFromNow = 0;
	if (errorWidget)
		*errorWidget = 0;
	KDateTime now = KDateTime::currentUtcDateTime();
	now.setTime(QTime(now.time().hour(), now.time().minute(), 0));
	if (mAfterTimeRadio->isChecked())
	{
		if (!mDelayTimeEdit->isValid())
		{
			if (showErrorMessage)
				KMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18n("Invalid time"));
			if (errorWidget)
				*errorWidget = mDelayTimeEdit;
			return KDateTime();
		}
		int delayMins = mDelayTimeEdit->value();
		if (minsFromNow)
			*minsFromNow = delayMins;
		return now.addSecs(delayMins * 60).toTimeSpec(timeSpec());
	}
	else
	{
		bool dateOnly = mAnyTimeAllowed && mDateRadio && mDateRadio->isChecked();
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
			return KDateTime();
		}

		KDateTime result;
		if (dateOnly)
		{
			result = KDateTime(mDateEdit->date(), timeSpec());
			if (checkExpired  &&  result.date() < now.date())
			{
				if (showErrorMessage)
					KMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18n("Alarm date has already expired"));
				if (errorWidget)
					*errorWidget = mDateEdit;
				return KDateTime();
			}
		}
		else
		{
			result = KDateTime(mDateEdit->date(), mTimeEdit->time(), timeSpec());
			if (checkExpired  &&  result <= now.addSecs(1))
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
}

/******************************************************************************
* Get the time specification to use.
*/
KDateTime::Spec AlarmTimeWidget::timeSpec() const
{
	if (mDeferring)
		return mTimeSpec.isValid() ? mTimeSpec : KDateTime::LocalZone;
	if (mNoTimeZone->isChecked())
		return KDateTime::ClockTime;
	const KTimeZone* tz = mTimeZone->timeZone();
	return tz ? KDateTime::Spec(tz) : KDateTime::LocalZone;
}

/******************************************************************************
* Set the date/time.
*/
void AlarmTimeWidget::setDateTime(const DateTime& dt)
{
	if (dt.date().isValid())
	{
		mTimeEdit->setValue(dt.effectiveTime());
		mDateEdit->setDate(dt.date());
		dateTimeChanged();     // update the delay time edit box
	}
	else
	{
		mTimeEdit->setValid(false);
		mDateEdit->setInvalid();
		mDelayTimeEdit->setValid(false);
	}
	if (mDateRadio)
	{
		if (dt.isDateOnly())
		{
			mAnyTimeAllowed = true;
			mDateRadio->setChecked(true);
		}
		setAnyTime();
	}
	if (!mDeferring)
	{
		const KTimeZone* tz = dt.timeZone();
		mNoTimeZone->setChecked(!tz);
		mTimeZone->setTimeZone(tz ? tz : Preferences::timeZone());
	}
}

/******************************************************************************
* Set the minimum date/time to track the current time.
*/
void AlarmTimeWidget::setMinDateTimeIsCurrent()
{
	mMinDateTimeIsNow = true;
	mMinDateTime = KDateTime();
	KDateTime now = KDateTime::currentDateTime(timeSpec());
	mDateEdit->setMinDate(now.date());
	setMaxMinTimeIf(now);
}

/******************************************************************************
* Set the minimum date/time, adjusting the entered date/time if necessary.
* If 'dt' is invalid, any current minimum date/time is cleared.
*/
void AlarmTimeWidget::setMinDateTime(const KDateTime& dt)
{
	mMinDateTimeIsNow = false;
	mMinDateTime = dt;
	mDateEdit->setMinDate(dt.date());
	setMaxMinTimeIf(KDateTime::currentDateTime(dt.timeSpec()));
}

/******************************************************************************
* Set the maximum date/time, adjusting the entered date/time if necessary.
* If 'dt' is invalid, any current maximum date/time is cleared.
*/
void AlarmTimeWidget::setMaxDateTime(const DateTime& dt)
{
	mPastMax = false;
	if (dt.isValid()  &&  dt.isDateOnly())
		mMaxDateTime = dt.effectiveKDateTime().addSecs(24*3600 - 60);
	else
		mMaxDateTime = dt.effectiveKDateTime();
	mDateEdit->setMaxDate(mMaxDateTime.date());
	KDateTime now = KDateTime::currentDateTime(dt.timeSpec());
	setMaxMinTimeIf(now);
	setMaxDelayTime(now);
}

/******************************************************************************
* If the minimum and maximum date/times fall on the same date, set the minimum
* and maximum times in the time edit box.
*/
void AlarmTimeWidget::setMaxMinTimeIf(const KDateTime& now)
{
	int   mint = 0;
	QTime maxt = time_23_59;
	mMinMaxTimeSet = false;
	if (mMaxDateTime.isValid())
	{
		bool set = true;
		KDateTime minDT;
		if (mMinDateTimeIsNow)
			minDT = now.addSecs(60);
		else if (mMinDateTime.isValid())
			minDT = mMinDateTime;
		else
			set = false;
		if (set  &&  mMaxDateTime.date() == minDT.date())
		{
			// The minimum and maximum times are on the same date, so
			// constrain the time value.
			mint = minDT.time().hour()*60 + minDT.time().minute();
			maxt = mMaxDateTime.time();
			mMinMaxTimeSet = true;
		}
	}
	mTimeEdit->setMinimum(mint);
	mTimeEdit->setMaximum(maxt);
	mTimeEdit->setWrapping(!mint  &&  maxt == time_23_59);
}

/******************************************************************************
* Set the maximum value for the delay time edit box, depending on the maximum
* value for the date/time.
*/
void AlarmTimeWidget::setMaxDelayTime(const KDateTime& now)
{
	int maxVal = maxDelayTime;
	if (mMaxDateTime.isValid())
	{
		if (now.date().daysTo(mMaxDateTime.date()) < 100)    // avoid possible 32-bit overflow on secsTo()
		{
			KDateTime dt(now);
			dt.setTime(QTime(now.time().hour(), now.time().minute(), 0));   // round down to nearest minute
			maxVal = dt.secsTo(mMaxDateTime) / 60;
			if (maxVal > maxDelayTime)
				maxVal = maxDelayTime;
		}
	}
	mDelayTimeEdit->setMaximum(maxVal);
}

/******************************************************************************
* Set the status for whether a time is specified, or just a date.
*/
void AlarmTimeWidget::setAnyTime()
{
	int old = mAnyTime;
	mAnyTime = mAnyTimeAllowed && mDateRadio && mDateRadio->isChecked() ? 1 : 0;
	if (mAnyTime != old)
		emit dateOnlyToggled(mAnyTime);
}

/******************************************************************************
* Enable/disable the "date only" radio button.
*/
void AlarmTimeWidget::enableAnyTime(bool enable)
{
	if (mDateRadio)
	{
		mAnyTimeAllowed = enable;
		if (!enable && mDateRadio->isChecked())
			mDateTimeRadio->setChecked(true);
		mDateRadio->setEnabled(enable);
		setAnyTime();
	}
}

/******************************************************************************
* Called every minute to update the alarm time data entry fields.
* If the maximum date/time has been reached, a 'pastMax()' signal is emitted.
*/
void AlarmTimeWidget::slotTimer()
{
	KDateTime now;
	if (mMinDateTimeIsNow)
	{
		// Make sure that the minimum date is updated when the day changes
		now = KDateTime::currentDateTime(mMinDateTime.timeSpec());
		mDateEdit->setMinDate(now.date());
	}
	if (mMaxDateTime.isValid())
	{
		if (!now.isValid())
			now = KDateTime::currentDateTime(mMinDateTime.timeSpec());
		if (!mPastMax)
		{
			// Check whether the maximum date/time has now been reached
			if (now.date() >= mMaxDateTime.date())
			{
				// The current date has reached or has passed the maximum date
				if (now.date() > mMaxDateTime.date()
				||  !mAnyTime && now.time() > mTimeEdit->maxTime())
				{
					mPastMax = true;
					emit pastMax();
				}
				else if (mMinDateTimeIsNow  &&  !mMinMaxTimeSet)
				{
					// The minimum date/time tracks the clock, so set the minimum
					// and maximum times
					setMaxMinTimeIf(now);
				}
			}
		}
		setMaxDelayTime(now);
	}

	if (mAfterTimeRadio->isChecked())
		delayTimeChanged(mDelayTimeEdit->value());
	else
		dateTimeChanged();
}


/******************************************************************************
* Called when the radio button states have been changed.
* Updates the appropriate edit box.
*/
void AlarmTimeWidget::slotButtonSet(QAbstractButton*)
{
	bool dt = mDateTimeRadio->isChecked();
	bool d  = mDateRadio && mDateRadio->isChecked();
	mDateEdit->setEnabled(dt || d);
	mTimeEdit->setEnabled(dt);
	// Ensure that the value of the delay edit box is > 0.
	KDateTime at(mDateEdit->date(), mTimeEdit->time(), timeSpec());
	int minutes = (KDateTime::currentUtcDateTime().secsTo(at) + 59) / 60;
	if (minutes <= 0)
		mDelayTimeEdit->setValid(true);
	mDelayTimeEdit->setEnabled(mAfterTimeRadio->isChecked());
	setAnyTime();
}

/******************************************************************************
* Called after the mNoTimeZone checkbox has been toggled.
*/
void AlarmTimeWidget::slotTimeZoneToggled(bool on)
{
	mTimeZone->setEnabled(!on && !mAfterTimeRadio->isChecked());
}

/******************************************************************************
* Called when the date or time edit box values have changed.
* Updates the time delay edit box accordingly.
*/
void AlarmTimeWidget::dateTimeChanged()
{
	KDateTime dt(mDateEdit->date(), mTimeEdit->time(), timeSpec());
	int minutes = (KDateTime::currentUtcDateTime().secsTo(dt) + 59) / 60;
	bool blocked = mDelayTimeEdit->signalsBlocked();
	mDelayTimeEdit->blockSignals(true);     // prevent infinite recursion between here and delayTimeChanged()
	if (minutes <= 0  ||  minutes > mDelayTimeEdit->maximum())
		mDelayTimeEdit->setValid(false);
	else
		mDelayTimeEdit->setValue(minutes);
	mDelayTimeEdit->blockSignals(blocked);
}

/******************************************************************************
* Called when the delay time edit box value has changed.
* Updates the Date and Time edit boxes accordingly.
*/
void AlarmTimeWidget::delayTimeChanged(int minutes)
{
	if (mDelayTimeEdit->isValid())
	{
		QDateTime dt = KDateTime::currentUtcDateTime().addSecs(minutes * 60).toTimeSpec(timeSpec()).dateTime();
		bool blockedT = mTimeEdit->signalsBlocked();
		bool blockedD = mDateEdit->signalsBlocked();
		mTimeEdit->blockSignals(true);     // prevent infinite recursion between here and dateTimeChanged()
		mDateEdit->blockSignals(true);
		mTimeEdit->setValue(dt.time());
		mDateEdit->setDate(dt.date());
		mTimeEdit->blockSignals(blockedT);
		mDateEdit->blockSignals(blockedD);
	}
}
