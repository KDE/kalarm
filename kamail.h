/*
 *  kamail.h  -  email functions
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef KAMAIL_H
#define KAMAIL_H

#include <qstring.h>
class KAEvent;
class EmailAddressList;


class KAMail
{
	public:
		static QString    send(const KAEvent&, bool allowNotify = true);
		static int        checkAddress(QString& address);
		static int        checkAttachment(QString& attachment)  { return checkAttachment(attachment, true); }
		static QString    convertAddresses(const QString& addresses, EmailAddressList&);
		static QString    convertAttachments(const QString& attachments, QStringList& list, bool check);
		static const QString EMAIL_QUEUED_NOTIFY;
		static QString    i18n_NeedFromEmailAddress();
	private:
#if QT_VERSION >= 300
		typedef QIODevice::Offset Offset;
#else
		typedef uint Offset;
#endif
		static QString    sendKMail(const KAEvent&, const QString& from, const QString& bcc, bool allowNotify);
		static QString    initHeaders(const KAEvent&, const QString& from, const QString& bcc, bool dateId);
		static QString    appendBodyAttachments(QString& message, const KAEvent&);
		static void       notifyQueued(const KAEvent&);
		static int        checkAttachment(QString& attachment, bool check);
		static char*      base64Encode(const char* in, Offset size, Offset& outSize);
};

#endif // KAMAIL_H
