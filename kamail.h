/*
 *  kamail.h  -  email functions
 *  Program:  kalarm
 *  (C) 2002, 2003 by David Jarvie <software@astrojar.org.uk>
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

#ifndef KAMAIL_H
#define KAMAIL_H

#include <qstring.h>
class KAlarmEvent;
class EmailAddressList;


class KAMail
{
	public:
		static QString    send(const KAlarmEvent&, bool allowNotify = true);
		static int        checkAddress(QString& address);
		static int        checkAttachment(QString& attachment)  { return checkAttachment(attachment, true); }
		static QString    convertAddresses(const QString& addresses, EmailAddressList&);
		static QString    convertAttachments(const QString& attachments, QStringList& list, bool check);
		static const QString EMAIL_QUEUED_NOTIFY;
	private:
#if QT_VERSION >= 300
		typedef QIODevice::Offset Offset;
#else
		typedef uint Offset;
#endif
		static QString    sendKMail(const KAlarmEvent&, const QString& from, const QString& bcc, bool allowNotify);
		static QString    initHeaders(const KAlarmEvent&, const QString& from, const QString& bcc, bool dateId);
		static QString    appendBodyAttachments(QString& message, const KAlarmEvent&);
		static void       notifyQueued(const KAlarmEvent&);
		static int        checkAttachment(QString& attachment, bool check);
		static Offset     base64Encode(char* in, char* out, Offset size);
};

#endif // KAMAIL_H
