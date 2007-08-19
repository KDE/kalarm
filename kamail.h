/*
 *  kamail.h  -  email functions
 *  Program:  kalarm
 *  Copyright © 2002-2007 by David Jarvie <software@astrojar.org.uk>
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

#ifndef KAMAIL_H
#define KAMAIL_H

#include <QString>
#include <QStringList>
class KUrl;
class KAEvent;
class EmailAddressList;
namespace KPIMIdentities { class IdentityManager; }
namespace KMime { namespace Types { struct Address; } }

struct KAMailData;


class KAMail
{
	public:
		static bool        send(const KAEvent&, QStringList& errmsgs, bool allowNotify = true);
		static int         checkAddress(QString& address);
		static int         checkAttachment(QString& attachment, KUrl* = 0);
		static bool        checkAttachment(const KUrl&);
		static QString     convertAddresses(const QString& addresses, EmailAddressList&);
		static QString     convertAttachments(const QString& attachments, QStringList& list);
		static KPIMIdentities::IdentityManager* identityManager();
		static bool        identitiesExist();
		static QString     controlCentreAddress();
		static QString     getMailBody(quint32 serialNumber);
		static QString     i18n_NeedFromEmailAddress();
		static QString     i18n_sent_mail();

	private:
		static KPIMIdentities::IdentityManager* mIdentityManager;     // KMail identity manager
		static QString     sendKMail(const KAMailData&);
		static QString     initHeaders(const KAMailData&, bool dateId);
		static QString     appendBodyAttachments(QString& message, const KAEvent&);
		static QString     addToKMailFolder(const KAMailData&, const char* folder, bool checkKmailRunning);
		static QString     convertAddress(KMime::Types::Address, EmailAddressList&);
		static void        notifyQueued(const KAEvent&);
		static QStringList errors(const QString& error = QString(), bool sendfail = true);
};

#endif // KAMAIL_H
