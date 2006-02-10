/*
 *  clientinfo.cpp  -  client application information
 *  Program:  KAlarm's alarm daemon (kalarmd)
 *  Copyright (c) 2001, 2004, 2005 by David Jarvie <software@astrojar.org.uk>
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

#include "adcalendar.h"
#include "clientinfo.h"

QMap<QByteArray, ClientInfo*> ClientInfo::mClients;


ClientInfo::ClientInfo(const QByteArray& appName, const QString& title,
                       const QByteArray& dcopObj, const QString& calendar, bool startClient)
	: mAppName(appName),
	  mTitle(title),
	  mDcopObject(dcopObj),
	  mCalendar(new ADCalendar(calendar, appName)),
	  mStartClient(startClient)
{
	mClients[mAppName] = this;
}

ClientInfo::ClientInfo(const QByteArray& appName, const QString& title,
                       const QByteArray& dcopObj, ADCalendar* calendar, bool startClient)
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
ClientInfo* ClientInfo::get(const QByteArray& appName)
{
	if (appName.isEmpty())
		return 0;
	QMap<QByteArray, ClientInfo*>::ConstIterator it = mClients.find(appName);
	if (it == mClients.end())
		return 0;
	return it.value();
}

/******************************************************************************
* Return the ClientInfo object for client which owns the specified calendar.
*/
ClientInfo* ClientInfo::get(const ADCalendar* cal)
{
	for (ClientInfo::ConstIterator it = ClientInfo::begin();  it != ClientInfo::end();  ++it)
		if (it.value()->calendar() == cal)
			return it.value();
	return 0;
}

/******************************************************************************
* Delete all clients.
*/
void ClientInfo::clear()
{
	QMap<QByteArray, ClientInfo*>::Iterator it;
	while ((it = mClients.begin()) != mClients.end())
		delete it.value();
}

/******************************************************************************
* Delete the client with the specified name.
*/
void ClientInfo::remove(const QByteArray& appName)
{
	QMap<QByteArray, ClientInfo*>::Iterator it = mClients.find(appName);
	if (it != mClients.end())
		delete it.value();
}
