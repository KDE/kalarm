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
#include <qstringlist.h>
class KAEvent;
class EmailAddressList;
namespace KPIM { class IdentityManager; }
namespace KMime { namespace Types { struct Address; } }

struct KAMailData;


class KAMail
{
	public:
		static bool        send(const KAEvent&, QStringList& errmsgs, bool allowNotify = true);
		static int         checkAddress(QString& address);
		static int         checkAttachment(QString& attachment, KURL* = 0);
		static bool        checkAttachment(const KURL&);
		static QString     convertAddresses(const QString& addresses, EmailAddressList&);
		static QString     convertAttachments(const QString& attachments, QStringList& list);
		static KPIM::IdentityManager* identityManager();
		static bool        identitiesExist();
		static QString     controlCentreAddress();
		static QString     i18n_NeedFromEmailAddress();
		static QString     i18n_sent_mail();

	private:
		static KPIM::IdentityManager* mIdentityManager;     // KMail identity manager
#if QT_VERSION >= 300
		typedef QIODevice::Offset Offset;
#else
		typedef uint Offset;
#endif
		static QString     sendKMail(const KAMailData&);
		static QString     initHeaders(const KAMailData&, bool dateId);
		static QString     appendBodyAttachments(QString& message, const KAEvent&);
		static QString     addToKMailFolder(const KAMailData&, const char* folder);
		static bool        callKMail(const QByteArray& callData, const QCString& iface, const QCString& function, const QCString& funcType);
		static QString     convertAddress(KMime::Types::Address, EmailAddressList&);
		static void        notifyQueued(const KAEvent&);
		static char*       base64Encode(const char* in, Offset size, Offset& outSize);
		static QStringList errors(const QString& error = QString::null, bool sendfail = true);
};

#endif // KAMAIL_H
