/*
 *  akonadiresource.cpp  -  class for an Akonadi alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2019 David Jarvie <djarvie@kde.org>
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

#include "akonadiresource.h"

#include "resources.h"
#include "akonadidatamodel.h"
#include "akonadiresourcemigrator.h"
#include "lib/autoqpointer.h"
#include "kalarm_debug.h"

#include <KAlarmCal/Akonadi>
#include <KAlarmCal/CompatibilityAttribute>
#include <KAlarmCal/EventAttribute>
#include <KAlarmCal/Version>

#include <AkonadiCore/AgentManager>
#include <AkonadiCore/ChangeRecorder>
#include <AkonadiCore/CollectionFetchJob>
#include <AkonadiCore/CollectionModifyJob>
#include <AkonadiCore/ItemCreateJob>
#include <AkonadiCore/ItemDeleteJob>
#include <AkonadiCore/ItemModifyJob>
#include <AkonadiWidgets/AgentConfigurationDialog>
using namespace Akonadi;

#include <KLocalizedString>

#include <QFileInfo>

namespace
{
const QString KALARM_RESOURCE(QStringLiteral("akonadi_kalarm_resource"));
const QString KALARM_DIR_RESOURCE(QStringLiteral("akonadi_kalarm_dir_resource"));

const Collection::Rights WritableRights = Collection::CanChangeItem | Collection::CanCreateItem | Collection::CanDeleteItem;

const QRegularExpression MatchMimeType(QStringLiteral("^application/x-vnd\\.kde\\.alarm.*"),
                                       QRegularExpression::DotMatchesEverythingOption);
}

// Class to provide an object for removeDuplicateResources() signals to be received.
class DuplicateResourceObject : public QObject
{
    Q_OBJECT
public:
    DuplicateResourceObject(QObject* parent = nullptr) : QObject(parent) {}
    void reset()  { mAgentPaths.clear(); }
public Q_SLOTS:
    void collectionFetchResult(KJob*);
private:
    struct ResourceCol
    {
        QString    resourceId;    // Akonadi resource identifier
        ResourceId collectionId;  // Akonadi collection ID
        ResourceCol() {}
        ResourceCol(const QString& r, ResourceId c)
            : resourceId(r), collectionId(c) {}
    };
    QHash<QString, ResourceCol> mAgentPaths;   // path, (resource identifier, collection ID) pairs
};
DuplicateResourceObject* AkonadiResource::mDuplicateResourceObject{nullptr};


Resource AkonadiResource::create(const Akonadi::Collection& collection)
{
    if (collection.id() < 0  ||  collection.remoteId().isEmpty())
        return Resource::null();    // return invalid Resource
    Resource resource = Resources::resource(collection.id());
    if (!resource.isValid())
    {
        // A resource with this ID doesn't exist, so create a new resource.
        addResource(new AkonadiResource(collection), resource);
    }
    return resource;
}

AkonadiResource::AkonadiResource(const Collection& collection)
    : ResourceType(collection.id())
    , mCollection(collection)
    , mValid(collection.id() >= 0  &&  !collection.remoteId().isEmpty())
{
    if (mValid)
    {
        // Fetch collection data, including remote ID, resource and mime types and
        // current attributes.
        fetchCollectionAttribute(false);
        // If the collection doesn't belong to a resource, it can't be used.
        mValid = AgentManager::self()->instance(mCollection.resource()).isValid();

        connect(AkonadiDataModel::monitor(), &Monitor::collectionRemoved, this, &AkonadiResource::slotCollectionRemoved);
    }
}

AkonadiResource::~AkonadiResource()
{
}

Resource AkonadiResource::nullResource()
{
    static Resource nullRes(new AkonadiResource(Collection()));
    return nullRes;
}

bool AkonadiResource::isValid() const
{
    // The collection ID must not have changed since construction.
    return mValid  &&  id() >= 0  &&  mCollection.id() == id();
}

Akonadi::Collection AkonadiResource::collection() const
{
    return mCollection;
}

ResourceType::StorageType AkonadiResource::storageType() const
{
    const QString id = AgentManager::self()->instance(mCollection.resource()).type().identifier();
    if (id == KALARM_RESOURCE)
        return File;
    if (id == KALARM_DIR_RESOURCE)
        return Directory;
    return NoStorage;
}

QString AkonadiResource::storageTypeString(bool description) const
{
    const AgentType agentType = AgentManager::self()->instance(mCollection.resource()).type();
    if (!agentType.isValid())
        return QString();
    if (description)
        return agentType.name();
    bool local = true;
    bool dir = false;
    if (agentType.identifier() == KALARM_DIR_RESOURCE)
        dir = true;
    else
        local = location().isLocalFile();
    return storageTypeStr(false, !dir, local);
}

QUrl AkonadiResource::location() const
{
    return QUrl::fromUserInput(mCollection.remoteId(), QString(), QUrl::AssumeLocalFile);
}

QString AkonadiResource::displayLocation() const
{
    // Don't simply use remoteId() since that may contain "file://" prefix, and percent encoding.
    return location().toDisplayString(QUrl::PrettyDecoded | QUrl::PreferLocalFile);
}

QString AkonadiResource::displayName() const
{
    return mCollection.displayName();
}

QString AkonadiResource::configName() const
{
    return mCollection.resource();
}

CalEvent::Types AkonadiResource::alarmTypes() const
{
    if (!mValid)
        return CalEvent::EMPTY;
    return CalEvent::types(mCollection.contentMimeTypes());
}

CalEvent::Types AkonadiResource::enabledTypes() const
{
    if (!mValid)
        return CalEvent::EMPTY;
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    return mCollectionAttribute.enabled();
}

void AkonadiResource::setEnabled(CalEvent::Type type, bool enabled)
{
    const CalEvent::Types types = enabledTypes();
    const CalEvent::Types newTypes = enabled ? types | type : types & ~type;
    if (newTypes != types)
        setEnabled(newTypes);
}

void AkonadiResource::setEnabled(CalEvent::Types types)
{
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    const bool newAttr = !mCollection.hasAttribute<CollectionAttribute>();
    if (mHaveCollectionAttribute  &&  mCollectionAttribute.enabled() == types)
        return;   // no change
    qCDebug(KALARM_LOG) << "AkonadiResource:" << mCollection.id() << "Set enabled:" << types << " was=" << mCollectionAttribute.enabled();
    mCollectionAttribute.setEnabled(types);
    mHaveCollectionAttribute = true;
    if (newAttr)
    {
        // Akonadi often doesn't notify changes to the enabled status
        // (surely a bug?), so ensure that the change is noticed.
        mNewEnabled = true;
    }
    modifyCollectionAttribute();
}

bool AkonadiResource::readOnly() const
{
    AkonadiDataModel::instance()->refresh(mCollection);    // update with latest data
    return (mCollection.rights() & WritableRights) != WritableRights;
}

int AkonadiResource::writableStatus(CalEvent::Type type) const
{
    if (!mValid)
        return -1;
    AkonadiDataModel::instance()->refresh(mCollection);    // update with latest data
    if ((type == CalEvent::EMPTY  && !enabledTypes())
    ||  (type != CalEvent::EMPTY  && !isEnabled(type)))
        return -1;
    if ((mCollection.rights() & WritableRights) != WritableRights)
        return -1;
    if (!mCollection.hasAttribute<CompatibilityAttribute>())
        return -1;
    switch (mCollection.attribute<CompatibilityAttribute>()->compatibility())
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

bool AkonadiResource::keepFormat() const
{
    if (!mValid)
        return false;
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    return mCollectionAttribute.keepFormat();
}

void AkonadiResource::setKeepFormat(bool keep)
{
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    if (mHaveCollectionAttribute  &&  mCollectionAttribute.keepFormat() == keep)
        return;   // no change
    mCollectionAttribute.setKeepFormat(keep);
    mHaveCollectionAttribute = true;
    modifyCollectionAttribute();
}

QColor AkonadiResource::backgroundColour() const
{
    if (!mValid)
        return QColor();
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    return mCollectionAttribute.backgroundColor();
}

void AkonadiResource::setBackgroundColour(const QColor& colour)
{
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    if (mHaveCollectionAttribute  &&  mCollectionAttribute.backgroundColor() == colour)
        return;   // no change
    mCollectionAttribute.setBackgroundColor(colour);
    mHaveCollectionAttribute = true;
    modifyCollectionAttribute();
}

bool AkonadiResource::configIsStandard(CalEvent::Type type) const
{
    if (!mValid)
        return false;
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    return mCollectionAttribute.isStandard(type);
}

CalEvent::Types AkonadiResource::configStandardTypes() const
{
    if (!mValid)
        return CalEvent::EMPTY;
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    return mCollectionAttribute.standard();
}

void AkonadiResource::configSetStandard(CalEvent::Type type, bool standard)
{
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    if (mHaveCollectionAttribute  &&  mCollectionAttribute.isStandard(type) == standard)
        return;   // no change
    mCollectionAttribute.setStandard(type, standard);
    mHaveCollectionAttribute = true;
    modifyCollectionAttribute();
}

void AkonadiResource::configSetStandard(CalEvent::Types types)
{
    if (!mHaveCollectionAttribute)
        fetchCollectionAttribute(true);
    if (mHaveCollectionAttribute  &&  mCollectionAttribute.standard() == types)
        return;   // no change
    mCollectionAttribute.setStandard(types);
    mHaveCollectionAttribute = true;
    modifyCollectionAttribute();
}

KACalendar::Compat AkonadiResource::compatibilityVersion(QString& versionString) const
{
    versionString.clear();
    if (!mValid)
        return KACalendar::Incompatible;
    AkonadiDataModel::instance()->refresh(mCollection);    // update with latest data
    if (!mCollection.hasAttribute<CompatibilityAttribute>())
        return KACalendar::Incompatible;
    const CompatibilityAttribute* attr = mCollection.attribute<CompatibilityAttribute>();
    versionString = KAlarmCal::getVersionString(attr->version());
    return attr->compatibility();
}

/******************************************************************************
* Update the resource to the current KAlarm storage format.
*/
bool AkonadiResource::updateStorageFormat()
{
//TODO: implement updateStorageFormat(): see AkonadiResourceMigrator::updateStorageFormat()
    return false;
}

/******************************************************************************
* Edit the resource's configuration.
*/
void AkonadiResource::editResource(QWidget* dialogParent)
{
    if (isValid())
    {
        AgentInstance instance = AgentManager::self()->instance(configName());
        if (instance.isValid())
        {
            // Use AutoQPointer to guard against crash on application exit while
            // the event loop is still running. It prevents double deletion (both
            // on deletion of parent, and on return from this function).
            AutoQPointer<AgentConfigurationDialog> dlg = new AgentConfigurationDialog(instance, dialogParent);
            dlg->exec();
        }
    }
}

/******************************************************************************
* Remove the resource. The calendar file is not removed.
*  @return true if the resource has been removed or a removal job has been scheduled.
*  @note The instance will be invalid once it has been removed.
*/
bool AkonadiResource::removeResource()
{
    if (!isValid())
        return false;
    qCDebug(KALARM_LOG) << "AkonadiResource::removeResource:" << id();
    notifyDeletion();
    // Note: Don't use CollectionDeleteJob, since that also deletes the backend storage.
    AgentManager* agentManager = AgentManager::self();
    const AgentInstance instance = agentManager->instance(configName());
    if (instance.isValid())
        agentManager->removeInstance(instance);
        // The instance will be removed from Resources by slotCollectionRemoved().
    return true;
}

/******************************************************************************
* Called when a monitored collection is removed.
* If it's this resource, invalidate the resource and remove it from Resources.
*/
void AkonadiResource::slotCollectionRemoved(const Collection& collection)
{
    if (collection.id() == id())
    {
        qCDebug(KALARM_LOG) << "AkonadiResource::slotCollectionRemoved:" << id();
        disconnect(AkonadiDataModel::monitor(), nullptr, this, nullptr);
        ResourceType::removeResource(collection.id());
    }
}

bool AkonadiResource::load(bool readThroughCache)
{
    Q_UNUSED(readThroughCache);
    AgentManager::self()->instance(mCollection.resource()).synchronize();
    return true;
}

bool AkonadiResource::isPopulated() const
{
    if (!ResourceType::isPopulated())
    {
        const QModelIndex ix = AkonadiDataModel::instance()->resourceIndex(mCollection.id());
        if (!ix.data(AkonadiDataModel::IsPopulatedRole).toBool())
            return false;
        setLoaded(true);
    }
    return true;
}

bool AkonadiResource::save(bool writeThroughCache)
{
    Q_UNUSED(writeThroughCache);
    AgentManager::self()->instance(mCollection.resource()).synchronize();
    return true;
}

#if 0
        /** Reload the resource. Any cached data is first discarded. */
        bool reload() override;
#endif

/******************************************************************************
* Add an event to the resource, and add it to Akonadi.
*/
bool AkonadiResource::addEvent(const KAEvent& event)
{
    qCDebug(KALARM_LOG) << "AkonadiResource::addEvent: ID:" << event.id();
    Item item;
    if (!KAlarmCal::setItemPayload(item, event, mCollection.contentMimeTypes()))
    {
        qCWarning(KALARM_LOG) << "AkonadiResource::addEvent: Invalid mime type for collection";
        return false;
    }
    ItemCreateJob* job = new ItemCreateJob(item, mCollection);
    connect(job, &ItemCreateJob::result, this, &AkonadiResource::itemJobDone);
    mPendingItemJobs[job] = -1;   // the Item doesn't have an ID yet
    job->start();
    return true;
}

/******************************************************************************
* Update an event in the resource, and update it in Akonadi.
* Its UID must be unchanged.
*/
bool AkonadiResource::updateEvent(const KAEvent& event)
{
    qCDebug(KALARM_LOG) << "AkonadiResource::updateEvent:" << event.id();
    Item item = AkonadiDataModel::instance()->itemForEvent(event.id());
    if (!item.isValid())
        return false;
    if (!KAlarmCal::setItemPayload(item, event, mCollection.contentMimeTypes()))
    {
        qCWarning(KALARM_LOG) << "AkonadiResource::updateEvent: Invalid mime type for collection";
        return false;
    }
    queueItemModifyJob(item);
    return true;
}

/******************************************************************************
* Delete an event from the resource, and from Akonadi.
*/
bool AkonadiResource::deleteEvent(const KAEvent& event)
{
    qCDebug(KALARM_LOG) << "AkonadiResource::deleteEvent:" << event.id();
    if (isBeingDeleted())
    {
        qCDebug(KALARM_LOG) << "AkonadiResource::deleteEvent: Collection being deleted";
        return true;    // the event's collection is being deleted
    }
    const Item item = AkonadiDataModel::instance()->itemForEvent(event.id());
    if (!item.isValid())
        return false;
    ItemDeleteJob* job = new ItemDeleteJob(item);
    connect(job, &ItemDeleteJob::result, this, &AkonadiResource::itemJobDone);
    mPendingItemJobs[job] = item.id();
    job->start();
    return true;
}

/******************************************************************************
* Save a command error change to Akonadi.
*/
void AkonadiResource::handleCommandErrorChange(const KAEvent& event)
{
    Item item = AkonadiDataModel::instance()->itemForEvent(event.id());
    if (item.isValid())
    {
        const KAEvent::CmdErrType err = event.commandError();
        switch (err)
        {
            case KAEvent::CMD_NO_ERROR:
                if (!item.hasAttribute<EventAttribute>())
                    return;   // no change
                Q_FALLTHROUGH();
            case KAEvent::CMD_ERROR:
            case KAEvent::CMD_ERROR_PRE:
            case KAEvent::CMD_ERROR_POST:
            case KAEvent::CMD_ERROR_PRE_POST:
            {
                EventAttribute* attr = item.attribute<EventAttribute>(Item::AddIfMissing);
                if (attr->commandError() == err)
                    return;   // no change
                attr->setCommandError(err);
                queueItemModifyJob(item);
                return;
            }
            default:
                break;
        }
    }
}

/******************************************************************************
* Return a reference to the Collection held by a resource.
*/
Collection& AkonadiResource::collection(Resource& res)
{
    static Collection nullCollection;
    AkonadiResource* akres = resource<AkonadiResource>(res);
    return akres ? akres->mCollection : nullCollection;
}
const Collection& AkonadiResource::collection(const Resource& res)
{
    static const Collection nullCollection;
    const AkonadiResource* akres = resource<AkonadiResource>(res);
    return akres ? akres->mCollection : nullCollection;
}

/******************************************************************************
* Return the event for an Akonadi Item.
*/
KAEvent AkonadiResource::event(Resource& resource, const Akonadi::Item& item)
{
    if (!item.isValid()  ||  !item.hasPayload<KAEvent>())
        return KAEvent();
    KAEvent ev = item.payload<KAEvent>();
    if (ev.isValid())
    {
        if (item.hasAttribute<EventAttribute>())
            ev.setCommandError(item.attribute<EventAttribute>()->commandError());
        // Set collection ID using a const method, to avoid unnecessary copying of KAEvent
        ev.setCollectionId_const(resource.id());
    }
    return ev;
}

/******************************************************************************
* Check for, and remove, any Akonadi resources which duplicate use of calendar
* files/directories.
*/
void AkonadiResource::removeDuplicateResources()
{
    if (!mDuplicateResourceObject)
        mDuplicateResourceObject = new DuplicateResourceObject(Resources::instance());
    mDuplicateResourceObject->reset();
    const AgentInstance::List agents = AgentManager::self()->instances();
    for (const AgentInstance& agent : agents)
    {
        if (agent.type().mimeTypes().indexOf(MatchMimeType) >= 0)
        {
            CollectionFetchJob* job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::Recursive);
            job->fetchScope().setResource(agent.identifier());
            connect(job, &CollectionFetchJob::result, mDuplicateResourceObject, &DuplicateResourceObject::collectionFetchResult);
        }
    }
}

/******************************************************************************
* Called when a removeDuplicateResources() CollectionFetchJob has completed.
*/
void DuplicateResourceObject::collectionFetchResult(KJob* j)
{
    CollectionFetchJob* job = qobject_cast<CollectionFetchJob*>(j);
    if (j->error())
        qCCritical(KALARM_LOG) << "AkonadiResource::collectionFetchResult: CollectionFetchJob" << job->fetchScope().resource()<< "error: " << j->errorString();
    else
    {
        AgentManager* agentManager = AgentManager::self();
        const Collection::List collections = job->collections();
        for (const Collection& c : collections)
        {
            if (c.contentMimeTypes().indexOf(MatchMimeType) >= 0)
            {
                ResourceCol thisRes(job->fetchScope().resource(), c.id());
                auto it = mAgentPaths.constFind(c.remoteId());
                if (it != mAgentPaths.constEnd())
                {
                    // Remove the resource containing the higher numbered Collection
                    // ID, which is likely to be the more recently created.
                    const ResourceCol prevRes = it.value();
                    if (thisRes.collectionId > prevRes.collectionId)
                    {
                        qCWarning(KALARM_LOG) << "AkonadiResource::collectionFetchResult: Removing duplicate resource" << thisRes.resourceId;
                        agentManager->removeInstance(agentManager->instance(thisRes.resourceId));
                        continue;
                    }
                    qCWarning(KALARM_LOG) << "AkonadiResource::collectionFetchResult: Removing duplicate resource" << prevRes.resourceId;
                    agentManager->removeInstance(agentManager->instance(prevRes.resourceId));
                }
                mAgentPaths[c.remoteId()] = thisRes;
            }
        }
    }
}

/******************************************************************************
* Called when a collection has been populated.
* Stores all its events, even if their alarm types are currently disabled.
* Emits a signal if all collections have been populated.
*/
void AkonadiResource::notifyCollectionLoaded(ResourceId id, const QList<KAEvent>& events)
{
    if (id >= 0)
    {
        Resource res = Resources::resource(id);
        AkonadiResource* akres = resource<AkonadiResource>(res);
        if (akres)
        {
            const CalEvent::Types types = akres->alarmTypes();
            QHash<QString, KAEvent> eventHash;
            for (const KAEvent& event : events)
                if (event.category() & types)
                    eventHash[event.id()] = event;
            akres->setLoadedEvents(eventHash);
        }
    }
}

/******************************************************************************
* Called when the collection's properties or content have changed.
* Updates this resource's copy of the collection, and emits a signal if
* properties of interest have changed.
*/
void AkonadiResource::notifyCollectionChanged(Resource& res, const Collection& collection, bool checkCompatibility)
{
    if (collection.id() != res.id())
        return;
    AkonadiResource* akres = resource<AkonadiResource>(res);
    if (!akres)
        return;

    Changes change = NoChange;

    // Check for a read/write permission change
    const Collection::Rights oldRights = akres->mCollection.rights() & WritableRights;
    const Collection::Rights newRights = collection.rights() & WritableRights;
    if (newRights != oldRights)
    {
        qCDebug(KALARM_LOG) << "AkonadiResource::setCollectionChanged:" << collection.id() << ": rights ->" << newRights;
        change |= ReadOnly;
    }

    // Check for a change in content mime types
    // (e.g. when a collection is first created at startup).
    if (collection.contentMimeTypes() != akres->mCollection.contentMimeTypes())
    {
        qCDebug(KALARM_LOG) << "AkonadiResource::setCollectionChanged:" << collection.id() << ": alarm types ->" << collection.contentMimeTypes();
        change |= AlarmTypes;
    }

    // Check for the collection being enabled/disabled.
    // Enabled/disabled can only be set by KAlarm (not the resource), so if the
    // attribute doesn't exist, it is ignored.
    const CalEvent::Types oldEnabled = akres->mLastEnabled;
    const CalEvent::Types newEnabled = collection.hasAttribute<CollectionAttribute>()
                                     ? collection.attribute<CollectionAttribute>()->enabled() : CalEvent::EMPTY;
    if (!akres->mCollectionAttrChecked  ||  newEnabled != oldEnabled)
    {
        qCDebug(KALARM_LOG) << "AkonadiResource::setCollectionChanged:" << collection.id() << ": enabled ->" << newEnabled;
        akres->mCollectionAttrChecked = true;
        change |= Enabled;
    }
    akres->mLastEnabled = newEnabled;

    akres->mCollection = collection;
    if (change != NoChange)
        Resources::notifySettingsChanged(akres, change, oldEnabled);

    if (!resource<AkonadiResource>(res))
        return;   // this resource has been deleted

    // Check for the backend calendar format changing.
    bool hadCompat = akres->mHaveCompatibilityAttribute;
    akres->mHaveCompatibilityAttribute = collection.hasAttribute<CompatibilityAttribute>();
    if (akres->mHaveCompatibilityAttribute)
    {
        // The attribute must exist in order to know the calendar format.
        if (checkCompatibility
        ||  !hadCompat
        ||  *collection.attribute<CompatibilityAttribute>() != *akres->mCollection.attribute<CompatibilityAttribute>())
        {
//TODO: check if a temporary AkonadiResource object is actually needed, as in the comment
            // Update to current KAlarm format if necessary, and if the user agrees.
            // Create a new temporary 'Resource' object, because the one passed
            // to this method can get overwritten with an old version of its
            // CompatibilityAttribute before AkonadiResourceMigration finishes,
            // due to AkonadiDataModel still containing an out of date value.
            qCDebug(KALARM_LOG) << "AkonadiResource::setCollectionChanged:" << collection.id() << ": compatibility ->" << collection.attribute<CompatibilityAttribute>()->compatibility();
            // Note that the AkonadiResource will be deleted once no more
            // QSharedPointers reference it.
            AkonadiResourceMigrator::updateToCurrentFormat(res, false, akres);
        }
    }
}

/******************************************************************************
* Called to notify that an event has been added or updated in Akonadi.
*/
void AkonadiResource::notifyEventsChanged(Resource& res, const QList<KAEvent>& events)
{
    AkonadiResource* akres = resource<AkonadiResource>(res);
    if (akres)
        akres->setUpdatedEvents(events);
}

/******************************************************************************
* Called when an Item has been changed or created in Akonadi.
*/
void AkonadiResource::notifyItemChanged(Resource& res, const Akonadi::Item& item, bool created)
{
    AkonadiResource* akres = resource<AkonadiResource>(res);
    if (akres)
    {
        int i = akres->mItemsBeingCreated.removeAll(item.id());   // the new item has now been initialised
        if (!created  ||  i)
            akres->checkQueuedItemModifyJob(item);    // execute the next job queued for the item
    }
}

/******************************************************************************
* Called to notify that an event is about to be deleted from Akonadi.
*/
void AkonadiResource::notifyEventsToBeDeleted(Resource& res, const QList<KAEvent>& events)
{
    AkonadiResource* akres = resource<AkonadiResource>(res);
    if (akres)
        akres->setDeletedEvents(events);
}

/******************************************************************************
* Queue an ItemModifyJob for execution. Ensure that only one job is
* simultaneously active for any one Item.
*
* This is necessary because we can't call two ItemModifyJobs for the same Item
* at the same time; otherwise Akonadi will detect a conflict and require manual
* intervention to resolve it.
*/
void AkonadiResource::queueItemModifyJob(const Item& item)
{
    qCDebug(KALARM_LOG) << "AkonadiResource::queueItemModifyJob:" << item.id();
    QHash<Item::Id, Item>::Iterator it = mItemModifyJobQueue.find(item.id());
    if (it != mItemModifyJobQueue.end())
    {
        // A job is already queued for this item. Replace the queued item value with the new one.
        qCDebug(KALARM_LOG) << "AkonadiResource::queueItemModifyJob: Replacing previously queued job";
        it.value() = item;
    }
    else
    {
        // There is no job already queued for this item
        if (mItemsBeingCreated.contains(item.id()))
        {
            qCDebug(KALARM_LOG) << "AkonadiResource::queueItemModifyJob: Waiting for item initialisation";
            mItemModifyJobQueue[item.id()] = item;   // wait for item initialisation to complete
        }
        else
        {
            Item newItem = item;
            Item current = item;
            if (AkonadiDataModel::instance()->refresh(current))  // fetch the up-to-date item
                newItem.setRevision(current.revision());
            mItemModifyJobQueue[item.id()] = Item();   // mark the queued item as now executing
            ItemModifyJob* job = new ItemModifyJob(newItem);
            job->disableRevisionCheck();
            connect(job, &ItemModifyJob::result, this, &AkonadiResource::itemJobDone);
            mPendingItemJobs[job] = item.id();
            qCDebug(KALARM_LOG) << "AkonadiResource::queueItemModifyJob: Executing Modify job for item" << item.id() << ", revision=" << newItem.revision();
        }
    }
}

/******************************************************************************
* Called when an item job has completed.
* Checks for any error.
* Note that for an ItemModifyJob, the item revision number may not be updated
* to the post-modification value. The next queued ItemModifyJob is therefore
* not kicked off from here, but instead from the slot attached to the
* itemChanged() signal, which has the revision updated.
*/
void AkonadiResource::itemJobDone(KJob* j)
{
    const QHash<KJob*, Item::Id>::iterator it = mPendingItemJobs.find(j);
    Item::Id itemId = -1;
    if (it != mPendingItemJobs.end())
    {
        itemId = it.value();
        mPendingItemJobs.erase(it);
    }
    const QByteArray jobClass = j->metaObject()->className();
    qCDebug(KALARM_LOG) << "AkonadiResource::itemJobDone:" << jobClass;
    if (j->error())
    {
        QString errMsg;
        if (jobClass == "Akonadi::ItemCreateJob")
            errMsg = i18nc("@info", "Failed to create alarm.");
        else if (jobClass == "Akonadi::ItemModifyJob")
            errMsg = i18nc("@info", "Failed to update alarm.");
        else if (jobClass == "Akonadi::ItemDeleteJob")
            errMsg = i18nc("@info", "Failed to delete alarm.");
        else
            Q_ASSERT(0);
        qCCritical(KALARM_LOG) << "AkonadiResource::itemJobDone:" << errMsg << itemId << ":" << j->errorString();

        if (itemId >= 0  &&  jobClass == "Akonadi::ItemModifyJob")
        {
            // Execute the next queued job for this item
            const Item current = AkonadiDataModel::instance()->itemById(itemId);  // fetch the up-to-date item
            checkQueuedItemModifyJob(current);
        }
        Resources::notifyResourceMessage(this, MessageType::Error, errMsg, j->errorString());
    }
    else
    {
        if (jobClass == "Akonadi::ItemCreateJob")
        {
            // Prevent modification of the item until it is fully initialised.
            // Either slotMonitoredItemChanged() or slotRowsInserted(), or both,
            // will be called when the item is done.
            itemId = static_cast<ItemCreateJob*>(j)->item().id();
            qCDebug(KALARM_LOG) << "AkonadiResource::itemJobDone(ItemCreateJob): item id=" << itemId;
            mItemsBeingCreated << itemId;
        }
    }

/*    if (itemId >= 0  &&  jobClass == "Akonadi::ItemModifyJob")
    {
        const QHash<Item::Id, Item>::iterator it = mItemModifyJobQueue.find(itemId);
        if (it != mItemModifyJobQueue.end())
        {
            if (!it.value().isValid())
                mItemModifyJobQueue.erase(it);   // there are no more jobs queued for the item
        }
    }*/
}

/******************************************************************************
* Check whether there are any ItemModifyJobs waiting for a specified item, and
* if so execute the first one provided its creation has completed. This
* prevents clashes in Akonadi conflicts between simultaneous ItemModifyJobs for
* the same item.
*
* Note that when an item is newly created (e.g. via addEvent()), the KAlarm
* resource itemAdded() function creates an ItemModifyJob to give it a remote
* ID. Until that job is complete, any other ItemModifyJob for the item will
* cause a conflict.
*/
void AkonadiResource::checkQueuedItemModifyJob(const Item& item)
{
    if (mItemsBeingCreated.contains(item.id()))
        return;    // the item hasn't been fully initialised yet
    const QHash<Item::Id, Item>::iterator it = mItemModifyJobQueue.find(item.id());
    if (it == mItemModifyJobQueue.end())
        return;    // there are no jobs queued for the item
    Item qitem = it.value();
    if (!qitem.isValid())
    {
        // There is no further job queued for the item, so remove the item from the list
        mItemModifyJobQueue.erase(it);
    }
    else
    {
        // Queue the next job for the Item, after updating the Item's
        // revision number to match that set by the job just completed.
        qitem.setRevision(item.revision());
        mItemModifyJobQueue[item.id()] = Item();   // mark the queued item as now executing
        ItemModifyJob* job = new ItemModifyJob(qitem);
        job->disableRevisionCheck();
        connect(job, &ItemModifyJob::result, this, &AkonadiResource::itemJobDone);
        mPendingItemJobs[job] = qitem.id();
        qCDebug(KALARM_LOG) << "Executing queued Modify job for item" << qitem.id() << ", revision=" << qitem.revision();
    }
}

/******************************************************************************
* Update the stored CollectionAttribute value from the Akonadi database.
*/
void AkonadiResource::fetchCollectionAttribute(bool refresh) const
{
    if (refresh)
        AkonadiDataModel::instance()->refresh(mCollection);    // update with latest data
    if (!mCollection.hasAttribute<CollectionAttribute>())
    {
        mCollectionAttribute = CollectionAttribute();
        mHaveCollectionAttribute = false;
    }
    else
    {
        mCollectionAttribute = *mCollection.attribute<CollectionAttribute>();
        mHaveCollectionAttribute = true;
    }
}

/******************************************************************************
* Update the CollectionAttribute value in the Akonadi database.
*/
void AkonadiResource::modifyCollectionAttribute()
{
    // Note that we can't supply 'mCollection' to CollectionModifyJob since that
    // also contains the CompatibilityAttribute value, which is read-only for
    // applications. So create a new Collection instance and only set a value
    // for CollectionAttribute.
    Collection c(mCollection.id());
    CollectionAttribute* att = c.attribute<CollectionAttribute>(Collection::AddIfMissing);
    *att = mCollectionAttribute;
    CollectionModifyJob* job = new CollectionModifyJob(c, this);
    connect(job, &CollectionModifyJob::result, this, &AkonadiResource::modifyCollectionAttrJobDone);
}

/******************************************************************************
* Called when a CollectionAttribute modification job has completed.
* Checks for any error.
*/
void AkonadiResource::modifyCollectionAttrJobDone(KJob* j)
{
    Collection collection = static_cast<CollectionModifyJob*>(j)->collection();
    const Collection::Id id = collection.id();
    const bool newEnabled = mNewEnabled;
    mNewEnabled = false;
    if (j->error())
    {
        // If the collection is being/has been deleted, ignore the error.
        if (!isBeingDeleted()
        &&  AkonadiDataModel::instance()->resource(id).isValid()
        &&  id == mCollection.id())
        {
            qCCritical(KALARM_LOG) << "AkonadiResource::modifyCollectionAttrJobDone:" << collection.id() << "Failed to update calendar" << displayName() << ":" << j->errorString();
            Resources::notifyResourceMessage(this, MessageType::Error, i18nc("@info", "Failed to update calendar \"%1\".", displayName()), j->errorString());
        }
    }
    else
    {
        AkonadiDataModel::instance()->refresh(mCollection);   // pick up the modified attribute
        if (newEnabled)
        {
            const CalEvent::Types oldEnabled = mLastEnabled;
            mLastEnabled = collection.hasAttribute<CollectionAttribute>()
                         ? collection.attribute<CollectionAttribute>()->enabled() : CalEvent::EMPTY;
            Resources::notifySettingsChanged(this, Enabled, oldEnabled);
        }
    }
}

#include "akonadiresource.moc"

// vim: et sw=4:
