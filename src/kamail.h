/*
 *  kamail.h  -  email functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "kalarmcalendar/kaevent.h"

#include <KCalendarCore/Person>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QQueue>

class QUrl;
class KJob;
namespace MailTransport  { class MessageQueueJob; }
namespace KMime {
    namespace Types { struct Address; }
    class Message;
}

using namespace KAlarmCal;

class KAMail : public QObject
{
    Q_OBJECT
public:
    // Data to store for each mail send job.
    // Some data is required by KAMail, while other data is used by the caller.
    struct JobData
    {
        JobData() = default;
        JobData(KAEvent& e, const KAAlarm& a, bool resched, bool notify)
              : event(e), alarm(a), reschedule(resched), allowNotify(notify), queued(false) {}
        KAEvent  event;
        KAAlarm  alarm;
        QString  from, bcc, subject;
        bool     reschedule;
        bool     allowNotify;
        bool     queued;
    };

    static int         send(JobData&, QStringList& errmsgs);
    static int         checkAddress(QString& address);
    static int         checkAttachment(QString& attachment, QUrl* = nullptr);
    static bool        checkAttachment(const QUrl&);
    static QString     convertAddresses(const QString& addresses, KCalendarCore::Person::List&);
    static QString     convertAttachments(const QString& attachments, QStringList& list);
    static QString     controlCentreAddress();
    static QString     getMailBody(quint32 serialNumber);
    static QString     i18n_NeedFromEmailAddress();
    static QString     i18n_sent_mail();

private Q_SLOTS:
    void               slotEmailSent(KJob*);

private:
    KAMail() = default;
    static KAMail*     instance();
    static QString     appendBodyAttachments(KMime::Message& message, JobData&);
    static void        notifyQueued(const KAEvent&);
    enum ErrType { SEND_FAIL, SEND_ERROR };
    static QStringList errors(const QString& error = QString(), ErrType = SEND_FAIL);

    static KAMail*     mInstance;
    static QQueue<MailTransport::MessageQueueJob*> mJobs;
    static QQueue<JobData>                         mJobData;
};

// vim: et sw=4:
