/*
 *  fileresource.cpp  -  base class for calendar resource accessed via file system
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2006-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileresource.h"

#include "fileresourcesettings.h"
#include "fileresourceconfigmanager.h"
#include "singlefileresourceconfigdialog.h"
#include "resources.h"
#include "lib/autoqpointer.h"
#include "kalarmcalendar/version.h"
#include "kalarm_debug.h"

#include <KLocalizedString>


FileResource::FileResource(FileResourceSettings::Ptr settings)
    : ResourceType(settings->id())
    , mSettings(settings)
{
    if (!mSettings  ||  !mSettings->isValid()
    ||  id() < 0  ||  mSettings->id() != id())
        mStatus = Status::NotConfigured;
}

FileResource::~FileResource()
{
}

bool FileResource::isValid() const
{
#ifdef DEBUG_DETAIL
    if (!mSettings  ||  !mSettings->isValid())
        qDebug() << "FileResource::isValid:" << displayId() << "NO (mSettings)";
    if (mStatus == Status::NotConfigured  ||  mStatus == Status::Unusable)
        qDebug() << "FileResource::isValid:" << displayId() << "NO (Not configured)";
    if (mStatus == Status::Closed)
        qDebug() << "FileResource::isValid:" << displayId() << "NO (Closed)";
    if (id() < 0)
        qDebug() << "FileResource::isValid:" << displayId() << "NO (ID < 0)";
    if (mSettings  &&  mSettings->id() != id())
        qDebug() << "FileResource::isValid:" << displayId() << "NO (ID:" << id() << mSettings->id() << ")";
#endif
    // The settings ID must not have changed since construction.
    return mSettings  &&  mSettings->isValid()
       &&  mStatus < Status::Unusable
       &&  id() >= 0  &&  mSettings->id() == id();
}

ResourceId FileResource::displayId() const
{
    return id() & ~IdFlag;
}

QString FileResource::storageTypeString(bool description) const
{
    if (!mSettings)
        return {};
    bool file;
    switch (mSettings->storageType())
    {
        case FileResourceSettings::File:
            file = true;
            break;
        case FileResourceSettings::Directory:
            file = false;
            break;
        default:
            return {};
    }
    return storageTypeStr(description, file, mSettings->url().isLocalFile());
}

QUrl FileResource::location() const
{
    return mSettings ? mSettings->url() : QUrl();
}

QString FileResource::displayLocation() const
{
    return mSettings ? mSettings->displayLocation() : QString();
}

QString FileResource::displayName() const
{
    return mSettings ? mSettings->displayName() : QString();
}

QString FileResource::configName() const
{
    return mSettings ? mSettings->configName() : QString();
}

CalEvent::Types FileResource::alarmTypes() const
{
    return mSettings ? mSettings->alarmTypes() : CalEvent::EMPTY;
}

CalEvent::Types FileResource::enabledTypes() const
{
    return mSettings && mSettings->isValid() ? mSettings->enabledTypes() : CalEvent::EMPTY;
}

void FileResource::setEnabled(CalEvent::Type type, bool enabled)
{
    if (mSettings)
    {
        const CalEvent::Types oldEnabled = mSettings->enabledTypes();
        const Changes changes = mSettings->setEnabled(type, enabled);
        handleEnabledChange(changes, oldEnabled);
    }
}

void FileResource::setEnabled(CalEvent::Types types)
{
    if (mSettings)
    {
        const CalEvent::Types oldEnabled = mSettings->enabledTypes();
        const Changes changes = mSettings->setEnabled(types);
        handleEnabledChange(changes, oldEnabled);
    }
}

bool FileResource::readOnly() const
{
    return mSettings ? mSettings->readOnly() : true;
}

void FileResource::setReadOnly(bool ronly)
{
    if (mSettings)
    {
        const CalEvent::Types oldEnabled = mSettings->enabledTypes();
        Changes changes = mSettings->setReadOnly(ronly);
        if (changes)
        {
            handleSettingsChange(changes);
            Resources::notifySettingsChanged(this, changes, oldEnabled);
        }
    }
}

int FileResource::writableStatus(CalEvent::Type type) const
{
    if (!mSettings  ||  !mSettings->isValid()  ||  mSettings->readOnly())
        return -1;
    if ((type == CalEvent::EMPTY  && !mSettings->enabledTypes())
    ||  (type != CalEvent::EMPTY  && !mSettings->isEnabled(type)))
        return -1;
    switch (mCompatibility)
    {
        case KACalendar::Current:
            return 1;
        case KACalendar::Converted:
        case KACalendar::Convertible:
            return 0;
        default:
            return -1;
    }
}

bool FileResource::isWritable(const KAEvent& event) const
{
    return isWritable(event.category());
}

bool FileResource::keepFormat() const
{
    return mSettings ? mSettings->keepFormat() : true;
}

void FileResource::setKeepFormat(bool keep)
{
    if (mSettings)
    {
        const CalEvent::Types oldEnabled = mSettings->enabledTypes();
        Changes changes = mSettings->setKeepFormat(keep);
        if (changes)
        {
            handleSettingsChange(changes);
            Resources::notifySettingsChanged(this, changes, oldEnabled);
        }
    }
}

QColor FileResource::backgroundColour() const
{
    return mSettings ? mSettings->backgroundColour() : QColor();
}

void FileResource::setBackgroundColour(const QColor& colour)
{
    if (mSettings)
    {
        const CalEvent::Types oldEnabled = mSettings->enabledTypes();
        Changes changes = mSettings->setBackgroundColour(colour);
        if (changes)
        {
            handleSettingsChange(changes);
            Resources::notifySettingsChanged(this, changes, oldEnabled);
        }
    }
}

bool FileResource::configIsStandard(CalEvent::Type type) const
{
    return mSettings ? mSettings->isStandard(type) : false;
}

CalEvent::Types FileResource::configStandardTypes() const
{
    return mSettings ? mSettings->standardTypes() : CalEvent::EMPTY;
}

void FileResource::configSetStandard(CalEvent::Type type, bool standard)
{
    if (mSettings)
    {
        const CalEvent::Types oldEnabled = mSettings->enabledTypes();
        Changes changes = mSettings->setStandard(type, standard);
        if (changes)
        {
            handleSettingsChange(changes);
            Resources::notifySettingsChanged(this, changes, oldEnabled);
        }
    }
}

void FileResource::configSetStandard(CalEvent::Types types)
{
    if (mSettings)
    {
        const CalEvent::Types oldEnabled = mSettings->enabledTypes();
        Changes changes = mSettings->setStandard(types);
        if (changes)
        {
            handleSettingsChange(changes);
            Resources::notifySettingsChanged(this, changes, oldEnabled);
        }
    }
}

KACalendar::Compat FileResource::compatibilityVersion(QString& versionString) const
{
    versionString = KAlarmCal::getVersionString(mVersion);
    return mCompatibility;
}

/******************************************************************************
* Edit the resource's configuration.
*/
void FileResource::editResource(QWidget* dialogParent)
{
    switch (storageType())
    {
        case File:
        {
            // Use AutoQPointer to guard against crash on application exit while
            // the dialogue is still open. It prevents double deletion (both on
            // deletion of parent, and on return from this function).
            AutoQPointer<SingleFileResourceConfigDialog> dlg = new SingleFileResourceConfigDialog(false, dialogParent);
            const CalEvent::Types enabled = enabledTypes();
            CalEvent::Types types = alarmTypes();
            if (((types & CalEvent::ACTIVE)  &&  (types & (CalEvent::ARCHIVED | CalEvent::TEMPLATE)))
            ||  ((types & CalEvent::ARCHIVED)  &&  (types & CalEvent::TEMPLATE)))
                types &= enabled;
            const CalEvent::Type alarmType = (types & CalEvent::ACTIVE)   ? CalEvent::ACTIVE
                                           : (types & CalEvent::ARCHIVED) ? CalEvent::ARCHIVED
                                           : (types & CalEvent::TEMPLATE) ? CalEvent::TEMPLATE
                                           : CalEvent::ACTIVE;
            dlg->setAlarmType(alarmType);    // set default alarm type
            dlg->setUrl(location(), true);   // show location but disallow edits
            dlg->setDisplayName(displayName());
            dlg->setReadOnly(readOnly());
            if (dlg->exec() == QDialog::Accepted)
            {
                // Make any changes requested by the user.
                // Note that the location and alarm type cannot be changed.
                qCDebug(KALARM_LOG) << "FileResource::editResource: Edited" << dlg->displayName();
                setReadOnly(dlg->readOnly());
                Changes changes = mSettings ? mSettings->setDisplayName(dlg->displayName()) : NoChange;
                if (changes != NoChange)
                    Resources::notifySettingsChanged(this, changes, enabled);
            }
            break;
        }
        case Directory:
            // Not currently intended to be implemented.
            break;

        default:
            break;
    }
}

/******************************************************************************
* Remove the resource and its settings. The calendar file is not removed.
* The instance will be invalid once it has been removed.
*/
bool FileResource::removeResource()
{
    qCDebug(KALARM_LOG) << "FileResource::removeResource:" << displayId();
    Resources::notifyResourceToBeRemoved(this);
    Resource res = Resources::resource(id());
    bool ok = FileResourceConfigManager::removeResource(res);
    ResourceType::removeResource(id());
    return ok;
}

/******************************************************************************
* Load the resource.
*/
bool FileResource::load(bool readThroughCache)
{
    qCDebug(KALARM_LOG) << "FileResource::load:" << displayName();
    QString errorMessage;
    if (!mSettings  ||  !mSettings->isValid())
    {
        qCWarning(KALARM_LOG) << "FileResource::load: Resource not configured!" << displayName();
        errorMessage = i18nc("@info", "Resource is not configured.");
    }
    else if (mStatus == Status::Closed)
        qCWarning(KALARM_LOG) << "FileResource::load: Resource closed!" << displayName();
    else
    {
        if (!isEnabled(CalEvent::EMPTY))
        {
            // Don't load a disabled resource, but mark it as usable (but not loaded).
            qCDebug(KALARM_LOG) << "FileResource::load: Resource disabled" << displayName();
            setStatus(Status::Ready);
            return false;
        }

        // Do the actual loading.
        QHash<QString, KAEvent> newEvents;
        switch (doLoad(newEvents, readThroughCache, errorMessage))
        {
            case 1:   // success
                loaded(true, newEvents, QString());
                return true;
            case 0:   // loading initiated
                return true;
            default:  // failure
                break;
        }
    }

    if (!errorMessage.isEmpty())
        Resources::notifyResourceMessage(this, MessageType::Error, xi18nc("@info", "Error loading calendar <resource>%1</resource>.", displayName()), errorMessage);
    setNewlyEnabled(false);
    return false;
}

/******************************************************************************
* Called when the resource has loaded, to finish setting it up.
*/
void FileResource::loaded(bool success, QHash<QString, KAEvent>& newEvents, const QString& errorMessage)
{
    if (!mSettings)
        return;
    if (!success)
    {
        // This is only done when a delayed load fails.
        // If the resource previously loaded successfully, leave its events (in
        // mEvents) unchanged.
        if (!errorMessage.isEmpty())
            Resources::notifyResourceMessage(this, MessageType::Error, xi18nc("@info", "Error loading calendar <resource>%1</resource>.", displayName()), errorMessage);
        return;
    }

    if (isEnabled(CalEvent::ACTIVE))
    {
        // Set any command execution error flags for the events.
        // These are stored in the KAlarm config file, not the alarm
        // calendar, since they are specific to the user's local system.
        bool changed = false;
        QHash<QString, KAEvent::CmdErrType> cmdErrors = mSettings->commandErrors();
        for (auto errit = cmdErrors.begin();  errit != cmdErrors.end();  )
        {
            auto evit = newEvents.find(errit.key());
            if (evit != newEvents.end())
            {
                KAEvent& event = evit.value();
                if (event.category() == CalEvent::ACTIVE)
                {
                    event.setCommandError(errit.value());
                    ++errit;
                    continue;
                }
            }
            // The event for this command error doesn't exist, or is not active,
            // so remove this command error from the settings.
            errit = cmdErrors.erase(errit);
            changed = true;
        }

        if (changed)
            mSettings->setCommandErrors(cmdErrors);
    }

    // Update the list of loaded events for the resource.
    setLoadedEvents(newEvents);
}

/******************************************************************************
* Save the resource.
*/
bool FileResource::save(QString* errorMessage, bool writeThroughCache, bool force)
{
    qCDebug(KALARM_LOG) << "FileResource::save:" << displayName();
    if (!checkSave())
        return false;

    QString errMessage;
    switch (doSave(writeThroughCache, force, errMessage))
    {
        case 1:   // success
            saved(true, QString());
            return true;

        case 0:   // saving initiated
            return true;

        default:  // failure
            if (!errMessage.isEmpty())
            {
                const QString msg = xi18nc("@info", "Error saving calendar <resource>%1</resource>.", displayName());
                if (errorMessage)
                {
                    *errorMessage = msg + errMessage;
                    errorMessage->replace(QRegularExpression(QStringLiteral("</html><html>")), QStringLiteral("<br><br>"));
                }
                else
                    Resources::notifyResourceMessage(this, MessageType::Error, msg, errMessage);
            }
            return false;
    }
}

/******************************************************************************
* Check whether the resource can be saved.
*/
bool FileResource::checkSave()
{
    QString errorMessage;
    if (!mSettings  ||  !mSettings->isValid())
    {
        qCWarning(KALARM_LOG) << "FileResource::checkSave: FileResource not configured!" << displayName();
        errorMessage = i18nc("@info", "Resource is not configured.");
    }
    else if (!isValid()  ||  !mSettings->enabledTypes())
        return false;
    else if (readOnly())
    {
        qCWarning(KALARM_LOG) << "FileResource::checkSave: Read-only resource!" << displayName();
        errorMessage = i18nc("@info", "Resource is read-only.");
    }
    else if (mCompatibility != KACalendar::Current)
    {
        qCWarning(KALARM_LOG) << "FileResource::checkSave: Calendar is in wrong format" << displayLocation();
        errorMessage = xi18nc("@info", "Calendar file is in wrong format: <filename>%1</filename>.", displayLocation());
    }
    else
        return true;

    Resources::notifyResourceMessage(this, MessageType::Error, xi18nc("@info", "Error saving calendar <resource>%1</resource>.", displayName()), errorMessage);
    return false;
}

/******************************************************************************
* Called when the resource has saved, to finish the process.
*/
void FileResource::saved(bool success, const QString& errorMessage)
{
    if (!success  &&  !errorMessage.isEmpty())
        Resources::notifyResourceMessage(this, MessageType::Error, xi18nc("@info", "Error saving calendar <resource>%1</resource>.", displayName()), errorMessage);
}

/******************************************************************************
* Add an event to the resource.
*/
bool FileResource::addEvent(const KAEvent& event)
{
    qCDebug(KALARM_LOG) << "FileResource::addEvent:" << event.id();
    if (!isValid())
        qCWarning(KALARM_LOG) << "FileResource::addEvent: Resource invalid!" << displayName();
    else if (!isEnabled(CalEvent::EMPTY))
        qCDebug(KALARM_LOG) << "FileResource::addEvent: Resource disabled!" << displayName();
    else if (!isWritable(event.category()))
        qCWarning(KALARM_LOG) << "FileResource::addEvent: Calendar not writable" << displayName();

    else if (doAddEvent(event))
    {
        setUpdatedEvents({event}, false);

        if (mSettings  &&  mSettings->isEnabled(CalEvent::ACTIVE))
        {
            // Add this event's command error to the settings.
            if (event.category() == CalEvent::ACTIVE
            &&  event.commandError() != KAEvent::CMD_NO_ERROR)
            {
                QHash<QString, KAEvent::CmdErrType> cmdErrors = mSettings->commandErrors();
                cmdErrors[event.id()] = event.commandError();
                mSettings->setCommandErrors(cmdErrors);
            }
        }

        scheduleSave();
        notifyUpdatedEvents();
        return true;
    }
    return false;
}

/******************************************************************************
* Update an event in the resource. Its UID must be unchanged.
*/
bool FileResource::updateEvent(const KAEvent& event, bool saveIfReadOnly)
{
    qCDebug(KALARM_LOG) << "FileResource::updateEvent:" << event.id();
    if (!isValid())
        qCWarning(KALARM_LOG) << "FileResource::updateEvent: Resource invalid!" << displayName();
    else if (!isEnabled(CalEvent::EMPTY))
        qCDebug(KALARM_LOG) << "FileResource::updateEvent: Resource disabled!" << displayName();
    else
    {
        const bool wantSave = saveIfReadOnly || !readOnly();
        if (!isWritable(event.category()))
        {
            if (wantSave)
            {
                qCWarning(KALARM_LOG) << "FileResource::updateEvent: Calendar not writable" << displayName();
                return false;
            }
            qCDebug(KALARM_LOG) << "FileResource::updateEvent: Not saving read-only calendar" << displayName();
        }

        if (doUpdateEvent(event))
        {
            setUpdatedEvents({event}, false);

            // Update command errors held in the settings, if appropriate.
            if (mSettings  &&  mSettings->isEnabled(CalEvent::ACTIVE))
                handleCommandErrorChange(event);

            if (wantSave)
                scheduleSave();
            notifyUpdatedEvents();
            return true;
        }
    }
    return false;
}

/******************************************************************************
* Delete an event from the resource.
*/
bool FileResource::deleteEvent(const KAEvent& event)
{
    qCDebug(KALARM_LOG) << "FileResource::deleteEvent:" << event.id();
    if (!isValid())
        qCWarning(KALARM_LOG) << "FileResource::deleteEvent: Resource invalid!" << displayName();
    else if (!isEnabled(CalEvent::EMPTY))
        qCDebug(KALARM_LOG) << "FileResource::deleteEvent: Resource disabled!" << displayName();
    else if (!isWritable(event.category()))
        qCWarning(KALARM_LOG) << "FileResource::deleteEvent: Calendar not writable" << displayName();

    else if (doDeleteEvent(event))
    {
        setDeletedEvents({event});

        if (mSettings  &&  mSettings->isEnabled(CalEvent::ACTIVE))
        {
            QHash<QString, KAEvent::CmdErrType> cmdErrors = mSettings->commandErrors();
            if (cmdErrors.remove(event.id()))
                mSettings->setCommandErrors(cmdErrors);
        }

        scheduleSave();
        return true;
    }
    return false;
}

/******************************************************************************
* Save a command error change to the settings.
*/
void FileResource::handleCommandErrorChange(const KAEvent& event)
{
    if (!mSettings)
        return;
    // Update command errors held in the settings, if appropriate.
    bool changed = false;
    QHash<QString, KAEvent::CmdErrType> cmdErrors = mSettings->commandErrors();
    if (event.category() != CalEvent::ACTIVE
    ||  event.commandError() == KAEvent::CMD_NO_ERROR)
    {
        if (cmdErrors.remove(event.id()))
            changed = true;
    }
    else if (event.category() == CalEvent::ACTIVE)
    {
        auto it = cmdErrors.find(event.id());
        if (it == cmdErrors.end())
        {
            cmdErrors[event.id()] = event.commandError();
            changed = true;
        }
        else if (event.commandError() != it.value())
        {
            it.value() = event.commandError();
            changed = true;
        }
    }
    if (changed)
    {
        mSettings->setCommandErrors(cmdErrors);
        Resources::notifyEventUpdated(this, event);
    }
}

/******************************************************************************
* Update a resource to the current KAlarm storage format.
*/
bool FileResource::updateStorageFormat(Resource& res)
{
    if (!res.is<FileResource>())
    {
        qCCritical(KALARM_LOG) << "FileResource::updateStorageFormat: Error: Not a FileResource:" << res.displayName();
        return false;
    }
    return resource<FileResource>(res)->updateStorageFmt();
}

/******************************************************************************
* Return the resource's unique identifier for use in cache file names etc.
*/
QString FileResource::identifier() const
{
    if (!mSettings)
        return {};
    return QStringLiteral("FileResource%1").arg(mSettings->id() & ~IdFlag);
}

/******************************************************************************
* Find the compatibility of an existing calendar file.
*/
KACalendar::Compat FileResource::getCompatibility(const KCalendarCore::FileStorage::Ptr& fileStorage, int& version)
{
    QString versionString;
    version = KACalendar::updateVersion(fileStorage, versionString);
    switch (version)
    {
        case KACalendar::IncompatibleFormat:
            return KACalendar::Incompatible;  // calendar is not in KAlarm format, or is in a future format
        case KACalendar::CurrentFormat:
            return KACalendar::Current;       // calendar is in the current format
        default:
            return KACalendar::Convertible;   // calendar is in an out of date format
    }
}

/******************************************************************************
* Called when the resource settings have changed.
*/
void FileResource::handleSettingsChange(Changes& changes)
{
    qCDebug(KALARM_LOG) << "FileResource::handleSettingsChange:" << displayId();
    if (changes & AlarmTypes)
    {
        qCDebug(KALARM_LOG) << "FileResource::handleSettingsChange:" << displayId() << "Update alarm types";
        load();
    }
    if (changes & Enabled)
    {
        qCDebug(KALARM_LOG) << "FileResource::handleSettingsChange:" << displayId() << "Update enabled status";
        if (mSettings  &&  mSettings->enabledTypes())
        {
            // Alarms are now enabled. Reload the calendar file because,
            // although ResourceType retains its record of alarms of disabled
            // types, changes are not processed when disabled calendar files
            // are updated. Also, when the calendar is loaded, disabled alarm
            // types are not fully processed by loaded().
            setNewlyEnabled();   // ensure all events are notified
            load();
            changes |= Loaded;
        }
    }
}

/******************************************************************************
* Called when the resource settings have changed.
*/
void FileResource::handleEnabledChange(Changes changes, CalEvent::Types oldEnabled)
{
    if (changes)
    {
        handleSettingsChange(changes);
        Resources::notifySettingsChanged(this, changes, oldEnabled);
    }
}

/******************************************************************************
* Set the new status of the resource.
* If the resource status is already Unusable, it cannot be set usable again.
*/
void FileResource::setStatus(Status newStatus)
{
    if (newStatus != mStatus
    &&  (mStatus < Status::Unusable  ||  mStatus == Status::Unusable  ||  newStatus > mStatus))
    {
        mStatus = newStatus;
        if (mStatus > Status::Unusable)
            setFailed();
        setError(mStatus == Status::Broken);
    }
}

// vim: et sw=4:
