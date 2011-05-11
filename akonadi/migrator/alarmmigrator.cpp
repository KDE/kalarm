/*
 *  alarmmigrator.cpp  -  migrates KAlarm resources to Akonadi
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

#include "alarmmigrator.h"
#include "kalarmsettings.h"
#include "kalarmdirsettings.h"
#include "collectionattribute.h"

#include <kresources/manager.h>
#include <kresources/resource.h>
#include <akonadi/agentinstancecreatejob.h>
#include <akonadi/agentmanager.h>
#include <akonadi/collectionfetchjob.h>
#include <akonadi/collectionfetchscope.h>
#include <akonadi/collectionmodifyjob.h>
#include <akonadi/entitydisplayattribute.h>

#include <kstandarddirs.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kmessagebox.h>

using namespace KRES;
using namespace Akonadi;


void AlarmMigrator::migrate()
{
    ++mCalendarsPending;   // prevent premature exit
    const QString configFile = KStandardDirs::locateLocal("config", QLatin1String("kresources/alarms/stdrc"));
    KConfig config(configFile);
    KRES::Manager<Resource> manager(QLatin1String("alarms"));
    manager.readConfig();
    for (KRES::Manager<Resource>::Iterator it = manager.begin();  it != manager.end();  ++it)
    {
        Resource* resource = *it;
        QString agentType;
        if (resource->type() == QLatin1String("file"))
            agentType = QLatin1String("akonadi_kalarm_resource");
        else if (resource->type() == QLatin1String("dir"))
            agentType = QLatin1String("akonadi_kalarm_dir_resource");
        else if (resource->type() == QLatin1String("remote"))
            agentType = QLatin1String("akonadi_kalarm_resource");
        else
            continue;   // unknown resource type - can't convert

        KConfigGroup* configGroup = new KConfigGroup(&config, "Resource_" + resource->identifier());
        CalendarMigrator* migrator = new CalendarMigrator(resource, configGroup);
        ++mCalendarsPending;
        AgentInstanceCreateJob* job = new AgentInstanceCreateJob(agentType, this);
        connect(job, SIGNAL(result(KJob*)), migrator, SLOT(agentCreated(KJob*)));
        job->start();
    }
    if (--mCalendarsPending == 0)
        exit(mExitCode);
}

void AlarmMigrator::calendarDone(CalendarMigrator* migrator)
{
    if (!migrator->errorMessage().isEmpty())
    {
        mExitCode = 1;
        QString errmsg = i18nc("@info/plain", "Failure to convert old configuration for calendar <resource>%1</resource>", migrator->resourceName());
        QString locn = i18nc("@info/plain File path or URL", "Location: %1", migrator->path());
        KMessageBox::error(0, i18nc("@info", "%1<nl/>%2<nl/>(%3)", errmsg, locn, migrator->errorMessage()));
    }
    migrator->deleteLater();
    if (--mCalendarsPending == 0)
        exit(mExitCode);
}


CalendarMigrator::CalendarMigrator(KRES::Resource* r, const KConfigGroup* c)
    : mResource(r),
      mConfig(c),
      mAlarmType(KAlarm::CalEvent::EMPTY),
      mFinished(false)
{
    if (mResource->type() == QLatin1String("file"))
    {
        mResourceType = LocalFile;
        mPath = mConfig->readPathEntry("CalendarURL", "");
    }
    else if (mResource->type() == QLatin1String("dir"))
    {
        mResourceType = LocalDir;
        mPath = mConfig->readPathEntry("CalendarURL", "");
    }
    else if (mResource->type() == QLatin1String("remote"))
    {
        mResourceType = RemoteFile;
        mPath = mConfig->readPathEntry("DownloadUrl", "");
    }
}

CalendarMigrator::~CalendarMigrator()
{
    delete mConfig;
    mConfig = 0;
}

QString CalendarMigrator::resourceName() const
{
    return mResource->resourceName();
}

/******************************************************************************
* Called when the agent creation job for this resource has completed.
* Applies the calendar resource configuration to the Akonadi agent.
*/
void CalendarMigrator::agentCreated(KJob* j)
{
    AgentInstanceCreateJob* job = static_cast<AgentInstanceCreateJob*>(j);
    if (j->error())
    {
        mErrorMessage = j->errorString();
        kError() << "AgentInstanceCreateJob error:" << mErrorMessage;
        finish(false);
        return;
    }

    // Configure the Akonadi Agent
    mAgent = job->instance();
    mAgent.setName(mResource->resourceName());
    switch (mConfig->readEntry("AlarmType", (int)0))
    {
        case 1:  mAlarmType = KAlarm::CalEvent::ACTIVE;  break;
        case 2:  mAlarmType = KAlarm::CalEvent::ARCHIVED;  break;
        case 4:  mAlarmType = KAlarm::CalEvent::TEMPLATE;  break;
        default:
            kError() << "Invalid alarm type for resource";
            finish(true);
            return;
    }
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
            kError() << "Invalid resource type:" << mResource->type();
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

bool CalendarMigrator::migrateLocalFile()
{
    OrgKdeAkonadiKAlarmSettingsInterface* iface = migrateBasic<OrgKdeAkonadiKAlarmSettingsInterface>();
    if (!iface)
        return false;
    iface->setMonitorFile(true);
    iface->writeConfig();   // save the Agent config changes
    delete iface;
    return true;
}

bool CalendarMigrator::migrateLocalDirectory()
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

bool CalendarMigrator::migrateRemoteFile()
{
    OrgKdeAkonadiKAlarmSettingsInterface* iface = migrateBasic<OrgKdeAkonadiKAlarmSettingsInterface>();
    if (!iface)
        return false;
    iface->setMonitorFile(true);
    iface->writeConfig();   // save the Agent config changes
    delete iface;
    return true;
}

template <class Interface> Interface* CalendarMigrator::migrateBasic()
{
    Interface* iface = new Interface("org.freedesktop.Akonadi.Resource." + mAgent.identifier(),
              "/Settings", QDBusConnection::sessionBus(), this);
    if (!iface->isValid())
    {
        mErrorMessage = iface->lastError().message();
        delete iface;
        return 0;
    }
    iface->setReadOnly(mConfig->readEntry("ResourceIsReadOnly", true));
    iface->setDisplayName(mConfig->readEntry("ResourceName", QString()));
    iface->setPath(mPath);
    return iface;
}

/******************************************************************************
* Called when a collection fetch job has retrieved the agent's collection.
* Obtains the collection handled by the agent, and configures it.
*/
void CalendarMigrator::collectionsReceived(const Akonadi::Collection::List& collections)
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
        Collection collection = collections[0];
        collection.setRemoteId(mPath);
        collection.setContentMimeTypes(KAlarm::CalEvent::mimeTypes(mAlarmType));
        EntityDisplayAttribute* dattr = collection.attribute<EntityDisplayAttribute>(Collection::AddIfMissing);
        dattr->setIconName("kalarm");
        KAlarm::CollectionAttribute* attr = collection.attribute<KAlarm::CollectionAttribute>(Entity::AddIfMissing);
        bool enabled = mConfig->readEntry("ResourceIsActive", false);
        attr->setEnabled(enabled ? mAlarmType : KAlarm::CalEvent::EMPTY);
        if (mConfig->readEntry("Standard", false))
            attr->setStandard(mAlarmType);
        QColor bgColour = mConfig->readEntry("Color", QColor());
        if (bgColour.isValid())
            attr->setBackgroundColor(bgColour);
        CollectionModifyJob* job = new CollectionModifyJob(collection, this);
        connect(job, SIGNAL(result(KJob*)), this, SLOT(modifyCollectionJobDone(KJob*)));
    }
}

/******************************************************************************
* Called when a collection fetch job has completed.
* Checks for error.
*/
void CalendarMigrator::collectionFetchResult(KJob* j)
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
void CalendarMigrator::modifyCollectionJobDone(KJob* j)
{
    Collection collection = static_cast<CollectionModifyJob*>(j)->collection();
    if (j->error())
    {
        mErrorMessage = j->errorString();
        kError() << "CollectionFetchJob error: " << mErrorMessage;
        finish(true);
    }
    else
        finish(false);
}

/******************************************************************************
* Emit the finished() signal. If 'cleanup' is true, delete the newly created
* but incomplete Agent.
*/
void CalendarMigrator::finish(bool cleanup)
{
    if (!mFinished)
    {
        if (cleanup)
            AgentManager::self()->removeInstance(mAgent);
        mFinished = true;
        emit finished(this);
    }
}

// vim: et sw=4:
