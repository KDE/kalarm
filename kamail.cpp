/*
 *  kamail.cpp  -  email functions
 *  Program:  kalarm
 *  Copyright © 2002-2010 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"   //krazy:exclude=includes (kalarm.h must be first)
#include "kamail.h"

#include "functions.h"
#include "identities.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "preferences.h"

#include <kpimidentities/identitymanager.h>
#include <kpimidentities/identity.h>
#include <kpimutils/email.h>
#include <mailtransport/transportmanager.h>
#include <mailtransport/transport.h>
#include <mailtransport/transportjob.h>
#include <kcal/person.h>
#include <kmime/kmime_header_parsing.h>
#include <kmime/kmime_headers.h>
#include <kmime/kmime_message.h>

#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kfileitem.h>
#include <kio/netaccess.h>
#include <ktemporaryfile.h>
#include <kemailsettings.h>
#include <k3resolver.h>
#include <kcodecs.h>
#include <kcharsets.h>
#include <kascii.h>
#include <kdebug.h>

#include <QFile>
#include <QList>
#include <QRegExp>
#include <QByteArray>
#include <QTextStream>
#include <QTextCodec>
#include <QtDBus/QtDBus>

#include <pwd.h>

#ifdef KMAIL_SUPPORTED
#include "kmailinterface.h"

static const char* KMAIL_DBUS_SERVICE = "org.kde.kmail";
static const char* KMAIL_DBUS_PATH    = "/KMail";
#endif

namespace HeaderParsing
{
bool parseAddress( const char* & scursor, const char * const send,
                   KMime::Types::Address & result, bool isCRLF=false );
}

static void initHeaders(KMime::Message&, KAMail::JobData&);
static KMime::Types::Mailbox::List parseAddresses(const QString& text, QString& invalidItem);
static QByteArray autoDetectCharset(const QString& text);
static const QTextCodec* codecForName(const QByteArray& str);

QString KAMail::i18n_NeedFromEmailAddress()
{ return i18nc("@info/plain", "A 'From' email address must be configured in order to execute email alarms."); }

QString KAMail::i18n_sent_mail()
{ return i18nc("@info/plain KMail folder name: this should be translated the same as in kmail", "sent-mail"); }

KAMail*                              KAMail::mInstance = 0;   // used only to enable signals/slots to work
QQueue<MailTransport::TransportJob*> KAMail::mJobs;
QQueue<KAMail::JobData>              KAMail::mJobData;

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
	KPIMIdentities::Identity identity;
	if (!jobdata.event.emailFromId())
		jobdata.from = Preferences::emailAddress();
	else
	{
		identity = Identities::identityManager()->identityForUoid(jobdata.event.emailFromId());
		if (identity.isNull())
		{
			kError() << "Identity" << jobdata.event.emailFromId() << "not found";
			errmsgs = errors(i18nc("@info", "Invalid 'From' email address.<nl/>Email identity <resource>%1</resource> not found", jobdata.event.emailFromId()));
			return -1;
		}
		jobdata.from = identity.fullEmailAddr();
		if (jobdata.from.isEmpty())
		{
			kError() << "Identity" << identity.identityName() << "uoid" << identity.uoid() << ": no email address";
			errmsgs = errors(i18nc("@info", "Invalid 'From' email address.<nl/>Email identity <resource>%1</resource> has no email address", identity.identityName()));
			return -1;
		}
	}
	if (jobdata.from.isEmpty())
	{
		switch (Preferences::emailFrom())
		{
#ifdef KMAIL_SUPPORTED
			case Preferences::MAIL_FROM_KMAIL:
				errmsgs = errors(i18nc("@info", "<para>No 'From' email address is configured (no default email identity found)</para>"
				                                "<para>Please set it in <application>KMail</application> or in the <application>KAlarm</application> Configuration dialog.</para>"));
				break;
#endif
			case Preferences::MAIL_FROM_SYS_SETTINGS:
				errmsgs = errors(i18nc("@info", "<para>No 'From' email address is configured.</para>"
				                                "<para>Please set it in the KDE System Settings or in the <application>KAlarm</application> Configuration dialog.</para>"));
				break;
			case Preferences::MAIL_FROM_ADDR:
			default:
				errmsgs = errors(i18nc("@info", "<para>No 'From' email address is configured.</para>"
				                                "<para>Please set it in the <application>KAlarm</application> Configuration dialog.</para>"));
				break;
		}
		return -1;
	}
	jobdata.bcc  = (jobdata.event.emailBcc() ? Preferences::emailBccAddress() : QString());
	kDebug() << "To:" << jobdata.event.emailAddresses(",")
	              << endl << "Subject:" << jobdata.event.emailSubject();

	MailTransport::TransportManager* manager = MailTransport::TransportManager::self();
	MailTransport::Transport* transport = 0;
	if (Preferences::emailClient() == Preferences::sendmail)
	{
		kDebug() << "Sending via sendmail";
		transport = manager->transportByName(QLatin1String("sendmail"), false);
		if (!transport)
		{
			QString command = KStandardDirs::findExe(QLatin1String("sendmail"),
			                                         QLatin1String("/sbin:/usr/sbin:/usr/lib"));
			transport = manager->createTransport();
			transport->setName(QLatin1String("sendmail"));
			transport->setType(MailTransport::Transport::EnumType::Sendmail);
			transport->setHost(command);
			transport->setRequiresAuthentication(false);
			transport->setStorePassword(false);
			manager->addTransport(transport);
			transport->writeConfig();
			kDebug() << "Creating sendmail transport, id=" << transport->id();
		}
	}
	else
	{
		kDebug() << "Sending via KDE";
		transport = manager->transportByName(identity.transport(), true);
		if (!transport)
		{
			kError() << "No mail transport found for identity" << identity.identityName() << "uoid" << identity.uoid();
			errmsgs = errors(i18nc("@info", "No mail transport configured for email identity <resource>%1</resource>", identity.identityName()));
			return -1;
		}
	}
	int transportId = transport->id();
	MailTransport::TransportJob* mailjob = manager->createTransportJob(transportId);
	if (!mailjob)
	{
		kError() << "Failed to create mail transport job for identity" << identity.identityName() << "uoid" << identity.uoid();
		errmsgs = errors(i18nc("@info", "Unable to create mail transport job"));
		return -1;
	}
	KMime::Message message;
	initHeaders(message, jobdata);
	err = appendBodyAttachments(message, jobdata);
	if (!err.isNull())
	{
		kError() << "Error compiling message:" << err;
		errmsgs = errors(err);
		return -1;
	}
	mailjob->setSender(KPIMUtils::extractEmailAddress(jobdata.from));
	mailjob->setTo(jobdata.event.emailPureAddresses());
	if (!jobdata.bcc.isEmpty())
		mailjob->setBcc(QStringList(KPIMUtils::extractEmailAddress(jobdata.bcc)));
	mailjob->setData(message.encodedContent());
	mJobs.enqueue(mailjob);
	mJobData.enqueue(jobdata);
	if (mJobs.count() == 1)
	{
		// There are no jobs already active or queued, so send now
		connect(mailjob, SIGNAL(result(KJob*)), instance(), SLOT(slotEmailSent(KJob*)));
		mailjob->start();
	}
	return 0;
}

/******************************************************************************
* Called when sending an email is complete.
*/
void KAMail::slotEmailSent(KJob* job)
{
	bool fail = false;
	bool copyerr = false;
	QStringList errmsgs;
	if (job->error())
	{
		kError() << "Failed:" << job->errorString();
		errmsgs = errors(job->errorString(), SEND_ERROR);
		fail = true;
	}
	JobData jobdata;
	if (mJobs.isEmpty()  ||  mJobData.isEmpty()  ||  job != mJobs.head())
	{
		// The queue has been corrupted, so we can't locate the job's data
		kError() << "Wrong job at head of queue: wiping queue";
		mJobs.clear();
		mJobData.clear();
		if (!errmsgs.isEmpty())
			theApp()->emailSent(jobdata, errmsgs);
		errmsgs.clear();
		errmsgs += i18nc("@info", "Emails may not have been sent");
		errmsgs += i18nc("@info", "Program error");
		theApp()->emailSent(jobdata, errmsgs);
		return;
	}
	mJobs.dequeue();
	jobdata = mJobData.dequeue();

#ifdef KMAIL_SUPPORTED
	if (Preferences::emailClient() == Preferences::sendmail
	&&  Preferences::emailCopyToKMail())
	{
		// Create a copy of the sent email in KMail's 'sent-mail' folder,
		// or if there was a send error, in KMail's 'outbox' folder.
		QString err = addToKMailFolder(jobdata, (fail ? "outbox" : "sent-mail"), true);
		if (!err.isNull())
		{
			kWarning() << "Error copying to KMail:" << err;
			errmsgs << errors(err, COPY_ERROR);    // not a fatal error - continue
			if (!fail)
				copyerr = true;
		}
	}
#endif

	if (jobdata.allowNotify)
		notifyQueued(jobdata.event);
	theApp()->emailSent(jobdata, errmsgs, copyerr);
	if (!mJobs.isEmpty())
	{
		// Send the next queued email
		connect(mJobs.head(), SIGNAL(result(KJob*)), instance(), SLOT(slotEmailSent(KJob*)));
		mJobs.head()->start();
	}
}

#ifdef KMAIL_SUPPORTED
/******************************************************************************
* Add the message to a KMail folder.
* Reply = reason for failure (which may be the empty string)
*       = null string if success.
*/
QString KAMail::addToKMailFolder(JobData& data, const char* folder, bool checkKmailRunning)
{
	QString err;
	if (checkKmailRunning)
		err = KAlarm::runKMail(true);
	if (err.isNull())
	{
		KMime::Message message;
		initHeaders(message, data);
		err = appendBodyAttachments(message, data);
		if (!err.isNull())
			return err;

		// Write to a temporary file for feeding to KMail
		KTemporaryFile tmpFile;
		if (!tmpFile.open())
		{
			kError() << folder << ": Unable to open a temporary mail file";
			return QString("");
		}
		tmpFile.setTextModeEnabled(true);
		tmpFile.write(message.encodedContent());
		if (tmpFile.error() != QFile::NoError)
		{
			kError() << folder << ": Error" << tmpFile.errorString() << " writing to temporary mail file";
			tmpFile.close();
			return QString("");
		}
		tmpFile.close();

		// Notify KMail of the message in the temporary file
		org::kde::kmail::kmail kmail(KMAIL_DBUS_SERVICE, KMAIL_DBUS_PATH, QDBusConnection::sessionBus());
		QDBusReply<int> reply = kmail.dbusAddMessage(QString::fromLatin1(folder), tmpFile.fileName(), QString());
		if (!reply.isValid())
			kError() << "D-Bus call failed:" << reply.error().message();
		else if (reply.value() <= 0)
			kError() << "D-Bus call returned error code =" << reply.value();
		else
			return QString();    // success
		err = i18nc("@info", "Error calling <application>KMail</application>");
	}
	kError() << folder << ":" << err;
	return err;
}
#endif // KMAIL_SUPPORTED

/******************************************************************************
* Create the headers part of the email.
*/
void initHeaders(KMime::Message& message, KAMail::JobData& data)
{
	KMime::Headers::Date* date = new KMime::Headers::Date;
	date->setDateTime(KDateTime::currentDateTime(Preferences::timeZone()));
	message.setHeader(date);

	KMime::Headers::From* from = new KMime::Headers::From;
	from->fromUnicodeString(data.from, autoDetectCharset(data.from));
	message.setHeader(from);

	KMime::Headers::To* to = new KMime::Headers::To;
	EmailAddressList toList = data.event.emailAddresses();
	for (int i = 0, count = toList.count();  i < count;  ++i)
		to->addAddress(toList[i].email().toLatin1(), toList[i].name());
	message.setHeader(to);

	if (!data.bcc.isEmpty())
	{
		KMime::Headers::Bcc* bcc = new KMime::Headers::Bcc;
		bcc->fromUnicodeString(data.bcc, autoDetectCharset(data.bcc));
		message.setHeader(bcc);
	}

	KMime::Headers::Subject* subject = new KMime::Headers::Subject;
	QString str = data.event.emailSubject();
	subject->fromUnicodeString(str, autoDetectCharset(str));
	message.setHeader(subject);

	KMime::Headers::UserAgent* agent = new KMime::Headers::UserAgent;
	agent->fromUnicodeString(KGlobal::mainComponent().aboutData()->programName() + "/" KALARM_VERSION, "us-ascii");
	message.setHeader(agent);

	KMime::Headers::MessageID* id = new KMime::Headers::MessageID;
	id->generate(data.from.mid(data.from.indexOf('@') + 1).toLatin1());
	message.setHeader(id);
}

/******************************************************************************
* Append the body and attachments to the email text.
* Reply = reason for error
*       = empty string if successful.
*/
QString KAMail::appendBodyAttachments(KMime::Message& message, JobData& data)
{
	QStringList attachments = data.event.emailAttachments();
	if (!attachments.count())
	{
		// There are no attachments, so simply append the message body
		message.contentType()->setMimeType("text/plain");
		message.contentType()->setCharset("utf-8");
		message.fromUnicodeString(data.event.message());
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
			KMime::Content* content = new KMime::Content();
			content->contentType()->setMimeType("text/plain");
			content->contentType()->setCharset("utf-8");
			content->fromUnicodeString(data.event.message());
			content->assemble();
			message.addContent(content);
		}

		// Append each attachment in turn
		for (QStringList::Iterator at = attachments.begin();  at != attachments.end();  ++at)
		{
			QString attachment = (*at).toLocal8Bit();
			KUrl url(attachment);
			QString attachError = i18nc("@info", "Error attaching file: <filename>%1</filename>", attachment);
			url.cleanPath();
			KIO::UDSEntry uds;
			if (!KIO::NetAccess::stat(url, uds, MainWindow::mainMainWindow())) {
				kError() << "Not found:" << attachment;
				return i18nc("@info", "Attachment not found: <filename>%1</filename>", attachment);
			}
			KFileItem fi(uds, url);
			if (fi.isDir()  ||  !fi.isReadable()) {
				kError() << "Not file/not readable:" << attachment;
				return attachError;
			}

			// Read the file contents
			QString tmpFile;
			if (!KIO::NetAccess::download(url, tmpFile, MainWindow::mainMainWindow())) {
				kError() << "Load failure:" << attachment;
				return attachError;
			}
			QFile file(tmpFile);
			if (!file.open(QIODevice::ReadOnly)) {
				kDebug() << "tmp load error:" << attachment;
				return attachError;
			}
			qint64 size = file.size();
			QByteArray contents = file.readAll();
			file.close();
			bool atterror = false;
			if (contents.size() < size) {
				kDebug() << "Read error:" << attachment;
				atterror = true;
			}

			QByteArray coded = KCodecs::base64Encode(contents, true);
			KMime::Content* content = new KMime::Content();
			content = new KMime::Content();
			content->setBody(coded + "\n\n");

			// Set the content type
			KMimeType::Ptr type = KMimeType::findByUrl(url);
			KMime::Headers::ContentType* ctype = new KMime::Headers::ContentType(content);
			ctype->fromUnicodeString(type->name(), autoDetectCharset(type->name()));
			ctype->setName(attachment, "local");
			content->setHeader(ctype);

			// Set the encoding
			KMime::Headers::ContentTransferEncoding* cte = new KMime::Headers::ContentTransferEncoding(content);
			cte->setEncoding(KMime::Headers::CEbase64);
			cte->setDecoded(false);
			content->setHeader(cte);
			content->assemble();
			message.addContent(content);
			if (atterror)
				return attachError;
		}
		message.assemble();
	}
	return QString();
}

/******************************************************************************
* If any of the destination email addresses are non-local, display a
* notification message saying that an email has been queued for sending.
*/
void KAMail::notifyQueued(const KAEvent& event)
{
	KMime::Types::Address addr;
	QString localhost = QLatin1String("localhost");
	QString hostname  = KNetwork::KResolver::localHostName();
	const EmailAddressList& addresses = event.emailAddresses();
	for (int i = 0, end = addresses.count();  i < end;  ++i)
	{
		QByteArray email = addresses[i].email().toLocal8Bit();
		const char* em = email;
		if (!email.isEmpty()
		&&  HeaderParsing::parseAddress(em, em + email.length(), addr))
		{
			QString domain = addr.mailboxList.first().addrSpec().domain;
			if (!domain.isEmpty()  &&  domain != localhost  &&  domain != hostname)
			{
				KMessageBox::information(0, i18nc("@info", "An email has been queued to be sent"), QString(), Preferences::EMAIL_QUEUED_NOTIFY);
				return;
			}
		}
	}
}

/******************************************************************************
*  Fetch the user's email address configured in the KDE System Settings.
*/
QString KAMail::controlCentreAddress()
{
	KEMailSettings e;
	return e.getSetting(KEMailSettings::EmailAddress);
}

/******************************************************************************
*  Parse a list of email addresses, optionally containing display names,
*  entered by the user.
*  Reply = the invalid item if error, else empty string.
*/
QString KAMail::convertAddresses(const QString& items, EmailAddressList& list)
{
	list.clear();
	QString invalidItem;
	KMime::Types::Mailbox::List mailboxes = parseAddresses(items, invalidItem);
	if (!invalidItem.isEmpty())
		return invalidItem;
	for (int i = 0, count = mailboxes.count();  i < count;  ++i)
		list += KCal::Person(mailboxes[i].name(), QString::fromLatin1(mailboxes[i].address()));
	return QString();
}

/******************************************************************************
*  Check the validity of an email address.
*  Because internal email addresses don't have to abide by the usual internet
*  email address rules, only some basic checks are made.
*  Reply = 1 if alright, 0 if empty, -1 if error.
*/
int KAMail::checkAddress(QString& address)
{
	address = address.trimmed();
	// Check that there are no list separator characters present
	if (address.indexOf(QLatin1Char(',')) >= 0  ||  address.indexOf(QLatin1Char(';')) >= 0)
		return -1;
	int n = address.length();
	if (!n)
		return 0;
	int start = 0;
	int end   = n - 1;
	if (address[end] == QLatin1Char('>'))
	{
		// The email address is in <...>
		if ((start = address.indexOf(QLatin1Char('<'))) < 0)
			return -1;
		++start;
		--end;
	}
	int i = address.indexOf(QLatin1Char('@'), start);
	if (i >= 0)
	{
		if (i == start  ||  i == end)          // check @ isn't the first or last character
//		||  address.indexOf(QLatin1Char('@'), i + 1) >= 0)    // check for multiple @ characters
			return -1;
	}
/*	else
	{
		// Allow the @ character to be missing if it's a local user
		if (!getpwnam(address.mid(start, end - start + 1).toLocal8Bit()))
			return false;
	}
	for (int i = start;  i <= end;  ++i)
	{
		char ch = address[i].toLatin1();
		if (ch == '.'  ||  ch == '@'  ||  ch == '-'  ||  ch == '_'
		||  (ch >= 'A' && ch <= 'Z')  ||  (ch >= 'a' && ch <= 'z')
		||  (ch >= '0' && ch <= '9'))
			continue;
		return false;
	}*/
	return 1;
}

/******************************************************************************
*  Convert a comma or semicolon delimited list of attachments into a
*  QStringList. The items are checked for validity.
*  Reply = the invalid item if error, else empty string.
*/
QString KAMail::convertAttachments(const QString& items, QStringList& list)
{
	KUrl url;
	list.clear();
	int length = items.length();
	for (int next = 0;  next < length;  )
	{
		// Find the first delimiter character (, or ;)
		int i = items.indexOf(QLatin1Char(','), next);
		if (i < 0)
			i = items.length();
		int sc = items.indexOf(QLatin1Char(';'), next);
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
	return QString();
}

/******************************************************************************
*  Check for the existence of the attachment file.
*  If non-null, '*url' receives the KUrl of the attachment.
*  Reply = 1 if attachment exists
*        = 0 if null name
*        = -1 if doesn't exist.
*/
int KAMail::checkAttachment(QString& attachment, KUrl* url)
{
	attachment = attachment.trimmed();
	if (attachment.isEmpty())
	{
		if (url)
			*url = KUrl();
		return 0;
	}
	// Check that the file exists
	KUrl u(attachment);
	u.cleanPath();
	if (url)
		*url = u;
	return checkAttachment(u) ? 1 : -1;
}

/******************************************************************************
*  Check for the existence of the attachment file.
*/
bool KAMail::checkAttachment(const KUrl& url)
{
	KIO::UDSEntry uds;
	if (!KIO::NetAccess::stat(url, uds, MainWindow::mainMainWindow()))
		return false;       // doesn't exist
	KFileItem fi(uds, url);
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
		case SEND_FAIL:  error1 = i18nc("@info", "Failed to send email");  break;
		case SEND_ERROR:  error1 = i18nc("@info", "Error sending email");  break;
		case COPY_ERROR:  error1 = i18nc("@info", "Error copying sent email to <application>KMail</application> <resource>%1</resource> folder", i18n_sent_mail());  break;
	}
kDebug()<<err<<","<<error1;
	if (err.isEmpty())
		return QStringList(error1);
	QStringList errs(QString::fromLatin1("%1:").arg(error1));
	errs += err;
	return errs;
}

#ifdef KMAIL_SUPPORTED
/******************************************************************************
*  Get the body of an email from KMail, given its serial number.
*/
QString KAMail::getMailBody(quint32 serialNumber)
{
	QList<QVariant> args;
	args << serialNumber << (int)0;
#ifdef __GNUC__
#warning Set correct DBus interface/object for kmail
#endif
	QDBusInterface iface(KMAIL_DBUS_SERVICE, QString(), QLatin1String("KMailIface"));
	QDBusReply<QString> reply = iface.callWithArgumentList(QDBus::Block, QLatin1String("getDecodedBodyPart"), args);
	if (!reply.isValid())
	{
		kError() << "D-Bus call failed:" << reply.error().message();
		return QString();
	}
	return reply.value();
}
#endif

//-----------------------------------------------------------------------------
// Based on KMail KMMsgBase::autoDetectCharset().
QByteArray autoDetectCharset(const QString& text)
{
    static QList<QByteArray> charsets;
    if (charsets.isEmpty())
        charsets << "us-ascii" << "iso-8859-1" << "locale" << "utf-8";

    for (int i = 0, count = charsets.count();  i < count;  ++i)
    {
        QByteArray encoding = charsets[i];
        if (encoding == "locale")
        {
            encoding = QTextCodec::codecForName(KGlobal::locale()->encoding())->name();
            kAsciiToLower(encoding.data());
        }
        if (text.isEmpty())
            return encoding;
        if (encoding == "us-ascii")
        {
            if (KMime::isUsAscii(text))
                return encoding;
        }
        else
        {
            const QTextCodec *codec = codecForName(encoding);
            if (!codec)
                kDebug() <<"Auto-Charset: Something is wrong and I can not get a codec. [" << encoding <<"]";
            else
            {
                 if (codec->canEncode(text))
                     return encoding;
            }
        }
    }
    return 0;
}

//-----------------------------------------------------------------------------
// Based on KMail KMMsgBase::codecForName().
const QTextCodec* codecForName(const QByteArray& str)
{
    if (str.isEmpty())
        return 0;
    QByteArray codec = str;
    kAsciiToLower(codec.data());
    return KGlobal::charsets()->codecForName(codec);
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
            char ch = text[i].toLatin1();
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
                        case '"':
                            lastch = ch;
                            state = 0x10;
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
                        return KMime::Types::Mailbox::List();
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
            QString addr = text.mid(startAddr, endAddr - startAddr);
            KMime::Types::Mailbox mbox;
            mbox.fromUnicodeString(addr);
            if (mbox.address().isEmpty())
            {
                invalidItem = text.mid(start, endAddr - start);
                return KMime::Types::Mailbox::List();
            }
            if (endName)
            {
                int len = endName - start;
                QString name = text.mid(start, endName - start);
                if (name[0] == '"'  &&  name[len - 1] == '"')
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
*  New function.
*  Allow a local user name to be specified as an email address.
*/
bool parseUserName( const char* & scursor, const char * const send,
                    QString & result, bool isCRLF ) {

  QString maybeLocalPart;
  QString tmp;

  if ( scursor != send ) {
    // first, eat any whitespace
    eatCFWS( scursor, send, isCRLF );

    char ch = *scursor++;
    switch ( ch ) {
    case '.': // dot
    case '@':
    case '"': // quoted-string
      return false;

    default: // atom
      scursor--; // re-set scursor to point to ch again
      tmp.clear();
      if ( parseAtom( scursor, send, result, false /* no 8bit */ ) ) {
        if (getpwnam(result.toLocal8Bit()))
          return true;
      }
      return false; // parseAtom can only fail if the first char is non-atext.
    }
  }
  return false;
}

/******************************************************************************
*  Modified function.
*  Allow a local user name to be specified as an email address, and reinstate
*  the original scursor on error return.
*/
bool parseAddress( const char* & scursor, const char * const send,
                   Address & result, bool isCRLF ) {
  // address       := mailbox / group

  eatCFWS( scursor, send, isCRLF );
  if ( scursor == send )
    return false;

  // first try if it's a single mailbox:
  Mailbox maybeMailbox;
  const char * oldscursor = scursor;
  if ( parseMailbox( scursor, send, maybeMailbox, isCRLF ) ) {
    // yes, it is:
    result.displayName.clear();
    result.mailboxList.append( maybeMailbox );
    return true;
  }
  scursor = oldscursor;

  // KAlarm: Allow a local user name to be specified
  // no, it's not a single mailbox. Try if it's a local user name:
  QString maybeUserName;
  if ( parseUserName( scursor, send, maybeUserName, isCRLF ) ) {
    // yes, it is:
    maybeMailbox.setName( QString() );
    AddrSpec addrSpec;
    addrSpec.localPart = maybeUserName;
    addrSpec.domain.clear();
    maybeMailbox.setAddress( addrSpec );
    result.displayName.clear();
    result.mailboxList.append( maybeMailbox );
    return true;
  }
  scursor = oldscursor;

  Address maybeAddress;

  // no, it's not a single mailbox. Try if it's a group:
  if ( !parseGroup( scursor, send, maybeAddress, isCRLF ) )
  {
    scursor = oldscursor;   // KAlarm: reinstate original scursor on error return
    return false;
  }

  result = maybeAddress;
  return true;
}

} // namespace HeaderParsing
