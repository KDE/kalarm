/*
 *  kamail.h  -  email functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "mailsend.h"

#include <KCalendarCore/Person>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QQueue>

class QUrl;
namespace KMime {
    namespace Types { struct Address; }
    class Message;
}
class AkonadiPlugin;

using namespace KAlarmCal;

class KAMail : public QObject
{
    Q_OBJECT
public:
    static int         send(MailSend::JobData&, QStringList& errmsgs);
    static int         checkAddress(QString& address);
    static int         checkAttachment(QString& attachment, QUrl* = nullptr);
    static bool        checkAttachment(const QUrl&);
    static QString     convertAddresses(const QString& addresses, KCalendarCore::Person::List&);
    static QString     convertAttachments(const QString& attachments, QStringList& list);
    static QString     controlCentreAddress();
    static QString     i18n_NeedFromEmailAddress();
    static QString     i18n_sent_mail();

private Q_SLOTS:
    void akonadiEmailSent(const MailSend::JobData&, const QStringList& errmsgs, bool sendError);

private:
    KAMail() = default;
    static KAMail*     instance();
    static QString     appendBodyAttachments(KMime::Message& message, MailSend::JobData&);
    static void        notifyQueued(const KAEvent&);
    enum ErrType { SEND_FAIL, SEND_ERROR };
    static QStringList errors(const QString& error = QString(), ErrType = SEND_FAIL);

    static KAMail*        mInstance;
    static AkonadiPlugin* mAkonadiPlugin;
};

// vim: et sw=4:
