/*
 *  timespinbox.cpp  -  time spinbox widget
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
 */

#include "kalarm.h"

#include <qvalidator.h>
#include <klocale.h>

#include "timespinbox.moc"


class TimeSpinBox::TimeValidator : public QValidator
{
	public:
		TimeValidator(int minMin, int maxMin, QWidget* parent, const char* name = 0)
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
	setShiftSteps(5, 360);    // shift-left button increments 5 min / 6 hours
	setSelectOnStep(false);
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
	setShiftSteps(5, 360);    // shift-left button increments 5 min / 6 hours
	setSelectOnStep(false);
}

QString TimeSpinBox::shiftWhatsThis()
{
	return i18n("Press the Shift key while clicking the spin buttons to adjust the time by a larger step (6 hours / 5 minutes).");
}

QTime TimeSpinBox::time() const
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
 * Set the spin box's value.
 */
void TimeSpinBox::setValue(int minutes)
{
	if (!enteredSetValue)
	{
		enteredSetValue = true;
		if (minutes > maxValue())
			setValid(false);
		else
		{
			if (invalid)
			{
				invalid = false;
				setSpecialValueText(QString());
				setMinValue(minimumValue);
			}
			SpinBox2::setValue(minutes);
			enteredSetValue = false;
		}
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

bool TimeSpinBox::isValid() const
{
	return value() >= minimumValue;
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
