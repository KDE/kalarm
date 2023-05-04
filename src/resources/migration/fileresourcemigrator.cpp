/*
 *  fileresourcemigrator.cpp  -  migrates or creates KAlarm file system resources
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileresourcemigrator.h"

#include "dirresourceimportdialog.h"
#include "preferences.h"
#include "resources/calendarfunctions.h"
#include "resources/fileresourcecalendarupdater.h"
#include "resources/fileresourceconfigmanager.h"
#include "resources/resources.h"
#include "lib/autoqpointer.h"
#include "lib/desktop.h"
#include "akonadiplugin/akonadiplugin.h"
#include "kalarm_debug.h"

#include <KLocalizedString>

#include <QStandardPaths>
#include <QDirIterator>
#include <QFileInfo>

using namespace KAlarmCal;

//clazy:excludeall=non-pod-global-static

namespace
{
bool readDirectoryResource(const QString& dirPath, CalEvent::Types alarmTypes, QHash<CalEvent::Type, QList<KAEvent>>& events);
}

FileResourceMigrator* FileResourceMigrator::mInstance = nullptr;
bool                  FileResourceMigrator::mCompleted = false;

/******************************************************************************
* Constructor.
*/
FileResourceMigrator::FileResourceMigrator(QObject* parent)
    : QObject(parent)
{
}

FileResourceMigrator::~FileResourceMigrator()
{
    qCDebug(KALARM_LOG) << "~FileResourceMigrator";
    mInstance = nullptr;
}

/******************************************************************************
* Create and return the unique FileResourceMigrator instance.
*/
FileResourceMigrator* FileResourceMigrator::instance()
{
    if (!mInstance  &&  !mCompleted)
    {
        // Check whether migration or default resource creation is actually needed.
        CalEvent::Types needed = CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE;
        const QList<Resource> resources = Resources::allResources<FileResource>();
        for (const Resource& resource : resources)
        {
            needed &= ~resource.alarmTypes();
            if (!needed)
            {
                mCompleted = true;
                return mInstance;
            }
        }
        // Migration or default resource creation is required.
        mInstance = new FileResourceMigrator;
    }
    return mInstance;
}

/******************************************************************************
* Migrate old Akonadi calendars, and create default file system resources.
*/
void FileResourceMigrator::start()
{
    if (mCompleted)
    {
        deleteLater();
        return;
    }

    qCDebug(KALARM_LOG) << "FileResourceMigrator::start";

    // First, check whether any file system resources already exist, and if so,
    // find their alarm types.
    const QList<Resource> resources = Resources::allResources<FileResource>();
    for (const Resource& resource : resources)
        mExistingAlarmTypes |= resource.alarmTypes();

    if (mExistingAlarmTypes != CalEvent::EMPTY)
    {
        // Some file system resources already exist, so no migration is
        // required. Create any missing default file system resources.
        akonadiMigrationComplete();
    }
    else
    {
        // There are no file system resources, so migrate any Akonadi resources.
        mAkonadiPlugin = Preferences::akonadiPlugin();
        if (!mAkonadiPlugin)
            akonadiMigrationComplete();   // Akonadi plugin is not available
        else
        {
            connect(mAkonadiPlugin, &AkonadiPlugin::akonadiMigrationComplete, this, &FileResourceMigrator::akonadiMigrationComplete);
            connect(mAkonadiPlugin, &AkonadiPlugin::migrateFileResource, this, &FileResourceMigrator::migrateFileResource);
            connect(mAkonadiPlugin, &AkonadiPlugin::migrateDirResource, this, &FileResourceMigrator::migrateDirResource);
            mAkonadiPlugin->initiateAkonadiResourceMigration();
            // Migration of Akonadi collections has now been initiated. On
            // completion, any missing default resources will be created.
        }
    }
}

/******************************************************************************
* Migrate one Akonadi single file collection to a file system resource.
*/
void FileResourceMigrator::migrateFileResource(const QString& resourceName,
                      const QUrl& location, CalEvent::Types alarmTypes,
                      const QString& displayName, const QColor& backgroundColour,
                      CalEvent::Types enabledTypes, CalEvent::Types standardTypes,
                      bool readOnly)
{
    FileResourceSettings::Ptr settings(new FileResourceSettings(
                  FileResourceSettings::File, location, alarmTypes, displayName,
                  backgroundColour, enabledTypes, standardTypes, readOnly));
    Resource resource = FileResourceConfigManager::addResource(settings);

    // Update the calendar to the current KAlarm format if necessary,
    // and if the user agrees.
    auto updater = new FileResourceCalendarUpdater(resource, true, this);
    connect(updater, &QObject::destroyed, this, &FileResourceMigrator::checkIfComplete);
    updater->update();   // note that 'updater' will auto-delete when finished

    mExistingAlarmTypes |= alarmTypes;

    if (mAkonadiPlugin)
    {
        // Delete the Akonadi resource, to prevent it using CPU, on the
        // assumption that Akonadi access won't be needed by any other
        // application. Excess CPU usage is one of the major bugs which
        // prompted replacing Akonadi resources with file resources.
        mAkonadiPlugin->deleteAkonadiResource(resourceName);
    }
}

/******************************************************************************
* Migrate one Akonadi directory collection to file system resources.
*/
void FileResourceMigrator::migrateDirResource(const QString& resourceName,
                      const QString& path, CalEvent::Types alarmTypes,
                      const QString& displayName, const QColor& backgroundColour,
                      CalEvent::Types enabledTypes, CalEvent::Types standardTypes,
                      bool readOnly)
{
    // Use AutoQPointer to guard against crash on application exit while
    // the dialogue is still open. It prevents double deletion (both on
    // deletion of parent, and on return from this function).
    AutoQPointer<DirResourceImportDialog> dlg = new DirResourceImportDialog(displayName, path, alarmTypes, Desktop::mainWindow());
    if (dlg->exec() == QDialog::Accepted)
    {
        if (dlg)
        {
            bool converted = false;
            QHash<CalEvent::Type, QList<KAEvent>> events;
            readDirectoryResource(path, alarmTypes, events);

            for (auto it = events.constBegin();  it != events.constEnd();  ++it)
            {
                const CalEvent::Type alarmType = it.key();
                Resource resource;
                const ResourceId id = dlg->resourceId(alarmType);
                if (id >= 0)
                {
                    // The directory resource's alarms are to be
                    // imported into an existing resource.
                    resource = Resources::resource(id);
                }
                else
                {
                    const QUrl destUrl = dlg->url(alarmType);
                    if (!destUrl.isValid())
                        continue;   // this alarm type is not to be imported

                    // The directory resource's alarms are to be
                    // imported into a new resource.
                    qCDebug(KALARM_LOG) << "FileResourceMigrator: Creating resource" << dlg->displayName(alarmType) << ", type:" << alarmType << ", standard:" << (bool)(standardTypes & alarmType);
                    FileResourceSettings::Ptr settings(new FileResourceSettings(
                                  FileResourceSettings::File, destUrl, alarmType,
                                  dlg->displayName(alarmType), backgroundColour,
                                  enabledTypes, (standardTypes & alarmType), readOnly));
                    resource = FileResourceConfigManager::addResource(settings);
                }

                // Add directory events of the appropriate type to this resource.
                for (const KAEvent& event : std::as_const(it.value()))
                    resource.addEvent(event);

                mExistingAlarmTypes |= alarmType;
                converted = true;
            }

            if (converted  &&  mAkonadiPlugin)
            {
                // Delete the Akonadi resource, to prevent it using CPU, on the
                // assumption that Akonadi access won't be needed by any other
                // application. Excess CPU usage is one of the major bugs which
                // prompted replacing Akonadi resources with file resources.
                mAkonadiPlugin->deleteAkonadiResource(resourceName);
            }
        }
    }
}

/******************************************************************************
* Called when Akonadi migration is complete or is known not to be possible.
*/
void FileResourceMigrator::akonadiMigrationComplete()
{
    // Create any necessary additional default file system resources.
    createDefaultResources();

    // Allow any calendar updater instances to complete and auto-delete.
    FileResourceCalendarUpdater::waitForCompletion();
}

/******************************************************************************
* Called when a CalendarUpdater has been destroyed.
* If there are none left, and we have finished, delete this object.
*/
void FileResourceMigrator::checkIfComplete()
{
    if (mCompleted  &&  !FileResourceCalendarUpdater::pending())
        deleteLater();
}

/******************************************************************************
* Create default file system resources for any alarm types not covered by
* existing resources. Normally, this occurs on the first run of KAlarm, but if
* resources have been deleted, it could occur on later runs.
* If the default calendar files already exist, they will be used; otherwise
* they will be created.
*/
void FileResourceMigrator::createDefaultResources()
{
    qCDebug(KALARM_LOG) << "FileResourceMigrator::createDefaultResources";
    if (!(mExistingAlarmTypes & CalEvent::ACTIVE))
        createCalendar(CalEvent::ACTIVE, QStringLiteral("calendar.ics"), i18nc("@info/plain Name of a calendar", "Active Alarms"));
    if (!(mExistingAlarmTypes & CalEvent::ARCHIVED))
        createCalendar(CalEvent::ARCHIVED, QStringLiteral("expired.ics"), i18nc("@info/plain Name of a calendar", "Archived Alarms"));
    if (!(mExistingAlarmTypes & CalEvent::TEMPLATE))
        createCalendar(CalEvent::TEMPLATE, QStringLiteral("template.ics"), i18nc("@info/plain Name of a calendar", "Alarm Templates"));

    mCompleted = true;
    checkIfComplete();    // delete this instance if everything is finished
}

/******************************************************************************
* Create a new default local file resource.
* This is created as enabled, read-write, and standard for its alarm type.
*/
void FileResourceMigrator::createCalendar(CalEvent::Type alarmType, const QString& file, const QString& name)
{
    const QUrl url = QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1Char('/') + file);
    qCDebug(KALARM_LOG) << "FileResourceMigrator: New:" << name << ", type=" << alarmType << ", path=" << url.toString();
    FileResourceSettings::Ptr settings(new FileResourceSettings(
                  FileResourceSettings::File, url, alarmType, name, QColor(), alarmType,
                  alarmType, false));
    Resource resource = FileResourceConfigManager::addResource(settings);
    if (resource.failed())
    {
        const QString errmsg = xi18nc("@info", "<para>Failed to create default calendar <resource>%1</resource></para>"
                                               "<para>Location: <filename>%2</filename></para>",
                                      name, resource.displayLocation());
        Resources::notifyResourceMessage(resource.id(), ResourceType::MessageType::Error, errmsg, QString());
        return;
    }

    // Update the calendar to the current KAlarm format if necessary,
    // and if the user agrees.
    auto updater = new FileResourceCalendarUpdater(resource, true, this);
    connect(updater, &QObject::destroyed, this, &FileResourceMigrator::checkIfComplete);
    updater->update();   // note that 'updater' will auto-delete when finished
}

namespace
{

/******************************************************************************
* Load and parse events from each file in a calendar directory.
*/
bool readDirectoryResource(const QString& dirPath, CalEvent::Types alarmTypes,
                           QHash<CalEvent::Type, QList<KAEvent>>& events)
{
    if (dirPath.isEmpty())
        return false;
    qCDebug(KALARM_LOG) << "FileResourceMigrator::readDirectoryResource:" << dirPath;
    const QDir dir(dirPath);
    if (!dir.exists())
        return false;

    // Read and parse each file in turn
    QDirIterator it(dir);
    while (it.hasNext())
    {
        it.next();
        const QString file = it.fileName();
        if (!file.isEmpty()
        &&  !file.startsWith(QLatin1Char('.')) && !file.endsWith(QLatin1Char('~'))
        &&  file != QLatin1String("WARNING_README.txt"))
        {
            const QString path = dirPath + QLatin1Char('/') + file;
            if (QFileInfo::exists(path)  // a temporary file may no longer exist
            &&  QFileInfo(path).isFile())
            {
                KAlarm::importCalendarFile(QUrl::fromLocalFile(path), alarmTypes, false, Desktop::mainWindow(), events);
            }
        }
    }
    return true;
}

}

// vim: et sw=4:
