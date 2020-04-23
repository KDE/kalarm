/*
 *  akonadidatamodel.cpp  -  KAlarm calendar file access using Akonadi
 *  Program:  kalarm
 *  Copyright © 2007-2020 David Jarvie <djarvie@kde.org>
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

#include "akonadidatamodel.h"

#include "preferences.h"
#include "resources/akonadiresourcemigrator.h"
#include "resources/resources.h"
#include "lib/synchtimer.h"
#include "kalarm_debug.h"

#include <KAlarmCal/AlarmText>
#include <KAlarmCal/CompatibilityAttribute>
#include <KAlarmCal/EventAttribute>

#include <AkonadiCore/AgentFilterProxyModel>
#include <AkonadiCore/AgentInstanceCreateJob>
#include <AkonadiCore/AgentManager>
#include <AkonadiCore/AgentType>
#include <AkonadiCore/AttributeFactory>
#include <AkonadiCore/ChangeRecorder>
#include <AkonadiCore/CollectionDeleteJob>
#include <AkonadiCore/CollectionFetchJob>
#include <AkonadiCore/EntityDisplayAttribute>
#include <AkonadiCore/Item>
#include <AkonadiCore/ItemFetchScope>
#include <AkonadiWidgets/AgentTypeDialog>

#include <KLocalizedString>
#include <KColorUtils>

#include <QUrl>
#include <QApplication>
#include <QFileInfo>
#include <QTimer>

using namespace Akonadi;
using namespace KAlarmCal;

// Ensure ResourceDataModelBase::UserRole is valid. ResourceDataModelBase does
// not include Akonadi headers, so here we check that it has been set to be
// compatible with EntityTreeModel::UserRole.
static_assert((int)ResourceDataModelBase::UserRole>=(int)Akonadi::EntityTreeModel::UserRole, "ResourceDataModelBase::UserRole wrong value");

/*=============================================================================
= Class: AkonadiDataModel
=============================================================================*/

AkonadiDataModel* AkonadiDataModel::mInstance = nullptr;
int               AkonadiDataModel::mTimeHourPos = -2;

/******************************************************************************
* Construct and return the singleton.
*/
AkonadiDataModel* AkonadiDataModel::instance()
{
    if (!mInstance)
        mInstance = new AkonadiDataModel(new ChangeRecorder(qApp), qApp);
    return mInstance;
}

/******************************************************************************
* Constructor.
*/
AkonadiDataModel::AkonadiDataModel(ChangeRecorder* monitor, QObject* parent)
    : EntityTreeModel(monitor, parent)
    , ResourceDataModelBase()
    , mMonitor(monitor)
{
    // Populate all collections, selected/enabled or unselected/disabled.
    setItemPopulationStrategy(ImmediatePopulation);

    // Restrict monitoring to collections containing the KAlarm mime types
    monitor->setCollectionMonitored(Collection::root());
    monitor->setResourceMonitored("akonadi_kalarm_resource");
    monitor->setResourceMonitored("akonadi_kalarm_dir_resource");
    monitor->setMimeTypeMonitored(KAlarmCal::MIME_ACTIVE);
    monitor->setMimeTypeMonitored(KAlarmCal::MIME_ARCHIVED);
    monitor->setMimeTypeMonitored(KAlarmCal::MIME_TEMPLATE);
    monitor->itemFetchScope().fetchFullPayload();
    monitor->itemFetchScope().fetchAttribute<EventAttribute>();

    AttributeFactory::registerAttribute<CollectionAttribute>();
    AttributeFactory::registerAttribute<CompatibilityAttribute>();
    AttributeFactory::registerAttribute<EventAttribute>();

    connect(monitor, SIGNAL(collectionChanged(Akonadi::Collection,QSet<QByteArray>)), SLOT(slotCollectionChanged(Akonadi::Collection,QSet<QByteArray>)));
    connect(monitor, &Monitor::collectionRemoved, this, &AkonadiDataModel::slotCollectionRemoved);
    initResourceMigrator();
    MinuteTimer::connect(this, SLOT(slotUpdateTimeTo()));
    Preferences::connect(SIGNAL(archivedColourChanged(QColor)), this, SLOT(slotUpdateArchivedColour(QColor)));
    Preferences::connect(SIGNAL(disabledColourChanged(QColor)), this, SLOT(slotUpdateDisabledColour(QColor)));
    Preferences::connect(SIGNAL(holidaysChanged(KHolidays::HolidayRegion)), this, SLOT(slotUpdateHolidays()));
    Preferences::connect(SIGNAL(workTimeChanged(QTime,QTime,QBitArray)), this, SLOT(slotUpdateWorkingHours()));

    connect(Resources::instance(), &Resources::resourceMessage, this, &AkonadiDataModel::slotResourceMessage, Qt::QueuedConnection);

    connect(this, &AkonadiDataModel::rowsInserted, this, &AkonadiDataModel::slotRowsInserted);
    connect(this, &AkonadiDataModel::rowsAboutToBeRemoved, this, &AkonadiDataModel::slotRowsAboutToBeRemoved);
    connect(this, &Akonadi::EntityTreeModel::collectionTreeFetched, this, &AkonadiDataModel::slotCollectionTreeFetched);
    connect(this, &Akonadi::EntityTreeModel::collectionPopulated, this, &AkonadiDataModel::slotCollectionPopulated);
    connect(monitor, &Monitor::itemChanged, this, &AkonadiDataModel::slotMonitoredItemChanged);

    connect(ServerManager::self(), &ServerManager::stateChanged, this, &AkonadiDataModel::checkResources);
    checkResources(ServerManager::state());
}


/******************************************************************************
* Destructor.
*/
AkonadiDataModel::~AkonadiDataModel()
{
    if (mInstance == this)
        mInstance = nullptr;
}

/******************************************************************************
* Called when the server manager changes state.
* If it is now running, i.e. the agent manager knows about
* all existing resources.
* Once it is running, i.e. the agent manager knows about
* all existing resources, if necessary migrate any KResources alarm calendars from
* pre-Akonadi versions of KAlarm, or create default Akonadi calendar resources
* if any are missing.
*/
void AkonadiDataModel::checkResources(ServerManager::State state)
{
    switch (state)
    {
        case ServerManager::Running:
            if (!isMigrating()  &&  !isMigrationComplete())
            {
                qCDebug(KALARM_LOG) << "AkonadiDataModel::checkResources: Server running";
                setMigrationInitiated();
                AkonadiResourceMigrator::execute();
            }
            break;
        case ServerManager::NotRunning:
            qCDebug(KALARM_LOG) << "AkonadiDataModel::checkResources: Server stopped";
            setMigrationInitiated(false);
            initResourceMigrator();
            Q_EMIT serverStopped();
            break;
        default:
            break;
    }
}

/******************************************************************************
* Initialise the calendar migrator so that it can be run (either for the first
* time, or again).
*/
void AkonadiDataModel::initResourceMigrator()
{
    AkonadiResourceMigrator::reset();
    connect(AkonadiResourceMigrator::instance(), &AkonadiResourceMigrator::creating,
                                           this, &AkonadiDataModel::slotCollectionBeingCreated);
    connect(AkonadiResourceMigrator::instance(), &QObject::destroyed,
                                           this, &AkonadiDataModel::slotMigrationCompleted);
}

ChangeRecorder* AkonadiDataModel::monitor()
{
    return instance()->mMonitor;
}

/******************************************************************************
* Return the data for a given role, for a specified item.
*/
QVariant AkonadiDataModel::data(const QModelIndex& index, int role) const
{
    if (role == ResourceIdRole)
        role = CollectionIdRole;   // use the base model for this
    if (roleHandled(role)  ||  role == ParentResourceIdRole)
    {
        const Collection collection = EntityTreeModel::data(index, CollectionRole).value<Collection>();
        if (collection.isValid())
        {
            // This is a Collection row
            // Update the collection's resource with the current collection value.
            const Resource& res = updateResource(collection);
            bool handled;
            const QVariant value = resourceData(role, res, handled);
            if (handled)
                return value;
        }
        else
        {
            Item item = EntityTreeModel::data(index, ItemRole).value<Item>();
            if (item.isValid())
            {
                // This is an Item row
                const QString mime = item.mimeType();
                if ((mime != KAlarmCal::MIME_ACTIVE  &&  mime != KAlarmCal::MIME_ARCHIVED  &&  mime != KAlarmCal::MIME_TEMPLATE)
                ||  !item.hasPayload<KAEvent>())
                    return QVariant();
                Resource res;
                const KAEvent ev(event(item, index, res));   // this sets item.parentCollection()
                if (role == ParentResourceIdRole)
                    return item.parentCollection().id();

                bool handled;
                const QVariant value = eventData(role, index.column(), ev, res, handled);
                if (handled)
                    return value;
            }
        }
    }
    return EntityTreeModel::data(index, role);
}

/******************************************************************************
* Return the number of columns for either a collection or an item.
*/
int AkonadiDataModel::entityColumnCount(HeaderGroup group) const
{
    switch (group)
    {
        case CollectionTreeHeaders:
            return 1;
        case ItemListHeaders:
            return ColumnCount;
        default:
            return EntityTreeModel::entityColumnCount(group);
    }
}

/******************************************************************************
* Return offset to add to headerData() role, for item models.
*/
int AkonadiDataModel::headerDataEventRoleOffset() const
{
    return TerminalUserRole * ItemListHeaders;
}

/******************************************************************************
* Return data for a column heading.
*/
QVariant AkonadiDataModel::entityHeaderData(int section, Qt::Orientation orientation, int role, HeaderGroup group) const
{
    bool eventHeaders = false;
    switch (group)
    {
        case ItemListHeaders:
            eventHeaders = true;
            Q_FALLTHROUGH();    // fall through to CollectionTreeHeaders
        case CollectionTreeHeaders:
        {
            bool handled;
            const QVariant value = ResourceDataModelBase::headerData(section, orientation, role, eventHeaders, handled);
            if (handled)
                return value;
            break;
        }
        default:
            break;
    }
    return EntityTreeModel::entityHeaderData(section, orientation, role, group);
}

/******************************************************************************
* Recursive function to Q_EMIT the dataChanged() signal for all items in a
* specified column range.
*/
void AkonadiDataModel::signalDataChanged(bool (*checkFunc)(const Item&), int startColumn, int endColumn, const QModelIndex& parent)
{
    int start = -1;
    int end   = -1;
    for (int row = 0, count = rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix = index(row, 0, parent);
        const Item item = ix.data(ItemRole).value<Item>();
        const bool isItem = item.isValid();
        if (isItem)
        {
            if ((*checkFunc)(item))
            {
                // For efficiency, Q_EMIT a single signal for each group of
                // consecutive items, rather than a separate signal for each item.
                if (start < 0)
                    start = row;
                end = row;
                continue;
            }
        }
        if (start >= 0)
            Q_EMIT dataChanged(index(start, startColumn, parent), index(end, endColumn, parent));
        start = -1;
        if (!isItem)
            signalDataChanged(checkFunc, startColumn, endColumn, ix);
    }

    if (start >= 0)
        Q_EMIT dataChanged(index(start, startColumn, parent), index(end, endColumn, parent));
}

/******************************************************************************
* Signal every minute that the time-to-alarm values have changed.
*/
static bool checkItem_isActive(const Item& item)
{ return item.mimeType() == KAlarmCal::MIME_ACTIVE; }

void AkonadiDataModel::slotUpdateTimeTo()
{
    signalDataChanged(&checkItem_isActive, TimeToColumn, TimeToColumn, QModelIndex());
}


/******************************************************************************
* Called when the colour used to display archived alarms has changed.
*/
static bool checkItem_isArchived(const Item& item)
{ return item.mimeType() == KAlarmCal::MIME_ARCHIVED; }

void AkonadiDataModel::slotUpdateArchivedColour(const QColor&)
{
    qCDebug(KALARM_LOG) << "AkonadiDataModel::slotUpdateArchivedColour";
    signalDataChanged(&checkItem_isArchived, 0, ColumnCount - 1, QModelIndex());
}

/******************************************************************************
* Called when the colour used to display disabled alarms has changed.
*/
static bool checkItem_isDisabled(const Item& item)
{
    if (item.hasPayload<KAEvent>())
    {
        const KAEvent event = item.payload<KAEvent>();
        if (event.isValid())
            return !event.enabled();
    }
    return false;
}

void AkonadiDataModel::slotUpdateDisabledColour(const QColor&)
{
    qCDebug(KALARM_LOG) << "AkonadiDataModel::slotUpdateDisabledColour";
    signalDataChanged(&checkItem_isDisabled, 0, ColumnCount - 1, QModelIndex());
}

/******************************************************************************
* Called when the definition of holidays has changed.
*/
static bool checkItem_excludesHolidays(const Item& item)
{
    if (item.hasPayload<KAEvent>())
    {
        const KAEvent event = item.payload<KAEvent>();
        if (event.isValid()  &&  event.holidaysExcluded())
            return true;
    }
    return false;
}

void AkonadiDataModel::slotUpdateHolidays()
{
    qCDebug(KALARM_LOG) << "AkonadiDataModel::slotUpdateHolidays";
    Q_ASSERT(TimeToColumn == TimeColumn + 1);  // signal should be emitted only for TimeTo and Time columns
    signalDataChanged(&checkItem_excludesHolidays, TimeColumn, TimeToColumn, QModelIndex());
}

/******************************************************************************
* Called when the definition of working hours has changed.
*/
static bool checkItem_workTimeOnly(const Item& item)
{
    if (item.hasPayload<KAEvent>())
    {
        const KAEvent event = item.payload<KAEvent>();
        if (event.isValid()  &&  event.workTimeOnly())
            return true;
    }
    return false;
}

void AkonadiDataModel::slotUpdateWorkingHours()
{
    qCDebug(KALARM_LOG) << "AkonadiDataModel::slotUpdateWorkingHours";
    Q_ASSERT(TimeToColumn == TimeColumn + 1);  // signal should be emitted only for TimeTo and Time columns
    signalDataChanged(&checkItem_workTimeOnly, TimeColumn, TimeToColumn, QModelIndex());
}

/******************************************************************************
* Reload a collection from Akonadi storage. The backend data is not reloaded.
*/
bool AkonadiDataModel::reload(Resource& resource)
{
    if (!resource.isValid())
        return false;
    qCDebug(KALARM_LOG) << "AkonadiDataModel::reload:" << resource.displayId();
    Collection collection(resource.id());
    mMonitor->setCollectionMonitored(collection, false);
    mMonitor->setCollectionMonitored(collection, true);
    return true;
}

/******************************************************************************
* Reload all collections from Akonadi storage. The backend data is not reloaded.
*/
void AkonadiDataModel::reload()
{
    qCDebug(KALARM_LOG) << "AkonadiDataModel::reload";
    const Collection::List collections = mMonitor->collectionsMonitored();
    for (const Collection& collection : collections)
    {
        mMonitor->setCollectionMonitored(collection, false);
        mMonitor->setCollectionMonitored(collection, true);
    }
}

/******************************************************************************
* Returns the index to a specified event.
*/
QModelIndex AkonadiDataModel::eventIndex(const KAEvent& event) const
{
    return itemIndex(Item(mEventIds.value(event.id()).itemId));
}

/******************************************************************************
* Returns the index to a specified event.
*/
QModelIndex AkonadiDataModel::eventIndex(const QString& eventId) const
{
    return itemIndex(Item(mEventIds.value(eventId).itemId));
}

/******************************************************************************
* Return all events belonging to a collection.
*/
QList<KAEvent> AkonadiDataModel::events(ResourceId id) const
{
    QList<KAEvent> list;
    const QModelIndex ix = modelIndexForCollection(this, Collection(id));
    if (ix.isValid())
        getChildEvents(ix, list);
    for (KAEvent& ev : list)
        ev.setResourceId(id);
    return list;
}

/******************************************************************************
* Recursive function to append all child Events with a given mime type.
*/
void AkonadiDataModel::getChildEvents(const QModelIndex& parent, QList<KAEvent>& events) const
{
    for (int row = 0, count = rowCount(parent);  row < count;  ++row)
    {
        const QModelIndex ix = index(row, 0, parent);
        const Item item = ix.data(ItemRole).value<Item>();
        if (item.isValid())
        {
            if (item.hasPayload<KAEvent>())
            {
                KAEvent event = item.payload<KAEvent>();
                if (event.isValid())
                    events += event;
            }
        }
        else
        {
            const Collection c = ix.data(CollectionRole).value<Collection>();
            if (c.isValid())
                getChildEvents(ix, events);
        }
    }
}

KAEvent AkonadiDataModel::event(const QString& eventId) const
{
    return event(eventIndex(eventId));
}

KAEvent AkonadiDataModel::event(const QModelIndex& ix) const
{
    if (!ix.isValid())
        return KAEvent();
    Item item = ix.data(ItemRole).value<Item>();
    Resource r;
    return event(item, ix, r);
}

/******************************************************************************
* Return the event for an Item at a specified model index.
* The item's parent collection is set, as is the event's collection ID.
*/
KAEvent AkonadiDataModel::event(Akonadi::Item& item, const QModelIndex& ix, Resource& res) const
{
//TODO: Tune performance: This function is called very frequently with the same parameters
    if (ix.isValid())
    {
        const Collection pc = ix.data(ParentCollectionRole).value<Collection>();
        item.setParentCollection(pc);
        res = resource(pc.id());
        if (res.isValid())
        {
            // Fetch the KAEvent defined by the Item, including commandError.
            return AkonadiResource::event(res, item);
        }
    }
    res = Resource::null();
    return KAEvent();
}

/******************************************************************************
* Return the up to date Item for a specified Akonadi ID.
*/
Item AkonadiDataModel::itemById(Item::Id id) const
{
    Item item(id);
    if (!refresh(item))
        return Item();
    return item;
}

/******************************************************************************
* Return the Item for a given event.
*/
Item AkonadiDataModel::itemForEvent(const QString& eventId) const
{
    const QModelIndex ix = eventIndex(eventId);
    if (!ix.isValid())
        return Item();
    return ix.data(ItemRole).value<Item>();
}

#if 0
/******************************************************************************
* Add an event to a specified Collection.
* If the event is scheduled to be added to the collection, it is updated with
* its Akonadi item ID.
* The event's 'updated' flag is cleared.
* Reply = true if item creation has been scheduled.
*/
bool AkonadiDataModel::addEvent(KAEvent& event, Resource& resource)
{
    qCDebug(KALARM_LOG) << "AkonadiDataModel::addEvent: ID:" << event.id();
    if (!resource.addEvent(event))
        return false;
    // Note that the item ID will be inserted in mEventIds after the Akonadi
    // Item has been created by ItemCreateJob, when slotRowsInserted() is called.
    mEventIds[event.id()] = EventIds(resource.id());
    return true;
}
#endif

/******************************************************************************
* Called when rows have been inserted into the model.
*/
void AkonadiDataModel::slotRowsInserted(const QModelIndex& parent, int start, int end)
{
    qCDebug(KALARM_LOG) << "AkonadiDataModel::slotRowsInserted:" << start << "-" << end << "(parent =" << parent << ")";
    QHash<Resource, QList<KAEvent>> events;
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix = index(row, 0, parent);
        const Collection collection = ix.data(CollectionRole).value<Collection>();
        if (collection.isValid())
        {
            // A collection has been inserted. Create a new resource to hold it.
            qCDebug(KALARM_LOG) << "AkonadiDataModel::slotRowsInserted: Collection" << collection.id() << collection.name();
            Resource& resource = updateResource(collection);
            // Ignore it if it isn't owned by a valid Akonadi resource.
            if (resource.isValid())
            {
                setCollectionChanged(resource, collection, true);
                Resources::notifyNewResourceInitialised(resource);

                if (!collection.hasAttribute<CompatibilityAttribute>())
                {
                    // If the compatibility attribute is missing at this point,
                    // it doesn't always get notified later, so fetch the
                    // collection to ensure that we see it.
                    AgentInstance agent = AgentManager::self()->instance(collection.resource());
                    CollectionFetchJob* job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::Recursive);
                    job->fetchScope().setResource(agent.identifier());
                    connect(job, &CollectionFetchJob::result, instance(), &AkonadiDataModel::collectionFetchResult);
                }
            }
        }
        else
        {
            // An item has been inserted
            Item item = ix.data(ItemRole).value<Item>();
            if (item.isValid())
            {
                qCDebug(KALARM_LOG) << "item id=" << item.id() << ", revision=" << item.revision();
                Resource res;
                const KAEvent evnt = event(item, ix, res);   // this sets item.parentCollection()
                if (evnt.isValid())
                {
                    qCDebug(KALARM_LOG) << "AkonadiDataModel::slotRowsInserted: Event" << evnt.id();
                    // Only notify new events if the collection is already populated.
                    // If not populated, all events will be notified when it is
                    // eventually populated.
                    if (res.isPopulated())
                        events[res] += evnt;
                    mEventIds[evnt.id()] = EventIds(item.parentCollection().id(), item.id());
                }

                // Notify the resource containing the item.
                AkonadiResource::notifyItemChanged(res, item, true);
            }
        }
    }

    for (auto it = events.constBegin();  it != events.constEnd();  ++it)
    {
        Resource res = it.key();
        AkonadiResource::notifyEventsChanged(res, it.value());
    }
}

/******************************************************************************
* Called when a CollectionFetchJob has completed.
* Check for and process changes in attribute values.
*/
void AkonadiDataModel::collectionFetchResult(KJob* j)
{
    CollectionFetchJob* job = qobject_cast<CollectionFetchJob*>(j);
    if (j->error())
        qCWarning(KALARM_LOG) << "AkonadiDataModel::collectionFetchResult: CollectionFetchJob" << job->fetchScope().resource()<< "error: " << j->errorString();
    else
    {
        const Collection::List collections = job->collections();
        for (const Collection& c : collections)
        {
            qCDebug(KALARM_LOG) << "AkonadiDataModel::collectionFetchResult:" << c.id();
            auto it = mResources.find(c.id());
            if (it == mResources.end())
                continue;
            Resource& resource = it.value();
            setCollectionChanged(resource, c, false);
        }
    }
}

/******************************************************************************
* Called when rows are about to be removed from the model.
*/
void AkonadiDataModel::slotRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end)
{
    qCDebug(KALARM_LOG) << "AkonadiDataModel::slotRowsAboutToBeRemoved:" << start << "-" << end << "(parent =" << parent << ")";
    QHash<Resource, QList<KAEvent>> events;
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix = index(row, 0, parent);
        Item item = ix.data(ItemRole).value<Item>();
        Resource res;
        const KAEvent evnt = event(item, ix, res);   // this sets item.parentCollection()
        if (evnt.isValid())
        {
            qCDebug(KALARM_LOG) << "AkonadiDataModel::slotRowsAboutToBeRemoved: Collection:" << item.parentCollection().id() << ", Event ID:" << evnt.id();
            events[res] += evnt;
            mEventIds.remove(evnt.id());
        }
    }

    for (auto it = events.constBegin();  it != events.constEnd();  ++it)
    {
        Resource res = it.key();
        AkonadiResource::notifyEventsToBeDeleted(res, it.value());
    }
}

/******************************************************************************
* Called when a monitored collection has changed.
* Updates the collection held by the collection's resource, and notifies
* changes of interest.
*/
void AkonadiDataModel::slotCollectionChanged(const Akonadi::Collection& c, const QSet<QByteArray>& attributeNames)
{
    qCDebug(KALARM_LOG) << "AkonadiDataModel::slotCollectionChanged:" << c.id() << attributeNames;
    auto it = mResources.find(c.id());
    if (it != mResources.end())
    {
        // The Monitor::collectionChanged() signal is not always emitted when
        // attributes are created!  So check whether any attributes not
        // included in 'attributeNames' have been created.
        Resource& resource = it.value();
        setCollectionChanged(resource, c, attributeNames.contains(CompatibilityAttribute::name()));
    }
}

/******************************************************************************
* Called when a monitored collection's properties or content have changed.
* Optionally emits a signal if properties of interest have changed.
*/
void AkonadiDataModel::setCollectionChanged(Resource& resource, const Collection& collection, bool checkCompat)
{
    AkonadiResource::notifyCollectionChanged(resource, collection, checkCompat);
    if (isMigrating())
    {
        mCollectionIdsBeingCreated.removeAll(collection.id());
        if (mCollectionsBeingCreated.isEmpty() && mCollectionIdsBeingCreated.isEmpty()
        &&  AkonadiResourceMigrator::completed())
        {
            qCDebug(KALARM_LOG) << "AkonadiDataModel::setCollectionChanged: Migration completed";
            setMigrationComplete();
        }
    }
}

/******************************************************************************
* Called when a monitored collection is removed.
*/
void AkonadiDataModel::slotCollectionRemoved(const Collection& collection)
{
    const Collection::Id id = collection.id();
    qCDebug(KALARM_LOG) << "AkonadiDataModel::slotCollectionRemoved:" << id;
    mResources.remove(collection.id());
    // AkonadiResource will remove the resource from Resources.
}

/******************************************************************************
* Called when a collection creation is about to start, or has completed.
*/
void AkonadiDataModel::slotCollectionBeingCreated(const QString& path, Akonadi::Collection::Id id, bool finished)
{
    if (finished)
    {
        mCollectionsBeingCreated.removeAll(path);
        mCollectionIdsBeingCreated << id;
    }
    else
        mCollectionsBeingCreated << path;
}

/******************************************************************************
* Called when the collection tree has been fetched for the first time.
*/
void AkonadiDataModel::slotCollectionTreeFetched()
{
    Resources::notifyResourcesCreated();
}

/******************************************************************************
* Called when a collection has been populated.
*/
void AkonadiDataModel::slotCollectionPopulated(Akonadi::Collection::Id id)
{
    qCDebug(KALARM_LOG) << "AkonadiDataModel::slotCollectionPopulated:" << id;
    AkonadiResource::notifyCollectionLoaded(id, events(id));
}

/******************************************************************************
* Called when calendar migration has completed.
*/
void AkonadiDataModel::slotMigrationCompleted()
{
    if (mCollectionsBeingCreated.isEmpty() && mCollectionIdsBeingCreated.isEmpty())
    {
        qCDebug(KALARM_LOG) << "AkonadiDataModel: Migration completed";
        setMigrationComplete();
    }
}

/******************************************************************************
* Called when an item in the monitored collections has changed.
*/
void AkonadiDataModel::slotMonitoredItemChanged(const Akonadi::Item& item, const QSet<QByteArray>&)
{
    qCDebug(KALARM_LOG) << "AkonadiDataModel::slotMonitoredItemChanged: item id=" << item.id() << ", revision=" << item.revision();
    const QModelIndex ix = itemIndex(item);
    if (ix.isValid())
    {
        Resource res;
        Item itm = item;
        KAEvent evnt = event(itm, ix, res);   // this sets item.parentCollection()
        if (evnt.isValid())
        {
            // Notify the resource containing the item.
            if (res.isValid())
                AkonadiResource::notifyItemChanged(res, itm, false);

            // Wait to ensure that the base EntityTreeModel has processed the
            // itemChanged() signal first, before we notify AkonadiResource
            // that the event has changed.
            mPendingEventChanges.enqueue(evnt);
            QTimer::singleShot(0, this, &AkonadiDataModel::slotEmitEventUpdated);
        }
    }
}

/******************************************************************************
* Called to Q_EMIT a signal when an event in the monitored collections has
* changed.
*/
void AkonadiDataModel::slotEmitEventUpdated()
{
    while (!mPendingEventChanges.isEmpty())
    {
        const KAEvent event = mPendingEventChanges.dequeue();
        Resource res = Resources::resource(event.resourceId());
        AkonadiResource::notifyEventsChanged(res, {event});
    }
}

/******************************************************************************
* Refresh the specified Collection with up to date data.
* Return: true if successful, false if collection not found.
*/
bool AkonadiDataModel::refresh(Akonadi::Collection& collection) const
{
    const QModelIndex ix = modelIndexForCollection(this, collection);
    if (!ix.isValid())
        return false;
    collection = ix.data(CollectionRole).value<Collection>();

    // Also update our own copy of the collection.
    updateResource(collection);
    return true;
}

/******************************************************************************
* Refresh the specified Item with up to date data.
* Return: true if successful, false if item not found.
*/
bool AkonadiDataModel::refresh(Akonadi::Item& item) const
{
    const QModelIndex ix = itemIndex(item);
    if (!ix.isValid())
        return false;
    item = ix.data(ItemRole).value<Item>();
    return true;
}

/******************************************************************************
* Return the AkonadiResource object for a collection ID.
*/
Resource AkonadiDataModel::resource(Collection::Id id) const
{
    return mResources.value(id, AkonadiResource::nullResource());
}

/******************************************************************************
* Return the resource at a specified index, with up to date data.
*/
Resource AkonadiDataModel::resource(const QModelIndex& ix) const
{
    return mResources.value(ix.data(CollectionIdRole).toLongLong(), AkonadiResource::nullResource());
}

/******************************************************************************
* Find the QModelIndex of a resource.
*/
QModelIndex AkonadiDataModel::resourceIndex(const Resource& resource) const
{
    const Collection& collection = AkonadiResource::collection(resource);
    const QModelIndex ix = modelIndexForCollection(this, collection);
    if (!ix.isValid())
        return QModelIndex();
    return ix;
}

/******************************************************************************
* Find the QModelIndex of a resource with a given ID.
*/
QModelIndex AkonadiDataModel::resourceIndex(Akonadi::Collection::Id id) const
{
    const QModelIndex ix = modelIndexForCollection(this, Collection(id));
    if (!ix.isValid())
        return QModelIndex();
    return ix;
}

/******************************************************************************
* Return a reference to the collection held in a Resource. This is the
* definitive copy of the collection used by this model.
* Return: the collection held by the model, or null if not found.
*/
Collection* AkonadiDataModel::collection(Collection::Id id) const
{
    auto it = mResources.find(id);
    if (it != mResources.end())
    {
        Collection& c = AkonadiResource::collection(it.value());
        if (c.isValid())
            return &c;
    }
    return nullptr;
}

/******************************************************************************
* Return a reference to the collection held in a Resource. This is the
* definitive copy of the collection used by this model.
* Return: the collection held by the model, or null if not found.
*/
Collection* AkonadiDataModel::collection(const Resource& resource) const
{
    return collection(resource.id());
}

/******************************************************************************
* Find the QModelIndex of an item.
*/
QModelIndex AkonadiDataModel::itemIndex(const Akonadi::Item& item) const
{
    const QModelIndexList ixs = modelIndexesForItem(this, item);
    for (const QModelIndex& ix : ixs)
    {
        if (ix.isValid())
            return ix;
    }
    return QModelIndex();
}

/******************************************************************************
* Update the resource which holds a given Collection, by copying the Collection
* value into it. If there is no resource, a new resource is created.
* Param: collection - this should have been fetched from the model to ensure
*                     that its value is up to date.
*/
Resource& AkonadiDataModel::updateResource(const Collection& collection) const
{
    auto it = mResources.find(collection.id());
    if (it != mResources.end())
    {
        Collection& resourceCol = AkonadiResource::collection(it.value());
        if (&collection != &resourceCol)
            resourceCol = collection;
    }
    else
    {
        // Create a new resource for the collection.
        it = mResources.insert(collection.id(), AkonadiResource::create(collection));
    }
    return it.value();
}

/******************************************************************************
* Display a message to the user.
*/
void AkonadiDataModel::slotResourceMessage(ResourceType::MessageType type, const QString& message, const QString& details)
{
    handleResourceMessage(type, message, details);
}

// vim: et sw=4:
