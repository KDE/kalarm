/*
 *  kamail.cpp  -  email functions
 *  Program:  kalarm
 *  Copyright © 2002-2006 by David Jarvie <software@astrojar.org.uk>
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

#include "kalarm.h"

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
#include <QtDBus>

#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kprocess.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kfileitem.h>
#include <kio/netaccess.h>
#include <ktemporaryfile.h>
#include <kemailsettings.h>
#include <kdebug.h>
#include <kresolver.h>

#include <libkpimidentities/identitymanager.h>
#include <libkpimidentities/identity.h>
#include <kcal/person.h>

#include <kmime/kmime_header_parsing.h>

#include "alarmevent.h"
#include "functions.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "preferences.h"
#include "kamail.h"
#include "kmailinterface.h"

static const char* KMAIL_DBUS_SERVICE = "org.kde.kmail";


namespace HeaderParsing
{
bool parseAddress( const char* & scursor, const char * const send,
                   KMime::Types::Address & result, bool isCRLF=false );
bool parseAddressList( const char* & scursor, const char * const send,
                       QList<KMime::Types::Address> & result, bool isCRLF=false );
}

struct KAMailData
{
	KAMailData(const KAEvent& e, const QString& fr, const QString& bc, bool allownotify)
	                 : event(e), from(fr), bcc(bc), allowNotify(allownotify) { }
	const KAEvent& event;
	QString        from;
	QString        bcc;
	bool           allowNotify;
};


QString KAMail::i18n_NeedFromEmailAddress()
{ return i18n("A 'From' email address must be configured in order to execute email alarms."); }

QString KAMail::i18n_sent_mail()
{ return i18nc("KMail folder name: this should be translated the same as in kmail", "sent-mail"); }

KPIM::IdentityManager* KAMail::mIdentityManager = 0;
KPIM::IdentityManager* KAMail::identityManager()
{
	if (!mIdentityManager)
		mIdentityManager = new KPIM::IdentityManager(true);   // create a read-only kmail identity manager
	return mIdentityManager;
}


/******************************************************************************
* Send the email message specified in an event.
* Reply = true if the message was sent - 'errmsgs' may contain copy error messages.
*       = false if the message was not sent - 'errmsgs' contains the error messages.
*/
bool KAMail::send(const KAEvent& event, QStringList& errmsgs, bool allowNotify)
{
	QString err;
	QString from;
	if (event.emailFromKMail().isEmpty())
		from = Preferences::emailAddress();
	else
	{
		from = mIdentityManager->identityForName(event.emailFromKMail()).fullEmailAddr();
		if (from.isEmpty())
		{
			errmsgs = errors(i18n("Invalid 'From' email address.\nKMail identity '%1' not found.", event.emailFromKMail()));
			return false;
		}
	}
	if (from.isEmpty())
	{
		switch (Preferences::emailFrom())
		{
			case Preferences::MAIL_FROM_KMAIL:
				errmsgs = errors(i18n("No 'From' email address is configured (no default KMail identity found)\nPlease set it in KMail or in the KAlarm Preferences dialog."));
				break;
			case Preferences::MAIL_FROM_CONTROL_CENTRE:
				errmsgs = errors(i18n("No 'From' email address is configured.\nPlease set it in the KDE Control Center or in the KAlarm Preferences dialog."));
				break;
			case Preferences::MAIL_FROM_ADDR:
			default:
				errmsgs = errors(i18n("No 'From' email address is configured.\nPlease set it in the KAlarm Preferences dialog."));
				break;
		}
		return false;
	}
	KAMailData data(event, from,
	                (event.emailBcc() ? Preferences::emailBccAddress() : QString()),
	                allowNotify);
	kDebug(5950) << "KAlarmApp::sendEmail(): To: " << event.emailAddresses(", ")
	              << "\nSubject: " << event.emailSubject() << endl;

	if (Preferences::emailClient() == Preferences::SENDMAIL)
	{
		// Use sendmail to send the message
		QString textComplete;
		QString command = KStandardDirs::findExe(QLatin1String("sendmail"),
		                                         QLatin1String("/sbin:/usr/sbin:/usr/lib"));
		if (!command.isNull())
		{
			command += QLatin1String(" -oi -t ");
			textComplete = initHeaders(data, false);
		}
		else
		{
			command = KStandardDirs::findExe(QLatin1String("mail"));
			if (command.isNull())
			{
				errmsgs = errors(i18n("%1 not found", QLatin1String("sendmail"))); // give up
				return false;
			}

			command += QLatin1String(" -s ");
			command += KShellProcess::quote(event.emailSubject());

			if (!data.bcc.isEmpty())
			{
				command += QLatin1String(" -b ");
				command += KShellProcess::quote(data.bcc);
			}

			command += ' ';
			command += event.emailAddresses(" "); // locally provided, okay
		}

		// Add the body and attachments to the message.
		// (Sendmail requires attachments to have already been included in the message.)
		err = appendBodyAttachments(textComplete, event);
		if (!err.isNull())
		{
			errmsgs = errors(err);
			return false;
		}

		// Execute the send command
		FILE* fd = popen(command.toLocal8Bit(), "w");
		if (!fd)
		{
			kError(5950) << "KAMail::send(): Unable to open a pipe to " << command << endl;
			errmsgs = errors();
			return false;
		}
		fwrite(textComplete.toLocal8Bit(), textComplete.length(), 1, fd);
		pclose(fd);

		if (Preferences::emailCopyToKMail())
		{
			// Create a copy of the sent email in KMail's 'Sent-mail' folder
			err = addToKMailFolder(data, "sent-mail", true);
			if (!err.isNull())
				errmsgs = errors(err, false);    // not a fatal error - continue
		}

		if (allowNotify)
			notifyQueued(event);
	}
	else
	{
		// Use KMail to send the message
		err = sendKMail(data);
		if (!err.isNull())
		{
			errmsgs = errors(err);
			return false;
		}
	}
	return true;
}

/******************************************************************************
* Send the email message via KMail.
* Reply = reason for failure (which may be the empty string)
*       = null string if success.
*/
QString KAMail::sendKMail(const KAMailData& data)
{
	QString err = KAlarm::runKMail(true);
	if (!err.isNull())
		return err;

	// KMail is now running
	QList<QVariant> args;
	args << data.from;
	args << data.event.emailAddresses(", ");
	args << "";    // CC:
	args << data.bcc;
	args << data.event.emailSubject();
	args << data.event.message();
#ifdef __GNUC__
#warning Append attachment URL list
#endif
//	args << KUrl::List(data.event.emailAttachments());
#ifdef __GNUC__
#warning Set correct DBus interface/object for kmail
#endif
	QDBusInterface iface(KMAIL_DBUS_SERVICE, QString(), QLatin1String("MailTransportServiceIface"));
	QDBusReply<bool> reply = iface.callWithArgumentList(QDBus::Block, QLatin1String("sendMessage"), args);
	if (!reply.isValid())
		kError(5950) << "KAMail::sendKMail(): D-Bus call failed: " << reply.error().message() << endl;
	else if (!reply.value())
		kError(5950) << "KAMail::sendKMail(): D-Bus call returned error" << endl;
	else
	{
		if (data.allowNotify)
			notifyQueued(data.event);
		return QString();
	}
	return i18n("Error calling KMail");
}

/******************************************************************************
* Add the message to a KMail folder.
* Reply = reason for failure (which may be the empty string)
*       = null string if success.
*/
QString KAMail::addToKMailFolder(const KAMailData& data, const char* folder, bool checkKmailRunning)
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
			kError(5950) << "KAMail::addToKMailFolder(" << folder << "): Unable to open a temporary mail file" << endl;
			return QString("");
		}
		QTextStream stream(&tmpFile);
		stream << message;
		stream.flush();
		if (tmpFile.error() != QFile::NoError)
		{
			kError(5950) << "KAMail::addToKMailFolder(" << folder << "): Error " << tmpFile.errorString() << " writing to temporary mail file" << endl;
			return QString("");
		}

		// Notify KMail of the message in the temporary file
		org::kde::kmail::kmail kmail("org.kde.kmail", "/KMail", QDBusConnection::sessionBus());
		QDBusReply<int> reply = kmail.dbusAddMessage(QString::fromLatin1(folder), tmpFile.fileName(), QString());
		if (!reply.isValid())
			kError(5950) << "KAMail::addToKMailFolder(): D-Bus call failed: " << reply.error().message() << endl;
		else if (reply.value() <= 0)
			kError(5950) << "KAMail::addToKMailFolder(): D-Bus call returned error code = " << reply.value() << endl;
		else
			return QString();    // success
		err = i18n("Error calling KMail");
	}
	kError(5950) << "KAMail::addToKMailFolder(" << folder << "): " << err << endl;
	return err;
}

/******************************************************************************
* Create the headers part of the email.
*/
QString KAMail::initHeaders(const KAMailData& data, bool dateId)
{
	QString message;
	if (dateId)
	{
		KDateTime now = KDateTime::currentUtcDateTime();
		message = QLatin1String("Date: ");
		message += now.toTimeSpec(Preferences::timeZone()).toString(KDateTime::RFCDateDay);
		message += QString::fromLatin1("\nMessage-Id: <%1.%2.%3>\n").arg(now.toTime_t()).arg(now.time().msec()).arg(data.from);
	}
	message += QLatin1String("From: ") + data.from;
	message += QLatin1String("\nTo: ") + data.event.emailAddresses(", ");
	if (!data.bcc.isEmpty())
		message += QLatin1String("\nBcc: ") + data.bcc;
	message += QLatin1String("\nSubject: ") + data.event.emailSubject();
	message += QString::fromLatin1("\nX-Mailer: %1" KALARM_VERSION).arg(KGlobal::mainComponent().aboutData()->programName());
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
		"application/x-shellscript", "application/x-nawk", "application/x-gawk", "application/x-awk",
		"application/x-perl", "application/x-python", "application/x-desktop",
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
			QString attachError = i18n("Error attaching file:\n%1", attachment);
			url.cleanPath();
			KIO::UDSEntry uds;
			if (!KIO::NetAccess::stat(url, uds, MainWindow::mainMainWindow())) {
				kError(5950) << "KAMail::appendBodyAttachments(): not found: " << attachment << endl;
				return i18n("Attachment not found:\n%1", attachment);
			}
			KFileItem fi(uds, url);
			if (fi.isDir()  ||  !fi.isReadable()) {
				kError(5950) << "KAMail::appendBodyAttachments(): not file/not readable: " << attachment << endl;
				return attachError;
			}

			// Check if the attachment is a text file
			QString mimeType = fi.mimetype();
			bool text = mimeType.startsWith("text/");
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
				kError(5950) << "KAMail::appendBodyAttachments(): load failure: " << attachment << endl;
				return attachError;
			}
			QFile file(tmpFile);
			if (!file.open(QIODevice::ReadOnly)) {
				kDebug(5950) << "KAMail::appendBodyAttachments() tmp load error: " << attachment << endl;
				return attachError;
			}
			qint64 size = file.size();
			QByteArray contents = file.readAll();
			file.close();
			bool atterror = false;
			if (contents.size() < size) {
				kDebug(5950) << "KAMail::appendBodyAttachments() read error: " << attachment << endl;
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
				QString text = (Preferences::emailClient() == Preferences::KMAIL)
				             ? i18n("An email has been queued to be sent by KMail")
				             : i18n("An email has been queued to be sent");
				KMessageBox::information(0, text, QString(), Preferences::EMAIL_QUEUED_NOTIFY);
				return;
			}
		}
	}
}

/******************************************************************************
*  Return whether any KMail identities exist.
*/
bool KAMail::identitiesExist()
{
	identityManager();    // create identity manager if not already done
	return mIdentityManager->begin() != mIdentityManager->end();
}

/******************************************************************************
*  Fetch the user's email address configured in the KDE Control Centre.
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
		kDebug(5950) << "mailbox groups not allowed! Name: \"" << addr.displayName << "\"" << endl;
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
QStringList KAMail::errors(const QString& err, bool sendfail)
{
	QString error1 = sendfail ? i18n("Failed to send email")
	                          : i18n("Error copying sent email to KMail %1 folder", i18n_sent_mail());
	if (err.isEmpty())
		return QStringList(error1);
	QStringList errs(QString::fromLatin1("%1:").arg(error1));
	errs += err;
	return errs;
}

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
		kError(5950) << "KAMail::getMailBody(): D-Bus call failed: " << reply.error().message() << endl;
		return QString();
	}
	return reply.value();
}

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
