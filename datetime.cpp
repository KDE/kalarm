/*
 *  datetime.cpp  -  date and time spinbox widgets
 *  Program:  kalarm
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
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
 */

#include "kalarm.h"

#include <qlayout.h>
#include <qgroupbox.h>
#include <qradiobutton.h>
#include <qpushbutton.h>
#include <qvalidator.h>
#include <qwhatsthis.h>

#include <kglobal.h>
#include <kdialog.h>
#include <kmessagebox.h>
#include <klocale.h>

#include "datetime.h"
#include "datetime.moc"


AlarmTimeWidget::AlarmTimeWidget(const QString& groupBoxTitle, int deferSpacing, QWidget* parent, const char* name)
	: QWidget(parent, name),
	  enteredDateTimeChanged(false)
{
	init(groupBoxTitle, true, deferSpacing);
}

AlarmTimeWidget::AlarmTimeWidget(int deferSpacing, QWidget* parent, const char* name)
	: QWidget(parent, name),
	  enteredDateTimeChanged(false)
{
	init(QString(), false, deferSpacing);
}

void AlarmTimeWidget::init(const QString& groupBoxTitle, bool groupBox, int deferSpacing)
{
	QVBoxLayout* layout = new QVBoxLayout(this, 0, KDialog::spacingHint());
	QWidget* page = this;
	if (groupBox)
	{
		page = new QGroupBox(groupBoxTitle, this);
		layout->addWidget(page);
		layout = new QVBoxLayout(page, KDialog::spacingHint());
	}
	layout->addSpacing(fontMetrics().lineSpacing()/2);
	QGridLayout* grid = new QGridLayout(page, 2, 3, KDialog::spacingHint());
	layout->addLayout(grid);

	// At time radio button/label
	atTimeRadio = new QRadioButton((deferSpacing ? i18n("Defer to date/time:") : i18n("At date/time:")), page, "atTimeRadio");
	atTimeRadio->setFixedSize(atTimeRadio->sizeHint());
	connect(atTimeRadio, SIGNAL(toggled(bool)), this, SLOT(slotAtTimeToggled(bool)));
	QWhatsThis::add(atTimeRadio,
			(deferSpacing ? i18n("Reschedule the alarm to the specified date and time.")
	                            : i18n("Schedule the alarm at the specified date and time.")));
	grid->addWidget(atTimeRadio, 0, 0, (deferSpacing ? AlignRight : AlignLeft));

	// Date spin box
	dateEdit = new DateSpinBox(page);
	QSize size = dateEdit->sizeHint();
	dateEdit->setFixedSize(size);
	grid->addWidget(dateEdit, 0, 1, AlignHCenter);
	grid->setColStretch(0, 1);
	QWhatsThis::add(dateEdit, i18n("Enter the date to schedule the alarm message."));
	connect(dateEdit, SIGNAL(valueChanged(int)), this, SLOT(slotDateTimeChanged(int)));

	// Time spin box
	timeEdit = new TimeSpinBox(page);
	timeEdit->setValue(2399);
	size = timeEdit->sizeHint();
	timeEdit->setFixedSize(size);
	grid->addWidget(timeEdit, 0, 2, AlignRight);
	QWhatsThis::add(timeEdit, i18n("Enter the time to schedule the alarm message."));
	connect(timeEdit, SIGNAL(valueChanged(int)), this, SLOT(slotDateTimeChanged(int)));

	// 'Time from now' radio button/label
	afterTimeRadio = new QRadioButton((deferSpacing ? i18n("Defer for time interval:") : i18n("Time from now:")), page, "afterTimeRadio");
	afterTimeRadio->setFixedSize(afterTimeRadio->sizeHint());
	connect(afterTimeRadio, SIGNAL(toggled(bool)), this, SLOT(slotAfterTimeToggled(bool)));
	QWhatsThis::add(afterTimeRadio,
			(deferSpacing ? i18n("Reschedule the alarm for the specified time interval after now.")
	                            : i18n("Schedule the alarm after the specified time interval from now.")));
	grid->addMultiCellWidget(afterTimeRadio, 1, 1, 0, 1, AlignRight);

	if (deferSpacing)
	{
		// Defer button
		// The width of this button is too narrow, so set it to correspond
		// with the width of the original "Defer..." button
		QPushButton* deferButton = new QPushButton(i18n("&Defer"), page);
		int width  = deferButton->fontMetrics().boundingRect(deferButton->text()).width() + deferSpacing;
		int height = deferButton->sizeHint().height();
		deferButton->setFixedSize(QSize(width, height));
		connect(deferButton, SIGNAL(clicked()), this, SLOT(slotDefer()));
		QWhatsThis::add(deferButton, i18n("Defer the alarm until the specified time."));
		grid->addWidget(deferButton, 1, 0, AlignLeft);
	}

	// Delay time spin box
	delayTime = new TimeSpinBox(1, 99*60+59, page);
	delayTime->setValue(2399);
	size = delayTime->sizeHint();
	delayTime->setFixedSize(size);
	grid->addWidget(delayTime, 1, 2, AlignRight);
	QWhatsThis::add(delayTime,
	      i18n("Enter the length of time (in hours and minutes) after the current time to schedule the alarm message."));
	connect(delayTime, SIGNAL(valueChanged(int)), this, SLOT(slotDelayTimeChanged(int)));

	// Initialise the radio button statuses
	atTimeRadio->setChecked(false);    // toggle the button to ensure things are set up correctly
	afterTimeRadio->setChecked(true);
	atTimeRadio->setChecked(true);

	// Timeout every minute to update alarm time fields.
	// But first synchronise to one second after the minute boundary.
	int firstInterval = 61 - QTime::currentTime().second();
	timer.start(1000 * firstInterval);
	timerSyncing = (firstInterval != 60);
	connect(&timer, SIGNAL(timeout()), this, SLOT(slotTimer()));
}

/******************************************************************************
*  Fetch the entered date/time.
*  If <= current time, output an error message.
*  Reply = true if it is after the current time.
*/
bool AlarmTimeWidget::getDateTime(QDateTime& dateTime) const
{
	QDateTime now = QDateTime::currentDateTime();
	if (atTimeRadio->isOn())
	{
		dateTime.setDate(dateEdit->getDate());
		dateTime.setTime(timeEdit->getTime());
		int seconds = now.time().second();
		if (dateTime <= now.addSecs(1 - seconds))
		{
			KMessageBox::sorry(const_cast<AlarmTimeWidget*>(this), i18n("Message time has already expired"));
			return false;
		}
	}
	else
	{
		dateTime = now.addSecs(delayTime->value() * 60);
		dateTime = dateTime.addSecs(-dateTime.time().second());
	}
	return true;
}

/******************************************************************************
*  Set the date/time.
*/
void AlarmTimeWidget::setDateTime(const QDateTime& dt)
{
	timeEdit->setValue(dt.time().hour()*60 + dt.time().minute());
	dateEdit->setDate(dt.date());
	QDate now = QDate::currentDate();
	dateEdit->setMinValue(DateSpinBox::getDateValue(dt.date() < now ? dt.date() : now));
}

/******************************************************************************
*  Called when the defer button is clicked.
*/
void AlarmTimeWidget::slotDefer()
{
	emit deferred();
}

/******************************************************************************
*  Called every minute to update the alarm time data entry fields.
*/
void AlarmTimeWidget::slotTimer()
{
	if (timerSyncing)
	{
		// We've synced to the minute boundary. Now set timer to 1 minute intervals.
		timer.changeInterval(1000 * 60);
		timerSyncing = false;
	}
	if (atTimeRadio->isOn())
		slotDateTimeChanged(0);
	else
		slotDelayTimeChanged(delayTime->value());
}

/******************************************************************************
*  Called after the atTimeRadio button has been toggled.
*/
void AlarmTimeWidget::slotAtTimeToggled(bool on)
{
	if (on  &&  afterTimeRadio->isOn()
	||  !on  &&  !afterTimeRadio->isOn())
		afterTimeRadio->setChecked(!on);
	dateEdit->setEnabled(on);
	timeEdit->setEnabled(on);
}

/******************************************************************************
*  Called after the afterTimeRadio button has been toggled.
*  Ensures that the value of the delay edit box is > 0.
*/
void AlarmTimeWidget::slotAfterTimeToggled(bool on)
{
	if (on  &&  atTimeRadio->isOn()
	||  !on  &&  !atTimeRadio->isOn())
		atTimeRadio->setChecked(!on);
	QDateTime dt(dateEdit->getDate(), timeEdit->getTime());
	int minutes = (QDateTime::currentDateTime().secsTo(dt) + 59) / 60;
	if (minutes <= 0)
		delayTime->setValid(true);
	delayTime->setEnabled(on);
}

/******************************************************************************
*  Called when the date or time edit box values have changed.
*  Updates the time delay edit box accordingly.
*/
void AlarmTimeWidget::slotDateTimeChanged(int)
{
	if (!enteredDateTimeChanged)          // prevent infinite recursion !!
	{
		enteredDateTimeChanged = true;
		QDateTime dt(dateEdit->getDate(), timeEdit->getTime());
		int minutes = (QDateTime::currentDateTime().secsTo(dt) + 59) / 60;
		if (minutes <= 0  ||  minutes > delayTime->maxValue())
			delayTime->setValid(false);
		else
			delayTime->setValue(minutes);
		enteredDateTimeChanged = false;
	}
}

/******************************************************************************
*  Called when the date or time edit box values have changed.
*  Updates the Date and Time edit boxes accordingly.
*/
void AlarmTimeWidget::slotDelayTimeChanged(int minutes)
{
	if (delayTime->valid())
	{
		QDateTime dt = QDateTime::currentDateTime().addSecs(minutes * 60);
		timeEdit->setValue(dt.time().hour()*60 + dt.time().minute());
		dateEdit->setDate(dt.date());
	}
}


/*=============================================================================
=  class TimeSpinBox
=============================================================================*/
class TimeSpinBox::TimeValidator : public QValidator
{
	public:
		TimeValidator(int minMin, int maxMin, QWidget* parent, const char* name = 0L)
			: QValidator(parent, name), minMinute(minMin), maxMinute(maxMin) { }
		virtual State validate(QString&, int&) const;
		int  minMinute, maxMinute;
};


// Construct a wrapping 00:00 - 23:59 time spin box
TimeSpinBox::TimeSpinBox(QWidget* parent, const char* name)
	: SpinBox2(0, 1439, 1, 60, parent, name),
	  minimumValue(0),
	  invalid(false),
	  enteredSetValue(false)
{
	validator = new TimeValidator(0, 1439, this, "TimeSpinBox validator");
	setValidator(validator);
	setWrapping(true);
}

// Construct a non-wrapping time spin box
TimeSpinBox::TimeSpinBox(int minMinute, int maxMinute, QWidget* parent, const char* name)
	: SpinBox2(minMinute, maxMinute, 1, 60, parent, name),
	  minimumValue(minMinute),
	  invalid(false),
	  enteredSetValue(false)
{
	validator = new TimeValidator(minMinute, maxMinute, this, "TimeSpinBox validator");
	setValidator(validator);
}

QTime TimeSpinBox::getTime() const
{
	return QTime(value() / 60, value() % 60);
}

QString TimeSpinBox::mapValueToText(int v)
{
	QString s;
	s.sprintf("%02d:%02d", v/60, v%60);
	return s;
}

/******************************************************************************
 * Convert the user-entered text to a value in minutes.
 * The allowed format is [hour]:[minute], where hour and
 * minute must be non-blank.
 */
int TimeSpinBox::mapTextToValue(bool* ok)
{
	QString text = cleanText();
	int colon = text.find(':');
	if (colon >= 0)
	{
		QString hour   = text.left(colon).stripWhiteSpace();
		QString minute = text.mid(colon + 1).stripWhiteSpace();
		if (!hour.isEmpty()  &&  !minute.isEmpty())
		{
			bool okhour, okmin;
			int m = minute.toUInt(&okmin);
			int t = hour.toUInt(&okhour) * 60 + m;
			if (okhour  &&  okmin  &&  m < 60  &&  t >= minimumValue  &&  t <= maxValue())
			{
				*ok = true;
				return t;
			}
		}
	}
	*ok = false;
	return 0;
}

/******************************************************************************
 * Set the spin box as valid or invalid.
 * If newly invalid, the value is displayed as asterisks.
 * If newly valid, the value is set to the minimum value.
 */
void TimeSpinBox::setValid(bool valid)
{
	if (valid  &&  invalid)
	{
		invalid = false;
		if (value() < minimumValue)
			SpinBox2::setValue(minimumValue);
		setSpecialValueText(QString());
		setMinValue(minimumValue);
	}
	else if (!valid  &&  !invalid)
	{
		invalid = true;
		setMinValue(minimumValue - 1);
		setSpecialValueText(QString::fromLatin1("**:**"));
		SpinBox2::setValue(minimumValue - 1);
	}
}

/******************************************************************************
 * Set the spin box as valid or invalid.
 * If newly invalid, the value is displayed as asterisks.
 * If newly valid, the value is set to the minimum value.
 */
void TimeSpinBox::setValue(int value)
{
	if (!enteredSetValue)
	{
		enteredSetValue = true;
		if (invalid)
		{
			invalid = false;
			setSpecialValueText(QString());
			setMinValue(minimumValue);
		}
		SpinBox2::setValue(value);
		enteredSetValue = false;
	}
}

/******************************************************************************
 * Step the spin box value.
 * If it was invalid, set it valid and set the value to the minimum.
 */
void TimeSpinBox::stepUp()
{
	if (invalid)
		setValid(true);
	else
		SpinBox2::stepUp();
}

void TimeSpinBox::stepDown()
{
	if (invalid)
		setValid(true);
	else
		SpinBox2::stepDown();
}


/******************************************************************************
 * Validate the time spin box input.
 * The entered time must contain a colon, but hours and/or minutes may be blank.
 */
QValidator::State TimeSpinBox::TimeValidator::validate(QString& text, int& /*cursorPos*/) const
{
	QValidator::State state = QValidator::Acceptable;
	QString hour;
	int hr;
	int mn = 0;
	int colon = text.find(':');
	if (colon >= 0)
	{
		QString minute = text.mid(colon + 1).stripWhiteSpace();
		if (minute.isEmpty())
			state = QValidator::Intermediate;
		else
		{
			bool ok;
			if ((mn = minute.toUInt(&ok)) >= 60  ||  !ok)
				return QValidator::Invalid;
		}

		hour = text.left(colon).stripWhiteSpace();
	}
	else
	{
		state = QValidator::Intermediate;
		hour = text;
	}

	if (hour.isEmpty())
		return QValidator::Intermediate;
	bool ok;
	if ((hr = hour.toUInt(&ok)) > maxMinute/60  ||  !ok)
		return QValidator::Invalid;
	if (state == QValidator::Acceptable)
	{
		int t = hr * 60 + mn;
		if (t < minMinute  ||  t > maxMinute)
			return QValidator::Invalid;
	}
	return state;
}


/*=============================================================================
=  class DateSpinBox
=============================================================================*/
QDate  DateSpinBox::baseDate(2000, 1, 1);


DateSpinBox::DateSpinBox(QWidget* parent, const char* name)
   : QSpinBox(0, 0, 1, parent, name)
{
	QDate now = QDate::currentDate();
	QDate maxDate(now.year() + 100, 12, 31);
	setRange(0, baseDate.daysTo(maxDate));
}

QDate DateSpinBox::getDate()
{
	return baseDate.addDays(value());
}

int DateSpinBox::getDateValue(const QDate& date)
{
	return baseDate.daysTo(date);
}

QString DateSpinBox::mapValueToText(int v)
{
	QDate date = baseDate.addDays(v);
	return KGlobal::locale()->formatDate(date, true);
}

/*
 * Convert the user-entered text to a value in days.
 * The date must be in the range
 */
int DateSpinBox::mapTextToValue(bool* ok)
{
	QDate date = KGlobal::locale()->readDate(cleanText());
	int days = baseDate.daysTo(date);
	int minval = baseDate.daysTo(QDate::currentDate());
	if (days >= minval  &&  days <= maxValue())
	{
		*ok = true;
		return days;
	}
	*ok = false;
	return 0;
}
