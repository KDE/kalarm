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
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
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
		static int        checkAttachment(QString& attachment, KURL* = 0);
		static QString    convertAddresses(const QString& addresses, EmailAddressList&);
		static QString    convertAddresses(const QString& addresses, QStringList&);
		static QString    convertAttachments(const QString& attachments, QStringList& list);
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
		static char*      base64Encode(const char* in, Offset size, Offset& outSize);
};

#endif // KAMAIL_H
