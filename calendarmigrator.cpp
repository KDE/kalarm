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
#include "kalarmsettings.h"
#include "kalarmdirsettings.h"
#include "collectionattribute.h"

#include <akonadi/agentinstancecreatejob.h>
#include <akonadi/agentmanager.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/collectionmodifyjob.h>
#include <akonadi/entitydisplayattribute.h>

#include <klocale.h>
#include <kconfiggroup.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kdebug.h>

using namespace Akonadi;


// Creates or migrates a single alarm calendar
class CalendarCreator : public QObject
{
        Q_OBJECT
    public:
        CalendarCreator(const QString& resourceType, const KConfigGroup&);
        CalendarCreator(KAlarm::CalEvent::Type, const QString& path, const QString& name);
        bool    isValid() const        { return mAlarmType != KAlarm::CalEvent::EMPTY; }
        bool    newCalendar() const    { return mNew; }
        QString resourceName() const   { return mName; }
        QString path() const           { return mPath; }
        QString errorMessage() const   { return mErrorMessage; }

    public slots:
        void agentCreated(KJob*);

    signals:
        void finished(CalendarCreator*);

    private slots:
        void collectionsReceived(const Akonadi::Collection::List&);
        void collectionFetchResult(KJob*);
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
        bool                   mReadOnly;
        bool                   mEnabled;
        bool                   mStandard;
        const bool             mNew;
        bool                   mFinished;
};


CalendarMigrator::CalendarMigrator(QObject* parent)
    : QObject(parent)
{
}

/******************************************************************************
* Migrate old KResource calendars, or if none, create default Akonadi resources.
*/
void CalendarMigrator::execute()
{
    CalendarMigrator* instance = new CalendarMigrator;
    instance->migrateOrCreate();
}

/******************************************************************************
* Migrate old KResource calendars, or if none, create default Akonadi resources.
*/
void CalendarMigrator::migrateOrCreate()
{
    kDebug();
    CalendarCreator* creator;
    AgentInstanceCreateJob* job;

    // First migrate any KResources alarm calendars from pre-Akonadi versions of KAlarm.
    bool haveResources = false;
    const QString configFile = KStandardDirs::locateLocal("config", QLatin1String("kresources/alarms/stdrc"));
    KConfig config(configFile, KConfig::SimpleConfig);
    QStringList groupNames = config.groupList();
    foreach (const QString& groupName, groupNames)
    {
        if (!groupName.startsWith(QLatin1String("Resource_")))
            continue;
        KConfigGroup configGroup(&config, groupName);
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
#if 0
    KRES::Manager<KRES::Resource> manager(QLatin1String("alarms"));
    manager.readConfig();
    for (KRES::Manager<KRES::Resource>::Iterator it = manager.begin();  it != manager.end();  ++it)
    {
        KRES::Resource* resource = *it;
        QString agentType;
        if (resource->type() == QLatin1String("file"))
            agentType = QLatin1String("akonadi_kalarm_resource");
        else if (resource->type() == QLatin1String("dir"))
            agentType = QLatin1String("akonadi_kalarm_dir_resource");
        else if (resource->type() == QLatin1String("remote"))
            agentType = QLatin1String("akonadi_kalarm_resource");
        else
            continue;   // unknown resource type - can't convert

        haveResources = true;
        KConfigGroup configGroup(&config, "Resource_" + resource->identifier());
#endif
        creator = new CalendarCreator(resourceType, configGroup);
        if (!creator->isValid())
            delete creator;
        else
        {
            connect(creator, SIGNAL(finished(CalendarCreator*)), SLOT(calendarCreated(CalendarCreator*)));
            mCalendarsPending << creator;
            job = new AgentInstanceCreateJob(agentType, this);
            connect(job, SIGNAL(result(KJob*)), creator, SLOT(agentCreated(KJob*)));
            job->start();
        }
    }

    if (!haveResources)
    {
        // There were no calendars to migrate, so create default ones.
        // Normally this occurs on first installation of KAlarm.
        creator = new CalendarCreator(KAlarm::CalEvent::ACTIVE, QLatin1String("calendar.ics"), i18nc("@info/plain", "Active Alarms"));
        connect(creator, SIGNAL(finished(CalendarCreator*)), SLOT(calendarCreated(CalendarCreator*)));
        mCalendarsPending << creator;
        job = new AgentInstanceCreateJob(QLatin1String("akonadi_kalarm_resource"), this);
        connect(job, SIGNAL(result(KJob*)), creator, SLOT(agentCreated(KJob*)));
        job->start();

        creator = new CalendarCreator(KAlarm::CalEvent::ARCHIVED, QLatin1String("expired.ics"), i18nc("@info/plain", "Archived Alarms"));
        connect(creator, SIGNAL(finished(CalendarCreator*)), SLOT(calendarCreated(CalendarCreator*)));
        mCalendarsPending << creator;
        job = new AgentInstanceCreateJob(QLatin1String("akonadi_kalarm_resource"), this);
        connect(job, SIGNAL(result(KJob*)), creator, SLOT(agentCreated(KJob*)));
        job->start();

        creator = new CalendarCreator(KAlarm::CalEvent::TEMPLATE, QLatin1String("template.ics"), i18nc("@info/plain", "Alarm Templates"));
        connect(creator, SIGNAL(finished(CalendarCreator*)), SLOT(calendarCreated(CalendarCreator*)));
        mCalendarsPending << creator;
        job = new AgentInstanceCreateJob(QLatin1String("akonadi_kalarm_resource"), this);
        connect(job, SIGNAL(result(KJob*)), creator, SLOT(agentCreated(KJob*)));
        job->start();
    }

    if (mCalendarsPending.isEmpty())
        deleteLater();
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
    mCalendarsPending.removeAt(i);    // remove it from the pending list

    if (!creator->errorMessage().isEmpty())
    {
        QString errmsg = creator->newCalendar()
                       ? i18nc("@info/plain", "Failed to create default calendar <resource>%1</resource>", creator->resourceName())
                       : i18nc("@info/plain", "Failed to convert old configuration for calendar <resource>%1</resource>", creator->resourceName());
        QString locn = i18nc("@info/plain File path or URL", "Location: %1", creator->path());
        KMessageBox::error(0, i18nc("@info", "%1<nl/>%2<nl/>(%3)", errmsg, locn, creator->errorMessage()));
    }
    creator->deleteLater();

    if (mCalendarsPending.isEmpty())
        deleteLater();
}


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
CalendarCreator::CalendarCreator(KAlarm::CalEvent::Type alarmType, const QString& path, const QString& name)
    : mAlarmType(alarmType),
      mResourceType(LocalFile),
      mPath(path),
      mName(name),
      mColour(),
      mReadOnly(false),
      mEnabled(true),
      mStandard(true),
      mNew(true),
      mFinished(false)
{
    kDebug() << "New:" << mName << ", type=" << mAlarmType << ", path=" << mPath;
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

    // Find the collection which this agent manages
    CollectionFetchJob* fjob = new CollectionFetchJob(Collection::root(), CollectionFetchJob::FirstLevel);
    fjob->fetchScope().setResource(mAgent.identifier());
    connect(fjob, SIGNAL(collectionsReceived(const Akonadi::Collection::List&)),
                  SLOT(collectionsReceived(const Akonadi::Collection::List&)));
    connect(fjob, SIGNAL(result(KJob*)), SLOT(collectionFetchResult(KJob*)));
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
    iface->setAlarmTypes(KAlarm::CalEvent::mimeTypes(mAlarmType));
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
    Interface* iface = new Interface("org.freedesktop.Akonadi.Resource." + mAgent.identifier(),
              "/Settings", QDBusConnection::sessionBus(), this);
    if (!iface->isValid())
    {
        mErrorMessage = iface->lastError().message();
        delete iface;
        return 0;
    }
    iface->setReadOnly(mReadOnly);
    iface->setDisplayName(mName);
    iface->setPath(mPath);
    return iface;
}

/******************************************************************************
* Called when a collection fetch job has retrieved the agent's collection.
* Obtains the collection handled by the agent, and configures it.
*/
void CalendarCreator::collectionsReceived(const Akonadi::Collection::List& collections)
{
    if (collections.count() != 1)
    {
        mErrorMessage = i18nc("@info/plain", "New configuration was corrupt");
        kError() << "Wrong number of collections for this resource:" << collections.count();
        finish(true);
    }
    else
    {
        // Set Akonadi Collection attributes
        kDebug() << mName;
        Collection collection = collections[0];
        collection.setRemoteId(mPath);
        collection.setContentMimeTypes(KAlarm::CalEvent::mimeTypes(mAlarmType));
        EntityDisplayAttribute* dattr = collection.attribute<EntityDisplayAttribute>(Collection::AddIfMissing);
        dattr->setIconName("kalarm");
        KAlarm::CollectionAttribute* attr = collection.attribute<KAlarm::CollectionAttribute>(Entity::AddIfMissing);
        attr->setEnabled(mEnabled ? mAlarmType : KAlarm::CalEvent::EMPTY);
        if (mStandard)
            attr->setStandard(mAlarmType);
        if (mColour.isValid())
            attr->setBackgroundColor(mColour);
        CollectionModifyJob* job = new CollectionModifyJob(collection, this);
        connect(job, SIGNAL(result(KJob*)), this, SLOT(modifyCollectionJobDone(KJob*)));
    }
}

/******************************************************************************
* Called when a collection fetch job has completed.
* Checks for error.
*/
void CalendarCreator::collectionFetchResult(KJob* j)
{
    if (j->error())
    {
        mErrorMessage = j->errorString();
        kError() << "CollectionFetchJob error: " << mErrorMessage;
        finish(true);
    }
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
