/*
 *  calendarmigrator.cpp  -  migrates or creates KAlarm Akonadi resources
 *  Program:  kalarm
 *  Copyright © 2011-2012 by David Jarvie <djarvie@kde.org>
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

#include <kalarmcal/collectionattribute.h>
#include <kalarmcal/compatibilityattribute.h>
#include <kalarmcal/version.h>

#include <akonadi/agentinstancecreatejob.h>
#include <akonadi/agentmanager.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/collectionmodifyjob.h>
#include <akonadi/entitydisplayattribute.h>
#include <akonadi/resourcesynchronizationjob.h>

#include <klocale.h>
#include <kconfiggroup.h>
#include <kstandarddirs.h>
#include <kdebug.h>

#include <QTimer>

using namespace Akonadi;
using namespace KAlarmCal;


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
        QString        path() const           { return mPath; }
        QString        errorMessage() const   { return mErrorMessage; }
        void           createAgent(const QString& agentType, QObject* parent);

    public slots:
        void agentCreated(KJob*);

    signals:
        void creating(const QString& path);
        void finished(CalendarCreator*);

    private slots:
        void fetchCollection();
        void collectionFetchResult(KJob*);
        void resourceSynchronised(KJob*);
        void modifyCollectionJobDone(KJob*);

    private:
        void finish(bool cleanup);
        bool migrateLocalFile();
        bool migrateLocalDirectory();
        bool migrateRemoteFile();
        template <class Interface> Interface* migrateBasic();

        enum ResourceType { LocalFile, LocalDir, RemoteFile };

        Akonadi::AgentInstance mAgent;
        CalEvent::Type         mAlarmType;
        ResourceType           mResourceType;
        QString                mPath;
        QString                mName;
        QColor                 mColour;
        QString                mErrorMessage;
        int                    mCollectionFetchRetryCount;
        bool                   mReadOnly;
        bool                   mEnabled;
        bool                   mStandard;
        const bool             mNew;
        bool                   mFinished;
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

    public slots:
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


CalendarMigrator* CalendarMigrator::mInstance = 0;

CalendarMigrator::CalendarMigrator(QObject* parent)
    : QObject(parent),
      mExistingAlarmTypes(0)
{
}

CalendarMigrator::~CalendarMigrator()
{
    kDebug();
}

/******************************************************************************
* Create and return the unique CalendarMigrator instance.
*/
CalendarMigrator* CalendarMigrator::instance()
{
    if (!mInstance)
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
    kDebug();

    // First, check whether any Akonadi resources already exist, and if
    // so, find their alarm types.
    const AgentInstance::List agents = AgentManager::self()->instances();
    foreach (const AgentInstance& agent, agents)
    {
        const QString type = agent.type().identifier();
        if (type == QLatin1String("akonadi_kalarm_resource")
        ||  type == QLatin1String("akonadi_kalarm_dir_resource"))
        {
            // Fetch the resource's collection to determine its alarm types
            CollectionFetchJob* job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::FirstLevel);
            job->fetchScope().setResource(agent.identifier());
            mFetchesPending << job;
            connect(job, SIGNAL(result(KJob*)), SLOT(collectionFetchResult(KJob*)));
            // Note: Once all collections have been fetched, any missing
            //       default resources will be created.
        }
    }

    if (mFetchesPending.isEmpty())
    {
        // There are no Akonadi resources, so migrate any KResources alarm
        // calendars from pre-Akonadi versions of KAlarm.
        const QString configFile = KStandardDirs::locateLocal("config", QLatin1String("kresources/alarms/stdrc"));
        const KConfig config(configFile, KConfig::SimpleConfig);

        // Fetch all the KResource identifiers which are actually in use
        const KConfigGroup group = config.group("General");
        const QStringList keys = group.readEntry("ResourceKeys", QStringList())
                               + group.readEntry("PassiveResourceKeys", QStringList());

        // Create an Akonadi resource for each KResource id
        CalendarCreator* creator;
        foreach (const QString& id, keys)
        {
            const KConfigGroup configGroup = config.group(QLatin1String("Resource_") + id);
            const QString resourceType = configGroup.readEntry("ResourceType", QString());
            QString agentType;
            if (resourceType == QLatin1String("file"))
                agentType = QLatin1String("akonadi_kalarm_resource");
            else if (resourceType == QLatin1String("dir"))
                agentType = QLatin1String("akonadi_kalarm_dir_resource");
            else if (resourceType == QLatin1String("remote"))
                agentType = QLatin1String("akonadi_kalarm_resource");
            else
                continue;   // unknown resource type - can't convert

            creator = new CalendarCreator(resourceType, configGroup);
            if (!creator->isValid())
                delete creator;
            else
            {
                connect(creator, SIGNAL(finished(CalendarCreator*)), SLOT(calendarCreated(CalendarCreator*)));
                connect(creator, SIGNAL(creating(QString)), SLOT(creatingCalendar(QString)));
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
        kError() << "CollectionFetchJob" << id << "error: " << j->errorString();
    else
    {
        const Collection::List collections = job->collections();
        if (collections.isEmpty())
            kError() << "No collections found for resource" << id;
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
    kDebug();
    CalendarCreator* creator;
    if (!(mExistingAlarmTypes & CalEvent::ACTIVE))
    {
        creator = new CalendarCreator(CalEvent::ACTIVE, QLatin1String("calendar.ics"), i18nc("@info/plain", "Active Alarms"));
        connect(creator, SIGNAL(finished(CalendarCreator*)), SLOT(calendarCreated(CalendarCreator*)));
        connect(creator, SIGNAL(creating(QString)), SLOT(creatingCalendar(QString)));
        mCalendarsPending << creator;
        creator->createAgent(QLatin1String("akonadi_kalarm_resource"), this);
    }
    if (!(mExistingAlarmTypes & CalEvent::ARCHIVED))
    {
        creator = new CalendarCreator(CalEvent::ARCHIVED, QLatin1String("expired.ics"), i18nc("@info/plain", "Archived Alarms"));
        connect(creator, SIGNAL(finished(CalendarCreator*)), SLOT(calendarCreated(CalendarCreator*)));
        connect(creator, SIGNAL(creating(QString)), SLOT(creatingCalendar(QString)));
        mCalendarsPending << creator;
        creator->createAgent(QLatin1String("akonadi_kalarm_resource"), this);
    }
    if (!(mExistingAlarmTypes & CalEvent::TEMPLATE))
    {
        creator = new CalendarCreator(CalEvent::TEMPLATE, QLatin1String("template.ics"), i18nc("@info/plain", "Alarm Templates"));
        connect(creator, SIGNAL(finished(CalendarCreator*)), SLOT(calendarCreated(CalendarCreator*)));
        connect(creator, SIGNAL(creating(QString)), SLOT(creatingCalendar(QString)));
        mCalendarsPending << creator;
        creator->createAgent(QLatin1String("akonadi_kalarm_resource"), this);
    }

    if (mCalendarsPending.isEmpty())
        deleteLater();
}

/******************************************************************************
* Called when a calendar resource is about to be created.
* Emits the 'creating' signal.
*/
void CalendarMigrator::creatingCalendar(const QString& path)
{
    emit creating(path, false);
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

    emit creating(creator->path(), true);

    if (!creator->errorMessage().isEmpty())
    {
        QString errmsg = creator->newCalendar()
                       ? i18nc("@info/plain", "Failed to create default calendar <resource>%1</resource>", creator->resourceName())
                       : i18nc("@info/plain 'Import Alarms' is the name of a menu option",
                               "Failed to convert old configuration for calendar <resource>%1</resource>. "
                               "Please use Import Alarms to load its alarms into a new or existing calendar.", creator->resourceName());
        const QString locn = i18nc("@info/plain File path or URL", "Location: %1", creator->path());
        if (creator->errorMessage().isEmpty())
            errmsg = i18nc("@info", "<para>%1</para><para>%2</para>", errmsg, locn);
        else
            errmsg = i18nc("@info", "<para>%1</para><para>%2<nl/>(%3)</para>", errmsg, locn, creator->errorMessage());
        KAMessageBox::error(MainWindow::mainMainWindow(), errmsg);
    }
    creator->deleteLater();

    mCalendarsPending.removeAt(i);    // remove it from the pending list
    if (mCalendarsPending.isEmpty())
        deleteLater();
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
void CalendarMigrator::updateToCurrentFormat(const Collection& collection, bool ignoreKeepFormat, QWidget* parent)
{
    kDebug() << collection.id();
    if (CalendarUpdater::containsCollection(collection.id()))
        return;   // prevent multiple simultaneous user prompts
    const AgentInstance agent = AgentManager::self()->instance(collection.resource());
    const QString id = agent.type().identifier();
    bool dirResource;
    if (id == QLatin1String("akonadi_kalarm_resource"))
        dirResource = false;
    else if (id == QLatin1String("akonadi_kalarm_dir_resource"))
        dirResource = true;
    else
    {
        kError() << "Invalid agent type" << id;
        return;
    }
    CalendarUpdater* updater = new CalendarUpdater(collection, dirResource, ignoreKeepFormat, false, parent);
    QTimer::singleShot(0, updater, SLOT(update()));
}


QList<CalendarUpdater*> CalendarUpdater::mInstances;

CalendarUpdater::CalendarUpdater(const Collection& collection, bool dirResource,
                                 bool ignoreKeepFormat, bool newCollection, QObject* parent)
    : mCollection(collection),
      mParent(parent),
      mDirResource(dirResource),
      mIgnoreKeepFormat(ignoreKeepFormat),
      mNewCollection(newCollection),
      mDuplicate(containsCollection(collection.id()))
{
    mInstances.append(this);
}

CalendarUpdater::~CalendarUpdater()
{
    mInstances.removeAll(this);
}

bool CalendarUpdater::containsCollection(Collection::Id id)
{
    for (int i = 0, count = mInstances.count();  i < count;  ++i)
    {
        if (mInstances[i]->mCollection.id() == id)
            return true;
    }
    return false;
}

bool CalendarUpdater::update()
{
    kDebug() << mCollection.id() << (mDirResource ? "directory" : "file");
    bool result = true;
    if (!mDuplicate     // prevent concurrent updates
    &&  mCollection.hasAttribute<CompatibilityAttribute>())   // must know format to update
    {
        const CompatibilityAttribute* compatAttr = mCollection.attribute<CompatibilityAttribute>();
        const KACalendar::Compat compatibility = compatAttr->compatibility();
        if ((compatibility & ~KACalendar::Converted)
        // The calendar isn't in the current KAlarm format
        &&  !(compatibility & ~(KACalendar::Convertible | KACalendar::Converted)))
        {
            // The calendar format is convertible to the current KAlarm format
            if (!mIgnoreKeepFormat
            &&  mCollection.hasAttribute<CollectionAttribute>()
            &&  mCollection.attribute<CollectionAttribute>()->keepFormat())
                kDebug() << "Not updating format (previous user choice)";
            else
            {
                // The user hasn't previously said not to convert it
                const QString versionString = KAlarmCal::getVersionString(compatAttr->version());
                const QString msg = KAlarm::conversionPrompt(mCollection.name(), versionString, false);
                kDebug() << "Version" << versionString;
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
                            errmsg = i18nc("@info/plain", "Invalid collection");
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
                                            i18nc("@info", "%1<nl/>(%2)",
                                                  i18nc("@info/plain", "Failed to update format of calendar <resource>%1</resource>", mCollection.name()),
                                            errmsg));
                    }
                }
                if (!mNewCollection)
                {
                    // Record the user's choice of whether to update the calendar
                    const QModelIndex ix = AkonadiModel::instance()->collectionIndex(mCollection);
                    AkonadiModel::instance()->setData(ix, !result, AkonadiModel::KeepFormatRole);
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
    kDebug();
    Interface* iface = getAgentInterface<Interface>(agent, errorMessage, parent);
    if (!iface)
    {
        kDebug() << errorMessage;
        return false;
    }
    iface->setUpdateStorageFormat(true);
    iface->writeConfig();
    delete iface;
    kDebug() << "true";
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
              QLatin1String("/Settings"), QDBusConnection::sessionBus(), parent);
    if (!iface->isValid())
    {
        errorMessage = iface->lastError().message();
        kDebug() << "D-Bus error accessing resource:" << errorMessage;
        delete iface;
        return 0;
    }
    return iface;
}


/******************************************************************************
* Constructor to migrate a KResources calendar, using its parameters.
*/
CalendarCreator::CalendarCreator(const QString& resourceType, const KConfigGroup& config)
    : mAlarmType(CalEvent::EMPTY),
      mNew(false),
      mFinished(false)
{
    // Read the resource configuration parameters from the config
    const char* pathKey = 0;
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
        kError() << "Invalid resource type:" << resourceType;
        return;
    }
    mPath = config.readPathEntry(pathKey, QLatin1String(""));
    switch (config.readEntry("AlarmType", (int)0))
    {
        case 1:  mAlarmType = CalEvent::ACTIVE;  break;
        case 2:  mAlarmType = CalEvent::ARCHIVED;  break;
        case 4:  mAlarmType = CalEvent::TEMPLATE;  break;
        default:
            kError() << "Invalid alarm type for resource";
            return;
    }
    mName     = config.readEntry("ResourceName", QString());
    mColour   = config.readEntry("Color", QColor());
    mReadOnly = config.readEntry("ResourceIsReadOnly", true);
    mEnabled  = config.readEntry("ResourceIsActive", false);
    mStandard = config.readEntry("Standard", false);
    kDebug() << "Migrating:" << mName << ", type=" << mAlarmType << ", path=" << mPath;
}

/******************************************************************************
* Constructor to create a new default local file resource.
* This is created as enabled, read-write, and standard for its alarm type.
*/
CalendarCreator::CalendarCreator(CalEvent::Type alarmType, const QString& file, const QString& name)
    : mAlarmType(alarmType),
      mResourceType(LocalFile),
      mName(name),
      mColour(),
      mReadOnly(false),
      mEnabled(true),
      mStandard(true),
      mNew(true),
      mFinished(false)
{
    mPath = KStandardDirs::locateLocal("appdata", file);
    kDebug() << "New:" << mName << ", type=" << mAlarmType << ", path=" << mPath;
}

/******************************************************************************
* Create the Akonadi agent for this calendar.
*/
void CalendarCreator::createAgent(const QString& agentType, QObject* parent)
{
    emit creating(mPath);
    AgentInstanceCreateJob* job = new AgentInstanceCreateJob(agentType, parent);
    connect(job, SIGNAL(result(KJob*)), SLOT(agentCreated(KJob*)));
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
        kError() << "AgentInstanceCreateJob error:" << mErrorMessage;
        finish(false);
        return;
    }

    // Configure the Akonadi Agent
    kDebug() << mName;
    AgentInstanceCreateJob* job = static_cast<AgentInstanceCreateJob*>(j);
    mAgent = job->instance();
    mAgent.setName(mName);
    bool ok = false;
    switch (mResourceType)
    {
        case LocalFile:
            ok = migrateLocalFile();
            break;
        case LocalDir:
            ok = migrateLocalDirectory();
            break;
        case RemoteFile:
            ok = migrateRemoteFile();
            break;
        default:
            kError() << "Invalid resource type";
            break;
    }
    if (!ok)
    {
        finish(true);
        return;
    }
    mAgent.reconfigure();   // notify the agent that its configuration has been changed

    // Wait for the resource to create its collection.
    ResourceSynchronizationJob* sjob = new ResourceSynchronizationJob(mAgent);
    connect(sjob, SIGNAL(result(KJob*)), SLOT(resourceSynchronised(KJob*)));
    sjob->start();   // this is required (not an Akonadi::Job)
}

/******************************************************************************
* Called when a resource synchronisation job has completed.
* Fetches the collection which this agent manages.
*/
void CalendarCreator::resourceSynchronised(KJob* j)
{
    kDebug() << mName;
    if (j->error())
    {
        // Don't give up on error - we can still try to fetch the collection
        kError() << "ResourceSynchronizationJob error: " << j->errorString();
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
    connect(job, SIGNAL(result(KJob*)), SLOT(collectionFetchResult(KJob*)));
    job->start();
}

bool CalendarCreator::migrateLocalFile()
{
    OrgKdeAkonadiKAlarmSettingsInterface* iface = migrateBasic<OrgKdeAkonadiKAlarmSettingsInterface>();
    if (!iface)
        return false;
    iface->setMonitorFile(true);
    iface->writeConfig();   // save the Agent config changes
    delete iface;
    return true;
}

bool CalendarCreator::migrateLocalDirectory()
{
    OrgKdeAkonadiKAlarmDirSettingsInterface* iface = migrateBasic<OrgKdeAkonadiKAlarmDirSettingsInterface>();
    if (!iface)
        return false;
    iface->setMonitorFiles(true);
    iface->writeConfig();   // save the Agent config changes
    delete iface;
    return true;
}

bool CalendarCreator::migrateRemoteFile()
{
    OrgKdeAkonadiKAlarmSettingsInterface* iface = migrateBasic<OrgKdeAkonadiKAlarmSettingsInterface>();
    if (!iface)
        return false;
    iface->setMonitorFile(true);
    iface->writeConfig();   // save the Agent config changes
    delete iface;
    return true;
}

template <class Interface> Interface* CalendarCreator::migrateBasic()
{
    Interface* iface = CalendarMigrator::getAgentInterface<Interface>(mAgent, mErrorMessage, this);
    if (iface)
    {
        iface->setReadOnly(mReadOnly);
        iface->setDisplayName(mName);
        iface->setPath(mPath);
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
    kDebug() << mName;
    if (j->error())
    {
        mErrorMessage = j->errorString();
        kError() << "CollectionFetchJob error: " << mErrorMessage;
        finish(true);
        return;
    }
    CollectionFetchJob* job = static_cast<CollectionFetchJob*>(j);
    const Collection::List collections = job->collections();
    if (collections.isEmpty())
    {
        if (++mCollectionFetchRetryCount >= 10)
        {
            mErrorMessage = i18nc("@info/plain", "New configuration timed out");
            kError() << "Timeout fetching collection for resource";
            finish(true);
            return;
        }
        // Need to wait a bit longer until the resource has initialised and
        // created its collection. Retry after 200ms.
        kDebug() << "Retrying";
        QTimer::singleShot(200, this, SLOT(fetchCollection()));
        return;
    }
    if (collections.count() > 1)
    {
        mErrorMessage = i18nc("@info/plain", "New configuration was corrupt");
        kError() << "Wrong number of collections for this resource:" << collections.count();
        finish(true);
        return;
    }

    // Set Akonadi Collection attributes
    Collection collection = collections[0];
    collection.setContentMimeTypes(CalEvent::mimeTypes(mAlarmType));
    EntityDisplayAttribute* dattr = collection.attribute<EntityDisplayAttribute>(Collection::AddIfMissing);
    dattr->setIconName(QLatin1String("kalarm"));
    CollectionAttribute* attr = collection.attribute<CollectionAttribute>(Entity::AddIfMissing);
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
    CollectionAttribute* att = c.attribute<CollectionAttribute>(Entity::AddIfMissing);
    *att = *attr;
    CollectionModifyJob* cmjob = new CollectionModifyJob(c, this);
    connect(cmjob, SIGNAL(result(KJob*)), this, SLOT(modifyCollectionJobDone(KJob*)));
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
        kError() << "CollectionFetchJob error: " << mErrorMessage;
        finish(true);
    }
    else
    {
        kDebug() << "Completed:" << mName;
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
        emit finished(this);
    }
}

#include "calendarmigrator.moc"

// vim: et sw=4:
