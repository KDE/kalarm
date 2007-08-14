/*
 *  latecancel.cpp  -  widget to specify cancellation if late
 *  Program:  kalarm
 *  Copyright © 2004,2005,2007 by David Jarvie <software@astrojar.org.uk>
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
QString LateCancelSelector::i18n_chk_CancelIfLate()    { return i18nc("@option:check", "Cancel if late"); }
QString LateCancelSelector::i18n_chk_AutoCloseWin()    { return i18nc("@option:check", "Auto-close window after this time"); }
QString LateCancelSelector::i18n_chk_AutoCloseWinLC()  { return i18nc("@option:check", "Auto-close window after late-cancellation time"); }


LateCancelSelector::LateCancelSelector(bool allowHourMinute, QWidget* parent)
	: QFrame(parent),
	  mDateOnly(false),
	  mReadOnly(false),
	  mAutoCloseShown(false)
{
	QString whatsThis = i18nc("@info:whatsthis",
	                          "If checked, the alarm will be canceled if it cannot be triggered within the "
	                         "specified period after its scheduled time. Possible reasons for not triggering "
	                         "include your being logged off, X not running, or the alarm daemon not running.\n\n"
	                         "If unchecked, the alarm will be triggered at the first opportunity after "
	                         "its scheduled time, regardless of how late it is.");

	mLayout = new QVBoxLayout(this);
	mLayout->setMargin(0);
	mLayout->setSpacing(KDialog::spacingHint());

	mStack = new QStackedWidget(this);
	mLayout->addWidget(mStack, 0, Qt::AlignLeft);
	mCheckboxFrame = new QFrame();
	mStack->addWidget(mCheckboxFrame);
	QVBoxLayout* vlayout = new QVBoxLayout(mCheckboxFrame);
	vlayout->setMargin(0);
	mCheckbox = new CheckBox(i18n_chk_CancelIfLate(), mCheckboxFrame);
	mCheckbox->setFixedSize(mCheckbox->sizeHint());
	connect(mCheckbox, SIGNAL(toggled(bool)), SLOT(slotToggled(bool)));
	mCheckbox->setWhatsThis(whatsThis);
	vlayout->addWidget(mCheckbox, 0, Qt::AlignLeft);

	mTimeSelectorFrame = new QFrame();
	mStack->addWidget(mTimeSelectorFrame);
	vlayout = new QVBoxLayout(mTimeSelectorFrame);
	vlayout->setMargin(0);
	mTimeSelector = new TimeSelector(i18nc("@option:check\nCancel if late by 10 minutes", "Ca&ncel if late by"), QString(),
	                                 whatsThis, i18nc("@label", "Enter how late will cause the alarm to be canceled"),
	                                 allowHourMinute, mTimeSelectorFrame);
	connect(mTimeSelector, SIGNAL(toggled(bool)), SLOT(slotToggled(bool)));
	vlayout->addWidget(mTimeSelector, 0, Qt::AlignLeft);

	QHBoxLayout* hlayout = new QHBoxLayout();
	hlayout->setMargin(0);
	hlayout->addSpacing(3*KDialog::spacingHint());
	mLayout->addLayout(hlayout);
	mAutoClose = new CheckBox(i18n_chk_AutoCloseWin(), this);
	mAutoClose->setFixedSize(mAutoClose->sizeHint());
	mAutoClose->setWhatsThis(i18nc("@info:whatsthis", "Automatically close the alarm window after the expiry of the late-cancellation period"));
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
