/*
 *  recurrenceedit.cpp  -  widget to edit the event's recurrence definition
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
 *
 *  Based on KOrganizer module koeditorrecurrence.cpp,
    Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    As a special exception, permission is given to link this program
    with any edition of Qt, and distribute the resulting executable,
    without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <qtooltip.h>
#include <qlayout.h>
#include <qvbox.h>
#include <qwidgetstack.h>
#include <qframe.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qwhatsthis.h>

#include <kglobal.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kdialog.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include <libkcal/event.h>

#include "kalarmapp.h"
#include "prefsettings.h"
#include "datetime.h"
#include "dateedit.h"
#include "timespinbox.h"
#include "spinbox.h"
#include "checkbox.h"
#include "combobox.h"
#include "radiobutton.h"
#include "msgevent.h"
#include "buttongroup.h"
using namespace KCal;

#include "recurrenceedit.moc"

static const char* const ordinal[] = {
	I18N_NOOP("1st"),  I18N_NOOP("2nd"),  I18N_NOOP("3rd"),  I18N_NOOP("4th"),
	I18N_NOOP("5th"),  I18N_NOOP("6th"),  I18N_NOOP("7th"),  I18N_NOOP("8th"),
	I18N_NOOP("9th"),  I18N_NOOP("10th"), I18N_NOOP("11th"), I18N_NOOP("12th"),
	I18N_NOOP("13th"), I18N_NOOP("14th"), I18N_NOOP("15th"), I18N_NOOP("16th"),
	I18N_NOOP("17th"), I18N_NOOP("18th"), I18N_NOOP("19th"), I18N_NOOP("20th"),
	I18N_NOOP("21st"), I18N_NOOP("22nd"), I18N_NOOP("23rd"), I18N_NOOP("24th"),
	I18N_NOOP("25th"), I18N_NOOP("26th"), I18N_NOOP("27th"), I18N_NOOP("28th"),
	I18N_NOOP("29th"), I18N_NOOP("30th"), I18N_NOOP("31st")
};


RecurrenceEdit::RecurrenceEdit(bool readOnly, QWidget* parent, const char* name)
	: QFrame(parent, name),
	  noEmitTypeChanged(true),
	  mReadOnly(readOnly)
{
	QBoxLayout* layout;
	QVBoxLayout* topLayout = new QVBoxLayout(this, marginKDE2, KDialog::spacingHint());

	// Create the recurrence rule Group box which holds the recurrence period
	// selection buttons, and the weekly, monthly and yearly recurrence rule
	// frames which specify options individual to each of these distinct
	// sections of the recurrence rule. Each frame is made visible by the
	// selection of its corresponding radio button.

#if KDE_VERSION >= 290
	recurGroup = new QGroupBox(1, Qt::Vertical, i18n("Recurrence Rule"), this, "recurGroup");
#else
	recurGroup = new QGroupBox(i18n("Recurrence Rule"), this, "recurGroup");
	layout = new QVBoxLayout(recurGroup, KDialog::marginHint(), KDialog::spacingHint());
	layout->addSpacing(fontMetrics().lineSpacing()/2);
	QBoxLayout* boxLayout = new QHBoxLayout(layout);
#endif
	topLayout->addWidget(recurGroup);
	ruleFrame = new QFrame(recurGroup, "ruleFrame");
#if KDE_VERSION < 290
	boxLayout->addWidget(ruleFrame);
#endif
	layout = new QVBoxLayout(ruleFrame, 0);
	layout->addSpacing(KDialog::spacingHint()/2);

	layout = new QHBoxLayout(layout, 0);
	ruleButtonGroup = new ButtonGroup(1, Qt::Horizontal, ruleFrame);
	ruleButtonGroup->setInsideMargin(0);
	ruleButtonGroup->setFrameStyle(QFrame::NoFrame);
	layout->addWidget(ruleButtonGroup);
	connect(ruleButtonGroup, SIGNAL(buttonSet(int)), SLOT(periodClicked(int)));

	recurEveryLabel = new QLabel(i18n("Recur e&very:"), ruleButtonGroup);
	recurEveryLabel->setFixedSize(recurEveryLabel->sizeHint());
	recurFrequencyStack = new QWidgetStack(ruleButtonGroup);
	recurFrequency = new SpinBox(1, 999, 1, ruleButtonGroup);
	recurFrequency->setLineShiftStep(10);
	recurFrequency->setSelectOnStep(false);
	recurFrequency->setReadOnly(mReadOnly);
	recurFrequencyStack->addWidget(recurFrequency, 0);
	recurHourMinFrequency = new TimeSpinBox(1, 99*60+59, ruleButtonGroup);
	recurHourMinFrequency->setReadOnly(mReadOnly);
	QWhatsThis::add(recurHourMinFrequency,
	      i18n("Enter the time (in hours and minutes) between repetitions of the alarm.\n%1")
	           .arg(TimeSpinBox::shiftWhatsThis()));
	recurFrequencyStack->addWidget(recurHourMinFrequency, 1);
	QSize size = recurFrequency->sizeHint().expandedTo(recurHourMinFrequency->sizeHint());
	recurFrequency->setFixedSize(size);
	recurHourMinFrequency->setFixedSize(size);
	recurFrequencyStack->setFixedSize(size);

	subdailyButton = new RadioButton(i18n("Ho&urs/Minutes"), ruleButtonGroup);
	subdailyButton->setFixedSize(subdailyButton->sizeHint());
	subdailyButton->setReadOnly(mReadOnly);
	QWhatsThis::add(subdailyButton,
	      i18n("Set the alarm repetition interval to the number of hours and minutes entered"));

	dailyButton = new RadioButton(i18n("&Days"), ruleButtonGroup);
	dailyButton->setFixedSize(dailyButton->sizeHint());
	dailyButton->setReadOnly(mReadOnly);
	QWhatsThis::add(dailyButton,
	      i18n("Set the alarm repetition interval to the number of days entered"));

	weeklyButton = new RadioButton(i18n("&Weeks"), ruleButtonGroup);
	weeklyButton->setFixedSize(weeklyButton->sizeHint());
	weeklyButton->setReadOnly(mReadOnly);
	QWhatsThis::add(weeklyButton,
	      i18n("Set the alarm repetition interval to the number of weeks entered"));

	monthlyButton = new RadioButton(i18n("&Months"), ruleButtonGroup);
	monthlyButton->setFixedSize(monthlyButton->sizeHint());
	monthlyButton->setReadOnly(mReadOnly);
	QWhatsThis::add(monthlyButton,
	      i18n("Set the alarm repetition interval to the number of months entered"));

	yearlyButton = new RadioButton(i18n("&Years"), ruleButtonGroup);
	yearlyButton->setFixedSize(yearlyButton->sizeHint());
	yearlyButton->setReadOnly(mReadOnly);
	QWhatsThis::add(yearlyButton,
	      i18n("Set the alarm repetition interval to the number of years entered"));

	subdailyButtonId = ruleButtonGroup->id(subdailyButton);
	dailyButtonId    = ruleButtonGroup->id(dailyButton);
	weeklyButtonId   = ruleButtonGroup->id(weeklyButton);
	monthlyButtonId  = ruleButtonGroup->id(monthlyButton);
	yearlyButtonId   = ruleButtonGroup->id(yearlyButton);

	QBoxLayout* lay = new QVBoxLayout(layout);

	lay->addStretch();
	layout = new QHBoxLayout(lay);

	layout->addSpacing(KDialog::marginHint());
	QFrame* divider = new QFrame(ruleFrame);
	divider->setFrameStyle(QFrame::VLine | QFrame::Sunken);
	layout->addWidget(divider);

	initNone();
	initWeekly();
	initMonthly();
	initYearly();

	ruleStack = new QWidgetStack(ruleFrame);
	layout->addWidget(ruleStack);
	layout->addStretch(1);
	ruleStack->addWidget(noneFrame, 0);
	ruleStack->addWidget(weeklyFrame, 1);
	ruleStack->addWidget(monthlyFrame, 2);
	ruleStack->addWidget(yearlyFrame, 3);

	// Create the recurrence range group which contains the controls
	// which specify how long the recurrence is to last.

	rangeButtonGroup = new QButtonGroup(i18n("Recurrence End"), this, "rangeButtonGroup");
	topLayout->addWidget(rangeButtonGroup);

	QVBoxLayout* vlayout = new QVBoxLayout(rangeButtonGroup, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	vlayout->addSpacing(fontMetrics().lineSpacing()/2);
	noEndDateButton = new RadioButton(i18n("No &end"), rangeButtonGroup);
	noEndDateButton->setFixedSize(noEndDateButton->sizeHint());
	noEndDateButton->setReadOnly(mReadOnly);
	connect(noEndDateButton, SIGNAL(toggled(bool)), SLOT(disableRange(bool)));
	QWhatsThis::add(noEndDateButton, i18n("Repeat the alarm indefinitely"));
	vlayout->addWidget(noEndDateButton, 1, Qt::AlignLeft);
	size = noEndDateButton->size();

	layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	repeatCountButton = new RadioButton(i18n("End a&fter:"), rangeButtonGroup);
	repeatCountButton->setReadOnly(mReadOnly);
	connect(repeatCountButton, SIGNAL(toggled(bool)), SLOT(enableDurationRange(bool)));
	QWhatsThis::add(repeatCountButton,
	      i18n("Repeat the alarm for the number of times specified"));
	mRepeatCountEntry = new SpinBox(1, 9999, 1, rangeButtonGroup);
	mRepeatCountEntry->setFixedSize(mRepeatCountEntry->sizeHint());
	mRepeatCountEntry->setLineShiftStep(10);
	mRepeatCountEntry->setSelectOnStep(false);
	mRepeatCountEntry->setReadOnly(mReadOnly);
	QWhatsThis::add(mRepeatCountEntry,
	      i18n("Enter the total number of times to trigger the alarm"));
	repeatCountLabel = new QLabel(i18n("occurrence(s)"), rangeButtonGroup);
	repeatCountLabel->setFixedSize(repeatCountLabel->sizeHint());
	layout->addWidget(repeatCountButton);
	layout->addSpacing(KDialog::spacingHint());
	layout->addWidget(mRepeatCountEntry);
	layout->addWidget(repeatCountLabel);
	layout->addStretch();
	size = size.expandedTo(repeatCountButton->sizeHint());

	layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	endDateButton = new RadioButton(i18n("End &by:"), rangeButtonGroup);
	endDateButton->setReadOnly(mReadOnly);
	connect(endDateButton, SIGNAL(toggled(bool)), SLOT(enableDateRange(bool)));
	QWhatsThis::add(endDateButton,
	      i18n("Repeat the alarm until the date/time specified"));
	endDateEdit = new DateEdit(rangeButtonGroup);
	endDateEdit->setFixedSize(endDateEdit->sizeHint());
	endDateEdit->setReadOnly(mReadOnly);
	QWhatsThis::add(endDateEdit,
	      i18n("Enter the last date to repeat the alarm"));
	endTimeEdit = new TimeSpinBox(rangeButtonGroup);
	endTimeEdit->setFixedSize(endTimeEdit->sizeHint());
	endTimeEdit->setReadOnly(mReadOnly);
	QWhatsThis::add(endTimeEdit,
	      i18n("Enter the last time to repeat the alarm.\n%1").arg(TimeSpinBox::shiftWhatsThis()));
	layout->addWidget(endDateButton);
	layout->addSpacing(KDialog::spacingHint());
	layout->addWidget(endDateEdit);
	layout->addWidget(endTimeEdit);
	layout->addStretch();
	size = size.expandedTo(endDateButton->sizeHint());

	// Line up the widgets to the right of the radio buttons
	repeatCountButton->setFixedSize(size);
	endDateButton->setFixedSize(size);

	topLayout->addStretch();
	noEmitTypeChanged = false;
}

void RecurrenceEdit::initNone()
{
	noneFrame = new QFrame(ruleFrame);
	noneFrame->setFrameStyle(QFrame::NoFrame);
}

/******************************************************************************
 * Set up the weekly recurrence dialog controls.
 */
void RecurrenceEdit::initWeekly()
{
	weeklyFrame = new QFrame(ruleFrame);
	weeklyFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* topLayout = new QVBoxLayout(weeklyFrame);
	topLayout->addStretch();
	QGridLayout* layout = new QGridLayout(topLayout, 7, 5, KDialog::spacingHint());
	layout->setRowStretch(0, 1);
	layout->setColStretch(0, 1);

	QLabel* label = new QLabel(i18n("On: Tuesday", "O&n:"), weeklyFrame);
	label->setFixedSize(label->sizeHint());
	layout->addWidget(label, 0, 1, Qt::AlignRight);
	layout->addColSpacing(2, 2*KDialog::spacingHint());
	for (int i = 0;  i < 7;  ++i)
	{
		dayBox[i] = new CheckBox(KGlobal::locale()->weekDayName(i + 1), weeklyFrame);    // starts Monday
		dayBox[i]->setFixedSize(dayBox[i]->sizeHint());
		dayBox[i]->setReadOnly(mReadOnly);
		QWhatsThis::add(dayBox[i],
		      i18n("Select the day(s) of the week on which to repeat the alarm"));
		layout->addWidget(dayBox[i], i, 3, Qt::AlignLeft);
	}
	label->setBuddy(dayBox[0]);
	layout->setColStretch(4, 1);
}

/******************************************************************************
 * Set up the monthly recurrence dialog controls.
 */
void RecurrenceEdit::initMonthly()
{
	int i;

	monthlyFrame = new QVBox(ruleFrame);
	monthlyFrame->setFrameStyle(QFrame::NoFrame);

	monthlyButtonGroup = new ButtonGroup(monthlyFrame);
	monthlyButtonGroup->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* topLayout = new QVBoxLayout(monthlyButtonGroup, KDialog::marginHint());

	QBoxLayout* layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
	onNthDayButton = new RadioButton(i18n("On the 7th day", "O&n the"), monthlyButtonGroup);
	onNthDayButton->setFixedSize(onNthDayButton->sizeHint());
	onNthDayButton->setReadOnly(mReadOnly);
	QWhatsThis::add(onNthDayButton,
	      i18n("Repeat the alarm on the selected day of the month"));
	layout->addWidget(onNthDayButton);
	nthDayEntry = new ComboBox(false, monthlyButtonGroup);
	nthDayEntry->setSizeLimit(11);
	for (i = 0;  i < 31;  ++i)
		nthDayEntry->insertItem(i18n(ordinal[i]));
	nthDayEntry->setFixedSize(nthDayEntry->sizeHint());
	nthDayEntry->setReadOnly(mReadOnly);
	QWhatsThis::add(nthDayEntry,
	      i18n("Select the day of the month on which to repeat the alarm"));
	layout->addWidget(nthDayEntry);
	QLabel* label = new QLabel(i18n("day"), monthlyButtonGroup);
	label->setFixedSize(label->sizeHint());
	layout->addWidget(label);
	layout->addStretch();

	layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
	onNthTypeOfDayButton = new RadioButton(i18n("On the 1st Tuesday", "On t&he"), monthlyButtonGroup);
	onNthTypeOfDayButton->setFixedSize(onNthTypeOfDayButton->sizeHint());
	onNthTypeOfDayButton->setReadOnly(mReadOnly);
	QWhatsThis::add(onNthTypeOfDayButton,
	      i18n("Repeat the alarm on one day of the week, in the selected week of the month"));
	layout->addWidget(onNthTypeOfDayButton);
	nthNumberEntry = new ComboBox(false, monthlyButtonGroup);
	for (i = 0;  i < 5;  ++i)
		nthNumberEntry->insertItem(i18n(ordinal[i]));
	nthNumberEntry->insertItem(i18n("Last"));
	nthNumberEntry->setFixedSize(nthNumberEntry->sizeHint());
	nthNumberEntry->setReadOnly(mReadOnly);
	QWhatsThis::add(nthNumberEntry,
	      i18n("Select the week of the month in which to repeat the alarm"));
	layout->addWidget(nthNumberEntry);
	nthTypeOfDayEntry = new ComboBox(false, monthlyButtonGroup);
	for (i = 1;  i <= 7;  ++i)
		nthTypeOfDayEntry->insertItem(KGlobal::locale()->weekDayName(i));    // starts Monday
	nthTypeOfDayEntry->setReadOnly(mReadOnly);
	QWhatsThis::add(nthTypeOfDayEntry,
	      i18n("Select the day of the week on which to repeat the alarm"));
	layout->addWidget(nthTypeOfDayEntry);
	layout->addStretch();

	onNthDayButtonId       = monthlyButtonGroup->id(onNthDayButton);
	onNthTypeOfDayButtonId = monthlyButtonGroup->id(onNthTypeOfDayButton);

	connect(monthlyButtonGroup, SIGNAL(buttonSet(int)), SLOT(monthlyClicked(int)));
}

/******************************************************************************
 * Set up the yearly recurrence dialog controls.
 */
void RecurrenceEdit::initYearly()
{
	int i;

	yearlyFrame = new QVBox(ruleFrame);
	yearlyFrame->setFrameStyle(QFrame::NoFrame);

	yearlyButtonGroup = new ButtonGroup(yearlyFrame);
	yearlyButtonGroup->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* topLayout = new QVBoxLayout(yearlyButtonGroup, KDialog::marginHint());

	// Set up the yearly month widgets
	QHBox* box = new QHBox(yearlyButtonGroup);   // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());
	topLayout->addWidget(box);
	yearMonthButton = new RadioButton(i18n("On 7th January", "O&n %1 %2"), box);
	yearMonthButton->setFixedSize(yearMonthButton->sizeHint());
	yearMonthButton->setReadOnly(mReadOnly);
	yearlyButtonGroup->insert(yearMonthButton);
	QWhatsThis::add(yearMonthButton,
	      i18n("Repeat the alarm on the selected date in the year"));
	box->setStretchFactor(new QWidget(box), 1);    // left adjust the controls
	box->setFixedHeight(box->sizeHint().height());

	// Set up the yearly position widgets
	QBoxLayout* vlayout = new QVBoxLayout(topLayout, KDialog::spacingHint());
	QBoxLayout* layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	yearlyOnNthTypeOfDayButton = new RadioButton(i18n("On the 1st Tuesday", "On t&he"), yearlyButtonGroup);
	yearlyOnNthTypeOfDayButton->setFixedSize(yearlyOnNthTypeOfDayButton->sizeHint());
	yearlyOnNthTypeOfDayButton->setReadOnly(mReadOnly);
	QWhatsThis::add(yearlyOnNthTypeOfDayButton,
	      i18n("Repeat the alarm on one day of the week, in the selected week of a month"));
	layout->addWidget(yearlyOnNthTypeOfDayButton);

	yearlyNthNumberEntry = new ComboBox(false, yearlyButtonGroup);
	for (i = 0;  i < 5;  ++i)
		yearlyNthNumberEntry->insertItem(i18n(ordinal[i]));
	yearlyNthNumberEntry->insertItem(i18n("Last"));
	yearlyNthNumberEntry->setFixedSize(yearlyNthNumberEntry->sizeHint());
	yearlyNthNumberEntry->setReadOnly(mReadOnly);
	QWhatsThis::add(yearlyNthNumberEntry,
	      i18n("Select the week of the month in which to repeat the alarm"));
	layout->addWidget(yearlyNthNumberEntry);

	yearlyNthTypeOfDayEntry = new ComboBox(false, yearlyButtonGroup);
	for (i = 1;  i <= 7;  ++i)
		yearlyNthTypeOfDayEntry->insertItem(KGlobal::locale()->weekDayName(i));    // starts Monday
	yearlyNthTypeOfDayEntry->setReadOnly(mReadOnly);
	QWhatsThis::add(yearlyNthTypeOfDayEntry,
	      i18n("Select the day of the week on which to repeat the alarm"));
	layout->addWidget(yearlyNthTypeOfDayEntry);

	layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("first week of January", "of"), yearlyButtonGroup);
	label->setFixedSize(label->sizeHint());
	int spac = yearlyOnNthTypeOfDayButton->width() - label->width();
	if (spac > 0)
		layout->addSpacing(spac);
	layout->addWidget(label);

	yeardayMonthComboBox = new ComboBox(yearlyButtonGroup);
	for (i = 1;  i <= 12;  ++i)
		yeardayMonthComboBox->insertItem(KGlobal::locale()->monthName(i));
	yeardayMonthComboBox->setSizeLimit(12);
	yeardayMonthComboBox->setReadOnly(mReadOnly);
	QWhatsThis::add(yeardayMonthComboBox,
	      i18n("Select the month of the year in which to repeat the alarm"));
	layout->addWidget(yeardayMonthComboBox);
	layout->addStretch();

/*	layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
	yearDayButton = new RadioButton(i18n("Recur on day"), yearlyButtonGroup);
	yearDayButton->setFixedSize(yearDayButton->sizeHint());
	yearDayButton->setReadOnly(mReadOnly);
	QWhatsThis::add(yearDayButton,
	      i18n("Repeat the alarm on the selected day number in the year"));
	layout->addWidget(yearDayButton);
	yearDayEntry = new SpinBox(1, 366, 1, yearlyButtonGroup);
	yearDayEntry->setFixedSize(yearDayEntry->sizeHint());
	yearDayEntry->setLineShiftStep(10);
	yearDayEntry->setSelectOnStep(false);
	QWhatsThis::add(yearDayEntry,
	      i18n("Select the day number in the year on which to repeat the alarm"));
	layout->addWidget(yearDayEntry);
	layout->addStretch();*/

	yearMonthButtonId = yearlyButtonGroup->id(yearMonthButton);
//	yearDayButtonId   = yearlyButtonGroup->id(yearDayButton);
	yearlyOnNthTypeOfDayButtonId = yearlyButtonGroup->id(yearlyOnNthTypeOfDayButton);

	connect(yearlyButtonGroup, SIGNAL(buttonSet(int)), SLOT(yearlyClicked(int)));
}

/******************************************************************************
 * Verify the consistency of the entered data.
 */
QWidget* RecurrenceEdit::checkData(const QDateTime& startDateTime, bool& noTime) const
{
	const_cast<RecurrenceEdit*>(this)->currStartDateTime = startDateTime;
	if (endDateButton->isChecked())
	{
		noTime = !endTimeEdit->isEnabled();
		QDate endDate = endDateEdit->date();
		if (endDate < startDateTime.date())
			return endDateEdit;
		if (!noTime  &&  QDateTime(endDate, endTimeEdit->time()) < startDateTime)
			return endTimeEdit;
	}
	return 0;
}

/******************************************************************************
 * Called when a recurrence period radio button is clicked.
 */
void RecurrenceEdit::periodClicked(int id)
{
	QFrame* frame;
	QString whatsThis;
	bool subdaily = (id == subdailyButtonId);
	if (subdaily)
	{
		frame = noneFrame;
		ruleButtonType = SUBDAILY;
	}
	else if (id == dailyButtonId)
	{
		frame = noneFrame;
		whatsThis = i18n("Enter the number of days between repetitions of the alarm");
		ruleButtonType = DAILY;
	}
	else if (id == weeklyButtonId)
	{
		frame = weeklyFrame;
		whatsThis = i18n("Enter the number of weeks between repetitions of the alarm");
		ruleButtonType = WEEKLY;
	}
	else if (id == monthlyButtonId)
	{
		frame = monthlyFrame;
		whatsThis = i18n("Enter the number of months between repetitions of the alarm");
		ruleButtonType = MONTHLY;
	}
	else if (id == yearlyButtonId)
	{
		frame = yearlyFrame;
		whatsThis = i18n("Enter the number of years between repetitions of the alarm");
		ruleButtonType = ANNUAL;
	}
	else
		return;

	ruleStack->raiseWidget(frame);
	if (subdaily)
	{
		recurFrequencyStack->raiseWidget(recurHourMinFrequency);
		recurEveryLabel->setBuddy(recurHourMinFrequency);
	}
	else
	{
		recurFrequencyStack->raiseWidget(recurFrequency);
		recurEveryLabel->setBuddy(recurFrequency);
	}
	endTimeEdit->setEnabled(subdaily && endDateButton->isChecked());
	if (!subdaily)
		QWhatsThis::add(recurFrequency, whatsThis);
	if (!noEmitTypeChanged)
		emit typeChanged(ruleButtonType);
}

void RecurrenceEdit::disableRange(bool on)
{
	if (on)
	{
		endDateEdit->setEnabled(false);
		endTimeEdit->setEnabled(false);
		mRepeatCountEntry->setEnabled(false);
		repeatCountLabel->setEnabled(false);
	}
}

void RecurrenceEdit::enableDurationRange(bool on)
{
	if (on)
	{
		endDateEdit->setEnabled(false);
		endTimeEdit->setEnabled(false);
		mRepeatCountEntry->setEnabled(true);
		repeatCountLabel->setEnabled(true);
	}
}

void RecurrenceEdit::enableDateRange(bool on)
{
	if (on)
	{
		endDateEdit->setEnabled(true);
		endTimeEdit->setEnabled(subdailyButton->isOn());
		mRepeatCountEntry->setEnabled(false);
		repeatCountLabel->setEnabled(false);
	}
}

/******************************************************************************
 * Called when a monthly recurrence type radio button is clicked.
 */
void RecurrenceEdit::monthlyClicked(int id)
{
	bool nthDay;
	if (id == onNthDayButtonId)
		nthDay = true;
	else if (id == onNthTypeOfDayButtonId)
		nthDay = false;
	else
		return;

	nthDayEntry->setEnabled(nthDay);
	nthNumberEntry->setEnabled(!nthDay);
	nthTypeOfDayEntry->setEnabled(!nthDay);
}

/******************************************************************************
 * Called when a yearly recurrence type radio button is clicked.
 */
void RecurrenceEdit::yearlyClicked(int id)
{
	bool date;
	if (id == yearMonthButtonId)
		date = true;
//	else if (id == yearDayButtonId)
	else if (id == yearlyOnNthTypeOfDayButtonId)
		date = false;
	else
		return;

//	yearDayEntry->setEnabled(!date);
	yearlyNthNumberEntry->setEnabled(!date);
	yearlyNthTypeOfDayEntry->setEnabled(!date);
	yeardayMonthComboBox->setEnabled(!date);
}

void RecurrenceEdit::showEvent(QShowEvent*)
{
	recurEveryLabel->buddy()->setFocus();
	emit shown();
}

void RecurrenceEdit::unsetAllCheckboxes()
{
	onNthDayButton->setChecked(false);
	onNthTypeOfDayButton->setChecked(false);
	yearMonthButton->setChecked(false);
//	yearDayButton->setChecked(false);
	yearlyOnNthTypeOfDayButton->setChecked(false);

	for (int i = 0;  i < 7;  ++i)
		dayBox[i]->setChecked(false);

	endDateButton->setChecked(false);
	noEndDateButton->setChecked(false);
	repeatCountButton->setChecked(false);
}

void RecurrenceEdit::setDefaults(const QDateTime& from)
{
	// unset everything
	unsetAllCheckboxes();

	currStartDateTime = from;
	QDate fromDate = from.date();

	noEmitTypeChanged = true;
	int button;
	switch (theApp()->settings()->defaultRecurPeriod())
	{
		case ANNUAL:  button = yearlyButtonId;   break;
		case MONTHLY: button = monthlyButtonId;  break;
		case WEEKLY:  button = weeklyButtonId;   break;
		case DAILY:   button = dailyButtonId;    break;
		case SUBDAILY:
		default:      button = subdailyButtonId; break;
	}
	ruleButtonGroup->setButton(button);
	noEmitTypeChanged = false;
	noEndDateButton->setChecked(true);

	recurHourMinFrequency->setValue(1);
	recurFrequency->setValue(1);

	checkDay(fromDate.dayOfWeek());
	monthlyButtonGroup->setButton(onNthDayButtonId);    // date in month
	int day = fromDate.day() - 1;
	int dayOfWeek = fromDate.dayOfWeek() - 1;
	int month = fromDate.month() - 1;
	nthDayEntry->setCurrentItem(day);
	nthNumberEntry->setCurrentItem(day / 7);
	nthTypeOfDayEntry->setCurrentItem(dayOfWeek);
	yearlyButtonGroup->setButton(yearMonthButtonId);     // date in year
	setStartDate(fromDate);
//	yearDayEntry->setValue(fromDate.dayOfYear());
	yearlyNthNumberEntry->setCurrentItem(day / 7);
	yearlyNthTypeOfDayEntry->setCurrentItem(dayOfWeek);
	yeardayMonthComboBox->setCurrentItem(month);

	endDateEdit->setDate(fromDate);
}

void RecurrenceEdit::setStartDate(const QDate& start)
{
	yearMonthButton->setText(i18n("On 7th January", "O&n %1 %2")
	                          .arg(i18n(ordinal[start.day() - 1]))
	                          .arg(KGlobal::locale()->monthName(start.month())));
	yearMonthButton->setMinimumSize(yearMonthButton->sizeHint());
}

void RecurrenceEdit::setEndDate(const QDate& start)
{
	endDateEdit->setDate(start);
}


void RecurrenceEdit::checkDay(int day)
{
	if (day >= 1  &&  day <= 7)
		dayBox[day - 1]->setChecked(true);
}

void RecurrenceEdit::getCheckedDays(QBitArray& rDays)
{
	rDays.fill(false);
	for (int i = 0;  i < 7;  ++i)
		if (dayBox[i]->isChecked())
			rDays.setBit(i, 1);
}

void RecurrenceEdit::setCheckedDays(QBitArray& rDays)
{
	for (int i = 0;  i < 7;  ++i)
		if (rDays.testBit(i))
			dayBox[i]->setChecked(true);
}

/******************************************************************************
 * Set the state of all controls to reflect the data in the specified event.
 */
void RecurrenceEdit::set(const KAlarmEvent& event)
{
	setDefaults(event.mainDateTime());
	int repeatDuration;
	Recurrence* recurrence = event.recurrence();
	if (!recurrence)
		return;
	switch (recurrence->doesRecur())
	{
		case Recurrence::rMinutely:
			ruleButtonGroup->setButton(subdailyButtonId);
			recurHourMinFrequency->setValue(recurrence->frequency());
			break;

		case Recurrence::rDaily:
			ruleButtonGroup->setButton(dailyButtonId);
			break;

		case Recurrence::rWeekly:
		{
			ruleButtonGroup->setButton(weeklyButtonId);
			QBitArray rDays = recurrence->days();
			setCheckedDays(rDays);
			break;
		}
		case Recurrence::rMonthlyPos:    // on nth (Tuesday) of the month
		{
			// we only handle one possibility in the list right now,
			// so I have hardcoded calls with first().  If we make the GUI
			// more extended, this can be changed.
			ruleButtonGroup->setButton(monthlyButtonId);
			monthlyButtonGroup->setButton(onNthTypeOfDayButtonId);
			QPtrList<Recurrence::rMonthPos> rmp = recurrence->monthPositions();
			int i = rmp.first()->rPos - 1;
			if (rmp.first()->negative)
				i = 5;
			nthNumberEntry->setCurrentItem(i);
			for (i = 0;  !rmp.first()->rDays.testBit(i);  ++i) ;
			nthTypeOfDayEntry->setCurrentItem(i);
			break;
		}
		case Recurrence::rMonthlyDay:     // on nth day of the month
		{
			ruleButtonGroup->setButton(monthlyButtonId);
			monthlyButtonGroup->setButton(onNthDayButtonId);
			QPtrList<int> rmd = recurrence->monthDays();
			int i = *rmd.first() - 1;
			nthDayEntry->setCurrentItem(i);
			break;
		}
		case Recurrence::rYearlyMonth:   // in the nth month of the year
		{
			ruleButtonGroup->setButton(yearlyButtonId);
			yearlyButtonGroup->setButton(yearMonthButtonId);
/*			QPtrList<int> rmd = recurrence->yearNums();
			yearMonthButton->setText(KGlobal::locale()->monthName(*rmd.first()));
			yearMonthButton->setMinimumSize(yearMonthLabel->sizeHint());*/
			break;
		}
/*			case Recurrence::rYearlyDay:     // on the nth day of the year
		{
			ruleButtonGroup->setButton(yearlyButtonId);
			yearlyButtonGroup->setButton(yearDayButtonId);
			break;
		}*/
		case Recurrence::rYearlyPos:     // on the nth (Tuesday) of a month in the year
		{
			ruleButtonGroup->setButton(yearlyButtonId);
			yearlyButtonGroup->setButton(yearlyOnNthTypeOfDayButtonId);
			QPtrList<Recurrence::rMonthPos> rmp = recurrence->yearMonthPositions();
			int i = rmp.first()->rPos - 1;
			if (rmp.first()->negative)
				i = 5;
			yearlyNthNumberEntry->setCurrentItem(i);
			for (i = 0;  !rmp.first()->rDays.testBit(i);  ++i) ;
				yearlyNthTypeOfDayEntry->setCurrentItem(i);
			QPtrList<int> rmd = recurrence->yearNums();
			yeardayMonthComboBox->setCurrentItem(*rmd.first() - 1);
			break;
		}
		case Recurrence::rNone:
		default:
			return;
	}

	recurFrequency->setValue(recurrence->frequency());
	repeatDuration = event.remainingRecurrences();

	// get range information
	QDateTime endtime = currStartDateTime;
	if (repeatDuration == -1)
		noEndDateButton->setChecked(true);
	else if (repeatDuration)
	{
		repeatCountButton->setChecked(true);
		mRepeatCountEntry->setValue(repeatDuration);
	}
	else
	{
		endDateButton->setChecked(true);
		endtime = recurrence->endDateTime();
		endTimeEdit->setValue(endtime.time().hour()*60 + endtime.time().minute());
	}
	endDateEdit->setDate(endtime.date());
}

/******************************************************************************
 * Update the specified KAlarmEvent with the entered recurrence data.
 */
void RecurrenceEdit::updateEvent(KAlarmEvent& event)
{
	// Get end date and repeat count, common to all types of recurring events
	QDate  endDate;
	QTime  endTime;
	int    repeatCount;
	if (noEndDateButton->isChecked())
		repeatCount = -1;
	else if (repeatCountButton->isChecked())
		repeatCount = mRepeatCountEntry->value();
	else
	{
		repeatCount = 0;
		endDate = endDateEdit->date();
		endTime = endTimeEdit->time();
	}

	// Set up the recurrence according to the type selected
	QButton* button = ruleButtonGroup->selected();
	if (button == subdailyButton)
	{
		QDateTime endDateTime(endDate, endTime);
		int frequency = recurHourMinFrequency->value();
		event.setRecurMinutely(frequency, repeatCount, endDateTime);
	}
	else if (button == dailyButton)
	{
		int frequency = recurFrequency->value();
		event.setRecurDaily(frequency, repeatCount, endDate);
	}
	else if (button == weeklyButton)
	{
		int frequency = recurFrequency->value();
		QBitArray rDays(7);
		getCheckedDays(rDays);
		event.setRecurWeekly(frequency, rDays, repeatCount, endDate);
	}
	else if (button == monthlyButton)
	{
		int frequency = recurFrequency->value();
		if (onNthTypeOfDayButton->isChecked())
		{
			// it's by position
			KAlarmEvent::MonthPos pos;
			pos.days.fill(false);
			pos.days.setBit(nthTypeOfDayEntry->currentItem());
			int i = nthNumberEntry->currentItem() + 1;
			pos.weeknum = (i <= 5) ? i : 5 - i;
			QValueList<KAlarmEvent::MonthPos> poses;
			poses.append(pos);
			event.setRecurMonthlyByPos(frequency, poses, repeatCount, endDate);
		}
		else
		{
			// it's by day
			short daynum  = nthDayEntry->currentItem() + 1;
			QValueList<int> daynums;
			daynums.append(daynum);
			event.setRecurMonthlyByDate(frequency, daynums, repeatCount, endDate);
		}
	}
	else if (button == yearlyButton)
	{
		int frequency = recurFrequency->value();
		if (yearMonthButton->isChecked())
		{
			QValueList<int> months;
			months.append(event.mainDate().month());
			event.setRecurAnnualByDate(frequency, months, repeatCount, endDate);
		}
		else
		{
			// it's by position
			KAlarmEvent::MonthPos pos;
			pos.days.fill(false);
			pos.days.setBit(yearlyNthTypeOfDayEntry->currentItem());
			int i = yearlyNthNumberEntry->currentItem() + 1;
			pos.weeknum = (i <= 5) ? i : 5 - i;
			QValueList<KAlarmEvent::MonthPos> poses;
			poses.append(pos);
			int month = yeardayMonthComboBox->currentItem() + 1;
			QValueList<int> months;
			months.append(month);
			event.setRecurAnnualByPos(frequency, poses, months, repeatCount, endDate);
		}
/*		else
		{
			// it's by day
			int daynum = event.mainDate().dayOfYear();
			QValueList<int> daynums;
			daynums.append(daynum);
			event.setRecurAnnualByDay(frequency, daynums, repeatCount, endDate);
		}*/
	}
}
