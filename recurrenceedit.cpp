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
#include <kdebug.h>

#include <libkcal/event.h>

#include "datetime.h"
#include "msgevent.h"
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


RecurrenceEdit::RecurrenceEdit(const QString& groupBoxTitle, QWidget* parent, const char* name)
   : QWidget(parent, name, 0),
     noEmitTypeChanged(true)
{
   // Create the recurrence rule Group box which holds the recurrence period
   // selection buttons, and the weekly, monthly and yearly recurrence rule
   // frames which specify options individual to each of these distinct
   // sections of the recurrence rule. Each frame is made visible by the
   // selection of its corresponding radio button.

   QBoxLayout* layout = new QVBoxLayout(this);
   ruleGroupBox = new QGroupBox(1, Qt::Horizontal, groupBoxTitle, this, "recurGroupBox");
   layout->addWidget(ruleGroupBox);

   QFrame* topFrame = new QFrame(ruleGroupBox,"repeatFrame");
   QBoxLayout* topLayout = new QVBoxLayout(topFrame, 0, KDialog::spacingHint());

   // Create the repetition type radio buttons

   repeatButtonGroup = new ButtonGroup(1, Vertical, topFrame);
   repeatButtonGroup->setFrameStyle(QFrame::NoFrame);
   repeatButtonGroup->setInsideMargin(0);
   topLayout->addWidget(repeatButtonGroup);
   connect(repeatButtonGroup, SIGNAL(clicked(int)), this, SLOT(repeatTypeClicked(int)));

   noneRadio = new QRadioButton(i18n("No repetition"), repeatButtonGroup);
   noneRadio->setFixedSize(noneRadio->sizeHint());
   QWhatsThis::add(noneRadio,
         i18n("Trigger the alarm once only"));

   repeatAtLoginRadio = new QRadioButton(i18n("Repeat at login"), repeatButtonGroup, "repeatAtLoginButton");
   repeatAtLoginRadio->setFixedSize(repeatAtLoginRadio->sizeHint());
   QWhatsThis::add(repeatAtLoginRadio,
         i18n("Repeat the alarm at every login until the specified time.\n"
              "Note that it will also be repeated any time the alarm daemon is restarted."));

   recurRadio = new QRadioButton(i18n("Recur"), repeatButtonGroup);
   recurRadio->setFixedSize(recurRadio->sizeHint());
   QWhatsThis::add(recurRadio, i18n("Regularly repeat the alarm"));

   // Create the Recurrence Rule definitions

   recurGroup = new QGroupBox(1, Qt::Vertical, i18n("Recurrence Rule"), topFrame, "recurGroup");
   topLayout->addWidget(recurGroup);
   ruleFrame = new QFrame(recurGroup, "ruleFrame");
   layout = new QVBoxLayout(ruleFrame, 0);
   layout->addSpacing(KDialog::spacingHint()/2);

   layout = new QHBoxLayout(layout, 0);
   ruleButtonGroup = new ButtonGroup(1, Horizontal, ruleFrame);
   ruleButtonGroup->setFrameStyle(QFrame::NoFrame);
   ruleButtonGroup->setInsideMargin(0);
   layout->addWidget(ruleButtonGroup);
   connect(ruleButtonGroup, SIGNAL(clicked(int)), this, SLOT(periodClicked(int)));

   QLabel* label       = new QLabel(i18n("Recur every:"), ruleButtonGroup);
   label->setFixedSize(label->sizeHint());
   recurFrequencyStack = new QWidgetStack(ruleButtonGroup);
   recurFrequency      = new QSpinBox(1, 999, 1, ruleButtonGroup);
   recurFrequencyStack->addWidget(recurFrequency, 0);
   recurHourMinFrequency = new TimeSpinBox(1, 99*60+59, ruleButtonGroup);
   QWhatsThis::add(recurHourMinFrequency,
         i18n("Enter the time (in hours and minutes) between repetitions of the alarm."));
   recurFrequencyStack->addWidget(recurHourMinFrequency, 1);
   QSize size = recurFrequency->sizeHint().expandedTo(recurHourMinFrequency->sizeHint());
   recurFrequency->setFixedSize(size);
   recurHourMinFrequency->setFixedSize(size);
   recurFrequencyStack->setFixedSize(size);

   subdailyButton = new QRadioButton(i18n("Hours/Minutes"), ruleButtonGroup);
   subdailyButton->setFixedSize(subdailyButton->sizeHint());
   QWhatsThis::add(subdailyButton,
         i18n("Set the alarm repetition interval to the number of hours and minutes entered"));
   dailyButton    = new QRadioButton(i18n("Days"), ruleButtonGroup);
   dailyButton->setFixedSize(dailyButton->sizeHint());
   QWhatsThis::add(dailyButton,
         i18n("Set the alarm repetition interval to the number of days entered"));
   weeklyButton   = new QRadioButton(i18n("Weeks"), ruleButtonGroup);
   weeklyButton->setFixedSize(weeklyButton->sizeHint());
   QWhatsThis::add(weeklyButton,
         i18n("Set the alarm repetition interval to the number of weeks entered"));
   monthlyButton  = new QRadioButton(i18n("Months"), ruleButtonGroup);
   monthlyButton->setFixedSize(monthlyButton->sizeHint());
   QWhatsThis::add(monthlyButton,
         i18n("Set the alarm repetition interval to the number of months entered"));
   yearlyButton   = new QRadioButton(i18n("Years"), ruleButtonGroup);
   yearlyButton->setFixedSize(yearlyButton->sizeHint());
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

   rangeButtonGroup = new QButtonGroup(i18n("Recurrence End"), topFrame, "rangeButtonGroup");
   topLayout->addWidget(rangeButtonGroup);

   QVBoxLayout* vlayout = new QVBoxLayout(rangeButtonGroup, KDialog::marginHint(), KDialog::spacingHint());
   vlayout->addSpacing(KDialog::spacingHint()*3/2);
   noEndDateButton = new QRadioButton(i18n("No end"), rangeButtonGroup);
   noEndDateButton->setFixedSize(noEndDateButton->sizeHint());
   QWhatsThis::add(noEndDateButton, i18n("Repeat the alarm indefinitely"));
   vlayout->addWidget(noEndDateButton, 1, 0);
   size = noEndDateButton->size();

   layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
   repeatCountButton = new QRadioButton(i18n("End after:"), rangeButtonGroup);
   QWhatsThis::add(repeatCountButton,
         i18n("Repeat the alarm for the number of time intervals specified"));
   repeatCountEntry  = new QSpinBox(1, 9999, 1, rangeButtonGroup);
   repeatCountEntry->setFixedSize(repeatCountEntry->sizeHint());
   QWhatsThis::add(repeatCountEntry,
         i18n("Enter the number of time intervals over which to repeat the alarm"));
   repeatCountLabel  = new QLabel(i18n("interval(s)"), rangeButtonGroup);
   repeatCountLabel->setFixedSize(repeatCountLabel->sizeHint());
   layout->addWidget(repeatCountButton);
   layout->addSpacing(KDialog::spacingHint());
   layout->addWidget(repeatCountEntry);
   layout->addWidget(repeatCountLabel);
   layout->addStretch();
   size = size.expandedTo(repeatCountButton->sizeHint());

   layout = new QHBoxLayout(vlayout, KDialog::spacingHint());
   endDateButton = new QRadioButton(i18n("End by:"), rangeButtonGroup);
   QWhatsThis::add(endDateButton,
         i18n("Repeat the alarm until the date/time specified"));
   endDateEdit   = new DateSpinBox(rangeButtonGroup);
   endDateEdit->setFixedSize(endDateEdit->sizeHint());
   QWhatsThis::add(endDateEdit,
         i18n("Enter the last date to repeat the alarm"));
   endTimeEdit   = new TimeSpinBox(rangeButtonGroup);
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

   connect(noEndDateButton, SIGNAL(toggled(bool)), this, SLOT(disableRange(bool)));
   connect(repeatCountButton, SIGNAL(toggled(bool)), this, SLOT(enableDurationRange(bool)));
   connect(endDateButton, SIGNAL(toggled(bool)), this, SLOT(enableDateRange(bool)));

   noEmitTypeChanged = false;
}

/******************************************************************************
 * Called when a repetition type radio button is clicked.
 */
void RecurrenceEdit::repeatTypeClicked(int id)
{
   bool recur = false;
   RepeatType repeatType;
   QButton* button = repeatButtonGroup->find(id);
   if (button == noneRadio)
      repeatType = NONE;
   else if (button == repeatAtLoginRadio)
      repeatType = AT_LOGIN;
   else if (button == recurRadio)
   {
      recur = true;
      repeatType = ruleButtonType;
   }
   else
      return;

   recurGroup->setEnabled(recur);
   rangeButtonGroup->setEnabled(recur);
   if (!noEmitTypeChanged)
      emit typeChanged(repeatType);
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
      recurFrequencyStack->raiseWidget(recurHourMinFrequency);
   else
      recurFrequencyStack->raiseWidget(recurFrequency);
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

void RecurrenceEdit::initNone()
{
   noneFrame = new QFrame(ruleFrame);
   noneFrame->setFrameStyle(QFrame::NoFrame);
}

void RecurrenceEdit::initWeekly()
{
   weeklyFrame = new QFrame(ruleFrame);
   weeklyFrame->setFrameStyle(QFrame::NoFrame);
   QBoxLayout* topLayout = new QVBoxLayout(weeklyFrame);
   topLayout->addStretch();
   QGridLayout* layout = new QGridLayout(topLayout, 7, 2, ruleButtonGroup->insideSpacing());
   layout->setRowStretch(0, 1);

   QLabel* label = new QLabel(i18n("Recur on:"), weeklyFrame);
   label->setFixedSize(label->sizeHint());
   layout->addWidget(label, 0, 0);
   for (int i = 0;  i < 7;  ++i)
   {
      dayBox[i] = new QCheckBox(KGlobal::locale()->weekDayName(i + 1), weeklyFrame);    // starts Monday
      dayBox[i]->setFixedSize(dayBox[i]->sizeHint());
      QWhatsThis::add(dayBox[i],
            i18n("Select the day(s) of the week on which to repeat the alarm"));
      layout->addWidget(dayBox[i], i, 1);
   }
   layout->setColStretch(1, 1);
}

void RecurrenceEdit::initMonthly()
{
   int i;

   monthlyFrame = new QVBox(ruleFrame);
   monthlyFrame->setFrameStyle(QFrame::NoFrame);

   monthlyButtonGroup = new ButtonGroup(monthlyFrame);
   monthlyButtonGroup->setFrameStyle(QFrame::NoFrame);
   QBoxLayout* topLayout = new QVBoxLayout(monthlyButtonGroup, KDialog::marginHint());

   QBoxLayout* layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
   onNthDayButton = new QRadioButton(i18n("Recur on the"), monthlyButtonGroup);
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
   onNthTypeOfDayButton = new QRadioButton(i18n("Recur on the"), monthlyButtonGroup);
   onNthTypeOfDayButton->setFixedSize(onNthTypeOfDayButton->sizeHint());
   QWhatsThis::add(onNthTypeOfDayButton,
         i18n("Repeat the alarm on one day of the week, in the selected week of the month"));
   layout->addWidget(onNthTypeOfDayButton);
   nthNumberEntry = new QComboBox(false, monthlyButtonGroup);
   for (i = 1;  i <= 5;  ++i)
      nthNumberEntry->insertItem(ordinal[i]);
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

   connect(monthlyButtonGroup, SIGNAL(clicked(int)), this, SLOT(monthlyClicked(int)));
}

void RecurrenceEdit::initYearly()
{
   int i;

   yearlyFrame = new QVBox(ruleFrame);
   yearlyFrame->setFrameStyle(QFrame::NoFrame);

   yearlyButtonGroup = new ButtonGroup(yearlyFrame);
   yearlyButtonGroup->setFrameStyle(QFrame::NoFrame);
   QBoxLayout* topLayout = new QVBoxLayout(yearlyButtonGroup, KDialog::marginHint());

   QBoxLayout* layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
   yearMonthButton = new QRadioButton(i18n("Recur on"), yearlyButtonGroup);
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

   layout = new QHBoxLayout(topLayout, KDialog::spacingHint());
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
   layout->addStretch();

   yearMonthButtonId = yearlyButtonGroup->id(yearMonthButton);
   yearDayButtonId   = yearlyButtonGroup->id(yearDayButton);

   connect(yearlyButtonGroup, SIGNAL(clicked(int)), this, SLOT(yearlyClicked(int)));
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
   else if (id == yearDayButtonId)
      date = false;
   else
      return;

   yearMonthDayEntry->setEnabled(date);
   yearMonthComboBox->setEnabled(date);
   yearDayEntry->setEnabled(!date);
}

void RecurrenceEdit::unsetAllCheckboxes()
{
   onNthDayButton->setChecked(false);
   onNthTypeOfDayButton->setChecked(false);
   yearMonthButton->setChecked(false);
   yearDayButton->setChecked(false);

   for (int i = 0;  i < 7;  ++i)
      dayBox[i]->setChecked(false);

   endDateButton->setChecked(false);
   noEndDateButton->setChecked(false);
   repeatCountButton->setChecked(false);
}


void RecurrenceEdit::setDefaults(const QDateTime& from, bool)
{
   // unset everything
   unsetAllCheckboxes();

   currStartDateTime = from;
   setDefaults(from);
}

void RecurrenceEdit::setDefaults(const QDateTime& from)
{
   QDate fromDate = from.date();

   noEmitTypeChanged = true;
   ruleButtonGroup->setButton(dailyButtonId);   // this must come before repeatButtonGroup->setButton()
   noEmitTypeChanged = false;
   repeatButtonGroup->setButton(repeatButtonGroup->id(noneRadio));
   noEndDateButton->setChecked(true);

   recurHourMinFrequency->setValue(1);
   recurFrequency->setValue(1);

   checkDay(fromDate.dayOfWeek());
   monthlyButtonGroup->setButton(onNthDayButtonId);    // date in month
   nthDayEntry->setCurrentItem(fromDate.day() - 1);
   nthNumberEntry->setCurrentItem((fromDate.day() - 1) / 7);
   nthTypeOfDayEntry->setCurrentItem(fromDate.dayOfWeek() - 1);
   yearlyButtonGroup->setButton(yearMonthButtonId);     // date in year
   yearMonthDayEntry->setCurrentItem(fromDate.day() - 1);
   yearMonthComboBox->setCurrentItem(fromDate.month() - 1);
   yearDayEntry->setValue(fromDate.dayOfYear());
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
void RecurrenceEdit::set(const KAlarmEvent& event, bool repeatatlogin)
{
   // unset everything
   unsetAllCheckboxes();
   currStartDateTime = event.dateTime();
   if (event.repeatAtLogin())
      repeatButtonGroup->setButton(repeatButtonGroup->id(repeatAtLoginRadio));
   else if (event.repeats())
   {
      noEmitTypeChanged = true;
      repeatButtonGroup->setButton(repeatButtonGroup->id(recurRadio));
      noEmitTypeChanged = false;
      int repeatDuration;
      Recurrence* recurrence = event.recurrence();
      if (recurrence)
      {
         switch (recurrence->doesRecur())
         {
            case Recurrence::rDaily:
            {
               ruleButtonGroup->setButton(dailyButtonId);
               break;
            }
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
                  i = 3 - i;
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
            case Recurrence::rYearlyDay:     // on the nth day of the year
            {
               ruleButtonGroup->setButton(yearlyButtonId);
               yearlyButtonGroup->setButton(yearDayButtonId);
               break;
            }
            case Recurrence::rNone:
            default:
               setDefaults(currStartDateTime);
               return;
         }

         recurFrequency->setValue(recurrence->frequency());
         repeatDuration = recurrence->duration();
      }
      else
      {
         ruleButtonGroup->setButton(subdailyButtonId);
         recurHourMinFrequency->setValue(event.repeatMinutes());
         repeatDuration = event.repeatCount();
      }

      // get range information
      if (repeatDuration == -1)
         noEndDateButton->setChecked(true);
      else if (repeatDuration)
      {
         repeatCountButton->setChecked(true);
         repeatCountEntry->setValue(repeatDuration - 1);
      }
      else
      {
         endDateButton->setChecked(true);
         endDateEdit->setDate(recurrence->endDate());
      }
   }
   else
   {
      // The event doesn't repeat. Set up defaults
      setDefaults(currStartDateTime);
   }
}

/******************************************************************************
 * Update the specified KAlarmEvent with the entered recurrence data.
 */
void RecurrenceEdit::writeEvent(KAlarmEvent& event)
{
   if (recurRadio->isChecked())
   {
      // Get end date and repeat count, common to all types of recurring events
      QDate  endDate;
      QTime  endTime;
      int    repeatCount;
      if (noEndDateButton->isChecked())
         repeatCount = -1;
      else if (repeatCountButton->isChecked())
         repeatCount = repeatCountEntry->value() + 1;
      else
      {
         repeatCount = 0;
         endDate = endDateEdit->getDate();
         endTime = endTimeEdit->getTime();
      }

      // Set up the recurrence according to the type selected
      QButton* button = ruleButtonGroup->selected();
      if (button == subdailyButton)
      {
         QDateTime endDateTime(endDate, endTime);
         int frequency = recurHourMinFrequency->value();
         event.setRecurSubDaily(frequency, repeatCount, endDateTime);
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
            pos.weeknum = nthNumberEntry->currentItem() + 1;
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
            // it's by day
            int daynum = event.date().dayOfYear();
            QValueList<int> daynums;
            daynums.append(daynum);
            event.setRecurAnnualByDay(frequency, daynums, repeatCount, endDate);
         }
      }
   }
   else
      event.initRecur(false);
}


bool RecurrenceEdit::validateInput()
{
   // Check input here

   return true;
}
