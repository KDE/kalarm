/*
 *  fileresource.cpp  -  base class for calendar resource accessed via file system
 *  Program:  kalarm
 *  Copyright © 2006-2020 David Jarvie <djarvie@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "fileresource.h"

#include "fileresourcesettings.h"
#include "fileresourceconfigmanager.h"
#include "singlefileresourceconfigdialog.h"
#include "resources.h"
#include "lib/autoqpointer.h"
#include "kalarm_debug.h"

#include <KAlarmCal/Version>

#include <KLocalizedString>

namespace
{
const QString loadErrorMessage = i18n("Error loading calendar '%1'.");
const QString saveErrorMessage = i18n("Error saving calendar '%1'.");
}

FileResource::FileResource(FileResourceSettings* settings)
    : ResourceType(settings->id())
    , mSettings(settings)
{
}

FileResource::~FileResource()
{
}

bool FileResource::isValid() const
{
    // The settings ID must not have changed since construction.
    return mSettings->isValid()
       &&  mStatus < Status::Unusable
       &&  id() >= 0  &&  mSettings->id() == id();
}

ResourceId FileResource::displayId() const
{
    return id() & ~IdFlag;
}

QString FileResource::storageTypeString(bool description) const
{
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
            return QString();
    }
    return storageTypeStr(description, file, mSettings->url().isLocalFile());
}

QUrl FileResource::location() const
{
    return mSettings->url();
}

QString FileResource::displayLocation() const
{
    return mSettings->displayLocation();
}

QString FileResource::displayName() const
{
    return mSettings->displayName();
}

QString FileResource::configName() const
{
    return mSettings->configName();
}

CalEvent::Types FileResource::alarmTypes() const
{
    return mSettings->alarmTypes();
}

CalEvent::Types FileResource::enabledTypes() const
{
    return mSettings->isValid() ? mSettings->enabledTypes() : CalEvent::EMPTY;
}

void FileResource::setEnabled(CalEvent::Type type, bool enabled)
{
    const CalEvent::Types oldEnabled = mSettings->enabledTypes();
    const Changes changes = mSettings->setEnabled(type, enabled);
    if (changes)
    {
        handleSettingsChange(changes);
        Resources::notifySettingsChanged(this, changes, oldEnabled);
    }
}

void FileResource::setEnabled(CalEvent::Types types)
{
    const CalEvent::Types oldEnabled = mSettings->enabledTypes();
    const Changes changes = mSettings->setEnabled(types);
    if (changes)
    {
        handleSettingsChange(changes);
        Resources::notifySettingsChanged(this, changes, oldEnabled);
    }
}

bool FileResource::readOnly() const
{
    return mSettings->readOnly();
}

void FileResource::setReadOnly(bool ronly)
{
    const CalEvent::Types oldEnabled = mSettings->enabledTypes();
    const Changes changes = mSettings->setReadOnly(ronly);
    if (changes)
    {
        handleSettingsChange(changes);
        Resources::notifySettingsChanged(this, changes, oldEnabled);
    }
}

int FileResource::writableStatus(CalEvent::Type type) const
{
    if (!mSettings->isValid()  ||  mSettings->readOnly())
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
    return mSettings->keepFormat();
}

void FileResource::setKeepFormat(bool keep)
{
    const CalEvent::Types oldEnabled = mSettings->enabledTypes();
    const Changes changes = mSettings->setKeepFormat(keep);
    if (changes)
    {
        handleSettingsChange(changes);
        Resources::notifySettingsChanged(this, changes, oldEnabled);
    }
}

QColor FileResource::backgroundColour() const
{
    return mSettings->backgroundColour();
}

void FileResource::setBackgroundColour(const QColor& colour)
{
    const CalEvent::Types oldEnabled = mSettings->enabledTypes();
    const Changes changes = mSettings->setBackgroundColour(colour);
    if (changes)
    {
        handleSettingsChange(changes);
        Resources::notifySettingsChanged(this, changes, oldEnabled);
    }
}

bool FileResource::configIsStandard(CalEvent::Type type) const
{
    return mSettings->isStandard(type);
}

CalEvent::Types FileResource::configStandardTypes() const
{
    return mSettings->standardTypes();
}

void FileResource::configSetStandard(CalEvent::Type type, bool standard)
{
    const CalEvent::Types oldEnabled = mSettings->enabledTypes();
    const Changes changes = mSettings->setStandard(type, standard);
    if (changes)
    {
        handleSettingsChange(changes);
        Resources::notifySettingsChanged(this, changes, oldEnabled);
    }
}

void FileResource::configSetStandard(CalEvent::Types types)
{
    const CalEvent::Types oldEnabled = mSettings->enabledTypes();
    const Changes changes = mSettings->setStandard(types);
    if (changes)
    {
        handleSettingsChange(changes);
        Resources::notifySettingsChanged(this, changes, oldEnabled);
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
                const Changes change = mSettings->setDisplayName(dlg->displayName());
                if (change != NoChange)
                    Resources::notifySettingsChanged(this, change, enabled);
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
    if (!mSettings->isValid())
    {
        qCWarning(KALARM_LOG) << "FileResource::load: Resource not configured!" << mSettings->displayName();
        errorMessage = i18n("Resource is not configured.");
    }
    else if (mStatus == Status::Closed)
        qCWarning(KALARM_LOG) << "FileResource::load: Resource closed!" << mSettings->displayName();
    else
    {
        if (!isEnabled(CalEvent::EMPTY))
        {
            // Don't load a disabled resource, but mark it as usable (but not loaded).
            qCDebug(KALARM_LOG) << "FileResource::load: Resource disabled" << mSettings->displayName();
            mStatus = Status::Ready;
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
        Resources::notifyResourceMessage(this, MessageType::Error, loadErrorMessage.arg(displayName()), errorMessage);
    return false;
}

/******************************************************************************
* Called when the resource has loaded, to finish setting it up.
*/
void FileResource::loaded(bool success, QHash<QString, KAEvent>& newEvents, const QString& errorMessage)
{
    if (!success)
    {
        // This is only done when a delayed load fails.
        // If the resource previously loaded successfully, leave its events (in
        // mEvents) unchanged.
        if (!success  &&  !errorMessage.isEmpty())
            Resources::notifyResourceMessage(this, MessageType::Error, loadErrorMessage.arg(displayName()), errorMessage);
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
bool FileResource::save(bool writeThroughCache, bool force)
{
    qCDebug(KALARM_LOG) << "FileResource::save:" << displayName();
    if (!checkSave())
        return false;

    QString errorMessage;
    switch (doSave(writeThroughCache, force, errorMessage))
    {
        case 1:   // success
            saved(true, QString());
            return true;

        case 0:   // saving initiated
            return true;

        default:  // failure
            if (!errorMessage.isEmpty())
                Resources::notifyResourceMessage(this, MessageType::Error, saveErrorMessage.arg(displayName()), errorMessage);
            return false;
    }
}

/******************************************************************************
* Check whether the resource can be saved.
*/
bool FileResource::checkSave()
{
    QString errorMessage;
    if (!mSettings->isValid())
    {
        qCWarning(KALARM_LOG) << "FileResource::checkSave: FileResource not configured!" << displayName();
        errorMessage = i18n("Resource is not configured.");
    }
    else if (!isValid()  ||  !mSettings->enabledTypes())
        return false;
    else if (readOnly())
    {
        qCWarning(KALARM_LOG) << "FileResource::checkSave: Read-only resource!" << displayName();
        errorMessage = i18n("Resource is read-only.");
    }
    else if (mCompatibility != KACalendar::Current)
    {
        qCWarning(KALARM_LOG) << "FileResource::checkSave: Calendar is in wrong format" << displayLocation();
        errorMessage = i18n("Calendar file is in wrong format: '%1'.", displayLocation());
    }
    else
        return true;

    Resources::notifyResourceMessage(this, MessageType::Error, saveErrorMessage.arg(displayName()), errorMessage);
    return false;
}

/******************************************************************************
* Called when the resource has saved, to finish the process.
*/
void FileResource::saved(bool success, const QString& errorMessage)
{
    if (!success  &&  !errorMessage.isEmpty())
        Resources::notifyResourceMessage(this, MessageType::Error, saveErrorMessage.arg(displayName()), errorMessage);
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

        if (mSettings->isEnabled(CalEvent::ACTIVE))
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
bool FileResource::updateEvent(const KAEvent& event)
{
    qCDebug(KALARM_LOG) << "FileResource::updateEvent:" << event.id();
    if (!isValid())
        qCWarning(KALARM_LOG) << "FileResource::updateEvent: Resource invalid!" << displayName();
    else if (!isEnabled(CalEvent::EMPTY))
        qCDebug(KALARM_LOG) << "FileResource::updateEvent: Resource disabled!" << displayName();
    else if (!isWritable(event.category()))
        qCWarning(KALARM_LOG) << "FileResource::updateEvent: Calendar not writable" << displayName();

    else if (doUpdateEvent(event))
    {
        setUpdatedEvents({event}, false);

        // Update command errors held in the settings, if appropriate.
        if (mSettings->isEnabled(CalEvent::ACTIVE))
            handleCommandErrorChange(event);

        scheduleSave();
        notifyUpdatedEvents();
        return true;
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

        if (mSettings->isEnabled(CalEvent::ACTIVE))
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

// vim: et sw=4:
