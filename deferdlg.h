/*
 *  deferdlg.h  -  dialogue to defer an alarm
 *  Program:  kalarm
 *  (C) 2002 - 2004 by David Jarvie <software@astrojar.org.uk>
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

#ifndef DEFERDLG_H
#define DEFERDLG_H

#include <kdialogbase.h>
#include "datetime.h"

class AlarmTimeWidget;


class DeferAlarmDlg : public KDialogBase
{
		Q_OBJECT
	public:
		DeferAlarmDlg(const QString& caption, const DateTime& initialDT,
		              bool cancelButton, QWidget* parent = 0, const char* name = 0);
		void             setLimit(const DateTime&);
		DateTime         setLimit(const QString& eventID);
		const DateTime&  getDateTime() const  { return mAlarmDateTime; }

	protected slots:
		virtual void     slotOk();
		virtual void     slotCancel();
		virtual void     slotUser1();

	private slots:
		void             slotPastLimit();

	private:
		AlarmTimeWidget* mTimeWidget;
		DateTime         mAlarmDateTime;
		DateTime         mLimitDateTime;   // latest date/time allowed for deferral
		QString          mLimitEventID;    // event from whose recurrences to derive the limit date/time for deferral
};

#endif // DEFERDLG_H
