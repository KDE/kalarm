/*
 *  kamail.cpp  -  email functions
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#include "kalarm.h"

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <pwd.h>

#include <qfile.h>

#include <kstandarddirs.h>
#include <dcopclient.h>
#include <dcopref.h>
#include <kmessagebox.h>
#include <kprocess.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kfileitem.h>
#include <kio/netaccess.h>
#include <kdebug.h>

#include <libkcal/person.h>

#include <kmime_header_parsing.h>

#include "alarmevent.h"
#include "preferences.h"
#include "kalarmapp.h"
#include "kamail.h"

namespace HeaderParsing
{
bool parseAddressList( const char* & scursor, const char * const send,
		       QValueList<KMime::Types::Address> & result, bool isCRLF=false );
}


/******************************************************************************
* Send the email message specified in an event.
*/
bool KAMail::send(const KAlarmEvent& event)
{
	QString from = theApp()->preferences()->emailAddress();
	kdDebug(5950) << "KAlarmApp::sendEmail():\nFrom: " << from << "\nTo: " << event.emailAddresses(", ")
	              << "\nSubject: " << event.emailSubject()
	              << "\nAttachment:\n" << event.emailAttachments(", ") << endl;

	if (theApp()->preferences()->emailClient() == Preferences::SENDMAIL)
	{
		QString textComplete;
		QString command = KStandardDirs::findExe(QString::fromLatin1("sendmail"),
		                                         QString::fromLatin1("/sbin:/usr/sbin:/usr/lib"));
		if (!command.isNull())
		{
			command += QString::fromLatin1(" -oi -t ");

			textComplete += QString::fromLatin1("From: ") + from;
			textComplete += QString::fromLatin1("\nTo: ") + event.emailAddresses(", ");
			if (event.emailBcc())
				textComplete += QString::fromLatin1("\nBcc: ") + from;
			textComplete += QString::fromLatin1("\nSubject: ") + event.emailSubject();
			textComplete += QString::fromLatin1("\nX-Mailer: %1" KALARM_VERSION).arg(theApp()->aboutData()->programName());
		}
		else
		{
			command = KStandardDirs::findExe(QString::fromLatin1("mail"));
			if (command.isNull())
				return false; // give up

			command += QString::fromLatin1(" -s ");
			command += KShellProcess::quote(event.emailSubject());

			if (event.emailBcc())
			{
				command += QString::fromLatin1(" -b ");
				command += KShellProcess::quote(from);
			}

			command += " ";
			command += event.emailAddresses(" "); // locally provided, okay
		}

		// Add the body and attachments to the message.
		// (Sendmail requires attachments to have already been included in the message.)
		appendBodyAttachments(textComplete, event);

//kdDebug()<<"Email:"<<command<<"\n-----\n"<<textComplete<<"\n-----\n";
		FILE* fd = popen(command.local8Bit(), "w");
		if (!fd)
		{
			kdError(5950) << "Unable to open a pipe to " << command << endl;
			return false;
		}
		fwrite(textComplete.local8Bit(), textComplete.length(), 1, fd);
		pclose(fd);
		return true;
	}
	else
	{
		return sendKMail(event, from);
	}
}

/******************************************************************************
* Send the email message via KMail.
*/
bool KAMail::sendKMail(const KAlarmEvent& event, const QString& from)
{
	if (theApp()->dcopClient()->isApplicationRegistered("kmail"))
	{
		// KMail is running - use a DCOP call
		QCString    replyType;
		QByteArray  replyData;
		QByteArray  data;
		QDataStream arg(data, IO_WriteOnly);
		arg << event.emailAddresses(", ")
		    << QString::null
		    << (event.emailBcc() ? from : QString::null)
		    << event.emailSubject()
		    << event.message();
		if (!event.emailAttachments().count())
		{
			arg << (int)0 << KURL();
			int result = 0;
			if (theApp()->dcopClient()->call("kmail", "KMailIface",
			                                 "openComposer(QString,QString,QString,QString,QString,int,KURL)",
			                                 data, replyType, replyData)
			&&  replyType == "int")
			{
				QDataStream _reply_stream(replyData, IO_ReadOnly);
				_reply_stream >> result;
			}
			if (!result)
			{
				kdDebug(5950) << "sendKMail(): kmail openComposer() call failed." << endl;
				return false;
			}
		}
		else
		{
			arg << false;
			if (!theApp()->dcopClient()->call("kmail", "KMailIface",
			                                  "openComposer(QString,QString,QString,QString,QString,bool)",
			                                   data, replyType, replyData)
			||  replyType != "DCOPRef")
			{
				kdDebug(5950) << "sendKMail(): kmail openComposer() call failed." << endl;
				return false;
			}
			DCOPRef composer;
			QDataStream _reply_stream(replyData, IO_ReadOnly);
			_reply_stream >> composer;
			QStringList attachments = event.emailAttachments();
			for (QStringList::Iterator at = attachments.begin();  at != attachments.end();  ++at)
			{
				QString attachment = (*at).local8Bit();
				QByteArray dataAtt;
				QDataStream argAtt(dataAtt, IO_WriteOnly);
				argAtt << KURL(attachment);
				argAtt << QString::null;
				if (!theApp()->dcopClient()->call(composer.app(), composer.object(), "addAttachment(KURL,QString)", dataAtt, replyType, replyData))
				{
					kdDebug(5950) << "sendKMail(): kmail composer:addAttachment() call failed." << endl;
					return false;
				}
			}
		}
	}
	else
	{
		// KMail isn't running - start it
		KProcess proc;
		proc << "kmail"
		     << "--subject" << event.emailSubject().local8Bit()
		     << "--body" << event.message().local8Bit();
		if (event.emailBcc())
			proc << "--bcc" << from.local8Bit();
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
			return false;
		}
	}
	return true;
}

/******************************************************************************
* Append the body and attachments to the email text.
* Reply = attachment filename if error reading it
*       = 0 if successful.
*/
QString KAMail::appendBodyAttachments(QString& message, const KAlarmEvent& event)
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
		for (QStringList::Iterator at = attachments.begin();  at != attachments.end();  ++at)
		{
			QString attachment = (*at).local8Bit();
			KURL url(attachment);
			url.cleanPath();
			KIO::UDSEntry uds;
			if (!KIO::NetAccess::stat(url, uds)) {
				kdError(5950) << "KAMail::appendBodyAttachments(): not found: " << attachment << endl;
				return attachment;
			}
			KFileItem fi(uds, url);
			if (fi.isDir()  ||  !fi.isReadable()) {
				kdError(5950) << "KAMail::appendBodyAttachments(): not file/not readable: " << attachment << endl;
				return attachment;
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
			message += QString::fromLatin1("\nContent-Transfer-Encoding: %1").arg(text ? "8bit" : "BASE64");
			message += QString::fromLatin1("\nContent-Disposition: attachment; filename=\"%4\"\n\n").arg(fi.text());

			// Read the file contents
			QString tmpFile;
			if (!KIO::NetAccess::download(url, tmpFile)) {
				kdError(5950) << "KAMail::appendBodyAttachments(): load failure: " << attachment << endl;
				return attachment;
			}
			QFile file(tmpFile);
			if (!file.open(IO_ReadOnly) ) {
				kdDebug(5950) << "KAMail::appendBodyAttachments() tmp load error: " << attachment << endl;
				return attachment;
			}
			Offset size = file.size();
			char* contents = new char [size + 1];
#if QT_VERSION < 300
			typedef int Q_LONG;
#endif
			Q_LONG bytes = file.readBlock(contents, size);
			file.close();
			contents[size] = 0;
			if (bytes == -1  ||  (Offset)bytes < size) {
				kdDebug(5950) << "KAMail::appendBodyAttachments() read error: " << attachment << endl;
				return attachment;
			}

			if (text)
			{
				message += contents;
			}
			else
			{
				// Convert the attachment to BASE64 encoding
				char* base64 = new char [size * 2];
				size = base64Encode(contents, base64, size);
				if (size == (Offset)-1) {
					kdDebug(5950) << "KAMail::appendBodyAttachments() base64 buffer overflow: " << attachment << endl;
					return attachment;
				}
				message += QString::fromLatin1(base64, size);
			}
		}
		message += QString::fromLatin1("\n--%1--\n.\n").arg(boundary);
	}
	return QString::null;
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

/******************************************************************************
*  Convert a comma or semicolon delimited list of attachments into a
*  QStringList. The items are checked for validity.
*  Reply = the invalid item if error, else empty string.
*/
QString KAMail::convertAttachments(const QString& items, QStringList& list, bool check)
{
	list.clear();
	QCString addrs = items.local8Bit();
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
		QString item = items.mid(next, i - next);
		int checkResult;
		checkResult = checkAttachment(item, check);
		switch (checkResult)
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
*  Optionally check for the existence of the attachment file.
*/
int KAMail::checkAttachment(QString& attachment, bool check)
{
	attachment.stripWhiteSpace();
	if (attachment.isEmpty())
		return 0;
	if (check)
	{
		// Check that the file exists
		KURL url(attachment);
		url.cleanPath();
		KIO::UDSEntry uds;
		if (!KIO::NetAccess::stat(url, uds))
			return -1;       // doesn't exist
		KFileItem fi(uds, url);
		if (fi.isDir()  ||  !fi.isReadable())
			return -1;
	}
	return 1;
}


/******************************************************************************
*  Optionally check for the existence of the attachment file.
*/
KAMail::Offset KAMail::base64Encode(char* in, char* out, KAMail::Offset size)
{
	const int MAX_LINELEN = 72;
	static unsigned char dtable[65] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";

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
			if (outIndex + 5 >= size*2)
				return (Offset)-1;
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
	return outIndex;
}


/*=============================================================================
=  KMime::HeaderParsing :  modified and additional functions.
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
