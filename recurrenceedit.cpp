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
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qwidgetstack.h>
#include <qframe.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qcombobox.h>
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
#include "msgevent.h"
#include "buttongroup.h"
using namespace KCal;

#include "recurrenceedit.moc"

static QString ordinal[] = {
	QString::null,
	i18n("1st"),  i18n("2nd"),  i18n("3rd"),  i18n("4th"),  i18n("5th"),
	i18n("6th"),  i18n("7th"),  i18n("8th"),  i18n("9th"),  i18n("10th"),
	i18n("11th"), i18n("12th"), i18n("13th"), i18n("14th"), i18n("15th"),
	i18n("16th"), i18n("17th"), i18n("18th"), i18n("19th"), i18n("20th"),
	i18n("21st"), i18n("22nd"), i18n("23rd"), i18n("24th"), i18n("25th"),
	i18n("26th"), i18n("27th"), i18n("28th"), i18n("29th"), i18n("30th"),
	i18n("31st")
};


RecurrenceEdit::RecurrenceEdit(QFrame* page, const char* name)
	: QObject(page, name),
	  noEmitTypeChanged(true)
{
	QBoxLayout* layout;
	QVBoxLayout* topLayout = new QVBoxLayout(page, marginKDE2, KDialog::spacingHint());

	// Create the recurrence rule Group box which holds the recurrence period
	// selection buttons, and the weekly, monthly and yearly recurrence rule
	// frames which specify options individual to each of these distinct
	// sections of the recurrence rule. Each frame is made visible by the
	// selection of its corresponding radio button.

#if KDE_VERSION >= 290
	recurGroup = new QGroupBox(1, Qt::Vertical, i18n("Recurrence Rule"), page, "recurGroup");
#else
	recurGroup = new QGroupBox(i18n("Recurrence Rule"), page, "recurGroup");
	layout = new QVBoxLayout(recurGroup, 2*KDialog::marginHint(), KDialog::spacingHint());
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
	connect(ruleButtonGroup, SIGNAL(clicked(int)), SLOT(periodClicked(int)));

	recurEveryLabel = new QLabel(i18n("Recur e&very:"), ruleButtonGroup);
	recurEveryLabel->setFixedSize(recurEveryLabel->sizeHint());
	recurFrequencyStack = new QWidgetStack(ruleButtonGroup);
	recurFrequency = new QSpinBox(1, 999, 1, ruleButtonGroup);
	recurFrequencyStack->addWidget(recurFrequency, 0);
	recurHourMinFrequency = new TimeSpinBox(1, 99*60+59, ruleButtonGroup);
	QWhatsThis::add(recurHourMinFrequency,
	      i18n("Enter the time (in hours and minutes) between repetitions of the alarm."));
	recurFrequencyStack->addWidget(recurHourMinFrequency, 1);
	QSize size = recurFrequency->sizeHint().expandedTo(recurHourMinFrequency->sizeHint());
	recurFrequency->setFixedSize(size);
	recurHourMinFrequency->setFixedSize(size);
	recurFrequencyStack->setFixedSize(size);

	subdailyButton = new QRadioButton(i18n("Ho&urs/Minutes"), ruleButtonGroup);
	subdailyButton->setFixedSize(subdailyButton->sizeHint());
	QWhatsThis::add(subdailyButton,
	      i18n("Set the alarm repetition interval to the number of hours and minutes entered"));

	dailyButton = new QRadioButton(i18n("&Days"), ruleButtonGroup);
	dailyButton->setFixedSize(dailyButton->sizeHint());
	QWhatsThis::add(dailyButton,
	      i18n("Set the alarm repetition interval to the number of days entered"));

	weeklyButton = new QRadioButton(i18n("&Weeks"), ruleButtonGroup);
	weeklyButton->setFixedSize(weeklyButton->sizeHint());
	QWhatsThis::add(weeklyButton,
	      i18n("Set the alarm repetition interval to the number of weeks entered"));

	monthlyButton = new QRadioButton(i18n("&Months"), ruleButtonGroup);
	monthlyButton->setFixedSize(monthlyButton->sizeHint());
	QWhatsThis::add(monthlyButton,
	      i18n("Set the alarm repetition interval to the number of months entered"));

	yearlyButton = new QRadioButton(i18n("&Years"), ruleButtonGroup);
	yearlyButton->setFixedSize(yearlyButton->sizeHint());
	QWhatsThis::add(yearlyButton,
	      i18n("Set the alarm repetition interval to the number of years entered"));

#if KDE_VERSION < 290
	ruleButtonGroup->addWidget(label);
	ruleButtonGroup->addWidget(recurFrequencyStack);
	ruleButtonGroup->addWidget(subdailyButton);
	ruleButtonGroup->addWidget(dailyButton);
	ruleButtonGroup->addWidget(weeklyButton);
	ruleButtonGroup->addWidget(monthlyButton);
	ruleButtonGroup->addWidget(yearlyButton);
#endif

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

	rangeButtonGroup = new QButtonGroup(i18n("Recurrence End"), page, "rangeButtonGroup");
	topLayout->addWidget(rangeButtonGroup);

	QVBoxLayout* vlayout = new QVBoxLayout(rangeButtonGroup, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	vlayout->addSpacing(page->fontMetrics().lineSpacing()/2);
	noEndDateButton = new QRadioButton(i18n("No &end"), rangeButtonGroup);
	noEndDateButton->setFixedSize(noEndDateButton->sizeHint());
	QWhatsThis::add(noEndDateButton, i18n("Repeat the alarm indefinitely"));
	vlayout->addWidget(noEndDateButton, 1, Qt::AlignLeft);
	size = noEndDateButton->size();

	layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	repeatCountButton = new QRadioButton(i18n("End a&fter:"), rangeButtonGroup);
	connect(repeatCountButton, SIGNAL(toggled(bool)), SLOT(rangeToggled(bool)));
	QWhatsThis::add(repeatCountButton,
	      i18n("Repeat the alarm for the number of times specified"));
	repeatCountEntry = new QSpinBox(1, 9999, 1, rangeButtonGroup);
	repeatCountEntry->setFixedSize(repeatCountEntry->sizeHint());
	QWhatsThis::add(repeatCountEntry,
	      i18n("Enter the total number of times to trigger the alarm"));
	repeatCountLabel = new QLabel(i18n("occurrence(s)"), rangeButtonGroup);
	repeatCountLabel->setFixedSize(repeatCountLabel->sizeHint());
	layout->addWidget(repeatCountButton);
	layout->addSpacing(KDialog::spacingHint());
	layout->addWidget(repeatCountEntry);
	layout->addWidget(repeatCountLabel);
	layout->addStretch();
	size = size.expandedTo(repeatCountButton->sizeHint());

	layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	endDateButton = new QRadioButton(i18n("End &by:"), rangeButtonGroup);
	connect(endDateButton, SIGNAL(toggled(bool)), SLOT(rangeToggled(bool)));
	QWhatsThis::add(endDateButton,
	      i18n("Repeat the alarm until the date/time specified"));
	endDateEdit = new DateSpinBox(rangeButtonGroup);
	endDateEdit->setFixedSize(endDateEdit->sizeHint());
	QWhatsThis::add(endDateEdit,
	      i18n("Enter the last date to repeat the alarm"));
	endTimeEdit = new TimeSpinBox(rangeButtonGroup);
	endTimeEdit->setFixedSize(endTimeEdit->sizeHint());
	QWhatsThis::add(endTimeEdit,
	      i18n("Enter the last time to repeat the alarm"));
	layout->addWidget(endDateButton);
	layout->addSpacing(KDialog::spacingHint());
	layout->addWidget(endDateEdit);
	layout->addWidget(endTimeEdit);
	layout->addStretch();
	size = size.expandedTo(endDateButton->sizeHint());

	// Line up the widgets to the right of the radio buttons
	repeatCountButton->setFixedSize(size);
	endDateButton->setFixedSize(size);

	connect(noEndDateButton, SIGNAL(toggled(bool)), SLOT(disableRange(bool)));
	connect(repeatCountButton, SIGNAL(toggled(bool)), SLOT(enableDurationRange(bool)));
	connect(endDateButton, SIGNAL(toggled(bool)), SLOT(enableDateRange(bool)));

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
		dayBox[i] = new QCheckBox(KGlobal::locale()->weekDayName(i + 1), weeklyFrame);    // starts Monday
		dayBox[i]->setFixedSize(dayBox[i]->sizeHint());
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
	onNthDayButton = new QRadioButton(i18n("On the 7th day", "O&n the"), monthlyButtonGroup);
	onNthDayButton->setFixedSize(onNthDayButton->sizeHint());
	QWhatsThis::add(onNthDayButton,
	      i18n("Repeat the alarm on the selected day of the month"));
	layout->addWidget(onNthDayButton);
	nthDayEntry = new QComboBox(false, monthlyButtonGroup);
	nthDayEntry->setSizeLimit(11);
	for (i = 1;  i <= 31;  ++i)
		nthDayEntry->insertItem(ordinal[i]);
	nthDayEntry->setFixedSize(nthDayEntry->sizeHint());
	QWhatsThis::add(nthDayEntry,
	      i18n("Select the day of the month on which to repeat the alarm"));
	layout->addWidget(nthDayEntry);
	QLabel* label = new QLabel(i18n("day"), monthlyButtonGroup);
	label->setFixedSize(label->sizeHint());
	layout->addWidget(label);
	layout->addStretch();

	layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
	onNthTypeOfDayButton = new QRadioButton(i18n("On the 1st Tuesday", "On t&he"), monthlyButtonGroup);
	onNthTypeOfDayButton->setFixedSize(onNthTypeOfDayButton->sizeHint());
	QWhatsThis::add(onNthTypeOfDayButton,
	      i18n("Repeat the alarm on one day of the week, in the selected week of the month"));
	layout->addWidget(onNthTypeOfDayButton);
	nthNumberEntry = new QComboBox(false, monthlyButtonGroup);
	for (i = 1;  i <= 5;  ++i)
		nthNumberEntry->insertItem(ordinal[i]);
	nthNumberEntry->insertItem(i18n("Last"));
	nthNumberEntry->setFixedSize(nthNumberEntry->sizeHint());
	QWhatsThis::add(nthNumberEntry,
	      i18n("Select the week of the month in which to repeat the alarm"));
	layout->addWidget(nthNumberEntry);
	nthTypeOfDayEntry = new QComboBox(false, monthlyButtonGroup);
	for (i = 1;  i <= 7;  ++i)
		nthTypeOfDayEntry->insertItem(KGlobal::locale()->weekDayName(i));    // starts Monday
	QWhatsThis::add(nthTypeOfDayEntry,
	      i18n("Select the day of the week on which to repeat the alarm"));
	layout->addWidget(nthTypeOfDayEntry);
	layout->addStretch();

	onNthDayButtonId       = monthlyButtonGroup->id(onNthDayButton);
	onNthTypeOfDayButtonId = monthlyButtonGroup->id(onNthTypeOfDayButton);

	connect(monthlyButtonGroup, SIGNAL(clicked(int)), SLOT(monthlyClicked(int)));
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
	QBoxLayout* layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
	yearMonthButton = new QRadioButton(i18n("On 7th January", "O&n"), yearlyButtonGroup);
	yearMonthButton->setFixedSize(yearMonthButton->sizeHint());
	QWhatsThis::add(yearMonthButton,
	      i18n("Repeat the alarm on the selected date in the year"));
	layout->addWidget(yearMonthButton);
	yearMonthDayEntry = new QComboBox(false, yearlyButtonGroup);
	yearMonthDayEntry->setSizeLimit(11);
	for (i = 1;  i <= 31;  ++i)
		yearMonthDayEntry->insertItem(ordinal[i]);
	yearMonthDayEntry->setFixedSize(yearMonthDayEntry->sizeHint());
	QWhatsThis::add(yearMonthDayEntry,
	      i18n("Select the day of the month on which to repeat the alarm"));
	layout->addWidget(yearMonthDayEntry);
	yearMonthComboBox = new QComboBox(yearlyButtonGroup);
	for (i = 1;  i <= 12;  ++i)
		yearMonthComboBox->insertItem(KGlobal::locale()->monthName(i));
	yearMonthComboBox->setSizeLimit(12);
	QWhatsThis::add(yearMonthComboBox,
	      i18n("Select the month of the year in which to repeat the alarm"));
	layout->addWidget(yearMonthComboBox);
	layout->addStretch();

	// Set up the yearly position widgets
	QBoxLayout* vlayout = new QVBoxLayout(topLayout, KDialog::spacingHint());
	layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	yearlyOnNthTypeOfDayButton = new QRadioButton(i18n("On the 1st Tuesday", "On t&he"), yearlyButtonGroup);
	yearlyOnNthTypeOfDayButton->setFixedSize(yearlyOnNthTypeOfDayButton->sizeHint());
	QWhatsThis::add(yearlyOnNthTypeOfDayButton,
	      i18n("Repeat the alarm on one day of the week, in the selected week of a month"));
	layout->addWidget(yearlyOnNthTypeOfDayButton);

	yearlyNthNumberEntry = new QComboBox(false, yearlyButtonGroup);
	for (i = 1;  i <= 5;  ++i)
		yearlyNthNumberEntry->insertItem(ordinal[i]);
	yearlyNthNumberEntry->insertItem(i18n("Last"));
	yearlyNthNumberEntry->setFixedSize(yearlyNthNumberEntry->sizeHint());
	QWhatsThis::add(yearlyNthNumberEntry,
	      i18n("Select the week of the month in which to repeat the alarm"));
	layout->addWidget(yearlyNthNumberEntry);

	yearlyNthTypeOfDayEntry = new QComboBox(false, yearlyButtonGroup);
	for (i = 1;  i <= 7;  ++i)
		yearlyNthTypeOfDayEntry->insertItem(KGlobal::locale()->weekDayName(i));    // starts Monday
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

	yeardayMonthComboBox = new QComboBox(yearlyButtonGroup);
	for (i = 1;  i <= 12;  ++i)
		yeardayMonthComboBox->insertItem(KGlobal::locale()->monthName(i));
	yeardayMonthComboBox->setSizeLimit(12);
	QWhatsThis::add(yeardayMonthComboBox,
	      i18n("Select the month of the year in which to repeat the alarm"));
	layout->addWidget(yeardayMonthComboBox);
	layout->addStretch();

/*	layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
	yearDayButton = new QRadioButton(i18n("Recur on day"), yearlyButtonGroup);
	yearDayButton->setFixedSize(yearDayButton->sizeHint());
	QWhatsThis::add(yearDayButton,
	      i18n("Repeat the alarm on the selected day number in the year"));
	layout->addWidget(yearDayButton);
	yearDayEntry = new QSpinBox(1, 366, 1, yearlyButtonGroup);
	yearDayEntry->setFixedSize(yearDayEntry->sizeHint());
	QWhatsThis::add(yearDayEntry,
	      i18n("Select the day number in the year on which to repeat the alarm"));
	layout->addWidget(yearDayEntry);
	layout->addStretch();*/

	yearMonthButtonId = yearlyButtonGroup->id(yearMonthButton);
//	yearDayButtonId   = yearlyButtonGroup->id(yearDayButton);
	yearlyOnNthTypeOfDayButtonId = yearlyButtonGroup->id(yearlyOnNthTypeOfDayButton);

	connect(yearlyButtonGroup, SIGNAL(clicked(int)), SLOT(yearlyClicked(int)));
}

/******************************************************************************
 * Verify the consistency of the entered data.
 */
bool RecurrenceEdit::checkData(const QDateTime& startDateTime) const
{
	const_cast<RecurrenceEdit*>(this)->currStartDateTime = startDateTime;
	if (endDateButton->isChecked())
	{
		QDate endDate = endDateEdit->date();
		bool time = endTimeEdit->isEnabled();
		if (time  &&  QDateTime(endDate, endTimeEdit->time()) < startDateTime
		||  !time  &&  endDate < startDateTime.date())
			return false;
	}
	return true;
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
		repeatCountEntry->setEnabled(false);
		repeatCountLabel->setEnabled(false);
	}
}

void RecurrenceEdit::enableDurationRange(bool on)
{
	if (on)
	{
		endDateEdit->setEnabled(false);
		endTimeEdit->setEnabled(false);
		repeatCountEntry->setEnabled(true);
		repeatCountLabel->setEnabled(true);
	}
}

void RecurrenceEdit::enableDateRange(bool on)
{
	if (on)
	{
		endDateEdit->setEnabled(true);
		endTimeEdit->setEnabled(subdailyButton->isOn());
		repeatCountEntry->setEnabled(false);
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

	yearMonthDayEntry->setEnabled(date);
	yearMonthComboBox->setEnabled(date);
//	yearDayEntry->setEnabled(!date);
	yearlyNthNumberEntry->setEnabled(!date);
	yearlyNthTypeOfDayEntry->setEnabled(!date);
	yeardayMonthComboBox->setEnabled(!date);
}

void RecurrenceEdit::rangeToggled(bool)
{
//	if (repeatCountButton->isOn())
//		repeatCountEntry->setFocus();
//	else if (endDateButton->isOn())
//		endDateEdit->setFocus();
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
	yearMonthDayEntry->setCurrentItem(day);
	yearMonthComboBox->setCurrentItem(month);
//	yearDayEntry->setValue(fromDate.dayOfYear());
	yearlyNthNumberEntry->setCurrentItem(day / 7);
	yearlyNthTypeOfDayEntry->setCurrentItem(dayOfWeek);
	yeardayMonthComboBox->setCurrentItem(month);

	endDateEdit->setDate(fromDate);
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
			QPtrList<int> rmd = recurrence->yearNums();
			yearMonthComboBox->setCurrentItem(*rmd.first() - 1);
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
	repeatDuration = event.repeatCount();

	// get range information
	QDateTime endtime = currStartDateTime;
	if (repeatDuration == -1)
		noEndDateButton->setChecked(true);
	else if (repeatDuration)
	{
		repeatCountButton->setChecked(true);
		repeatCountEntry->setValue(repeatDuration);
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
		repeatCount = repeatCountEntry->value();
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
			int month = yearMonthComboBox->currentItem() + 1;
			QValueList<int> months;
			months.append(month);
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
