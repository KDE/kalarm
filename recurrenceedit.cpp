/*
 *  recurrenceedit.cpp  -  widget to edit the event's recurrence definition
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie  software@astrojar.org.uk
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
#include "alarmevent.h"
#include "preferences.h"
#include "alarmtimewidget.h"
#include "dateedit.h"
#include "timespinbox.h"
#include "timeperiod.h"
#include "spinbox.h"
#include "checkbox.h"
#include "combobox.h"
#include "radiobutton.h"
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
	mRecurFrequency = new TimePeriod(ruleButtonGroup);
	mRecurFrequency->setHourMinRange(1, 5999);
	mRecurFrequency->setUnitRange(1, 999);
	mRecurFrequency->setUnitSteps(1, 10);
	mRecurFrequency->setFixedSize(mRecurFrequency->sizeHint());
	mRecurFrequency->setSelectOnStep(false);
	mRecurFrequency->setReadOnly(mReadOnly);
	mRecurFrequency->setHourMinWhatsThis(
	      i18n("Enter the time (in hours and minutes) between repetitions of the alarm.\n%1")
	           .arg(TimeSpinBox::shiftWhatsThis()));
	recurEveryLabel->setBuddy(mRecurFrequency);

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
	ruleStack->addWidget(mNoneRuleFrame, 0);
	ruleStack->addWidget(mWeekRuleFrame, 1);
	ruleStack->addWidget(mMonthRuleFrame, 2);
	ruleStack->addWidget(mYearRuleFrame, 3);

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
	QSize size = noEndDateButton->size();

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
	connect(mRepeatCountEntry, SIGNAL(valueChanged(int)), SLOT(repeatCountChanged(int)));
	QWhatsThis::add(mRepeatCountEntry,
	      i18n("Enter the total number of times to trigger the alarm"));
	repeatCountButton->setFocusWidget(mRepeatCountEntry);
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
	endDateButton->setFocusWidget(endDateEdit);
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
	mNoneRuleFrame = new QFrame(ruleFrame);
	mNoneRuleFrame->setFrameStyle(QFrame::NoFrame);
}

/******************************************************************************
 * Set up the weekly recurrence dialog controls.
 */
void RecurrenceEdit::initWeekly()
{
	mWeekRuleFrame = new QFrame(ruleFrame);
	mWeekRuleFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* topLayout = new QVBoxLayout(mWeekRuleFrame);
	topLayout->addStretch();
	QGridLayout* layout = new QGridLayout(topLayout, 7, 5, KDialog::spacingHint());
	layout->setRowStretch(0, 1);
	layout->setColStretch(0, 1);

	QLabel* label = new QLabel(i18n("On: Tuesday", "O&n:"), mWeekRuleFrame);
	label->setFixedSize(label->sizeHint());
	layout->addWidget(label, 0, 1, Qt::AlignRight);
	layout->addColSpacing(2, 2*KDialog::spacingHint());

	// List the days of the week starting at the user's start day of the week.
	// Save the first day of the week, just in case it changes while the dialog is open.
#if KDE_VERSION >= 310
	mWeekRuleFirstDay = KGlobal::locale()->weekStartDay();
#else
	mWeekRuleFirstDay = KGlobal::locale()->weekStartsMonday() ? 1 : 7;
#endif
	for (int i = 0;  i < 7;  ++i)
	{
		int day = (i + mWeekRuleFirstDay - 1)%7 + 1;
		mWeekRuleDayBox[i] = new CheckBox(KGlobal::locale()->weekDayName(day), mWeekRuleFrame);
		mWeekRuleDayBox[i]->setFixedSize(mWeekRuleDayBox[i]->sizeHint());
		mWeekRuleDayBox[i]->setReadOnly(mReadOnly);
		QWhatsThis::add(mWeekRuleDayBox[i],
		      i18n("Select the day(s) of the week on which to repeat the alarm"));
		layout->addWidget(mWeekRuleDayBox[i], i, 3, Qt::AlignLeft);
	}
	label->setBuddy(mWeekRuleDayBox[0]);
	layout->setColStretch(4, 1);
}

/******************************************************************************
 * Set up the monthly recurrence dialog controls.
 */
void RecurrenceEdit::initMonthly()
{
	int i;

	mMonthRuleFrame = new QVBox(ruleFrame);
	mMonthRuleFrame->setFrameStyle(QFrame::NoFrame);

	mMonthRuleButtonGroup = new ButtonGroup(mMonthRuleFrame);
	mMonthRuleButtonGroup->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* topLayout = new QVBoxLayout(mMonthRuleButtonGroup, KDialog::marginHint());

	QBoxLayout* layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
	mMonthRuleOnNthDayButton = new RadioButton(i18n("On the 7th day", "O&n the"), mMonthRuleButtonGroup);
	mMonthRuleOnNthDayButton->setFixedSize(mMonthRuleOnNthDayButton->sizeHint());
	mMonthRuleOnNthDayButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mMonthRuleOnNthDayButton,
	      i18n("Repeat the alarm on the selected day of the month"));
	layout->addWidget(mMonthRuleOnNthDayButton);
	mMonthRuleNthDayEntry = new ComboBox(false, mMonthRuleButtonGroup);
	mMonthRuleNthDayEntry->setSizeLimit(11);
	for (i = 0;  i < 31;  ++i)
		mMonthRuleNthDayEntry->insertItem(i18n(ordinal[i]));
	mMonthRuleNthDayEntry->setFixedSize(mMonthRuleNthDayEntry->sizeHint());
	mMonthRuleNthDayEntry->setReadOnly(mReadOnly);
	QWhatsThis::add(mMonthRuleNthDayEntry,
	      i18n("Select the day of the month on which to repeat the alarm"));
	mMonthRuleOnNthDayButton->setFocusWidget(mMonthRuleNthDayEntry);
	layout->addWidget(mMonthRuleNthDayEntry);
	QLabel* label = new QLabel(i18n("day"), mMonthRuleButtonGroup);
	label->setFixedSize(label->sizeHint());
	layout->addWidget(label);
	layout->addStretch();

	layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
	mMonthRuleOnNthTypeOfDayButton = new RadioButton(i18n("On the 1st Tuesday", "On t&he"), mMonthRuleButtonGroup);
	mMonthRuleOnNthTypeOfDayButton->setFixedSize(mMonthRuleOnNthTypeOfDayButton->sizeHint());
	mMonthRuleOnNthTypeOfDayButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mMonthRuleOnNthTypeOfDayButton,
	      i18n("Repeat the alarm on one day of the week, in the selected week of the month"));
	layout->addWidget(mMonthRuleOnNthTypeOfDayButton);
	mMonthRuleNthNumberEntry = new ComboBox(false, mMonthRuleButtonGroup);
	for (i = 0;  i < 5;  ++i)
		mMonthRuleNthNumberEntry->insertItem(i18n(ordinal[i]));
	mMonthRuleNthNumberEntry->insertItem(i18n("Last"));
	mMonthRuleNthNumberEntry->setFixedSize(mMonthRuleNthNumberEntry->sizeHint());
	mMonthRuleNthNumberEntry->setReadOnly(mReadOnly);
	QWhatsThis::add(mMonthRuleNthNumberEntry,
	      i18n("Select the week of the month in which to repeat the alarm"));
	mMonthRuleOnNthTypeOfDayButton->setFocusWidget(mMonthRuleNthNumberEntry);
	layout->addWidget(mMonthRuleNthNumberEntry);
	mMonthRuleNthTypeOfDayEntry = new ComboBox(false, mMonthRuleButtonGroup);
	for (i = 1;  i <= 7;  ++i)
		mMonthRuleNthTypeOfDayEntry->insertItem(KGlobal::locale()->weekDayName(i));    // starts Monday
	mMonthRuleNthTypeOfDayEntry->setReadOnly(mReadOnly);
	QWhatsThis::add(mMonthRuleNthTypeOfDayEntry,
	      i18n("Select the day of the week on which to repeat the alarm"));
	layout->addWidget(mMonthRuleNthTypeOfDayEntry);
	layout->addStretch();

	mMonthRuleOnNthDayButtonId       = mMonthRuleButtonGroup->id(mMonthRuleOnNthDayButton);
	mMonthRuleOnNthTypeOfDayButtonId = mMonthRuleButtonGroup->id(mMonthRuleOnNthTypeOfDayButton);

	connect(mMonthRuleButtonGroup, SIGNAL(buttonSet(int)), SLOT(monthlyClicked(int)));
}

/******************************************************************************
 * Set up the yearly recurrence dialog controls.
 */
void RecurrenceEdit::initYearly()
{
	int i;

	mYearRuleFrame = new QVBox(ruleFrame);
	mYearRuleFrame->setFrameStyle(QFrame::NoFrame);

	mYearRuleButtonGroup = new ButtonGroup(mYearRuleFrame);
	mYearRuleButtonGroup->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* topLayout = new QVBoxLayout(mYearRuleButtonGroup, KDialog::marginHint());

	// Set up the February 29th selection widget
	mYearRuleFeb29Button = new RadioButton(i18n("On &29th February"), mYearRuleButtonGroup);
	mYearRuleFeb29Button->setFixedSize(mYearRuleFeb29Button->sizeHint());
	mYearRuleFeb29Button->setReadOnly(mReadOnly);
	mYearRuleButtonGroup->insert(mYearRuleFeb29Button);
	QWhatsThis::add(mYearRuleFeb29Button,
	      i18n("Repeat the alarm on 29th February in leap years, and on 1st March in non-leap years."));
	topLayout->addWidget(mYearRuleFeb29Button);

	// Set up the yearly date widget
	mYearRuleDayMonthButton = new RadioButton(i18n("On 7th January", "O&n %1 %2"), mYearRuleButtonGroup);
	mYearRuleDayMonthButton->setFixedSize(mYearRuleDayMonthButton->sizeHint());
	mYearRuleDayMonthButton->setReadOnly(mReadOnly);
	mYearRuleButtonGroup->insert(mYearRuleDayMonthButton);
	QWhatsThis::add(mYearRuleDayMonthButton,
	      i18n("Repeat the alarm on the selected date in the year"));
	topLayout->addWidget(mYearRuleDayMonthButton);

	// Set up the yearly position widgets
	QBoxLayout* vlayout = new QVBoxLayout(topLayout, KDialog::spacingHint());
	QBoxLayout* layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	mYearRuleOnNthTypeOfDayButton = new RadioButton(i18n("On the 1st Tuesday", "On t&he"), mYearRuleButtonGroup);
	mYearRuleOnNthTypeOfDayButton->setFixedSize(mYearRuleOnNthTypeOfDayButton->sizeHint());
	mYearRuleOnNthTypeOfDayButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mYearRuleOnNthTypeOfDayButton,
	      i18n("Repeat the alarm on one day of the week, in the selected week of a month"));
	layout->addWidget(mYearRuleOnNthTypeOfDayButton);

	mYearRuleNthNumberEntry = new ComboBox(false, mYearRuleButtonGroup);
	for (i = 0;  i < 5;  ++i)
		mYearRuleNthNumberEntry->insertItem(i18n(ordinal[i]));
	mYearRuleNthNumberEntry->insertItem(i18n("Last Monday in March", "Last"));
	mYearRuleNthNumberEntry->setFixedSize(mYearRuleNthNumberEntry->sizeHint());
	mYearRuleNthNumberEntry->setReadOnly(mReadOnly);
	QWhatsThis::add(mYearRuleNthNumberEntry,
	      i18n("Select the week of the month in which to repeat the alarm"));
	mYearRuleOnNthTypeOfDayButton->setFocusWidget(mYearRuleNthNumberEntry);
	layout->addWidget(mYearRuleNthNumberEntry);

	mYearRuleNthTypeOfDayEntry = new ComboBox(false, mYearRuleButtonGroup);
	for (i = 1;  i <= 7;  ++i)
		mYearRuleNthTypeOfDayEntry->insertItem(KGlobal::locale()->weekDayName(i));    // starts Monday
	mYearRuleNthTypeOfDayEntry->setReadOnly(mReadOnly);
	QWhatsThis::add(mYearRuleNthTypeOfDayEntry,
	      i18n("Select the day of the week on which to repeat the alarm"));
	layout->addWidget(mYearRuleNthTypeOfDayEntry);

	layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	QLabel* label = new QLabel(i18n("first week of January", "of"), mYearRuleButtonGroup);
	label->setFixedSize(label->sizeHint());
	int spac = mYearRuleOnNthTypeOfDayButton->width() - label->width();
	if (spac > 0)
		layout->addSpacing(spac);
	layout->addWidget(label);

	mYearRuleDayMonthComboBox = new ComboBox(mYearRuleButtonGroup);
	for (i = 1;  i <= 12;  ++i)
		mYearRuleDayMonthComboBox->insertItem(KGlobal::locale()->monthName(i));
	mYearRuleDayMonthComboBox->setSizeLimit(12);
	mYearRuleDayMonthComboBox->setReadOnly(mReadOnly);
	QWhatsThis::add(mYearRuleDayMonthComboBox,
	      i18n("Select the month of the year in which to repeat the alarm"));
	layout->addWidget(mYearRuleDayMonthComboBox);
	layout->addStretch();

/*	layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
	mYearRuleDayButton = new RadioButton(i18n("Recur on day"), mYearRuleButtonGroup);
	mYearRuleDayButton->setFixedSize(mYearRuleDayButton->sizeHint());
	mYearRuleDayButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mYearRuleDayButton,
	      i18n("Repeat the alarm on the selected day number in the year"));
	layout->addWidget(mYearRuleDayButton);
	mYearRuleDayEntry = new SpinBox(1, 366, 1, mYearRuleButtonGroup);
	mYearRuleDayEntry->setFixedSize(mYearRuleDayEntry->sizeHint());
	mYearRuleDayEntry->setLineShiftStep(10);
	mYearRuleDayEntry->setSelectOnStep(false);
	QWhatsThis::add(mYearRuleDayEntry,
	      i18n("Select the day number in the year on which to repeat the alarm"));
	layout->addWidget(mYearRuleDayEntry);
	layout->addStretch();*/

	mYearRuleFeb29ButtonId          = mYearRuleButtonGroup->id(mYearRuleFeb29Button);
	mYearRuleDayMonthButtonId       = mYearRuleButtonGroup->id(mYearRuleDayMonthButton);
//	mYearRuleDayButtonId            = mYearRuleButtonGroup->id(mYearRuleDayButton);
	mYearRuleOnNthTypeOfDayButtonId = mYearRuleButtonGroup->id(mYearRuleOnNthTypeOfDayButton);

	connect(mYearRuleButtonGroup, SIGNAL(buttonSet(int)), SLOT(yearlyClicked(int)));
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
		frame = mNoneRuleFrame;
		ruleButtonType = SUBDAILY;
	}
	else if (id == dailyButtonId)
	{
		frame = mNoneRuleFrame;
		whatsThis = i18n("Enter the number of days between repetitions of the alarm");
		ruleButtonType = DAILY;
	}
	else if (id == weeklyButtonId)
	{
		frame = mWeekRuleFrame;
		whatsThis = i18n("Enter the number of weeks between repetitions of the alarm");
		ruleButtonType = WEEKLY;
	}
	else if (id == monthlyButtonId)
	{
		frame = mMonthRuleFrame;
		whatsThis = i18n("Enter the number of months between repetitions of the alarm");
		ruleButtonType = MONTHLY;
	}
	else if (id == yearlyButtonId)
	{
		frame = mYearRuleFrame;
		whatsThis = i18n("Enter the number of years between repetitions of the alarm");
		ruleButtonType = ANNUAL;
	}
	else
		return;

	ruleStack->raiseWidget(frame);
	mRecurFrequency->showHourMin(subdaily);
	endTimeEdit->setEnabled(subdaily && endDateButton->isChecked());
	if (!subdaily)
		mRecurFrequency->setUnitWhatsThis(whatsThis);
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
	if (id == mMonthRuleOnNthDayButtonId)
		nthDay = true;
	else if (id == mMonthRuleOnNthTypeOfDayButtonId)
		nthDay = false;
	else
		return;

	mMonthRuleNthDayEntry->setEnabled(nthDay);
	mMonthRuleNthNumberEntry->setEnabled(!nthDay);
	mMonthRuleNthTypeOfDayEntry->setEnabled(!nthDay);
}

/******************************************************************************
 * Called when a yearly recurrence type radio button is clicked.
 */
void RecurrenceEdit::yearlyClicked(int id)
{
	bool date;
	if (id == mYearRuleDayMonthButtonId
	||  id == mYearRuleFeb29ButtonId)
		date = true;
//	else if (id == mYearRuleDayButtonId)
	else if (id == mYearRuleOnNthTypeOfDayButtonId)
		date = false;
	else
		return;

//	mYearRuleDayEntry->setEnabled(!date);
	mYearRuleNthNumberEntry->setEnabled(!date);
	mYearRuleNthTypeOfDayEntry->setEnabled(!date);
	mYearRuleDayMonthComboBox->setEnabled(!date);
}

void RecurrenceEdit::showEvent(QShowEvent*)
{
	recurEveryLabel->buddy()->setFocus();
	emit shown();
}

/******************************************************************************
 * Called when the value of the repeat count field changes, to reset the
 * minimum value to 1 if the value was 0.
 */
void RecurrenceEdit::repeatCountChanged(int value)
{
	if (value > 0  &&  mRepeatCountEntry->minValue() == 0)
		mRepeatCountEntry->setMinValue(1);
}

void RecurrenceEdit::unsetAllCheckboxes()
{
	mMonthRuleOnNthDayButton->setChecked(false);
	mMonthRuleOnNthTypeOfDayButton->setChecked(false);
	mYearRuleFeb29Button->setChecked(false);
	mYearRuleDayMonthButton->setChecked(false);
//	mYearRuleDayButton->setChecked(false);
	mYearRuleOnNthTypeOfDayButton->setChecked(false);

	for (int i = 0;  i < 7;  ++i)
		mWeekRuleDayBox[i]->setChecked(false);

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
	switch (theApp()->preferences()->defaultRecurPeriod())
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

	mRecurFrequency->setHourMinValue(1);
	mRecurFrequency->setUnitValue(1);

	checkDay(fromDate.dayOfWeek());
	mMonthRuleButtonGroup->setButton(mMonthRuleOnNthDayButtonId);    // date in month
	int day = fromDate.day() - 1;
	int dayOfWeek = fromDate.dayOfWeek() - 1;
	int month = fromDate.month() - 1;
	mMonthRuleNthDayEntry->setCurrentItem(day);
	mMonthRuleNthNumberEntry->setCurrentItem(day / 7);
	mMonthRuleNthTypeOfDayEntry->setCurrentItem(dayOfWeek);
	mYearRuleButtonGroup->setButton(mYearRuleDayMonthButtonId);     // date in year
	setStartDate(fromDate);
//	mYearRuleDayEntry->setValue(fromDate.dayOfYear());
	mYearRuleNthNumberEntry->setCurrentItem(day / 7);
	mYearRuleNthTypeOfDayEntry->setCurrentItem(dayOfWeek);
	mYearRuleDayMonthComboBox->setCurrentItem(month);

	endDateEdit->setDate(fromDate);
}

void RecurrenceEdit::setStartDate(const QDate& start)
{
	int day = start.day();
	int month = start.month();
	if (month == 3  &&  day == 1  &&  !QDate::leapYear(start.year()))
	{
		// For a start date of March 1st in a non-leap year, a recurrence on
		// either February 29th or March 1st is permissible
		mYearRuleFeb29Button->show();
	}
	else
		mYearRuleFeb29Button->hide();
	mYearRuleDayMonthButton->setText(i18n("On 7th January", "O&n %1 %2")
	                                 .arg(i18n(ordinal[day - 1]))
	                                 .arg(KGlobal::locale()->monthName(month)));
	mYearRuleDayMonthButton->setMinimumSize(mYearRuleDayMonthButton->sizeHint());
}

void RecurrenceEdit::setEndDate(const QDate& start)
{
	endDateEdit->setDate(start);
}


void RecurrenceEdit::checkDay(int day)
{
	if (day >= 1  &&  day <= 7)
		mWeekRuleDayBox[(day + 7 - mWeekRuleFirstDay) % 7]->setChecked(true);
}

void RecurrenceEdit::getCheckedDays(QBitArray& rDays)
{
	rDays.fill(false);
	for (int i = 0;  i < 7;  ++i)
		if (mWeekRuleDayBox[i]->isChecked())
			rDays.setBit((i + mWeekRuleFirstDay - 1) % 7, 1);
}

void RecurrenceEdit::setCheckedDays(QBitArray& rDays)
{
	for (int i = 0;  i < 7;  ++i)
		if (rDays.testBit((i + mWeekRuleFirstDay - 1) % 7))
			mWeekRuleDayBox[i]->setChecked(true);
}

/******************************************************************************
 * Set the state of all controls to reflect the data in the specified event.
 */
void RecurrenceEdit::set(const KAlarmEvent& event)
{
	setDefaults(event.mainDateTime().dateTime());
	int repeatDuration;
	Recurrence* recurrence = event.recurrence();
	if (!recurrence)
		return;
	switch (recurrence->doesRecur())
	{
		case Recurrence::rMinutely:
			ruleButtonGroup->setButton(subdailyButtonId);
			mRecurFrequency->setHourMinValue(recurrence->frequency());
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
			mMonthRuleButtonGroup->setButton(mMonthRuleOnNthTypeOfDayButtonId);
			QPtrList<Recurrence::rMonthPos> rmp = recurrence->monthPositions();
			int i = rmp.first()->rPos - 1;
			if (rmp.first()->negative)
				i = 5;
			mMonthRuleNthNumberEntry->setCurrentItem(i);
			for (i = 0;  !rmp.first()->rDays.testBit(i);  ++i) ;
			mMonthRuleNthTypeOfDayEntry->setCurrentItem(i);
			break;
		}
		case Recurrence::rMonthlyDay:     // on nth day of the month
		{
			ruleButtonGroup->setButton(monthlyButtonId);
			mMonthRuleButtonGroup->setButton(mMonthRuleOnNthDayButtonId);
			QPtrList<int> rmd = recurrence->monthDays();
			int i = *rmd.first() - 1;
			mMonthRuleNthDayEntry->setCurrentItem(i);
			break;
		}
		case Recurrence::rYearlyMonth:   // in the nth month of the year
		{
			ruleButtonGroup->setButton(yearlyButtonId);
			bool feb29 = (event.recursFeb29()  &&  !mYearRuleFeb29Button->isHidden());
			mYearRuleButtonGroup->setButton(feb29 ? mYearRuleFeb29ButtonId : mYearRuleDayMonthButtonId);
			break;
		}
/*		case Recurrence::rYearlyDay:     // on the nth day of the year
		{
			ruleButtonGroup->setButton(yearlyButtonId);
			mYearRuleButtonGroup->setButton(mYearRuleDayButtonId);
			break;
		}*/
		case Recurrence::rYearlyPos:     // on the nth (Tuesday) of a month in the year
		{
			ruleButtonGroup->setButton(yearlyButtonId);
			mYearRuleButtonGroup->setButton(mYearRuleOnNthTypeOfDayButtonId);
			QPtrList<Recurrence::rMonthPos> rmp = recurrence->yearMonthPositions();
			int i = rmp.first()->rPos - 1;
			if (rmp.first()->negative)
				i = 5;
			mYearRuleNthNumberEntry->setCurrentItem(i);
			for (i = 0;  !rmp.first()->rDays.testBit(i);  ++i) ;
				mYearRuleNthTypeOfDayEntry->setCurrentItem(i);
			QPtrList<int> rmd = recurrence->yearNums();
			mYearRuleDayMonthComboBox->setCurrentItem(*rmd.first() - 1);
			break;
		}
		case Recurrence::rNone:
		default:
			return;
	}

	mRecurFrequency->setUnitValue(recurrence->frequency());
	repeatDuration = event.remainingRecurrences();

	// get range information
	QDateTime endtime = currStartDateTime;
	if (repeatDuration == -1)
		noEndDateButton->setChecked(true);
	else if (repeatDuration)
	{
		repeatCountButton->setChecked(true);
		if (event.mainExpired())
		{
			mRepeatCountEntry->setMinValue(0);
			repeatDuration = 0;
		}
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
	int frequency = mRecurFrequency->value();
	QButton* button = ruleButtonGroup->selected();
	if (button == subdailyButton)
	{
		QDateTime endDateTime(endDate, endTime);
		event.setRecurMinutely(frequency, repeatCount, endDateTime);
	}
	else if (button == dailyButton)
	{
		event.setRecurDaily(frequency, repeatCount, endDate);
	}
	else if (button == weeklyButton)
	{
		QBitArray rDays(7);
		getCheckedDays(rDays);
		event.setRecurWeekly(frequency, rDays, repeatCount, endDate);
	}
	else if (button == monthlyButton)
	{
		if (mMonthRuleOnNthTypeOfDayButton->isChecked())
		{
			// it's by position
			KAlarmEvent::MonthPos pos;
			pos.days.fill(false);
			pos.days.setBit(mMonthRuleNthTypeOfDayEntry->currentItem());
			int i = mMonthRuleNthNumberEntry->currentItem() + 1;
			pos.weeknum = (i <= 5) ? i : 5 - i;
			QValueList<KAlarmEvent::MonthPos> poses;
			poses.append(pos);
			event.setRecurMonthlyByPos(frequency, poses, repeatCount, endDate);
			event.setFirstRecurrence();
		}
		else
		{
			// it's by day
			short daynum  = mMonthRuleNthDayEntry->currentItem() + 1;
			QValueList<int> daynums;
			daynums.append(daynum);
			event.setRecurMonthlyByDate(frequency, daynums, repeatCount, endDate);
		}
	}
	else if (button == yearlyButton)
	{
		if (mYearRuleOnNthTypeOfDayButton->isChecked())
		{
			// it's by position
			KAlarmEvent::MonthPos pos;
			pos.days.fill(false);
			pos.days.setBit(mYearRuleNthTypeOfDayEntry->currentItem());
			int i = mYearRuleNthNumberEntry->currentItem() + 1;
			pos.weeknum = (i <= 5) ? i : 5 - i;
			QValueList<KAlarmEvent::MonthPos> poses;
			poses.append(pos);
			int month = mYearRuleDayMonthComboBox->currentItem() + 1;
			QValueList<int> months;
			months.append(month);
			event.setRecurAnnualByPos(frequency, poses, months, repeatCount, endDate);
			event.setFirstRecurrence();
		}
/*		else if (mYearRuleDayButton->isChecked())
		{
			// it's by day
			int daynum = event.mainDate().dayOfYear();
			QValueList<int> daynums;
			daynums.append(daynum);
			event.setRecurAnnualByDay(frequency, daynums, repeatCount, endDate);
		}*/
		else
		{
			bool feb29 = mYearRuleFeb29Button->isChecked();
			QValueList<int> months;
			months.append(feb29 ? 2 : event.mainDate().month());
			event.setRecurAnnualByDate(frequency, months, feb29, repeatCount, endDate);
		}
	}
}
