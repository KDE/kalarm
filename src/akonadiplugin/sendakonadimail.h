/*
 *  sendakonadimail.h  -  sends email using MailTransport library
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2010-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "mailsend.h"

#include <KMime/Message>

#include <QObject>
#include <QQueue>
#include <QStringList>

class KJob;
namespace KIdentityManagementCore { class Identity; }
namespace Akonadi { class MessageQueueJob; }
namespace KAlarmCal { class KAEvent; }

class SendAkonadiMail : public QObject
{
    Q_OBJECT
public:
    static SendAkonadiMail* instance();
    static QString send(KMime::Message::Ptr message, const KIdentityManagementCore::Identity& identity,
                        const QString& normalizedFrom, bool keepSentMail, MailSend::JobData& jobdata);

Q_SIGNALS:
    /** Emitted when a send job has been queued. */
    void queued(const KAlarmCal::KAEvent&);

    /** Emitted when an email has been sent, successfully or not.
     *  @param jobdata    Job data supplied to send().
     *  @param errmsgs    Error messages if an error occurred, else empty.
     *  @param sendError  true if send failed.
     */
    void sent(const MailSend::JobData&, const QStringList& errmsgs, bool sendError);

private Q_SLOTS:
    void slotEmailSent(KJob*);

private:
    SendAkonadiMail() = default;

    static SendAkonadiMail*                        mInstance;
    static QQueue<Akonadi::MessageQueueJob*>       mJobs;
    static QQueue<MailSend::JobData>               mJobData;
};

// vim: et sw=4:
