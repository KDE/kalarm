/*
 *  recurrenceedit.cpp  -  widget to edit the event's recurrence definition
 *  Program:  kalarm
 *  (C) 2002 - 2004 by David Jarvie <software@astrojar.org.uk>
 *
 *  Based originally on KOrganizer module koeditorrecurrence.cpp,
 *  Copyright (c) 2000,2001 Cornelius Schumacher <schumacher@kde.org>
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

#include <qtooltip.h>
#include <qlayout.h>
#include <qvbox.h>
#include <qwidgetstack.h>
#include <qlistbox.h>
#include <qframe.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qwhatsthis.h>

#include <kglobal.h>
#include <klocale.h>
#include <kcalendarsystem.h>
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
#include "recurrenceeditprivate.moc"

namespace
{

const char* const ordinal[] = {
	I18N_NOOP("1st"),  I18N_NOOP("2nd"),  I18N_NOOP("3rd"),  I18N_NOOP("4th"),
	I18N_NOOP("5th"),  I18N_NOOP("6th"),  I18N_NOOP("7th"),  I18N_NOOP("8th"),
	I18N_NOOP("9th"),  I18N_NOOP("10th"), I18N_NOOP("11th"), I18N_NOOP("12th"),
	I18N_NOOP("13th"), I18N_NOOP("14th"), I18N_NOOP("15th"), I18N_NOOP("16th"),
	I18N_NOOP("17th"), I18N_NOOP("18th"), I18N_NOOP("19th"), I18N_NOOP("20th"),
	I18N_NOOP("21st"), I18N_NOOP("22nd"), I18N_NOOP("23rd"), I18N_NOOP("24th"),
	I18N_NOOP("25th"), I18N_NOOP("26th"), I18N_NOOP("27th"), I18N_NOOP("28th"),
	I18N_NOOP("29th"), I18N_NOOP("30th"), I18N_NOOP("31st")
};

int firstDayOfWeek = 1;
inline int dayOfWeek_to_offset(int dayOfWeek)  { return (dayOfWeek + 7 - firstDayOfWeek) % 7; }
inline int offset_to_dayOfWeek(int offset)     { return (offset + firstDayOfWeek - 1) % 7 + 1; }

}


RecurrenceEdit::RecurrenceEdit(bool readOnly, QWidget* parent, const char* name)
	: QFrame(parent, name),
	  mRuleButtonType(INVALID_RECUR),
	  mWeeklyShown(false),
	  mMonthlyShown(false),
	  mYearlyShown(false),
	  noEmitTypeChanged(true),
	  mReadOnly(readOnly)
{
	firstDayOfWeek = KGlobal::locale()->weekStartDay();

	QBoxLayout* layout;
	QVBoxLayout* topLayout = new QVBoxLayout(this, marginKDE2, KDialog::spacingHint());

	/* Create the recurrence rule Group box which holds the recurrence period
	 * selection buttons, and the weekly, monthly and yearly recurrence rule
	 * frames which specify options individual to each of these distinct
	 * sections of the recurrence rule. Each frame is made visible by the
	 * selection of its corresponding radio button.
	 */

	recurGroup = new QGroupBox(1, Qt::Vertical, i18n("Recurrence Rule"), this, "recurGroup");
	topLayout->addWidget(recurGroup);
	ruleFrame = new QFrame(recurGroup, "ruleFrame");
	layout = new QVBoxLayout(ruleFrame, 0);
	layout->addSpacing(KDialog::spacingHint()/2);

	layout = new QHBoxLayout(layout, 0);
	QBoxLayout* lay = new QVBoxLayout(layout, 0);
	ruleButtonGroup = new ButtonGroup(1, Qt::Horizontal, ruleFrame);
	ruleButtonGroup->setInsideMargin(0);
	ruleButtonGroup->setFrameStyle(QFrame::NoFrame);
	lay->addWidget(ruleButtonGroup);
	lay->addStretch();    // top-adjust the interval radio buttons
	connect(ruleButtonGroup, SIGNAL(buttonSet(int)), SLOT(periodClicked(int)));

	mNoneButton = new RadioButton(i18n("No recurrence"), ruleButtonGroup);
	mNoneButton->setFixedSize(mNoneButton->sizeHint());
	mNoneButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mNoneButton, i18n("Do not repeat the alarm"));

	mAtLoginButton = new RadioButton(i18n("At &login"), ruleButtonGroup);
	mAtLoginButton->setFixedSize(mAtLoginButton->sizeHint());
	mAtLoginButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mAtLoginButton,
	      i18n("Trigger the alarm at the specified date/time and at every login until then.\n"
	           "Note that it will also be triggered any time the alarm daemon is restarted."));

	mSubDailyButton = new RadioButton(i18n("Ho&urly/Minutely"), ruleButtonGroup);
	mSubDailyButton->setFixedSize(mSubDailyButton->sizeHint());
	mSubDailyButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mSubDailyButton,
	      i18n("Repeat the alarm at hourly/minutely intervals"));

	mDailyButton = new RadioButton(i18n("&Daily"), ruleButtonGroup);
	mDailyButton->setFixedSize(mDailyButton->sizeHint());
	mDailyButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mDailyButton,
	      i18n("Repeat the alarm at daily intervals"));

	mWeeklyButton = new RadioButton(i18n("&Weekly"), ruleButtonGroup);
	mWeeklyButton->setFixedSize(mWeeklyButton->sizeHint());
	mWeeklyButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mWeeklyButton,
	      i18n("Repeat the alarm at weekly intervals"));

	mMonthlyButton = new RadioButton(i18n("&Monthly"), ruleButtonGroup);
	mMonthlyButton->setFixedSize(mMonthlyButton->sizeHint());
	mMonthlyButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mMonthlyButton,
	      i18n("Repeat the alarm at monthly intervals"));

	mYearlyButton = new RadioButton(i18n("&Yearly"), ruleButtonGroup);
	mYearlyButton->setFixedSize(mYearlyButton->sizeHint());
	mYearlyButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mYearlyButton,
	      i18n("Repeat the alarm at annual intervals"));

	mNoneButtonId     = ruleButtonGroup->id(mNoneButton);
	mAtLoginButtonId  = ruleButtonGroup->id(mAtLoginButton);
	mSubDailyButtonId = ruleButtonGroup->id(mSubDailyButton);
	mDailyButtonId    = ruleButtonGroup->id(mDailyButton);
	mWeeklyButtonId   = ruleButtonGroup->id(mWeeklyButton);
	mMonthlyButtonId  = ruleButtonGroup->id(mMonthlyButton);
	mYearlyButtonId   = ruleButtonGroup->id(mYearlyButton);

	lay = new QVBoxLayout(layout);

	lay->addStretch();
	layout = new QHBoxLayout(lay);

	layout->addSpacing(KDialog::marginHint());
	QFrame* divider = new QFrame(ruleFrame);
	divider->setFrameStyle(QFrame::VLine | QFrame::Sunken);
	layout->addWidget(divider);
	layout->addSpacing(KDialog::marginHint());

	initNone();
	initSubDaily();
	initDaily();
	initWeekly();
	initMonthly();
	initYearly();

	ruleStack = new QWidgetStack(ruleFrame);
	layout->addWidget(ruleStack);
	layout->addStretch(1);
	ruleStack->addWidget(mNoneRuleFrame, 0);
	ruleStack->addWidget(mSubDayRuleFrame, 1);
	ruleStack->addWidget(mDayRuleFrame, 2);
	ruleStack->addWidget(mWeekRuleFrame, 3);
	ruleStack->addWidget(mMonthRuleFrame, 4);
	ruleStack->addWidget(mYearRuleFrame, 5);
	layout->addSpacing(KDialog::marginHint());

	// Create the recurrence range group which contains the controls
	// which specify how long the recurrence is to last.

	mRangeButtonGroup = new ButtonGroup(i18n("Recurrence End"), this, "mRangeButtonGroup");
	connect(mRangeButtonGroup, SIGNAL(buttonSet(int)), SLOT(rangeTypeClicked()));
	topLayout->addWidget(mRangeButtonGroup);

	QVBoxLayout* vlayout = new QVBoxLayout(mRangeButtonGroup, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	vlayout->addSpacing(fontMetrics().lineSpacing()/2);
	mNoEndDateButton = new RadioButton(i18n("No &end"), mRangeButtonGroup);
	mNoEndDateButton->setFixedSize(mNoEndDateButton->sizeHint());
	mNoEndDateButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mNoEndDateButton, i18n("Repeat the alarm indefinitely"));
	vlayout->addWidget(mNoEndDateButton, 1, Qt::AlignLeft);
	QSize size = mNoEndDateButton->size();

	layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	mRepeatCountButton = new RadioButton(i18n("End a&fter:"), mRangeButtonGroup);
	mRepeatCountButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mRepeatCountButton,
	      i18n("Repeat the alarm for the number of times specified"));
	mRepeatCountEntry = new SpinBox(1, 9999, 1, mRangeButtonGroup);
	mRepeatCountEntry->setFixedSize(mRepeatCountEntry->sizeHint());
	mRepeatCountEntry->setLineShiftStep(10);
	mRepeatCountEntry->setSelectOnStep(false);
	mRepeatCountEntry->setReadOnly(mReadOnly);
	connect(mRepeatCountEntry, SIGNAL(valueChanged(int)), SLOT(repeatCountChanged(int)));
	QWhatsThis::add(mRepeatCountEntry,
	      i18n("Enter the total number of times to trigger the alarm"));
	mRepeatCountButton->setFocusWidget(mRepeatCountEntry);
	mRepeatCountLabel = new QLabel(i18n("occurrence(s)"), mRangeButtonGroup);
	mRepeatCountLabel->setFixedSize(mRepeatCountLabel->sizeHint());
	layout->addWidget(mRepeatCountButton);
	layout->addSpacing(KDialog::spacingHint());
	layout->addWidget(mRepeatCountEntry);
	layout->addWidget(mRepeatCountLabel);
	layout->addStretch();
	size = size.expandedTo(mRepeatCountButton->sizeHint());

	layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
	mEndDateButton = new RadioButton(i18n("End &by:"), mRangeButtonGroup);
	mEndDateButton->setReadOnly(mReadOnly);
	QWhatsThis::add(mEndDateButton,
	      i18n("Repeat the alarm until the date/time specified"));
	mEndDateEdit = new DateEdit(mRangeButtonGroup);
	mEndDateEdit->setFixedSize(mEndDateEdit->sizeHint());
	mEndDateEdit->setReadOnly(mReadOnly);
	QWhatsThis::add(mEndDateEdit,
	      i18n("Enter the last date to repeat the alarm"));
	mEndDateButton->setFocusWidget(mEndDateEdit);
	mEndTimeEdit = new TimeSpinBox(mRangeButtonGroup);
	mEndTimeEdit->setFixedSize(mEndTimeEdit->sizeHint());
	mEndTimeEdit->setReadOnly(mReadOnly);
	static const QString lastTimeText = i18n("Enter the last time to repeat the alarm.");
	QWhatsThis::add(mEndTimeEdit, QString("%1\n\n%2").arg(lastTimeText).arg(TimeSpinBox::shiftWhatsThis()));
	mEndAnyTimeCheckBox = new CheckBox(i18n("Any time"), mRangeButtonGroup);
	mEndAnyTimeCheckBox->setFixedSize(mEndAnyTimeCheckBox->sizeHint());
	mEndAnyTimeCheckBox->setReadOnly(mReadOnly);
	connect(mEndAnyTimeCheckBox, SIGNAL(toggled(bool)), SLOT(slotAnyTimeToggled(bool)));
	QWhatsThis::add(mEndAnyTimeCheckBox,
	      i18n("Stop repeating the alarm after your first login on or after the specified end date"));
	layout->addWidget(mEndDateButton);
	layout->addSpacing(KDialog::spacingHint());
	layout->addWidget(mEndDateEdit);
	layout->addWidget(mEndTimeEdit);
	layout->addWidget(mEndAnyTimeCheckBox);
	layout->addStretch();
	size = size.expandedTo(mEndDateButton->sizeHint());

	// Line up the widgets to the right of the radio buttons
	mRepeatCountButton->setFixedSize(size);
	mEndDateButton->setFixedSize(size);

	// Create the exceptions group which specifies dates to be excluded
	// from the recurrence.

	mExceptionGroup = new QGroupBox(i18n("E&xceptions"), this, "mExceptionGroup");
	topLayout->addWidget(mExceptionGroup);
	topLayout->setStretchFactor(mExceptionGroup, 2);
	vlayout = new QVBoxLayout(mExceptionGroup, marginKDE2 + KDialog::marginHint(), KDialog::spacingHint());
	vlayout->addSpacing(fontMetrics().lineSpacing()/2);
	QGridLayout *grid = new QGridLayout(vlayout, 4, 4, 0);

	mExceptionDateList = new QListBox(mExceptionGroup);
	mExceptionDateList->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	connect(mExceptionDateList, SIGNAL(selectionChanged()), SLOT(enableExceptionButtons()));
	QWhatsThis::add(mExceptionDateList,
	      i18n("The list of exceptions, i.e. dates/times excluded from the recurrence"));
	grid->addMultiCellWidget(mExceptionDateList, 0, 3, 0, 0);
	grid->setColStretch(0, 1);
	grid->addColSpacing(1, KDialog::spacingHint());

	if (mReadOnly)
	{
		mExceptionDateEdit     = 0;
		mChangeExceptionButton = 0;
		mDeleteExceptionButton = 0;
	}
	else
	{
		mExceptionDateEdit = new DateEdit(mExceptionGroup);
		mExceptionDateEdit->setFixedSize(mExceptionDateEdit->sizeHint());
		mExceptionDateEdit->setDate(QDate::currentDate());
		QWhatsThis::add(mExceptionDateEdit,
		      i18n("Enter a date to insert in the exceptions list. "
		           "Use in conjunction with the Add or Change button below."));
		grid->addMultiCellWidget(mExceptionDateEdit, 0, 0, 2, 3, Qt::AlignLeft);

		QPushButton* button = new QPushButton(i18n("Add"), mExceptionGroup);
		button->setFixedSize(button->sizeHint());
		connect(button, SIGNAL(clicked()), SLOT(addException()));
		QWhatsThis::add(button,
		      i18n("Add the date entered above to the exceptions list"));
		grid->addWidget(button, 1, 2);

		mChangeExceptionButton = new QPushButton(i18n("Change"), mExceptionGroup);
		mChangeExceptionButton->setFixedSize(mChangeExceptionButton->sizeHint());
		connect(mChangeExceptionButton, SIGNAL(clicked()), SLOT(changeException()));
		QWhatsThis::add(mChangeExceptionButton,
		      i18n("Replace the currently highlighted item in the exceptions list with the date entered above"));
		grid->addWidget(mChangeExceptionButton, 1, 3);
#if !KDE_IS_VERSION(3,1,90)
		grid->addRowSpacing(2, KDialog::spacingHint());
#endif

		mDeleteExceptionButton = new QPushButton(i18n("Delete"), mExceptionGroup);
		mDeleteExceptionButton->setFixedSize(mDeleteExceptionButton->sizeHint());
		connect(mDeleteExceptionButton, SIGNAL(clicked()), SLOT(deleteException()));
		QWhatsThis::add(mDeleteExceptionButton,
		      i18n("Remove the currently highlighted item from the exceptions list"));
		grid->addMultiCellWidget(mDeleteExceptionButton, 3, 3, 2, 3, Qt::AlignLeft | Qt::AlignVCenter);
		grid->setRowStretch(3, 2);
	}

	noEmitTypeChanged = false;
}

void RecurrenceEdit::initNone()
{
	mNoneRuleFrame = new QFrame(ruleFrame);
	mNoneRuleFrame->setFrameStyle(QFrame::NoFrame);
}

/******************************************************************************
 * Set up the daily recurrence dialog controls.
 */
void RecurrenceEdit::initSubDaily()
{
	mSubDayRuleFrame = new QFrame(ruleFrame, "subdayFrame");
	mSubDayRuleFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* topLayout = new QVBoxLayout(mSubDayRuleFrame, 0, KDialog::spacingHint());

	mSubDayRecurFrequency = new RecurFrequency(true, i18n("hours:minutes"),
	                                 i18n("Enter the number of hours and minutes between repetitions of the alarm"),
	                                 mReadOnly, mSubDayRuleFrame);
	connect(mSubDayRecurFrequency, SIGNAL(valueChanged()), SIGNAL(frequencyChanged()));
	topLayout->addWidget(mSubDayRecurFrequency);
}

/******************************************************************************
 * Set up the daily recurrence dialog controls.
 */
void RecurrenceEdit::initDaily()
{
	mDayRuleFrame = new QFrame(ruleFrame, "dayFrame");
	mDayRuleFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* topLayout = new QVBoxLayout(mDayRuleFrame, 0, KDialog::spacingHint());

	mDayRecurFrequency = new RecurFrequency(false, i18n("day(s)"),
	                                 i18n("Enter the number of days between repetitions of the alarm"),
	                                 mReadOnly, mDayRuleFrame);
	connect(mDayRecurFrequency, SIGNAL(valueChanged()), SIGNAL(frequencyChanged()));
	topLayout->addWidget(mDayRecurFrequency);
}

/******************************************************************************
 * Set up the weekly recurrence dialog controls.
 */
void RecurrenceEdit::initWeekly()
{
	mWeekRuleFrame = new QFrame(ruleFrame, "weekFrame");
	mWeekRuleFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* topLayout = new QVBoxLayout(mWeekRuleFrame, 0, KDialog::spacingHint());

	mWeekRecurFrequency = new RecurFrequency(false, i18n("week(s)"),
	                                 i18n("Enter the number of weeks between repetitions of the alarm"),
	                                 mReadOnly, mWeekRuleFrame);
	connect(mWeekRecurFrequency, SIGNAL(valueChanged()), SIGNAL(frequencyChanged()));
	topLayout->addWidget(mWeekRecurFrequency);

	QGridLayout* grid = new QGridLayout(topLayout, 1, 4, KDialog::spacingHint());
	grid->setRowStretch(0, 1);

	QLabel* label = new QLabel(i18n("On: Tuesday", "O&n:"), mWeekRuleFrame);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 0, 0, Qt::AlignRight | Qt::AlignTop);
	grid->addColSpacing(1, KDialog::spacingHint());

	// List the days of the week starting at the user's start day of the week.
	// Save the first day of the week, just in case it changes while the dialog is open.
	QWidget* box = new QWidget(mWeekRuleFrame);   // this is to control the QWhatsThis text display area
	QGridLayout* dgrid = new QGridLayout(box, 4, 2, 0, KDialog::spacingHint());
	const KCalendarSystem* calendar = KGlobal::locale()->calendar();
	for (int i = 0;  i < 7;  ++i)
	{
		int day = offset_to_dayOfWeek(i);
		mWeekRuleDayBox[i] = new CheckBox(calendar->weekDayName(day), box);
		mWeekRuleDayBox[i]->setFixedSize(mWeekRuleDayBox[i]->sizeHint());
		mWeekRuleDayBox[i]->setReadOnly(mReadOnly);
		dgrid->addWidget(mWeekRuleDayBox[i], i%4, i/4, Qt::AlignLeft);
	}
	box->setFixedSize(box->sizeHint());
	QWhatsThis::add(box,
	      i18n("Select the days of the week on which to repeat the alarm"));
	grid->addWidget(box, 0, 2, Qt::AlignLeft);
	label->setBuddy(mWeekRuleDayBox[0]);
	grid->setColStretch(3, 1);
}

/******************************************************************************
 * Set up the monthly recurrence dialog controls.
 */
void RecurrenceEdit::initMonthly()
{
	mMonthRuleFrame = new QFrame(ruleFrame, "monthFrame");
	mMonthRuleFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* topLayout = new QVBoxLayout(mMonthRuleFrame, 0, KDialog::spacingHint());

	mMonthRecurFrequency = new RecurFrequency(false, i18n("month(s)"),
	                                 i18n("Enter the number of months between repetitions of the alarm"),
	                                 mReadOnly, mMonthRuleFrame);
	connect(mMonthRecurFrequency, SIGNAL(valueChanged()), SIGNAL(frequencyChanged()));
	topLayout->addWidget(mMonthRecurFrequency );

	mMonthRuleButtonGroup = new ButtonGroup(mMonthRuleFrame);
	mMonthRuleButtonGroup->setFrameStyle(QFrame::NoFrame);
	topLayout->addWidget(mMonthRuleButtonGroup);
	QBoxLayout* groupLayout = new QVBoxLayout(mMonthRuleButtonGroup);

	initDayOfMonth(&mMonthRuleOnNthDayButton, &mMonthRuleNthDayEntry, mMonthRuleButtonGroup, groupLayout);

	initWeekOfMonth(&mMonthRuleOnNthTypeOfDayButton, &mMonthRuleNthNumberEntry, &mMonthRuleNthTypeOfDayEntry, mMonthRuleButtonGroup, groupLayout);

	mMonthRuleOnNthDayButtonId       = mMonthRuleButtonGroup->id(mMonthRuleOnNthDayButton);
	mMonthRuleOnNthTypeOfDayButtonId = mMonthRuleButtonGroup->id(mMonthRuleOnNthTypeOfDayButton);

	connect(mMonthRuleButtonGroup, SIGNAL(buttonSet(int)), SLOT(monthlyClicked(int)));
}

/******************************************************************************
 * Set up the yearly recurrence dialog controls.
 */
void RecurrenceEdit::initYearly()
{
	mYearRuleFrame = new QFrame(ruleFrame, "yearFrame");
	mYearRuleFrame->setFrameStyle(QFrame::NoFrame);
	QBoxLayout* topLayout = new QVBoxLayout(mYearRuleFrame, 0, KDialog::spacingHint());

	mYearRecurFrequency = new RecurFrequency(false, i18n("year(s)"),
	                                 i18n("Enter the number of years between repetitions of the alarm"),
	                                 mReadOnly, mYearRuleFrame);
	connect(mYearRecurFrequency, SIGNAL(valueChanged()), SIGNAL(frequencyChanged()));
	topLayout->addWidget(mYearRecurFrequency);

	mYearRuleButtonGroup = new ButtonGroup(mYearRuleFrame);
	mYearRuleButtonGroup->setFrameStyle(QFrame::NoFrame);
	topLayout->addWidget(mYearRuleButtonGroup);
	QBoxLayout* groupLayout = new QVBoxLayout(mYearRuleButtonGroup);

	// Set up the yearly date widgets
	initDayOfMonth(&mYearRuleDayMonthButton, &mYearRuleNthDayEntry, mYearRuleButtonGroup, groupLayout);
	connect(mYearRuleNthDayEntry, SIGNAL(activated(int)), SLOT(yearDayOfMonthSelected(int)));

	// Set up the yearly position widgets
	initWeekOfMonth(&mYearRuleOnNthTypeOfDayButton, &mYearRuleNthNumberEntry, &mYearRuleNthTypeOfDayEntry, mYearRuleButtonGroup, groupLayout);

	// Set up the month selection widgets
	QGridLayout* grid = new QGridLayout(groupLayout, KDialog::spacingHint());
	grid->addRowSpacing(0, KDialog::marginHint());
	QLabel* label = new QLabel(i18n("first week of January", "of:"), mYearRuleButtonGroup);
	label->setFixedSize(label->sizeHint());
	grid->addWidget(label, 1, 0, Qt::AlignLeft | Qt::AlignTop);
	grid->addColSpacing(1, KDialog::spacingHint());

	// List the months of the year.
	QWidget* box = new QWidget(mYearRuleButtonGroup);   // this is to control the QWhatsThis text display area
	QGridLayout* mgrid = new QGridLayout(box, 4, 3, 0, KDialog::spacingHint());
	const KCalendarSystem* calendar = KGlobal::locale()->calendar();
	for (int i = 0;  i < 12;  ++i)
	{
		mYearRuleMonthBox[i] = new CheckBox(calendar->monthName(i + 1, 2000), box);
		mYearRuleMonthBox[i]->setFixedSize(mYearRuleMonthBox[i]->sizeHint());
		mYearRuleMonthBox[i]->setReadOnly(mReadOnly);
		mgrid->addWidget(mYearRuleMonthBox[i], i%4, i/4, Qt::AlignLeft);
	}
	box->setFixedSize(box->sizeHint());
	QWhatsThis::add(box,
	      i18n("Select the months of the year in which to repeat the alarm"));
	grid->addWidget(box, 1, 2, Qt::AlignLeft);
	grid->setColStretch(2, 1);

/*	layout = new QHBoxLayout(groupLayout, KDialog::spacingHint());
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

	mYearRuleDayMonthButtonId       = mYearRuleButtonGroup->id(mYearRuleDayMonthButton);
//	mYearRuleDayButtonId            = mYearRuleButtonGroup->id(mYearRuleDayButton);
	mYearRuleOnNthTypeOfDayButtonId = mYearRuleButtonGroup->id(mYearRuleOnNthTypeOfDayButton);

	connect(mYearRuleButtonGroup, SIGNAL(buttonSet(int)), SLOT(yearlyClicked(int)));
}

/******************************************************************************
 * Initialise a day-of-the-month selection combo box.
 */
void RecurrenceEdit::initDayOfMonth(RadioButton** radio, ComboBox** combo, QWidget* parent, QBoxLayout* groupLayout)
{
	QBoxLayout* layout = new QHBoxLayout(groupLayout, KDialog::spacingHint());
	*radio = new RadioButton(i18n("On the 7th day", "O&n the"), parent);
	RadioButton* r = *radio;
	r->setFixedSize(r->sizeHint());
	r->setReadOnly(mReadOnly);
	QWhatsThis::add(r, i18n("Repeat the alarm on the selected day of the month"));
	layout->addWidget(r);

	*combo = new ComboBox(false, parent);
	ComboBox* c = *combo;
	c->setSizeLimit(11);
	for (int i = 0;  i < 31;  ++i)
		c->insertItem(i18n(ordinal[i]));
	c->insertItem(i18n("Last day of month", "Last"));
	c->setFixedSize(c->sizeHint());
	c->setReadOnly(mReadOnly);
	QWhatsThis::add(c, i18n("Select the day of the month on which to repeat the alarm"));
	r->setFocusWidget(c);
	layout->addWidget(c);

	QLabel* label = new QLabel(i18n("day"), parent);
	label->setFixedSize(label->sizeHint());
	layout->addWidget(label);
	layout->addStretch();
}

/******************************************************************************
 * Initialise a day in the week-of-the-month selection combo box.
 */
void RecurrenceEdit::initWeekOfMonth(RadioButton** radio, ComboBox** weekCombo, ComboBox** dayCombo, QWidget* parent, QBoxLayout* groupLayout)
{
	int i;

	QBoxLayout* layout = new QHBoxLayout(groupLayout, KDialog::spacingHint());
	*radio = new RadioButton(i18n("On the 1st Tuesday", "On t&he"), parent);
	RadioButton* r = *radio;
	r->setFixedSize(r->sizeHint());
	r->setReadOnly(mReadOnly);
	QWhatsThis::add(r,
	      i18n("Repeat the alarm on one day of the week, in the selected week of the month"));
	layout->addWidget(r);

	*weekCombo = new ComboBox(false, parent);
	ComboBox* wc = *weekCombo;
	for (i = 0;  i < 5;  ++i)
		wc->insertItem(i18n(ordinal[i]));
	wc->insertItem(i18n("Last Monday in March", "Last"));
	wc->insertItem(i18n("2nd last"));
	wc->insertItem(i18n("3rd last"));
	wc->insertItem(i18n("4th last"));
	wc->insertItem(i18n("5th last"));
	QWhatsThis::add(wc, i18n("Select the week of the month in which to repeat the alarm"));
	wc->setFixedSize(wc->sizeHint());
	wc->setReadOnly(mReadOnly);
	r->setFocusWidget(wc);
	layout->addWidget(wc);

	*dayCombo = new ComboBox(false, parent);
	ComboBox* dc = *dayCombo;
	const KCalendarSystem* calendar = KGlobal::locale()->calendar();
	for (i = 0;  i < 7;  ++i)
	{
		int day = offset_to_dayOfWeek(i);
		dc->insertItem(calendar->weekDayName(day));
	}
	dc->setReadOnly(mReadOnly);
	QWhatsThis::add(dc,
	      i18n("Select the day of the week on which to repeat the alarm"));
	layout->addWidget(dc);
	layout->addStretch();
}

/******************************************************************************
 * Verify the consistency of the entered data.
 */
QWidget* RecurrenceEdit::checkData(const QDateTime& startDateTime, QString& errorMessage) const
{
	if (mAtLoginButton->isOn())
		return 0;
	const_cast<RecurrenceEdit*>(this)->currStartDateTime = startDateTime;
	if (mEndDateButton->isChecked())
	{
		QWidget* errWidget = 0;
		bool noTime = !mEndTimeEdit->isEnabled();
		QDate endDate = mEndDateEdit->date();
		if (endDate < startDateTime.date())
			errWidget = mEndDateEdit;
		else if (!noTime  &&  QDateTime(endDate, mEndTimeEdit->time()) < startDateTime)
			errWidget = mEndTimeEdit;
		if (errWidget)
		{
			errorMessage = noTime
			             ? i18n("End date is earlier than start date")
			             : i18n("End date/time is earlier than start date/time");
			return errWidget;
		}
	}
	QButton* button = ruleButtonGroup->selected();
	if (button == mWeeklyButton)
	{
		// Weekly recurrence: check that at least one day is selected
		QBitArray days(7);
		if (!getCheckedDays(days))
		{
			errorMessage = i18n("No day selected");
			return mWeekRuleDayBox[0];
		}
	}
	else if (button == mYearlyButton)
	{
		// Yearly recurrence: check that at least one month is selected
		QValueList<int> months;
		getCheckedMonths(months);
		if (!months.count())
		{
			errorMessage = i18n("No month selected");
			return mYearRuleMonthBox[0];
		}
	}
	return 0;
}

/******************************************************************************
 * Called when a recurrence period radio button is clicked.
 */
void RecurrenceEdit::periodClicked(int id)
{
	QFrame* frame;
	RepeatType oldType = mRuleButtonType;
	bool none     = (id == mNoneButtonId);
	bool atLogin  = (id == mAtLoginButtonId);
	bool subdaily = (id == mSubDailyButtonId);
	if (none)
	{
		frame = mNoneRuleFrame;
		mRuleButtonType = NO_RECUR;
	}
	else if (atLogin)
	{
		frame = mNoneRuleFrame;
		mRuleButtonType = AT_LOGIN;
		mRangeButtonGroup->setButton(mRangeButtonGroup->id(mEndDateButton));
	}
	else if (subdaily)
	{
		frame = mSubDayRuleFrame;
		mRuleButtonType = SUBDAILY;
	}
	else if (id == mDailyButtonId)
	{
		frame = mDayRuleFrame;
		mRuleButtonType = DAILY;
	}
	else if (id == mWeeklyButtonId)
	{
		frame = mWeekRuleFrame;
		mRuleButtonType = WEEKLY;
		mWeeklyShown = true;
	}
	else if (id == mMonthlyButtonId)
	{
		frame = mMonthRuleFrame;
		mRuleButtonType = MONTHLY;
		mMonthlyShown = true;
	}
	else if (id == mYearlyButtonId)
	{
		frame = mYearRuleFrame;
		mRuleButtonType = ANNUAL;
		mYearlyShown = true;
	}
	else
		return;

	if (mRuleButtonType != oldType)
	{
		ruleStack->raiseWidget(frame);
		if (oldType == NO_RECUR  ||  none)
			mRangeButtonGroup->setEnabled(!none);
		mExceptionGroup->setEnabled(!(none || atLogin));
		mEndAnyTimeCheckBox->setEnabled(atLogin);
		if (!none)
		{
			mNoEndDateButton->setEnabled(!atLogin);
			mRepeatCountButton->setEnabled(!atLogin);
		}
		rangeTypeClicked();
		if (!noEmitTypeChanged)
			emit typeChanged(mRuleButtonType);
	}
}

void RecurrenceEdit::slotAnyTimeToggled(bool on)
{
	QButton* button = ruleButtonGroup->selected();
	mEndTimeEdit->setEnabled(button == mAtLoginButton && !on
	                     ||  button == mSubDailyButton && mEndDateButton->isChecked());
}

/******************************************************************************
 * Called when a recurrence range type radio button is clicked.
 */
void RecurrenceEdit::rangeTypeClicked()
{
	bool endDate = mEndDateButton->isOn();
	mEndDateEdit->setEnabled(endDate);
	mEndTimeEdit->setEnabled(endDate
	                         &&  (mAtLoginButton->isOn() && !mEndAnyTimeCheckBox->isChecked()
	                              ||  mSubDailyButton->isOn()));
	bool repeatCount = mRepeatCountButton->isOn();
	mRepeatCountEntry->setEnabled(repeatCount);
	mRepeatCountLabel->setEnabled(repeatCount);
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
	if (id == mYearRuleDayMonthButtonId)
		date = true;
//	else if (id == mYearRuleDayButtonId)
	else if (id == mYearRuleOnNthTypeOfDayButtonId)
		date = false;
	else
		return;

	mYearRuleNthDayEntry->setEnabled(date);
//	mYearRuleDayEntry->setEnabled(!date);
	mYearRuleNthNumberEntry->setEnabled(!date);
	mYearRuleNthTypeOfDayEntry->setEnabled(!date);
}

/******************************************************************************
 * Called when a day of the month is selected in a yearly recurrence, to
 * disable months for which the day is out of range.
 */
void RecurrenceEdit::yearDayOfMonthSelected(int index)
{
	mYearRuleMonthBox[1]->setEnabled(index < 29  ||  index > 30);     // February
	bool enable = (index != 30);
	mYearRuleMonthBox[3]->setEnabled(enable);     // April
	mYearRuleMonthBox[5]->setEnabled(enable);     // June
	mYearRuleMonthBox[8]->setEnabled(enable);     // September
	mYearRuleMonthBox[10]->setEnabled(enable);    // November
}

void RecurrenceEdit::showEvent(QShowEvent*)
{
	QButton* button = ruleButtonGroup->selected();
	QWidget* w = (button == mSubDailyButton) ? mSubDayRecurFrequency
	           : (button == mDailyButton)    ? mDayRecurFrequency
	           : (button == mWeeklyButton)   ? mWeekRecurFrequency
	           : (button == mMonthlyButton)  ? mMonthRecurFrequency
	           : (button == mYearlyButton)   ? mYearRecurFrequency
	           : (QWidget*)button;
	w->setFocus();
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

/******************************************************************************
 * Add the date entered in the exception date edit control to the list of
 * exception dates.
 */
void RecurrenceEdit::addException()
{
	if (!mExceptionDateEdit  ||  !mExceptionDateEdit->isValid())
		return;
	QDate date = mExceptionDateEdit->date();
	QValueList<QDate>::Iterator it;
	int index = 0;
	bool insert = true;
	for (it = mExceptionDates.begin();  it != mExceptionDates.end();  ++index, ++it)
	{
		if (date <= *it)
		{
			insert = (date != *it);
			break;
		}
	}
	if (insert)
	{
		mExceptionDates.insert(it, date);
		mExceptionDateList->insertItem(KGlobal::locale()->formatDate(date), index);
	}
	mExceptionDateList->setCurrentItem(index);
	enableExceptionButtons();
}

/******************************************************************************
 * Change the currently highlighted exception date to that entered in the
 * exception date edit control.
 */
void RecurrenceEdit::changeException()
{
	if (!mExceptionDateEdit  ||  !mExceptionDateEdit->isValid())
		return;
	int index = mExceptionDateList->currentItem();
	if (index >= 0  &&  mExceptionDateList->isSelected(index))
	{
		QDate olddate = mExceptionDates[index];
		QDate newdate = mExceptionDateEdit->date();
		if (newdate != olddate)
		{
			mExceptionDates.remove(mExceptionDates.at(index));
			mExceptionDateList->removeItem(index);
			addException();
		}
	}
}

/******************************************************************************
 * Delete the currently highlighted exception date.
 */
void RecurrenceEdit::deleteException()
{
	int index = mExceptionDateList->currentItem();
	if (index >= 0  &&  mExceptionDateList->isSelected(index))
	{
		mExceptionDates.remove(mExceptionDates.at(index));
		mExceptionDateList->removeItem(index);
		enableExceptionButtons();
	}
}

/******************************************************************************
 * Delete the currently highlighted exception date.
 */
void RecurrenceEdit::enableExceptionButtons()
{
	int index = mExceptionDateList->currentItem();
	bool enable = (index >= 0  &&  mExceptionDateList->isSelected(index));
	if (mDeleteExceptionButton)
		mDeleteExceptionButton->setEnabled(enable);
	if (mChangeExceptionButton)
		mChangeExceptionButton->setEnabled(enable);
}

/******************************************************************************
 * Notify this instance of a change in the alarm start date.
 */
void RecurrenceEdit::setStartDate(const QDate& start, const QDate& today)
{
	if (!mReadOnly)
	{
		setRuleDefaults(start);
		if (start < today)
		{
			mEndDateEdit->setMinDate(today);
			if (mExceptionDateEdit)
				mExceptionDateEdit->setMinDate(today);
		}
		else
		{
			const QString startString = i18n("Date cannot be earlier than start date", "start date");
			mEndDateEdit->setMinDate(start, startString);
			if (mExceptionDateEdit)
				mExceptionDateEdit->setMinDate(start, startString);
		}
	}
}

/******************************************************************************
 * Specify the default recurrence end date.
 */
void RecurrenceEdit::setDefaultEndDate(const QDate& end)
{
	if (!mEndDateButton->isOn())
		mEndDateEdit->setDate(end);
}

void RecurrenceEdit::setEndDateTime(const DateTime& end)
{
	mEndDateEdit->setDate(end.date());
	mEndTimeEdit->setValue(end.time().hour()*60 + end.time().minute());
	mEndTimeEdit->setEnabled(!end.isDateOnly());
	mEndAnyTimeCheckBox->setChecked(end.isDateOnly());
}

DateTime RecurrenceEdit::endDateTime() const
{
	if (ruleButtonGroup->selected() == mAtLoginButton  &&  mEndAnyTimeCheckBox->isChecked())
		return DateTime(mEndDateEdit->date());
	return DateTime(mEndDateEdit->date(), mEndTimeEdit->time());
}

bool RecurrenceEdit::getCheckedDays(QBitArray& rDays) const
{
	bool found = false;
	rDays.fill(false);
	for (int i = 0;  i < 7;  ++i)
		if (mWeekRuleDayBox[i]->isChecked())
		{
			rDays.setBit(offset_to_dayOfWeek(i) - 1, 1);
			found = true;
		}
	return found;
}

void RecurrenceEdit::setCheckedDays(QBitArray& rDays)
{
	for (int i = 0;  i < 7;  ++i)
		if (rDays.testBit(offset_to_dayOfWeek(i) - 1))
			mWeekRuleDayBox[i]->setChecked(true);
}

/******************************************************************************
 * Fetch which months have been checked (1 - 12).
 * Reply = true if February has been checked.
 */
bool RecurrenceEdit::getCheckedMonths(QValueList<int>& months) const
{
	bool feb = false;
	months.clear();
	for (int i = 0;  i < 12;  ++i)
		if (mYearRuleMonthBox[i]->isChecked()  &&  mYearRuleMonthBox[i]->isEnabled())
		{
			months.append(i + 1);
			if (i == 1)
				feb = true;
		}
	return feb;
}

/******************************************************************************
 * Set all controls to their default values.
 */
void RecurrenceEdit::setDefaults(const QDateTime& from)
{
	currStartDateTime = from;
	QDate fromDate = from.date();
	mNoEndDateButton->setChecked(true);

	mSubDayRecurFrequency->setValue(1);
	mDayRecurFrequency->setValue(1);
	mWeekRecurFrequency->setValue(1);
	mMonthRecurFrequency->setValue(1);
	mYearRecurFrequency->setValue(1);

	setRuleDefaults(fromDate);
	mMonthRuleButtonGroup->setButton(mMonthRuleOnNthDayButtonId);   // date in month
	mYearRuleButtonGroup->setButton(mYearRuleDayMonthButtonId);     // date in year

	mEndDateEdit->setDate(fromDate);

	noEmitTypeChanged = true;
	int button;
	switch (Preferences::instance()->defaultRecurPeriod())
	{
		case AT_LOGIN: button = mAtLoginButtonId;  break;
		case ANNUAL:   button = mYearlyButtonId;   break;
		case MONTHLY:  button = mMonthlyButtonId;  break;
		case WEEKLY:   button = mWeeklyButtonId;   break;
		case DAILY:    button = mDailyButtonId;    break;
		case SUBDAILY: button = mSubDailyButtonId; break;
		case NO_RECUR:
		default:       button = mNoneButtonId;     break;
	}
	ruleButtonGroup->setButton(button);
	noEmitTypeChanged = false;
	rangeTypeClicked();
	enableExceptionButtons();
}

/******************************************************************************
 * Set the controls for weekly, monthly and yearly rules to their default
 * values, depending on the recurrence start date.
 */
void RecurrenceEdit::setRuleDefaults(const QDate& fromDate)
{
	int day       = fromDate.day() - 1;
	int dayOfWeek = fromDate.dayOfWeek();
	int month     = fromDate.month() - 1;
	if (!mWeeklyShown)
	{
		for (int i = 0;  i < 7;  ++i)
			mWeekRuleDayBox[i]->setChecked(false);
		if (dayOfWeek > 0  &&  dayOfWeek <= 7)
			mWeekRuleDayBox[dayOfWeek_to_offset(dayOfWeek)]->setChecked(true);
	}
	if (!mMonthlyShown)
	{
		mMonthRuleNthDayEntry->setCurrentItem(day);
		mMonthRuleNthNumberEntry->setCurrentItem(day / 7);
		mMonthRuleNthTypeOfDayEntry->setCurrentItem(dayOfWeek_to_offset(dayOfWeek));
	}
	if (!mYearlyShown)
	{
//		mYearRuleDayEntry->setValue(fromDate.dayOfYear());
		mYearRuleNthDayEntry->setCurrentItem(day);
		mYearRuleNthNumberEntry->setCurrentItem(day / 7);
		mYearRuleNthTypeOfDayEntry->setCurrentItem(dayOfWeek_to_offset(dayOfWeek));
		for (int i = 0;  i < 12;  ++i)
			mYearRuleMonthBox[i]->setChecked(i == month);
	}
}

/******************************************************************************
 * Set the state of all controls to reflect the data in the specified event.
 */
void RecurrenceEdit::set(const KAEvent& event)
{
	setDefaults(event.mainDateTime().dateTime());
	if (event.repeatAtLogin())
	{
		ruleButtonGroup->setButton(mAtLoginButtonId);
		mEndDateButton->setChecked(true);
		return;
	}
	ruleButtonGroup->setButton(mNoneButtonId);
	int repeatDuration;
	Recurrence* recurrence = event.recurrence();
	if (!recurrence)
		return;
	short rtype = recurrence->doesRecur();
	switch (rtype)
	{
		case Recurrence::rMinutely:
			ruleButtonGroup->setButton(mSubDailyButtonId);
			mSubDayRecurFrequency->setValue(recurrence->frequency());
			break;

		case Recurrence::rDaily:
			ruleButtonGroup->setButton(mDailyButtonId);
			mDayRecurFrequency->setValue(recurrence->frequency());
			break;

		case Recurrence::rWeekly:
		{
			ruleButtonGroup->setButton(mWeeklyButtonId);
			mWeekRecurFrequency->setValue(recurrence->frequency());
			QBitArray rDays = recurrence->days();
			setCheckedDays(rDays);
			break;
		}
		case Recurrence::rMonthlyPos:    // on nth (Tuesday) of the month
		{
			ruleButtonGroup->setButton(mMonthlyButtonId);
			mMonthRecurFrequency->setValue(recurrence->frequency());
			mMonthRuleButtonGroup->setButton(mMonthRuleOnNthTypeOfDayButtonId);
			QPtrList<Recurrence::rMonthPos> rmp = recurrence->monthPositions();
			int i = rmp.first()->rPos - 1;
			if (rmp.first()->negative)
				i += 5;
			mMonthRuleNthNumberEntry->setCurrentItem(i);
			for (i = 0;  !rmp.first()->rDays.testBit(i);  ++i) ;
			mMonthRuleNthTypeOfDayEntry->setCurrentItem(dayOfWeek_to_offset(i + 1));
			break;
		}
		case Recurrence::rMonthlyDay:     // on nth day of the month
		{
			ruleButtonGroup->setButton(mMonthlyButtonId);
			mMonthRecurFrequency->setValue(recurrence->frequency());
			mMonthRuleButtonGroup->setButton(mMonthRuleOnNthDayButtonId);
			QPtrList<int> rmd = recurrence->monthDays();
			int i = rmd.first() ? *rmd.first() : 1;
			if (i > 0)
				--i;
			else
				i = 30 - i;
			mMonthRuleNthDayEntry->setCurrentItem(i);
			break;
		}
		case Recurrence::rYearlyMonth:   // in the nth month of the year
		case Recurrence::rYearlyPos:     // on the nth (Tuesday) of a month in the year
		{
			if (rtype == Recurrence::rYearlyMonth)
			{
				ruleButtonGroup->setButton(mYearlyButtonId);
				mYearRecurFrequency->setValue(recurrence->frequency());
				mYearRuleButtonGroup->setButton(mYearRuleDayMonthButtonId);
				int day = event.mainDate().day();
				QPtrList<int> rmd = recurrence->monthDays();
				if (rmd.first())
					day = *rmd.first();
				mYearRuleNthDayEntry->setCurrentItem(day < 0 ? 30 - day : day == 1 && event.recursFeb29() ? 29-1 : day-1);
			}
			else if (rtype == Recurrence::rYearlyPos)
			{
				ruleButtonGroup->setButton(mYearlyButtonId);
				mYearRecurFrequency->setValue(recurrence->frequency());
				mYearRuleButtonGroup->setButton(mYearRuleOnNthTypeOfDayButtonId);
				QPtrList<Recurrence::rMonthPos> rmp = recurrence->yearMonthPositions();
				int i = rmp.first()->rPos - 1;
				if (rmp.first()->negative)
					i += 5;
				mYearRuleNthNumberEntry->setCurrentItem(i);
				for (i = 0;  !rmp.first()->rDays.testBit(i);  ++i) ;
					mYearRuleNthTypeOfDayEntry->setCurrentItem(dayOfWeek_to_offset(i + 1));
			}
			for (int i = 0;  i < 12;  ++i)
				mYearRuleMonthBox[i]->setChecked(false);
			QPtrList<int> rmd = recurrence->yearNums();
			for (int* ii = rmd.first();  ii;  ii = rmd.next())
				mYearRuleMonthBox[*ii - 1]->setChecked(true);
			break;
		}
/*		case Recurrence::rYearlyDay:     // on the nth day of the year
		{
			ruleButtonGroup->setButton(mYearlyButtonId);
			mYearRecurFrequency->setValue(recurrence->frequency());
			mYearRuleButtonGroup->setButton(mYearRuleDayButtonId);
			break;
		}*/
		case Recurrence::rNone:
		default:
			return;
	}

	repeatDuration = event.remainingRecurrences();

	// Get range information
	QDateTime endtime = currStartDateTime;
	if (repeatDuration == -1)
		mNoEndDateButton->setChecked(true);
	else if (repeatDuration)
	{
		mRepeatCountButton->setChecked(true);
		if (event.mainExpired())
		{
			mRepeatCountEntry->setMinValue(0);
			repeatDuration = 0;
		}
		mRepeatCountEntry->setValue(repeatDuration);
	}
	else
	{
		mEndDateButton->setChecked(true);
		endtime = recurrence->endDateTime();
		mEndTimeEdit->setValue(endtime.time().hour()*60 + endtime.time().minute());
	}
	mEndDateEdit->setDate(endtime.date());

	// Get exception information
	mExceptionDates = event.exceptionDates();
	qHeapSort(mExceptionDates);
	mExceptionDateList->clear();
	for (DateList::ConstIterator it = mExceptionDates.begin();  it != mExceptionDates.end();  ++it)
		mExceptionDateList->insertItem(KGlobal::locale()->formatDate(*it));
	enableExceptionButtons();

	rangeTypeClicked();
}

/******************************************************************************
 * Update the specified KAEvent with the entered recurrence data.
 */
void RecurrenceEdit::updateEvent(KAEvent& event)
{
	// Get end date and repeat count, common to all types of recurring events
	QDate  endDate;
	QTime  endTime;
	int    repeatCount;
	if (mNoEndDateButton->isChecked())
		repeatCount = -1;
	else if (mRepeatCountButton->isChecked())
		repeatCount = mRepeatCountEntry->value();
	else
	{
		repeatCount = 0;
		endDate = mEndDateEdit->date();
		endTime = mEndTimeEdit->time();
	}

	// Set up the recurrence according to the type selected
	int frequency = 0;
	QButton* button = ruleButtonGroup->selected();
	event.setRepeatAtLogin(button == mAtLoginButton);
	if (button == mSubDailyButton)
	{
		frequency = mSubDayRecurFrequency->value();
		QDateTime endDateTime(endDate, endTime);
		event.setRecurMinutely(frequency, repeatCount, endDateTime);
	}
	else if (button == mDailyButton)
	{
		frequency = mDayRecurFrequency->value();
		event.setRecurDaily(frequency, repeatCount, endDate);
	}
	else if (button == mWeeklyButton)
	{
		frequency = mWeekRecurFrequency->value();
		QBitArray rDays(7);
		getCheckedDays(rDays);
		event.setRecurWeekly(frequency, rDays, repeatCount, endDate);
	}
	else if (button == mMonthlyButton)
	{
		frequency = mMonthRecurFrequency->value();
		if (mMonthRuleOnNthTypeOfDayButton->isChecked())
		{
			// it's by position
			KAEvent::MonthPos pos;
			pos.days.fill(false);
			pos.days.setBit(offset_to_dayOfWeek(mMonthRuleNthTypeOfDayEntry->currentItem()) - 1);
			int i = mMonthRuleNthNumberEntry->currentItem() + 1;
			pos.weeknum = (i <= 5) ? i : 5 - i;
			QValueList<KAEvent::MonthPos> poses;
			poses.append(pos);
			event.setRecurMonthlyByPos(frequency, poses, repeatCount, endDate);
			event.setFirstRecurrence();
		}
		else
		{
			// it's by day
			short daynum  = mMonthRuleNthDayEntry->currentItem() + 1;
			if (daynum > 31)
				daynum = 31 - daynum;
			QValueList<int> daynums;
			daynums.append(daynum);
			event.setRecurMonthlyByDate(frequency, daynums, repeatCount, endDate);
		}
	}
	else if (button == mYearlyButton)
	{
		frequency = mYearRecurFrequency->value();
		QValueList<int> months;
		bool feb = getCheckedMonths(months);

		if (mYearRuleOnNthTypeOfDayButton->isChecked())
		{
			// it's by position
			KAEvent::MonthPos pos;
			pos.days.fill(false);
			pos.days.setBit(offset_to_dayOfWeek(mYearRuleNthTypeOfDayEntry->currentItem()) - 1);
			int i = mYearRuleNthNumberEntry->currentItem() + 1;
			pos.weeknum = (i <= 5) ? i : 5 - i;
			QValueList<KAEvent::MonthPos> poses;
			poses.append(pos);
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
			short daynum  = mYearRuleNthDayEntry->currentItem() + 1;
			if (daynum > 31)
				daynum = 31 - daynum;
			bool feb29 = (daynum == 29) && feb;
			DateTime start = event.mainDateTime();
			QDate d = start.date();
			int startday = d.day();
			event.setRecurAnnualByDate(frequency, months, (startday != daynum ? daynum : 0),
			                           feb29, repeatCount, endDate);
			if (daynum > 0)
			{
				bool adjust = false;
				if (startday != daynum)
				{
					// The day of the month for the recurrence is different from the
					// event start day, so adjust the event start to be the first
					// recurrence after the preset event start.
					if (daynum > startday)
						do
						{
							d = d.addDays(-startday);    // last day of previous month
						} while (d.day() < daynum);
					d = d.addDays(daynum - d.day());
					event.adjustStartDate(d);
					adjust = true;
				}
				else
				{
					// The day of the month of the recurrence is the same as the event
					// start day, so check that the start month is one of the selected
					// recurrence months.
					adjust = true;
					int month = d.month();
					for (QValueList<int>::ConstIterator it = months.begin();  it != months.end();  ++it)
					{
						if (*it == month)
						{
							adjust = false;
							break;
						}
					}
				}
				if (adjust)
				{
					DateTime newTime;
					KAEvent::OccurType type = event.nextOccurrence(QDateTime(d, QTime(23,59,59)), newTime);
					if (type != KAEvent::FIRST_OCCURRENCE  &&  type != KAEvent::NO_OCCURRENCE)
						event.adjustStartDate(newTime.date());
				}
			}
			else
			{
				// The recurrence is on the last day of the month, so adjust the
				// event start to the first recurrence date.
				// First adjust to a month which is supposed to recur.
				bool adjust = true;
				int month = d.month();
				for (QValueList<int>::ConstIterator it = months.begin();  it != months.end();  ++it)
				{
					if (*it == month)
					{
						adjust = false;
						break;
					}
				}
				if (adjust)
				{
					DateTime newTime;
					KAEvent::OccurType type = event.nextOccurrence(QDateTime(d), newTime);
					if (type != KAEvent::FIRST_OCCURRENCE  &&  type != KAEvent::NO_OCCURRENCE)
						event.adjustStartDate(newTime.date());
					d = start.date();
					startday = d.day();
				}
				int lastDay = d.daysInMonth();
				if (startday != lastDay)
					event.adjustStartDate(d.addDays(lastDay - startday));
			}
		}
	}
	else
	{
		event.setNoRecur();
		return;
	}

	// Set up exceptions
	event.setExceptionDates(mExceptionDates);
}

/*=============================================================================
= Class RecurFrequency
= Recurrence frequency widget.
=============================================================================*/

RecurFrequency::RecurFrequency(bool time, const QString& text, const QString& whatsThis,
                               bool readOnly, QWidget* parent, const char* name)
	: QHBox(parent, name)
{
	QHBox* box = new QHBox(this);    // this is to control the QWhatsThis text display area
	box->setSpacing(KDialog::spacingHint());

	QLabel* label = new QLabel(i18n("Recur e&very"), box);
	label->setFixedSize(label->sizeHint());
	if (time)
	{
		mIntSpinBox = 0;
		mSpinBox = mTimeSpinBox = new TimeSpinBox(1, 5999, box);
		mTimeSpinBox->setFixedSize(mTimeSpinBox->sizeHint());
		mTimeSpinBox->setReadOnly(readOnly);
	}
	else
	{
		mTimeSpinBox = 0;
		mSpinBox = mIntSpinBox = new SpinBox(1, 999, 1, box);
		mIntSpinBox->setFixedSize(mIntSpinBox->sizeHint());
		mIntSpinBox->setReadOnly(readOnly);
	}
	connect(mSpinBox, SIGNAL(valueChanged(int)), SIGNAL(valueChanged()));
	label->setBuddy(mSpinBox);
	label = new QLabel(text, box);
	label->setFixedSize(label->sizeHint());
	QWhatsThis::add(this, whatsThis);

	box->setFixedSize(sizeHint());
	new QWidget(this);     // left adjust the visible widgets
	setFixedHeight(sizeHint().height());
	setFocusProxy(mSpinBox);
}

int RecurFrequency::value() const
{
	if (mIntSpinBox)
		return mIntSpinBox->value();
	if (mTimeSpinBox)
		return mTimeSpinBox->value();
	return 0;
}

void RecurFrequency::setValue(int n)
{
	if (mIntSpinBox)
		mIntSpinBox->setValue(n);
	if (mTimeSpinBox)
		mTimeSpinBox->setValue(n);
}
