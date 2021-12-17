/*
 *  singlefileresource.cpp  -  calendar resource held in a single file
 *  Program:  kalarm
 *  Partly based on ICalResourceBase and SingleFileResource in kdepim-runtime.
 *  SPDX-FileCopyrightText: 2009-2021 David Jarvie <djarvie@kde.org>
 *  SPDX-FileCopyrightText: 2008 Bertjan Broeksema <broeksema@kde.org>
 *  SPDX-FileCopyrightText: 2008 Volker Krause <vkrause@kde.org>
 *  SPDX-FileCopyrightText: 2006 Till Adam <adam@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "singlefileresource.h"

#include "resources.h"
#include "kalarm_debug.h"

#include <KAlarmCal/KACalendar>
#include <KAlarmCal/KAEvent>

#include <KCalendarCore/ICalFormat>

#include <KIO/Job>
#include <KDirWatch>
#include <KLocalizedString>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QTimer>
#include <QTimeZone>
#include <QEventLoopLocker>

using namespace KCalendarCore;
using namespace KAlarmCal;

Q_DECLARE_METATYPE(QEventLoopLocker*)

namespace
{
const int SAVE_TIMER_DELAY = 1000;   // 1 second
}

Resource SingleFileResource::create(FileResourceSettings* settings)
{
    if (!settings  ||  !settings->isValid())
        return Resource::null();    // return invalid Resource
    Resource resource = Resources::resource(settings->id());
    if (!resource.isValid())
    {
        // A resource with this ID doesn't exist, so create a new resource.
        addResource(new SingleFileResource(settings), resource);
    }
    return resource;
}

/******************************************************************************
* Constructor.
*/
SingleFileResource::SingleFileResource(FileResourceSettings* settings)
    : FileResource(settings)
    , mSaveTimer(new QTimer(this))
{
    qCDebug(KALARM_LOG) << "SingleFileResource: Starting" << displayName();
    if (!load())
        setFailed();
    else
    {
        // Monitor local file changes (but not cache files).
        connect(KDirWatch::self(), &KDirWatch::dirty, this, &SingleFileResource::localFileChanged);
        connect(KDirWatch::self(), &KDirWatch::created, this, &SingleFileResource::localFileChanged);

        mSaveTimer->setSingleShot(true);
        mSaveTimer->setInterval(SAVE_TIMER_DELAY);
        connect(mSaveTimer, &QTimer::timeout, this, &SingleFileResource::slotSave);
    }
}

/******************************************************************************
* Destructor.
*/
SingleFileResource::~SingleFileResource()
{
    qCDebug(KALARM_LOG) << "SingleFileResource::~SingleFileResource" << displayName();
    SingleFileResource::close();    // avoid call to virtual method in destructor
}

bool SingleFileResource::readOnly() const
{
    if (mFileReadOnly)
        return true;
    return FileResource::readOnly();
}

/******************************************************************************
* Return whether the resource can be written to.
*/
int SingleFileResource::writableStatus(CalEvent::Type type) const
{
    if (mFileReadOnly)
        return -1;
    return FileResource::writableStatus(type);
}

/******************************************************************************
* Update the backend calendar storage format to the current KAlarm format.
*/
bool SingleFileResource::updateStorageFmt()
{
    if (failed()  ||  readOnly()  ||  enabledTypes() == CalEvent::EMPTY  ||  !mSettings)
        return false;
    if (!mFileStorage)
    {
        qCCritical(KALARM_LOG) << "SingleFileResource::updateStorageFormat:" << displayId() << "Calendar not open";
        return false;
    }

    QString versionString;
    if (KACalendar::updateVersion(mFileStorage, versionString) != KACalendar::CurrentFormat)
    {
        qCWarning(KALARM_LOG) << "SingleFileResource::updateStorageFormat:" << displayId() << "Cannot convert calendar to current storage format";
        return false;
    }

    qCDebug(KALARM_LOG) << "SingleFileResource::updateStorageFormat: Updating storage for" << displayName();
    mCompatibility = KACalendar::Current;
    mVersion = KACalendar::CurrentFormat;
    save(nullptr, true, true);

    mSettings->setUpdateFormat(false);
    mSettings->save();
    return true;
}

/******************************************************************************
* Re-read the file, ignoring saved hash or cache.
*/
bool SingleFileResource::reload(bool discardMods)
{
    mCurrentHash.clear();   // ensure that load() re-reads the file
    mLoadedEvents.clear();

    if (!isEnabled(CalEvent::EMPTY))
        return false;
    qCDebug(KALARM_LOG) << "SingleFileResource::reload()" << displayName();

    // If it has been modified since its last load, write it back to save the changes.
    if (!discardMods  &&  mCalendar  &&  mCalendar->isModified()
    &&  !mSaveUrl.isEmpty()  &&  isWritable(CalEvent::EMPTY))
    {
        if (save())
            return true;  // no need to load again - we would re-read what has just been saved
    }

    return load();
}

bool SingleFileResource::isSaving() const
{
    return mUploadJob;
}

/******************************************************************************
* Read the backend file and fetch its calendar data.
*/
int SingleFileResource::doLoad(QHash<QString, KAEvent>& newEvents, bool readThroughCache, QString& errorMessage)
{
    if (!mSettings)
        return -1;

    newEvents.clear();

    if (mDownloadJob)
    {
        qCWarning(KALARM_LOG) << "SingleFileResource::load:" << displayId() << "Another download is still in progress";
        errorMessage = i18nc("@info", "A previous load is still in progress.");
        return -1;
    }
    if (mUploadJob)
    {
        qCWarning(KALARM_LOG) << "SingleFileResource::load:" << displayId() << "Another file upload is still in progress.";
        errorMessage = i18nc("@info", "A previous save is still in progress.");
        return -1;
    }

    const bool    isSettingsLocalFile = mSettings->url().isLocalFile();
    const QString settingsLocalFileName = isSettingsLocalFile ? mSettings->url().toLocalFile() : QString();

    if (isSettingsLocalFile)
        KDirWatch::self()->removeFile(settingsLocalFileName);

    mSaveUrl = mSettings->url();
    if (mCurrentHash.isEmpty())
    {
        // This is the first call to load(). If the saved hash matches the
        // file's hash, there will be no need to load the file again.
        mCurrentHash = mSettings->hash();
    }

    QString localFileName;
    if (isSettingsLocalFile)
    {
        // It's a local file.
        // Cache file name, because readLocalFile() will clear mSaveUrl on failure.
        localFileName = settingsLocalFileName;
        if (mFileStorage  &&  localFileName != mFileStorage->fileName())
        {
            // The resource's location should never change, so this code should
            // never be reached!
            qCWarning(KALARM_LOG) << "SingleFileResource::load:" << displayId() << "Error? File location changed to" << localFileName;
            setLoadFailure(true);
            mFileStorage.clear();
            mCalendar.clear();
            mLoadedEvents.clear();
        }

        // Check if the file exists, and if not, create it
        QFile f(localFileName);
        if (!f.exists())
        {

            // First try to create the directory the file should be located in
            QDir dir = QFileInfo(f).dir();
            if (!dir.exists())
                dir.mkpath(dir.path());

            if (!f.open(QIODevice::WriteOnly)  ||  !f.resize(0))
            {
                const QString path = mSettings->displayLocation();
                qCWarning(KALARM_LOG) << "SingleFileResource::load:" << displayId() << "Could not create file" << path;
                errorMessage = xi18nc("@info", "Could not create calendar file <filename>%1</filename>.", path);
                mStatus = Status::Broken;
                mSaveUrl.clear();
                setLoadFailure(false);
                return -1;
            }
            // Check whether this user can actually write to the newly created file.
            // This might not tally with the open() permissions, since there are
            // circumstances on Linux where a file created by a user can be owned by root.
            QFile fnew(localFileName);
            if (!fnew.isWritable())
            {
                fnew.remove();
                const QString path = mSettings->displayLocation();
                qCWarning(KALARM_LOG) << "SingleFileResource::load:" << displayId() << "Could not create writable file" << path;
                errorMessage = xi18nc("@info", "Could not create writable calendar file <filename>%1</filename>.", path);
                mStatus = Status::Broken;
                mSaveUrl.clear();
                setLoadFailure(false);
                return -1;
            }
            mFileReadOnly = false;
            mStatus = Status::Ready;
        }
        else
            mFileReadOnly = !QFileInfo(localFileName).isWritable();
    }
    else
    {
        // It's a remote file.
        const QString cachePath = cacheFilePath();
        if (!QFile::exists(cachePath))
            readThroughCache = true;

        if (readThroughCache)
        {
            auto ref = new QEventLoopLocker();
            // NOTE: Test what happens with remotefile -> save, close before save is finished.
            mDownloadJob = KIO::file_copy(mSettings->url(), QUrl::fromLocalFile(cacheFilePath()), -1,
                                          KIO::Overwrite | KIO::DefaultFlags | KIO::HideProgressInfo);
            mDownloadJob->setProperty("QEventLoopLocker", QVariant::fromValue(ref));
            connect(mDownloadJob, &KJob::result,
                    this, &SingleFileResource::slotDownloadJobResult);
//            connect(mDownloadJob, SIGNAL(percent(KJob*,ulong)), SLOT(handleProgress(KJob*,ulong)));
            mStatus = Status::Loading;
            return 0;     // loading initiated
        }

        // Load from cache, without downloading again.
        localFileName = cachePath;
    }

    // It's a local file (or we're reading the cache file).
    if (!readLocalFile(localFileName, errorMessage))
    {
        qCWarning(KALARM_LOG) << "SingleFileResource::load:" << displayId() << "Could not read file" << localFileName;
        // A user error message has been set by readLocalFile().
        mStatus = Status::Broken;
        setLoadFailure(true);
        return -1;
    }

    if (isSettingsLocalFile)
        KDirWatch::self()->addFile(settingsLocalFileName);

    newEvents = mLoadedEvents;
    mStatus = Status::Ready;
    return 1;     // success
}

/******************************************************************************
* Called when loading fails.
* If the resource file doesn't exist or can't be created, the resource is still
* regarded as loaded.
*/
void SingleFileResource::setLoadFailure(bool exists)
{
    mLoadedEvents.clear();
    QHash<QString, KAEvent> events;
    setLoadedEvents(events);
    setLoaded(!exists);
}

/******************************************************************************
* Write the current mCalendar to the backend file, if it has changed since the
* last load() or save().
* The file location to write to is given by mSaveUrl. This is for two reasons:
* 1) to ensure that if the file location has changed (which shouldn't happen!),
*    the contents will not accidentally overwrite the new file.
* 2) to allow the save location to be parameterised.
*/
int SingleFileResource::doSave(bool writeThroughCache, bool force, QString& errorMessage)
{
    mSaveTimer->stop();

    if (!force  &&  mCalendar  &&  !mCalendar->isModified())
        return 1;    // there are no changes to save

    if (mSaveUrl.isEmpty())
    {
        qCWarning(KALARM_LOG) << "SingleFileResource::save:" << displayId() << "No file specified";
        mStatus = Status::Broken;
        return -1;
    }

    bool isLocalFile = mSaveUrl.isLocalFile();
    QString localFileName;
    if (isLocalFile)
    {
        // It's a local file.
        localFileName = mSaveUrl.toLocalFile();
        KDirWatch::self()->removeFile(localFileName);
        writeThroughCache = false;
    }
    else
    {
        // It's a remote file.
        // Check if there is a download or an upload in progress.
        if (mDownloadJob)
        {
            qCWarning(KALARM_LOG) << "SingleFileResource::save:" << displayId() << "A download is still in progress.";
            errorMessage = i18nc("@info", "A previous load is still in progress.");
            return -1;
        }
        if (mUploadJob)
        {
            qCWarning(KALARM_LOG) << "SingleFileResource::save:" << displayId() << "Another file upload is still in progress.";
            errorMessage = i18nc("@info", "A previous save is still in progress.");
            return -1;
        }
        localFileName = cacheFilePath();
    }

    // Write to the local file or the cache file.
    // This sets the 'modified' status of mCalendar to false.
    const bool writeResult = writeToFile(localFileName, errorMessage);
    // Update the hash so we can detect at localFileChanged() if the file actually
    // did change.
    mCurrentHash = calculateHash(localFileName);
    saveHash(mCurrentHash);
    if (isLocalFile)
    {
        if (!KDirWatch::self()->contains(localFileName))
            KDirWatch::self()->addFile(localFileName);
    }
    if (!writeResult)
    {
        qCWarning(KALARM_LOG) << "SingleFileResource::save:" << displayId() << "Error writing to file" << localFileName;
        // A user error message has been set by writeToFile().
        mStatus = Status::Broken;
        return -1;
    }

    if (!isLocalFile  &&  writeThroughCache)
    {
        // Write the cache file to the remote file.
        auto ref = new QEventLoopLocker();
        // Start a job to upload the locally cached file to the remote location.
        mUploadJob = KIO::file_copy(QUrl::fromLocalFile(cacheFilePath()), mSaveUrl, -1, KIO::Overwrite | KIO::DefaultFlags | KIO::HideProgressInfo);
        mUploadJob->setProperty("QEventLoopLocker", QVariant::fromValue(ref));
        connect(mUploadJob, &KJob::result,
                this, &SingleFileResource::slotUploadJobResult);
//        connect(mUploadJob, SIGNAL(percent(KJob*,ulong)), SLOT(handleProgress(KJob*,ulong)));
        mStatus = Status::Saving;
        return 0;
    }

    mStatus = Status::Ready;
    return 1;
}

/******************************************************************************
* Schedule the resource for saving after a delay, so as to enable multiple
* changes to be saved together.
*/
bool SingleFileResource::scheduleSave(bool writeThroughCache)
{
    qCDebug(KALARM_LOG) << "SingleFileResource::scheduleSave:" << displayId() << writeThroughCache;
    if (!checkSave())
        return false;
    if (mSaveTimer->isActive())
    {
        mSavePendingCache = mSavePendingCache || writeThroughCache;
        return false;
    }
    mSaveTimer->start();
    mSavePendingCache = writeThroughCache;
    return true;
}

/******************************************************************************
* Close the resource.
*/
void SingleFileResource::close()
{
    qCDebug(KALARM_LOG) << "SingleFileResource::close" << displayId();
    if (mDownloadJob)
        mDownloadJob->kill();

    save(nullptr, true);   // write through cache
    // If a remote file upload job has been started, the use of
    // QEventLoopLocker should ensure that it continues to completion even if
    // the destructor for this instance is executed.

    if (mSettings  &&  mSettings->url().isLocalFile())
        KDirWatch::self()->removeFile(mSettings->url().toLocalFile());
    mCalendar.clear();
    mFileStorage.clear();
    mStatus = Status::Closed;
}

/******************************************************************************
* Called when the resource's settings object is about to be destroyed.
*/
void SingleFileResource::removeSettings()
{
    if (mSettings  &&  mSettings->url().isLocalFile())
        KDirWatch::self()->removeFile(mSettings->url().toLocalFile());
    FileResource::removeSettings();
}

/******************************************************************************
* Called from addEvent() to add an event to the resource.
*/
bool SingleFileResource::doAddEvent(const KAEvent& event)
{
    if (!mCalendar)
    {
        qCCritical(KALARM_LOG) << "SingleFileResource::addEvent:" << displayId() << "Calendar not open";
        return false;
    }

    KCalendarCore::Event::Ptr kcalEvent(new KCalendarCore::Event);
    event.updateKCalEvent(kcalEvent, KAEvent::UID_SET);
    if (!mCalendar->addEvent(kcalEvent))
    {
        qCCritical(KALARM_LOG) << "SingleFileResource::addEvent:" << displayId() << "Error adding event with id" << event.id();
        return false;
    }
    return addLoadedEvent(kcalEvent);
}

/******************************************************************************
* Add an event to the list of loaded events.
*/
bool SingleFileResource::addLoadedEvent(const KCalendarCore::Event::Ptr& kcalEvent)
{
    if (!mSettings)
        return false;

    KAEvent event(kcalEvent);
    if (!event.isValid())
    {
        qCDebug(KALARM_LOG) << "SingleFileResource::addLoadedEvent:" << displayId() << "Invalid event:" << kcalEvent->uid();
        return false;
    }

    event.setResourceId(mSettings->id());
    event.setCompatibility(mCompatibility);
    mLoadedEvents[event.id()] = event;
    return true;
}

/******************************************************************************
* Called when an event has been updated. Its UID must be unchanged.
* Store the updated event in the calendar.
*/
bool SingleFileResource::doUpdateEvent(const KAEvent& event)
{
    if (!mCalendar)
    {
        qCCritical(KALARM_LOG) << "SingleFileResource::updateEvent:" << displayId() << "Calendar not open";
        return false;
    }

    KCalendarCore::Event::Ptr calEvent = mCalendar->event(event.id());
    if (!calEvent)
    {
        qCWarning(KALARM_LOG) << "SingleFileResource::doUpdateEvent:" << displayId() << "Event not found" << event.id();
        return false;
    }
    if (calEvent->isReadOnly())
    {
        qCWarning(KALARM_LOG) << "SingleFileResource::updateEvent:" << displayId() << "Event is read only:" << event.id();
        return false;
    }

    // Update the event in place.
    mCalendar->deleteEventInstances(calEvent);
    event.updateKCalEvent(calEvent, KAEvent::UID_SET);
    mCalendar->setModified(true);
    return true;
}

/******************************************************************************
* Called from deleteEvent() to delete an event from the resource.
*/
bool SingleFileResource::doDeleteEvent(const KAEvent& event)
{
    if (!mCalendar)
    {
        qCCritical(KALARM_LOG) << "SingleFileResource::doDeleteEvent:" << displayId() << "Calendar not open";
        return false;
    }

    bool found = false;
    const KCalendarCore::Event::Ptr calEvent = mCalendar->event(event.id());
    if (calEvent)
    {
        if (calEvent->isReadOnly())
        {
            qCWarning(KALARM_LOG) << "SingleFileResource::updateEvent:" << displayId() << "Event is read only:" << event.id();
            return false;
        }
        found = mCalendar->deleteEvent(calEvent);
        mCalendar->deleteEventInstances(calEvent);
    }
    mLoadedEvents.remove(event.id());

    if (!found)
    {
        qCWarning(KALARM_LOG) << "SingleFileResource::doDeleteEvent:" << displayId() << "Event not found" << event.id();
        return false;
    }
    return true;
}

/******************************************************************************
* Update the hash of the file, and read it if the hash has changed.
*/
bool SingleFileResource::readLocalFile(const QString& fileName, QString& errorMessage)
{
    if (mFileReadOnly  &&  !QFileInfo(fileName).size())
        return true;
    const QByteArray newHash = calculateHash(fileName);
    if (newHash == mCurrentHash)
        qCDebug(KALARM_LOG) << "SingleFileResource::readLocalFile:" << displayId() << "hash unchanged";
    else
    {
        if (!readFromFile(fileName, errorMessage))
        {
            mCurrentHash.clear();
            mSaveUrl.clear(); // reset so we don't accidentally overwrite the file
            return false;
        }
        if (mCurrentHash.isEmpty())
        {
            // This is the very first time the file has been read, so store
            // the hash as save() might not be called at all (e.g. in case of
            // read only resources).
            saveHash(newHash);
        }
        mCurrentHash = newHash;
    }
    return true;
}

/******************************************************************************
* Read calendar data from the given file.
* Find the calendar file's compatibility with the current KAlarm format.
* The file is always local; loading from the network is done automatically if
* needed.
*/
bool SingleFileResource::readFromFile(const QString& fileName, QString& errorMessage)
{
    qCDebug(KALARM_LOG) << "SingleFileResource::readFromFile:" << fileName;
    mLoadedEvents.clear();
    mCalendar.reset(new KCalendarCore::MemoryCalendar(QTimeZone::utc()));
    mFileStorage.reset(new KCalendarCore::FileStorage(mCalendar, fileName, new KCalendarCore::ICalFormat()));
    const bool result = mFileStorage->load();
    if (!result)
    {
        qCCritical(KALARM_LOG) << "SingleFileResource::readFromFile: Error loading file " << fileName;
        errorMessage = xi18nc("@info", "Could not load file <filename>%1</filename>.", fileName);
        return false;
    }
    if (mCalendar->incidences().isEmpty())
    {
        // It's a new file. Set up the KAlarm custom property.
        KACalendar::setKAlarmVersion(mCalendar);
    }
    mCompatibility = getCompatibility(mFileStorage, mVersion);

    // Retrieve events from the calendar
    const KCalendarCore::Event::List events = mCalendar->events();
    for (const KCalendarCore::Event::Ptr& kcalEvent : std::as_const(events))
    {
        if (kcalEvent->alarms().isEmpty())
            qCDebug(KALARM_LOG) << "SingleFileResource::readFromFile:" << displayId() << "KCalendarCore::Event has no alarms:" << kcalEvent->uid();
        else
            addLoadedEvent(kcalEvent);
    }
    mCalendar->setModified(false);
    return true;
}

/******************************************************************************
* Write calendar data to the given file.
*/
bool SingleFileResource::writeToFile(const QString& fileName, QString& errorMessage)
{
    qCDebug(KALARM_LOG) << "SingleFileResource::writeToFile:" << fileName;
    if (!mCalendar)
    {
        qCCritical(KALARM_LOG) << "SingleFileResource::writeToFile:" << displayId() << "mCalendar is null!";
        errorMessage = i18nc("@info", "Calendar not open.");
        return false;
    }
    KACalendar::setKAlarmVersion(mCalendar);   // write the application ID into the calendar
    KCalendarCore::FileStorage* fileStorage = mFileStorage.data();
    if (!mFileStorage  ||  fileName != mFileStorage->fileName())
        fileStorage = new KCalendarCore::FileStorage(mCalendar, fileName,
                                                     new KCalendarCore::ICalFormat());

    bool success = true;
    if (!fileStorage->save())    // this sets mCalendar->modified to false
    {
        qCCritical(KALARM_LOG) << "SingleFileResource::writeToFile:" << displayId() << "Failed to save calendar to file " << fileName;
        errorMessage = xi18nc("@info", "Could not save file <filename>%1</filename>.", fileName);
        success = false;
    }

    if (fileStorage != mFileStorage.data())
        delete fileStorage;

    return success;
}

/******************************************************************************
* Return the path of the cache file to use. Its directory is created if needed.
*/
QString SingleFileResource::cacheFilePath() const
{
    static QString cacheDir;
    if (cacheDir.isEmpty())
    {
        cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(cacheDir);
    }
    return cacheDir + QLatin1Char('/') + identifier();
}

/******************************************************************************
* Calculate the hash of a file.
*/
QByteArray SingleFileResource::calculateHash(const QString& fileName) const
{
    QFile file(fileName);
    if (file.exists())
    {
        if (file.open(QIODevice::ReadOnly))
        {
            const qint64 blockSize = 512 * 1024; // Read blocks of 512K
            QCryptographicHash hash(QCryptographicHash::Md5);

            while (!file.atEnd())
                hash.addData(file.read(blockSize));

            file.close();

            return hash.result();
        }
    }
    return {};
}

/******************************************************************************
* Save a hash value into the resource's config.
*/
void SingleFileResource::saveHash(const QByteArray& hash) const
{
    if (mSettings)
    {
        mSettings->setHash(hash.toHex());
        mSettings->save();
    }
}

/******************************************************************************
* Called when the local file has changed.
* Not applicable to remote files or their cache files.
*/
void SingleFileResource::localFileChanged(const QString& fileName)
{
    if (!mSettings)
        return;

    if (fileName != mSettings->url().toLocalFile())
        return;   // not the calendar file for this resource

    const QByteArray newHash = calculateHash(fileName);

    // Only need to synchronize when the file was changed by another process.
    if (newHash == mCurrentHash)
        return;

    qCWarning(KALARM_LOG) << "SingleFileResource::localFileChanged:" << displayId() << "Calendar" << mSaveUrl.toDisplayString(QUrl::PreferLocalFile) << "changed by another process: reloading";

    load();
}

/******************************************************************************
* Called when download of the remote to the cache file has completed.
*/
void SingleFileResource::slotDownloadJobResult(KJob* job)
{
    bool success = true;
    QString errorMessage;
    if (job->error() && job->error() != KIO::ERR_DOES_NOT_EXIST)
    {
        if (mStatus != Status::Closed)
            mStatus = Status::Broken;
        setLoadFailure(false);
        const QString path = displayLocation();
        qCWarning(KALARM_LOG) << "SingleFileResource::slotDownloadJobResult:" << displayId() << "Could not load file" << path << job->errorString();
        errorMessage = xi18nc("@info", "Could not load file <filename>%1</filename>. (%2)", path, job->errorString());
        success = false;
    }
    else
    {
        const QString localFileName = QUrl::fromLocalFile(cacheFilePath()).toLocalFile();
        if (!readLocalFile(localFileName, errorMessage))
        {
            qCWarning(KALARM_LOG) << "SingleFileResource::slotDownloadJobResult:" << displayId() << "Could not load local file" << localFileName;
            // A user error message has been set by readLocalFile().
            if (mStatus != Status::Closed)
                mStatus = Status::Broken;
            setLoadFailure(true);
            success = false;
        }
        else if (mStatus != Status::Closed)
            mStatus = Status::Ready;
    }

    mDownloadJob = nullptr;
    auto ref = job->property("QEventLoopLocker").value<QEventLoopLocker*>();
    if (ref)
        delete ref;

    FileResource::loaded(success, mLoadedEvents, errorMessage);
}

/******************************************************************************
* Called when upload of the cache file to the remote has completed.
*/
void SingleFileResource::slotUploadJobResult(KJob* job)
{
    QString errorMessage;
    bool success = true;
    if (job->error())
    {
        if (mStatus != Status::Closed)
            mStatus = Status::Broken;
        const QString path = displayLocation();
        qCWarning(KALARM_LOG) << "SingleFileResource::slotDownloadJobResult:" << displayId() << "Could not save file" << path << job->errorString();
        errorMessage = xi18nc("@info", "Could not save file <filename>%1</filename>. (%2)", path, job->errorString());
        success = false;
    }
    else if (mStatus != Status::Closed)
        mStatus = Status::Ready;

    mUploadJob = nullptr;
    auto ref = job->property("QEventLoopLocker").value<QEventLoopLocker*>();
    if (ref)
        delete ref;

    FileResource::saved(success, errorMessage);
}

/******************************************************************************
* Called when the resource settings have changed.
*/
void SingleFileResource::handleSettingsChange(Changes& changes)
{
    qCDebug(KALARM_LOG) << "SingleFileResource::handleSettingsChange:" << displayId();
    if (changes & UpdateFormat)
    {
        if (mSettings  &&  mSettings->updateFormat())
        {
            // This is a request to update the backend calendar storage format
            // to the current KAlarm format.
            qCDebug(KALARM_LOG) << "SingleFileResource::handleSettingsChange:" << displayId() << "Update storage format";
            switch (mCompatibility)
            {
                case KACalendar::Current:
                    qCWarning(KALARM_LOG) << "SingleFileResource::handleSettingsChange:" << displayId() << "Already current storage format";
                    break;
                case KACalendar::Incompatible:
                default:
                    qCWarning(KALARM_LOG) << "SingleFileResource::handleSettingsChange:" << displayId() << "Incompatible storage format: compat=" << mCompatibility;
                    break;
                case KACalendar::Converted:
                case KACalendar::Convertible:
                {
                    if (!isEnabled(CalEvent::EMPTY))
                    {
                        qCWarning(KALARM_LOG) << "SingleFileResource::handleSettingsChange:" << displayId() << "Cannot update storage format for a disabled resource";
                        break;
                    }
                    if (mSettings->readOnly()  ||  mFileReadOnly)
                    {
                        qCWarning(KALARM_LOG) << "SingleFileResource::handleSettingsChange:" << displayId() << "Cannot update storage format for a read-only resource";
                        break;
                    }
                    // Update the backend storage format to the current KAlarm format
                    QTimer::singleShot(0, this, [this] { updateFormat(); });   //NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
                    return;
                }
            }
            mSettings->setUpdateFormat(false);
            mSettings->save();
        }
    }
    FileResource::handleSettingsChange(changes);
}

// vim: et sw=4:
