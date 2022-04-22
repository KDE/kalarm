/*
 *  akonadiresourcemigrator.cpp  -  migrates KAlarm Akonadi resources
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2011-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "akonadiresourcemigrator.h"

#include "collectionattribute.h"
#include "akonadiplugin_debug.h"

#include <Akonadi/AgentManager>
#include <Akonadi/AttributeFactory>
#include <Akonadi/CollectionFetchJob>
#include <Akonadi/CollectionFetchScope>
#include <Akonadi/CollectionModifyJob>

using namespace KAlarmCal;

//clazy:excludeall=non-pod-global-static

namespace
{
const QString KALARM_RESOURCE(QStringLiteral("akonadi_kalarm_resource"));
const QString KALARM_DIR_RESOURCE(QStringLiteral("akonadi_kalarm_dir_resource"));

// Holds an Akonadi collection's properties.
struct CollectionProperties
{
    QColor          backgroundColour;
    CalEvent::Types alarmTypes;
    CalEvent::Types enabledTypes {CalEvent::EMPTY};
    CalEvent::Types standardTypes {CalEvent::EMPTY};
    bool            readOnly;

    // Fetch the properties of a collection which has been fetched by CollectionFetchJob.
    explicit CollectionProperties(const Akonadi::Collection&);
};

const Akonadi::Collection::Rights WritableRights = Akonadi::Collection::CanChangeItem | Akonadi::Collection::CanCreateItem | Akonadi::Collection::CanDeleteItem;
}

AkonadiResourceMigrator* AkonadiResourceMigrator::mInstance = nullptr;
bool                     AkonadiResourceMigrator::mCompleted = false;

/******************************************************************************
* Constructor.
*/
AkonadiResourceMigrator::AkonadiResourceMigrator(QObject* parent)
    : QObject(parent)
{
}

AkonadiResourceMigrator::~AkonadiResourceMigrator()
{
    qCDebug(AKONADIPLUGIN_LOG) << "~AkonadiResourceMigrator";
    mInstance = nullptr;
    mCompleted = true;
}

/******************************************************************************
* Create and return the unique AkonadiResourceMigrator instance.
*/
AkonadiResourceMigrator* AkonadiResourceMigrator::instance()
{
    if (!mInstance  &&  !mCompleted)
        mInstance = new AkonadiResourceMigrator;
    return mInstance;
}

/******************************************************************************
* Initiate migration of old Akonadi calendars, and create default file system
* resources.
*/
void AkonadiResourceMigrator::initiateMigration()
{
    connect(Akonadi::ServerManager::self(), &Akonadi::ServerManager::stateChanged, this, &AkonadiResourceMigrator::checkServer);
    auto akstate = Akonadi::ServerManager::state();
    mAkonadiStarted = (akstate == Akonadi::ServerManager::NotRunning);
    checkServer(akstate);
}

/******************************************************************************
* Called when the Akonadi server manager changes state.
* Once it is running, migrate any Akonadi KAlarm resources.
*/
void AkonadiResourceMigrator::checkServer(Akonadi::ServerManager::State state)
{
    switch (state)
    {
        case Akonadi::ServerManager::Running:
            migrateResources();
            break;

        case Akonadi::ServerManager::Stopping:
            // Wait until the server has stopped, so that we can restart it.
            return;

        default:
            if (Akonadi::ServerManager::start())
                return;   // wait for the server to change to Running state

            // Can't start Akonadi, so give up trying to migrate.
            qCWarning(AKONADIPLUGIN_LOG) << "AkonadiResourceMigrator::checkServer: Failed to start Akonadi server";
            terminate(false);
            break;
    }

    disconnect(Akonadi::ServerManager::self(), nullptr, this, nullptr);
}

/******************************************************************************
* Initiate migration of Akonadi KAlarm resources.
* Reply = true if migration initiated;
*       = false if no KAlarm Akonadi resources found.
*/
void AkonadiResourceMigrator::migrateResources()
{
    qCDebug(AKONADIPLUGIN_LOG) << "AkonadiResourceMigrator::migrateResources: initiated";
    mCollectionPaths.clear();
    mFetchesPending.clear();
    Akonadi::AttributeFactory::registerAttribute<CollectionAttribute>();

    // Create jobs to fetch all KAlarm Akonadi collections.
    bool migrating = false;
    const Akonadi::AgentInstance::List agents = Akonadi::AgentManager::self()->instances();
    for (const Akonadi::AgentInstance& agent : agents)
    {
        const QString type = agent.type().identifier();
        if (type == KALARM_RESOURCE  ||  type == KALARM_DIR_RESOURCE)
        {
            Akonadi::CollectionFetchJob* job = new Akonadi::CollectionFetchJob(Akonadi::Collection::root(), Akonadi::CollectionFetchJob::FirstLevel);
            job->fetchScope().setResource(agent.identifier());
            mFetchesPending[job] = (type == KALARM_DIR_RESOURCE);
            connect(job, &KJob::result, this, &AkonadiResourceMigrator::collectionFetchResult);
            migrating = true;
        }
    }
    if (!migrating)
        terminate(false);   // there are no Akonadi resources to migrate
}

/******************************************************************************
* Called when an Akonadi collection fetch job has completed.
* Check for, and remove, any Akonadi resources which duplicate use of calendar
* files/directories.
*/
void AkonadiResourceMigrator::collectionFetchResult(KJob* j)
{
    auto job = qobject_cast<Akonadi::CollectionFetchJob*>(j);
    const QString id = job->fetchScope().resource();
    if (j->error())
        qCCritical(AKONADIPLUGIN_LOG) << "AkonadiResourceMigrator::collectionFetchResult: CollectionFetchJob" << id << "error: " << j->errorString();
    else
    {
        const Akonadi::Collection::List collections = job->collections();
        if (collections.isEmpty())
            qCCritical(AKONADIPLUGIN_LOG) << "AkonadiResourceMigrator::collectionFetchResult: No collections found for resource" << id;
        else
        {
            // Note that a KAlarm Akonadi agent contains only one collection.
            const Akonadi::Collection& collection(collections[0]);
            const bool dirType = mFetchesPending.value(job, false);
            const AkResourceData thisRes(job->fetchScope().resource(), collection, dirType);
            bool saveThis = true;
            auto it = mCollectionPaths.constFind(collection.remoteId());
            if (it != mCollectionPaths.constEnd())
            {
                // Remove the resource which, in decreasing order of priority:
                // - Is disabled;
                // - Is not a standard resource;
                // - Contains the higher numbered Collection ID, which is likely
                //   to be the more recently created.
                const AkResourceData prevRes = it.value();
                const CollectionProperties properties[2] = { CollectionProperties(prevRes.collection),
                                                             CollectionProperties(thisRes.collection) };
                int propToUse = (thisRes.collection.id() < prevRes.collection.id()) ? 1 : 0;
                if (properties[1 - propToUse].standardTypes  &&  !properties[propToUse].standardTypes)
                    propToUse = 1 - propToUse;
                if (properties[1 - propToUse].enabledTypes  &&  !properties[propToUse].enabledTypes)
                    propToUse = 1 - propToUse;
                saveThis = (propToUse == 1);

                const auto resourceToRemove = saveThis ? prevRes.resourceId : thisRes.resourceId;
                qCWarning(AKONADIPLUGIN_LOG) << "AkonadiResourceMigrator::collectionFetchResult: Removing duplicate resource" << resourceToRemove;
                Akonadi::AgentManager* agentManager = Akonadi::AgentManager::self();
                agentManager->removeInstance(agentManager->instance(resourceToRemove));
            }
            if (saveThis)
                mCollectionPaths[collection.remoteId()] = thisRes;
        }
    }
    mFetchesPending.remove(job);
    if (mFetchesPending.isEmpty())
    {
        // De-duplication is complete. Migrate the remaining Akonadi resources.
        doMigrateResources();
    }
}

/******************************************************************************
* Migrate Akonadi KAlarm resources to file system resources.
*/
void AkonadiResourceMigrator::doMigrateResources()
{
    qCDebug(AKONADIPLUGIN_LOG) << "AkonadiResourceMigrator::doMigrateResources";

    // First, migrate KAlarm calendar file Akonadi resources.
    // This will allow any KAlarm directory resources to be merged into
    // single file resources, if the user prefers that.
    for (auto it = mCollectionPaths.constBegin();  it != mCollectionPaths.constEnd();  ++it)
    {
        const AkResourceData& resourceData = it.value();
        if (!resourceData.dirType)
            migrateCollection(resourceData.collection, false);
    }

    // Now migrate KAlarm directory Akonadi resources, which must be merged
    // or converted into single file resources.
    for (auto it = mCollectionPaths.constBegin();  it != mCollectionPaths.constEnd();  ++it)
    {
        const AkResourceData& resourceData = it.value();
        if (resourceData.dirType)
            migrateCollection(resourceData.collection, true);
    }

    // The alarm types of all collections have been found.
    mCollectionPaths.clear();
    terminate(true);
}

/******************************************************************************
* Migrate one Akonadi collection to a file system resource.
*/
void AkonadiResourceMigrator::migrateCollection(const Akonadi::Collection& collection, bool dirType)
{
    // Fetch the collection's properties.
    const CollectionProperties colProperties(collection);

    if (!dirType)
    {
        // It's a single file resource.
        qCDebug(AKONADIPLUGIN_LOG) << "AkonadiResourceMigrator: Migrate file resource" << collection.displayName() << ", alarm types:" << (int)colProperties.alarmTypes << ", enabled types:" << (int)colProperties.enabledTypes << ", standard types:" << (int)colProperties.standardTypes;
        Q_EMIT fileResource(collection.resource(),
                            QUrl::fromUserInput(collection.remoteId(), QString(), QUrl::AssumeLocalFile),
                            colProperties.alarmTypes, collection.displayName(), colProperties.backgroundColour,
                            colProperties.enabledTypes, colProperties.standardTypes, colProperties.readOnly);
    }
    else
    {
        // Convert Akonadi directory resource to single file resources.
        qCDebug(AKONADIPLUGIN_LOG) << "AkonadiResourceMigrator: Migrate directory resource" << collection.displayName() << ", alarm types:" << (int)colProperties.alarmTypes << ", enabled types:" << (int)colProperties.enabledTypes;
        Q_EMIT dirResource(collection.resource(), collection.remoteId(),
                           colProperties.alarmTypes, collection.displayName(), colProperties.backgroundColour,
                           colProperties.enabledTypes, colProperties.standardTypes, colProperties.readOnly);
    }
}

/******************************************************************************
* Delete an Akonadi resource after it has been migrated to a file system resource.
*/
void AkonadiResourceMigrator::deleteAkonadiResource(const QString& resourceName)
{
    // Delete the Akonadi resource, to prevent it using CPU, on the
    // assumption that Akonadi access won't be needed by any other
    // application. Excess CPU usage is one of the major bugs which
    // prompted replacing Akonadi resources with file resources.
    Akonadi::AgentManager* agentManager = Akonadi::AgentManager::self();
    const Akonadi::AgentInstance agent = agentManager->instance(resourceName);
    agentManager->removeInstance(agent);
}

/******************************************************************************
* Called when Akonadi migration is complete or is known not to be possible.
*/
void AkonadiResourceMigrator::terminate(bool migrated)
{
    qCDebug(AKONADIPLUGIN_LOG) << "AkonadiResourceMigrator::terminate" << migrated;

    Q_EMIT migrationComplete(migrated);

    // Ignore any further Akonadi server state changes, to prevent possible
    // repeated migrations.
    disconnect(Akonadi::ServerManager::self(), nullptr, this, nullptr);

    if (mAkonadiStarted)
    {
        // The Akonadi server wasn't running before we started it, so stop it
        // now that it's no longer needed.
        Akonadi::ServerManager::stop();
    }

    deleteLater();
}

namespace
{

/******************************************************************************
* Fetch an Akonadi collection's properties.
*/
CollectionProperties::CollectionProperties(const Akonadi::Collection& collection)
{
    readOnly   = (collection.rights() & WritableRights) != WritableRights;
    alarmTypes = CalEvent::types(collection.contentMimeTypes());
    if (collection.hasAttribute<CollectionAttribute>())
    {
        const auto* attr = collection.attribute<CollectionAttribute>();
        enabledTypes     = attr->enabled() & alarmTypes;
        standardTypes    = attr->standard() & enabledTypes;
        backgroundColour = attr->backgroundColor();
    }
}

}

// vim: et sw=4:
