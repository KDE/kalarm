/*
    Alarm Daemon client data file access.

    This file is part of the KAlarm alarm daemon.
    Copyright (c) 2001, 2004 David Jarvie <software@astrojar.org.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

    As a special exception, permission is given to link this program
    with any edition of Qt, and distribute the resulting executable,
    without including the source code for Qt in the source distribution.
*/

#include <kstandarddirs.h>
#include <kurl.h>
#include <kdebug.h>
#include <ksimpleconfig.h>

#include "adconfigdatabase.h"

// The config file containing client and calendar information
#define CLIENT_DATA_FILE "clients"

// Config file key strings
const QCString ADConfigDataBase::CLIENT_KEY("Client_");
const QString ADConfigDataBase::CLIENTS_KEY("Clients");
const QCString ADConfigDataBase::GUI_KEY("Gui_");
const QString ADConfigDataBase::GUIS_KEY("Guis");
// Client data file key strings
const QString ADConfigDataBase::CLIENT_CALENDAR_KEY("Calendar");
const QString ADConfigDataBase::CLIENT_TITLE_KEY("Title");
const QString ADConfigDataBase::CLIENT_DCOP_OBJECT_KEY("DCOP object");
const QString ADConfigDataBase::CLIENT_NOTIFICATION_KEY("Notification");
const QString ADConfigDataBase::CLIENT_DISP_CAL_KEY("Display calendar names");


ADConfigDataBase::ADConfigDataBase(bool daemon)
  : mIsAlarmDaemon(daemon)
{
  mCalendars.setAutoDelete(true);
}

/*
 * Read all client applications from the client data file and store them in the client list.
 * Open all calendar files listed in the client data file and start monitoring them.
 * Calendar files are monitored until their client application registers, upon which
 * monitoring ceases until the client application tell the daemon to monitor it.
 * Reply = updated Clients option in main Alarm Daemon config file.
 */
QString ADConfigDataBase::readConfigData(bool sessionStarting, bool& deletedClients, bool& deletedCalendars,
                                         ADCalendarBaseFactory *calFactory)
{
  kdDebug(5900) << "ADConfigDataBase::readConfigData()" << endl;
  deletedClients   = false;
  deletedCalendars = false;
  if (mClientDataFile.isEmpty())
  {
    if (mIsAlarmDaemon)
      mClientDataFile = locateLocal("appdata", QString(CLIENT_DATA_FILE));
    else
      mClientDataFile = locate("data", QString("kalarmd/" CLIENT_DATA_FILE));
  }
  KSimpleConfig clientConfig(mClientDataFile);
  clientConfig.setGroup("General");
  QStrList clients;
  clientConfig.readListEntry(CLIENTS_KEY, clients);

  // Delete any clients which are no longer in the config file
  for (ClientList::Iterator cl = mClients.begin();  cl != mClients.end();  )
  {
    bool found = false;
    for (unsigned int i = 0;  i < clients.count();  ++i)
      if (clients.at(i) == (*cl).appName)
      {
        found = true;
        break;
      }
    if (!found)
    {
      // This client has disappeared. Remove it and its calendars
      for (ADCalendarBase* cal = mCalendars.first();  cal;  cal = mCalendars.next())
      {
        if (cal->appName() == (*cl).appName)
        {
          mCalendars.remove(cal);
          deletedCalendars = true;
        }
      }
      ClientList::Iterator c = cl;
      ++cl;                      // prevent iterator becoming invalid with remove()
      mClients.remove(c);
      deletedClients = true;
    }
    else
      ++cl;
  }

  // Update the clients and calendars lists with the new data
  bool writeNewClients = false;
  QString newClients;
  for (unsigned int i = 0;  i < clients.count();  ++i)
  {
    kdDebug(5900) << "ADConfigDataBase::readConfigData(): client: "
                  << clients.at(i) << endl;
    QCString client = clients.at(i);
    if ( client.isEmpty() ||
         KStandardDirs::findExe( client ).isNull() )
    {
      // Null client name, or application doesn't exist
      if (mIsAlarmDaemon)
      {
        if (!client.isEmpty())
          clientConfig.deleteGroup(CLIENT_KEY + client, true);
        writeNewClients = true;
      }
    }
    else
    {
      QString groupKey = CLIENT_KEY + client;

      // Get this client's details from its own config section.
      // If the client is already known, update its details.
      ClientInfo info = getClientInfo( client );
      if ( info.isValid() )
        removeClientInfo( client );
      clientConfig.setGroup(groupKey);
      QString  title      = clientConfig.readEntry(CLIENT_TITLE_KEY, client);   // read app title (default = app name)
      QCString dcopObject = clientConfig.readEntry(CLIENT_DCOP_OBJECT_KEY).local8Bit();
      int      type       = clientConfig.readNumEntry(CLIENT_NOTIFICATION_KEY, 0);
      bool displayCalName = clientConfig.readBoolEntry(CLIENT_DISP_CAL_KEY, true);
      info = ClientInfo( client, title, dcopObject, type, displayCalName, sessionStarting );
      mClients.append( info );

      // Get the client's calendar files
      QStrList newCalendars;       // to contain a list of calendars configured for this client
      int len = CLIENT_CALENDAR_KEY.length();
      QMap<QString, QString> entries = clientConfig.entryMap(groupKey);
      for (QMap<QString, QString>::ConstIterator it = entries.begin();  it != entries.end();  ++it)
      {
        if (it.key().startsWith(CLIENT_CALENDAR_KEY))
        {
          kdDebug(5900) << "ADConfigDataBase::readConfigData(): " << it.key() << "=" << it.data() << endl;
          bool ok;
          int rcIndex = it.key().mid(len).toInt(&ok);
          if (ok)
          {
            // The config file key is CalendarN, so open the calendar file
            QStringList items = QStringList::split(',', it.data());
            if (items.count() >= 2)
            {
              QString calname = items.last();
              if ( !calname.isEmpty() ) {
                ADCalendarBase* cal = getCalendar(calname);
                if (cal)
                {
                  // The calendar is already in the client's list, so remove
                  // this redundant client data file entry.
                  if (mIsAlarmDaemon)
                    deleteConfigCalendar(cal);
                }
                else
                {
                  // Add the calendar to the client's list
                  cal = calFactory->create(calname, client);
                  cal->setRcIndex(rcIndex);
                  mCalendars.append(cal);
                  kdDebug(5900) << "ADConfigDataBase::readConfigData(): calendar " << cal->urlString() << endl;
                }
                newCalendars.append(calname.latin1());
              }
            }
          }
        }
      }

      if (!newClients.isEmpty())
        newClients += ',';
      newClients += client;

      // Remove any previous calendars which were not in the client data file
      for (ADCalendarBase *cal = mCalendars.first();  cal;  )
      {
        kdDebug(5900) << "tick..." << endl;
        if (cal->appName() == client)
        {
          if (newCalendars.find(cal->urlString().latin1()) == -1) {
            deletedCalendars = true;
            mCalendars.remove();
            cal = mCalendars.current();
            continue;
          }
        }
        cal = mCalendars.next();
      }
    }
  }

  kdDebug(5900) << "ADConfigDataBase::readConfigData() done" << endl;

  return writeNewClients ? newClients : QString::null;
}

void ADConfigDataBase::deleteConfigCalendar(const ADCalendarBase*)
{
}

/* Return the ClientInfo structure for the specified client application */
ClientInfo ADConfigDataBase::getClientInfo(const QCString& appName)
{
  ClientList::Iterator it;
  for( it = mClients.begin(); it != mClients.end(); ++it ) {
    if ( (*it).appName == appName ) return *it;
  }
  return ClientInfo();
}

void ADConfigDataBase::removeClientInfo( const QCString &appName )
{
  ClientList::Iterator it;
  for( it = mClients.begin(); it != mClients.end(); ++it ) {
    if ( (*it).appName == appName ) {
      mClients.remove(it);
      break;
    }
  }
}

/* Return the ADCalendarBase structure for the specified full calendar URL */
ADCalendarBase *ADConfigDataBase::getCalendar(const QString& calendarURL)
{
  if (!calendarURL.isEmpty())
  {
    for (ADCalendarBase *cal = mCalendars.first();  cal;  cal = mCalendars.next())
    {
      if (cal->urlString() == calendarURL)
        return cal;
    }
  }
  return 0L;
}

/*
 * Expand a DCOP call parameter URL to a full URL.
 * (We must store full URLs in the calendar data since otherwise
 *  later calls to reload or remove calendars won't necessarily
 *  find a match.)
 */
QString ADConfigDataBase::expandURL(const QString& urlString)
{
  if (urlString.isEmpty())
    return QString();
  return KURL(urlString).url();
}

const QDateTime& ADConfigDataBase::baseDateTime()
{
  static const QDateTime bdt(QDate(1970,1,1), QTime(0,0,0));
  return bdt;
}
