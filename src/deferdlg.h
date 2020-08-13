/*
 *  deferdlg.h  -  dialog to defer an alarm
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DEFERDLG_H
#define DEFERDLG_H

#include "eventid.h"

#include <KAlarmCal/DateTime>

#include <QDialog>

class AlarmTimeWidget;
class QDialogButtonBox;
namespace KAlarmCal { class KAEvent; }

using namespace KAlarmCal;


class DeferAlarmDlg : public QDialog
{
        Q_OBJECT
    public:
        DeferAlarmDlg(const DateTime& initialDT, bool anyTimeOption, bool cancelButton, QWidget* parent = nullptr);
        void             setLimit(const DateTime&);
        DateTime         setLimit(const KAEvent& event);
        const DateTime&  getDateTime() const   { return mAlarmDateTime; }
        void             setDeferMinutes(int mins);
        int              deferMinutes() const  { return mDeferMinutes; }

    protected Q_SLOTS:
        virtual void     slotOk();
        virtual void     slotCancelDeferral();

    private Q_SLOTS:
        void             slotPastLimit();

    private:
        AlarmTimeWidget* mTimeWidget;
        QDialogButtonBox* mButtonBox;
        DateTime         mAlarmDateTime;
        DateTime         mLimitDateTime;   // latest date/time allowed for deferral
        EventId          mLimitEventId;    // event IDs from whose recurrences to derive the limit date/time for deferral
        int              mDeferMinutes;    // number of minutes deferral selected, or 0 if date/time entered
};

#endif // DEFERDLG_H

// vim: et sw=4:
