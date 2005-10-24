/*
 *  latecancel.cpp  -  widget to specify cancellation if late
 *  Program:  kalarm
 *  Copyright (c) 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <klocale.h>
#include <kdialog.h>

#include "checkbox.h"
#include "latecancel.moc"


// Collect these widget labels together to ensure consistent wording and
// translations across different modules.
QString LateCancelSelector::i18n_CancelIfLate()       { return i18n("Cancel if late"); }
QString LateCancelSelector::i18n_n_CancelIfLate()     { return i18n("Ca&ncel if late"); }
QString LateCancelSelector::i18n_AutoCloseWin()       { return i18n("Auto-close window after this time"); }
QString LateCancelSelector::i18n_AutoCloseWinLC()     { return i18n("Auto-close window after late-cancelation time"); }
QString LateCancelSelector::i18n_i_AutoCloseWinLC()   { return i18n("Auto-close w&indow after late-cancelation time"); }


LateCancelSelector::LateCancelSelector(bool allowHourMinute, QWidget* parent)
	: QFrame(parent),
	  mDateOnly(false),
	  mReadOnly(false),
	  mAutoCloseShown(false)
{
	QString whatsThis = i18n("If checked, the alarm will be canceled if it cannot be triggered within the "
	                         "specified period after its scheduled time. Possible reasons for not triggering "
	                         "include your being logged off, X not running, or the alarm daemon not running.\n\n"
	                         "If unchecked, the alarm will be triggered at the first opportunity after "
	                         "its scheduled time, regardless of how late it is.");

	mLayout = new QVBoxLayout(this);
	mLayout->setSpacing(KDialog::spacingHint());

	mStack = new QStackedWidget(this);
	mCheckboxFrame = new QFrame(mStack);
	mStack->addWidget(mCheckboxFrame);
	QVBoxLayout* vlayout = new QVBoxLayout(mCheckboxFrame);
	mCheckbox = new CheckBox(i18n_n_CancelIfLate(), mCheckboxFrame);
	mCheckbox->setFixedSize(mCheckbox->sizeHint());
	connect(mCheckbox, SIGNAL(toggled(bool)), SLOT(slotToggled(bool)));
	mCheckbox->setWhatsThis(whatsThis);
	vlayout->addWidget(mCheckbox, 0, Qt::AlignLeft);

	mTimeSelectorFrame = new QFrame(mStack);
	mStack->addWidget(mTimeSelectorFrame);
	vlayout = new QVBoxLayout(mTimeSelectorFrame);
	mTimeSelector = new TimeSelector(i18n("Cancel if late by 10 minutes", "Ca&ncel if late by"), QString::null,
	                                 whatsThis, i18n("Enter how late will cause the alarm to be canceled"),
	                                 allowHourMinute, mTimeSelectorFrame);
	connect(mTimeSelector, SIGNAL(toggled(bool)), SLOT(slotToggled(bool)));
	vlayout->addWidget(mTimeSelector);
	mLayout->addWidget(mStack);

	QHBoxLayout* hlayout = new QHBoxLayout(mLayout);
	hlayout->addSpacing(3*KDialog::spacingHint());
	mAutoClose = new CheckBox(i18n_AutoCloseWin(), this);
	mAutoClose->setFixedSize(mAutoClose->sizeHint());
	mAutoClose->setWhatsThis(i18n("Automatically close the alarm window after the expiry of the late-cancelation period"));
	hlayout->addWidget(mAutoClose);
	hlayout->addStretch();

	mAutoClose->hide();
	mAutoClose->setEnabled(false);
}

/******************************************************************************
*  Set the read-only status.
*/
void LateCancelSelector::setReadOnly(bool ro)
{
	if ((int)ro != (int)mReadOnly)
	{
		mReadOnly = ro;
		mCheckbox->setReadOnly(mReadOnly);
		mTimeSelector->setReadOnly(mReadOnly);
		mAutoClose->setReadOnly(mReadOnly);
	}
}

int LateCancelSelector::minutes() const
{
	return mTimeSelector->minutes();
}

void LateCancelSelector::setMinutes(int minutes, bool dateOnly, TimePeriod::Units defaultUnits)
{
	slotToggled(minutes);
	mTimeSelector->setMinutes(minutes, dateOnly, defaultUnits);
}

void LateCancelSelector::setDateOnly(bool dateOnly)
{
	if (dateOnly != mDateOnly)
	{
		mDateOnly = dateOnly;
		if (mTimeSelector->isChecked())      // don't change when it's not visible
			mTimeSelector->setDateOnly(dateOnly);
	}
}

void LateCancelSelector::showAutoClose(bool show)
{
	if (show)
		mAutoClose->show();
	else
		mAutoClose->hide();
	mAutoCloseShown = show;
	mLayout->activate();
}

bool LateCancelSelector::isAutoClose() const
{
	return mAutoCloseShown  &&  mAutoClose->isEnabled()  &&  mAutoClose->isChecked();
}

void LateCancelSelector::setAutoClose(bool autoClose)
{
	mAutoClose->setChecked(autoClose);
}

/******************************************************************************
*  Called when either of the checkboxes is toggled.
*/
void LateCancelSelector::slotToggled(bool on)
{
	mCheckbox->setChecked(on);
	mTimeSelector->setChecked(on);
	if (on)
	{
		mTimeSelector->setDateOnly(mDateOnly);
		mStack->setCurrentWidget(mTimeSelectorFrame);
	}
	else
		mStack->setCurrentWidget(mCheckboxFrame);
	mAutoClose->setEnabled(on);
}
