/*
    Copyright (c) 2008 Bertjan Broeksema <broeksema@kde.org>
    Copyright (c) 2008 Volker Krause <vkrause@kde.org>

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

#include "singlefileresourcebase.h"

#include <akonadi/changerecorder.h>
#include <akonadi/entitydisplayattribute.h>
#include <akonadi/itemfetchscope.h>

#include <kio/job.h>
#include <kio/jobuidelegate.h>
#include <KDebug>
#include <KDirWatch>
#include <KLocale>
#include <KStandardDirs>

#include <QtCore/QDir>
#include <QtCore/QCryptographicHash>

using namespace Akonadi;

SingleFileResourceBase::SingleFileResourceBase( const QString & id )
  : ResourceBase( id ), mDownloadJob( 0 ), mUploadJob( 0 )
{
  connect( this, SIGNAL( reloadConfiguration() ), SLOT( reloadFile() ) );
  QTimer::singleShot( 0, this, SLOT( readFile() ) );

  changeRecorder()->itemFetchScope().fetchFullPayload();
  changeRecorder()->fetchCollection( true );

  connect( changeRecorder(), SIGNAL( changesAdded() ), SLOT( scheduleWrite() ) );

  connect( KDirWatch::self(), SIGNAL( dirty( QString ) ), SLOT( fileChanged( QString ) ) );
  connect( KDirWatch::self(), SIGNAL( created( QString ) ), SLOT( fileChanged( QString ) ) );

  KGlobal::locale()->insertCatalog( "akonadi_singlefile_resource" );
}

KSharedConfig::Ptr SingleFileResourceBase::runtimeConfig() const
{
  return KSharedConfig::openConfig( name() + "rc", KConfig::SimpleConfig, "cache" );
}

bool SingleFileResourceBase::readLocalFile( const QString &fileName )
{
  const QByteArray newHash = calculateHash( fileName );
  if ( mCurrentHash != newHash ) {
    if ( !mCurrentHash.isEmpty() ) {
      // There was a hash stored in the config file or a chached one from
      // a previous read and it is different from the hash we just read.
      handleHashChange();
    }

    if ( !readFromFile( fileName ) ) {
      mCurrentHash.clear();
      mCurrentUrl = KUrl(); // reset so we don't accidentally overwrite the file
      return false;
    }

    if ( mCurrentHash.isEmpty() ) {
      // This is the very first time we read the file so make sure to store
      // the hash as writeFile() might not be called at all (e.g in case of
      // read only resources).
      saveHash( newHash );
    }

    // Only synchronize when the contents of the file have changed wrt to
    // the last time this file was read. Before we synchronize first
    // clearCache is called to make sure that the cached items get the
    // actual values as present in the file.
    clearCache();
    synchronize();
  } else {
    // The hash didn't change, notify implementing resources about the
    // actual file name that should be used when reading the file is
    // necessary.
    setLocalFileName( fileName );
  }

  mCurrentHash = newHash;
  return true;
}

void SingleFileResourceBase::setLocalFileName( const QString &fileName )
{
  // Default implementation.
  if ( !readFromFile( fileName ) ) {
    mCurrentHash.clear();
    mCurrentUrl = KUrl(); // reset so we don't accidentally overwrite the file
    return;
  }
}

QString SingleFileResourceBase::cacheFile() const
{
  return KStandardDirs::locateLocal( "cache", "akonadi/" + identifier() );
}

QByteArray SingleFileResourceBase::calculateHash( const QString &fileName ) const
{
  QFile file( fileName );
  if ( !file.exists() )
    return QByteArray();

  if ( !file.open( QIODevice::ReadOnly ) )
    return QByteArray();

  QCryptographicHash hash( QCryptographicHash::Md5 );
  qint64 blockSize = 512 * 1024; // Read blocks of 512K

  while ( !file.atEnd() ) {
    hash.addData( file.read( blockSize ) );
  }

  file.close();

  return hash.result();
}

void SingleFileResourceBase::handleHashChange()
{
  // Default implementation does nothing.
  kDebug() << "The hash has changed.";
}

QByteArray SingleFileResourceBase::loadHash() const
{
  KConfigGroup generalGroup( runtimeConfig(), "General" );
  return QByteArray::fromHex( generalGroup.readEntry<QByteArray>( "hash", QByteArray() ) );
}

void SingleFileResourceBase::saveHash( const QByteArray &hash ) const
{
  KSharedConfig::Ptr config = runtimeConfig();
  KConfigGroup generalGroup( config, "General" );
  generalGroup.writeEntry( "hash", hash.toHex() );
  config->sync();
}

void SingleFileResourceBase::setSupportedMimetypes( const QStringList & mimeTypes, const QString &icon )
{
  mSupportedMimetypes = mimeTypes;
  mCollectionIcon = icon;
}

void SingleFileResourceBase::collectionChanged( const Akonadi::Collection & collection )
{
  QString newName = collection.name();
  if ( collection.hasAttribute<EntityDisplayAttribute>() ) {
    EntityDisplayAttribute *attr = collection.attribute<EntityDisplayAttribute>();
    if ( !attr->displayName().isEmpty() )
      newName = attr->displayName();
  }

  if ( newName != name() )
    setName( newName );

  changeCommitted( collection );
}

void SingleFileResourceBase::reloadFile()
{
  // Update the network setting.
  setNeedsNetwork( !mCurrentUrl.isEmpty() && !mCurrentUrl.isLocalFile() );

  // if we have something loaded already, make sure we write that back in case
  // the settings changed
  if ( !mCurrentUrl.isEmpty() && !readOnly() )
    writeFile();

  readFile();

  // name or rights could have changed
  synchronizeCollectionTree();
}

void SingleFileResourceBase::handleProgress( KJob *, unsigned long pct )
{
  emit percent( pct );
}

void SingleFileResourceBase::fileChanged( const QString & fileName )
{
  if ( fileName != mCurrentUrl.toLocalFile() )
    return;

  const QByteArray newHash = calculateHash( fileName );

  // There is only a need to synchronize when the file was changed by another
  // process. At this point we're sure that it is the file that the resource
  // was configured for because of the check at the beginning of this function.
  if ( newHash == mCurrentHash )
    return;

  if ( !mCurrentUrl.isEmpty() ) {
    QString lostFoundFileName;
    const KUrl prevUrl = mCurrentUrl;
    int i = 0;
    do {
      lostFoundFileName = KStandardDirs::locateLocal( "data", identifier() + QDir::separator()
          + prevUrl.fileName() + '-' + QString::number( ++i ) );
    } while ( KStandardDirs::exists( lostFoundFileName ) );

    // create the directory if it doesn't exist yet
    QDir dir = QFileInfo(lostFoundFileName).dir();
    if ( !dir.exists() )
      dir.mkpath( dir.path() );

    mCurrentUrl = KUrl( lostFoundFileName );
    writeFile();
    mCurrentUrl = prevUrl;

    emit warning( i18n( "The file '%1' was changed on disk while there were still pending changes in Akonadi. "
      "To avoid data loss, a backup of the internal changes has been created at '%2'.",
      prevUrl.prettyUrl(), KUrl( lostFoundFileName ).prettyUrl() ) );
  }

  readFile();

  // Notify resources, so that information bound to the file like indexes etc.
  // can be updated.
  handleHashChange();
  clearCache();
  synchronize();
}

void SingleFileResourceBase::scheduleWrite()
{
  scheduleCustomTask(this, "writeFile", QVariant(true), ResourceBase::AfterChangeReplay);
}

void SingleFileResourceBase::slotDownloadJobResult( KJob *job )
{
  if ( job->error() && job->error() != KIO::ERR_DOES_NOT_EXIST ) {
    emit status( Broken, i18n( "Could not load file '%1'.", mCurrentUrl.prettyUrl() ) );
  } else {
    readLocalFile( KUrl( cacheFile() ).toLocalFile() );
  }

  mDownloadJob = 0;
  KGlobal::deref();

  emit status( Idle, i18nc( "@info:status", "Ready" ) );
}

void SingleFileResourceBase::slotUploadJobResult( KJob *job )
{
  if ( job->error() ) {
    emit status( Broken, i18n( "Could not save file '%1'.", mCurrentUrl.prettyUrl() ) );
  }

  mUploadJob = 0;
  KGlobal::deref();

  emit status( Idle, i18nc( "@info:status", "Ready" ) );
}

#include "singlefileresourcebase.moc"
