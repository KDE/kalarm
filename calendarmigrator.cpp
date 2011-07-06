/*
 *  calendarmigrator.cpp  -  migrates or creates KAlarm Akonadi resources
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
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
#include "kalarmsettings.h"
#include "kalarmdirsettings.h"
#include "collectionattribute.h"
#include "compatibilityattribute.h"
#include "version.h"

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
#include <kmessagebox.h>
#include <kdebug.h>

#include <QTimer>

using namespace Akonadi;
using KAlarm::CollectionAttribute;
using KAlarm::CompatibilityAttribute;


// Creates, or migrates from KResources, a single alarm calendar
class CalendarCreator : public QObject
{
        Q_OBJECT
    public:
        CalendarCreator(const QString& resourceType, const KConfigGroup&);
        CalendarCreator(KAlarm::CalEvent::Type, const QString& file, const QString& name);
        bool    isValid() const        { return mAlarmType != KAlarm::CalEvent::EMPTY; }
        bool    newCalendar() const    { return mNew; }
        QString resourceName() const   { return mName; }
        QString path() const           { return mPath; }
        QString errorMessage() const   { return mErrorMessage; }
        void    createAgent(const QString& agentType, QObject* parent);

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
        KAlarm::CalEvent::Type mAlarmType;
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

    public slots:
        bool update();

    private:
        Akonadi::Collection mCollection;
        QObject*            mParent;
        bool                mDirResource;
        bool                mIgnoreKeepFormat;
        bool                mNewCollection;
};


CalendarMigrator* CalendarMigrator::mInstance = 0;

CalendarMigrator::CalendarMigrator(QObject* parent)
    : QObject(parent)
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
* Migrate old KResource calendars, or if none, create default Akonadi resources.
*/
void CalendarMigrator::migrateOrCreate()
{
    kDebug();
    CalendarCreator* creator;

    // First migrate any KResources alarm calendars from pre-Akonadi versions of KAlarm.
    bool haveResources = false;
    const QString configFile = KStandardDirs::locateLocal("config", QLatin1String("kresources/alarms/stdrc"));
    KConfig config(configFile, KConfig::SimpleConfig);

    // Fetch all the resource identifiers which are actually in use
    KConfigGroup group = config.group("General");
    QStringList keys = group.readEntry("ResourceKeys", QStringList())
                     + group.readEntry("PassiveResourceKeys", QStringList());

    // Create an Akonadi resource for each resource id
    foreach (const QString& id, keys)
    {
        KConfigGroup configGroup = config.group(QLatin1String("Resource_") + id);
        QString resourceType = configGroup.readEntry("ResourceType", QString());
        QString agentType;
        if (resourceType == QLatin1String("file"))
            agentType = QLatin1String("akonadi_kalarm_resource");
        else if (resourceType == QLatin1String("dir"))
            agentType = QLatin1String("akonadi_kalarm_dir_resource");
        else if (resourceType == QLatin1String("remote"))
            agentType = QLatin1String("akonadi_kalarm_resource");
        else
            continue;   // unknown resource type - can't convert

        haveResources = true;
        creator = new CalendarCreator(resourceType, configGroup);
        if (!creator->isValid())
            delete creator;
        else
        {
            connect(creator, SIGNAL(finished(CalendarCreator*)), SLOT(calendarCreated(CalendarCreator*)));
            connect(creator, SIGNAL(creating(const QString&)), SLOT(creatingCalendar(CalendarCreator*)));
            mCalendarsPending << creator;
            creator->createAgent(agentType, this);
        }
    }

    if (!haveResources)
    {
#ifdef __GNUC__
#warning Check if this uses existing default calendar files even if not found in KResources
#endif
        // There were no KResources calendars to migrate, so create default ones.
        // Normally this occurs on first installation of KAlarm.
        creator = new CalendarCreator(KAlarm::CalEvent::ACTIVE, QLatin1String("calendar.ics"), i18nc("@info/plain", "Active Alarms"));
        connect(creator, SIGNAL(finished(CalendarCreator*)), SLOT(calendarCreated(CalendarCreator*)));
        connect(creator, SIGNAL(creating(const QString&)), SLOT(creatingCalendar(CalendarCreator*)));
        mCalendarsPending << creator;
        creator->createAgent(QLatin1String("akonadi_kalarm_resource"), this);

        creator = new CalendarCreator(KAlarm::CalEvent::ARCHIVED, QLatin1String("expired.ics"), i18nc("@info/plain", "Archived Alarms"));
        connect(creator, SIGNAL(finished(CalendarCreator*)), SLOT(calendarCreated(CalendarCreator*)));
        connect(creator, SIGNAL(creating(const QString&)), SLOT(creatingCalendar(CalendarCreator*)));
        mCalendarsPending << creator;
        creator->createAgent(QLatin1String("akonadi_kalarm_resource"), this);

        creator = new CalendarCreator(KAlarm::CalEvent::TEMPLATE, QLatin1String("template.ics"), i18nc("@info/plain", "Alarm Templates"));
        connect(creator, SIGNAL(finished(CalendarCreator*)), SLOT(calendarCreated(CalendarCreator*)));
        connect(creator, SIGNAL(creating(const QString&)), SLOT(creatingCalendar(CalendarCreator*)));
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
void CalendarMigrator::creatingCalendar(CalendarCreator* creator)
{
    emit creating(creator->path(), false);
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
        QString locn = i18nc("@info/plain File path or URL", "Location: %1", creator->path());
        if (creator->errorMessage().isEmpty())
            errmsg = i18nc("@info", "<para>%1</para><para>%2</para>", errmsg, locn);
        else
            errmsg = i18nc("@info", "<para>%1</para><para>%2<nl/>(%3)</para>", errmsg, locn, creator->errorMessage());
        KMessageBox::error(0, errmsg);
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
void CalendarMigrator::updateToCurrentFormat(const Collection& collection, bool ignoreKeepFormat, QObject* parent)
{
    kDebug() << collection.id();
    AgentInstance agent = AgentManager::self()->instance(collection.resource());
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


CalendarUpdater::CalendarUpdater(const Collection& collection, bool dirResource,
                                 bool ignoreKeepFormat, bool newCollection, QObject* parent)
    : mCollection(collection),
      mParent(parent),
      mDirResource(dirResource),
      mIgnoreKeepFormat(ignoreKeepFormat),
      mNewCollection(newCollection)
{
}

bool CalendarUpdater::update()
{
    kDebug() << mCollection.id() << (mDirResource ? "directory" : "file");
    bool result = true;
    if (mCollection.hasAttribute<CompatibilityAttribute>())
    {
        const CompatibilityAttribute* compatAttr = mCollection.attribute<CompatibilityAttribute>();
        KAlarm::Calendar::Compat compatibility = compatAttr->compatibility();
        if ((compatibility & ~KAlarm::Calendar::Converted)
        // The calendar isn't in the current KAlarm format
        &&  !(compatibility & ~(KAlarm::Calendar::Convertible | KAlarm::Calendar::Converted))
        // The calendar format is convertible to the current KAlarm format
        &&  (mIgnoreKeepFormat
            || !mCollection.hasAttribute<CollectionAttribute>()
            || !mCollection.attribute<CollectionAttribute>()->keepFormat()))
        {
            // The user hasn't previously said not to convert it
            QString versionString = KAlarm::getVersionString(compatAttr->version());
            QString msg = KAlarm::Calendar::conversionPrompt(mCollection.name(), versionString, false);
            if (KMessageBox::warningYesNo(0, msg) == KMessageBox::Yes)
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
                    AgentInstance agent = AgentManager::self()->instance(mCollection.resource());
                    if (mDirResource)
                        CalendarMigrator::updateStorageFormat<OrgKdeAkonadiKAlarmDirSettingsInterface>(agent, errmsg, mParent);
                    else
                        CalendarMigrator::updateStorageFormat<OrgKdeAkonadiKAlarmSettingsInterface>(agent, errmsg, mParent);
                }
                if (!errmsg.isEmpty())
                {
                    KMessageBox::error(0, i18nc("@info", "%1<nl/>(%2)",
                                                i18nc("@info/plain", "Failed to update format of calendar <resource>%1</resource>", mCollection.name()),
                                                errmsg));
                }
            }
            else
            {
                // The user chose not to update the calendar
                result = false;
                if (!mNewCollection)
                {
                    QModelIndex ix = AkonadiModel::instance()->collectionIndex(mCollection);
                    AkonadiModel::instance()->setData(ix, true, AkonadiModel::KeepFormatRole);
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
    Interface* iface = new Interface("org.freedesktop.Akonadi.Resource." + agent.identifier(),
              "/Settings", QDBusConnection::sessionBus(), parent);
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
    : mAlarmType(KAlarm::CalEvent::EMPTY),
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
    mPath = config.readPathEntry(pathKey, "");
    switch (config.readEntry("AlarmType", (int)0))
    {
        case 1:  mAlarmType = KAlarm::CalEvent::ACTIVE;  break;
        case 2:  mAlarmType = KAlarm::CalEvent::ARCHIVED;  break;
        case 4:  mAlarmType = KAlarm::CalEvent::TEMPLATE;  break;
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
CalendarCreator::CalendarCreator(KAlarm::CalEvent::Type alarmType, const QString& file, const QString& name)
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
        iface->setAlarmTypes(KAlarm::CalEvent::mimeTypes(mAlarmType));
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
    Collection::List collections = job->collections();
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
    collection.setContentMimeTypes(KAlarm::CalEvent::mimeTypes(mAlarmType));
    EntityDisplayAttribute* dattr = collection.attribute<EntityDisplayAttribute>(Collection::AddIfMissing);
    dattr->setIconName("kalarm");
    CollectionAttribute* attr = collection.attribute<CollectionAttribute>(Entity::AddIfMissing);
    attr->setEnabled(mEnabled ? mAlarmType : KAlarm::CalEvent::EMPTY);
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
    CalendarUpdater* updater = new CalendarUpdater(collection, dirResource, false, true, this);
    if (!updater->update())   // note that 'updater' will auto-delete when finished
    {
        // Record that the user chose not to update the calendar
        attr->setKeepFormat(true);
    }

    // Update the collection's attributes in the Akonadi database
    CollectionModifyJob* cmjob = new CollectionModifyJob(collection, this);
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
#include "moc_calendarmigrator.cpp"

// vim: et sw=4:
