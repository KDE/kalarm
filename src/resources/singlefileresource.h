/*
 *  singlefileresource.h  -  calendar resource held in a single file
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "fileresource.h"
#include "fileresourceconfigmanager.h"

#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/FileStorage>

#include <QUrl>

namespace KIO {
class FileCopyJob;
}
class KJob;
class QTimer;

using namespace KAlarmCal;

/**
 * Calendar resource stored in a single file, either local or remote.
 */
class SingleFileResource : public FileResource
{
    Q_OBJECT
public:
    /** Construct a new SingleFileResource.
     *  Initialises the resource and initiates loading its events.
     */
    static Resource create(FileResourceSettings* settings);

protected:
    /** Constructor.
     *  Initialises the resource and initiates loading its events.
     */
    explicit SingleFileResource(FileResourceSettings* settings);

public:
    ~SingleFileResource() override;

    /** Return the type of storage used by the resource. */
    StorageType storageType() const override   { return File; }

    /** Return whether the resource is configured as read-only or is
     *  read-only on disc.
     */
    bool readOnly() const override;

    /** Return whether the resource is both enabled and fully writable for a
     *  given alarm type, i.e. not read-only, and compatible with the current
     *  KAlarm calendar format.
     *
     *  @param type  alarm type to check for, or EMPTY to check for any type.
     *  @return 1 = fully enabled and writable,
     *          0 = enabled and writable except that backend calendar is in an
     *              old KAlarm format,
     *         -1 = read-only, disabled or incompatible format.
     */
    int writableStatus(CalEvent::Type type = CalEvent::EMPTY) const override;

    /** Reload the resource. Any cached data is first discarded.
     *  @param discardMods  Discard any modifications since the last save.
     *  @return true if loading succeeded or has been initiated.
     *          false if it failed.
     */
    bool reload(bool discardMods = false) override;

    /** Return whether the resource is waiting for a save() to complete. */
    bool isSaving() const override;

    /** Close the resource. This saves any unsaved data.
     *  Saving is not performed if the resource is disabled.
     */
    void close() override;

    /** Called when the resource's FileResourceSettings object is about to be destroyed. */
    void removeSettings() override;

protected:
    /** Update the resource to the current KAlarm storage format. */
    bool updateStorageFmt() override;

    /** This method is called by load() to allow derived classes to implement
     *  loading the resource from its backend, and fetch all events into
     *  @p newEvents.
     *  If the resource is cached, it should be loaded from the cache file (which
     *  if @p readThroughCache is true, should first be downloaded from the
     *  resource file).
     *  If the resource initiates but does not complete loading, loaded() must be
     *  called when loading completes or fails.
     *  @see loaded()
     *
     *  @param newEvents         To be updated to contain the events fetched.
     *  @param readThroughCache  If the resource is cached, refresh the cache first.
     *  @return 1 = loading succeeded
     *          0 = loading has been initiated, but has not yet completed
     *         -1 = loading failed.
     */
    int doLoad(QHash<QString, KAEvent>& newEvents, bool readThroughCache, QString& errorMessage) override;

    /** This method is called by save() to allow derived classes to implement
     *  saving the resource to its backend.
     *  If the resource is cached, it should be saved to the cache file (which
     *  if @p writeThroughCache is true, should then be uploaded from the
     *  resource file).
     *  If the resource initiates but does not complete saving, saved() must be
     *  called when saving completes or fails.
     *  @see saved()
     *
     *  @param writeThroughCache  If the resource is cached, update the file
     *                            after writing to the cache.
     *  @param force              Save even if no changes have been made since last
     *                            loaded or saved.
     *  @return 1 = saving succeeded
     *          0 = saving has been initiated, but has not yet completed
     *         -1 = saving failed.
     */
    int doSave(bool writeThroughCache, bool force, QString& errorMessage) override;

    /** Schedule the resource for saving.
     *  This delays calling save(), so as to enable multiple event changes to
     *  be saved together.
     *
     *  @return true if saving succeeded or has been initiated/scheduled.
     *          false if it failed.
     */
    bool scheduleSave(bool writeThroughCache = true) override;

    /**
     * Handles everything needed when the hash of a file has changed between the
     * last write and the first read. This stores the new hash in a config file
     * and notifies implementing resources to handle a hash change if the
     * previous known hash was not empty.
     * Returns true on success, false otherwise.
     */
    bool readLocalFile(const QString& fileName, QString& errorMessage);

    /** Read the data from the given local file. */
    bool readFromFile(const QString& fileName, QString& errorMessage);

    /**
     * Reimplement to write your data to the given file.
     * The file is always local, storing back to the network url is done
     * automatically when needed.
     */
    bool writeToFile(const QString& fileName, QString& errorMessage);

    /** This method is called by addEvent() to allow derived classes to add
     *  an event to the resource.
     */
    bool doAddEvent(const KAEvent&) override;

    /** This method is called by updateEvent() to allow derived classes to update
     *  an event in the resource. The event's UID must be unchanged.
     */
    bool doUpdateEvent(const KAEvent&) override;

    /** This method is called by deleteEvent() to allow derived classes to delete
     *  an event from the resource.
     */
    bool doDeleteEvent(const KAEvent&) override;

    /** Called when settings have changed, to allow derived classes to process
     *  the changes.
     *  @note  Resources::notifySettingsChanged() is called after this, to
     *         notify clients.
     */
    void handleSettingsChange(Changes&) override;

    /**
     * Generates the full path for the cache file in the case that a remote file
     * is used.
     */
    QString cacheFilePath() const;

    /**
     * Calculates an MD5 hash for given file. If the file does not exists
     * or the path is empty, this will return an empty QByteArray.
     */
    QByteArray calculateHash(const QString& fileName) const;

    /**
     * Stores the given hash into the config file.
     */
    void saveHash(const QByteArray& hash) const;

private Q_SLOTS:
    void slotSave()   { save(nullptr, mSavePendingCache); }
//    void handleProgress(KJob*, unsigned long);
    void localFileChanged(const QString& fileName);
    void slotDownloadJobResult(KJob*);
    void slotUploadJobResult(KJob*);
    void updateFormat()    { updateStorageFmt(); }
    bool addLoadedEvent(const KCalendarCore::Event::Ptr&);

private:
    void setLoadFailure(bool exists);

    QUrl               mSaveUrl;   // current local file for save() to use (may be temporary)
    KIO::FileCopyJob*  mDownloadJob {nullptr};
    KIO::FileCopyJob*  mUploadJob {nullptr};
    QByteArray         mCurrentHash;
    KCalendarCore::MemoryCalendar::Ptr mCalendar;
    KCalendarCore::FileStorage::Ptr    mFileStorage;
    QHash<QString, KAEvent> mLoadedEvents;    // events loaded from calendar last time file was read
    QTimer*            mSaveTimer {nullptr};  // timer to enable multiple saves to be grouped
    bool               mSavePendingCache;     // writeThroughCache parameter for delayed save()
    bool               mFileReadOnly {false}; // the calendar file is a read-only local file
};


// vim: et sw=4:
