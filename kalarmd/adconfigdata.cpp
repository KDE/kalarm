/*
 *  adcalendar.cpp  -  configuration file access
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

#include "kalarmd.h"

#include <qregexp.h>
#include <qstringlist.h>

#include <kconfig.h>
#include <kstandarddirs.h>
#include <kdebug.h>

#include "adcalendar.h"
#include "adconfigdata.h"

// Config file key strings
const QString CLIENT_GROUP(QString::fromLatin1("Client "));
const QRegExp CLIENT_GROUP_SEARCH("^Client ");
// Client data file key strings
const QString CALENDAR_KEY(QString::fromLatin1("Calendar"));
const QString TITLE_KEY(QString::fromLatin1("Title"));
const QString DCOP_OBJECT_KEY(QString::fromLatin1("DCOP object"));
const QString START_CLIENT_KEY(QString::fromLatin1("Start"));


/******************************************************************************
* Read the configuration file.
* Create the client list and open all calendar files.
*/
void ADConfigData::readConfig()
{
	kdDebug(5900) << "ADConfigData::readConfig()" << endl;
	ClientInfo::clear();
	KConfig* config = KGlobal::config();
	QStringList clients = config->groupList().grep(CLIENT_GROUP_SEARCH);
	for (QStringList::Iterator cl = clients.begin();  cl != clients.end();  ++cl)
	{
		// Read this client's configuration
		config->setGroup(*cl);
		QString client = *cl;
		client.remove(CLIENT_GROUP_SEARCH);
		QString  title       = config->readEntry(TITLE_KEY, client);   // read app title (default = app name)
		QCString dcopObject  = config->readEntry(DCOP_OBJECT_KEY).local8Bit();
		bool     startClient = config->readBoolEntry(START_CLIENT_KEY, false);
		QString  calendar    = config->readPathEntry(CALENDAR_KEY);

		// Verify the configuration
		bool ok = false;
		if (client.isEmpty()  ||  KStandardDirs::findExe(client).isNull())
			kdError(5900) << "ADConfigData::readConfig(): group '" << *cl << "' deleted (client app not found)\n";
		else if (calendar.isEmpty())
			kdError(5900) << "ADConfigData::readConfig(): no calendar specified for '" << client << "'\n";
		else if (dcopObject.isEmpty())
			kdError(5900) << "ADConfigData::readConfig(): no DCOP object specified for '" << client << "'\n";
		else
		{
			ADCalendar* cal = ADCalendar::getCalendar(calendar);
			if (cal)
				kdError(5900) << "ADConfigData::readConfig(): calendar registered by multiple clients: " << calendar << endl;
			else
				ok = true;
		}
		if (!ok)
		{
			config->deleteGroup(*cl, true);
			continue;
		}

		// Create the client and calendar objects
		new ClientInfo(client.local8Bit(), title, dcopObject, calendar, startClient);
		kdDebug(5900) << "ADConfigData::readConfig(): client " << client << " : calendar " << calendar << endl;
	}

	// Remove obsolete CheckInterval entry (if it exists)
        config->setGroup("General");
	config->deleteEntry("CheckInterval");

	// Save any updates
	config->sync();
}

/******************************************************************************
* Write a client application's details to the config file.
*/
void ADConfigData::writeClient(const QCString& appName, const ClientInfo* cinfo)
{
	KConfig* config = KGlobal::config();
	config->setGroup(CLIENT_GROUP + QString::fromLocal8Bit(appName));
	config->writeEntry(TITLE_KEY, cinfo->title());
	config->writeEntry(DCOP_OBJECT_KEY, QString::fromLocal8Bit(cinfo->dcopObject()));
	config->writeEntry(START_CLIENT_KEY, cinfo->startClient());
	config->writePathEntry(CALENDAR_KEY, cinfo->calendar()->urlString());
	config->sync();
}

/******************************************************************************
* Remove a client application's details from the config file.
*/
void ADConfigData::removeClient(const QCString& appName)
{
	KConfig* config = KGlobal::config();
	config->deleteGroup(CLIENT_GROUP + QString::fromLocal8Bit(appName));
	config->sync();
}

/******************************************************************************
* Set the calendar file URL for a specified application.
*/
void ADConfigData::setCalendar(const QCString& appName, ADCalendar* cal)
{
	KConfig* config = KGlobal::config();
	config->setGroup(CLIENT_GROUP + QString::fromLocal8Bit(appName));
	config->writePathEntry(CALENDAR_KEY, cal->urlString());
        config->sync();
}

/******************************************************************************
* DCOP call to set autostart at login on or off.
*/
void ADConfigData::enableAutoStart(bool on)
{
        kdDebug(5900) << "ADConfigData::enableAutoStart(" << on << ")\n";
        KConfig* config = KGlobal::config();
	config->reparseConfiguration();
        config->setGroup(QString::fromLatin1(DAEMON_AUTOSTART_SECTION));
        config->writeEntry(QString::fromLatin1(DAEMON_AUTOSTART_KEY), on);
        config->sync();
}

