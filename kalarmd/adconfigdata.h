/*
 *  adconfigdata.h  -  configuration file access
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

#ifndef ADCONFIGDATA_H
#define ADCONFIGDATA_H

#include "clientinfo.h"

class ADCalendar;
class ClientInfo;


class ADConfigData
{
	public:
		static void readConfig();
		static void writeClient(const QCString& appName, const ClientInfo*);
		static void removeClient(const QCString& appName);
		static void setCalendar(const QCString& appName, ADCalendar*);
		static void enableAutoStart(bool);
};

#endif // ADCONFIGDATA_H
