/*
 *  kamail.cpp  -  email functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kamail.h"

#include "kalarm.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "preferences.h"
#include "lib/messagebox.h"
#include "kalarmcalendar/identities.h"
#include "akonadiplugin/akonadiplugin.h"
#include "kalarm_debug.h"

#include "kmailinterface.h"

#include <KIdentityManagementCore/IdentityManager>
#include <KIdentityManagementCore/Identity>
#include <KMime/HeaderParsing>
#include <KMime/Headers>
#include <KMime/Message>

#include <KEmailAddress>
#include <KAboutData>
#include <KLocalizedString>
#include <KFileItem>
#include <KIO/StatJob>
#include <KIO/StoredTransferJob>
#include <KJobWidgets>
#include <KEMailSettings>
#include <KCodecs>
#include <KShell>

#include <QUrl>
#include <QFile>
#include <QHostInfo>
#include <QList>
#include <QByteArray>
#include <QStandardPaths>
#include <QDBusInterface>
using namespace Qt::Literals::StringLiterals;

#ifdef Q_OS_WIN
#define popen _popen
#define pclose _pclose
#endif

using namespace MailSend;

namespace HeaderParsing
{
bool parseAddress(const char* & scursor, const char* const send,
                  KMime::Types::Address& result, bool isCRLF = false);
}

namespace
{
void                        initHeaders(KMime::Message&, JobData&);
KMime::Types::Mailbox::List parseAddresses(const QString& text, QString& invalidItem);
QString                     extractEmailAndNormalize(const QString& emailAddress);
}

QString KAMail::i18n_NeedFromEmailAddress()
{ return i18nc("@info", "A 'From' email address must be configured in order to execute email alarms."); }

QString KAMail::i18n_sent_mail()
{ return i18nc("@info KMail folder name: this should be translated the same as in kmail", "sent-mail"); }

KAMail*        KAMail::mInstance = nullptr;   // used only to enable signals/slots to work
AkonadiPlugin* KAMail::mAkonadiPlugin = nullptr;

KAMail* KAMail::instance()
{
    if (!mInstance)
        mInstance = new KAMail();
    return mInstance;
}

/******************************************************************************
* Send the email message specified in an event.
* Reply = 1 if the message was sent - 'errmsgs' may contain copy error messages.
*       = 0 if the message is queued for sending.
*       = -1 if the message was not sent - 'errmsgs' contains the error messages.
*/
int KAMail::send(JobData& jobdata, QStringList& errmsgs)
{
    QString err;
    KIdentityManagementCore::Identity identity;
    jobdata.from = Preferences::emailAddress();
    if (jobdata.event.emailFromId()
    &&  Preferences::emailFrom() == Preferences::MAIL_FROM_KMAIL)
    {
        identity = Identities::identityManager()->identityForUoid(jobdata.event.emailFromId());
        if (identity.isNull())
        {
            qCCritical(KALARM_LOG) << "KAMail::send: Identity" << jobdata.event.emailFromId() << "not found";
            errmsgs = errors(xi18nc("@info", "Invalid 'From' email address.<nl/>Email identity <resource>%1</resource> not found", jobdata.event.emailFromId()));
            return -1;
        }
        if (identity.primaryEmailAddress().isEmpty())
        {
            qCCritical(KALARM_LOG) << "KAMail::send: Identity" << identity.identityName() << "uoid" << identity.uoid() << ": no email address";
            errmsgs = errors(xi18nc("@info", "Invalid 'From' email address.<nl/>Email identity <resource>%1</resource> has no email address", identity.identityName()));
            return -1;
        }
        jobdata.from = identity.fullEmailAddr();
    }
    if (jobdata.from.isEmpty())
    {
        switch (Preferences::emailFrom())
        {
            case Preferences::MAIL_FROM_KMAIL:
                errmsgs = errors(xi18nc("@info", "<para>No 'From' email address is configured (no default email identity found)</para>"
                                                "<para>Please set it in <application>KMail</application> or in the <application>KAlarm</application> Configuration dialog.</para>"));
                break;
            case Preferences::MAIL_FROM_SYS_SETTINGS:
                errmsgs = errors(xi18nc("@info", "<para>No 'From' email address is configured.</para>"
                                                "<para>Please set a default address in <application>KMail</application> or in the <application>KAlarm</application> Configuration dialog.</para>"));
                break;
            case Preferences::MAIL_FROM_ADDR:
            default:
                errmsgs = errors(xi18nc("@info", "<para>No 'From' email address is configured.</para>"
                                                "<para>Please set it in the <application>KAlarm</application> Configuration dialog.</para>"));
                break;
        }
        return -1;
    }
    jobdata.bcc  = (jobdata.event.emailBcc() ? Preferences::emailBccAddress() : QString());
    qCDebug(KALARM_LOG) << "KAMail::send: To:" << jobdata.event.emailAddresses(QStringLiteral(","))
                        << "\nSubject:" << jobdata.event.emailSubject();

    KMime::Message::Ptr message = KMime::Message::Ptr(new KMime::Message);

    if (Preferences::emailClient() == Preferences::sendmail)
    {
        qCDebug(KALARM_LOG) << "KAMail::send: Sending via sendmail";
        const QStringList paths{QStringLiteral("/sbin"), QStringLiteral("/usr/sbin"), QStringLiteral("/usr/lib")};
        QString command = QStandardPaths::findExecutable(QStringLiteral("sendmail"), paths);
        if (!command.isNull())
        {
            command += QStringLiteral(" -f ");
            command += extractEmailAndNormalize(jobdata.from);
            command += QStringLiteral(" -oi -t ");
            initHeaders(*message, jobdata);
        }
        else
        {
            command = QStandardPaths::findExecutable(QStringLiteral("mail"), paths);
            if (command.isNull())
            {
                qCCritical(KALARM_LOG) << "KAMail::send: sendmail not found";
                errmsgs = errors(xi18nc("@info", "<command>%1</command> not found", QStringLiteral("sendmail"))); // give up
                return -1;
            }

            command += QStringLiteral(" -s ");
            command += KShell::quoteArg(jobdata.event.emailSubject());

            if (!jobdata.bcc.isEmpty())
            {
                command += QStringLiteral(" -b ");
                command += extractEmailAndNormalize(jobdata.bcc);
            }

            command += ' '_L1;
            command += jobdata.event.emailPureAddresses(QStringLiteral(" ")); // locally provided, okay
        }
        // Add the body and attachments to the message.
        // (Sendmail requires attachments to have already been included in the message.)
        err = appendBodyAttachments(*message, jobdata);
        if (!err.isNull())
        {
            qCCritical(KALARM_LOG) << "KAMail::send: Error compiling message:" << err;
            errmsgs = errors(err);
            return -1;
        }

        // Execute the send command
        FILE* fd = ::popen(command.toLocal8Bit().constData(), "w");
        if (!fd)
        {
            qCCritical(KALARM_LOG) << "KAMail::send: Unable to open a pipe to " << command;
            errmsgs = errors();
            return -1;
        }
        message->assemble();
        QByteArray encoded = message->encodedContent();
        fwrite(encoded.constData(), encoded.length(), 1, fd);
        pclose(fd);

        if (jobdata.allowNotify)
            notifyQueued(jobdata.event);
        return 1;
    }
    else if (Preferences::useAkonadi())
    {
        qCDebug(KALARM_LOG) << "KAMail::send: Sending via KDE";
        initHeaders(*message, jobdata);
        err = appendBodyAttachments(*message, jobdata);
        if (!err.isNull())
        {
            qCCritical(KALARM_LOG) << "KAMail::send: Error compiling message:" << err;
            errmsgs = errors(err);
            return -1;
        }

        if (!mAkonadiPlugin)
        {
            mAkonadiPlugin = Preferences::akonadiPlugin();
            connect(mAkonadiPlugin, &AkonadiPlugin::emailSent, instance(), &KAMail::akonadiEmailSent);
            connect(mAkonadiPlugin, &AkonadiPlugin::emailQueued, instance(), [](const KAEvent& e) { notifyQueued(e); });
        }
        const bool keepEmail = (Preferences::emailClient() == Preferences::kmail || Preferences::emailCopyToKMail());
        err = mAkonadiPlugin->sendMail(message, identity, extractEmailAndNormalize(jobdata.from), keepEmail, jobdata);
        if (!err.isEmpty())
        {
            errmsgs = errors(err);
            return -1;
        }
    }
    return 0;
}

/******************************************************************************
* Called when sending an email via Akonadi is complete.
*/
void KAMail::akonadiEmailSent(const JobData& jobdata, const QStringList& errmsgs, bool sendError)
{
    QStringList messages = errmsgs;
    if (!errmsgs.isEmpty()  &&  sendError)
        messages = errors(errmsgs.at(0), SEND_ERROR);
    theApp()->emailSent(jobdata, messages);
}

/******************************************************************************
* Append the body and attachments to the email text.
* Reply = reason for error
*       = empty string if successful.
*/
QString KAMail::appendBodyAttachments(KMime::Message& message, JobData& data)
{
    const QStringList attachments = data.event.emailAttachments();
    if (attachments.isEmpty())
    {
        // There are no attachments, so simply append the message body
        message.contentType()->setMimeType("text/plain");
        message.contentType()->setCharset("utf-8");
        message.fromUnicodeString(data.event.message());
        auto encodings = KMime::encodingsForData(message.body());
        encodings.removeAll(KMime::Headers::CE8Bit);  // not handled by KMime
        message.contentTransferEncoding()->setEncoding(encodings.at(0));
        message.assemble();
    }
    else
    {
        // There are attachments, so the message must be in MIME format
        message.contentType()->setMimeType("multipart/mixed");
        message.contentType()->setBoundary(KMime::multiPartBoundary());

        if (!data.event.message().isEmpty())
        {
            // There is a message body
            auto content = new KMime::Content();
            content->contentType()->setMimeType("text/plain");
            content->contentType()->setCharset("utf-8");
            content->fromUnicodeString(data.event.message());
            auto encodings = KMime::encodingsForData(content->body());
            encodings.removeAll(KMime::Headers::CE8Bit);  // not handled by KMime
            content->contentTransferEncoding()->setEncoding(encodings.at(0));
            content->assemble();
            message.appendContent(content);
        }

        // Append each attachment in turn
        for (const QString& att : attachments)
        {
            const QString attachment = QString::fromLatin1(att.toLocal8Bit());
            const QUrl url = QUrl::fromUserInput(attachment, QString(), QUrl::AssumeLocalFile);
            const QString attachError = xi18nc("@info", "Error attaching file: <filename>%1</filename>", attachment);
            QByteArray contents;
            bool atterror = false;
            if (!url.isLocalFile())
            {
                auto statJob = KIO::stat(url, KIO::StatJob::SourceSide, KIO::StatDetail::StatDefaultDetails);
                KJobWidgets::setWindow(statJob, MainWindow::mainMainWindow());
                if (!statJob->exec())
                {
                    qCCritical(KALARM_LOG) << "KAMail::appendBodyAttachments: Not found:" << attachment;
                    return xi18nc("@info", "Attachment not found: <filename>%1</filename>", attachment);
                }
                KFileItem fi(statJob->statResult(), url);
                if (fi.isDir()  ||  !fi.isReadable())
                {
                    qCCritical(KALARM_LOG) << "KAMail::appendBodyAttachments: Not file/not readable:" << attachment;
                    return attachError;
                }

                // Read the file contents
                auto downloadJob = KIO::storedGet(url);
                KJobWidgets::setWindow(downloadJob, MainWindow::mainMainWindow());
                if (!downloadJob->exec())
                {
                    qCCritical(KALARM_LOG) << "KAMail::appendBodyAttachments: Load failure:" << attachment;
                    return attachError;
                }
                contents = downloadJob->data();
                if (static_cast<unsigned>(contents.size()) < fi.size())
                {
                    qCDebug(KALARM_LOG) << "KAMail::appendBodyAttachments: Read error:" << attachment;
                    atterror = true;
                }
            }
            else
            {
                QFile f(url.toLocalFile());
                if (!f.open(QIODevice::ReadOnly))
                {
                    qCCritical(KALARM_LOG) << "KAMail::appendBodyAttachments: Load failure:" << attachment;
                    return attachError;
                }
                contents = f.readAll();
            }

            QByteArray coded = KCodecs::base64Encode(contents);
            auto content = new KMime::Content();
            content->setEncodedBody(coded + "\n\n");

            // Set the content type
            QMimeDatabase mimeDb;
            QString typeName = mimeDb.mimeTypeForUrl(url).name();
            auto ctype = new KMime::Headers::ContentType;
            ctype->fromUnicodeString(typeName);
            ctype->setName(attachment);
            content->setHeader(ctype);

            // Set the encoding
            auto cte = new KMime::Headers::ContentTransferEncoding;
            cte->setEncoding(KMime::Headers::CEbase64);
            content->setHeader(cte);
            content->assemble();
            message.appendContent(content);
            if (atterror)
                return attachError;
        }
        message.assemble();
    }
    return {};
}

/******************************************************************************
* If any of the destination email addresses are non-local, display a
* notification message saying that an email has been queued for sending.
*/
void KAMail::notifyQueued(const KAEvent& event)
{
    KMime::Types::Address addr;
    const QString localhost = QStringLiteral("localhost");
    const QString hostname  = QHostInfo::localHostName();
    const KCalendarCore::Person::List addresses = event.emailAddressees();
    for (const KCalendarCore::Person& address : addresses)
    {
        const QByteArray email = address.email().toLocal8Bit();
        const char* em = email.constData();
        if (!email.isEmpty()
        &&  HeaderParsing::parseAddress(em, em + email.length(), addr))
        {
            const QString domain = addr.mailboxList.at(0).addrSpec().domain;
            if (!domain.isEmpty()  &&  domain != localhost  &&  domain != hostname)
            {
                KAMessageBox::information(MainWindow::mainMainWindow(), i18nc("@info", "An email has been queued to be sent"), QString(), Preferences::EMAIL_QUEUED_NOTIFY);
                return;
            }
        }
    }
}

/******************************************************************************
* Fetch the user's email address configured in KMail.
*/
QString KAMail::controlCentreAddress()
{
    KEMailSettings e;
    return e.getSetting(KEMailSettings::EmailAddress);
}

/******************************************************************************
* Parse a list of email addresses, optionally containing display names,
* entered by the user.
* Reply = the invalid item if error, else empty string.
*/
QString KAMail::convertAddresses(const QString& items, KCalendarCore::Person::List& list)
{
    list.clear();
    QString invalidItem;
    const KMime::Types::Mailbox::List mailboxes = parseAddresses(items, invalidItem);
    if (!invalidItem.isEmpty())
        return invalidItem;
    for (const KMime::Types::Mailbox& mailbox : mailboxes)
    {
        const KCalendarCore::Person person(mailbox.name(), mailbox.addrSpec().asString());
        list += person;
    }
    return {};
}

/******************************************************************************
* Check the validity of an email address.
* Because internal email addresses don't have to abide by the usual internet
* email address rules, only some basic checks are made.
* Reply = 1 if alright, 0 if empty, -1 if error.
*/
int KAMail::checkAddress(QString& address)
{
    address = address.trimmed();
    // Check that there are no list separator characters present
    if (address.indexOf(','_L1) >= 0  ||  address.indexOf(';'_L1) >= 0)
        return -1;
    const int n = address.length();
    if (!n)
        return 0;
    int start = 0;
    int end   = n - 1;
    if (address.at(end) == '>'_L1)
    {
        // The email address is in <...>
        if ((start = address.indexOf('<'_L1)) < 0)
            return -1;
        ++start;
        --end;
    }
    const int i = address.indexOf('@'_L1, start);
    if (i >= 0)
    {
        if (i == start  ||  i == end)          // check @ isn't the first or last character
//        ||  address.indexOf('@'_L1, i + 1) >= 0)    // check for multiple @ characters
            return -1;
    }
/*    else
    {
        // Allow the @ character to be missing if it's a local user
        if (!getpwnam(address.mid(start, end - start + 1).toLocal8Bit()))
            return false;
    }
    for (int i = start;  i <= end;  ++i)
    {
        char ch = address.at(i).toLatin1();
        if (ch == '.'  ||  ch == '@'  ||  ch == '-'  ||  ch == '_'
        ||  (ch >= 'A' && ch <= 'Z')  ||  (ch >= 'a' && ch <= 'z')
        ||  (ch >= '0' && ch <= '9'))
            continue;
        return false;
    }*/
    return 1;
}

/******************************************************************************
* Convert a comma or semicolon delimited list of attachments into a
* QStringList. The items are checked for validity.
* Reply = the invalid item if error, else empty string.
*/
QString KAMail::convertAttachments(const QString& items, QStringList& list)
{
    list.clear();
    int length = items.length();
    for (int next = 0;  next < length;  )
    {
        // Find the first delimiter character (, or ;)
        int i = items.indexOf(','_L1, next);
        if (i < 0)
            i = items.length();
        int sc = items.indexOf(';'_L1, next);
        if (sc < 0)
            sc = items.length();
        if (sc < i)
            i = sc;
        QString item = items.mid(next, i - next).trimmed();
        switch (checkAttachment(item))
        {
            case 1:   list += item;  break;
            case 0:   break;          // empty attachment name
            case -1:
            default:  return item;    // error
        }
        next = i + 1;
    }
    return {};
}

/******************************************************************************
* Check for the existence of the attachment file.
* If non-null, '*url' receives the QUrl of the attachment.
* Reply = 1 if attachment exists
*       = 0 if null name
*       = -1 if doesn't exist.
*/
int KAMail::checkAttachment(QString& attachment, QUrl* url)
{
    attachment = attachment.trimmed();
    if (attachment.isEmpty())
    {
        if (url)
            *url = QUrl();
        return 0;
    }
    // Check that the file exists
    QUrl u = QUrl::fromUserInput(attachment, QString(), QUrl::AssumeLocalFile);
    u.setPath(QDir::cleanPath(u.path()));
    if (url)
        *url = u;
    return checkAttachment(u) ? 1 : -1;
}

/******************************************************************************
* Check for the existence of the attachment file.
*/
bool KAMail::checkAttachment(const QUrl& url)
{
    auto statJob = KIO::stat(url);
    KJobWidgets::setWindow(statJob, MainWindow::mainMainWindow());
    if (!statJob->exec())
        return false;       // doesn't exist
    KFileItem fi(statJob->statResult(), url);
    if (fi.isDir()  ||  !fi.isReadable())
        return false;
    return true;
}

/******************************************************************************
* Set the appropriate error messages for a given error string.
*/
QStringList KAMail::errors(const QString& err, ErrType prefix)
{
    QString error1;
    switch (prefix)
    {
        case SEND_FAIL:   error1 = i18nc("@info", "Failed to send email");  break;
        case SEND_ERROR:  error1 = i18nc("@info", "Error sending email");  break;
    }
    if (err.isEmpty())
        return QStringList(error1);
    return QStringList{QStringLiteral("%1:").arg(error1), err};
}

namespace
{

/******************************************************************************
* Create the headers part of the email.
*/
void initHeaders(KMime::Message& message, JobData& data)
{
    auto date = new KMime::Headers::Date;
    date->setDateTime(KADateTime::currentDateTime(Preferences::timeSpec()).qDateTime());
    message.setHeader(date);

    auto from = new KMime::Headers::From;
    from->fromUnicodeString(data.from);
    message.setHeader(from);

    auto to = new KMime::Headers::To;
    const KCalendarCore::Person::List toList = data.event.emailAddressees();
    for (const KCalendarCore::Person& who : toList)
        to->addAddress(who.email().toLatin1(), who.name());
    message.setHeader(to);

    if (!data.bcc.isEmpty())
    {
        auto bcc = new KMime::Headers::Bcc;
        bcc->fromUnicodeString(data.bcc);
        message.setHeader(bcc);
    }

    auto subject = new KMime::Headers::Subject;
    const QString str = data.event.emailSubject();
    subject->fromUnicodeString(str);
    message.setHeader(subject);

    auto agent = new KMime::Headers::UserAgent;
    agent->fromUnicodeString(KAboutData::applicationData().displayName() + QLatin1StringView("/" KALARM_VERSION));
    agent->setRFC2047Charset("us-ascii");
    message.setHeader(agent);

    auto id = new KMime::Headers::MessageID;
    id->generate(data.from.mid(data.from.indexOf('@'_L1) + 1).toLatin1());
    message.setHeader(id);
}

/******************************************************************************
* Extract the pure addresses from given email addresses.
*/
QString extractEmailAndNormalize(const QString& emailAddress)
{
    return KEmailAddress::extractEmailAddress(KEmailAddress::normalizeAddressesAndEncodeIdn(emailAddress));
}

/******************************************************************************
* Parse a string containing multiple addresses, separated by comma or semicolon,
* while retaining Unicode name parts.
* Note that this only needs to parse strings input into KAlarm, so it only
* needs to accept the common syntax for email addresses, not obsolete syntax.
*/
KMime::Types::Mailbox::List parseAddresses(const QString& text, QString& invalidItem)
{
    KMime::Types::Mailbox::List list;
    int state     = 0;
    int start     = 0;  // start of this item
    int endName   = 0;  // character after end of name
    int startAddr = 0;  // start of address
    int endAddr   = 0;  // character after end of address
    char lastch = '\0';
    bool ended    = false;   // found the end of the item
    for (int i = 0, count = text.length();  i <= count;  ++i)
    {
        if (i == count)
            ended = true;
        else
        {
            const char ch = text[i].toLatin1();
            switch (state)
            {
                case 0:   // looking for start of item
                    if (ch == ' ' || ch == '\t')
                        continue;
                    start = i;
                    state = (ch == '"') ? 10 : 1;
                    break;
                case 1:   // looking for start of address, or end of item
                    switch (ch)
                    {
                        case '<':
                            startAddr = i + 1;
                            state = 2;
                            break;
                        case ',':
                        case ';':
                            ended = true;
                            break;
                        case ' ':
                            break;
                        default:
                            endName = i + 1;
                            break;
                    }
                    break;
                case 2:   // looking for '>' at end of address
                    if (ch == '>')
                    {
                        endAddr = i;
                        state = 3;
                    }
                    break;
                case 3:   // looking for item separator
                    if (ch == ','  ||  ch == ';')
                        ended = true;
                    else if (ch != ' ')
                    {
                        invalidItem = text.mid(start);
                        return {};
                    }
                    break;
                case 10:   // looking for closing quote
                    if (ch == '"'  &&  lastch != '\\')
                    {
                        ++start;   // remove opening quote from name
                        endName = i;
                        state = 11;
                    }
                    lastch = ch;
                    break;
                case 11:   // looking for '<'
                    if (ch == '<')
                    {
                        startAddr = i + 1;
                        state = 2;
                    }
                    break;
            }
        }
        if (ended)
        {
            // Found the end of the item - add it to the list
            if (!startAddr)
            {
                startAddr = start;
                endAddr   = endName;
                endName   = 0;
            }
            const QString addr = text.mid(startAddr, endAddr - startAddr);
            KMime::Types::Mailbox mbox;
            mbox.fromUnicodeString(addr);
            if (mbox.address().isEmpty())
            {
                invalidItem = text.mid(start, endAddr - start);
                return {};
            }
            if (endName)
            {
                int len = endName - start;
                QString name = text.mid(start, endName - start);
                if (name.at(0) == '"'_L1  &&  name.at(len - 1) == '"'_L1)
                    name = name.mid(1, len - 2);
                mbox.setName(name);
            }
            list.append(mbox);

            endName = startAddr = endAddr = 0;
            start = i + 1;
            state = 0;
            ended = false;
        }
    }
    return list;
}

} // namespace

/*=============================================================================
=  HeaderParsing :  modified and additional functions.
=  The following functions are modified from, or additional to, those in
=  libkmime kmime_header_parsing.cpp.
=============================================================================*/

namespace HeaderParsing
{

using namespace KMime;
using namespace KMime::Types;
using namespace KMime::HeaderParsing;

/******************************************************************************
* New function.
* Allow a local user name to be specified as an email address.
*/
bool parseUserName(const char* & scursor, const char* const send, QString& result, bool isCRLF)
{
    QByteArrayView atom;

    if (scursor != send)
    {
        // first, eat any whitespace
        eatCFWS(scursor, send, isCRLF);

        char ch = *scursor++;
        switch (ch)
        {
            case '.': // dot
            case '@':
            case '"': // quoted-string
                return false;

            default: // atom
                scursor--; // re-set scursor to point to ch again
                if (parseAtom(scursor, send, atom, false /* no 8bit */))
                {
                    //TODO FIXME on windows
#ifndef WIN32
                    if (getpwnam(atom.toByteArray().constData()))
                    {
                        result = QLatin1StringView(atom);
                        return true;
                    }
#endif
                }
                return false; // parseAtom can only fail if the first char is non-atext.
        }
    }
    return false;
}

/******************************************************************************
* Modified function.
* Allow a local user name to be specified as an email address, and reinstate
* the original scursor on error return.
*/
bool parseAddress(const char* & scursor, const char* const send, Address& result, bool isCRLF)
{
    // address       := mailbox / group

    eatCFWS(scursor, send, isCRLF);
    if (scursor == send)
        return false;

    // first try if it's a single mailbox:
    Mailbox maybeMailbox;
    const char * oldscursor = scursor;
    if (parseMailbox(scursor, send, maybeMailbox, isCRLF))
    {
        // yes, it is:
        result.setDisplayName({});
        result.mailboxList.append(maybeMailbox);
        return true;
    }
    scursor = oldscursor;

    // KAlarm: Allow a local user name to be specified
    // no, it's not a single mailbox. Try if it's a local user name:
    QString maybeUserName;
    if (parseUserName(scursor, send, maybeUserName, isCRLF))
    {
        // yes, it is:
        maybeMailbox.setName(QString());
        AddrSpec addrSpec;
        addrSpec.localPart = maybeUserName;
        addrSpec.domain.clear();
        maybeMailbox.setAddress(addrSpec);
        result.setDisplayName({});
        result.mailboxList.append(maybeMailbox);
        return true;
    }
    scursor = oldscursor;

    Address maybeAddress;

    // no, it's not a single mailbox. Try if it's a group:
    if (!parseGroup(scursor, send, maybeAddress, isCRLF))
    {
        scursor = oldscursor;   // KAlarm: reinstate original scursor on error return
        return false;
    }

    result = maybeAddress;
    return true;
}

} // namespace HeaderParsing

#include "moc_kamail.cpp"

// vim: et sw=4:
