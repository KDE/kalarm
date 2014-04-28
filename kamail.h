/*
 *  kamail.h  -  email functions
 *  Program:  kalarm
 *  Copyright © 2002-2011 by David Jarvie <djarvie@kde.org>
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

#ifndef KAMAIL_H
#define KAMAIL_H

#include <kalarmcal/kaevent.h>

#ifdef USE_AKONADI
#include <KCalCore/person.h>
#else
#include <kcal/person.h>
#endif

#include <QObject>
#include <QString>
#include <QStringList>
#include <QQueue>
#ifdef USE_AKONADI
#include <QList>
#endif

class KUrl;
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
            JobData() {}
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
        static int         checkAttachment(QString& attachment, KUrl* = 0);
        static bool        checkAttachment(const KUrl&);
#ifdef USE_AKONADI
        static QString     convertAddresses(const QString& addresses, KCalCore::Person::List&);
#else
        static QString     convertAddresses(const QString& addresses, QList<KCal::Person>&);
#endif
        static QString     convertAttachments(const QString& attachments, QStringList& list);
        static QString     controlCentreAddress();
#ifdef KMAIL_SUPPORTED
        static QString     getMailBody(quint32 serialNumber);
#endif
        static QString     i18n_NeedFromEmailAddress();
        static QString     i18n_sent_mail();

    private slots:
        void               slotEmailSent(KJob*);

    private:
        KAMail() {}
        static KAMail*     instance();
        static QString     appendBodyAttachments(KMime::Message& message, JobData&);
        static void        notifyQueued(const KAEvent&);
        enum ErrType { SEND_FAIL, SEND_ERROR };
        static QStringList errors(const QString& error = QString(), ErrType = SEND_FAIL);

        static KAMail*     mInstance;
        static QQueue<MailTransport::MessageQueueJob*> mJobs;
        static QQueue<JobData>                         mJobData;
};

#endif // KAMAIL_H

// vim: et sw=4:
