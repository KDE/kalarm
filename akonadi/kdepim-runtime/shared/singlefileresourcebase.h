/*
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

#ifndef AKONADI_SINGLEFILERESOURCEBASE_H
#define AKONADI_SINGLEFILERESOURCEBASE_H

#include <akonadi/resourcebase.h>

#include <KDE/KUrl>
#include <QtCore/QStringList>
#include <QtCore/QTimer>

namespace KIO {
class FileCopyJob;
class Job;
}

namespace Akonadi
{

/**
 * Base class for single file based resources.
 * @see SingleFileResource
 */
class SingleFileResourceBase : public ResourceBase, public AgentBase::Observer
{
  Q_OBJECT
  public:
    SingleFileResourceBase( const QString &id );

    /**
     * Set the mimetypes supported by this resource and an optional icon for the collection.
     */
    void setSupportedMimetypes( const QStringList &mimeTypes, const QString &icon = QString() );

    void collectionChanged( const Akonadi::Collection &collection );

  public Q_SLOTS:
    void reloadFile();

    /*
     * Read the current state from a file. This can happen
     * from direct callers, or as part of a scheduled task.
     * @p taskContext specifies whether the method is being
     * called from within a task or not.
     */
    virtual void readFile( bool taskContext = false ) = 0;
    /*
     * Writes the current state out to a file. This can happen
     * from direct callers, or as part of a scheduled task.
     * @p taskContext specifies whether the method is being
     * called from within a task or not.
     */
    virtual void writeFile( bool taskContext = false ) = 0;

  protected:
    /**
     * Returns a pointer to the KConfig object which is used to store runtime
     * information of the resource.
     */
    KSharedConfig::Ptr runtimeConfig() const;


    /**
     * Handles everything needed when the hash of a file has changed between the
     * last write and the first read. This stores the new hash in a config file
     * and notifies implementing resources to handle a hash change if the
     * previous known hash was not empty. Finally this method clears the cache
     * and calls synchronize.
     * Returns true on succes, false otherwise.
     */
    bool readLocalFile( const QString &fileName );

    /**
     * Reimplement to read your data from the given file.
     * The file is always local, loading from the network is done
     * automatically if needed.
     */
    virtual bool readFromFile( const QString &fileName ) = 0;

    /**
     * Reimplement to write your data to the given file.
     * The file is always local, storing back to the network url is done
     * automatically when needed.
     */
    virtual bool writeToFile( const QString &fileName ) = 0;

    /**
     * It is not always needed to parse the file when a resources is started.
     * (e.g. When the hash of the file is the same as the last time the resource
     * has written changes to the file). In this case setActualFileName is
     * called so that the implementing resource does know which file to read
     * when it actually needs to read the file.
     *
     * The default implementation will just call readFromFile( fileName ), so
     * implementing resources will have to explictly reimplement this method to
     * actually get any profit of this.
     *
     * @p fileName This will always be a path to a local file.
     */
    virtual void setLocalFileName( const QString &fileName );

    /**
     * Generates the full path for the cache file in the case that a remote file
     * is used.
     */
    QString cacheFile() const;

    /**
     * Calculates an MD5 hash for given file. If the file does not exists
     * or the path is empty, this will return an empty QByteArray.
     */
    QByteArray calculateHash( const QString &fileName ) const;

    /**
     * This method is called when the hash of the file has changed between the
     * last writeFile() and a readFile() call. This means that the file was
     * changed by another program.
     *
     * Note: This method is <em>not</em> called when the last known hash is
     *       empty. In that case it is assumed that the file is loaded for the
     *       first time.
     */
    virtual void handleHashChange();

    /**
     * Returns the hash that was stored to a cache file.
     */
    QByteArray loadHash() const;

    /**
     * Stores the given hash into a cache file.
     */
    void saveHash( const QByteArray &hash ) const;

    /**
     * Returns whether the resource can be written to.
     */
    virtual bool readOnly() const = 0;

  protected:
    KUrl mCurrentUrl;
    QStringList mSupportedMimetypes;
    QString mCollectionIcon;
    KIO::FileCopyJob *mDownloadJob;
    KIO::FileCopyJob *mUploadJob;
    QByteArray mCurrentHash;

  protected Q_SLOTS:
    void scheduleWrite(); /// Called when changes are added to the ChangeRecorder.

  private Q_SLOTS:
    void handleProgress( KJob *, unsigned long );
    void fileChanged( const QString &fileName );
    void slotDownloadJobResult( KJob * );
    void slotUploadJobResult( KJob * );
};

}

#endif
