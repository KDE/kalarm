/*
 *  clientinfo.h  -  client application information
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  (C) 2001, 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _CALCLIENT_H
#define _CALCLIENT_H

#include <qcstring.h>
#include <qstring.h>
#include <qmap.h>

class ADCalendar;


/*=============================================================================
=  Class: ClientInfo
=  Details of a KAlarm client application.
=============================================================================*/
class ClientInfo
{
	public:
		typedef QMap<QCString, ClientInfo*>::ConstIterator ConstIterator;

		ClientInfo(const QCString &appName, const QString &title, const QCString &dcopObj,
		           const QString& calendar, bool startClient);
		ClientInfo(const QCString &appName, const QString &title, const QCString &dcopObj,
			   ADCalendar* calendar, bool startClient);
		~ClientInfo();
		ADCalendar*          setCalendar(const QString& url);
		void                 detachCalendar()            { mCalendar = 0; }
		void                 setStartClient(bool start)  { mStartClient = start; }

		QCString             appName() const             { return mAppName; }
		QString              title() const               { return mTitle; }
		QCString             dcopObject() const          { return mDcopObject; }
		ADCalendar*          calendar() const            { return mCalendar; }
		bool                 startClient() const         { return mStartClient; }

		static ConstIterator begin()                     { return mClients.begin(); }
		static ConstIterator end()                       { return mClients.end(); }
		static ClientInfo*   get(const QCString& appName);
		static ClientInfo*   get(const ADCalendar*);
		static void          remove(const QCString& appName);
		static void          clear();

	private:
		static QMap<QCString, ClientInfo*> mClients;  // list of all constructed clients
		QCString             mAppName;      // client's executable and DCOP name
		QString              mTitle;        // application title for display purposes
		QCString             mDcopObject;   // object to receive DCOP messages
		ADCalendar*          mCalendar;     // this client's event calendar
		bool                 mStartClient;  // whether to notify events via command line if client app isn't running
};

#endif
