/*
 *  akonadimodel.cpp  -  KAlarm calendar file access using Akonadi
 *  Program:  kalarm
 *  Copyright © 2007-2019 David Jarvie <djarvie@kde.org>
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

#include "akonadimodel.h"
#include "alarmtime.h"
#include "autoqpointer.h"
#include "calendarmigrator.h"
#include "mainwindow.h"
#include "messagebox.h"
#include "preferences.h"
#include "synchtimer.h"
#include "kalarmsettings.h"
#include "kalarmdirsettings.h"

#include <kalarmcal/alarmtext.h>
#include <kalarmcal/compatibilityattribute.h>
#include <kalarmcal/eventattribute.h>

#include <AkonadiCore/agentfilterproxymodel.h>
#include <AkonadiCore/agentinstancecreatejob.h>
#include <AkonadiCore/agentmanager.h>
#include <AkonadiCore/agenttype.h>
#include <AkonadiCore/attributefactory.h>
#include <AkonadiCore/changerecorder.h>
#include <AkonadiCore/collectiondeletejob.h>
#include <AkonadiCore/collectionfetchjob.h>
#include <AkonadiCore/entitydisplayattribute.h>
#include <AkonadiCore/item.h>
#include <AkonadiCore/itemfetchscope.h>
#include <AkonadiWidgets/agenttypedialog.h>

#include <KLocalizedString>
#include <kcolorutils.h>

#include <QUrl>
#include <QApplication>
#include <QFileInfo>
#include <QTimer>
#include "kalarm_debug.h"

using namespace Akonadi;
using namespace KAlarmCal;

// Ensure CalendarDataModel::UserRole is valid. CalendarDataModel does not
// include Akonadi headers, so here we check that it has been set to be
// compatible with EntityTreeModel::UserRole.
static_assert((int)CalendarDataModel::UserRole>=(int)Akonadi::EntityTreeModel::UserRole, "CalendarDataModel::UserRole wrong value");

static const Collection::Rights writableRights = Collection::CanChangeItem | Collection::CanCreateItem | Collection::CanDeleteItem;

/*=============================================================================
= Class: AkonadiModel
=============================================================================*/

AkonadiModel* AkonadiModel::mInstance = nullptr;
int           AkonadiModel::mTimeHourPos = -2;

/******************************************************************************
* Construct and return the singleton.
*/
AkonadiModel* AkonadiModel::instance()
{
    if (!mInstance)
        mInstance = new AkonadiModel(new ChangeRecorder(qApp), qApp);
    return mInstance;
}

/******************************************************************************
* Constructor.
*/
AkonadiModel::AkonadiModel(ChangeRecorder* monitor, QObject* parent)
    : EntityTreeModel(monitor, parent)
    , CalendarDataModel()
    , mMonitor(monitor)
    , mMigrationChecked(false)
    , mMigrating(false)
{
    // Set lazy population to enable the contents of unselected collections to be ignored
    setItemPopulationStrategy(LazyPopulation);

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
    connect(monitor, &Monitor::collectionRemoved, this, &AkonadiModel::slotCollectionRemoved);
    initCalendarMigrator();
    MinuteTimer::connect(this, SLOT(slotUpdateTimeTo()));
    Preferences::connect(SIGNAL(archivedColourChanged(QColor)), this, SLOT(slotUpdateArchivedColour(QColor)));
    Preferences::connect(SIGNAL(disabledColourChanged(QColor)), this, SLOT(slotUpdateDisabledColour(QColor)));
    Preferences::connect(SIGNAL(holidaysChanged(KHolidays::HolidayRegion)), this, SLOT(slotUpdateHolidays()));
    Preferences::connect(SIGNAL(workTimeChanged(QTime,QTime,QBitArray)), this, SLOT(slotUpdateWorkingHours()));

    connect(this, &AkonadiModel::rowsInserted, this, &AkonadiModel::slotRowsInserted);
    connect(this, &AkonadiModel::rowsAboutToBeRemoved, this, &AkonadiModel::slotRowsAboutToBeRemoved);
    connect(this, &Akonadi::EntityTreeModel::collectionPopulated, this, &AkonadiModel::slotCollectionPopulated);
    connect(monitor, &Monitor::itemChanged, this, &AkonadiModel::slotMonitoredItemChanged);

    connect(ServerManager::self(), &ServerManager::stateChanged, this, &AkonadiModel::checkResources);
    checkResources(ServerManager::state());
}


/******************************************************************************
* Destructor.
*/
AkonadiModel::~AkonadiModel()
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
void AkonadiModel::checkResources(ServerManager::State state)
{
    switch (state)
    {
        case ServerManager::Running:
            if (!mMigrationChecked)
            {
                qCDebug(KALARM_LOG) << "AkonadiModel::checkResources: Server running";
                mMigrationChecked = true;
                mMigrating = true;
                CalendarMigrator::execute();
            }
            break;
        case ServerManager::NotRunning:
            qCDebug(KALARM_LOG) << "AkonadiModel::checkResources: Server stopped";
            mMigrationChecked = false;
            mMigrating = false;
            mCollectionAlarmTypes.clear();
            mCollectionRights.clear();
            mCollectionEnabled.clear();
            initCalendarMigrator();
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
void AkonadiModel::initCalendarMigrator()
{
    CalendarMigrator::reset();
    connect(CalendarMigrator::instance(), &CalendarMigrator::creating,
                                    this, &AkonadiModel::slotCollectionBeingCreated);
    connect(CalendarMigrator::instance(), &QObject::destroyed,
                                    this, &AkonadiModel::slotMigrationCompleted);
}

/******************************************************************************
* Return whether calendar migration has completed.
*/
bool AkonadiModel::isMigrationCompleted() const
{
    return mMigrationChecked && !mMigrating;
}

/******************************************************************************
* Return the data for a given role, for a specified item.
*/
QVariant AkonadiModel::data(const QModelIndex& index, int role) const
{
    if (roleHandled(role))
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
                const KAEvent ev(event(item, index));   // this sets item.parentCollection()

                const Resource res = resource(item.parentCollection().id());
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
int AkonadiModel::entityColumnCount(HeaderGroup group) const
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
* Return data for a column heading.
*/
QVariant AkonadiModel::entityHeaderData(int section, Qt::Orientation orientation, int role, HeaderGroup group) const
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
            const QVariant value = CalendarDataModel::headerData(section, orientation, role, eventHeaders, handled);
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
void AkonadiModel::signalDataChanged(bool (*checkFunc)(const Item&), int startColumn, int endColumn, const QModelIndex& parent)
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

void AkonadiModel::slotUpdateTimeTo()
{
    signalDataChanged(&checkItem_isActive, TimeToColumn, TimeToColumn, QModelIndex());
}


/******************************************************************************
* Called when the colour used to display archived alarms has changed.
*/
static bool checkItem_isArchived(const Item& item)
{ return item.mimeType() == KAlarmCal::MIME_ARCHIVED; }

void AkonadiModel::slotUpdateArchivedColour(const QColor&)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotUpdateArchivedColour";
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

void AkonadiModel::slotUpdateDisabledColour(const QColor&)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotUpdateDisabledColour";
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

void AkonadiModel::slotUpdateHolidays()
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotUpdateHolidays";
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

void AkonadiModel::slotUpdateWorkingHours()
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotUpdateWorkingHours";
    Q_ASSERT(TimeToColumn == TimeColumn + 1);  // signal should be emitted only for TimeTo and Time columns
    signalDataChanged(&checkItem_workTimeOnly, TimeColumn, TimeToColumn, QModelIndex());
}

/******************************************************************************
* Reload a collection from Akonadi storage. The backend data is not reloaded.
*/
bool AkonadiModel::reloadResource(const Resource& resource)
{
    if (!resource.isValid())
        return false;
    qCDebug(KALARM_LOG) << "AkonadiModel::reloadResource:" << resource.id();
    Collection collection(resource.id());
    mMonitor->setCollectionMonitored(collection, false);
    mMonitor->setCollectionMonitored(collection, true);
    return true;
}

/******************************************************************************
* Reload a collection from Akonadi storage. The backend data is not reloaded.
*/
void AkonadiModel::reload()
{
    qCDebug(KALARM_LOG) << "AkonadiModel::reload";
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
QModelIndex AkonadiModel::eventIndex(const KAEvent& event) const
{
    return itemIndex(Item(mEventIds.value(event.id()).itemId));
}

/******************************************************************************
* Returns the index to a specified event.
*/
QModelIndex AkonadiModel::eventIndex(const QString& eventId) const
{
    return itemIndex(Item(mEventIds.value(eventId).itemId));
}

#if 0
/******************************************************************************
* Return all events of a given type belonging to a collection.
*/
KAEvent::List AkonadiModel::events(Akonadi::Collection& collection, CalEvent::Type type) const
{
    KAEvent::List list;
    const QModelIndex ix = modelIndexForCollection(this, collection);
    if (ix.isValid())
        getChildEvents(ix, type, list);
    return list;
}

/******************************************************************************
* Recursive function to append all child Events with a given mime type.
*/
void AkonadiModel::getChildEvents(const QModelIndex& parent, CalEvent::Type type, KAEvent::List& events) const
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
                if (event.isValid()  &&  event.category() == type)
                    events += event;
            }
        }
        else
        {
            const Collection c = ix.data(CollectionRole).value<Collection>();
            if (c.isValid())
                getChildEvents(ix, type, events);
        }
    }
}
#endif

KAEvent AkonadiModel::event(const QString& eventId) const
{
    return event(eventIndex(eventId));
}

KAEvent AkonadiModel::event(const QModelIndex& ix) const
{
    if (!ix.isValid())
        return KAEvent();
    Item item = ix.data(ItemRole).value<Item>();
    return event(item, ix);
}

/******************************************************************************
* Return the event for an Item at a specified model index.
* The item's parent collection is set, as is the event's collection ID.
*/
KAEvent AkonadiModel::event(Akonadi::Item& item, const QModelIndex& ix, AkonadiResource** res) const
{
//TODO: Tune performance: This function is called very frequently with the same parameters
    if (ix.isValid())
    {
        const Collection pc = ix.data(ParentCollectionRole).value<Collection>();
        item.setParentCollection(pc);
        AkonadiResource* akres = resource(pc.id()).resource<AkonadiResource>();
        if (akres)
        {
            if (res)
                *res = akres;
            return akres->event(item);
        }
    }
    if (res)
        *res = nullptr;
    return KAEvent();
}

/******************************************************************************
* Return the up to date Item for a specified Akonadi ID.
*/
Item AkonadiModel::itemById(Item::Id id) const
{
    Item item(id);
    if (!refresh(item))
        return Item();
    return item;
}

/******************************************************************************
* Return the Item for a given event.
*/
Item AkonadiModel::itemForEvent(const QString& eventId) const
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
bool AkonadiModel::addEvent(KAEvent& event, Resource& resource)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::addEvent: ID:" << event.id();
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
void AkonadiModel::slotRowsInserted(const QModelIndex& parent, int start, int end)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotRowsInserted:" << start << "-" << end << "(parent =" << parent << ")";
    EventList events;
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix = index(row, 0, parent);
        const Collection collection = ix.data(CollectionRole).value<Collection>();
        if (collection.isValid())
        {
            // A collection has been inserted. Create a new resource to hold it.
            qCDebug(KALARM_LOG) << "AkonadiModel::slotRowsInserted: Collection" << collection.id() << collection.name();
            Resource& resource = updateResource(collection);
            // Ignore it if it isn't owned by a valid Akonadi resource.
            if (resource.isValid())
            {
                const QSet<QByteArray> attrs{ CollectionAttribute::name() };
                setCollectionChanged(resource, collection, attrs, true);
                Q_EMIT resourceAdded(resource);

                if (!mCollectionsBeingCreated.contains(collection.remoteId())
                &&  (collection.rights() & writableRights) == writableRights)
                {
                    // Update to current KAlarm format if necessary, and if the user agrees
                    CalendarMigrator::updateToCurrentFormat(resource, false, MainWindow::mainMainWindow());
                }

                if (!collection.hasAttribute<CompatibilityAttribute>())
                {
                    // If the compatibility attribute is missing at this point,
                    // it doesn't always get notified later, so fetch the
                    // collection to ensure that we see it.
                    AgentInstance agent = AgentManager::self()->instance(collection.resource());
                    CollectionFetchJob* job = new CollectionFetchJob(Collection::root(), CollectionFetchJob::Recursive);
                    job->fetchScope().setResource(agent.identifier());
                    connect(job, &CollectionFetchJob::result, instance(), &AkonadiModel::collectionFetchResult);
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
                AkonadiResource* akResource;
                const KAEvent evnt = event(item, ix, &akResource);   // this sets item.parentCollection()
                if (evnt.isValid())
                {
                    qCDebug(KALARM_LOG) << "AkonadiModel::slotRowsInserted: Event" << evnt.id();
                    events += Event(evnt, item.parentCollection());
                    mEventIds[evnt.id()] = EventIds(item.parentCollection().id(), item.id());
                }

                // Notify the resource containing the item.
                if (akResource)
                    akResource->notifyItemChanged(item, true);
            }
        }
    }

    if (!events.isEmpty())
        Q_EMIT eventsAdded(events);
}

/******************************************************************************
* Called when a CollectionFetchJob has completed.
* Check for and process changes in attribute values.
*/
void AkonadiModel::collectionFetchResult(KJob* j)
{
    CollectionFetchJob* job = qobject_cast<CollectionFetchJob*>(j);
    if (j->error())
        qCWarning(KALARM_LOG) << "AkonadiModel::collectionFetchResult: CollectionFetchJob" << job->fetchScope().resource()<< "error: " << j->errorString();
    else
    {
        const Collection::List collections = job->collections();
        for (const Collection& c : collections)
        {
            qCDebug(KALARM_LOG) << "AkonadiModel::collectionFetchResult:" << c.id();
            auto it = mResources.find(c.id());
            if (it == mResources.end())
                continue;
            Resource& resource = it.value();
            Collection& resourceCol = AkonadiResource::collection(resource);
            QSet<QByteArray> attrNames;
            if (c.hasAttribute<CollectionAttribute>())
            {
                if (!resourceCol.hasAttribute<CollectionAttribute>()
                ||  *c.attribute<CollectionAttribute>() != *resourceCol.attribute<CollectionAttribute>())
                    attrNames.insert(CollectionAttribute::name());
            }
            if (c.hasAttribute<CompatibilityAttribute>())
            {
                if (!resourceCol.hasAttribute<CompatibilityAttribute>()
                ||  *c.attribute<CompatibilityAttribute>() != *resourceCol.attribute<CompatibilityAttribute>())
                    attrNames.insert(CompatibilityAttribute::name());
            }
            resourceCol = c;   // update our copy of the collection
            if (!attrNames.isEmpty())
            {
                // Process the changed attribute values.
                setCollectionChanged(resource, c, attrNames, false);
            }
        }
    }
}

/******************************************************************************
* Called when rows are about to be removed from the model.
*/
void AkonadiModel::slotRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotRowsAboutToBeRemoved:" << start << "-" << end << "(parent =" << parent << ")";
    EventList events;
    for (int row = start;  row <= end;  ++row)
    {
        const QModelIndex ix = index(row, 0, parent);
        Item item = ix.data(ItemRole).value<Item>();
        const KAEvent evnt = event(item, ix);   // this sets item.parentCollection()
        if (evnt.isValid())
        {
            qCDebug(KALARM_LOG) << "AkonadiModel::slotRowsAboutToBeRemoved: Collection:" << item.parentCollection().id() << ", Event ID:" << evnt.id();
            events += Event(evnt, item.parentCollection());
            mEventIds.remove(evnt.id());
        }
    }

    if (!events.isEmpty())
        Q_EMIT eventsToBeRemoved(events);
}

/******************************************************************************
* Called when a monitored collection has changed.
* Updates the collection held by the collection's resource, and notifies
* changes of interest.
*/
void AkonadiModel::slotCollectionChanged(const Akonadi::Collection& c, const QSet<QByteArray>& attributeNames)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotCollectionChanged:" << c.id() << attributeNames;
    auto it = mResources.find(c.id());
    if (it != mResources.end())
    {
        // The Monitor::collectionChanged() signal is not always emitted when
        // attributes are created!  So check whether any attributes not
        // included in 'attributeNames' have been created.
        Resource& resource = it.value();
        Collection& resourceCol = AkonadiResource::collection(resource);
        QSet<QByteArray> attrNames = attributeNames;
        if (!resourceCol.hasAttribute<CollectionAttribute>()  &&  c.hasAttribute<CollectionAttribute>())
            attrNames.insert(CollectionAttribute::name());
        if (!resourceCol.hasAttribute<CompatibilityAttribute>()  &&  c.hasAttribute<CompatibilityAttribute>())
            attrNames.insert(CompatibilityAttribute::name());
        if (&c != &resourceCol)
            resourceCol = c;
        setCollectionChanged(resource, c, attrNames, false);
    }
}

/******************************************************************************
* Called when a monitored collection's properties or content have changed.
* Optionally emits a signal if properties of interest have changed.
*/
void AkonadiModel::setCollectionChanged(Resource& resource, const Collection& collection, const QSet<QByteArray>& attributeNames, bool rowInserted)
{
    // Check for a read/write permission change
    const Collection::Rights oldRights = mCollectionRights.value(collection.id(), Collection::AllRights);
    const Collection::Rights newRights = collection.rights() & writableRights;
    if (newRights != oldRights)
    {
        qCDebug(KALARM_LOG) << "AkonadiModel::setCollectionChanged:" << collection.id() << ": rights ->" << newRights;
        mCollectionRights[collection.id()] = newRights;
        Q_EMIT resourceStatusChanged(resource, ReadOnly, (newRights != writableRights), rowInserted);
    }

    // Check for a change in content mime types
    // (e.g. when a collection is first created at startup).
    const CalEvent::Types oldAlarmTypes = mCollectionAlarmTypes.value(collection.id(), CalEvent::EMPTY);
    const CalEvent::Types newAlarmTypes = resource.alarmTypes();
    if (newAlarmTypes != oldAlarmTypes)
    {
        qCDebug(KALARM_LOG) << "AkonadiModel::setCollectionChanged:" << collection.id() << ": alarm types ->" << newAlarmTypes;
        mCollectionAlarmTypes[collection.id()] = newAlarmTypes;
        Q_EMIT resourceStatusChanged(resource, AlarmTypes, static_cast<int>(newAlarmTypes), rowInserted);
    }

    // Check for the collection being enabled/disabled
    if (attributeNames.contains(CollectionAttribute::name())  &&  collection.hasAttribute<CollectionAttribute>())
    {
        // Enabled/disabled can only be set by KAlarm (not the resource), so if the
        // attibute doesn't exist, it is ignored.
        const CalEvent::Types newEnabled = collection.attribute<CollectionAttribute>()->enabled();
        handleEnabledChange(resource, newEnabled, rowInserted);
    }

    // Check for the backend calendar format changing
    if (attributeNames.contains(CompatibilityAttribute::name()))
    {
        // Update to current KAlarm format if necessary, and if the user agrees
        qCDebug(KALARM_LOG) << "AkonadiModel::setCollectionChanged: CompatibilityAttribute";
        CalendarMigrator::updateToCurrentFormat(resource, false, MainWindow::mainMainWindow());
    }

    if (mMigrating)
    {
        mCollectionIdsBeingCreated.removeAll(collection.id());
        if (mCollectionsBeingCreated.isEmpty() && mCollectionIdsBeingCreated.isEmpty()
        &&  CalendarMigrator::completed())
        {
            qCDebug(KALARM_LOG) << "AkonadiModel::setCollectionChanged: Migration completed";
            mMigrating = false;
            Q_EMIT migrationCompleted();
        }
    }
}

/******************************************************************************
* Called by a resource to notify that its status has changed.
*/
void AkonadiModel::notifySettingsChanged(AkonadiResource* res, Change change)
{
    AkonadiModel* model = AkonadiModel::instance();
    auto it = model->mResources.find(res->id());
    if (it != model->mResources.end())
    {
        Resource& resource = it.value();
        if (change == Enabled)
        {
            const CalEvent::Types newEnabled = resource.enabledTypes();
            instance()->handleEnabledChange(resource, newEnabled, false);
        }
    }
}

/******************************************************************************
* Emit a signal if a resource's enabled state has changed.
*/
void AkonadiModel::handleEnabledChange(Resource& resource, CalEvent::Types newEnabled, bool rowInserted)
{
    static bool firstEnabled = true;
    const CalEvent::Types oldEnabled = mCollectionEnabled.value(resource.id(), CalEvent::EMPTY);
    if (firstEnabled  ||  newEnabled != oldEnabled)
    {
        qCDebug(KALARM_LOG) << "AkonadiModel::handleEnabledChange:" << resource.id() << ": enabled ->" << newEnabled;
        firstEnabled = false;
        mCollectionEnabled[resource.id()] = newEnabled;
        Q_EMIT resourceStatusChanged(resource, Enabled, static_cast<int>(newEnabled), rowInserted);
    }
}

/******************************************************************************
* Called when a monitored collection is removed.
*/
void AkonadiModel::slotCollectionRemoved(const Collection& collection)
{
    const Collection::Id id = collection.id();
    qCDebug(KALARM_LOG) << "AkonadiModel::slotCollectionRemoved:" << id;
    mResources.remove(collection.id());
    mCollectionRights.remove(id);
    Q_EMIT collectionDeleted(id);
}

/******************************************************************************
* Called when a collection creation is about to start, or has completed.
*/
void AkonadiModel::slotCollectionBeingCreated(const QString& path, Akonadi::Collection::Id id, bool finished)
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
* Called when a collection has been populated.
*/
void AkonadiModel::slotCollectionPopulated(Akonadi::Collection::Id)
{
    if (isFullyPopulated())
    {
        // All collections have now been populated.
        Q_EMIT collectionsPopulated();
        // Prevent the signal being emitted more than once.
        disconnect(this, &Akonadi::EntityTreeModel::collectionPopulated, this, &AkonadiModel::slotCollectionPopulated);
    }
}

/******************************************************************************
* Called when calendar migration has completed.
*/
void AkonadiModel::slotMigrationCompleted()
{
    if (mCollectionsBeingCreated.isEmpty() && mCollectionIdsBeingCreated.isEmpty())
    {
        qCDebug(KALARM_LOG) << "AkonadiModel: Migration completed";
        mMigrating = false;
        Q_EMIT migrationCompleted();
    }
}

/******************************************************************************
* Called when an item in the monitored collections has changed.
*/
void AkonadiModel::slotMonitoredItemChanged(const Akonadi::Item& item, const QSet<QByteArray>&)
{
    qCDebug(KALARM_LOG) << "AkonadiModel::slotMonitoredItemChanged: item id=" << item.id() << ", revision=" << item.revision();
    const QModelIndex ix = itemIndex(item);
    if (ix.isValid())
    {
        AkonadiResource* akResource;
        Item itm = item;
        KAEvent evnt = event(itm, ix, &akResource);   // this sets item.parentCollection()
        if (evnt.isValid())
        {
            // Notify the resource containing the item.
            if (akResource)
                akResource->notifyItemChanged(itm, false);

            // Wait to ensure that the base EntityTreeModel has processed the
            // itemChanged() signal first, before we Q_EMIT eventChanged().
            mPendingEventChanges.enqueue(Event(evnt, itm.parentCollection()));
            QTimer::singleShot(0, this, &AkonadiModel::slotEmitEventChanged);
        }
    }
}

/******************************************************************************
* Called to Q_EMIT a signal when an event in the monitored collections has
* changed.
*/
void AkonadiModel::slotEmitEventChanged()
{
    while (!mPendingEventChanges.isEmpty())
    {
        Q_EMIT eventChanged(mPendingEventChanges.dequeue());
    }
}

/******************************************************************************
* Refresh the specified Collection with up to date data.
* Return: true if successful, false if collection not found.
*/
bool AkonadiModel::refresh(Akonadi::Collection& collection) const
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
bool AkonadiModel::refresh(Akonadi::Item& item) const
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
Resource AkonadiModel::resource(Collection::Id id) const
{
    return mResources.value(id, AkonadiResource::nullResource());
}

/******************************************************************************
* Find the collection containing the specified event.
*/
Resource AkonadiModel::resourceForEvent(const QString& eventId) const
{
    const Collection::Id id = mEventIds.value(eventId).collectionId;
    return mResources.value(id, AkonadiResource::nullResource());
}

/******************************************************************************
* Return the resource at a specified index, with up to date data.
*/
Resource AkonadiModel::resource(const QModelIndex& ix) const
{
    return mResources.value(ix.data(CollectionIdRole).toLongLong(), AkonadiResource::nullResource());
}

/******************************************************************************
* Find the QModelIndex of a resource.
*/
QModelIndex AkonadiModel::resourceIndex(const Resource& resource) const
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
QModelIndex AkonadiModel::resourceIndex(Akonadi::Collection::Id id) const
{
    const QModelIndex ix = modelIndexForCollection(this, Collection(id));
    if (!ix.isValid())
        return QModelIndex();
    return ix;
}

/******************************************************************************
* Find the ID of the collection containing the specified event.
*/
Collection::Id AkonadiModel::resourceIdForEvent(const QString& eventId) const
{
    return mEventIds.value(eventId).collectionId;
}

/******************************************************************************
* Return a reference to the collection held in a Resource. This is the
* definitive copy of the collection used by this model.
* Return: the collection held by the model, or null if not found.
*/
Collection* AkonadiModel::collection(Collection::Id id) const
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
Collection* AkonadiModel::collection(const Resource& resource) const
{
    return collection(resource.id());
}

/******************************************************************************
* Find the QModelIndex of an item.
*/
QModelIndex AkonadiModel::itemIndex(const Akonadi::Item& item) const
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
Resource& AkonadiModel::updateResource(const Collection& collection) const
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
        it = mResources.insert(collection.id(), Resource(new AkonadiResource(collection)));
    }
    return it.value();
}

void AkonadiModel::notifyResourceError(AkonadiResource*, const QString& message, const QString& details)
{
    KAMessageBox::detailedError(MainWindow::mainMainWindow(), message, details);
}

// vim: et sw=4:
