/*
 *  clientinfo.cpp  -  client application information
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  (C) 2001, 2004 by David Jarvie <software@astrojar.org.uk>
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

#include "adcalendar.h"
#include "clientinfo.h"

QMap<QCString, ClientInfo*> ClientInfo::mClients;


ClientInfo::ClientInfo(const QCString& appName, const QString& title,
                       const QCString& dcopObj, const QString& calendar, bool startClient)
	: mAppName(appName),
	  mTitle(title),
	  mDcopObject(dcopObj),
	  mCalendar(new ADCalendar(calendar, appName)),
	  mStartClient(startClient)
{
	mClients[mAppName] = this;
}

ClientInfo::ClientInfo(const QCString& appName, const QString& title,
                       const QCString& dcopObj, ADCalendar* calendar, bool startClient)
	: mAppName(appName),
	  mTitle(title),
	  mDcopObject(dcopObj),
	  mCalendar(calendar),
	  mStartClient(startClient)
{
	mClients[mAppName] = this;
}

ClientInfo::~ClientInfo()
{
	delete mCalendar;
	mClients.remove(mAppName);
}

/******************************************************************************
* Set a new calendar for the specified client application.
*/
ADCalendar* ClientInfo::setCalendar(const QString& url)
{
	if (url != mCalendar->urlString())
	{
		delete mCalendar;
		mCalendar = new ADCalendar(url, mAppName);
	}
	return mCalendar;
}

/******************************************************************************
* Return the ClientInfo object for the specified client application.
*/
ClientInfo* ClientInfo::get(const QCString& appName)
{
	if (appName.isEmpty())
		return 0;
	QMap<QCString, ClientInfo*>::ConstIterator it = mClients.find(appName);
	if (it == mClients.end())
		return 0;
	return it.data();
}

/******************************************************************************
* Return the ClientInfo object for client which owns the specified calendar.
*/
ClientInfo* ClientInfo::get(const ADCalendar* cal)
{
	for (ClientInfo::ConstIterator it = ClientInfo::begin();  it != ClientInfo::end();  ++it)
		if (it.data()->calendar() == cal)
			return it.data();
	return 0;
}

/******************************************************************************
* Delete all clients.
*/
void ClientInfo::clear()
{
	QMap<QCString, ClientInfo*>::Iterator it;
	while ((it = mClients.begin()) != mClients.end())
		delete it.data();
}

/******************************************************************************
* Delete the client with the specified name.
*/
void ClientInfo::remove(const QCString& appName)
{
	QMap<QCString, ClientInfo*>::Iterator it = mClients.find(appName);
	if (it != mClients.end())
		delete it.data();
}
