/*
 *  deferdlg.h  -  dialog to defer an alarm
 *  Program:  kalarm
 *  Copyright © 2002-2004,2006,2007-2012 by David Jarvie <djarvie@kde.org>
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

#ifndef DEFERDLG_H
#define DEFERDLG_H

#include "eventid.h"

#include <kalarmcal/datetime.h>
#include <AkonadiCore/collection.h>
#include <kdialog.h>

class AlarmTimeWidget;
namespace KAlarmCal { class KAEvent; }

using namespace KAlarmCal;


class DeferAlarmDlg : public KDialog
{
        Q_OBJECT
    public:
        DeferAlarmDlg(const DateTime& initialDT, bool anyTimeOption, bool cancelButton, QWidget* parent = 0);
        void             setLimit(const DateTime&);
        DateTime         setLimit(const KAEvent& event);
        const DateTime&  getDateTime() const   { return mAlarmDateTime; }
        void             setDeferMinutes(int mins);
        int              deferMinutes() const  { return mDeferMinutes; }

    protected slots:
        virtual void     slotOk();
        virtual void     slotCancelDeferral();

    private slots:
        void             slotPastLimit();

    private:
        AlarmTimeWidget* mTimeWidget;
        DateTime         mAlarmDateTime;
        DateTime         mLimitDateTime;   // latest date/time allowed for deferral
        EventId          mLimitEventId;    // event IDs from whose recurrences to derive the limit date/time for deferral
        int              mDeferMinutes;    // number of minutes deferral selected, or 0 if date/time entered
};

#endif // DEFERDLG_H

// vim: et sw=4:
