/*
 *  clientinfo.h  -  client application information
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  Copyright (c) 2001, 2004, 2005 by David Jarvie <software@astrojar.org.uk>
 *  Based on the original, (c) 1998, 1999 Preston Brown
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

#ifndef CLIENTINFO_H
#define CLIENTINFO_H

#include <QByteArray>
#include <QString>
#include <QMap>

class ADCalendar;


/*=============================================================================
=  Class: ClientInfo
=  Details of a KAlarm client application.
=============================================================================*/
class ClientInfo
{
	public:
		typedef QMap<QByteArray, ClientInfo*>::ConstIterator ConstIterator;

		ClientInfo(const QByteArray& appName, const QString& title, const QByteArray& dcopObj,
		           const QString& calendar, bool startClient);
		ClientInfo(const QByteArray& appName, const QString& title, const QByteArray& dcopObj,
			   ADCalendar* calendar, bool startClient);
		~ClientInfo();
		ADCalendar*          setCalendar(const QString& url);
		void                 detachCalendar()            { mCalendar = 0; }
		void                 setStartClient(bool start)  { mStartClient = start; }

		QByteArray           appName() const             { return mAppName; }
		QString              title() const               { return mTitle; }
		QByteArray           dcopObject() const          { return mDcopObject; }
		ADCalendar*          calendar() const            { return mCalendar; }
		bool                 startClient() const         { return mStartClient; }

		static ConstIterator begin()                     { return mClients.begin(); }
		static ConstIterator end()                       { return mClients.end(); }
		static ClientInfo*   get(const QByteArray& appName);
		static ClientInfo*   get(const ADCalendar*);
		static void          remove(const QByteArray& appName);
		static void          clear();

	private:
		static QMap<QByteArray, ClientInfo*> mClients;  // list of all constructed clients
		QByteArray           mAppName;      // client's executable and DCOP name
		QString              mTitle;        // application title for display purposes
		QByteArray           mDcopObject;   // object to receive DCOP messages
		ADCalendar*          mCalendar;     // this client's event calendar
		bool                 mStartClient;  // whether to notify events via command line if client app isn't running
};

#endif
