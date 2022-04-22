/*
 *  mailsend.h  -  data relating to sending emails
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2004-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "kalarmcalendar/kaevent.h"

namespace MailSend
{

// Data to store for each mail send job.
// Some data is required by KAMail, while other data is used by the caller.
struct JobData
{
    JobData() = default;
    JobData(KAlarmCal::KAEvent& e, const KAlarmCal::KAAlarm& a, bool resched, bool notify)
          : event(e), alarm(a), reschedule(resched), allowNotify(notify), queued(false) {}
    KAlarmCal::KAEvent  event;
    KAlarmCal::KAAlarm  alarm;
    QString             from, bcc, subject;
    bool                reschedule;
    bool                allowNotify;
    bool                queued;
};

}

// vim: et sw=4:
