/*
 *  deferdlg.cpp  -  dialogue to defer an alarm
 *  Program:  kalarm
 *  (C) 2002 by David Jarvie  software@astrojar.org.uk
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

#include "datetime.h"
#include "deferdlg.moc"


DeferAlarmDlg::DeferAlarmDlg(const QString& caption, const QDateTime& initialDT, const QDateTime& limitDT,
                             bool cancelButton, QWidget* parent, const char* name)
	: KDialogBase(parent, name, true, caption, Ok|Cancel|User1, Ok, false, i18n("Cancel &Deferral")),
	  limitDateTime(limitDT)
{
	if (!cancelButton)
		showButton(User1, false);

	QWidget* page = new QWidget(this);
	setMainWidget(page);
	QVBoxLayout* layout = new QVBoxLayout(page, marginKDE2, spacingHint());

	timeWidget = new AlarmTimeWidget(AlarmTimeWidget::DEFER_TIME, page, "timeGroup");
	timeWidget->setDateTime(initialDT);
	layout->addWidget(timeWidget);
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
	bool anyTime;
	if (timeWidget->getDateTime(alarmDateTime, anyTime))
	{
		if (limitDateTime.isValid()  &&  alarmDateTime >= limitDateTime)
			KMessageBox::sorry(this, i18n("Cannot defer past the alarm's next recurrence (currently %1)")
				                          .arg(KGlobal::locale()->formatDateTime(limitDateTime)));
		else
			accept();
	}
}

/******************************************************************************
*  Called when the Cancel Deferral button is clicked.
*/
void DeferAlarmDlg::slotUser1()
{
	alarmDateTime = QDateTime();
	accept();
}

/******************************************************************************
*  Called when the Cancel button is clicked.
*/
void DeferAlarmDlg::slotCancel()
{
	reject();
}
