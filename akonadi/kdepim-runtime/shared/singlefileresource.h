/*
    Copyright (c) 2008 Bertjan Broeksema <broeksema@kde.org>
    Copyright (c) 2008 Volker Krause <vkrause@kde.org>
    Copyright (c) 2010 David Jarvie <djarvie@kde.org>

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

#ifndef AKONADI_SINGLEFILERESOURCE_H
#define AKONADI_SINGLEFILERESOURCE_H

#include "singlefileresourcebase.h"
#include "singlefileresourceconfigdialog.h"

#include <akonadi/entitydisplayattribute.h>

#include <kio/job.h>
#include <KDirWatch>
#include <KLocale>
#include <KStandardDirs>

#include <QFile>
#include <QDir>
#include <QPointer>

namespace Akonadi
{

/**
 * Base class for single file based resources.
 */
template <typename Settings>
class SingleFileResource : public SingleFileResourceBase
{
  public:
    SingleFileResource( const QString &id )
      : SingleFileResourceBase( id )
      , mSettings( new Settings( componentData().config() ) )
    {
      // The resource needs network when the path refers to a non local file.
      setNeedsNetwork( !KUrl( mSettings->path() ).isLocalFile() );
    }

    /**
     * Read changes from the backend file.
     */
    void readFile( bool taskContext = false )
    {
      if ( KDirWatch::self()->contains( mCurrentUrl.toLocalFile() ) )
        KDirWatch::self()->removeFile( mCurrentUrl.toLocalFile() );

      if ( mSettings->path().isEmpty() ) {
        emit status( Broken, i18n( "No file selected." ) );
        if ( taskContext )
          cancelTask();
        return;
      }

      mCurrentUrl = KUrl( mSettings->path() );
      if ( mCurrentHash.isEmpty() ) {
        // First call to readFile() lets see if there is a hash stored in a
        // cache file. If both are the same than there is no need to load the
        // file and synchronize the resource.
        mCurrentHash = loadHash();
      }

      if ( mCurrentUrl.isLocalFile() )
      {
        if ( mSettings->displayName().isEmpty()
        && ( name().isEmpty() || name() == identifier() ) && !mCurrentUrl.isEmpty() )
          setName( mCurrentUrl.fileName() );

        // check if the file does not exist yet, if so, create it
        if ( !QFile::exists( mCurrentUrl.toLocalFile() ) ) {
          QFile f( mCurrentUrl.toLocalFile() );

          // first create try to create the directory the file should be located in
          QDir dir = QFileInfo(f).dir();
          if ( ! dir.exists() ) {
            dir.mkpath( dir.path() );
          }

          if ( f.open( QIODevice::WriteOnly ) && f.resize( 0 ) ) {
            emit status( Idle, i18nc( "@info:status", "Ready" ) );
          } else {
            emit status( Broken, i18n( "Could not create file '%1'.", mCurrentUrl.prettyUrl() ) );
            mCurrentUrl.clear();
            if ( taskContext )
              cancelTask();
            return;
          }
        }

        // Cache, because readLocalFile will clear mCurrentUrl on failure.
        const QString localFileName = mCurrentUrl.toLocalFile();
        if ( !readLocalFile( mCurrentUrl.toLocalFile() ) ) {
          emit status( Broken, i18n( "Could not read file '%1'", localFileName ) );
          if ( taskContext )
            cancelTask();
          return;
        }

        if ( mSettings->monitorFile() )
          KDirWatch::self()->addFile( mCurrentUrl.toLocalFile() );

        emit status( Idle, i18nc( "@info:status", "Ready" ) );
      }
      else // !mCurrentUrl.isLocalFile()
      {
        if ( mDownloadJob )
        {
          emit error( i18n( "Another download is still in progress." ) );
          if ( taskContext )
            cancelTask();
          return;
        }

        if ( mUploadJob )
        {
          emit error( i18n( "Another file upload is still in progress." ) );
          if ( taskContext )
            cancelTask();
          return;
        }

        KGlobal::ref();

        // NOTE: Test what happens with remotefile -> save, close before save is finished.
        mDownloadJob = KIO::file_copy( mCurrentUrl, KUrl( cacheFile() ), -1, KIO::Overwrite | KIO::DefaultFlags | KIO::HideProgressInfo );
        connect( mDownloadJob, SIGNAL( result( KJob * ) ),
                SLOT( slotDownloadJobResult( KJob * ) ) );
        connect( mDownloadJob, SIGNAL( percent( KJob *, unsigned long ) ),
                 SLOT( handleProgress( KJob *, unsigned long ) ) );

        emit status( Running, i18n( "Downloading remote file." ) );
      }

      const QString display =  mSettings->displayName();
      if ( !display.isEmpty() ) {
        setName( display );
      }
    }

    void writeFile( const QVariant &task_context )
    {
      writeFile( task_context.canConvert<bool>() && task_context.toBool() );
    }

    /**
     * Write changes to the backend file.
     */
    void writeFile( bool taskContext = false )
    {
      if ( mSettings->readOnly() ) {
        emit error( i18n( "Trying to write to a read-only file: '%1'.", mSettings->path() ) );
        if ( taskContext )
          cancelTask();
        return;
      }

      // We don't use the Settings::self()->path() here as that might have changed
      // and in that case it would probably cause data lose.
      if ( mCurrentUrl.isEmpty() ) {
        emit status( Broken, i18n( "No file specified." ) );
        if ( taskContext )
          cancelTask();
        return;
      }

      if ( mCurrentUrl.isLocalFile() ) {
        KDirWatch::self()->stopScan();
        const bool writeResult = writeToFile( mCurrentUrl.toLocalFile() );
        // Update the hash so we can detect at fileChanged() if the file actually
        // did change.
        mCurrentHash = calculateHash( mCurrentUrl.toLocalFile() );
        saveHash( mCurrentHash );
        KDirWatch::self()->startScan();
        if ( !writeResult )
        {
          if ( taskContext )
            cancelTask();
          return;
        }
        emit status( Idle, i18nc( "@info:status", "Ready" ) );

      } else {
        // Check if there is a download or an upload in progress.
        if ( mDownloadJob ) {
          emit error( i18n( "A download is still in progress." ) );
          if ( taskContext )
            cancelTask();
          return;
        }

        if ( mUploadJob ) {
          emit error( i18n( "Another file upload is still in progress." ) );
          if ( taskContext )
            cancelTask();
          return;
        }

        // Write te items to the localy cached file.
        if ( !writeToFile( cacheFile() ) )
        {
          if ( taskContext )
            cancelTask();
          return;
        }

        // Update the hash so we can detect at fileChanged() if the file actually
        // did change.
        mCurrentHash = calculateHash( cacheFile() );
        saveHash( mCurrentHash );

        KGlobal::ref();
        // Start a job to upload the localy cached file to the remote location.
        mUploadJob = KIO::file_copy( KUrl( cacheFile() ), mCurrentUrl, -1, KIO::Overwrite | KIO::DefaultFlags | KIO::HideProgressInfo );
        connect( mUploadJob, SIGNAL( result( KJob * ) ),
                SLOT( slotUploadJobResult( KJob * ) ) );
        connect( mUploadJob, SIGNAL( percent( KJob *, unsigned long ) ),
                 SLOT( handleProgress( KJob *, unsigned long ) ) );

        emit status( Running, i18n( "Uploading cached file to remote location." ) );
      }
      if ( taskContext )
        taskDone();
    }

    virtual void collectionChanged( const Collection &collection )
    {
      QString newName;
      if ( collection.hasAttribute<EntityDisplayAttribute>() ) {
        EntityDisplayAttribute *attr = collection.attribute<EntityDisplayAttribute>();
        newName = attr->displayName();
      }
      const QString oldName = mSettings->displayName();
      if ( newName != oldName ) {
        mSettings->setDisplayName( newName );
        mSettings->writeConfig();
      }
      SingleFileResourceBase::collectionChanged( collection );
    }

  public Q_SLOTS:
    /**
     * Display the configuration dialog for the resource.
     */
    void configure( WId windowId )
    {
      QPointer<SingleFileResourceConfigDialog<Settings> > dlg
          = new SingleFileResourceConfigDialog<Settings>( windowId, mSettings );
      customizeConfigDialog( dlg );
      if ( dlg->exec() == QDialog::Accepted ) {
        if ( dlg ) {   // in case is got destroyed
          configDialogAcceptedActions( dlg );
        }
        reloadFile();
        synchronizeCollectionTree();
        emit configurationDialogAccepted();
      } else {
        emit configurationDialogRejected();
      }
      delete dlg;
    }

  protected:
    /**
     * Implement in derived classes to customize the configuration dialog
     * before it is displayed.
     */
    virtual void customizeConfigDialog( SingleFileResourceConfigDialog<Settings>* dlg )
    {
      Q_UNUSED(dlg);
    }

    /**
     * Implement in derived classes to do things when the configuration dialog
     * has been accepted, before reloadFile() is called.
     */
    virtual void configDialogAcceptedActions( SingleFileResourceConfigDialog<Settings>* dlg )
    {
      Q_UNUSED(dlg);
    }

    void retrieveCollections()
    {
      Collection c;
      c.setParentCollection( Collection::root() );
      c.setRemoteId( mSettings->path() );
      const QString display = mSettings->displayName();
      c.setName( display.isEmpty() ? identifier() : display );
      QStringList mimeTypes;
      c.setContentMimeTypes( mSupportedMimetypes );
      if ( readOnly() ) {
        c.setRights( Collection::CanChangeCollection );
      } else {
        Collection::Rights rights;
        rights |= Collection::CanChangeItem;
        rights |= Collection::CanCreateItem;
        rights |= Collection::CanDeleteItem;
        rights |= Collection::CanChangeCollection;
        c.setRights( rights );
      }
      EntityDisplayAttribute* attr = c.attribute<EntityDisplayAttribute>( Collection::AddIfMissing );
      attr->setDisplayName( name() );
      attr->setIconName( mCollectionIcon );

      Collection::List list;
      list << c;
      collectionsRetrieved( list );
    }

    void retrieveItems( const Akonadi::Collection &collection )
    {
      mCollectionId = collection.id();
    }

    bool readOnly() const
    {
      return mSettings->readOnly();
    }

    Collection::Id collectionId() const
    {
      return mCollectionId;
    }

  protected:
    Settings *mSettings;

  private:
    Collection::Id mCollectionId;  //!< the one and only collection for this resource
};

}

#endif
