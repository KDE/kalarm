/*
 *  deferdlg.cpp  -  dialogue to defer an alarm
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <qlayout.h>

#include <kglobal.h>
#include <klocale.h>
#include <kmessagebox.h>

#include <libkcal/event.h>
#include <libkcal/recurrence.h>

#include "kalarmapp.h"
#include "alarmtimewidget.h"
#include "alarmevent.h"
#include "datetime.h"
#include "deferdlg.moc"

using namespace KCal;


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
	layout->addWidget(mTimeWidget);
	layout->addSpacing(KDialog::spacingHint());

	setButtonWhatsThis(Ok, i18n("Defer the alarm until the specified time."));
	setButtonWhatsThis(User1, i18n("Cancel the deferred alarm. This does not affect future recurrences."));
}


DeferAlarmDlg::~DeferAlarmDlg()
{
}


/******************************************************************************
*  Called when the OK button is clicked.
*/
void DeferAlarmDlg::slotOk()
{
	if (!mTimeWidget->getDateTime(mAlarmDateTime))
	{
		bool recurs = false;
		bool reminder = false;
		DateTime endTime;
		if (!mLimitEventID.isEmpty())
		{
			// Get the event being deferred
			const Event* kcalEvent = theApp()->getEvent(mLimitEventID);
			if (kcalEvent)
			{
				KAlarmEvent event(*kcalEvent);
				Recurrence* recurrence = kcalEvent->recurrence();
				if (recurrence  &&  recurrence->doesRecur() != Recurrence::rNone)
				{
					// It's a repeated alarm. Don't allow it to be deferred past its next occurrence.
					QDateTime now = QDateTime::currentDateTime();
					event.nextOccurrence(now, endTime);
					recurs = true;
					if (event.reminder())
					{
						DateTime reminderTime = endTime.addMins(-event.reminder());
						if (now < reminderTime)
						{
							endTime = reminderTime;
							reminder = true;
						}
					}
				}
				else if ((event.reminder() || event.reminderDeferral() || event.reminderArchived())
				     &&  QDateTime::currentDateTime() < event.mainDateTime().dateTime())
				{
					// It's an advance warning alarm. Don't allow it to be deferred past its main alarm time.
					endTime = event.mainDateTime();
					reminder = true;
				}
			}
		}
		else
			endTime = mLimitDateTime;
		if (endTime.isValid()  &&  mAlarmDateTime >= endTime)
		{
			QString text = !reminder ? i18n("Cannot defer past the alarm's next recurrence (currently %1)")
			             : recurs    ? i18n("Cannot defer past the alarm's next reminder (currently %1)")
			             :             i18n("Cannot defer reminder past the main alarm time (%1)");
			KMessageBox::sorry(this, text.arg(endTime.formatLocale()));
		}
		else
			accept();
	}
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
