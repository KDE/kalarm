/*
 *  repetition.cpp  -  pushbutton and dialog to specify alarm repetition
 *  Program:  kalarm
 *  Copyright © 2004-2008 by David Jarvie <djarvie@kde.org>
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
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <kdialog.h>
#include <klocale.h>

#include "buttongroup.h"
#include "radiobutton.h"
#include "spinbox.h"
#include "timeperiod.h"
#include "timeselector.h"
#include "repetition.moc"

using namespace KCal;


/*=============================================================================
= Class RepetitionButton
= Button to display the Simple Alarm Repetition dialog.
=============================================================================*/

RepetitionButton::RepetitionButton(const QString& caption, bool waitForInitialisation, QWidget* parent)
	: QPushButton(caption, parent),
	  mDialog(0),
	  mInterval(0),
	  mCount(0),
	  mMaxDuration(-1),
	  mDateOnly(false),
	  mWaitForInit(waitForInitialisation),
	  mReadOnly(false)
{
	setCheckable(true);
	setChecked(false);
	connect(this, SIGNAL(clicked()), SLOT(slotPressed()));
}

void RepetitionButton::set(const Duration& interval, int count)
{
	mInterval = interval;
	mCount    = count;
	setChecked(mInterval && mCount);
}

/******************************************************************************
*  Set the data for the dialog.
*/
void RepetitionButton::set(const Duration& interval, int count, bool dateOnly, int maxDuration)
{
	mInterval    = interval;
	mCount       = count;
	mMaxDuration = maxDuration;
	mDateOnly    = dateOnly;
	setChecked(mInterval && mCount);
}

/******************************************************************************
*  Create the alarm repetition dialog.
*  If 'waitForInitialisation' is true, the dialog won't be displayed until set()
*  is called to initialise its data.
*/
void RepetitionButton::activate(bool waitForInitialisation)
{
	if (!mDialog)
		mDialog = new RepetitionDlg(i18nc("@title:window", "Alarm Sub-Repetition"), mReadOnly, this);
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
void RepetitionButton::initialise(const Duration& interval, int count, bool dateOnly, int maxDuration)
{
	mInterval    = (maxDuration > 0 && interval.asSeconds()/60 > maxDuration) ? Duration(60) : interval;
	mCount       = count;
	mMaxDuration = maxDuration;
	mDateOnly    = dateOnly;
	if (mDialog)
	{
		mDialog->set(interval, count, dateOnly, maxDuration);
		displayDialog();    // display the dialog now
	}
	else
		setChecked(mInterval && mCount);
}

/******************************************************************************
*  Display the simple alarm repetition dialog.
*  Alarm repetition has the following restrictions:
*  1) Not allowed for a repeat-at-login alarm
*  2) For a date-only alarm, the repeat interval must be a whole number of days.
*  3) The overall repeat duration must be less than the recurrence interval.
*/
void RepetitionButton::displayDialog()
{
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
	setChecked(mInterval && mCount);
	delete mDialog;
	mDialog = 0;
	if (change)
		emit changed();   // delete dialog first, or initialise() will redisplay dialog
}


/*=============================================================================
= Class RepetitionDlg
= Simple alarm repetition dialog.
=============================================================================*/

static const int MAX_COUNT = 9999;    // maximum range for count spinbox


RepetitionDlg::RepetitionDlg(const QString& caption, bool readOnly, QWidget* parent)
	: KDialog(parent),
	  mMaxDuration(-1),
	  mDateOnly(false),
	  mReadOnly(readOnly)
{
	setCaption(caption);
	setButtons(Ok|Cancel);
	int spacing = spacingHint();
	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* topLayout = new QVBoxLayout(page);
	topLayout->setMargin(0);
	topLayout->setSpacing(spacing);

	mTimeSelector = new TimeSelector(i18nc("@option:check Repeat every 10 minutes", "Repeat every"), QString(),
	                  i18nc("@info:whatsthis", "Instead of the alarm triggering just once at each recurrence, "
	                       "checking this option makes the alarm trigger multiple times at each recurrence."),
	                  i18nc("@info:whatsthis", "Enter the time between repetitions of the alarm"),
	                  true, page);
	mTimeSelector->setFixedSize(mTimeSelector->sizeHint());
	connect(mTimeSelector, SIGNAL(valueChanged(const KCal::Duration&)), SLOT(intervalChanged(const KCal::Duration&)));
	connect(mTimeSelector, SIGNAL(toggled(bool)), SLOT(repetitionToggled(bool)));
	topLayout->addWidget(mTimeSelector, 0, Qt::AlignLeft);

	mButtonBox = new QGroupBox(page);
	topLayout->addWidget(mButtonBox);
	mButtonGroup = new ButtonGroup(mButtonBox);
	connect(mButtonGroup, SIGNAL(buttonSet(QAbstractButton*)), SLOT(typeClicked()));

	QVBoxLayout* vlayout = new QVBoxLayout(mButtonBox);
	vlayout->setMargin(marginHint());
	vlayout->setSpacing(spacing);
	QHBoxLayout* layout = new QHBoxLayout();
	layout->setMargin(0);
	vlayout->addLayout(layout);
	mCountButton = new RadioButton(i18nc("@option:radio", "Number of repetitions:"), mButtonBox);
	mCountButton->setFixedSize(mCountButton->sizeHint());
	mCountButton->setWhatsThis(i18nc("@info:whatsthis", "Check to specify the number of times the alarm should repeat after each recurrence"));
	mButtonGroup->addButton(mCountButton);
	layout->addWidget(mCountButton);
	mCount = new SpinBox(1, MAX_COUNT, mButtonBox);
	mCount->setFixedSize(mCount->sizeHint());
	mCount->setSingleShiftStep(10);
	mCount->setSelectOnStep(false);
	connect(mCount, SIGNAL(valueChanged(int)), SLOT(countChanged(int)));
	mCount->setWhatsThis(i18nc("@info:whatsthis", "Enter the number of times to trigger the alarm after its initial occurrence"));
	layout->addWidget(mCount);
	mCountButton->setFocusWidget(mCount);
	layout->addStretch();

	layout = new QHBoxLayout();
	layout->setMargin(0);
	vlayout->addLayout(layout);
	mDurationButton = new RadioButton(i18nc("@option:radio", "Duration:"), mButtonBox);
	mDurationButton->setFixedSize(mDurationButton->sizeHint());
	mDurationButton->setWhatsThis(i18nc("@info:whatsthis", "Check to specify how long the alarm is to be repeated"));
	mButtonGroup->addButton(mDurationButton);
	layout->addWidget(mDurationButton);
	mDuration = new TimePeriod(true, mButtonBox);
	mDuration->setFixedSize(mDuration->sizeHint());
	connect(mDuration, SIGNAL(valueChanged(const KCal::Duration&)), SLOT(durationChanged(const KCal::Duration&)));
	mDuration->setWhatsThis(i18nc("@info:whatsthis", "Enter the length of time to repeat the alarm"));
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
void RepetitionDlg::set(const Duration& interval, int count, bool dateOnly, int maxDuration)
{
	Duration inter = interval;
	if (!interval)
		count = 0;
	else if (!count)
		inter = 0;
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
	TimePeriod::Units units = mDateOnly ? TimePeriod::Days : TimePeriod::HoursMinutes;
	mTimeSelector->setPeriod(inter, mDateOnly, units);
	if (!mMaxDuration  ||  !count)
		mTimeSelector->setChecked(false);
	else
	{
		bool on = mTimeSelector->isChecked();
		repetitionToggled(on);    // enable/disable controls
		if (on)
			intervalChanged(inter);    // ensure mCount range is set
		mCount->setValue(count);
		mDuration->setPeriod(inter * count, mDateOnly, units);
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
Duration RepetitionDlg::interval() const
{
	return mTimeSelector->period();
}

/******************************************************************************
*  Set the entered repeat count.
*/
int RepetitionDlg::count() const
{
	Duration interval = mTimeSelector->period();
	if (interval)
	{
		if (mCountButton->isChecked())
			return mCount->value();
		if (mDurationButton->isChecked())
			return mDuration->period().asSeconds() / interval.asSeconds();
	}
	return 0;    // no repetition
}

/******************************************************************************
*  Called when the time interval widget has changed value.
*  Adjust the maximum repetition count accordingly.
*/
void RepetitionDlg::intervalChanged(const KCal::Duration& interval)
{
	if (mTimeSelector->isChecked()  &&  interval.asSeconds() > 0)
	{
		mCount->setRange(1, (mMaxDuration >= 0 ? mMaxDuration / (interval.asSeconds()/60) : MAX_COUNT));
		if (mCountButton->isChecked())
			countChanged(mCount->value());
		else
			durationChanged(mDuration->period());
	}
}

/******************************************************************************
*  Called when the count spinbox has changed value.
*  Adjust the duration accordingly.
*/
void RepetitionDlg::countChanged(int count)
{
	Duration interval = mTimeSelector->period();
	if (interval)
	{
		bool blocked = mDuration->signalsBlocked();
		mDuration->blockSignals(true);
		mDuration->setPeriod(interval * count, mDateOnly,
		                      (mDateOnly ? TimePeriod::Days : TimePeriod::HoursMinutes));
		mDuration->blockSignals(blocked);
	}
}

/******************************************************************************
*  Called when the duration widget has changed value.
*  Adjust the count accordingly.
*/
void RepetitionDlg::durationChanged(const KCal::Duration& duration)
{
	Duration interval = mTimeSelector->period();
	if (interval)
	{
		bool blocked = mCount->signalsBlocked();
		mCount->blockSignals(true);
		mCount->setValue(duration.asSeconds() / interval.asSeconds());
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
	mButtonBox->setEnabled(on);
	mCount->setEnabled(on  &&  mCountButton->isChecked());
	mDuration->setEnabled(on  &&  mDurationButton->isChecked());
}

/******************************************************************************
*  Called when one of the count or duration radio buttons is toggled.
*/
void RepetitionDlg::typeClicked()
{
	if (mTimeSelector->isChecked())
	{
		mCount->setEnabled(mCountButton->isChecked());
		mDuration->setEnabled(mDurationButton->isChecked());
	}
}
