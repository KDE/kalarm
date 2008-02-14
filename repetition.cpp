/*
 *  repetition.cpp  -  pushbutton and dialogue to specify alarm sub-repetition
 *  Program:  kalarm
 *  Copyright © 2004,2005,2007,2008 by David Jarvie <djarvie@kde.org>
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

#include <qlayout.h>
#include <qwhatsthis.h>

#include <kdebug.h>
#include <kdialog.h>
#include <klocale.h>

#include "buttongroup.h"
#include "radiobutton.h"
#include "spinbox.h"
#include "timeperiod.h"
#include "timeselector.h"
#include "repetition.moc"


/*=============================================================================
= Class RepetitionButton
= Button to display the Alarm Sub-Repetition dialogue.
=============================================================================*/

RepetitionButton::RepetitionButton(const QString& caption, bool waitForInitialisation, QWidget* parent, const char* name)
	: QPushButton(caption, parent, name),
	  mDialog(0),
	  mInterval(0),
	  mCount(0),
	  mMaxDuration(-1),
	  mDateOnly(false),
	  mWaitForInit(waitForInitialisation),
	  mReadOnly(false)
{
	setToggleButton(true);
	setOn(false);
	connect(this, SIGNAL(clicked()), SLOT(slotPressed()));
}

void RepetitionButton::set(int interval, int count)
{
	mInterval = interval;
	mCount = count;
	setOn(mInterval && mCount);
}

/******************************************************************************
*  Set the data for the dialog.
*/
void RepetitionButton::set(int interval, int count, bool dateOnly, int maxDuration)
{
	mInterval    = interval;
	mCount       = count;
	mMaxDuration = maxDuration;
	mDateOnly    = dateOnly;
	setOn(mInterval && mCount);
}

/******************************************************************************
*  Create the alarm repetition dialog.
*  If 'waitForInitialisation' is true, the dialog won't be displayed until set()
*  is called to initialise its data.
*/
void RepetitionButton::activate(bool waitForInitialisation)
{
	if (!mDialog)
		mDialog = new RepetitionDlg(i18n("Alarm Sub-Repetition"), mReadOnly, this);
	mDialog->set(mInterval, mCount, mDateOnly, mMaxDuration);
	if (waitForInitialisation)
		emit needsInitialisation();     // request dialog initialisation
	else
		displayDialog();    // display the dialog now
}

/******************************************************************************
*  Set the data for the dialog and display it.
*  To be called only after needsInitialisation() has been emitted.
*/
void RepetitionButton::initialise(int interval, int count, bool dateOnly, int maxDuration)
{
	if (maxDuration > 0 && interval > maxDuration)
		count = 0;
	mCount       = count;
	mInterval    = interval;
	mMaxDuration = maxDuration;
	mDateOnly    = dateOnly;
	if (mDialog)
	{
		mDialog->set(interval, count, dateOnly, maxDuration);
		displayDialog();    // display the dialog now
	}
	else
		setOn(mInterval && mCount);
}

/******************************************************************************
*  Display the alarm sub-repetition dialog.
*  Alarm repetition has the following restrictions:
*  1) Not allowed for a repeat-at-login alarm
*  2) For a date-only alarm, the repeat interval must be a whole number of days.
*  3) The overall repeat duration must be less than the recurrence interval.
*/
void RepetitionButton::displayDialog()
{
	kdDebug(5950) << "RepetitionButton::displayDialog()" << endl;
	bool change = false;
	if (mReadOnly)
	{
		mDialog->setReadOnly(true);
		mDialog->exec();
	}
	else if (mDialog->exec() == QDialog::Accepted)
	{
		mCount    = mDialog->count();
		mInterval = mDialog->interval();
		change = true;
	}
	setOn(mInterval && mCount);
	delete mDialog;
	mDialog = 0;
	if (change)
		emit changed();   // delete dialog first, or initialise() will redisplay dialog
}


/*=============================================================================
= Class RepetitionDlg
= Alarm sub-repetition dialogue.
=============================================================================*/

static const int MAX_COUNT = 9999;    // maximum range for count spinbox


RepetitionDlg::RepetitionDlg(const QString& caption, bool readOnly, QWidget* parent, const char* name)
	: KDialogBase(parent, name, true, caption, Ok|Cancel),
	  mMaxDuration(-1),
	  mDateOnly(false),
	  mReadOnly(readOnly)
{
	int spacing = spacingHint();
	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* topLayout = new QVBoxLayout(page, 0, spacing);

	mTimeSelector = new TimeSelector(i18n("Repeat every 10 minutes", "&Repeat every"), QString::null,
	                  i18n("Instead of the alarm triggering just once at each recurrence, "
	                       "checking this option makes the alarm trigger multiple times at each recurrence."),
	                  i18n("Enter the time between repetitions of the alarm"),
	                  true, page);
	mTimeSelector->setFixedSize(mTimeSelector->sizeHint());
	connect(mTimeSelector, SIGNAL(valueChanged(int)), SLOT(intervalChanged(int)));
	connect(mTimeSelector, SIGNAL(toggled(bool)), SLOT(repetitionToggled(bool)));
	topLayout->addWidget(mTimeSelector, 0, Qt::AlignAuto);

	mButtonGroup = new ButtonGroup(page, "buttonGroup");
	connect(mButtonGroup, SIGNAL(buttonSet(int)), SLOT(typeClicked()));
	topLayout->addWidget(mButtonGroup);

	QBoxLayout* vlayout = new QVBoxLayout(mButtonGroup, marginHint(), spacing);
	QBoxLayout* layout = new QHBoxLayout(vlayout, spacing);
	mCountButton = new RadioButton(i18n("&Number of repetitions:"), mButtonGroup);
	mCountButton->setFixedSize(mCountButton->sizeHint());
	QWhatsThis::add(mCountButton,
	      i18n("Check to specify the number of times the alarm should repeat after each recurrence"));
	layout->addWidget(mCountButton);
	mCount = new SpinBox(1, MAX_COUNT, 1, mButtonGroup);
	mCount->setFixedSize(mCount->sizeHint());
	mCount->setLineShiftStep(10);
	mCount->setSelectOnStep(false);
	connect(mCount, SIGNAL(valueChanged(int)), SLOT(countChanged(int)));
	QWhatsThis::add(mCount,
	      i18n("Enter the number of times to trigger the alarm after its initial occurrence"));
	layout->addWidget(mCount);
	mCountButton->setFocusWidget(mCount);
	layout->addStretch();

	layout = new QHBoxLayout(vlayout, spacing);
	mDurationButton = new RadioButton(i18n("&Duration:"), mButtonGroup);
	mDurationButton->setFixedSize(mDurationButton->sizeHint());
	QWhatsThis::add(mDurationButton,
	      i18n("Check to specify how long the alarm is to be repeated"));
	layout->addWidget(mDurationButton);
	mDuration = new TimePeriod(true, mButtonGroup);
	mDuration->setFixedSize(mDuration->sizeHint());
	connect(mDuration, SIGNAL(valueChanged(int)), SLOT(durationChanged(int)));
	QWhatsThis::add(mDuration,
	      i18n("Enter the length of time to repeat the alarm"));
	layout->addWidget(mDuration);
	mDurationButton->setFocusWidget(mDuration);
	layout->addStretch();

	mCountButton->setChecked(true);
	repetitionToggled(false);
	setReadOnly(mReadOnly);
}

/******************************************************************************
*  Set the state of all controls to reflect the data in the specified alarm.
*/
void RepetitionDlg::set(int interval, int count, bool dateOnly, int maxDuration)
{
	if (!interval)
		count = 0;
	else if (!count)
		interval = 0;
	if (dateOnly != mDateOnly)
	{
		mDateOnly = dateOnly;
		mTimeSelector->setDateOnly(mDateOnly);
		mDuration->setDateOnly(mDateOnly);
	}
	mMaxDuration = maxDuration;
	if (mMaxDuration)
	{
		int maxhm = (mMaxDuration > 0) ? mMaxDuration : 9999;
		int maxdw = (mMaxDuration > 0) ? mMaxDuration / 1440 : 9999;
		mTimeSelector->setMaximum(maxhm, maxdw);
		mDuration->setMaximum(maxhm, maxdw);
	}
	// Set the units - needed later if the control is unchecked initially.
	TimePeriod::Units units = mDateOnly ? TimePeriod::DAYS : TimePeriod::HOURS_MINUTES;
	mTimeSelector->setMinutes(interval, mDateOnly, units);
	if (!mMaxDuration  ||  !count)
		mTimeSelector->setChecked(false);
	else
	{
		bool on = mTimeSelector->isChecked();
		repetitionToggled(on);    // enable/disable controls
		if (on)
			intervalChanged(interval);    // ensure mCount range is set
		mCount->setValue(count);
		mDuration->setMinutes(count * interval, mDateOnly, units);
		mCountButton->setChecked(true);
	}
	mTimeSelector->setEnabled(mMaxDuration);
}

/******************************************************************************
*  Set the read-only status.
*/
void RepetitionDlg::setReadOnly(bool ro)
{
	ro = ro || mReadOnly;
	mTimeSelector->setReadOnly(ro);
	mCountButton->setReadOnly(ro);
	mCount->setReadOnly(ro);
	mDurationButton->setReadOnly(ro);
	mDuration->setReadOnly(ro);
}

/******************************************************************************
*  Get the period between repetitions in minutes.
*/
int RepetitionDlg::interval() const
{
	return mTimeSelector->minutes();
}

/******************************************************************************
*  Set the entered repeat count.
*/
int RepetitionDlg::count() const
{
	int interval = mTimeSelector->minutes();
	if (interval)
	{
		if (mCountButton->isOn())
			return mCount->value();
		if (mDurationButton->isOn())
			return mDuration->minutes() / interval;
	}
	return 0;    // no repetition
}

/******************************************************************************
*  Called when the time interval widget has changed value.
*  Adjust the maximum repetition count accordingly.
*/
void RepetitionDlg::intervalChanged(int minutes)
{
	if (mTimeSelector->isChecked()  &&  minutes > 0)
	{
		mCount->setRange(1, (mMaxDuration >= 0 ? mMaxDuration / minutes : MAX_COUNT));
		if (mCountButton->isOn())
			countChanged(mCount->value());
		else
			durationChanged(mDuration->minutes());
	}
}

/******************************************************************************
*  Called when the count spinbox has changed value.
*  Adjust the duration accordingly.
*/
void RepetitionDlg::countChanged(int count)
{
	int interval = mTimeSelector->minutes();
	if (interval)
	{
		bool blocked = mDuration->signalsBlocked();
		mDuration->blockSignals(true);
		mDuration->setMinutes(count * interval, mDateOnly,
		                      (mDateOnly ? TimePeriod::DAYS : TimePeriod::HOURS_MINUTES));
		mDuration->blockSignals(blocked);
	}
}

/******************************************************************************
*  Called when the duration widget has changed value.
*  Adjust the count accordingly.
*/
void RepetitionDlg::durationChanged(int minutes)
{
	int interval = mTimeSelector->minutes();
	if (interval)
	{
		bool blocked = mCount->signalsBlocked();
		mCount->blockSignals(true);
		mCount->setValue(minutes / interval);
		mCount->blockSignals(blocked);
	}
}

/******************************************************************************
*  Called when the time period widget is toggled on or off.
*/
void RepetitionDlg::repetitionToggled(bool on)
{
	if (mMaxDuration == 0)
		on = false;
	mButtonGroup->setEnabled(on);
	mCount->setEnabled(on  &&  mCountButton->isOn());
	mDuration->setEnabled(on  &&  mDurationButton->isOn());
}

/******************************************************************************
*  Called when one of the count or duration radio buttons is toggled.
*/
void RepetitionDlg::typeClicked()
{
	if (mTimeSelector->isChecked())
	{
		mCount->setEnabled(mCountButton->isOn());
		mDuration->setEnabled(mDurationButton->isOn());
	}
}
