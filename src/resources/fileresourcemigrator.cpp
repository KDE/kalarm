/*
 *  fileresourcemigrator.cpp  -  migrates or creates KAlarm non-Akonadi resources
 *  Program:  kalarm
 *  Copyright © 2011-2020 David Jarvie <djarvie@kde.org>
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

#include "fileresourcemigrator.h"

#include "akonadiresource.h"
#include "dirresourceimportdialog.h"
#include "fileresourcecalendarupdater.h"
#include "fileresourceconfigmanager.h"
#include "resources.h"
#include "calendarfunctions.h"
#include "preferences.h"
#include "lib/autoqpointer.h"
#include "lib/desktop.h"
#include "kalarm_debug.h"

#include <KAlarmCal/CollectionAttribute>
#include <KAlarmCal/KACalendar>
#include <KAlarmCal/Version>

#include <AkonadiCore/AgentManager>
#include <AkonadiCore/AttributeFactory>
#include <AkonadiCore/CollectionFetchJob>
#include <AkonadiCore/CollectionFetchScope>
#include <AkonadiCore/CollectionModifyJob>

#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/ICalFormat>

#include <KLocalizedString>
#include <KConfig>
#include <KConfigGroup>
#include <Kdelibs4Migration>

#include <QStandardPaths>
#include <QDirIterator>
#include <QFileInfo>

using namespace KAlarmCal;

namespace
{
const QString KALARM_RESOURCE(QStringLiteral("akonadi_kalarm_resource"));
const QString KALARM_DIR_RESOURCE(QStringLiteral("akonadi_kalarm_dir_resource"));

const Akonadi::Collection::Rights WritableRights = Akonadi::Collection::CanChangeItem | Akonadi::Collection::CanCreateItem | Akonadi::Collection::CanDeleteItem;

bool readDirectoryResource(const QString& dirPath, CalEvent::Types alarmTypes, QHash<CalEvent::Type, QVector<KAEvent>>& events);
}

// Private class to provide private signals.
class FileResourceMigrator::AkonadiMigration : public QObject
{
    Q_OBJECT
public:
    AkonadiMigration()  {}
    void setComplete(bool needed)   { required = needed; Q_EMIT completed(required); }
    bool required {false};
Q_SIGNALS:
    void completed(bool needed);
};

FileResourceMigrator* FileResourceMigrator::mInstance = nullptr;
bool                  FileResourceMigrator::mCompleted = false;

/******************************************************************************
* Constructor.
*/
FileResourceMigrator::FileResourceMigrator(QObject* parent)
    : QObject(parent)
    , mAkonadiMigration(new AkonadiMigration)
{
}

FileResourceMigrator::~FileResourceMigrator()
{
    qCDebug(KALARM_LOG) << "~FileResourceMigrator";
    delete mAkonadiMigration;
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
        const QVector<Resource> resources = Resources::allResources<FileResource>();
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
* Migrate old Akonadi or KResource calendars, and create default file system
* resources.
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
    const QVector<Resource> resources = Resources::allResources<FileResource>();
    for (const Resource& resource : resources)
        mExistingAlarmTypes |= resource.alarmTypes();

    if (mExistingAlarmTypes != CalEvent::EMPTY)
    {
        // Some file system resources already exist, so no migration is
        // required. Create any missing default file system resources.
        mMigrateKResources = false;   // ignore KResources
        akonadiMigrationComplete();
    }
    else
    {
        // There are no file system resources, so migrate any Akonadi resources.
        mAkonadiMigration->required = true;
        connect(mAkonadiMigration, &FileResourceMigrator::AkonadiMigration::completed, this, &FileResourceMigrator::akonadiMigrationComplete);
        connect(Akonadi::ServerManager::self(), &Akonadi::ServerManager::stateChanged, this, &FileResourceMigrator::checkAkonadiResources);
        checkAkonadiResources(Akonadi::ServerManager::state());
        // Migration of Akonadi collections has now been initiated. On
        // completion, either KResource calendars will be migrated, or
        // any missing default resources will be created.
    }
}

/******************************************************************************
* Called when the Akonadi server manager changes state.
* Once it is running, migrate any Akonadi KAlarm resources.
*/
void FileResourceMigrator::checkAkonadiResources(Akonadi::ServerManager::State state)
{
    switch (state)
    {
        case Akonadi::ServerManager::Running:
        {
            if (!AkonadiResource::removeDuplicateResources(&callMigrateAkonadiResources))
                mAkonadiMigration->setComplete(false);   // there are no Akonadi resources to migrate
            break;
        }
        case Akonadi::ServerManager::Stopping:
            // Wait until the server has stopped, so that we can restart it.
            return;

        default:
            if (Akonadi::ServerManager::start())
                return;   // wait for the server to change to Running state

            // Can't start Akonadi, so give up trying to migrate.
            qCWarning(KALARM_LOG) << "FileResourceMigrator::checkAkonadiResources: Failed to start Akonadi server";
            mAkonadiMigration->setComplete(false);
            break;
    }

    disconnect(Akonadi::ServerManager::self(), nullptr, this, nullptr);
}

/******************************************************************************
* Called when removal of duplicate Akonadi KAlarm resources has completed.
*/
void FileResourceMigrator::callMigrateAkonadiResources()
{
    if (mInstance)
        mInstance->migrateAkonadiResources();
}

/******************************************************************************
* Migrate any Akonadi KAlarm resources.
*/
void FileResourceMigrator::migrateAkonadiResources()
{
    qCDebug(KALARM_LOG) << "FileResourceMigrator::migrateAkonadiResources: initiated";
    Akonadi::AttributeFactory::registerAttribute<CollectionAttribute>();
    const Akonadi::AgentInstance::List agents = Akonadi::AgentManager::self()->instances();

    // First, migrate KAlarm calendar file resources.
    // This will allow any KAlarm directory resources to be merged into
    // single file resources, if the user prefers that.
    for (const Akonadi::AgentInstance& agent : agents)
    {
        const QString type = agent.type().identifier();
        if (type == KALARM_RESOURCE)
        {
            // Fetch the resource's collection to determine its alarm types
            Akonadi::CollectionFetchJob* job = new Akonadi::CollectionFetchJob(Akonadi::Collection::root(), Akonadi::CollectionFetchJob::FirstLevel);
            job->fetchScope().setResource(agent.identifier());
            mFetchesPending << job;
            connect(job, &KJob::result, this, &FileResourceMigrator::collectionFetchResult);

            mMigrateKResources = false;   // ignore KResources if Akonadi resources exist
        }
    }
    // Now migrate KAlarm directory resources, which must be merged
    // or converted into single file resources.
    for (const Akonadi::AgentInstance& agent : agents)
    {
        const QString type = agent.type().identifier();
        if (type == KALARM_DIR_RESOURCE)
        {
            // Fetch the resource's collection to determine its alarm types
            Akonadi::CollectionFetchJob* job = new Akonadi::CollectionFetchJob(Akonadi::Collection::root(), Akonadi::CollectionFetchJob::FirstLevel);
            job->fetchScope().setResource(agent.identifier());
            mFetchesPending << job;
            connect(job, &KJob::result, this, &FileResourceMigrator::collectionFetchResult);

            mMigrateKResources = false;   // ignore KResources if Akonadi resources exist
        }
    }
    if (mFetchesPending.isEmpty())
        mAkonadiMigration->setComplete(false);   // there are no Akonadi resources to migrate
}

/******************************************************************************
* Called when an Akonadi collection fetch job has completed.
* Migrate the collection to a file system resource.
*/
void FileResourceMigrator::collectionFetchResult(KJob* j)
{
    Akonadi::CollectionFetchJob* job = static_cast<Akonadi::CollectionFetchJob*>(j);
    const QString id = job->fetchScope().resource();
    if (j->error())
        qCCritical(KALARM_LOG) << "FileResourceMigrator::collectionFetchResult: CollectionFetchJob" << id << "error: " << j->errorString();
    else
    {
        const Akonadi::Collection::List collections = job->collections();
        if (collections.isEmpty())
            qCCritical(KALARM_LOG) << "FileResourceMigrator::collectionFetchResult: No collections found for resource" << id;
        else
        {
            // Migrate this collection.
            const Akonadi::Collection& collection(collections[0]);
            const Akonadi::AgentInstance agent = Akonadi::AgentManager::self()->instance(collection.resource());
            const QString resourceType = agent.type().identifier();
            const bool readOnly = (collection.rights() & WritableRights) != WritableRights;
            const CalEvent::Types alarmTypes = CalEvent::types(collections[0].contentMimeTypes());
            CalEvent::Types enabledTypes  = CalEvent::EMPTY;
            CalEvent::Types standardTypes = CalEvent::EMPTY;
            QColor backgroundColour;
            if (collection.hasAttribute<CollectionAttribute>())
            {
                const CollectionAttribute* attr = collection.attribute<CollectionAttribute>();
                enabledTypes     = attr->enabled();
                standardTypes    = attr->standard();
                backgroundColour = attr->backgroundColor();
            }
            bool converted = false;
            if (resourceType == KALARM_RESOURCE)
            {
                qCDebug(KALARM_LOG) << "FileResourceMigrator: Creating resource" << collection.displayName() << ", alarm types:" << alarmTypes << ", standard types:" << standardTypes;
                FileResourceSettings::Ptr settings(new FileResourceSettings(
                              FileResourceSettings::File,
                              QUrl::fromUserInput(collection.remoteId(), QString(), QUrl::AssumeLocalFile),
                              alarmTypes, collection.displayName(), backgroundColour,
                              enabledTypes, standardTypes, readOnly));
                Resource resource = FileResourceConfigManager::addResource(settings);

                // Update the calendar to the current KAlarm format if necessary,
                // and if the user agrees.
                FileResourceCalendarUpdater* updater = new FileResourceCalendarUpdater(resource, true, this);
                connect(updater, &QObject::destroyed, this, &FileResourceMigrator::checkIfComplete);
                updater->update();   // note that 'updater' will auto-delete when finished

                mExistingAlarmTypes |= alarmTypes;
                converted = true;
            }
            else if (resourceType == KALARM_DIR_RESOURCE)
            {
                // Convert Akonadi directory resource to single file resources.

                // Use AutoQPointer to guard against crash on application exit while
                // the dialogue is still open. It prevents double deletion (both on
                // deletion of parent, and on return from this function).
                AutoQPointer<DirResourceImportDialog> dlg = new DirResourceImportDialog(collection.displayName(), collection.remoteId(), alarmTypes, Desktop::mainWindow());
                if (dlg->exec() == QDialog::Accepted)
                {
                    if (dlg)
                    {
                        QHash<CalEvent::Type, QVector<KAEvent>> events;
                        readDirectoryResource(collection.remoteId(), alarmTypes, events);

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
                            for (const KAEvent& event : qAsConst(it.value()))
                                resource.addEvent(event);

                            mExistingAlarmTypes |= alarmType;
                            converted = true;
                        }
                    }
                }
            }

            if (converted)
            {
                // Delete the Akonadi resource, to prevent it using CPU, on the
                // assumption that Akonadi access won't be needed by any other
                // application. Excess CPU usage is one of the major bugs which
                // prompted replacing Akonadi with file resources.
                Akonadi::AgentManager::self()->removeInstance(agent);
            }
        }
    }
    mFetchesPending.removeAll(job);

    if (mFetchesPending.isEmpty())
    {
        // The alarm types of all collections have been found.
        mAkonadiMigration->setComplete(true);
    }
}

/******************************************************************************
* Called when Akonadi migration is complete or is known not to be possible.
*/
void FileResourceMigrator::akonadiMigrationComplete()
{
    if (!mAkonadiMigration->required)
    {
        // There are no Akonadi resources, so migrate any KResources alarm
        // calendars from pre-Akonadi versions of KAlarm.
        migrateKResources();
    }

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
* Migrate old KResource calendars, and create default file system resources.
*/
void FileResourceMigrator::migrateKResources()
{
    if (!mMigrateKResources)
        return;
    if (mExistingAlarmTypes == CalEvent::EMPTY)
    {
        // There are no file system resources, so migrate any KResources alarm
        // calendars from pre-Akonadi versions of KAlarm.
        const QString kresConfFile = QStringLiteral("kresources/alarms/stdrc");
        QString configFile = QStandardPaths::locate(QStandardPaths::ConfigLocation, kresConfFile);
        if (configFile.isEmpty())
        {
            Kdelibs4Migration kde4;
            if (!kde4.kdeHomeFound())
                return;    // can't find $KDEHOME
            configFile = kde4.locateLocal("config", kresConfFile);
            if (configFile.isEmpty())
                return;    // can't find KResources config file
        }
        qCDebug(KALARM_LOG) << "FileResourceMigrator::migrateKResources";
        const KConfig config(configFile, KConfig::SimpleConfig);

        // Fetch all the KResource identifiers which are actually in use
        const KConfigGroup group = config.group("General");
        const QStringList keys = group.readEntry("ResourceKeys", QStringList())
                               + group.readEntry("PassiveResourceKeys", QStringList());

        // Create a file system resource for each KResource id
        for (const QString& id : keys)
        {
            // Read the resource configuration parameters from the config
            const KConfigGroup configGroup = config.group(QLatin1String("Resource_") + id);
            const QString resourceType = configGroup.readEntry("ResourceType", QString());
            const char* pathKey = nullptr;
            FileResourceSettings::StorageType storageType;
            if (resourceType == QLatin1String("file"))
            {
                storageType = FileResourceSettings::File;
                pathKey = "CalendarURL";
            }
            else if (resourceType == QLatin1String("dir"))
            {
                storageType = FileResourceSettings::Directory;
                pathKey = "CalendarURL";
            }
            else if (resourceType == QLatin1String("remote"))
            {
                storageType = FileResourceSettings::File;
                pathKey = "DownloadUrl";
            }
            else
            {
                qCWarning(KALARM_LOG) << "CalendarCreator: Invalid resource type:" << resourceType;
                continue;   // unknown resource type - can't convert
            }

            const QUrl url = QUrl::fromUserInput(configGroup.readPathEntry(pathKey, QString()));
            CalEvent::Type alarmType = CalEvent::EMPTY;
            switch (configGroup.readEntry("AlarmType", (int)0))
            {
                case 1:  alarmType = CalEvent::ACTIVE;    break;
                case 2:  alarmType = CalEvent::ARCHIVED;  break;
                case 4:  alarmType = CalEvent::TEMPLATE;  break;
                default:
                    qCWarning(KALARM_LOG) << "FileResourceMigrator::migrateKResources: Invalid alarm type for resource";
                    continue;
            }
            const QString name  = configGroup.readEntry("ResourceName", QString());
            const bool enabled  = configGroup.readEntry("ResourceIsActive", false);
            const bool standard = configGroup.readEntry("Standard", false);
            qCDebug(KALARM_LOG) << "FileResourceMigrator::migrateKResources: Migrating:" << name << ", type=" << alarmType << ", path=" << url.toString();
            FileResourceSettings::Ptr settings(new FileResourceSettings(
                          storageType, url, alarmType, name,
                          configGroup.readEntry("Color", QColor()),
                          (enabled ? alarmType : CalEvent::EMPTY),
                          (standard ? alarmType : CalEvent::EMPTY),
                          configGroup.readEntry("ResourceIsReadOnly", true)));
            Resource resource = FileResourceConfigManager::addResource(settings);

            // Update the calendar to the current KAlarm format if necessary,
            // and if the user agrees.
            FileResourceCalendarUpdater* updater = new FileResourceCalendarUpdater(resource, true, this);
            connect(updater, &QObject::destroyed, this, &FileResourceMigrator::checkIfComplete);
            updater->update();   // note that 'updater' will auto-delete when finished

            mExistingAlarmTypes |= alarmType;
        }
    }
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
                  CalEvent::EMPTY, false));
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
    FileResourceCalendarUpdater* updater = new FileResourceCalendarUpdater(resource, true, this);
    connect(updater, &QObject::destroyed, this, &FileResourceMigrator::checkIfComplete);
    updater->update();   // note that 'updater' will auto-delete when finished
}

namespace
{

/******************************************************************************
* Load and parse events from each file in a calendar directory.
*/
bool readDirectoryResource(const QString& dirPath, CalEvent::Types alarmTypes, QHash<CalEvent::Type, QVector<KAEvent>>& events)
{
    if (dirPath.isEmpty())
        return false;
    qCDebug(KALARM_LOG) << "FileResourceMigrator::readDirectoryResource:" << dirPath;
    const QDir dir(dirPath);
    if (!dir.exists())
        return false;

    // Read and parse each file in turn
    QList<QUrl> files;
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

#include "fileresourcemigrator.moc"

// vim: et sw=4:
