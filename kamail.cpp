/*
 *  kamail.cpp  -  email functions
 *  Program:  kalarm
 *  (C) 2002 - 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "kalarm.h"

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pwd.h>

#include <qfile.h>
#include <qregexp.h>

#include <kstandarddirs.h>
#include <dcopclient.h>
#include <dcopref.h>
#include <kmessagebox.h>
#include <kprocess.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kfileitem.h>
#include <kio/netaccess.h>
#include <ktempfile.h>
#include <kdebug.h>

#include <libkcal/person.h>

#include <kmime_header_parsing.h>

#include "alarmevent.h"
#include "preferences.h"
#include "kalarmapp.h"
#include "mainwindow.h"
#include "kamail.h"

namespace HeaderParsing
{
bool parseAddress( const char* & scursor, const char * const send,
                   KMime::Types::Address & result, bool isCRLF=false );
bool parseAddressList( const char* & scursor, const char * const send,
                       QValueList<KMime::Types::Address> & result, bool isCRLF=false );
}

namespace
{
QString getHostName();
}

const QString KAMail::EMAIL_QUEUED_NOTIFY = QString::fromLatin1("EmailQueuedNotify");

QString KAMail::i18n_NeedFromEmailAddress()
{ return i18n("A 'From' email address must be configured in order to execute email alarms."); }


/******************************************************************************
* Send the email message specified in an event.
* Reply = reason for failure (which may be the empty string)
*       = null string if success.
*/
QString KAMail::send(const KAEvent& event, bool allowNotify)
{
	Preferences* preferences = Preferences::instance();
	QString from = preferences->emailAddress();
	if (from.isEmpty())
	{
		return preferences->emailUseControlCentre()
		       ? i18n("No 'From' email address is configured.\nPlease set it in the KDE Control Center or in the %1 Preferences dialog.").arg(kapp->aboutData()->programName())
		       : i18n("No 'From' email address is configured.\nPlease set it in the %1 Preferences dialog.").arg(kapp->aboutData()->programName());
	}
	QString bcc = event.emailBcc() ? preferences->emailBccAddress() : QString::null;
	kdDebug(5950) << "KAlarmApp::sendEmail(): To: " << event.emailAddresses(", ")
	              << "\nSubject: " << event.emailSubject() << endl;

	if (preferences->emailClient() == Preferences::SENDMAIL)
	{
		QString textComplete;
		QString command = KStandardDirs::findExe(QString::fromLatin1("sendmail"),
		                                         QString::fromLatin1("/sbin:/usr/sbin:/usr/lib"));
		if (!command.isNull())
		{
			command += QString::fromLatin1(" -oi -t ");
			textComplete = initHeaders(event, from, bcc, false);
		}
		else
		{
			command = KStandardDirs::findExe(QString::fromLatin1("mail"));
			if (command.isNull())
				return i18n("%1 not found").arg(QString::fromLatin1("sendmail")); // give up

			command += QString::fromLatin1(" -s ");
			command += KShellProcess::quote(event.emailSubject());

			if (!bcc.isEmpty())
			{
				command += QString::fromLatin1(" -b ");
				command += KShellProcess::quote(bcc);
			}

			command += " ";
			command += event.emailAddresses(" "); // locally provided, okay
		}

		// Add the body and attachments to the message.
		// (Sendmail requires attachments to have already been included in the message.)
		QString err = appendBodyAttachments(textComplete, event);
		if (!err.isNull())
			return err;

//kdDebug(5950)<<"Email:"<<command<<"\n-----\n"<<textComplete<<"\n-----\n";
		FILE* fd = popen(command.local8Bit(), "w");
		if (!fd)
		{
			kdError(5950) << "KAMail::send(): Unable to open a pipe to " << command << endl;
			return QString("");
		}
		fwrite(textComplete.local8Bit(), textComplete.length(), 1, fd);
		pclose(fd);
		if (allowNotify)
			notifyQueued(event);
		return QString::null;
	}
	else
	{
		return sendKMail(event, from, bcc, allowNotify);
	}
}

/******************************************************************************
* Send the email message via KMail.
*/
QString KAMail::sendKMail(const KAEvent& event, const QString& from, const QString& bcc, bool allowNotify)
{
	if (kapp->dcopClient()->isApplicationRegistered("kmail"))
	{
		// KMail is running - use a DCOP call.
		// First, determine which DCOP call to use.
		bool useSend = false;
		QCString funcname = "sendMessage()";
		QCString function = "sendMessage(QString,QString,QString,QString,QString,QString,KURL::List)";
		QCStringList funcs = kapp->dcopClient()->remoteFunctions("kmail", "MailTransportServiceIface");
		for (QCStringList::Iterator it=funcs.begin();  it != funcs.end() && !useSend;  ++it)
		{
			QCString func = DCOPClient::normalizeFunctionSignature(*it);
			if (func.left(5) == "bool ")
			{
				func = func.mid(5);
				func.replace(QRegExp(" [0-9A-Za-z_:]+"), "");
				useSend = (func == function);
			}
		}
		if (!useSend)
		{
			funcname = "dcopAddMessage()";
			function = "dcopAddMessage(QString,QString)";
		}

		QCString    replyType;
		QByteArray  replyData;
		QByteArray  data;
		QDataStream arg(data, IO_WriteOnly);
		int result = 0;
		kdDebug(5950) << "KAMail::sendKMail(): using " << funcname << endl;
		if (useSend)
		{
			// This version of KMail has the sendMessage() function,
			// which transmits the message immediately.
			arg << from;
			arg << event.emailAddresses(", ");
			arg << "";
			arg << bcc;
			arg << event.emailSubject();
			arg << event.message();
			arg << KURL::List(event.emailAttachments());
			if (kapp->dcopClient()->call("kmail", "MailTransportServiceIface", function,
			                             data, replyType, replyData)
			&&  replyType == "bool")
				result = 1;
		}
		else
		{
			// KMail is an older version, so use dcopAddMessage()
			// to add the message to the outbox for later transmission.
			QString message = initHeaders(event, from, bcc, true);
			QString err = appendBodyAttachments(message, event);
			if (!err.isNull())
				return err;

			// Write to a temporary file for feeding to KMail
			KTempFile tmpFile;
			tmpFile.setAutoDelete(true);     // delete file when it is destructed
			QTextStream* stream = tmpFile.textStream();
			if (!stream)
			{
				kdError(5950) << "KAMail::sendKMail(): Unable to open a temporary mail file" << endl;
				return QString("");
			}
			*stream << message;
			tmpFile.close();
			if (tmpFile.status())
			{
				kdError(5950) << "KAMail::sendKMail(): Error " << tmpFile.status() << " writing to temporary mail file" << endl;
				return QString("");
			}

			// Notify KMail of the message in the temporary file
			arg << QString::fromLatin1("outbox") << tmpFile.name();
			if (kapp->dcopClient()->call("kmail", "KMailIface", "dcopAddMessage(QString,QString)",
			                             data, replyType, replyData)
			&&  replyType == "int")
				result = 1;
		}
		if (result)
		{
			QDataStream _reply_stream(replyData, IO_ReadOnly);
			_reply_stream >> result;
		}
		if (result <= 0)
		{
			kdError(5950) << "KAMail::sendKMail(): kmail " << funcname << " call failed (error code = " << result << ")" << endl;
			return i18n("Error calling KMail");
		}
		if (allowNotify)
			notifyQueued(event);
	}
	else
	{
		// KMail isn't running - start it
		KProcess proc;
		proc << "kmail"
		     << "--subject" << event.emailSubject().local8Bit()
		     << "--body" << event.message().local8Bit();
		if (!bcc.isEmpty())
			proc << "--bcc" << bcc.local8Bit();
		QStringList attachments = event.emailAttachments();
		if (attachments.count())
		{
			for (QStringList::Iterator at = attachments.begin();  at != attachments.end();  ++at)
				proc << "--attach" << (*at).local8Bit();
		}
		EmailAddressList addresses = event.emailAddresses();
		for (EmailAddressList::Iterator ad = addresses.begin();  ad != addresses.end();  ++ad)
			proc << (*ad).fullName().local8Bit();
		if (!proc.start(KProcess::DontCare))
		{
			kdDebug(5950) << "sendKMail(): kmail start failed" << endl;
			return i18n("Error starting KMail");
		}
	}
	return QString::null;
}

/******************************************************************************
* Create the headers part of the email.
*/
QString KAMail::initHeaders(const KAEvent& event, const QString& from, const QString& bcc, bool dateId)
{
	QString message;
	if (dateId)
	{
		struct timeval tod;
		gettimeofday(&tod, 0);
		time_t timenow = tod.tv_sec;
		char buff[64];
		strftime(buff, sizeof(buff), "Date: %a, %d %b %Y %H:%M:%S %z", localtime(&timenow));
		message = QString::fromLatin1(buff);
		message += QString::fromLatin1("\nMessage-Id: <%1.%2.%3>\n").arg(timenow).arg(tod.tv_usec).arg(from);
	}
	message += QString::fromLatin1("From: ") + from;
	message += QString::fromLatin1("\nTo: ") + event.emailAddresses(", ");
	if (!bcc.isEmpty())
		message += QString::fromLatin1("\nBcc: ") + bcc;
	message += QString::fromLatin1("\nSubject: ") + event.emailSubject();
	message += QString::fromLatin1("\nX-Mailer: %1" KALARM_VERSION).arg(kapp->aboutData()->programName());
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
		QCString boundary;
		boundary.sprintf("------------_%lu_-%lx=", 2*timenow, timenow);
		message += QString::fromLatin1("\nMIME-Version: 1.0");
		message += QString::fromLatin1("\nContent-Type: multipart/mixed;\n  boundary=\"%1\"\n").arg(boundary);

		if (!event.message().isEmpty())
		{
			// There is a message body
			message += QString::fromLatin1("\n--%1\nContent-Type: text/plain\nContent-Transfer-Encoding: 8bit\n\n").arg(boundary);
			message += event.message();
		}

		// Append each attachment in turn
		QString attachError = i18n("Error attaching file:\n%1");
		for (QStringList::Iterator at = attachments.begin();  at != attachments.end();  ++at)
		{
			QString attachment = (*at).local8Bit();
			KURL url(attachment);
			url.cleanPath();
			KIO::UDSEntry uds;
			if (!KIO::NetAccess::stat(url, uds)) {
				kdError(5950) << "KAMail::appendBodyAttachments(): not found: " << attachment << endl;
				return i18n("Attachment not found:\n%1").arg(attachment);
			}
			KFileItem fi(uds, url);
			if (fi.isDir()  ||  !fi.isReadable()) {
				kdError(5950) << "KAMail::appendBodyAttachments(): not file/not readable: " << attachment << endl;
				return attachError.arg(attachment);
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
			message += QString::fromLatin1("\nContent-Transfer-Encoding: %1").arg(QString::fromLatin1(text ? "8bit" : "BASE64"));
			message += QString::fromLatin1("\nContent-Disposition: attachment; filename=\"%4\"\n\n").arg(fi.text());

			// Read the file contents
			QString tmpFile;
			if (!KIO::NetAccess::download(url, tmpFile)) {
				kdError(5950) << "KAMail::appendBodyAttachments(): load failure: " << attachment << endl;
				return attachError.arg(attachment);
			}
			QFile file(tmpFile);
			if (!file.open(IO_ReadOnly) ) {
				kdDebug(5950) << "KAMail::appendBodyAttachments() tmp load error: " << attachment << endl;
				return attachError.arg(attachment);
			}
			Offset size = file.size();
			char* contents = new char [size + 1];
#if QT_VERSION < 300
			typedef int Q_LONG;
#endif
			Q_LONG bytes = file.readBlock(contents, size);
			file.close();
			contents[size] = 0;
			bool atterror = false;
			if (bytes == -1  ||  (Offset)bytes < size) {
				kdDebug(5950) << "KAMail::appendBodyAttachments() read error: " << attachment << endl;
				atterror = true;
			}
			else if (text)
			{
				// Text attachment doesn't need conversion
				message += contents;
			}
			else
			{
				// Convert the attachment to BASE64 encoding
				Offset base64Size;
				char* base64 = base64Encode(contents, size, base64Size);
				if (base64Size == (Offset)-1) {
					kdDebug(5950) << "KAMail::appendBodyAttachments() base64 buffer overflow: " << attachment << endl;
					atterror = true;
				}
				else
					message += QString::fromLatin1(base64, base64Size);
				delete[] base64;
			}
			delete[] contents;
			if (atterror)
				return attachError.arg(attachment);
		}
		message += QString::fromLatin1("\n--%1--\n.\n").arg(boundary);
	}
	return QString::null;
}

/******************************************************************************
* If any of the destination email addresses are non-local, display a
* notification message saying that an email has been queued for sending.
*/
void KAMail::notifyQueued(const KAEvent& event)
{
	KMime::Types::Address addr;
	QString localhost = QString::fromLatin1("localhost");
	QString hostname  = getHostName();
	const EmailAddressList& addresses = event.emailAddresses();
	for (QValueList<KCal::Person>::ConstIterator it = addresses.begin();  it != addresses.end();  ++it)
	{
		QCString email = (*it).email().local8Bit();
		const char* em = email;
		if (!email.isEmpty()
		&&  HeaderParsing::parseAddress(em, em + email.length(), addr))
		{
			QString domain = addr.mailboxList.first().addrSpec.domain;
			if (!domain.isEmpty()  &&  domain != localhost  &&  domain != hostname)
			{
				QString text = (Preferences::instance()->emailClient() == Preferences::KMAIL)
				             ? i18n("An email has been queued to be sent by KMail")
				             : i18n("An email has been queued to be sent");
				KMessageBox::information(0, text, QString::null, EMAIL_QUEUED_NOTIFY);
				return;
			}
		}
	}
}

/******************************************************************************
*  Parse a list of email addresses, optionally containing display names,
*  entered by the user.
*  Reply = the invalid item if error, else empty string.
*/
QString KAMail::convertAddresses(const QString& items, EmailAddressList& list)
{
	list.clear();
	QCString addrs = items.local8Bit();
	const char* ad = static_cast<const char*>(addrs);

	// parse an address-list
	QValueList<KMime::Types::Address> maybeAddressList;
	if (!HeaderParsing::parseAddressList(ad, ad + addrs.length(), maybeAddressList))
		return QString::fromLocal8Bit(ad);

	// extract the mailboxes and complain if there are groups
	for (QValueList<KMime::Types::Address>::ConstIterator it = maybeAddressList.begin();
	     it != maybeAddressList.end();  ++it)
	{
		if (!(*it).displayName.isEmpty())
		{
			kdDebug(5950) << "mailbox groups not allowed! Name: \"" << (*it).displayName << "\"" << endl;
			return (*it).displayName;
		}
		const QValueList<KMime::Types::Mailbox>& mblist = (*it).mailboxList;
		for (QValueList<KMime::Types::Mailbox>::ConstIterator mb = mblist.begin();
		     mb != mblist.end();  ++mb)
		{
			QString addrPart = (*mb).addrSpec.localPart;
			if (!(*mb).addrSpec.domain.isEmpty())
			{
				addrPart += QChar('@');
				addrPart += (*mb).addrSpec.domain;
			}
			list += KCal::Person((*mb).displayName, addrPart);
		}
	}
	return QString::null;
}

QString KAMail::convertAddresses(const QString& items, QStringList& list)
{
	EmailAddressList addrs;
	QString item = convertAddresses(items, addrs);
	if (item.isEmpty())
	{
		for (EmailAddressList::Iterator ad = addrs.begin();  ad != addrs.end();  ++ad)
			list += (*ad).fullName().local8Bit();
	}
	return item;
}

/******************************************************************************
*  Check the validity of an email address.
*  Because internal email addresses don't have to abide by the usual internet
*  email address rules, only some basic checks are made.
*  Reply = 1 if alright, 0 if empty, -1 if error.
*/
int KAMail::checkAddress(QString& address)
{
	address = address.stripWhiteSpace();
	// Check that there are no list separator characters present
	if (address.find(',') >= 0  ||  address.find(';') >= 0)
		return -1;
	int n = address.length();
	if (!n)
		return 0;
	int start = 0;
	int end   = n - 1;
	if (address[end] == '>')
	{
		// The email address is in <...>
		if ((start = address.find('<')) < 0)
			return -1;
		++start;
		--end;
	}
	int i = address.find('@', start);
	if (i >= 0)
	{
		if (i == start  ||  i == end)          // check @ isn't the first or last character
//		||  address.find('@', i + 1) >= 0)    // check for multiple @ characters
			return -1;
	}
/*	else
	{
		// Allow the @ character to be missing if it's a local user
		if (!getpwnam(address.mid(start, end - start + 1).local8Bit()))
			return false;
	}
	for (int i = start;  i <= end;  ++i)
	{
		char ch = address[i].latin1();
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
	KURL url;
	list.clear();
	int length = items.length();
	for (int next = 0;  next < length;  )
	{
		// Find the first delimiter character (, or ;)
		int i = items.find(',', next);
		if (i < 0)
			i = items.length();
		int sc = items.find(';', next);
		if (sc < 0)
			sc = items.length();
		if (sc < i)
			i = sc;
		QString item = items.mid(next, i - next).stripWhiteSpace();
		switch (checkAttachment(item))
		{
			case 1:   list += item;  break;
			case 0:   break;          // empty attachment name
			case -1:
			default:  return item;    // error
		}
		next = i + 1;
	}
	return QString::null;
}

/******************************************************************************
*  Check for the existence of the attachment file.
*  If non-null, '*url' receives the KURL of the attachment.
*/
int KAMail::checkAttachment(QString& attachment, KURL* url)
{
	attachment.stripWhiteSpace();
	if (attachment.isEmpty())
	{
		if (url)
			*url = KURL();
		return 0;
	}
	// Check that the file exists
	KURL u = KURL::fromPathOrURL(attachment);
	u.cleanPath();
	if (url)
		*url = u;
	KIO::UDSEntry uds;
	if (!KIO::NetAccess::stat(u, uds))
		return -1;       // doesn't exist
	KFileItem fi(uds, u);
	if (fi.isDir()  ||  !fi.isReadable())
		return -1;
	return 1;
}


/******************************************************************************
*  Convert a block of memory to Base64 encoding.
*  'outSize' is set to the number of bytes used in the returned block, or to
*            -1 if overflow.
*  Reply = BASE64 buffer, which the caller must delete[] afterwards.
*/
char* KAMail::base64Encode(const char* in, KAMail::Offset size, KAMail::Offset& outSize)
{
	const int MAX_LINELEN = 72;
	static unsigned char dtable[65] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

	char* out = new char [2*size + 5];
	outSize = (Offset)-1;
	Offset outIndex = 0;
	int lineLength = 0;
	for (Offset inIndex = 0;  inIndex < size;  )
	{
		unsigned char igroup[3];
		int n;
		for (n = 0;  n < 3;  ++n)
		{
			if (inIndex < size)
				igroup[n] = (unsigned char)in[inIndex++];
			else
			{
				igroup[n] = igroup[2] = 0;
				break;
			}
		}

		if (n > 0)
		{
			unsigned char ogroup[4];
			ogroup[0] = dtable[igroup[0] >> 2];
			ogroup[1] = dtable[((igroup[0] & 3) << 4) | (igroup[1] >> 4)];
			ogroup[2] = dtable[((igroup[1] & 0xF) << 2) | (igroup[2] >> 6)];
			ogroup[3] = dtable[igroup[2] & 0x3F];

			if (n < 3)
			{
				ogroup[3] = '=';
				if (n < 2)
					ogroup[2] = '=';
			}
			if (outIndex >= size*2)
			{
				delete[] out;
				return 0;
			}
			for (int i = 0;  i < 4;  ++i)
			{
				if (lineLength >= MAX_LINELEN)
				{
					out[outIndex++] = '\r';
					out[outIndex++] = '\n';
					lineLength = 0;
				}
				out[outIndex++] = ogroup[i];
				++lineLength;
			}
		}
	}

	if (outIndex + 2 < size*2)
	{
		out[outIndex++] = '\r';
		out[outIndex++] = '\n';
	}
	outSize = outIndex;
	return out;
}

namespace
{
/******************************************************************************
* Get the local system's host name.
*/
QString getHostName()
{
        char hname[256];
        if (gethostname(hname, sizeof(hname)))
                return QString::null;
        return QString::fromLocal8Bit(hname);
}
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

  while ( scursor != send ) {
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
      tmp = QString::null;
      if ( parseAtom( scursor, send, result, false /* no 8bit */ ) ) {
        if (getpwnam(result.local8Bit()))
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
    result.displayName = QString::null;
    result.mailboxList.append( maybeMailbox );
    return true;
  }
  scursor = oldscursor;

  // KAlarm: Allow a local user name to be specified
  // no, it's not a single mailbox. Try if it's a local user name:
  QString maybeUserName;
  if ( parseUserName( scursor, send, maybeUserName, isCRLF ) ) {
    // yes, it is:
    maybeMailbox.displayName = QString::null;
    maybeMailbox.addrSpec.localPart = maybeUserName;
    maybeMailbox.addrSpec.domain = QString::null;
    result.displayName = QString::null;
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
		       QValueList<Address> & result, bool isCRLF ) {
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
