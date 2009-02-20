/*
 *  kamail.cpp  -  email functions
 *  Program:  kalarm
 *  Copyright © 2002-2009 by David Jarvie <djarvie@kde.org>
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

#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pwd.h>

#include <QFile>
#include <QList>
#include <QRegExp>
#include <QByteArray>
#include <QTextStream>
#include <QtDBus/QtDBus>

#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kshell.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kfileitem.h>
#include <kio/netaccess.h>
#include <ktemporaryfile.h>
#include <kemailsettings.h>
#include <kdebug.h>
#include <k3resolver.h>

#include <kpimidentities/identitymanager.h>
#include <kpimidentities/identity.h>
#include <kpimutils/email.h>
#include <mailtransport/transportmanager.h>
#include <mailtransport/transport.h>
#include <mailtransport/transportjob.h>
#include <kcal/person.h>

#include <kmime/kmime_header_parsing.h>

#ifdef KMAIL_SUPPORTED
#include "kmailinterface.h"

static const char* KMAIL_DBUS_SERVICE = "org.kde.kmail";
static const char* KMAIL_DBUS_PATH    = "/KMail";
#endif


namespace HeaderParsing
{
bool parseAddress( const char* & scursor, const char * const send,
                   KMime::Types::Address & result, bool isCRLF=false );
bool parseAddressList( const char* & scursor, const char * const send,
                       QList<KMime::Types::Address> & result, bool isCRLF=false );
}


static QString initHeaders(const KAMail::JobData&, bool dateId);

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

	if (Preferences::emailClient() == Preferences::sendmail)
	{
		// Use sendmail to send the message
		QString textComplete;
		QString command = KStandardDirs::findExe(QLatin1String("sendmail"),
		                                         QLatin1String("/sbin:/usr/sbin:/usr/lib"));
		if (!command.isNull())
		{
			command += QLatin1String(" -f ");
			command += KPIMUtils::extractEmailAddress(jobdata.from);
			command += QLatin1String(" -oi -t ");
			textComplete = initHeaders(jobdata, false);
		}
		else
		{
			command = KStandardDirs::findExe(QLatin1String("mail"));
			if (command.isNull())
			{
				errmsgs = errors(i18nc("@info", "<command>%1</command> not found", QLatin1String("sendmail"))); // give up
				return -1;
			}

			command += QLatin1String(" -s ");
			command += KShell::quoteArg(jobdata.event.emailSubject());

			if (!jobdata.bcc.isEmpty())
			{
				command += QLatin1String(" -b ");
				command += KShell::quoteArg(jobdata.bcc);
			}

			command += ' ';
			command += jobdata.event.emailAddresses(" "); // locally provided, okay
		}

		// Add the body and attachments to the message.
		// (Sendmail requires attachments to have already been included in the message.)
		err = appendBodyAttachments(textComplete, jobdata.event);
		if (!err.isNull())
		{
			errmsgs = errors(err);
			return -1;
		}

		// Execute the send command
		FILE* fd = popen(command.toLocal8Bit(), "w");
		if (!fd)
		{
			kError() << "Unable to open a pipe to" << command;
			errmsgs = errors();
			return -1;
		}
		fwrite(textComplete.toLocal8Bit(), textComplete.length(), 1, fd);
		pclose(fd);

#ifdef KMAIL_SUPPORTED
		if (Preferences::emailCopyToKMail())
		{
			// Create a copy of the sent email in KMail's 'Sent-mail' folder
			err = addToKMailFolder(jobdata, "sent-mail", true);
			if (!err.isNull())
				errmsgs = errors(err, COPY_ERROR);    // not a fatal error - continue
		}
#endif

		if (jobdata.allowNotify)
			notifyQueued(jobdata.event);
		return 1;
	}
	else
	{
		// Use KDE to send the message
		MailTransport::TransportManager* manager = MailTransport::TransportManager::self();
		MailTransport::Transport* transport = manager->transportByName(identity.transport(), true);
		if (!transport)
		{
			kError() << "No mail transport found for identity" << identity.identityName() << "uoid" << identity.uoid();
			errmsgs = errors(i18nc("@info", "No mail transport configured for email identity <resource>%1</resource>", identity.identityName()));
			return -1;
		}
		int transportId = transport->id();
		MailTransport::TransportJob* mailjob = manager->createTransportJob(transportId);
		if (!mailjob)
		{
			kError() << "Failed to create mail transport job for identity" << identity.identityName() << "uoid" << identity.uoid();
			errmsgs = errors(i18nc("@info", "Unable to create mail transport job"));
			return -1;
		}
		QString message = initHeaders(jobdata, true);
		message += jobdata.event.message();
		err = appendBodyAttachments(message, jobdata.event);
		if (!err.isNull())
		{
			errmsgs = errors(err);
			return -1;
		}
		mailjob->setSender(jobdata.from);
		mailjob->setTo(QStringList(jobdata.event.emailAddresses()));
		if (!jobdata.bcc.isEmpty())
			mailjob->setBcc(QStringList(jobdata.bcc));
		mailjob->setData(message.toLocal8Bit());
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
}

/******************************************************************************
* Called when sending an email is complete.
*/
void KAMail::slotEmailSent(KJob* job)
{
	QStringList errmsgs;
	JobData jobdata;
	if (job->error())
	{
		kError() << "Failed:" << job->error();
		errmsgs = errors(job->errorString(), SEND_ERROR);
	}
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
	if (jobdata.allowNotify)
		notifyQueued(jobdata.event);
	theApp()->emailSent(jobdata, errmsgs);
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
QString KAMail::addToKMailFolder(const JobData& data, const char* folder, bool checkKmailRunning)
{
	QString err;
	if (checkKmailRunning)
		err = KAlarm::runKMail(true);
	if (err.isNull())
	{
		QString message = initHeaders(data, true);
		err = appendBodyAttachments(message, data.event);
		if (!err.isNull())
			return err;

		// Write to a temporary file for feeding to KMail
		KTemporaryFile tmpFile;
		if (!tmpFile.open())
		{
			kError() << folder << ": Unable to open a temporary mail file";
			return QString("");
		}
		QTextStream stream(&tmpFile);
		stream << message;
		stream.flush();
		if (tmpFile.error() != QFile::NoError)
		{
			kError() << folder << ": Error" << tmpFile.errorString() << " writing to temporary mail file";
			return QString("");
		}

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
QString initHeaders(const KAMail::JobData& data, bool dateId)
{
	QString message;
	if (dateId)
	{
		KDateTime now = KDateTime::currentUtcDateTime();
		QString from = data.from;
		from.remove(QRegExp("^.*<")).remove(QRegExp(">.*$"));
		message = QLatin1String("Date: ");
		message += now.toTimeSpec(Preferences::timeZone()).toString(KDateTime::RFCDateDay);
		message += QString::fromLatin1("\nMessage-Id: <%1.%2.%3>\n").arg(now.toTime_t()).arg(now.time().msec()).arg(from);
	}
	message += QLatin1String("From: ") + data.from;
	message += QLatin1String("\nTo: ") + data.event.emailAddresses(", ");
	if (!data.bcc.isEmpty())
		message += QLatin1String("\nBcc: ") + data.bcc;
	message += QLatin1String("\nSubject: ") + data.event.emailSubject();
	message += QString::fromLatin1("\nX-Mailer: %1/" KALARM_VERSION).arg(KGlobal::mainComponent().aboutData()->programName());
	return message;
}

/******************************************************************************
* Append the body and attachments to the email text.
* Reply = reason for error
*       = 0 if successful.
*/
QString KAMail::appendBodyAttachments(QString& message, const KAEvent& event)
{
	static const char* textMimeTypes[] = {
		"application/x-sh", "application/x-csh", "application/x-shellscript",
		"application/x-nawk", "application/x-gawk", "application/x-awk",
		"application/x-perl", "application/x-desktop",
		0
	};
	QStringList attachments = event.emailAttachments();
	if (!attachments.count())
	{
		// There are no attachments, so simply append the message body
		message += "\n\n";
		message += event.message();
	}
	else
	{
		// There are attachments, so the message must be in MIME format
		// Create a boundary string
		time_t timenow;
		time(&timenow);
		QString boundary;
		boundary.sprintf("------------_%lu_-%lx=", 2*timenow, timenow);
		message += QLatin1String("\nMIME-Version: 1.0");
		message += QString::fromLatin1("\nContent-Type: multipart/mixed;\n  boundary=\"%1\"\n").arg(boundary);

		if (!event.message().isEmpty())
		{
			// There is a message body
			message += QString::fromLatin1("\n--%1\nContent-Type: text/plain\nContent-Transfer-Encoding: 8bit\n\n").arg(boundary);
			message += event.message();
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

			// Check if the attachment is a text file
			QString mimeType = fi.mimetype();
			bool text = mimeType.startsWith(QLatin1String("text/"));
			if (!text)
			{
				for (int i = 0;  !text && textMimeTypes[i];  ++i)
					text = (mimeType == textMimeTypes[i]);
			}

			message += QString::fromLatin1("\n--%1").arg(boundary);
			message += QString::fromLatin1("\nContent-Type: %2; name=\"%3\"").arg(mimeType).arg(fi.text());
			message += QString::fromLatin1("\nContent-Transfer-Encoding: %1").arg(QLatin1String(text ? "8bit" : "BASE64"));
			message += QString::fromLatin1("\nContent-Disposition: attachment; filename=\"%4\"\n\n").arg(fi.text());

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
			else if (text)
			{
				// Text attachment doesn't need conversion
				message += QString::fromLatin1(contents);
			}
			else
			{
				// Convert the attachment to BASE64 encoding
				message += QString::fromLatin1(contents.toBase64());
			}
			if (atterror)
				return attachError;
		}
		message += QString::fromLatin1("\n--%1--\n.\n").arg(boundary);
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
	QByteArray addrs = items.toLocal8Bit();
	const char* ad = static_cast<const char*>(addrs);

	// parse an address-list
	QList<KMime::Types::Address> maybeAddressList;
	if (!HeaderParsing::parseAddressList(ad, ad + addrs.length(), maybeAddressList))
		return QString::fromLocal8Bit(ad);    // return the address in error

	// extract the mailboxes and complain if there are groups
	for (int i = 0, end = maybeAddressList.count();  i < end;  ++i)
	{
		QString bad = convertAddress(maybeAddressList[i], list);
		if (!bad.isEmpty())
			return bad;
	}
	return QString();
}

#if 0
/******************************************************************************
*  Parse an email address, optionally containing display name, entered by the
*  user, and append it to the specified list.
*  Reply = the invalid item if error, else empty string.
*/
QString KAMail::convertAddress(const QString& item, EmailAddressList& list)
{
	QByteArray addr = item.toLocal8Bit();
	const char* ad = static_cast<const char*>(addr);
	KMime::Types::Address maybeAddress;
	if (!HeaderParsing::parseAddress(ad, ad + addr.length(), maybeAddress))
		return item;     // error
	return convertAddress(maybeAddress, list);
}
#endif

/******************************************************************************
*  Convert a single KMime::Types address to a KCal::Person instance and append
*  it to the specified list.
*/
QString KAMail::convertAddress(KMime::Types::Address addr, EmailAddressList& list)
{
	if (!addr.displayName.isEmpty())
	{
		kDebug() << "Mailbox groups not allowed! Name:" << addr.displayName;
		return addr.displayName;
	}
	const QList<KMime::Types::Mailbox>& mblist = addr.mailboxList;
	for (int i = 0, end = mblist.count();  i < end;  ++i)
	{
		QString addrPart = mblist[i].addrSpec().localPart;
		if (!mblist[i].addrSpec().domain.isEmpty())
		{
			addrPart += QLatin1Char('@');
			addrPart += mblist[i].addrSpec().domain;
		}
		list += KCal::Person(mblist[i].name(), addrPart);
	}
	return QString();
}

/*
QString KAMail::convertAddresses(const QString& items, QStringList& list)
{
	EmailAddressList addrs;
	QString item = convertAddresses(items, addrs);
	if (!item.isEmpty())
		return item;
	for (EmailAddressList::Iterator ad = addrs.begin();  ad != addrs.end();  ++ad)
	{
		item = (*ad).fullName().toLocal8Bit();
		switch (checkAddress(item))
		{
			case 1:      // OK
				list += item;
				break;
			case 0:      // null address
				break;
			case -1:     // invalid address
				return item;
		}
	}
	return QString();
}*/

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

#if 0
/******************************************************************************
*  Convert a comma or semicolon delimited list of attachments into a
*  KUrl::List. The items are checked for validity.
*  Reply = the invalid item if error, else empty string.
*/
QString KAMail::convertAttachments(const QString& items, KUrl::List& list)
{
	KUrl url;
	list.clear();
	QByteArray addrs = items.toLocal8Bit();
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
		QString item = items.mid(next, i - next);
		switch (checkAttachment(item, &url))
		{
			case 1:   list += url;  break;
			case 0:   break;          // empty attachment name
			case -1:
			default:  return item;    // error
		}
		next = i + 1;
	}
	return QString();
}
#endif

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

/*=============================================================================
=  HeaderParsing :  modified and additional functions.
=  The following functions are modified from, or additional to, those in
=  libkdenetwork kmime_header_parsing.cpp.
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
  if ( scursor == send ) return false;

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

/******************************************************************************
*  Modified function.
*  Allow either ',' or ';' to be used as an email address separator.
*/
bool parseAddressList( const char* & scursor, const char * const send,
		       QList<Address> & result, bool isCRLF ) {
  while ( scursor != send ) {
    eatCFWS( scursor, send, isCRLF );
    // end of header: this is OK.
    if ( scursor == send ) return true;
    // empty entry: ignore:
    if ( *scursor == ',' || *scursor == ';' ) { scursor++; continue; }   // KAlarm: allow ';' as address separator

    // parse one entry
    Address maybeAddress;
    if ( !parseAddress( scursor, send, maybeAddress, isCRLF ) ) return false;
    result.append( maybeAddress );

    eatCFWS( scursor, send, isCRLF );
    // end of header: this is OK.
    if ( scursor == send ) return true;
    // comma separating entries: eat it.
    if ( *scursor == ',' || *scursor == ';' ) scursor++;   // KAlarm: allow ';' as address separator
  }
  return true;
}

} // namespace HeaderParsing
