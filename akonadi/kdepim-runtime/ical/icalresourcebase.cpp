/*
    Copyright (c) 2006 Till Adam <adam@kde.org>
    Copyright (c) 2009 David Jarvie <djarvie@kde.org>

    This library is free software; you can redistribute it and/or modify it
    under the terms of the GNU Library General Public License as published by
    the Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    This library is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
    License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to the
    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.
*/

#include "icalresourcebase.h"
#include "icalsettingsadaptor.h"
#include "singlefileresourceconfigdialog.h"

#include <akonadi/dbusconnectionpool.h>

#include <kcalcore/filestorage.h>
#include <kcalcore/memorycalendar.h>
#include <kcalcore/incidence.h>
#include <kcalcore/icalformat.h>

#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>

using namespace Akonadi;
using namespace KCalCore;
using namespace SETTINGS_NAMESPACE;

ICalResourceBase::ICalResourceBase( const QString &id )
  : SingleFileResource<Settings>( id )
{
  KGlobal::locale()->insertCatalog( "akonadi_ical_resource" );
}

void ICalResourceBase::initialise( const QStringList &mimeTypes, const QString &icon )
{
  setSupportedMimetypes( mimeTypes, icon );
  new ICalSettingsAdaptor( mSettings );
  DBusConnectionPool::threadConnection().registerObject( QLatin1String( "/Settings" ),
                                                         mSettings, QDBusConnection::ExportAdaptors );
}

ICalResourceBase::~ICalResourceBase()
{
}

bool ICalResourceBase::retrieveItem( const Akonadi::Item &item,
                                     const QSet<QByteArray> &parts )
{
  kDebug( 5251 ) << "Item:" << item.url();

  if ( !mCalendar ) {
    emit error( i18n("Calendar not loaded.") );
    return false;
  }

  return doRetrieveItem( item, parts );
}

void ICalResourceBase::aboutToQuit()
{
  if ( !mSettings->readOnly() ) {
    writeFile();
  }
  mSettings->writeConfig();
}

void ICalResourceBase::customizeConfigDialog( SingleFileResourceConfigDialog<Settings> *dlg )
{
#ifndef KDEPIM_MOBILE_UI
  dlg->setFilter( "text/calendar" );
#else
  dlg->setFilter( "*.ics *.vcs" );
#endif
  dlg->setCaption( i18n("Select Calendar") );
}

bool ICalResourceBase::readFromFile( const QString &fileName )
{
  mCalendar = KCalCore::MemoryCalendar::Ptr( new KCalCore::MemoryCalendar( QLatin1String( "UTC" ) ) );
  mFileStorage = KCalCore::FileStorage::Ptr( new KCalCore::FileStorage( mCalendar, fileName,
                                                                        new KCalCore::ICalFormat() ) );
  const bool result = mFileStorage->load();
  if ( !result ) {
    kError() << "Error loading file " << fileName;
  }

  return result;
}

void ICalResourceBase::itemRemoved( const Akonadi::Item &item )
{
  if ( !mCalendar ) {
    kError() << "mCalendar is 0!";
    cancelTask( i18n("Calendar not loaded.") );
    return;
  }

  Incidence::Ptr i = mCalendar->incidence( item.remoteId() );
  if ( i ) {
    if ( !mCalendar->deleteIncidence( i ) ) {
      kError() << "Can't delete incidence with uid " << item.remoteId()
               << "; item.id() = " << item.id();
      cancelTask();
      return;
    }
  } else {
    kError() << "Can't find incidence with uid " << item.remoteId()
             << "; item.id() = " << item.id();
  }
  scheduleWrite();
  changeProcessed();
}

void ICalResourceBase::retrieveItems( const Akonadi::Collection &col )
{
  SingleFileResource<Settings>::retrieveItems( col );
  reloadFile();
  if ( mCalendar ) {
    doRetrieveItems( col );
  } else {
    kError() << "mCalendar is 0!";
  }
}

bool ICalResourceBase::writeToFile( const QString &fileName )
{
  if ( !mCalendar ) {
    kError() << "mCalendar is 0!";
    return false;
  }

  KCalCore::FileStorage *fileStorage = mFileStorage.data();
  if ( fileName != mFileStorage->fileName() ) {
    fileStorage = new KCalCore::FileStorage( mCalendar,
                                             fileName,
                                             new KCalCore::ICalFormat() );
  }

  bool success = true;
  if ( !fileStorage->save() ) {
    kError() << "Failed to save calendar to file " + fileName;
    emit error( i18n("Failed to save calendar file to %1", fileName ) );
    success = false;
  }

  if ( fileStorage != mFileStorage.data() ) {
    delete fileStorage;
  }

  return success;
}

KCalCore::MemoryCalendar::Ptr ICalResourceBase::calendar() const
{
  return mCalendar;
}

KCalCore::FileStorage::Ptr ICalResourceBase::fileStorage() const
{
  return mFileStorage;
}


#include "icalresourcebase.moc"
