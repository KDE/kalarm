/*
 *  calendarmigrator.cpp  -  migrates or creates KAlarm Akonadi resources
 *  Program:  kalarm
 *  Copyright © 2011-2019 David Jarvie <djarvie@kde.org>
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

#include "calendarmigrator.h"

#include "akonadimodel.h"
#include "functions.h"
#include "kalarmsettings.h"
#include "kalarmdirsettings.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "resources/akonadiresource.h"
#include "kalarm_debug.h"

#include <KAlarmCal/CollectionAttribute>
#include <KAlarmCal/CompatibilityAttribute>
#include <KAlarmCal/Version>

#include <AkonadiCore/AgentInstanceCreateJob>
#include <AkonadiCore/AgentManager>
#include <AkonadiCore/CollectionFetchJob>
#include <AkonadiCore/CollectionFetchScope>
#include <AkonadiCore/CollectionModifyJob>
#include <AkonadiCore/EntityDisplayAttribute>
#include <AkonadiCore/ResourceSynchronizationJob>

#include <KLocalizedString>
#include <KConfig>
#include <KConfigGroup>

#include <QTimer>
#include <QStandardPaths>

using namespace Akonadi;
using namespace KAlarmCal;

namespace
{
const QString KALARM_RESOURCE(QStringLiteral("akonadi_kalarm_resource"));
const QString KALARM_DIR_RESOURCE(QStringLiteral("akonadi_kalarm_dir_resource"));
}

// Creates, or migrates from KResources, a single alarm calendar
class CalendarCreator : public QObject
{
        Q_OBJECT
    public:
        // Constructor to migrate a calendar from KResources.
        CalendarCreator(const QString& resourceType, const KConfigGroup&);
        // Constructor to create a default Akonadi calendar.
        CalendarCreator(CalEvent::Type, const QString& file, const QString& name);
        bool           isValid() const        { return mAlarmType != CalEvent::EMPTY; }
        CalEvent::Type alarmType() const      { return mAlarmType; }
        bool           newCalendar() const    { return mNew; }
        QString        resourceName() const   { return mName; }
        Collection::Id collectionId() const   { return mCollectionId; }
        QString        path() const           { return mUrlString; }
        QString        errorMessage() const   { return mErrorMessage; }
        void           createAgent(const QString& agentType, QObject* parent);

    public Q_SLOTS:
        void agentCreated(KJob*);

    Q_SIGNALS:
        void creating(const QString& path);
        void finished(CalendarCreator*);

    private Q_SLOTS:
        void fetchCollection();
        void collectionFetchResult(KJob*);
        void resourceSynchronised(KJob*);
        void modifyCollectionJobDone(KJob*);

    private:
        void finish(bool cleanup);
        bool writeLocalFileConfig();
        bool writeLocalDirectoryConfig();
        bool writeRemoteFileConfig();
        template <class Interface> Interface* writeBasicConfig();

        enum ResourceType { LocalFile, LocalDir, RemoteFile };

        AgentInstance    mAgent;
        CalEvent::Type   mAlarmType;
        ResourceType     mResourceType;
        QString          mUrlString;
        QString          mName;
        QColor           mColour;
        QString          mErrorMessage;
        Collection::Id   mCollectionId;
        int              mCollectionFetchRetryCount;
        bool             mReadOnly;
        bool             mEnabled;
        bool             mStandard;
        const bool       mNew;     // true if creating default, false if converting
        bool             mFinished{false};
};

// Updates the backend calendar format of a single alarm calendar
class CalendarUpdater : public QObject
{
        Q_OBJECT
    public:
        CalendarUpdater(const Collection& collection, bool dirResource,
                        bool ignoreKeepFormat, bool newCollection, QObject* parent);
        ~CalendarUpdater();
        // Return whether another instance is already updating this collection
        bool isDuplicate() const   { return mDuplicate; }
        // Check whether any instance is for the given collection ID
        static bool containsCollection(Collection::Id);

    public Q_SLOTS:
        bool update();

    private:
        static QList<CalendarUpdater*> mInstances;
        Akonadi::Collection mCollection;
        QObject*            mParent;
        const bool          mDirResource;
        const bool          mIgnoreKeepFormat;
        const bool          mNewCollection;
        const bool          mDuplicate;     // another instance is already updating this collection
};


CalendarMigrator* CalendarMigrator::mInstance = nullptr;
bool              CalendarMigrator::mCompleted = false;

CalendarMigrator::CalendarMigrator(QObject* parent)
    : QObject(parent)
    , mExistingAlarmTypes{}
{
}

CalendarMigrator::~CalendarMigrator()
{
    qCDebug(KALARM_LOG) << "~CalendarMigrator";
    mInstance = nullptr;
}

/******************************************************************************
* Reset to allow migration to be run again.
*/
void CalendarMigrator::reset()
{
    mCompleted = false;
}

/******************************************************************************
* Create and return the unique CalendarMigrator instance.
*/
CalendarMigrator* CalendarMigrator::instance()
{
    if (!mInstance && !mCompleted)
        mInstance = new CalendarMigrator;
    return mInstance;
}

/******************************************************************************
* Migrate old KResource calendars, or if none, create default Akonadi resources.
*/
void CalendarMigrator::execute()
{
    instance()->migrateOrCreate();
}

/******************************************************************************
* Migrate old KResource calendars, and create default Akonadi resources.
*/
void CalendarMigrator::migrateOrCreate()
{
    qCDebug(KALARM_LOG) << "CalendarMigrator::migrateOrCreate";

    // First, check whether any Akonadi resources already exist, and if
    // so, find their alarm types.
    const AgentInstance::List agents = AgentManager::self()->instances();
    for (const AgentInstance& agent : agents)
    {
        const QString type = agent.type().identifier();
        if (type == KALARM_RESOURCE  ||  type == KALARM_DIR_RESOURCE)
        {
            // Fetch the resource's collection to determine its alarm types
            CollectionFetchJob* job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::FirstLevel);
            job->fetchScope().setResource(agent.identifier());
            mFetchesPending << job;
            connect(job, &KJob::result, this, &CalendarMigrator::collectionFetchResult);
            // Note: Once all collections have been fetched, any missing
            //       default resources will be created.
        }
    }

    if (mFetchesPending.isEmpty())
    {
        // There are no Akonadi resources, so migrate any KResources alarm
        // calendars from pre-Akonadi versions of KAlarm.
        const QString configFile = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/kresources/alarms/stdrc");
        const KConfig config(configFile, KConfig::SimpleConfig);

        // Fetch all the KResource identifiers which are actually in use
        const KConfigGroup group = config.group("General");
        const QStringList keys = group.readEntry("ResourceKeys", QStringList())
                               + group.readEntry("PassiveResourceKeys", QStringList());

        // Create an Akonadi resource for each KResource id
        CalendarCreator* creator;
        for (const QString& id : keys)
        {
            const KConfigGroup configGroup = config.group(QLatin1String("Resource_") + id);
            const QString resourceType = configGroup.readEntry("ResourceType", QString());
            QString agentType;
            if (resourceType == QLatin1String("file"))
                agentType = KALARM_RESOURCE;
            else if (resourceType == QLatin1String("dir"))
                agentType = KALARM_DIR_RESOURCE;
            else if (resourceType == QLatin1String("remote"))
                agentType = KALARM_RESOURCE;
            else
                continue;   // unknown resource type - can't convert

            creator = new CalendarCreator(resourceType, configGroup);
            if (!creator->isValid())
                delete creator;
            else
            {
                connect(creator, &CalendarCreator::finished, this, &CalendarMigrator::calendarCreated);
                connect(creator, &CalendarCreator::creating, this, &CalendarMigrator::creatingCalendar);
                mExistingAlarmTypes |= creator->alarmType();
                mCalendarsPending << creator;
                creator->createAgent(agentType, this);
            }
        }

        // After migrating KResources, create any necessary additional default
        // Akonadi resources.
        createDefaultResources();
    }
}

/******************************************************************************
* Called when a collection fetch job has completed.
* Finds which mime types are handled by the existing collection.
*/
void CalendarMigrator::collectionFetchResult(KJob* j)
{
    CollectionFetchJob* job = static_cast<CollectionFetchJob*>(j);
    const QString id = job->fetchScope().resource();
    if (j->error())
        qCCritical(KALARM_LOG) << "CalendarMigrator::collectionFetchResult: CollectionFetchJob" << id << "error: " << j->errorString();
    else
    {
        const Collection::List collections = job->collections();
        if (collections.isEmpty())
            qCCritical(KALARM_LOG) << "CalendarMigrator::collectionFetchResult: No collections found for resource" << id;
        else
            mExistingAlarmTypes |= CalEvent::types(collections[0].contentMimeTypes());
    }
    mFetchesPending.removeAll(job);

    if (mFetchesPending.isEmpty())
    {
        // The alarm types of all collections have been found, so now
        // create any necessary default Akonadi resources.
        createDefaultResources();
    }
}

/******************************************************************************
* Create default Akonadi resources for any alarm types not covered by existing
* resources. Normally, this occurs on the first run of KAlarm, but if resources
* have been deleted, it could occur on later runs.
* If the default calendar files already exist, they will be used; otherwise
* they will be created.
*/
void CalendarMigrator::createDefaultResources()
{
    qCDebug(KALARM_LOG) << "CalendarMigrator::createDefaultResources";
    CalendarCreator* creator;
    if (!(mExistingAlarmTypes & CalEvent::ACTIVE))
    {
        creator = new CalendarCreator(CalEvent::ACTIVE, QStringLiteral("calendar.ics"), i18nc("@info", "Active Alarms"));
        connect(creator, &CalendarCreator::finished, this, &CalendarMigrator::calendarCreated);
        connect(creator, &CalendarCreator::creating, this, &CalendarMigrator::creatingCalendar);
        mCalendarsPending << creator;
        creator->createAgent(KALARM_RESOURCE, this);
    }
    if (!(mExistingAlarmTypes & CalEvent::ARCHIVED))
    {
        creator = new CalendarCreator(CalEvent::ARCHIVED, QStringLiteral("expired.ics"), i18nc("@info", "Archived Alarms"));
        connect(creator, &CalendarCreator::finished, this, &CalendarMigrator::calendarCreated);
        connect(creator, &CalendarCreator::creating, this, &CalendarMigrator::creatingCalendar);
        mCalendarsPending << creator;
        creator->createAgent(KALARM_RESOURCE, this);
    }
    if (!(mExistingAlarmTypes & CalEvent::TEMPLATE))
    {
        creator = new CalendarCreator(CalEvent::TEMPLATE, QStringLiteral("template.ics"), i18nc("@info", "Alarm Templates"));
        connect(creator, &CalendarCreator::finished, this, &CalendarMigrator::calendarCreated);
        connect(creator, &CalendarCreator::creating, this, &CalendarMigrator::creatingCalendar);
        mCalendarsPending << creator;
        creator->createAgent(KALARM_RESOURCE, this);
    }

    if (mCalendarsPending.isEmpty())
    {
        mCompleted = true;
        deleteLater();
    }
}

/******************************************************************************
* Called when a calendar resource is about to be created.
* Emits the 'creating' signal.
*/
void CalendarMigrator::creatingCalendar(const QString& path)
{
    Q_EMIT creating(path, -1, false);
}

/******************************************************************************
* Called when creation of a migrated or new default calendar resource has
* completed or failed.
*/
void CalendarMigrator::calendarCreated(CalendarCreator* creator)
{
    int i = mCalendarsPending.indexOf(creator);
    if (i < 0)
        return;    // calendar already finished

    Q_EMIT creating(creator->path(), creator->collectionId(), true);

    if (!creator->errorMessage().isEmpty())
    {
        QString errmsg = creator->newCalendar()
                       ? xi18nc("@info/plain", "Failed to create default calendar <resource>%1</resource>", creator->resourceName())
                       : xi18nc("@info/plain 'Import Alarms' is the name of a menu option",
                               "Failed to convert old configuration for calendar <resource>%1</resource>. "
                               "Please use Import Alarms to load its alarms into a new or existing calendar.", creator->resourceName());
        const QString locn = i18nc("@info File path or URL", "Location: %1", creator->path());
        if (creator->errorMessage().isEmpty())
            errmsg = xi18nc("@info", "<para>%1</para><para>%2</para>", errmsg, locn);
        else
            errmsg = xi18nc("@info", "<para>%1</para><para>%2<nl/>(%3)</para>", errmsg, locn, creator->errorMessage());
        KAMessageBox::error(MainWindow::mainMainWindow(), errmsg);
    }
    creator->deleteLater();

    mCalendarsPending.removeAt(i);    // remove it from the pending list
    if (mCalendarsPending.isEmpty())
    {
        mCompleted = true;
        deleteLater();
    }
}

/******************************************************************************
* If an existing Akonadi resource calendar can be converted to the current
* KAlarm format, prompt the user whether to convert it, and if yes, tell the
* Akonadi resource to update the backend storage to the current format.
* The CollectionAttribute's KeepFormat property will be updated if the user
* chooses not to update the calendar.
*
* Note: the collection should be up to date: use AkonadiModel::refresh() before
*       calling this function.
*/
void CalendarMigrator::updateToCurrentFormat(const Resource& resource, bool ignoreKeepFormat, QWidget* parent)
{
    qCDebug(KALARM_LOG) << "CalendarMigrator::updateToCurrentFormat:" << resource.id();
    if (CalendarUpdater::containsCollection(resource.id()))
        return;   // prevent multiple simultaneous user prompts
    const AgentInstance agent = AgentManager::self()->instance(resource.configName());
    const QString id = agent.type().identifier();
    bool dirResource;
    if (id == KALARM_RESOURCE)
        dirResource = false;
    else if (id == KALARM_DIR_RESOURCE)
        dirResource = true;
    else
    {
        qCCritical(KALARM_LOG) << "CalendarMigrator::updateToCurrentFormat: Invalid agent type" << id;
        return;
    }
    const Collection& collection = AkonadiResource::collection(resource);
    if (!parent)
        parent = MainWindow::mainMainWindow();
    CalendarUpdater* updater = new CalendarUpdater(collection, dirResource, ignoreKeepFormat, false, parent);
    QTimer::singleShot(0, updater, &CalendarUpdater::update);
}

/*===========================================================================*/

QList<CalendarUpdater*> CalendarUpdater::mInstances;

CalendarUpdater::CalendarUpdater(const Collection& collection, bool dirResource,
                                 bool ignoreKeepFormat, bool newCollection, QObject* parent)
    : mCollection(collection)
    , mParent(parent)
    , mDirResource(dirResource)
    , mIgnoreKeepFormat(ignoreKeepFormat)
    , mNewCollection(newCollection)
    , mDuplicate(containsCollection(collection.id()))
{
    mInstances.append(this);
}

CalendarUpdater::~CalendarUpdater()
{
    mInstances.removeAll(this);
}

bool CalendarUpdater::containsCollection(Collection::Id id)
{
    for (CalendarUpdater* instance : mInstances)
    {
        if (instance->mCollection.id() == id)
            return true;
    }
    return false;
}

bool CalendarUpdater::update()
{
    qCDebug(KALARM_LOG) << "CalendarUpdater::update:" << mCollection.id() << (mDirResource ? "directory" : "file");
    bool result = true;
    if (mDuplicate)
        qCDebug(KALARM_LOG) << "CalendarUpdater::update: Not updating (concurrent update in progress)";
    else if (mCollection.hasAttribute<CompatibilityAttribute>())   // must know format to update
    {
        const CompatibilityAttribute* compatAttr = mCollection.attribute<CompatibilityAttribute>();
        const KACalendar::Compat compatibility = compatAttr->compatibility();
        qCDebug(KALARM_LOG) << "CalendarUpdater::update: current format:" << compatibility;
        if ((compatibility & ~KACalendar::Converted)
        // The calendar isn't in the current KAlarm format
        &&  !(compatibility & ~(KACalendar::Convertible | KACalendar::Converted)))
        {
            // The calendar format is convertible to the current KAlarm format
            if (!mIgnoreKeepFormat
            &&  mCollection.hasAttribute<CollectionAttribute>()
            &&  mCollection.attribute<CollectionAttribute>()->keepFormat())
                qCDebug(KALARM_LOG) << "CalendarUpdater::update: Not updating format (previous user choice)";
            else
            {
                // The user hasn't previously said not to convert it
                const QString versionString = KAlarmCal::getVersionString(compatAttr->version());
                const QString msg = KAlarm::conversionPrompt(mCollection.name(), versionString, false);
                qCDebug(KALARM_LOG) << "CalendarUpdater::update: Version" << versionString;
                if (KAMessageBox::warningYesNo(qobject_cast<QWidget*>(mParent), msg) != KMessageBox::Yes)
                    result = false;   // the user chose not to update the calendar
                else
                {
                    // Tell the resource to update the backend storage format
                    QString errmsg;
                    if (!mNewCollection)
                    {
                        // Refetch the collection's details because anything could
                        // have happened since the prompt was first displayed.
                        if (!AkonadiModel::instance()->refresh(mCollection))
                            errmsg = i18nc("@info", "Invalid collection");
                    }
                    if (errmsg.isEmpty())
                    {
                        const AgentInstance agent = AgentManager::self()->instance(mCollection.resource());
                        if (mDirResource)
                            CalendarMigrator::updateStorageFormat<OrgKdeAkonadiKAlarmDirSettingsInterface>(agent, errmsg, mParent);
                        else
                            CalendarMigrator::updateStorageFormat<OrgKdeAkonadiKAlarmSettingsInterface>(agent, errmsg, mParent);
                    }
                    if (!errmsg.isEmpty())
                    {
                        KAMessageBox::error(MainWindow::mainMainWindow(),
                                            xi18nc("@info", "%1<nl/>(%2)",
                                                  xi18nc("@info/plain", "Failed to update format of calendar <resource>%1</resource>", mCollection.name()),
                                            errmsg));
                    }
                }
                if (!mNewCollection)
                {
                    // Record the user's choice of whether to update the calendar
                    Resource resource = AkonadiModel::instance()->resource(mCollection.id());
                    resource.setKeepFormat(!result);
                }
            }
        }
    }
    deleteLater();
    return result;
}

/******************************************************************************
* Tell an Akonadi resource to update the backend storage format to the current
* KAlarm format.
* Reply = true if success; if false, 'errorMessage' contains the error message.
*/
template <class Interface> bool CalendarMigrator::updateStorageFormat(const AgentInstance& agent, QString& errorMessage, QObject* parent)
{
    qCDebug(KALARM_LOG) << "CalendarMigrator::updateStorageFormat";
    Interface* iface = getAgentInterface<Interface>(agent, errorMessage, parent);
    if (!iface)
    {
        qCDebug(KALARM_LOG) << "CalendarMigrator::updateStorageFormat:" << errorMessage;
        return false;
    }
    iface->setUpdateStorageFormat(true);
    iface->save();
    delete iface;
    qCDebug(KALARM_LOG) << "CalendarMigrator::updateStorageFormat: true";
    return true;
}

/******************************************************************************
* Create a D-Bus interface to an Akonadi resource.
* Reply = interface if success
*       = 0 if error: 'errorMessage' contains the error message.
*/
template <class Interface> Interface* CalendarMigrator::getAgentInterface(const AgentInstance& agent, QString& errorMessage, QObject* parent)
{
    Interface* iface = new Interface(QLatin1String("org.freedesktop.Akonadi.Resource.") + agent.identifier(),
              QStringLiteral("/Settings"), QDBusConnection::sessionBus(), parent);
    if (!iface->isValid())
    {
        errorMessage = iface->lastError().message();
        qCDebug(KALARM_LOG) << "CalendarMigrator::getAgentInterface: D-Bus error accessing resource:" << errorMessage;
        delete iface;
        return nullptr;
    }
    return iface;
}


/******************************************************************************
* Constructor to migrate a KResources calendar, using its parameters.
*/
CalendarCreator::CalendarCreator(const QString& resourceType, const KConfigGroup& config)
    : mAlarmType(CalEvent::EMPTY)
    , mNew(false)
{
    // Read the resource configuration parameters from the config
    const char* pathKey = nullptr;
    if (resourceType == QLatin1String("file"))
    {
        mResourceType = LocalFile;
        pathKey = "CalendarURL";
    }
    else if (resourceType == QLatin1String("dir"))
    {
        mResourceType = LocalDir;
        pathKey = "CalendarURL";
    }
    else if (resourceType == QLatin1String("remote"))
    {
        mResourceType = RemoteFile;
        pathKey = "DownloadUrl";
    }
    else
    {
        qCCritical(KALARM_LOG) << "CalendarCreator: Invalid resource type:" << resourceType;
        return;
    }
    const QString path = config.readPathEntry(pathKey, QString());
    mUrlString = QUrl::fromUserInput(path).toString();
    switch (config.readEntry("AlarmType", (int)0))
    {
        case 1:  mAlarmType = CalEvent::ACTIVE;  break;
        case 2:  mAlarmType = CalEvent::ARCHIVED;  break;
        case 4:  mAlarmType = CalEvent::TEMPLATE;  break;
        default:
            qCCritical(KALARM_LOG) << "CalendarCreator: Invalid alarm type for resource";
            return;
    }
    mName     = config.readEntry("ResourceName", QString());
    mColour   = config.readEntry("Color", QColor());
    mReadOnly = config.readEntry("ResourceIsReadOnly", true);
    mEnabled  = config.readEntry("ResourceIsActive", false);
    mStandard = config.readEntry("Standard", false);
    qCDebug(KALARM_LOG) << "CalendarCreator: Migrating:" << mName << ", type=" << mAlarmType << ", path=" << mUrlString;
}

/******************************************************************************
* Constructor to create a new default local file resource.
* This is created as enabled, read-write, and standard for its alarm type.
*/
CalendarCreator::CalendarCreator(CalEvent::Type alarmType, const QString& file, const QString& name)
    : mAlarmType(alarmType)
    , mResourceType(LocalFile)
    , mName(name)
    , mColour()
    , mReadOnly(false)
    , mEnabled(true)
    , mStandard(true)
    , mNew(true)
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QLatin1Char('/') + file;
    mUrlString = QUrl::fromLocalFile(path).toString();
    qCDebug(KALARM_LOG) << "CalendarCreator: New:" << mName << ", type=" << mAlarmType << ", path=" << mUrlString;
}

/******************************************************************************
* Create the Akonadi agent for this calendar.
*/
void CalendarCreator::createAgent(const QString& agentType, QObject* parent)
{
    Q_EMIT creating(mUrlString);
    AgentInstanceCreateJob* job = new AgentInstanceCreateJob(agentType, parent);
    connect(job, &KJob::result, this, &CalendarCreator::agentCreated);
    job->start();
}

/******************************************************************************
* Called when the agent creation job for this resource has completed.
* Applies the calendar resource configuration to the Akonadi agent.
*/
void CalendarCreator::agentCreated(KJob* j)
{
    if (j->error())
    {
        mErrorMessage = j->errorString();
        qCCritical(KALARM_LOG) << "CalendarCreator::agentCreated: AgentInstanceCreateJob error:" << mErrorMessage;
        finish(false);
        return;
    }

    // Configure the Akonadi Agent
    qCDebug(KALARM_LOG) << "CalendarCreator::agentCreated:" << mName;
    AgentInstanceCreateJob* job = static_cast<AgentInstanceCreateJob*>(j);
    mAgent = job->instance();
    mAgent.setName(mName);
    bool ok = false;
    switch (mResourceType)
    {
        case LocalFile:
            ok = writeLocalFileConfig();
            break;
        case LocalDir:
            ok = writeLocalDirectoryConfig();
            break;
        case RemoteFile:
            ok = writeRemoteFileConfig();
            break;
        default:
            qCCritical(KALARM_LOG) << "CalendarCreator::agentCreated: Invalid resource type";
            break;
    }
    if (!ok)
    {
        finish(true);
        return;
    }
    mAgent.reconfigure();   // notify the agent that its configuration has been changed

    // Wait for the resource to create its collection and synchronize the backend storage.
    ResourceSynchronizationJob* sjob = new ResourceSynchronizationJob(mAgent);
    connect(sjob, &KJob::result, this, &CalendarCreator::resourceSynchronised);
    sjob->start();   // this is required (not an Akonadi::Job)
}

/******************************************************************************
* Called when a resource synchronization job has completed.
* Fetches the collection which this agent manages.
*/
void CalendarCreator::resourceSynchronised(KJob* j)
{
    qCDebug(KALARM_LOG) << "CalendarCreator::resourceSynchronised:" << mName;
    if (j->error())
    {
        // Don't give up on error - we can still try to fetch the collection
        qCCritical(KALARM_LOG) << "ResourceSynchronizationJob error: " << j->errorString();
        // Try again to synchronize the backend storage.
        mAgent.synchronize();
    }
    mCollectionFetchRetryCount = 0;
    fetchCollection();
}

/******************************************************************************
* Find the collection which this agent manages.
*/
void CalendarCreator::fetchCollection()
{
    CollectionFetchJob* job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::FirstLevel);
    job->fetchScope().setResource(mAgent.identifier());
    connect(job, &KJob::result, this, &CalendarCreator::collectionFetchResult);
    job->start();
}

bool CalendarCreator::writeLocalFileConfig()
{
    OrgKdeAkonadiKAlarmSettingsInterface* iface = writeBasicConfig<OrgKdeAkonadiKAlarmSettingsInterface>();
    if (!iface)
        return false;
    iface->setMonitorFile(true);
    iface->save();   // save the Agent config changes
    delete iface;
    return true;
}

bool CalendarCreator::writeLocalDirectoryConfig()
{
    OrgKdeAkonadiKAlarmDirSettingsInterface* iface = writeBasicConfig<OrgKdeAkonadiKAlarmDirSettingsInterface>();
    if (!iface)
        return false;
    iface->setMonitorFiles(true);
    iface->save();   // save the Agent config changes
    delete iface;
    return true;
}

bool CalendarCreator::writeRemoteFileConfig()
{
    OrgKdeAkonadiKAlarmSettingsInterface* iface = writeBasicConfig<OrgKdeAkonadiKAlarmSettingsInterface>();
    if (!iface)
        return false;
    iface->setMonitorFile(true);
    iface->save();   // save the Agent config changes
    delete iface;
    return true;
}

template <class Interface> Interface* CalendarCreator::writeBasicConfig()
{
    Interface* iface = CalendarMigrator::getAgentInterface<Interface>(mAgent, mErrorMessage, this);
    if (iface)
    {
        iface->setReadOnly(mReadOnly);
        iface->setDisplayName(mName);
        iface->setPath(mUrlString);    // this must be a full URL, not a local path
        iface->setAlarmTypes(CalEvent::mimeTypes(mAlarmType));
        iface->setUpdateStorageFormat(false);
    }
    return iface;
}

/******************************************************************************
* Called when a collection fetch job has completed.
* Obtains the collection handled by the agent, and configures it.
*/
void CalendarCreator::collectionFetchResult(KJob* j)
{
    qCDebug(KALARM_LOG) << "CalendarCreator::collectionFetchResult:" << mName;
    if (j->error())
    {
        mErrorMessage = j->errorString();
        qCCritical(KALARM_LOG) << "CalendarCreator::collectionFetchResult: CollectionFetchJob error: " << mErrorMessage;
        finish(true);
        return;
    }
    CollectionFetchJob* job = static_cast<CollectionFetchJob*>(j);
    const Collection::List collections = job->collections();
    if (collections.isEmpty())
    {
        if (++mCollectionFetchRetryCount >= 10)
        {
            mErrorMessage = i18nc("@info", "New configuration timed out");
            qCCritical(KALARM_LOG) << "CalendarCreator::collectionFetchResult: Timeout fetching collection for resource";
            finish(true);
            return;
        }
        // Need to wait a bit longer until the resource has initialised and
        // created its collection. Retry after 200ms.
        qCDebug(KALARM_LOG) << "CalendarCreator::collectionFetchResult: Retrying";
        QTimer::singleShot(200, this, &CalendarCreator::fetchCollection);
        return;
    }
    if (collections.count() > 1)
    {
        mErrorMessage = i18nc("@info", "New configuration was corrupt");
        qCCritical(KALARM_LOG) << "CalendarCreator::collectionFetchResult: Wrong number of collections for this resource:" << collections.count();
        finish(true);
        return;
    }

    // Set Akonadi Collection attributes
    Collection collection = collections[0];
    mCollectionId = collection.id();
    collection.setContentMimeTypes(CalEvent::mimeTypes(mAlarmType));
    EntityDisplayAttribute* dattr = collection.attribute<EntityDisplayAttribute>(Collection::AddIfMissing);
    dattr->setIconName(QStringLiteral("kalarm"));
    CollectionAttribute* attr = collection.attribute<CollectionAttribute>(Collection::AddIfMissing);
    attr->setEnabled(mEnabled ? mAlarmType : CalEvent::EMPTY);
    if (mStandard)
        attr->setStandard(mAlarmType);
    if (mColour.isValid())
        attr->setBackgroundColor(mColour);

    // Update the calendar to the current KAlarm format if necessary,
    // and if the user agrees.
    bool dirResource = false;
    switch (mResourceType)
    {
        case LocalFile:
        case RemoteFile:
            break;
        case LocalDir:
            dirResource = true;
            break;
        default:
            Q_ASSERT(0); // Invalid resource type
            break;
    }
    bool keep = false;
    bool duplicate = false;
    if (!mReadOnly)
    {
        CalendarUpdater* updater = new CalendarUpdater(collection, dirResource, false, true, this);
        duplicate = updater->isDuplicate();
        keep = !updater->update();   // note that 'updater' will auto-delete when finished
    }
    if (!duplicate)
    {
        // Record the user's choice of whether to update the calendar
        attr->setKeepFormat(keep);
    }

    // Update the collection's CollectionAttribute value in the Akonadi database.
    // Note that we can't supply 'collection' to CollectionModifyJob since
    // that also contains the CompatibilityAttribute value, which is read-only
    // for applications. So create a new Collection instance and only set a
    // value for CollectionAttribute.
    Collection c(collection.id());
    CollectionAttribute* att = c.attribute<CollectionAttribute>(Collection::AddIfMissing);
    *att = *attr;
    CollectionModifyJob* cmjob = new CollectionModifyJob(c, this);
    connect(cmjob, &KJob::result, this, &CalendarCreator::modifyCollectionJobDone);
}

/******************************************************************************
* Called when a collection modification job has completed.
* Checks for any error.
*/
void CalendarCreator::modifyCollectionJobDone(KJob* j)
{
    Collection collection = static_cast<CollectionModifyJob*>(j)->collection();
    if (j->error())
    {
        mErrorMessage = j->errorString();
        qCCritical(KALARM_LOG) << "CalendarCreator::modifyCollectionJobDone: CollectionFetchJob error: " << mErrorMessage;
        finish(true);
    }
    else
    {
        qCDebug(KALARM_LOG) << "CalendarCreator::modifyCollectionJobDone: Completed:" << mName;
        finish(false);
    }
}

/******************************************************************************
* Emit the finished() signal. If 'cleanup' is true, delete the newly created
* but incomplete Agent.
*/
void CalendarCreator::finish(bool cleanup)
{
    if (!mFinished)
    {
        if (cleanup)
            AgentManager::self()->removeInstance(mAgent);
        mFinished = true;
        Q_EMIT finished(this);
    }
}

#include "calendarmigrator.moc"

// vim: et sw=4:
