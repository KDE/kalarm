/*
 *  sendakonadimail.cpp  -  sends email using MailTransport library
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2010-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sendakonadimail.h"

#include "mailsend.h"
#include "akonadiplugin_debug.h"

#include <KIdentityManagement/Identity>
#include <MailTransport/TransportManager>
#include <MailTransport/Transport>
#include <MailTransportAkonadi/MessageQueueJob>

#include <KEmailAddress>
#include <KLocalizedString>

using namespace MailSend;

namespace
{
QStringList extractEmailsAndNormalize(const QString& emailAddresses);
}

SendAkonadiMail*                              SendAkonadiMail::mInstance = nullptr;   // used only to enable signals/slots to work
QQueue<MailTransport::MessageQueueJob*> SendAkonadiMail::mJobs;
QQueue<JobData>                         SendAkonadiMail::mJobData;

SendAkonadiMail* SendAkonadiMail::instance()
{
    if (!mInstance)
        mInstance = new SendAkonadiMail();
    return mInstance;
}

/******************************************************************************
* Send an email message.
* Reply = empty string if the message is queued for sending,
*       = error message if the message was not sent.
*/
QString SendAkonadiMail::send(KMime::Message::Ptr message, const KIdentityManagement::Identity& identity,
                        const QString& normalizedFrom, bool keepSentMail, JobData& jobdata)
{
    qCDebug(AKONADIPLUGIN_LOG) << "SendAkonadiMail::send: Sending via KDE";
    MailTransport::TransportManager* manager = MailTransport::TransportManager::self();
    const int transportId = identity.transport().isEmpty() ? -1 : identity.transport().toInt();
    MailTransport::Transport* transport = manager->transportById(transportId, true);
    if (!transport)
    {
        qCCritical(AKONADIPLUGIN_LOG) << "SendAkonadiMail::send: No mail transport found for identity" << identity.identityName() << "uoid" << identity.uoid();
        return xi18nc("@info", "No mail transport configured for email identity <resource>%1</resource>", identity.identityName());
    }
    qCDebug(AKONADIPLUGIN_LOG) << "SendAkonadiMail::send: Using transport" << transport->name() << ", id=" << transport->id();

    auto mailjob = new MailTransport::MessageQueueJob(qApp);
    mailjob->setMessage(message);
    mailjob->transportAttribute().setTransportId(transport->id());
    // MessageQueueJob email addresses must be pure, i.e. without display name. Note
    // that display names are included in the actual headers set up in 'message'.
    mailjob->addressAttribute().setFrom(normalizedFrom);
    mailjob->addressAttribute().setTo(extractEmailsAndNormalize(jobdata.event.emailAddresses(QStringLiteral(","))));
    if (!jobdata.bcc.isEmpty())
        mailjob->addressAttribute().setBcc(extractEmailsAndNormalize(jobdata.bcc));
    MailTransport::SentBehaviourAttribute::SentBehaviour sentAction =
                         keepSentMail ? MailTransport::SentBehaviourAttribute::MoveToDefaultSentCollection
                                      : MailTransport::SentBehaviourAttribute::Delete;
    mailjob->sentBehaviourAttribute().setSentBehaviour(sentAction);
    mJobs.enqueue(mailjob);
    mJobData.enqueue(jobdata);
    if (mJobs.count() == 1)
    {
        // There are no jobs already active or queued, so send now
        connect(mailjob, &KJob::result, instance(), &SendAkonadiMail::slotEmailSent);
        mailjob->start();
    }
    return {};
}

/******************************************************************************
* Called when sending an email is complete.
*/
void SendAkonadiMail::slotEmailSent(KJob* job)
{
    bool sendError = false;
    QStringList errmsgs;
    if (job->error())
    {
        qCCritical(AKONADIPLUGIN_LOG) << "SendAkonadiMail::slotEmailSent: Failed:" << job->errorString();
        errmsgs += job->errorString();
        sendError = true;
    }
    JobData jobdata;
    if (mJobs.isEmpty()  ||  mJobData.isEmpty()  ||  job != std::as_const(mJobs).head())
    {
        // The queue has been corrupted, so we can't locate the job's data
        qCCritical(AKONADIPLUGIN_LOG) << "SendAkonadiMail::slotEmailSent: Wrong job at head of queue: wiping queue";
        mJobs.clear();
        mJobData.clear();
        if (!errmsgs.isEmpty())
            Q_EMIT sent(jobdata, errmsgs, sendError);
        errmsgs.clear();
        errmsgs += i18nc("@info", "Emails may not have been sent");
        errmsgs += i18nc("@info", "Program error");
        Q_EMIT sent(jobdata, errmsgs, false);
        return;
    }
    mJobs.dequeue();
    jobdata = mJobData.dequeue();
    if (jobdata.allowNotify)
        Q_EMIT queued(jobdata.event);
    Q_EMIT sent(jobdata, errmsgs, sendError);
    if (!mJobs.isEmpty())
    {
        // Send the next queued email
        auto job1 = mJobs.head();
        connect(job1, &KJob::result, instance(), &SendAkonadiMail::slotEmailSent);
        job1->start();
    }
}

namespace
{

QStringList extractEmailsAndNormalize(const QString& emailAddresses)
{
    const QStringList splitEmails(KEmailAddress::splitAddressList(emailAddresses));
    QStringList normalizedEmail;
    normalizedEmail.reserve(splitEmails.count());
    for (const QString& email : splitEmails)
    {
        normalizedEmail << KEmailAddress::extractEmailAddress(KEmailAddress::normalizeAddressesAndEncodeIdn(email));
    }
    return normalizedEmail;
}

} // namespace

// vim: et sw=4:
