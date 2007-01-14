/*
 *  deferdlg.cpp  -  dialogue to defer an alarm
 *  Program:  kalarm
 *  Copyright © 2002-2006 by David Jarvie <software@astrojar.org.uk>
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

#include <QVBoxLayout>

#include <kglobal.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include <kcal/event.h>
#include <kcal/recurrence.h>

#include "alarmcalendar.h"
#include "alarmevent.h"
#include "alarmtimewidget.h"
#include "datetime.h"
#include "functions.h"
#include "kalarmapp.h"
#include "deferdlg.moc"


DeferAlarmDlg::DeferAlarmDlg(const QString& caption, const DateTime& initialDT,
                             bool cancelButton, QWidget* parent)
	: KDialog(parent)
{
	setWindowModality(Qt::WindowModal);
	setCaption(caption);
	setButtons(Ok | Cancel | User1);
	setButtonGuiItem(User1, KGuiItem(i18n("Cancel &Deferral")));
	if (!cancelButton)
		showButton(User1, false);
	connect(this, SIGNAL(okClicked()), SLOT(slotOk()));
	connect(this, SIGNAL(user1Clicked()), SLOT(slotCancelDeferral()));

	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* layout = new QVBoxLayout(page);
	layout->setMargin(0);
	layout->setSpacing(spacingHint());

	mTimeWidget = new AlarmTimeWidget(AlarmTimeWidget::DEFER_TIME, page);
	mTimeWidget->setDateTime(initialDT);
	mTimeWidget->setMinDateTimeIsCurrent();
	connect(mTimeWidget, SIGNAL(pastMax()), SLOT(slotPastLimit()));
	layout->addWidget(mTimeWidget);
	layout->addSpacing(spacingHint());

	setButtonWhatsThis(Ok, i18n("Defer the alarm until the specified time."));
	setButtonWhatsThis(User1, i18n("Cancel the deferred alarm. This does not affect future recurrences."));
}


/******************************************************************************
*  Called when the OK button is clicked.
*/
void DeferAlarmDlg::slotOk()
{
	mAlarmDateTime = mTimeWidget->getDateTime(&mDeferMinutes);
	if (!mAlarmDateTime.isValid())
		return;
	KAEvent::DeferLimitType limitType;
	DateTime endTime;
	if (!mLimitEventID.isEmpty())
	{
		// Get the event being deferred
		const KCal::Event* kcalEvent = AlarmCalendar::getEvent(mLimitEventID);
		if (kcalEvent)
		{
			KAEvent event(kcalEvent);
			endTime = event.deferralLimit(&limitType);
		}
	}
	else
	{
		endTime = mLimitDateTime;
		limitType = mLimitDateTime.isValid() ? KAEvent::LIMIT_MAIN : KAEvent::LIMIT_NONE;
	}
	if (endTime.isValid()  &&  mAlarmDateTime > endTime)
	{
		QString text;
		switch (limitType)
		{
			case KAEvent::LIMIT_REPETITION:
				text = i18nc("This refers to simple repetitions set up using the Simple Repetition dialog",
				             "Cannot defer past the alarm's next repetition (currently %1)",
				             endTime.formatLocale());
				break;
			case KAEvent::LIMIT_RECURRENCE:
				text = i18nc("This refers to recurrences set up using the Recurrence tab",
				             "Cannot defer past the alarm's next recurrence (currently %1)",
				             endTime.formatLocale());
				break;
			case KAEvent::LIMIT_REMINDER:
				text = i18n("Cannot defer past the alarm's next reminder (currently %1)",
				            endTime.formatLocale());
				break;
			case KAEvent::LIMIT_MAIN:
				text = i18n("Cannot defer reminder past the main alarm time (%1)",
				            endTime.formatLocale());
				break;
			case KAEvent::LIMIT_NONE:
				break;   // can't happen with a valid endTime
		}
		KMessageBox::sorry(this, text);
	}
	else
		accept();
}

/******************************************************************************
* Select the 'Time from now' radio button and preset its value.
*/
void DeferAlarmDlg::setDeferMinutes(int minutes)
{
	mTimeWidget->selectTimeFromNow(minutes);
}

/******************************************************************************
*  Called the maximum date/time for the date/time edit widget has been passed.
*/
void DeferAlarmDlg::slotPastLimit()
{
	enableButtonOk(false);
}

/******************************************************************************
*  Set the time limit for deferral based on the next occurrence of the alarm
*  with the specified ID.
*/
void DeferAlarmDlg::setLimit(const DateTime& limit)
{
	mLimitEventID .clear();
	mLimitDateTime = limit;
	mTimeWidget->setMaxDateTime(mLimitDateTime);
}

/******************************************************************************
*  Set the time limit for deferral based on the next occurrence of the alarm
*  with the specified ID.
*/
DateTime DeferAlarmDlg::setLimit(const QString& eventID)
{
	mLimitEventID = eventID;
	const KCal::Event* kcalEvent = AlarmCalendar::getEvent(mLimitEventID);
	if (kcalEvent)
	{
		KAEvent event(kcalEvent);
		mLimitDateTime = event.deferralLimit();
	}
	else
		mLimitDateTime = DateTime();
	mTimeWidget->setMaxDateTime(mLimitDateTime);
	return mLimitDateTime;
}

/******************************************************************************
*  Called when the Cancel Deferral button is clicked.
*/
void DeferAlarmDlg::slotCancelDeferral()
{
	mAlarmDateTime = DateTime();
	accept();
}
