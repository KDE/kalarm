/*
    Calendar access for KDE Alarm Daemon.

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

#include <unistd.h>
#include <time.h>

#include <qfile.h>

#include <kstandarddirs.h>
#include <ksimpleconfig.h>
#include <ktempfile.h>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kdebug.h>

#include "adcalendarbase.h"
#include "adcalendarbase.moc"

ADCalendarBase::EventsMap ADCalendarBase::eventsHandled_;

ADCalendarBase::ADCalendarBase(const QString& url, const QCString& appname, Type type)
  : mUrlString(url),
    mAppName(appname),
    mActionType(type),
    mRcIndex(-1),
    mLoaded(false),
    mLoadedConnected(false),
    mUnregistered(false)
{
  if (mAppName == "korgac")
  {
    KConfig cfg( locate( "config", "korganizerrc" ) );
    cfg.setGroup( "Time & Date" );
    QString tz = cfg.readEntry( "TimeZoneId" );
    kdDebug(5900) << "ADCalendarBase(): tz: " << tz << endl;
    if( tz.isEmpty() ) {
      // Set a reasonable default timezone is none
      // was found in the config file
      // see koprefs.cpp in korganizer
      QString zone;
      char zonefilebuf[100];
      int len = readlink("/etc/localtime",zonefilebuf,100);
      if (len > 0 && len < 100) {
	zonefilebuf[len] = '\0';
	zone = zonefilebuf;
	zone = zone.mid(zone.find("zoneinfo/") + 9);
      } else {
	tzset();
	zone = tzname[0];
      }
      tz = zone;
    }
    setTimeZoneId( tz );
  }
}

/*
 * Load the calendar file.
 */
bool ADCalendarBase::loadFile_()
{
  if ( !mTempFileName.isNull() ) {
    // Don't try to load the file if already downloading it
    kdError(5900) << "ADCalendarBase::loadFile_(): already downloading another file\n";
    return false;
  }
  mLoaded = false;
  KURL url( mUrlString );
  if ( url.isLocalFile() ) {
    // It's a local file
    loadLocalFile( url.path() );
    emit loaded( this, mLoaded );
  } else {
    // It's a remote file. Download to a temporary file before loading it.
    KTempFile tempFile;
    mTempFileName = tempFile.name();
    KURL dest;
    dest.setPath( mTempFileName );
    KIO::FileCopyJob *job = KIO::file_copy( url, dest, -1, true );
    connect( job, SIGNAL( result( KIO::Job * ) ),
             SLOT( slotDownloadJobResult( KIO::Job * ) ) );
  }
  return true;
}

void ADCalendarBase::slotDownloadJobResult( KIO::Job *job )
{
  if ( job->error() ) {
    KURL url( mUrlString );
    kdDebug(5900) << "Error downloading calendar from " << url.prettyURL() << endl;
    job->showErrorDialog( 0 );
  } else {
    kdDebug(5900) << "--- Downloaded to " << mTempFileName << endl;
    loadLocalFile( mTempFileName );
  }
  unlink( QFile::encodeName( mTempFileName ) );
  mTempFileName = QString::null;
  emit loaded( this, mLoaded );
}

void ADCalendarBase::loadLocalFile( const QString& filename )
{
  mLoaded = load( filename );
  if (!mLoaded)
    kdDebug(5900) << "ADCalendarBase::loadLocalFile(): Error loading calendar file '" << filename << "'\n";
  else
  {
    // Remove all now non-existent events from the handled list
    for (EventsMap::Iterator it = eventsHandled_.begin();  it != eventsHandled_.end();  )
    {
      if (it.data().calendarURL == mUrlString  &&  !event(it.key()))
      {
        // Event belonged to this calendar, but can't find it any more
        EventsMap::Iterator i = it;
        ++it;                      // prevent iterator becoming invalid with remove()
        eventsHandled_.remove(i);
      }
      else
        ++it;
    }
  }
}

bool ADCalendarBase::setLoadedConnected()
{
  if (mLoadedConnected)
    return true;
  mLoadedConnected = true;
  return false;
}

void ADCalendarBase::dump() const
{
  kdDebug(5900) << "  <calendar>" << endl;
  kdDebug(5900) << "    <url>" << urlString() << "</url>" << endl;
  kdDebug(5900) << "    <appname>" << appName() << "</appname>" << endl;
  if ( loaded() ) kdDebug(5900) << "    <loaded/>" << endl;
  kdDebug(5900) << "    <actiontype>" << int(actionType()) << "</actiontype>" << endl;
  if (enabled() ) kdDebug(5900) << "    <enabled/>" << endl;
  else kdDebug(5900) << "    <disabled/>" << endl;
  if (available()) kdDebug(5900) << "    <available/>" << endl;
  kdDebug(5900) << "  </calendar>" << endl;
}
