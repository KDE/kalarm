/*
 *  deferdlg.cpp  -  dialogue to defer an alarm
 *  Program:  kalarm
 *  (C) 2002 - 2005 by David Jarvie <software@astrojar.org.uk>
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

#include <qlayout.h>

#include <kglobal.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>

#include <libkcal/event.h>
#include <libkcal/recurrence.h>

#include "alarmcalendar.h"
#include "alarmevent.h"
#include "alarmtimewidget.h"
#include "datetime.h"
#include "functions.h"
#include "kalarmapp.h"
#include "deferdlg.moc"


DeferAlarmDlg::DeferAlarmDlg(const QString& caption, const DateTime& initialDT,
                             bool cancelButton, QWidget* parent, const char* name)
	: KDialogBase(parent, name, true, caption, Ok|Cancel|User1, Ok, false, i18n("Cancel &Deferral"))
{
	if (!cancelButton)
		showButton(User1, false);

	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* layout = new QVBoxLayout(page, marginKDE2, spacingHint());

	mTimeWidget = new AlarmTimeWidget(AlarmTimeWidget::DEFER_TIME, page, "timeGroup");
	mTimeWidget->setDateTime(initialDT);
	mTimeWidget->setMinDateTimeIsCurrent();
	connect(mTimeWidget, SIGNAL(pastMax()), SLOT(slotPastLimit()));
	layout->addWidget(mTimeWidget);
	layout->addSpacing(KDialog::spacingHint());

	setButtonWhatsThis(Ok, i18n("Defer the alarm until the specified time."));
	setButtonWhatsThis(User1, i18n("Cancel the deferred alarm. This does not affect future recurrences."));
}


/******************************************************************************
*  Called when the OK button is clicked.
*/
void DeferAlarmDlg::slotOk()
{
	mAlarmDateTime = mTimeWidget->getDateTime();
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
			KAEvent event(*kcalEvent);
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
				text = i18n("This refers to simple repetitions set up using the Simple Repetition dialog",
				            "Cannot defer past the alarm's next repetition (currently %1)");
				break;
			case KAEvent::LIMIT_RECURRENCE:
				text = i18n("This refers to recurrences set up using the Recurrence tab",
				            "Cannot defer past the alarm's next recurrence (currently %1)");
				break;
			case KAEvent::LIMIT_REMINDER:
				text = i18n("Cannot defer past the alarm's next reminder (currently %1)");
				break;
			case KAEvent::LIMIT_MAIN:
				text = i18n("Cannot defer reminder past the main alarm time (%1)");
				break;
			case KAEvent::LIMIT_NONE:
				break;   // can't happen with a valid endTime
		}
		KMessageBox::sorry(this, text.arg(endTime.formatLocale()));
	}
	else
		accept();
}

/******************************************************************************
*  Called the maximum date/time for the date/time edit widget has been passed.
*/
void DeferAlarmDlg::slotPastLimit()
{
	enableButtonOK(false);
}

/******************************************************************************
*  Set the time limit for deferral based on the next occurrence of the alarm
*  with the specified ID.
*/
void DeferAlarmDlg::setLimit(const DateTime& limit)
{
	mLimitEventID  = QString::null;
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
		KAEvent event(*kcalEvent);
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
void DeferAlarmDlg::slotUser1()
{
	mAlarmDateTime = DateTime();
	accept();
}

/******************************************************************************
*  Called when the Cancel button is clicked.
*/
void DeferAlarmDlg::slotCancel()
{
	reject();
}
