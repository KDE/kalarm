/*
    Client data access for KDE Alarm Daemon.

    This file is part of the KDE alarm daemon.
    Copyright (c) 2001 David Jarvie <software@astrojar.org.uk>

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

#include <kdebug.h>
#include <kstandarddirs.h>

#include "adcalendar.h"

#include "adconfigdatarw.h"

void ADConfigDataRW::readDaemonData(bool sessionStarting)
{
  kdDebug(5900) << "ADConfigDataRW::readDaemonData()" << endl;

  KSimpleConfig clientConfig(clientDataFile());

  ADCalendarFactory calFactory;
  bool cls, cals;
  QString newClients = readConfigData(sessionStarting, cls, cals, &calFactory);
  if (!newClients.isEmpty())
  {
    // One or more clients in the Clients config entry was invalid, so rewrite the entry
    clientConfig.setGroup("General");
    clientConfig.writeEntry(CLIENTS_KEY, newClients);
  }

  // Read the GUI clients
  QStrList guis;
  clientConfig.readListEntry(GUIS_KEY, guis);
  bool writeNewGuis = false;
  QString newGuis;
  for (unsigned int i = 0;  i < guis.count();  ++i)
  {
    QCString gui = guis.at(i);
    kdDebug(5900) << "ADConfigDataRW::readDaemonData(): gui: " << gui << endl;
    if (gui.isEmpty()
    ||  KStandardDirs::findExe(gui).isNull())
    {
      // Null client name, or application doesn't exist
      if (!gui.isEmpty())
        clientConfig.deleteGroup(GUI_KEY + gui, true);
      writeNewGuis = true;
    }
    else
    {
      // Get this client's details from its own config section
      QString groupKey = GUI_KEY + gui;
      clientConfig.setGroup(groupKey);
      QCString dcopObject = clientConfig.readEntry(CLIENT_DCOP_OBJECT_KEY).local8Bit();
      mGuis.insert(gui, dcopObject);
      if (!newGuis.isEmpty())
        newGuis += ',';
      newGuis += gui;
    }
  }
  if (writeNewGuis)
  {
    // One or more clients in the Guis config entry was invalid, so rewrite the entry
    clientConfig.setGroup("General");
    clientConfig.writeEntry(GUIS_KEY, newGuis);
  }
}

/*
 * Write a client application's details to the client data file.
 * Any existing entries relating to the application are deleted,
 * including calendar file information.
 */
void ADConfigDataRW::writeConfigClient(const QCString& appName, const ClientInfo& cinfo)
{
  KSimpleConfig clientConfig(clientDataFile());
  addConfigClient(clientConfig, appName, CLIENTS_KEY);

  QString groupKey = CLIENT_KEY + appName;
  clientConfig.deleteGroup(groupKey, true);

  clientConfig.setGroup(groupKey);
  clientConfig.writeEntry(CLIENT_TITLE_KEY, cinfo.title);
  if (!cinfo.dcopObject.isEmpty())
    clientConfig.writeEntry(CLIENT_DCOP_OBJECT_KEY, QString::fromLocal8Bit(cinfo.dcopObject));
  clientConfig.writeEntry(CLIENT_NOTIFICATION_KEY, cinfo.notificationType);
  clientConfig.writeEntry(CLIENT_DISP_CAL_KEY, cinfo.displayCalName);
  int i = 0;
  for (ADCalendarBase* cal = mCalendars.first();  cal;  cal = mCalendars.next())
  {
    if (cal->appName() == appName)
    {
      cal->setRcIndex(++i);
      writeConfigCalendar(cal, clientConfig);
    }
  }
}

/*
 * Write a GUI client application's details to the client data file.
 */
void ADConfigDataRW::writeConfigClientGui(const QCString& appName, const QString& dcopObject)
{
  KSimpleConfig clientConfig(clientDataFile());
  addConfigClient(clientConfig, appName, GUIS_KEY);

  QString groupKey = GUI_KEY + appName;

  clientConfig.setGroup(groupKey);
  clientConfig.writeEntry(CLIENT_DCOP_OBJECT_KEY, dcopObject);
}

/*
 * Add a client application's name to the client data file list.
 */
void ADConfigDataRW::addConfigClient(KSimpleConfig& clientConfig, const QCString& appName, const QString& key)
{
  clientConfig.setGroup("General");
  QStringList clients = QStringList::split(',', clientConfig.readEntry(key), true);
  if (clients.find(appName) == clients.end())
  {
    // It's a new client, so add it to the Clients config file entry
    for (QStringList::Iterator i = clients.begin();  i != clients.end();  )
    {
      if ((*i).isEmpty())
        i = clients.remove(i);    // remove null entries
      else
        ++i;
    }
    clients.append(appName);
    clientConfig.writeEntry(key, clients.join(","));
  }
}

// Add a calendar file URL to the client data file for a specified application.
void ADConfigDataRW::addConfigCalendar(const QCString& appName, ADCalendarBase* cal)
{
  KSimpleConfig clientConfig(clientDataFile());
  QString groupKey = CLIENT_KEY + appName;
  QMap<QString, QString> entries = clientConfig.entryMap(groupKey);
  // Find an unused CalendarN entry for this calendar
  for (int i = 1;  ;  ++i)
  {
    QString key = CLIENT_CALENDAR_KEY + QString::number(i);
    if (entries.find(key) == entries.end())
    {
      // This calendar index is unused, so use it for the new calendar
      cal->setRcIndex(i);
      writeConfigCalendar(cal, clientConfig);
      return;
    }
  }
}

// Update a calendar file entry in the client data file.
void ADConfigDataRW::writeConfigCalendar(const ADCalendarBase* cal)
{
  if (cal->rcIndex() > 0)
  {
    KSimpleConfig clientConfig(clientDataFile());
    writeConfigCalendar(cal, clientConfig);
  }
}

// Update a calendar file entry in the client data file.
void ADConfigDataRW::writeConfigCalendar(const ADCalendarBase* cal, KSimpleConfig& clientConfig)
{
  if (cal->rcIndex() > 0)
  {
    clientConfig.setGroup(CLIENT_KEY + cal->appName());
    QString dt;
    if (cal->lastCheck().isValid())
      dt = QString::number(baseDateTime().secsTo(cal->lastCheck()));
    clientConfig.writeEntry(CLIENT_CALENDAR_KEY + QString::number(cal->rcIndex()),
                            QString("%1,%2,%3").arg(cal->actionType()).arg(dt).arg(cal->urlString()));
  }
}

/*
 * Delete all entries in the client data file for the specified calendar
 */
void ADConfigDataRW::deleteConfigCalendar(const ADCalendarBase* cal)
{
  KSimpleConfig clientConfig(clientDataFile());
  QString groupKey = CLIENT_KEY + cal->appName();
  int len = CLIENT_CALENDAR_KEY.length();
  QMap<QString, QString> entries = clientConfig.entryMap(groupKey);
  for (QMap<QString, QString>::ConstIterator it = entries.begin();  it != entries.end();  ++it)
  {
    if (it.key().startsWith(CLIENT_CALENDAR_KEY))
    {
      bool ok;
      it.key().mid(len).toInt(&ok);
      if (ok)
      {
        // The config file key is CalendarN
        int comma = it.data().find(',');
        if (comma >= 0  &&  it.data().mid(comma + 1) == cal->urlString())
        {
          clientConfig.setGroup(groupKey);
          clientConfig.deleteEntry(it.key(), true);
        }
      }
    }
  }
}

/*
 * Flush changes in the client data file to disc.
 */
void ADConfigDataRW::sync()
{
  KSimpleConfig clientConfig(clientDataFile());
  clientConfig.sync();
}
